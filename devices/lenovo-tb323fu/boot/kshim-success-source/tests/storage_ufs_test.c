/* SPDX-License-Identifier: GPL-3.0-only */
#include <storage_ufs.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static _Alignas(1024) unsigned char arena[8192];
struct fixture {
    uint32_t reg[0x304/4]; uint64_t ticks;
    unsigned writes, submits, to_device, to_cpu;
    int stuck, bad_tag, bad_status, bad_ocs, short_data, bad_response;
    unsigned tur_fail, tur_count, cap_count;
    unsigned sense_len, segment, sense_format, sense_key, sense_asc;
    unsigned char disk[4096];
    uint32_t boot_lun;
    int ignore_boot_lun_write;
};
#define TUR_ALWAYS 0xFFFFFFFFu
static uint32_t le32(const unsigned char *p)
{ return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static void be32(unsigned char *p,uint32_t v)
{ p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v; }
/* CHECK CONDITION response UPIU with autosense. The sense length is big
 * endian at rsp[32] and sense bytes start at rsp[34]; max sense is 18 bytes.
 * Fixed 0x70/0x71 carry the key at sense[2] (rsp[36]); descriptor 0x72/0x73
 * carry it at sense[1] (rsp[35]). */
static void tur_sense(struct fixture *f,unsigned char *rsp)
{
    rsp[6]=1;rsp[7]=2;
    rsp[10]=f->segment>>8;rsp[11]=f->segment;
    rsp[32]=f->sense_len>>8;rsp[33]=f->sense_len;
    rsp[34]=f->sense_format;
    if(f->sense_format==0x72||f->sense_format==0x73){
        rsp[35]=f->sense_key;rsp[36]=f->sense_asc;
    } else {
        rsp[36]=f->sense_key;rsp[37]=f->sense_asc;
    }
}
static int rd(void *p,uint32_t o,uint32_t *v) {
    struct fixture *f=p;assert(o<sizeof(f->reg));*v=f->reg[o/4];return 0;
}
static int wr(void *p,uint32_t o,uint32_t v) {
    struct fixture *f=p;assert(o<sizeof(f->reg));++f->writes;
    if(o==0x20) { f->reg[o/4]&=~v;return 0; }
    f->reg[o/4]=v;
    if(o!=0x58)return 0;
    assert(v==1);++f->submits;
    assert(f->to_device==f->submits+1);
    assert(le32(arena+16)==0x10000400 && le32(arena+20)==0);
    assert(arena[24]==128 && arena[26]==128);
    assert(arena[30]==0 && arena[31]==1); /* PRDT offset 256 dwords. */
    unsigned char *q=arena+1024,*rsp=arena+1536;
    assert(q[3]==0);
    memset(rsp,0,512);rsp[0]=q[0]+0x20;rsp[2]=q[2];
    rsp[3]=f->bad_tag?1:0;rsp[6]=f->bad_response?1:0;rsp[7]=f->bad_status?2:0;
    if(q[0]==0x16) {
        memcpy(rsp+12,q+12,20);
        if(q[12]==3||q[12]==4){
            assert(q[13]==0&&q[14]==0&&q[15]==0&&q[18]==0&&q[19]==0);
            assert(q[5]==(q[12]==4?0x81:1));
            if(q[12]==4&&!f->ignore_boot_lun_write)f->boot_lun=q[23];
            be32(rsp+20,f->boot_lun);
        } else {
            assert(q[5]==1 && q[12]==1);
            rsp[10]=0;rsp[11]=4;rsp[18]=0;rsp[19]=4;
            rsp[32]=4;rsp[33]=q[13];rsp[34]=q[14];rsp[35]=1;
        }
    } else if(q[0]==1) {
        unsigned char op=q[16];
        if(op==0)++f->tur_count;
        if(op==0x9e)++f->cap_count;
        if(op==0&&f->tur_fail){
            if(f->tur_fail!=TUR_ALWAYS)--f->tur_fail;
            tur_sense(f,rsp);
        }
        if(op==0x9e) { /* READ CAPACITY(16), one LUN with eight4K blocks. */
            assert(q[17]==0x10 && q[29]==32 && q[1]==0x40);
            memset(arena+4096,0,32);arena[4103]=7;be32(arena+4104,4096);
        } else if(op==0x88) {
            assert(q[1]==0x40 && q[31]==0);
            assert(le32(arena+2048)==0x10001000);
            assert(le32(arena+2060)==4095);
            memcpy(arena+4096,f->disk,4096);
        } else if(op==0x8a) {
            assert(q[1]==0x20 && (q[17]&8)); /* FUA */
            memcpy(f->disk,arena+4096,4096);
        } else assert(op==0 || op==0x35);
        if(f->short_data){rsp[1]|=0x20;be32(rsp+12,4);}
    } else assert(q[0]==0);
    arena[8]=f->bad_ocs?1:0;
    if(!f->stuck)f->reg[0x58/4]=0;
    return 0;
}
static void dev(void *p,void *a,size_t n){assert(a==arena&&n==8192);++((struct fixture*)p)->to_device;}
static void cpu(void *p,void *a,size_t n){assert(a==arena&&n==8192);++((struct fixture*)p)->to_cpu;}
static uint64_t now(void *p){return ((struct fixture*)p)->ticks;}
static void delay(void *p,uint32_t u){((struct fixture*)p)->ticks+=(u+999)/1000;}
static struct kshim_ufs_ops ops(struct fixture *f)
{ return (struct kshim_ufs_ops){f,rd,wr,dev,cpu,now,delay,false}; }
static void init(struct fixture *f) {
    memset(f,0,sizeof(*f));memset(arena,0,sizeof(arena));
    f->reg[0]=1u<<24;f->reg[0x34/4]=1;f->reg[0x30/4]=15;
    f->reg[0x50/4]=0x11110000;f->reg[0x54/4]=2;
    f->reg[0x60/4]=1;f->reg[0x24/4]=0x400;f->reg[0x18/4]=0x123;
}
static void claim(struct fixture *f,struct kshim_ufs_host *h) {
    struct kshim_ufs_ops o=ops(f);memset(h,0,sizeof(*h));
    assert(kshim_ufs_claim(h,&o,arena,sizeof(arena),0x10000000)==0);
}
static void set_ua(struct fixture *f,unsigned fails,unsigned char format,
                   unsigned char key)
{
    f->tur_fail=fails;f->sense_len=18;f->segment=20;
    f->sense_format=format;f->sense_key=key;f->sense_asc=0x2a;
}
int main(void) {
    struct fixture f;struct kshim_ufs_host h;struct kshim_ufs_lun l;
    unsigned char data[4096];unsigned char desc[32];uint16_t length=sizeof(desc);
    /* A: the first TUR reports CHECK CONDITION with a fixed-format UNIT
     * ATTENTION (response=1, status=2, 18-byte sense in a 20-byte data
     * segment), with a response-header error code. open_lun must
     * record the key, retry that TUR and then read capacity. */
    init(&f);set_ua(&f,1,0x70,6);claim(&f,&h);
    assert(kshim_ufs_open_lun(&h,0,true,&l)==0);
    assert(f.tur_count==2&&f.cap_count==1);
    assert(kshim_ufs_release(&h)==0);
    /* B: descriptor sense carries the key at sense[1] (rsp[35]); the ASC at
     * sense[2] (rsp[36]) is deliberately not 6, so a fixed-format reader
     * would extract the wrong key and give up instead of retrying. */
    init(&f);set_ua(&f,1,0x72,6);claim(&f,&h);
    assert(kshim_ufs_open_lun(&h,0,true,&l)==0);
    assert(f.tur_count==2&&f.cap_count==1);
    assert(kshim_ufs_release(&h)==0);
    /* Deferred fixed/descriptor variants follow the same sense layout. */
    static const unsigned char deferred[]={0x71,0x73};
    for(unsigned i=0;i<sizeof(deferred);++i){
        init(&f);set_ua(&f,1,deferred[i],6);claim(&f,&h);
        assert(kshim_ufs_open_lun(&h,0,true,&l)==0);
        assert(f.tur_count==2&&f.cap_count==1);
        assert(kshim_ufs_release(&h)==0);
    }
    /* C: a persistent UNIT ATTENTION is bounded at three TURs and never
     * reaches READ CAPACITY. */
    init(&f);set_ua(&f,TUR_ALWAYS,0x70,6);claim(&f,&h);
    assert(kshim_ufs_open_lun(&h,0,true,&l)==KSHIM_UFS_SCSI);
    assert(f.tur_count==3&&f.cap_count==0);
    assert(h.last_scsi_status==2&&h.last_sense_key==6);
    assert(kshim_ufs_release(&h)==0);
    /* D: only UNIT ATTENTION is retried; ILLEGAL REQUEST fails on the first
     * TUR and there is no generic SCSI retry. */
    init(&f);set_ua(&f,TUR_ALWAYS,0x70,5);claim(&f,&h);
    assert(kshim_ufs_open_lun(&h,0,true,&l)==KSHIM_UFS_SCSI);
    assert(f.tur_count==1&&f.cap_count==0);
    assert(h.last_scsi_status==2&&h.last_sense_key==5);
    assert(kshim_ufs_release(&h)==0);
    /* E: truncated or unrecognized sense must never be normalized into a
     * UNIT ATTENTION from bytes outside the declared sense data: fixed
     * length 2 with the key in padding, unknown format byte, declared length
     * longer than the data segment, and descriptor length 1 without a key. */
    static const struct {
        unsigned char format; unsigned len, segment; unsigned char key;
    } malformed[]={{0x70,2,4,6},{0x00,18,20,6},{0x70,18,4,6},{0x72,1,2,6}};
    for(unsigned i=0;i<sizeof(malformed)/sizeof(malformed[0]);++i){
        init(&f);set_ua(&f,1,malformed[i].format,malformed[i].key);
        f.sense_len=malformed[i].len;f.segment=malformed[i].segment;
        claim(&f,&h);
        assert(kshim_ufs_open_lun(&h,0,true,&l)==KSHIM_UFS_SCSI);
        assert(f.tur_count==1&&f.cap_count==0);
        assert(h.last_sense_key==0);
        assert(kshim_ufs_release(&h)==0);
    }
    /* F: response=1 with status=0 is still a failed response, not success;
     * NOP and descriptor query keep rejecting it, and a status-only TUR must
     * not be mistaken for a good command. */
    init(&f);claim(&f,&h);f.bad_response=1;
    assert(kshim_ufs_nop(&h)==KSHIM_UFS_RESPONSE);
    length=sizeof(desc);
    assert(kshim_ufs_read_descriptor(&h,2,0,desc,&length)==KSHIM_UFS_RESPONSE);
    assert(kshim_ufs_open_lun(&h,0,true,&l)==KSHIM_UFS_RESPONSE);
    assert(f.tur_count==1&&f.cap_count==0);
    assert(kshim_ufs_release(&h)==0);
    init(&f);claim(&f,&h);
    assert(kshim_ufs_nop(&h)==0);
    assert(kshim_ufs_read_descriptor(&h,2,0,desc,&length)==0&&length==4&&desc[1]==2);
    assert(kshim_ufs_open_lun(&h,0,true,&l)==0);
    assert(l.block.block_size==4096&&l.block.block_count==8);
    memset(f.disk,0xa5,4096);assert(kshim_block_read(&l.block,3,1,data)==0);
    assert(data[0]==0xa5&&data[4095]==0xa5);
    unsigned submissions=f.submits;
    assert(kshim_block_read(&l.block,8,1,data)==KSHIM_STORAGE_RANGE&&f.submits==submissions);
    memset(data,0x5a,4096);assert(kshim_block_write(&l.block,4,1,data)==0&&f.disk[4095]==0x5a);
    assert(kshim_block_flush(&l.block)==0);
    assert(kshim_ufs_release(&h)==0);
    assert(f.reg[0x50/4]==0x11110000&&f.reg[0x54/4]==2&&f.reg[0x60/4]==1);
    assert(f.reg[0x24/4]==0x400&&f.reg[0x18/4]==0x123);
    for(unsigned i=0;i<5;++i) {
        init(&f);struct kshim_ufs_ops o=ops(&f);memset(&h,0,sizeof(h));
        if(i==0)f.reg[0x58/4]=2;
        if(i==1){f.reg[0]|=1u<<30;f.reg[0x300/4]=1;}
        if(i==2)f.reg[0x34/4]=0;
        if(i==3)f.reg[0]|=1u<<29;
        if(i==4)f.reg[0x30/4]|=0x10;
        assert(kshim_ufs_claim(&h,&o,arena,sizeof(arena),0x10000000)!=0&&f.writes==0);
    }
    init(&f);claim(&f,&h);f.bad_tag=1;
    assert(kshim_ufs_nop(&h)!=0);assert(kshim_ufs_release(&h)==0);
    init(&f);claim(&f,&h);f.bad_ocs=1;
    assert(kshim_ufs_nop(&h)!=0);assert(kshim_ufs_release(&h)==0);
    init(&f);claim(&f,&h);assert(kshim_ufs_open_lun(&h,0,false,&l)==0);
    assert(kshim_block_write(&l.block,0,1,data)==KSHIM_STORAGE_READ_ONLY);
    f.short_data=1;memset(data,0xcc,4096);
    assert(kshim_block_read(&l.block,0,1,data)!=0&&data[0]==0xcc);
    assert(kshim_ufs_release(&h)==0);
    init(&f);claim(&f,&h);f.stuck=1;
    assert(kshim_ufs_nop(&h)==KSHIM_UFS_TIMEOUT);
    submissions=f.submits;
    assert(kshim_ufs_nop(&h)==KSHIM_UFS_QUARANTINED&&f.submits==submissions);
    assert(kshim_ufs_release(&h)!=0&&h.owned);
    init(&f);claim(&f,&h);f.boot_lun=1;uint32_t boot_lun=0;
    assert(kshim_ufs_get_boot_lun(&h,&boot_lun)==0&&boot_lun==1);
    assert(kshim_ufs_set_boot_lun(&h,2)==0&&f.boot_lun==2);
    f.ignore_boot_lun_write=1;assert(kshim_ufs_set_boot_lun(&h,1)!=0);
    submissions=f.submits;assert(kshim_ufs_set_boot_lun(&h,0)!=0&&f.submits==submissions);
    f.bad_tag=1;assert(kshim_ufs_get_boot_lun(&h,&boot_lun)!=0);f.bad_tag=0;
    assert(kshim_ufs_release(&h)==0);
    puts("UFS transport, boot LUN readback, block IO, restore, timeout quarantine: PASS");
}
