/* SPDX-License-Identifier: GPL-3.0-only */
#include <storage_ufs_settings.h>
#include <string.h>

static int restored(const struct kshim_ufs_host *h)
{
    const uint32_t offsets[] = {0x50,0x54,0x60,0x24,0x18,0x58,0x78};
    const uint32_t expected[] = {h->saved_base[0],h->saved_base[1],
        h->saved_run,h->saved_ie,h->saved_ahit,0,0};
    for (unsigned i=0; i<sizeof(offsets)/sizeof(offsets[0]); ++i) {
        uint32_t value;
        if (h->ops.read32(h->ops.ctx,offsets[i],&value) || value!=expected[i])
            return KSHIM_STORAGE_IO;
    }
    return 0;
}
static int finish(void *ctx)
{
    struct kshim_ufs_settings *b=ctx;
    if (!b->host.owned) return b->unsafe ? KSHIM_UFS_QUARANTINED : 0;
    int rc=kshim_ufs_release(&b->host);
    if (!rc) rc=restored(&b->host);
    b->release_status=rc;
    if (rc) b->unsafe=1; /* Never clear an uncertain DMA/firmware state. */
    return rc;
}
static int start(struct kshim_ufs_settings *b, bool writes)
{
    if (b->unsafe) return KSHIM_UFS_QUARANTINED;
    if (b->host.owned) return KSHIM_UFS_BUSY;
    uint64_t dma=0;
    int rc=b->preflight(b->preflight_context,&dma);
    if (!rc) rc=kshim_ufs_claim(&b->host,&b->ops,b->arena,KSHIM_UFS_DMA_BYTES,dma);
    if (!rc) rc=kshim_ufs_nop(&b->host);
    if (!rc) rc=kshim_ufs_prepare_device(&b->host);
    if (!rc) rc=kshim_ufs_open_lun(&b->host,0,writes,&b->lun);
    b->begin_status=rc;
    if (rc && b->host.owned) {
        int cleanup=finish(b);
        if (cleanup) return cleanup;
    }
    return rc;
}
static int begin_save(void *ctx)
{
    struct kshim_ufs_settings *b=ctx;
    ++b->save_attempts;
    int rc=start(b,true);
    if (rc) return rc;
    const struct kshim_config_store *old=&b->settings.medium[0].config;
    struct kshim_partition partition;
    struct kshim_config_store current;
    /* A stable blockdev pointer is not proof that the medium is unchanged.
     * Re-read its GPT and descriptor after every new claim, before any write. */
    if (b->settings.source!=KSHIM_SETTINGS_INTERNAL ||
        b->lun.block.block_size!=b->block_size ||
        b->lun.block.block_count!=b->block_count) {
        rc=KSHIM_STORAGE_FORMAT;
    } else {
        rc=kshim_gpt_find_backup(&b->lun.block,&partition,b->scratch,sizeof(b->scratch));
        if (!rc && (partition.first_lba!=old->partition.first_lba ||
                    partition.last_lba!=old->partition.last_lba ||
                    memcmp(partition.uuid,old->partition.uuid,16))) rc=KSHIM_STORAGE_FORMAT;
        if (!rc) rc=kshim_config_open(&current,&b->lun.block,&partition,b->board,
                                        b->scratch,sizeof(b->scratch));
        if (!rc && (current.region_lba!=old->region_lba ||
                    memcmp(current.installation_uuid,old->installation_uuid,16)))
            rc=KSHIM_STORAGE_FORMAT;
    }
    b->begin_status=rc;
    if (rc) {
        int cleanup=finish(b);
        if (cleanup) return cleanup;
    }
    return rc;
}
int kshim_ufs_settings_open(struct kshim_ufs_settings *b,
    const struct kshim_ufs_ops *ops,void *arena,uint32_t board,
    int (*preflight)(void *,uint64_t *),void *ctx,
    const struct kshim_config_values *defaults)
{
    if (!b || !ops || !arena || !board || !preflight || !defaults)
        return KSHIM_STORAGE_ARG;
    if (b->unsafe) return KSHIM_UFS_QUARANTINED;
    if (b->host.owned) return KSHIM_UFS_BUSY;
    memset(b,0,sizeof(*b));
    b->ops=*ops;b->arena=arena;b->board=board;
    b->preflight=preflight;b->preflight_context=ctx;
    int rc=start(b,false);
    if (rc) { b->load_status=rc; return rc; }
    b->block_size=b->lun.block.block_size;b->block_count=b->lun.block.block_count;
    rc=kshim_settings_open(&b->settings,&b->lun.block,NULL,board,defaults);
    int cleanup=finish(b);
    if (cleanup) {
        b->settings.source=KSHIM_SETTINGS_NONE;
        rc=cleanup;
    } else if (rc==KSHIM_STORAGE_OK || rc==KSHIM_STORAGE_EMPTY) {
        b->settings.io_context=b;
        b->settings.io_begin=begin_save;
        b->settings.io_end=finish;
    }
    b->load_status=rc;
    return rc;
}
bool kshim_ufs_settings_handoff_safe(const struct kshim_ufs_settings *b)
{
    return b && !b->unsafe && !b->host.owned;
}
static int get_boot_lun(void *ctx,uint32_t *value)
{ return kshim_ufs_get_boot_lun(ctx,value); }
static int set_boot_lun(void *ctx,uint32_t value)
{ return kshim_ufs_set_boot_lun(ctx,value); }
int kshim_ufs_settings_ab(struct kshim_ufs_settings *b,struct kshim_ab_status *status,
                           bool change,unsigned target,unsigned expected_current)
{
    if(!b||!b->preflight||!status)return KSHIM_STORAGE_ARG;
    int rc=start(b,change);if(rc)return rc;
    struct kshim_ufs_lun luns[KSHIM_AB_LUNS];
    struct kshim_ab_media media={.ctx=&b->host,.get_boot_lun=get_boot_lun,.set_boot_lun=set_boot_lun};
    for(unsigned l=0;l<KSHIM_AB_LUNS;++l){
        uint8_t unit[255];uint16_t length=sizeof(unit);
        rc=kshim_ufs_read_descriptor(&b->host,2,l,unit,&length);
        if(rc)break;
        if(length<5 || unit[2]!=l || unit[3]!=1 || unit[4]>2) {
            rc=KSHIM_STORAGE_FORMAT;break;
        }
        media.boot_id[l]=unit[4];
        rc=kshim_ufs_open_lun(&b->host,l,change,&luns[l]);
        if(rc)break;
        media.lun[l]=luns[l].block;
    }
    if(!rc)rc=change?kshim_ab_select(&media,target,expected_current):kshim_ab_inspect(&media,status);
    if(rc==KSHIM_AB_UNCERTAIN)b->unsafe=1;
    int cleanup=finish(b);
    return cleanup?cleanup:rc;
}
