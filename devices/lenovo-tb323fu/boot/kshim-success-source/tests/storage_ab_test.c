/* SPDX-License-Identifier: GPL-3.0-only */
#include <storage_ab.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLOCKS 256u
#define ENTRIES 16u
#define ACTIVE (UINT64_C(1)<<50)
struct fixture;
struct disk { struct fixture *owner; uint8_t *bytes; unsigned size; };
struct fixture {
    struct disk disk[6];
    struct kshim_ab_media media;
    unsigned writes, fail_at, persistent, corrupt_at, fail_lun, boot_lun, lun_writes;
};
static void p32(uint8_t *p,uint32_t n) { for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(n>>(8*i)); }
static void p64(uint8_t *p,uint64_t n) { p32(p,(uint32_t)n);p32(p+4,(uint32_t)(n>>32)); }
static uint64_t u64(const uint8_t *p) { uint64_t n=0;for(unsigned i=0;i<8;++i)n|=(uint64_t)p[i]<<(8*i);return n; }
static int read_disk(void *ctx,uint64_t at,uint32_t n,void *out)
{
    struct disk *d=ctx;assert(at+n<=BLOCKS);memcpy(out,d->bytes+at*d->size,n*d->size);return 0;
}
static int write_disk(void *ctx,uint64_t at,uint32_t n,const void *in)
{
    struct disk *d=ctx;struct fixture *f=d->owner;assert(at+n<=BLOCKS);++f->writes;
    memcpy(d->bytes+at*d->size,in,n*d->size);
    if(f->corrupt_at==f->writes)d->bytes[at*d->size]^=1;
    return f->fail_at && (f->writes==f->fail_at || (f->persistent&&f->writes>=f->fail_at)) ? -1 : 0;
}
static int flush(void *ctx) { (void)ctx;return 0; }
static int get_lun(void *ctx,uint32_t *n) { *n=((struct fixture*)ctx)->boot_lun;return 0; }
static int set_lun(void *ctx,uint32_t n)
{
    struct fixture *f=ctx;++f->lun_writes;f->boot_lun=n;
    if(f->fail_lun){f->fail_lun=0;return -1;}return 0;
}
static uint8_t *entry(struct fixture *f,unsigned l,unsigned i)
{ return f->disk[l].bytes+2*f->disk[l].size+128*i; }
static void partition(struct fixture *f,unsigned l,unsigned i,const char *name,unsigned first,unsigned last,const char *magic)
{
    uint8_t *e=entry(f,l,i);unsigned s=name[strlen(name)-1]=='b';
    memset(e,0,128);memset(e,s?0x22:0x11,16);e[16]=(uint8_t)(l+1);e[17]=(uint8_t)(i+1);
    p64(e+32,first);p64(e+40,last);
    p64(e+48,(UINT64_C(3)<<48)|(UINT64_C(7)<<51)|(s?0:ACTIVE));
    for(unsigned j=0;name[j];++j)e[56+2*j]=(uint8_t)name[j];
    if(magic)memcpy(f->disk[l].bytes+first*f->disk[l].size,magic,strlen(magic));
}
static void tables(struct fixture *f,unsigned l)
{
    unsigned bs=f->disk[l].size,ab=(ENTRIES*128+bs-1)/bs;
    uint8_t *a=entry(f,l,0);
    memcpy(f->disk[l].bytes+(BLOCKS-1-ab)*bs,a,ENTRIES*128);
    for(unsigned j=0;j<2;++j){
        uint8_t *h=f->disk[l].bytes+(j?BLOCKS-1:1)*bs;
        memset(h,0,bs);memcpy(h,"EFI PART",8);p32(h+8,0x10000);p32(h+12,92);
        p64(h+24,j?BLOCKS-1:1);p64(h+32,j?1:BLOCKS-1);p64(h+40,2+ab);p64(h+48,BLOCKS-2-ab);
        memset(h+56,l+1,16);p64(h+72,j?BLOCKS-1-ab:2);p32(h+80,ENTRIES);p32(h+84,128);
        p32(h+88,kshim_crc32(a,ENTRIES*128));p32(h+16,kshim_crc32(h,92));
    }
}
static void init(struct fixture *f,unsigned bs)
{
    memset(f,0,sizeof(*f));f->boot_lun=1;
    f->media.ctx=f;f->media.get_boot_lun=get_lun;f->media.set_boot_lun=set_lun;
    f->media.boot_id[1]=1;f->media.boot_id[2]=2;
    for(unsigned l=0;l<6;++l){
        f->disk[l]=(struct disk){f,calloc(BLOCKS,bs),bs};assert(f->disk[l].bytes);
        f->media.lun[l]=(struct kshim_blockdev){&f->disk[l],bs,BLOCKS,read_disk,write_disk,flush};
    }
    partition(f,0,0,"misc",8,100,NULL);
    const char *names[]={"xbl_a","xbl_config_a","abl_a","xbl_b","xbl_config_b","abl_b"};
    for(unsigned i=0;i<6;++i)partition(f,1+i/3,i%3,names[i],8+8*(i%3),15+8*(i%3),"\x7f" "ELF");
    partition(f,4,0,"boot_a",8,15,"ANDROID!");partition(f,4,1,"boot_b",16,23,"ANDROID!");
    partition(f,4,2,"vendor_boot_a",24,31,"VNDRBOOT");partition(f,4,3,"vendor_boot_b",32,39,"VNDRBOOT");
    partition(f,4,4,"vbmeta_a",40,47,"AVB0");partition(f,4,5,"vbmeta_b",48,55,"AVB0");
    for(unsigned l=0;l<6;++l)tables(f,l);
}
static void dispose(struct fixture *f) { for(unsigned l=0;l<6;++l)free(f->disk[l].bytes); }
static void unchanged(const struct fixture *f,const struct fixture *old)
{ for(unsigned l=0;l<6;++l)assert(!memcmp(f->disk[l].bytes,old->disk[l].bytes,BLOCKS*f->disk[l].size));assert(f->boot_lun==1); }
static unsigned success(unsigned bs)
{
    struct fixture f;init(&f,bs);struct kshim_ab_status status;
    assert(kshim_ab_inspect(&f.media,&status)==0&&status.current==0&&status.bootable[0]&&status.bootable[1]);
    assert(!f.writes&&!f.lun_writes);
    uint8_t original[6][128];for(unsigned i=0;i<6;++i)memcpy(original[i],entry(&f,4,i),128);
    assert(kshim_ab_select(&f.media,1,0)==0&&f.boot_lun==2);
    unsigned writes=f.writes;
    assert(kshim_ab_inspect(&f.media,&status)==0&&status.current==1&&status.bootable[0]&&status.bootable[1]);
    for(unsigned i=0;i<6;++i){
        uint8_t *e=entry(&f,4,i);assert(!memcmp(e,original[i^1],16));
        assert(!memcmp(e+16,original[i]+16,32)&&!memcmp(e+56,original[i]+56,72));
        assert(!!(u64(e+48)&ACTIVE)==(i%2==1));
    }
    assert(kshim_ab_select(&f.media,1,1)==0&&f.writes==writes);
    assert(kshim_ab_select(&f.media,0,1)==0&&f.boot_lun==1);
    assert(kshim_ab_inspect(&f.media,&status)==0&&status.current==0);
    dispose(&f);return writes;
}
static void failures(unsigned bs,unsigned writes)
{
    struct fixture original;init(&original,bs);
    for(unsigned at=1;at<=writes;++at){
        struct fixture f;init(&f,bs);f.fail_at=at;
        assert(kshim_ab_select(&f.media,1,0)!=0);unchanged(&f,&original);dispose(&f);
    }
    struct fixture f;init(&f,bs);f.fail_lun=1;
    assert(kshim_ab_select(&f.media,1,0)!=0);unchanged(&f,&original);dispose(&f);
    init(&f,bs);f.corrupt_at=1;
    assert(kshim_ab_select(&f.media,1,0)!=0);unchanged(&f,&original);dispose(&f);
    init(&f,bs);f.fail_at=1;f.persistent=1;
    assert(kshim_ab_select(&f.media,1,0)==KSHIM_AB_UNCERTAIN);dispose(&f);
    dispose(&original);
}
static void reject(unsigned bs)
{
    for(unsigned test=0;test<11;++test){
        struct fixture f;init(&f,bs);struct kshim_ab_status status;
        if(test==0)f.disk[4].bytes[bs+16]^=1;
        if(test==1)f.disk[4].bytes[(BLOCKS-1)*bs+16]^=1;
        if(test==2)f.boot_lun=2;
        if(test==3)f.disk[0].bytes[8*bs+2048]=1;
        if(test==4)f.disk[0].bytes[8*bs+32768]=1;
        if(test==5)memset(f.disk[2].bytes+24*bs,0,4);
        if(test==6)f.disk[0].bytes[8*bs]='r';
        if(test==7){entry(&f,4,0)[56]='X';tables(&f,4);}
        if(test==8)f.media.boot_id[1]=2;
        if(test==9)f.media.boot_id[2]=0;
        if(test==10)f.media.boot_id[3]=2;
        int rc=kshim_ab_inspect(&f.media,&status);
        if(test==5)assert(!rc&&!status.bootable[1]);else assert(rc);
        assert(kshim_ab_select(&f.media,1,0)!=0&&!f.writes&&!f.lun_writes);dispose(&f);
    }
    struct fixture f;init(&f,bs);
    assert(kshim_ab_select(&f.media,1,1)!=0&&!f.writes);dispose(&f);
}
int main(void)
{
    const unsigned sizes[]={512,4096};
    for(unsigned i=0;i<2;++i){unsigned writes=success(sizes[i]);failures(sizes[i],writes);reject(sizes[i]);}
    puts("A/B GPT, Boot LUN, empty slot, CRC, OTA and rollback checks: PASS");return 0;
}
