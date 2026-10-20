#include <boot_metrics.h>

#if defined(CONFIG_KSHIM_BOOT_TIMING) && CONFIG_KSHIM_BOOT_TIMING
#include <libfdt.h>

#if !defined(KSHIM_HOST_TEST) || !KSHIM_HOST_TEST
#include <timer.h>
#endif

static uint64_t mStageTime[KSHIM_BOOT_STAGE_COUNT];
static uint32_t mRecorded;
static uint32_t mMmuEarly[2]; /* enabled, status */

void kshim_boot_metrics_set_mmu(uint32_t Enabled, int32_t Status)
{
    mMmuEarly[0] = Enabled;
    mMmuEarly[1] = (uint32_t)Status;
}

__attribute__((weak)) uint64_t kshim_boot_metrics_now_ms(void)
{
#if defined(KSHIM_HOST_TEST) && KSHIM_HOST_TEST
    return 0U;
#else
    return get_current_time();
#endif
}

void kshim_boot_metrics_record_once(kshim_boot_stage_t Stage)
{
    if ((unsigned)Stage >= KSHIM_BOOT_STAGE_COUNT)
        return;
    uint32_t Bit = UINT32_C(1) << (unsigned)Stage;
    if ((mRecorded & Bit) != 0U)
        return;
    mStageTime[Stage] = kshim_boot_metrics_now_ms();
    mRecorded |= Bit;
}

int kshim_boot_metrics_export(void *Fdt)
{
    fdt64_t Elapsed[KSHIM_BOOT_STAGE_COUNT];
    uint64_t Entry = (mRecorded & (UINT32_C(1) << KSHIM_BOOT_STAGE_ENTRY)) != 0U
        ? mStageTime[KSHIM_BOOT_STAGE_ENTRY] : UINT64_MAX;
    if (Fdt == NULL)
        return -FDT_ERR_BADMAGIC;
    int Status = fdt_check_header(Fdt);
    if (Status != 0)
        return Status;
    int Chosen = fdt_path_offset(Fdt, "/chosen");
    if (Chosen < 0)
        return Chosen;
    for (unsigned Index = 0; Index < KSHIM_BOOT_STAGE_COUNT; ++Index) {
        uint64_t Value = UINT64_MAX;
        if (Entry != UINT64_MAX && (mRecorded & (UINT32_C(1) << Index)) != 0U &&
            mStageTime[Index] >= Entry)
            Value = mStageTime[Index] - Entry;
        Elapsed[Index] = cpu_to_fdt64(Value);
    }
    Status = fdt_setprop(Fdt, Chosen, "kshim,boot-timing-ms",
                         Elapsed, sizeof(Elapsed));
    if (Status != 0)
        return Status;
    Status = fdt_setprop_u64(Fdt, Chosen, "kshim,boot-entry-counter-ms", Entry);
    if (Status != 0)
        return Status;
    fdt32_t Mmu[2] = {
        cpu_to_fdt32(mMmuEarly[0]),
        cpu_to_fdt32(mMmuEarly[1]),
    };
    Status = fdt_setprop(Fdt, Chosen, "kshim,mmu-early", Mmu, sizeof(Mmu));
    if (Status != 0)
        return Status;
    return fdt_setprop_u32(Fdt, Chosen, "kshim,boot-timing-version", 1U);
}
#endif
