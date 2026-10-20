#pragma once

#include <stdint.h>

#if defined(__has_include)
#if __has_include(<config.h>)
#include <config.h>
#endif
#endif

/* Version 1 of /chosen:kshim,boot-timing-ms is eight big-endian u64 values
 * in this fixed order. Values are milliseconds since ENTRY; missing stages
 * or a missing ENTRY use UINT64_MAX. Do not reorder these indices.
 * kshim,boot-entry-counter-ms stores ENTRY's unadjusted counter time (or
 * UINT64_MAX), and kshim,boot-timing-version is a big-endian u32 set to 1.
 * HANDOFF includes time spent waiting for the user's menu selection. */
typedef enum {
    KSHIM_BOOT_STAGE_ENTRY = 0,
    KSHIM_BOOT_STAGE_RESOURCES,
    KSHIM_BOOT_STAGE_MMU,
    KSHIM_BOOT_STAGE_FONT,
    KSHIM_BOOT_STAGE_UI,
    KSHIM_BOOT_STAGE_ENTRIES,
    KSHIM_BOOT_STAGE_FIRST_FRAME,
    KSHIM_BOOT_STAGE_HANDOFF,
    KSHIM_BOOT_STAGE_COUNT
} kshim_boot_stage_t;

#if defined(CONFIG_KSHIM_BOOT_TIMING) && CONFIG_KSHIM_BOOT_TIMING
/* Each stage samples the clock only on its first call. */
void kshim_boot_metrics_record_once(kshim_boot_stage_t Stage);

/* Records the early-MMU result so /chosen:kshim,mmu-early can report whether
 * translation was already live on entry and how kshim_mmu_configure_early()
 * ended.  Negative status values are exported as their two's complement. */
void kshim_boot_metrics_set_mmu(uint32_t Enabled, int32_t Status);

/* The caller supplies the selected Linux DT, with an existing /chosen node.
 * Only the three properties documented above are changed. No DT resize or
 * memory/reservation edits are performed. A libfdt error (including NOSPACE)
 * is diagnostic only and must not prevent boot. */
int kshim_boot_metrics_export(void *Fdt);

/* Weak implementation uses the existing millisecond counter on hardware;
 * host tests override this symbol with a deterministic clock. */
uint64_t kshim_boot_metrics_now_ms(void);
#else
static inline void kshim_boot_metrics_record_once(kshim_boot_stage_t Stage)
{
    (void)Stage;
}

static inline void kshim_boot_metrics_set_mmu(uint32_t Enabled, int32_t Status)
{
    (void)Enabled;
    (void)Status;
}

static inline int kshim_boot_metrics_export(void *Fdt)
{
    (void)Fdt;
    return 0;
}
#endif
