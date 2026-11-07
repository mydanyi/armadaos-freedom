/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef KSHIM_HOST_TEST
#include <config.h>
#endif
#include <storage_settings_native.h>
#include <storage_settings_export.h>
#include <storage_probe_io.h>
#include <storage_ufs_dma.h>
#include <storage_ufs_native_io.h>
#include <timer.h>
#include <kshim_mmu.h>

static _Alignas(4096) uint8_t Arena[KSHIM_UFS_DMA_BYTES];
static struct kshim_ufs_settings Backend;
static struct storage_probe_snapshot Fresh;
static uint32_t TestPerformed;
static int TestStatus;
static uint64_t now_ms(void *ctx) { (void)ctx;return get_current_time(); }
static void delay_us(void *ctx,uint32_t us) { (void)ctx;(void)delay(us); }
static const struct kshim_ufs_ops Ops={
    .read32=storage_ufs_native_read32,.write32=storage_ufs_native_write32,
    .sync_device=storage_ufs_native_sync_device,.sync_cpu=storage_ufs_native_sync_cpu,
    .now_ms=now_ms,.delay_us=delay_us,.manage_link=true
};
static int preflight(void *ctx,uint64_t *physical)
{
    const struct kshim_ufs_settings *b=ctx;
    if ((b->board!=DEVICE_BOARD_TB322FC && b->board!=DEVICE_BOARD_TB323FU) ||
        arm64_read_cntfrq()==0) return KSHIM_UFS_UNSUPPORTED;
    int rc=storage_ufs_native_dma_address(Arena,sizeof(Arena),physical);
    if (rc) return rc;
    storage_probe_native_capture((enum device_board)b->board,&Fresh);
    return kshim_ufs_dma_is_identity(&Fresh,*physical,sizeof(Arena)) ?
        0 : KSHIM_UFS_UNSUPPORTED;
}
int storage_settings_native_open(enum device_board actual_board,
                                  const struct kshim_config_values *defaults)
{
    if (!kshim_ufs_settings_handoff_safe(&Backend)) return KSHIM_UFS_QUARANTINED;
    TestPerformed=0;TestStatus=0;
    if (actual_board!=DEVICE_BOARD_TB322FC && actual_board!=DEVICE_BOARD_TB323FU) {
        Backend.board=(uint32_t)actual_board;
        Backend.load_status=KSHIM_UFS_UNSUPPORTED;
        Backend.settings.source=KSHIM_SETTINGS_NONE;
        return KSHIM_UFS_UNSUPPORTED;
    }
    int rc=kshim_ufs_settings_open(&Backend,&Ops,Arena,(uint32_t)actual_board,
                                     preflight,&Backend,defaults);
#if defined(CONFIG_KSHIM_SETTINGS_SAVE_TEST) && CONFIG_KSHIM_SETTINGS_SAVE_TEST
    /* Explicit Gen4 test profile only. Never overwrite existing settings or
     * provision an old backup. Seed a verified EMPTY pair with internal Linux
     * and Chinese; later boots must load that record without another write. */
    if (actual_board==DEVICE_BOARD_TB322FC && rc==KSHIM_STORAGE_EMPTY) {
        const struct kshim_config_values initial={0,2,0, 0, 0};
        TestPerformed=1;
        TestStatus=kshim_settings_save(&Backend.settings,&initial);
        if (TestStatus) return TestStatus;
    }
#endif
    return rc;
}
int storage_settings_native_note_boot(uint32_t target)
{
    struct kshim_settings_session *s=&Backend.settings;
    if (target<1 || target>3) return KSHIM_STORAGE_ARG;
    if (!kshim_ufs_settings_handoff_safe(&Backend)) return KSHIM_UFS_QUARANTINED;
    if (s->source!=KSHIM_SETTINGS_INTERNAL) return KSHIM_STORAGE_NOT_FOUND;
    if (s->values.last_target==target && !s->save_uncertain) return 0;

    /* The menu closes UI/SMP and disables the MMU before returning its
     * selection. Reuse the existing runtime tables only for this transaction;
     * the selected payload's memory remains outside this temporary interval. */
    bool restore_off=!kshim_mmu_is_enabled();
    if (restore_off && (!kshim_mmu_root_table() || kshim_mmu_enable_current_cpu())) {
        Backend.begin_status=KSHIM_UFS_UNSUPPORTED;
        return KSHIM_UFS_UNSUPPORTED;
    }
    int rc=kshim_settings_note_boot(s,target);
    /* Keep mappings alive if DMA ownership could not be safely returned. */
    if (!kshim_ufs_settings_handoff_safe(&Backend)) return KSHIM_UFS_QUARANTINED;
    if (restore_off && kshim_mmu_prepare_handoff()) {
        Backend.unsafe=1;
        return KSHIM_UFS_QUARANTINED;
    }
    return rc;
}
struct kshim_settings_session *storage_settings_native_session(void)
{
    return &Backend.settings;
}
bool storage_settings_native_handoff_safe(void)
{
    return kshim_ufs_settings_handoff_safe(&Backend);
}
int storage_settings_native_export(void *fdt)
{
    return storage_settings_export(fdt,&Backend,TestPerformed,TestStatus);
}
bool storage_settings_native_ab_supported(void)
{
    return Backend.preflight && (Backend.board==DEVICE_BOARD_TB322FC || Backend.board==DEVICE_BOARD_TB323FU)
        && kshim_ufs_settings_handoff_safe(&Backend);
}
int storage_settings_native_ab_status(struct kshim_ab_status *status)
{
    if(!storage_settings_native_ab_supported())return KSHIM_UFS_UNSUPPORTED;
    return kshim_ufs_settings_ab(&Backend,status,false,0,0);
}
int storage_settings_native_ab_select(unsigned target,unsigned expected_current)
{
    if(!storage_settings_native_ab_supported())return KSHIM_UFS_UNSUPPORTED;
    struct kshim_ab_status status;
    return kshim_ufs_settings_ab(&Backend,&status,true,target,expected_current);
}
