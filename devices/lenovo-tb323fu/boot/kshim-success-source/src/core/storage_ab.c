/* SPDX-License-Identifier: GPL-3.0-only */
#include <storage_ab.h>
#include <stdlib.h>
#include <string.h>

/* Qualcomm BootLib SetActiveSlot/SwapPtnGuid: only the TYPE GUID moves;
 * unique partition UUIDs and locations remain attached to their partitions. */
#define ACTIVE (UINT64_C(1)<<50)
#define PRIORITY (UINT64_C(3)<<48)
#define RETRIES (UINT64_C(7)<<51)
#define SUCCESS (UINT64_C(1)<<54)
#define UNBOOTABLE (UINT64_C(1)<<55)
#define MAX_ENTRIES 256u
#define ENTRY_BYTES 128u
struct table {
    uint8_t *header[2], *entries[2], *next;
    uint64_t array_lba[2];
    uint32_t count, bytes, blocks, header_bytes[2];
};
struct capture {
    struct table table[KSHIM_AB_LUNS];
    uint8_t *boot[2];
    unsigned boot_lun[2], misc_lun;
    uint8_t *misc;
    uint32_t boot_lun_id;
    struct kshim_ab_status status;
};
static uint32_t r32(const uint8_t *p)
{ return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static uint64_t r64(const uint8_t *p) { return r32(p)|(uint64_t)r32(p+4)<<32; }
static void w32(uint8_t *p,uint32_t n) { for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(n>>(8*i)); }
static void w64(uint8_t *p,uint64_t n) { for(unsigned i=0;i<8;++i)p[i]=(uint8_t)(n>>(8*i)); }
static int zero(const uint8_t *p,size_t n) { while(n--)if(*p++)return 0;return 1; }
static int name(const uint8_t *e,char out[37])
{
    unsigned n=0;
    while(n<36 && e[56+2*n]) {
        if(e[57+2*n] || e[56+2*n]<32 || e[56+2*n]>126)return -1;
        out[n]=(char)e[56+2*n];++n;
    }
    out[n]=0;
    for(unsigned i=n;i<36;++i)if(e[56+2*i]||e[57+2*i])return -1;
    return (int)n;
}
static int slot(const uint8_t *e)
{
    char text[37];int n=name(e,text);
    return n>2 && text[n-2]=='_' && (text[n-1]=='a'||text[n-1]=='b') ? text[n-1]-'a' : -1;
}
static uint8_t *find(struct capture *c,const char *want,unsigned *lun)
{
    uint8_t *found=NULL;
    for(unsigned l=0;l<KSHIM_AB_LUNS;++l)
        for(unsigned i=0;i<c->table[l].count;++i) {
            uint8_t *e=c->table[l].entries[0]+i*ENTRY_BYTES;char text[37];
            if(zero(e,16)||name(e,text)<0||strcmp(text,want))continue;
            if(found)return NULL;
            found=e;if(lun)*lun=l;
        }
    return found;
}
static void release(struct capture *c)
{
    for(unsigned l=0;l<KSHIM_AB_LUNS;++l) {
        for(unsigned j=0;j<2;++j){free(c->table[l].header[j]);free(c->table[l].entries[j]);}
        free(c->table[l].next);
    }
}
static int load_table(const struct kshim_blockdev *d,struct table *t)
{
    if((d->block_size!=512 && d->block_size!=4096)||d->block_count<10)return KSHIM_STORAGE_FORMAT;
    for(unsigned j=0;j<2;++j) {
        uint64_t lba=j?d->block_count-1:1;
        uint8_t *h=t->header[j]=calloc(1,d->block_size);
        if(!h)return KSHIM_STORAGE_OVERFLOW;
        int rc=kshim_block_read(d,lba,1,h);if(rc)return rc;
        uint32_t hs=r32(h+12),stored=r32(h+16),count=r32(h+80);
        if(memcmp(h,"EFI PART",8)||r32(h+8)!=0x10000||r32(h+20)||hs<92||hs>d->block_size||
           r64(h+24)!=lba||r64(h+32)!=(j?1:d->block_count-1)||zero(h+56,16)||
           !count||count>MAX_ENTRIES||r32(h+84)!=ENTRY_BYTES)return KSHIM_STORAGE_FORMAT;
        w32(h+16,0);uint32_t computed=kshim_crc32(h,hs);w32(h+16,stored);
        if(computed!=stored)return KSHIM_STORAGE_CRC;
        uint32_t bytes=count*ENTRY_BYTES,blocks=(bytes+d->block_size-1)/d->block_size;
        uint64_t first=r64(h+40),last=r64(h+48),array=r64(h+72);
        if(first>last||first<2+blocks||last>=d->block_count-1-blocks||array>=d->block_count||
           blocks>d->block_count-array||(!j && (array<2||array+blocks>first))||
           (j && (array<=last||array+blocks>d->block_count-1)))return KSHIM_STORAGE_RANGE;
        if(j && (count!=t->count||memcmp(h+40,t->header[0]+40,32)))return KSHIM_STORAGE_CONFLICT;
        t->count=count;t->bytes=bytes;t->blocks=blocks;t->array_lba[j]=array;t->header_bytes[j]=hs;
        uint8_t *e=t->entries[j]=calloc(blocks,d->block_size);if(!e)return KSHIM_STORAGE_OVERFLOW;
        if((rc=kshim_block_read(d,array,blocks,e)))return rc;
        if(kshim_crc32(e,bytes)!=r32(h+88))return KSHIM_STORAGE_CRC;
        if(j && memcmp(e,t->entries[0],bytes))return KSHIM_STORAGE_CONFLICT;
        for(unsigned i=0;i<count;++i) {
            uint8_t *p=e+i*ENTRY_BYTES;char text[37];if(zero(p,16))continue;
            uint64_t a=r64(p+32),b=r64(p+40);
            if(zero(p+16,16)||name(p,text)<1||a<first||a>b||b>last)return KSHIM_STORAGE_RANGE;
            for(unsigned k=0;k<i;++k){uint8_t *q=e+k*ENTRY_BYTES;
                if(!zero(q,16) && a<=r64(q+40) && r64(q+32)<=b)return KSHIM_STORAGE_RANGE;}
        }
    }
    t->next=malloc(t->bytes);if(!t->next)return KSHIM_STORAGE_OVERFLOW;
    memcpy(t->next,t->entries[0],t->bytes);return 0;
}
static int read_part(const struct kshim_blockdev *d,const uint8_t *e,uint64_t offset,uint8_t *block)
{
    if(offset/d->block_size>r64(e+40)-r64(e+32))return KSHIM_STORAGE_RANGE;
    return kshim_block_read(d,r64(e+32)+offset/d->block_size,1,block);
}
static int load(const struct kshim_ab_media *m,struct capture *c)
{
    uint8_t scratch[4096];int rc;
    if(!m||!m->get_boot_lun)return KSHIM_STORAGE_ARG;
    for(unsigned l=0;l<KSHIM_AB_LUNS;++l)if((rc=load_table(&m->lun[l],&c->table[l])))return rc;
    c->boot[0]=find(c,"boot_a",&c->boot_lun[0]);c->boot[1]=find(c,"boot_b",&c->boot_lun[1]);
    c->misc=find(c,"misc",&c->misc_lun);
    if(!c->boot[0]||!c->boot[1]||!c->misc)return KSHIM_STORAGE_CONFLICT;
    const struct kshim_blockdev *md=&m->lun[c->misc_lun];
    if((rc=read_part(md,c->misc,0,scratch)))return rc;
    if(!zero(scratch,32))return KSHIM_STORAGE_CONFLICT;
    if((rc=read_part(md,c->misc,2048,scratch)))return rc;
    if(!zero(scratch+2048%md->block_size,32))return KSHIM_STORAGE_FORMAT;
    if((rc=read_part(md,c->misc,32768,scratch)))return rc;
    const uint8_t *v=scratch+32768%md->block_size;
    if(!zero(v,64) && (v[0]!=2||r32(v+1)!=0x56740ab0||v[6]>1||(v[5]!=0&&v[5]!=4)))
        return KSHIM_STORAGE_CONFLICT;
    unsigned active=0;
    for(unsigned s=0;s<2;++s) {
        uint64_t a=r64(c->boot[s]+48);
        if(a&ACTIVE){c->status.current=s;++active;}
        const struct kshim_blockdev *d=&m->lun[c->boot_lun[s]];
        if((rc=read_part(d,c->boot[s],0,scratch)))return rc;
        int header=!memcmp(scratch,"ANDROID!",8)||!memcmp(scratch+56,"ARM\x64",4);
        c->status.bootable[s]=header && (a&PRIORITY) && !(a&UNBOOTABLE) && (a&(RETRIES|SUCCESS));
    }
    if(active!=1)return KSHIM_STORAGE_CONFLICT;
    /* bBootLunEn selects a boot ID, not a physical LUN number. Confirm the
     * unit descriptors bind those IDs to the LUNs containing each XBL. */
    for(unsigned s=0;s<2;++s) {
        unsigned l, matches=0;
        if(!find(c,s?"xbl_b":"xbl_a",&l) || m->boot_id[l]!=s+1)
            return KSHIM_STORAGE_CONFLICT;
        for(unsigned k=0;k<KSHIM_AB_LUNS;++k)if(m->boot_id[k]==s+1)++matches;
        if(matches!=1)return KSHIM_STORAGE_CONFLICT;
    }
    if((rc=m->get_boot_lun(m->ctx,&c->boot_lun_id)))return rc;
    if(c->boot_lun_id!=c->status.current+1)return KSHIM_STORAGE_CONFLICT;
    /* A factory-empty secondary boot chain must never become active just
     * because an installer has written boot_b. Firmware authenticates these
     * images at the next reset; here reject missing/empty boot-chain members. */
    const char *required[]={"abl_a","xbl_a","xbl_config_a","vendor_boot_a","vbmeta_a","init_boot_a"};
    for(unsigned s=0;s<2;++s)for(unsigned i=0;i<sizeof(required)/sizeof(required[0]);++i){
        char text[37];strcpy(text,required[i]);text[strlen(text)-1]=(char)('a'+s);
        unsigned l;uint8_t *e=find(c,text,&l);
        if(!e){c->status.bootable[s]=0;continue;}
        if((rc=read_part(&m->lun[l],e,0,scratch)))return rc;
        if((i<3 && memcmp(scratch,"\x7f" "ELF",4)) ||
           (i==3 && memcmp(scratch,"VNDRBOOT",8)) || (i==4 && memcmp(scratch,"AVB0",4)) ||
           (i==5 && memcmp(scratch,"ANDROID!",8)))
            c->status.bootable[s]=0;
    }
    /* Refuse unmatched/duplicate A/B pairs before mutation. */
    for(unsigned l=0;l<KSHIM_AB_LUNS;++l)for(unsigned i=0;i<c->table[l].count;++i){
        uint8_t *e=c->table[l].entries[0]+i*ENTRY_BYTES;if(zero(e,16)||slot(e)<0)continue;
        char text[37];int n=name(e,text);text[n-1]=text[n-1]=='a'?'b':'a';
        if(!find(c,text,NULL))return KSHIM_STORAGE_CONFLICT;
    }
    return 0;
}
int kshim_ab_inspect(const struct kshim_ab_media *m,struct kshim_ab_status *s)
{
    if(!s)return KSHIM_STORAGE_ARG;
    struct capture *c=calloc(1,sizeof(*c));if(!c)return KSHIM_STORAGE_OVERFLOW;
    int rc=load(m,c);if(!rc)*s=c->status;release(c);free(c);return rc;
}
static uint8_t *candidate(struct capture *c,unsigned l,uint8_t *entry)
{ return c->table[l].next+(entry-c->table[l].entries[0]); }
static void plan(struct capture *c,unsigned target)
{
    for(unsigned l=0;l<KSHIM_AB_LUNS;++l)for(unsigned i=0;i<c->table[l].count;++i){
        uint8_t *old=c->table[l].entries[0]+i*ENTRY_BYTES,*e=c->table[l].next+i*ENTRY_BYTES;
        int s=slot(old);if(zero(old,16)||s<0)continue;
        uint64_t attr=r64(e+48);attr=s==(int)target?attr|ACTIVE:attr&~ACTIVE;w64(e+48,attr);
        if(s==0){char text[37];int n=name(old,text);text[n-1]='b';unsigned other;
            uint8_t *b=find(c,text,&other),*out=candidate(c,other,b);
            memcpy(e,b,16);memcpy(out,old,16);}
    }
    for(unsigned s=0;s<2;++s){uint8_t *e=candidate(c,c->boot_lun[s],c->boot[s]);uint64_t a=r64(e+48);
        a&=~(PRIORITY|ACTIVE);
        if(s==target){a|=PRIORITY|ACTIVE|RETRIES;a&=~(SUCCESS|UNBOOTABLE);}
        else a|=UINT64_C(2)<<48;
        w64(e+48,a);
    }
}
static int write_tables(const struct kshim_ab_media *m,struct capture *c,int restore)
{
    uint8_t header[4096],block[4096],verify[4096];
    for(unsigned l=0;l<KSHIM_AB_LUNS;++l){
        struct table *t=&c->table[l];const struct kshim_blockdev *d=&m->lun[l];
        if(!memcmp(t->next,t->entries[0],t->bytes))continue;
        /* Back-up table first, and verify it before touching the primary. */
        for(int j=1;j>=0;--j){
            memcpy(header,t->header[j],d->block_size);
            const uint8_t *entries=restore?t->entries[j]:t->next;
            if(!restore){w32(header+88,kshim_crc32(entries,t->bytes));w32(header+16,0);
                w32(header+16,kshim_crc32(header,t->header_bytes[j]));}
            for(unsigned k=0;k<t->blocks;++k){
                memcpy(block,t->entries[j]+k*d->block_size,d->block_size);
                size_t offset=k*d->block_size,take=t->bytes-offset;
                if(take>d->block_size)take=d->block_size;
                memcpy(block,entries+offset,take);
                int rc=kshim_block_write(d,t->array_lba[j]+k,1,block);if(rc)return rc;
            }
            uint64_t at=j?d->block_count-1:1;
            int rc=kshim_block_write(d,at,1,header);if(rc)return rc;
            if((rc=kshim_block_flush(d)))return rc;
            if((rc=kshim_block_read(d,at,1,verify)))return rc;
            if(memcmp(header,verify,d->block_size))return KSHIM_STORAGE_IO;
            for(unsigned k=0;k<t->blocks;++k){
                if((rc=kshim_block_read(d,t->array_lba[j]+k,1,verify)))return rc;
                size_t offset=k*d->block_size,take=t->bytes-offset;if(take>d->block_size)take=d->block_size;
                if(memcmp(verify,entries+offset,take))return KSHIM_STORAGE_IO;
            }
        }
    }
    return 0;
}
int kshim_ab_select(const struct kshim_ab_media *m,unsigned target,unsigned expected_current)
{
    if(!m||!m->set_boot_lun||target>1||expected_current>1)return KSHIM_STORAGE_ARG;
    struct capture *c=calloc(1,sizeof(*c));if(!c)return KSHIM_STORAGE_OVERFLOW;
    int rc=load(m,c);if(rc)goto done;
    if(c->status.current!=expected_current||!c->status.bootable[target]){rc=KSHIM_STORAGE_CONFLICT;goto done;}
    if(target==c->status.current)goto done;
    plan(c,target);
    rc=write_tables(m,c,0);
    if(!rc)rc=m->set_boot_lun(m->ctx,target+1);
    uint32_t actual=0;
    if(!rc)rc=m->get_boot_lun(m->ctx,&actual);
    if(!rc&&actual!=target+1)rc=KSHIM_STORAGE_IO;
    if(rc){
        int undo=write_tables(m,c,1);
        int lun=m->set_boot_lun(m->ctx,c->boot_lun_id);
        int check=m->get_boot_lun(m->ctx,&actual);
        if(undo||lun||check||actual!=c->boot_lun_id)rc=KSHIM_AB_UNCERTAIN;
    }
done:
    release(c);free(c);return rc;
}
