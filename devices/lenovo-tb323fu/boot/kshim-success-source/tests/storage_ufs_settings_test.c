/* SPDX-License-Identifier: GPL-3.0-only */
#include <storage_ufs_settings.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct disk { uint8_t head[12288],tail[65536];unsigned writes;int fail; };
static void p32(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;++i)p[i]=n>>(8*i);}
static void p64(uint8_t *p,uint64_t n){p32(p,n);p32(p+4,n>>32);}
static struct disk internal;
static struct kshim_ufs_settings backend;
static unsigned preflights,claims,releases,read_count;
static int preflight_rc,claim_rc,claim_partial,nop_rc,power_rc,lun_rc,release_rc;
static int corrupt_restore,changed_geometry;
static int ab_mode,descriptor_bad,descriptor_fail,ab_result;
static unsigned ab_calls,descriptor_calls;
static uint32_t registers[0x80/4];
static unsigned char arena[KSHIM_UFS_DMA_BYTES];
static const uint64_t start=16,blocks=524288,total=524320;
static int read_disk(void *ctx,uint64_t b,uint32_t n,void *out){
    struct disk *d=ctx;assert(backend.host.owned);++read_count;
    if(b+n<=3){memcpy(out,d->head+b*4096,n*4096);return 0;}
    uint64_t tail=start+blocks-16;
    if(b<tail||n>tail+16-b)return -1;
    memcpy(out,d->tail+(b-tail)*4096,n*4096);return 0;
}
static int write_disk(void *ctx,uint64_t b,uint32_t n,const void *in){
    struct disk *d=ctx;assert(backend.host.owned);uint64_t tail=start+blocks-16;
    assert((b==tail+1||b==tail+2)&&n==1);++d->writes;
    if(d->fail)return -1;
    memcpy(d->tail+(b-tail)*4096,in,4096);return 0;
}
static int flush(void *ctx){assert(backend.host.owned);return ((struct disk*)ctx)->fail?-1:0;}
static struct kshim_blockdev device(struct disk *d)
{return (struct kshim_blockdev){d,4096,total,read_disk,write_disk,flush};}
static void provision(struct disk *d){
    memset(d,0,sizeof(*d));uint8_t *h=d->head+4096,*e=d->head+8192,*c=d->tail;
    memcpy(h,"EFI PART",8);p32(h+8,0x10000);p32(h+12,92);p64(h+24,1);
    p64(h+32,total-1);p64(h+40,3);p64(h+48,total-3);h[56]=1;
    p64(h+72,2);p32(h+80,1);p32(h+84,128);
    e[0]=1;e[16]=1;p64(e+32,start);p64(e+40,start+blocks-1);
    const char name[]="ARMADA_BACKUP";
    for(unsigned i=0;i<sizeof(name)-1;++i)e[56+2*i]=(uint8_t)name[i];
    p32(h+88,kshim_crc32(e,128));p32(h+16,kshim_crc32(h,92));
    memcpy(c,"KSCFGV3",8);p32(c+8,1);p32(c+12,4096);p64(c+16,2147483648ULL);
    p64(c+24,100000);p64(c+32,2147418112ULL);p32(c+40,65536);p32(c+44,4096);
    c[51]=1;c[64]=2;p32(c+80,4096);p32(c+84,8192);p32(c+88,1);
    p32(c+4092,kshim_crc32(c,4092));
}

static int read_register(void *ctx,uint32_t off,uint32_t *value){
    (void)ctx;assert(off<sizeof(registers));*value=registers[off/4];return 0;
}
static const struct kshim_ufs_ops ops={.read32=read_register,.manage_link=true};
static int preflight(void *ctx,uint64_t *dma){
    assert(ctx==&internal);++preflights;*dma=0x10000000;return preflight_rc;
}
int kshim_ufs_claim(struct kshim_ufs_host *h,const struct kshim_ufs_ops *o,
                    void *a,size_t bytes,uint64_t dma){
    ++claims;assert(h==&backend.host&&!h->owned&&a==arena&&bytes==8192&&dma==0x10000000);
    if(claim_rc&&!claim_partial)return claim_rc;
    memset(h,0,sizeof(*h));h->ops=*o;h->owned=1;
    h->saved_base[0]=registers[0x50/4];h->saved_base[1]=registers[0x54/4];
    h->saved_run=registers[0x60/4];h->saved_ie=registers[0x24/4];h->saved_ahit=registers[0x18/4];
    return claim_rc;
}
int kshim_ufs_nop(struct kshim_ufs_host *h){assert(h->owned);return nop_rc;}
int kshim_ufs_prepare_device(struct kshim_ufs_host *h){assert(h->owned);return power_rc;}
int kshim_ufs_open_lun(struct kshim_ufs_host *h,uint8_t number,bool writes,
                       struct kshim_ufs_lun *lun){
    assert(h->owned&&(number==0||ab_mode));if(lun_rc)return lun_rc;
    lun->block=device(&internal);lun->host=h;lun->number=number;
    if(!writes){lun->block.write=NULL;lun->block.flush=NULL;}
    if(changed_geometry)++lun->block.block_count;
    return 0;
}
int kshim_ufs_read_descriptor(struct kshim_ufs_host *h,uint8_t id,uint8_t index,
                              void *out,uint16_t *length){
    assert(h->owned&&ab_mode&&id==2&&index<6&&*length>=5);++descriptor_calls;
    if(descriptor_fail)return descriptor_fail;
    uint8_t *unit=out;memset(unit,0,5);unit[0]=5;unit[1]=2;unit[2]=index;
    unit[3]=descriptor_bad?0:1;unit[4]=index==1?1:index==2?2:0;*length=5;return 0;
}
int kshim_ufs_get_boot_lun(struct kshim_ufs_host *h,uint32_t *value){
    assert(h->owned);*value=1;return 0;
}
int kshim_ufs_set_boot_lun(struct kshim_ufs_host *h,uint32_t value){
    assert(h->owned&&value==2);return 0;
}
static void check_ab_media(const struct kshim_ab_media *m,bool writes){
    assert(backend.host.owned&&m->ctx==&backend.host);++ab_calls;
    for(unsigned l=0;l<6;++l){
        assert(m->lun[l].block_size==4096&&!!m->lun[l].write==writes);
        assert(m->boot_id[l]==(l==1?1:l==2?2:0));
    }
    uint32_t lun;assert(!m->get_boot_lun(m->ctx,&lun)&&lun==1);
    if(writes)assert(!m->set_boot_lun(m->ctx,2));
}
int kshim_ab_inspect(const struct kshim_ab_media *m,struct kshim_ab_status *s){
    check_ab_media(m,false);*s=(struct kshim_ab_status){0,{1,1}};return ab_result;
}
int kshim_ab_select(const struct kshim_ab_media *m,unsigned target,unsigned current){
    assert(target==1&&current==0);check_ab_media(m,true);return ab_result;
}
int kshim_ufs_release(struct kshim_ufs_host *h){
    assert(h->owned);++releases;if(release_rc)return release_rc;
    h->owned=0;if(corrupt_restore)registers[0x60/4]^=1;return 0;
}
static void reset(void){
    memset(&backend,0,sizeof(backend));provision(&internal);
    memset(registers,0,sizeof(registers));registers[0x50/4]=0x80000000;
    registers[0x54/4]=1;registers[0x60/4]=1;registers[0x24/4]=0x102;
    registers[0x18/4]=0x123;
    preflights=claims=releases=read_count=0;
    preflight_rc=claim_rc=claim_partial=nop_rc=power_rc=lun_rc=release_rc=0;
    corrupt_restore=changed_geometry=0;
    ab_mode=descriptor_bad=descriptor_fail=ab_result=0;ab_calls=descriptor_calls=0;
}
static int open_store(void){
    const struct kshim_config_values defaults={0,0,0, 0, 0};
    return kshim_ufs_settings_open(&backend,&ops,arena,1,preflight,&internal,&defaults);
}
static const struct kshim_config_values saved={0,2,1, 0, 0};
static void load_save_reopen(void){
    reset();assert(open_store()==KSHIM_STORAGE_EMPTY);
    assert(kshim_ufs_settings_handoff_safe(&backend)&&!backend.host.owned);
    assert(claims==1&&releases==1&&internal.writes==0&&backend.settings.source==KSHIM_SETTINGS_INTERNAL);
    assert(kshim_settings_save(&backend.settings,&saved)==0);
    assert(claims==2&&releases==2&&preflights==2&&internal.writes==1);
    assert(!backend.host.owned&&backend.settings.sequence==1&&backend.settings.values.language==1);
    assert(open_store()==0&&backend.settings.sequence==1&&backend.settings.values.default_target==2);
    assert(!backend.host.owned&&claims==3&&releases==3);
    assert(kshim_settings_save(&backend.settings,&saved)==0&&backend.settings.sequence==2);
    assert(claims==4&&releases==4&&internal.writes==2);
}
static void start_failures(void){
    reset();preflight_rc=-71;assert(open_store()==-71&&claims==0&&releases==0);
    assert(kshim_ufs_settings_handoff_safe(&backend));
    reset();claim_rc=-72;assert(open_store()==-72&&claims==1&&releases==0);
    assert(kshim_ufs_settings_handoff_safe(&backend));
    reset();claim_rc=-72;claim_partial=1;assert(open_store()==-72&&releases==1);
    assert(kshim_ufs_settings_handoff_safe(&backend));
    reset();nop_rc=-73;assert(open_store()==-73&&releases==1&&read_count==0);
    reset();power_rc=-74;assert(open_store()==-74&&releases==1&&read_count==0);
    reset();lun_rc=-75;assert(open_store()==-75&&releases==1&&read_count==0);
    reset();memset(internal.tail,0,sizeof(internal.tail));
    assert(open_store()==KSHIM_STORAGE_NOT_FOUND&&releases==1&&internal.writes==0);
    assert(backend.settings.source==KSHIM_SETTINGS_NONE&&kshim_ufs_settings_handoff_safe(&backend));
}
static void save_failures(void){
    reset();assert(open_store()==KSHIM_STORAGE_EMPTY);
    preflight_rc=-71;assert(kshim_settings_save(&backend.settings,&saved)==-71);
    assert(claims==1&&releases==1&&internal.writes==0);
    preflight_rc=0;internal.fail=1;
    assert(kshim_settings_save(&backend.settings,&saved)!=0&&releases==2);
    assert(!backend.host.owned&&backend.settings.sequence==0&&backend.settings.save_uncertain);
    internal.fail=0;release_rc=-76;
    assert(kshim_settings_save(&backend.settings,&saved)==-76);
    assert(backend.host.owned&&!kshim_ufs_settings_handoff_safe(&backend));
    assert(backend.settings.sequence==0&&backend.settings.values.language==0&&backend.settings.save_uncertain);
    unsigned old_claims=claims,old_writes=internal.writes,old_releases=releases;
    assert(kshim_settings_save(&backend.settings,&saved)!=0);
    assert(open_store()!=0&&claims==old_claims&&internal.writes==old_writes&&releases==old_releases);
}
static void identity_recheck(void){
    reset();assert(open_store()==KSHIM_STORAGE_EMPTY);
    changed_geometry=1;assert(kshim_settings_save(&backend.settings,&saved)!=0);
    assert(internal.writes==0&&releases==2&&kshim_ufs_settings_handoff_safe(&backend));
    reset();assert(open_store()==KSHIM_STORAGE_EMPTY);
    internal.tail[64]^=4;p32(internal.tail+4092,kshim_crc32(internal.tail,4092));
    assert(kshim_settings_save(&backend.settings,&saved)!=0&&internal.writes==0&&releases==2);
    reset();assert(open_store()==KSHIM_STORAGE_EMPTY);
    uint8_t *entry=internal.head+8192,*hdr=internal.head+4096;
    entry[16]^=2;p32(hdr+88,kshim_crc32(entry,128));p32(hdr+16,0);p32(hdr+16,kshim_crc32(hdr,92));
    assert(kshim_settings_save(&backend.settings,&saved)!=0&&internal.writes==0&&releases==2);
}
static void unsafe_finish(void){
    reset();release_rc=-76;assert(open_store()==-76);
    assert(!kshim_ufs_settings_handoff_safe(&backend)&&backend.settings.source==KSHIM_SETTINGS_NONE);
    reset();corrupt_restore=1;assert(open_store()!=0);
    assert(!backend.host.owned&&!kshim_ufs_settings_handoff_safe(&backend));
    unsigned old_claims=claims;corrupt_restore=0;
    assert(open_store()!=0&&claims==old_claims); /* unsafe latch survives reopen */
    reset();backend.host.owned=1;
    assert(open_store()==KSHIM_UFS_BUSY&&claims==0&&releases==0&&backend.host.owned);
}
static void history_reopen(void){
    reset();assert(open_store()==KSHIM_STORAGE_EMPTY);
    assert(kshim_settings_save(&backend.settings,&saved)==0);
    assert(kshim_settings_note_boot(&backend.settings,2)==0 && backend.settings.sequence==2);
    unsigned old_claims=claims,old_writes=internal.writes;
    assert(kshim_settings_note_boot(&backend.settings,2)==0 && claims==old_claims && internal.writes==old_writes);
    assert(kshim_settings_note_boot(&backend.settings,1)==0 && backend.settings.sequence==3);
    assert(kshim_ufs_settings_handoff_safe(&backend));
    assert(open_store()==0 && backend.settings.values.last_target==1 && backend.settings.values.default_target==2);
    release_rc=-76;
    assert(kshim_settings_note_boot(&backend.settings,2)==-76);
    assert(backend.settings.values.last_target==1 && !kshim_ufs_settings_handoff_safe(&backend));
}
static void ab_ownership(void){
    struct kshim_ab_status status;
    reset();assert(open_store()==KSHIM_STORAGE_EMPTY);ab_mode=1;
    assert(kshim_ufs_settings_ab(&backend,&status,false,0,0)==0);
    assert(ab_calls==1&&descriptor_calls==6&&status.current==0);
    assert(claims==2&&releases==2&&kshim_ufs_settings_handoff_safe(&backend));
    descriptor_bad=1;
    assert(kshim_ufs_settings_ab(&backend,&status,true,1,0)==KSHIM_STORAGE_FORMAT);
    assert(ab_calls==1&&claims==3&&releases==3&&kshim_ufs_settings_handoff_safe(&backend));
    descriptor_bad=0;descriptor_fail=-77;
    assert(kshim_ufs_settings_ab(&backend,&status,true,1,0)==-77&&ab_calls==1);
    descriptor_fail=0;ab_result=KSHIM_STORAGE_IO;
    assert(kshim_ufs_settings_ab(&backend,&status,true,1,0)==KSHIM_STORAGE_IO);
    assert(ab_calls==2&&kshim_ufs_settings_handoff_safe(&backend));
    ab_result=KSHIM_AB_UNCERTAIN;
    assert(kshim_ufs_settings_ab(&backend,&status,true,1,0)==KSHIM_AB_UNCERTAIN);
    assert(!backend.host.owned&&!kshim_ufs_settings_handoff_safe(&backend));
    unsigned old_claims=claims;
    assert(kshim_ufs_settings_ab(&backend,&status,false,0,0)==KSHIM_UFS_QUARANTINED);
    assert(claims==old_claims);
    reset();assert(open_store()==KSHIM_STORAGE_EMPTY);ab_mode=1;release_rc=-76;
    assert(kshim_ufs_settings_ab(&backend,&status,false,0,0)==-76);
    assert(!kshim_ufs_settings_handoff_safe(&backend));
}
int main(void){
    ab_ownership();
    history_reopen();
    load_save_reopen();start_failures();save_failures();identity_recheck();unsafe_finish();
    puts("Native UFS settings ownership/media binding/reopen/failure isolation: PASS");
}
