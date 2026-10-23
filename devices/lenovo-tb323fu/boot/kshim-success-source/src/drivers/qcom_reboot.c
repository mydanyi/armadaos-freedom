/* SPDX-License-Identifier: MIT
 * TB322FC/TB323FU reboot targets: stock hardware DT + Qualcomm's published
 * SM8750/SM8850 NVMEM and SCM protocols. No address, vendor RESET2 tuple or
 * dump-mode guess.
 */
#if !defined(KSHIM_HOST_TEST)
#include <config.h>
#endif
#include <qcom_reboot.h>
#include <qcom_dt.h>
#include <psci.h>
#include <string.h>
#if defined(__aarch64__)
#include <timer.h>
#endif

#if CONFIG_KSHIM_QCOM_REBOOT

#define SCM_INFO UINT32_C(0xc2000601)
#define SCM_IO_READ UINT32_C(0xc2000501)
#define SCM_IO_WRITE UINT32_C(0xc2000502)
#define SCM_OEM_REBOOT UINT32_C(0xc3000922)
#define PSCI_RESET2 UINT32_C(0xc4000012)

static int enabled_ancestors(const void *fdt, int node)
{
    return dt_available(fdt, node);
}

static int unique_compatible(const void *fdt, const char *compatible)
{
    int found = -1;
    for (int n = fdt_node_offset_by_compatible(fdt, -1, compatible); n >= 0;
         n = fdt_node_offset_by_compatible(fdt, n, compatible)) {
        if (!enabled_ancestors(fdt, n)) continue;
        if (found >= 0) return -1;
        found = n;
    }
    return found;
}

int kshim_qcom_reboot_parse(const void *fdt,
                          struct kshim_qcom_reboot_config *config)
{
    struct kshim_qcom_reboot_config parsed = {.fdt = fdt};
    int length;
    if (!config || !fdt || fdt_check_header(fdt))
        return KSHIM_QCOM_REBOOT_INVALID_DT;
    /* This driver is additionally opt-in at build time. Mainline board names
     * are accepted only if the actual hardware descriptions below exist. */
    if (fdt_node_check_compatible(fdt, 0, "qcom,canoep") &&
        fdt_node_check_compatible(fdt, 0, "lenovo,tb323fu") &&
        fdt_node_check_compatible(fdt, 0, "lenovo,baldur") &&
        fdt_node_check_compatible(fdt, 0, "qcom,sunp") &&
        /* TB710FU (SM8650): same SCM dload + SDAM restart_reason DT ABI. */
        fdt_node_check_compatible(fdt, 0, "qcom,pineapple"))
        return KSHIM_QCOM_REBOOT_UNSUPPORTED;

    int scm = unique_compatible(fdt, "qcom,scm");
    if (scm < 0) return KSHIM_QCOM_REBOOT_INVALID_DT;
    const fdt32_t *p = fdt_getprop(fdt, scm, "qcom,dload-mode", &length);
    if (!p || length != 8) return KSHIM_QCOM_REBOOT_INVALID_DT;
    int syscon = dt_phandle(fdt, fdt32_to_cpu(p[0]));
    uint32_t offset = fdt32_to_cpu(p[1]);
    struct dt_range range;
    if (syscon < 0 || !enabled_ancestors(fdt, syscon) ||
        fdt_node_check_compatible(fdt, syscon, "syscon") ||
        dt_reg(fdt, syscon, 0, &range) || !range.base ||
        (range.base & 3U) || (offset & 3U) ||
        range.base > UINT64_MAX - offset ||
        !dt_contains(range, range.base + offset, 4))
        return KSHIM_QCOM_REBOOT_INVALID_DT;
    parsed.dload_address = range.base + offset;

    int reason = unique_compatible(fdt, "qcom,reboot-reason");
    if (reason < 0 || fdt_stringlist_count(fdt, reason, "nvmem-cell-names") != 1 ||
        fdt_stringlist_search(fdt, reason, "nvmem-cell-names", "restart_reason") != 0)
        return KSHIM_QCOM_REBOOT_INVALID_DT;
    p = fdt_getprop(fdt, reason, "nvmem-cells", &length);
    if (!p || length != 4) return KSHIM_QCOM_REBOOT_INVALID_DT;
    int cell = dt_phandle(fdt, fdt32_to_cpu(p[0]));
    if (cell < 0 || !enabled_ancestors(fdt, cell))
        return KSHIM_QCOM_REBOOT_INVALID_DT;
    int provider = fdt_parent_offset(fdt, cell);
    uint32_t ac, sc, base;
    if (provider < 0 ||
        fdt_node_check_compatible(fdt, provider, "qcom,spmi-sdam") ||
        dt_u32(fdt, provider, "#address-cells", &ac) || ac != 1 ||
        dt_u32(fdt, provider, "#size-cells", &sc) || sc != 1 ||
        dt_u32(fdt, provider, "reg", &base) || !base || (base & 0xffU) ||
        base > UINT16_MAX - 255U ||
        kshim_dt_spmi_identity(fdt, provider, &parsed.controller,
                              &parsed.bus, &parsed.sid))
        return KSHIM_QCOM_REBOOT_INVALID_DT;
    p = fdt_getprop(fdt, cell, "reg", &length);
    if (!p || length != 8 || fdt32_to_cpu(p[1]) != 1)
        return KSHIM_QCOM_REBOOT_INVALID_DT;
    offset = fdt32_to_cpu(p[0]);
    /* SDAM user-data cells start at 0x40. The published reboot ABI has a
     * seven-bit reason beginning at bit one; do not reinterpret other cells. */
    if (offset < 0x40 || offset > 0xff)
        return KSHIM_QCOM_REBOOT_INVALID_DT;
    p = fdt_getprop(fdt, cell, "bits", &length);
    if (!p || length != 8 || fdt32_to_cpu(p[0]) != 1 ||
        fdt32_to_cpu(p[1]) != 7)
        return KSHIM_QCOM_REBOOT_INVALID_DT;
    parsed.reason_address = (uint16_t)(base + offset);
    parsed.reason_shift = 1;
    parsed.reason_mask = 0xfe;
    *config = parsed;
    return 0;
}

__attribute__((weak)) void kshim_qcom_reboot_smccc(uint64_t registers[8])
{
#if defined(__aarch64__)
    register uint64_t x0 __asm__("x0") = registers[0];
    register uint64_t x1 __asm__("x1") = registers[1];
    register uint64_t x2 __asm__("x2") = registers[2];
    register uint64_t x3 __asm__("x3") = registers[3];
    register uint64_t x4 __asm__("x4") = registers[4];
    register uint64_t x5 __asm__("x5") = registers[5];
    register uint64_t x6 __asm__("x6") = registers[6];
    register uint64_t x7 __asm__("x7") = registers[7];
    __asm__ volatile("smc #0" : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3),
                     "+r"(x4), "+r"(x5), "+r"(x6), "+r"(x7) : :
                     "x8", "x9", "x10", "x11", "x12", "x13", "x14",
                     "x15", "x16", "x17", "memory", "cc");
    registers[0] = x0; registers[1] = x1; registers[2] = x2; registers[3] = x3;
    registers[4] = x4; registers[5] = x5; registers[6] = x6; registers[7] = x7;
#else
    registers[0] = UINT64_MAX;
#endif
}

static int scm_call(uint32_t function, uint32_t count, uint64_t arg0,
                    uint64_t arg1, uint32_t *value)
{
    uint64_t r[8] = {function, count, arg0, arg1, 0, 0, 0, 0};
    /* Qualcomm's interrupted-call continuation preserves the returned A6;
     * original arguments are restored and A0 becomes SCM_INTERRUPTED=1. */
    for (;;) {
        kshim_qcom_reboot_smccc(r);
        if (r[0] != 1) break;
        r[1] = count; r[2] = arg0; r[3] = arg1;
        r[4] = 0; r[5] = 0; r[7] = 0;
    }
    if (r[0] != 0) return KSHIM_QCOM_REBOOT_IO_FAILED;
    if (value) *value = (uint32_t)r[1];
    return 0;
}

static int scm_available(uint32_t query)
{
    uint32_t available = 0;
    return scm_call(SCM_INFO, 1, query, 0, &available) == 0 && available == 1;
}

static int dload_read(const struct kshim_qcom_reboot_config *c, uint32_t *value)
{
    return scm_call(SCM_IO_READ, 1, c->dload_address, 0, value);
}

static int dload_write(const struct kshim_qcom_reboot_config *c, uint32_t value)
{
    return scm_call(SCM_IO_WRITE, 2, c->dload_address, value, NULL);
}

/* A separate adapter leaves the key-input adapter's read-only contract intact.
 * It never configures interrupts or PMIC ownership. CrDK refuses foreign-EE
 * write channels before the cookie write can be submitted. */
static struct {
    SpmiDeviceContext controller;
    struct kshim_qcom_reboot_config config;
    uintptr_t read_command, write_command, write_data;
    int fault, writing, ready;
} reboot_spmi;

static int region_contains(const SpmiMemoryRegion *region, uintptr_t address,
                           size_t bytes)
{
    return region->BaseAddress && region->Size >= bytes &&
           region->BaseAddress <= UINTPTR_MAX - (region->Size - 1) &&
           address >= region->BaseAddress &&
           address - region->BaseAddress <= region->Size - bytes &&
           !(address & 3U);
}

static int spmi_address_valid(uintptr_t address)
{
    for (unsigned i = 0; i < SPMI_MEMORY_REGION_TYPE_MAX; ++i)
        if (region_contains(&reboot_spmi.controller.Regions[i], address, 4))
            return 1;
    reboot_spmi.fault = 1;
    return 0;
}

static void barrier(void)
{
#if defined(__aarch64__)
    __asm__ volatile("dsb sy" ::: "memory");
#else
    __asm__ volatile("" ::: "memory");
#endif
}

static UINT32 spmi_mmio_read(void *context, UINTN address)
{
    (void)context;
    if (!spmi_address_valid(address)) return 0;
    uint32_t value = *(volatile uint32_t *)address;
    barrier();
    return value;
}

static void spmi_mmio_write(void *context, UINTN address, UINT32 value)
{
    (void)context;
    if (!spmi_address_valid(address) ||
        (address != reboot_spmi.read_command &&
         !(reboot_spmi.writing && (address == reboot_spmi.write_command ||
                                   address == reboot_spmi.write_data)))) {
        reboot_spmi.fault = 1;
        return;
    }
    barrier();
    *(volatile uint32_t *)address = value;
    barrier();
}

static void spmi_stall(void *context, UINT32 us)
{
    (void)context;
#if defined(__aarch64__)
    (void)delay(us);
#else
    (void)us;
#endif
}

__attribute__((weak)) int kshim_qcom_reboot_spmi_prepare(
    const struct kshim_qcom_reboot_config *c)
{
    const struct kshim_resources *resources = kshim_resources_get();
    memset(&reboot_spmi, 0, sizeof(reboot_spmi));
    if (!resources || resources->fdt != c->fdt || resources->spmi_status ||
        c->controller >= resources->spmi_count)
        return -1;
    const SpmiDeviceContext *source = &resources->spmi[c->controller].controller;
    if (source->BusId != c->bus) return -1;
    SpmiDeviceContext *ctx = &reboot_spmi.controller;
    memcpy(ctx->Regions, source->Regions, sizeof(ctx->Regions));
    ctx->ActiveEE = source->ActiveEE;
    ctx->Channel = source->Channel;
    ctx->BusId = source->BusId;
    ctx->Io = (SpmiIoOps){spmi_mmio_read, spmi_mmio_write, spmi_stall, NULL};
    if (CR_ERROR(SpmiLibInit(&ctx)) || reboot_spmi.fault ||
        (ctx->Version != SPMI_ARB_VERSION_8 && ctx->Version != SPMI_ARB_VERSION_8P5))
        return -1;
    uint32_t read = ctx->PmicArb.Ops->GetChannelOffset(
        ctx, c->sid, c->reason_address, SPMI_ARB_CHANNEL_OBS);
    uint32_t write = ctx->PmicArb.Ops->GetChannelOffset(
        ctx, c->sid, c->reason_address, SPMI_ARB_CHANNEL_RW);
    SpmiMemoryRegion *rr = &ctx->Regions[SPMI_MEMORY_REGION_TYPE_OBSERVER];
    SpmiMemoryRegion *wr = &ctx->Regions[SPMI_MEMORY_REGION_TYPE_CH_SLAVES];
    if (read == UINT32_MAX || write == UINT32_MAX || reboot_spmi.fault ||
        rr->BaseAddress > UINTPTR_MAX - read ||
        wr->BaseAddress > UINTPTR_MAX - write ||
        !region_contains(rr, rr->BaseAddress + read, SPMI_PMIC_ARB_RDATA0 + 4) ||
        !region_contains(wr, wr->BaseAddress + write, SPMI_PMIC_ARB_WDATA0 + 4))
        return -1;
    reboot_spmi.read_command = rr->BaseAddress + read + SPMI_PMIC_ARB_CMD;
    reboot_spmi.write_command = wr->BaseAddress + write + SPMI_PMIC_ARB_CMD;
    reboot_spmi.write_data = wr->BaseAddress + write + SPMI_PMIC_ARB_WDATA0;
    reboot_spmi.config = *c;
    reboot_spmi.ready = 1;
    return 0;
}

static int spmi_matches(const struct kshim_qcom_reboot_config *c)
{
    const struct kshim_qcom_reboot_config *saved = &reboot_spmi.config;
    return reboot_spmi.ready && c->fdt == saved->fdt &&
           c->controller == saved->controller && c->bus == saved->bus &&
           c->sid == saved->sid && c->reason_address == saved->reason_address;
}

__attribute__((weak)) int kshim_qcom_reboot_spmi_read(
    const struct kshim_qcom_reboot_config *c, uint8_t *value)
{
    if (!value || !spmi_matches(c)) return -1;
    reboot_spmi.fault = 0;
    CR_STATUS status = SpmiRead(&reboot_spmi.controller, c->sid,
                                c->reason_address, value, 1);
    return CR_ERROR(status) || reboot_spmi.fault ? -1 : 0;
}

__attribute__((weak)) int kshim_qcom_reboot_spmi_write(
    const struct kshim_qcom_reboot_config *c, uint8_t value)
{
    if (!spmi_matches(c)) return -1;
    reboot_spmi.fault = 0;
    reboot_spmi.writing = 1;
    CR_STATUS status = SpmiWrite(&reboot_spmi.controller, c->sid,
                                 c->reason_address, &value, 1);
    reboot_spmi.writing = 0;
    return CR_ERROR(status) || reboot_spmi.fault ? -1 : 0;
}

int kshim_qcom_reboot_request(const void *fdt, enum kshim_qcom_reboot_target target)
{
    struct kshim_qcom_reboot_config c;
    uint32_t old_dload, actual_dload;
    uint8_t old_reason = 0, actual_reason;
    int changed_dload = 0, changed_reason = 0;
    int error, rollback_failed = 0, oem_reset = 0;
    if ((unsigned)target > KSHIM_QCOM_REBOOT_EDL)
        return KSHIM_QCOM_REBOOT_UNSUPPORTED;
    error = kshim_qcom_reboot_parse(fdt, &c);
    if (error) return error;
    /* Query capabilities before changing either reboot cookie. */
    if (kshim_psci_init(fdt) || kshim_psci_conduit() != KSHIM_PSCI_CONDUIT_SMC)
        return KSHIM_QCOM_REBOOT_UNSUPPORTED;
    if (!scm_available(UINT32_C(0x02000601)) ||
        !scm_available(UINT32_C(0x02000501)) ||
        !scm_available(UINT32_C(0x02000502)))
        return KSHIM_QCOM_REBOOT_UNSUPPORTED;
    if (target == KSHIM_QCOM_REBOOT_EDL) {
        /* Qualcomm's warm-restart notifier prefers the OEM reboot service.
         * Choose one supported warm-reset path before changing cookies;
         * never try another reset after the selected call has returned. */
        oem_reset = scm_available(UINT32_C(0x03000922));
        if (!oem_reset && (kshim_psci_version() < UINT32_C(0x10001) ||
                           kshim_psci_features(PSCI_RESET2) != 0))
            return KSHIM_QCOM_REBOOT_UNSUPPORTED;
    }
    if (dload_read(&c, &old_dload)) return KSHIM_QCOM_REBOOT_IO_FAILED;
    if (target != KSHIM_QCOM_REBOOT_EDL &&
        (kshim_qcom_reboot_spmi_prepare(&c) ||
         kshim_qcom_reboot_spmi_read(&c, &old_reason)))
        return KSHIM_QCOM_REBOOT_IO_FAILED;

    uint32_t desired_dload = target == KSHIM_QCOM_REBOOT_EDL ? 1U : 0U;
    /* A failed write can still have reached hardware: include it in rollback. */
    changed_dload = 1;
    error = dload_write(&c, desired_dload);
    if (error) goto rollback;
    error = KSHIM_QCOM_REBOOT_VERIFY_FAILED;
    if (dload_read(&c, &actual_dload) || actual_dload != desired_dload)
        goto rollback;
    if (target != KSHIM_QCOM_REBOOT_EDL) {
        unsigned reason = target == KSHIM_QCOM_REBOOT_RECOVERY ? 1U : 2U;
        uint8_t desired = (old_reason & (uint8_t)~c.reason_mask) |
                          (uint8_t)(reason << c.reason_shift);
        changed_reason = 1;
        error = KSHIM_QCOM_REBOOT_IO_FAILED;
        if (kshim_qcom_reboot_spmi_write(&c, desired)) goto rollback;
        error = KSHIM_QCOM_REBOOT_VERIFY_FAILED;
        if (kshim_qcom_reboot_spmi_read(&c, &actual_reason) || actual_reason != desired)
            goto rollback;
    }
    barrier();
    if (target == KSHIM_QCOM_REBOOT_EDL) {
        if (oem_reset)
            (void)scm_call(SCM_OEM_REBOOT, 0, 0, 0, NULL);
        else
            (void)kshim_psci_call(PSCI_RESET2, 0, 0, 0);
    } else {
        (void)kshim_psci_request_system_reset();
    }
    error = KSHIM_QCOM_REBOOT_RESET_RETURNED;
rollback:
    if (changed_reason) {
        if (kshim_qcom_reboot_spmi_write(&c, old_reason)) rollback_failed = 1;
        if (kshim_qcom_reboot_spmi_read(&c, &actual_reason) || actual_reason != old_reason)
            rollback_failed = 1;
    }
    if (changed_dload) {
        if (dload_write(&c, old_dload)) rollback_failed = 1;
        if (dload_read(&c, &actual_dload) || actual_dload != old_dload)
            rollback_failed = 1;
    }
    reboot_spmi.ready = 0;
    return rollback_failed ? KSHIM_QCOM_REBOOT_ROLLBACK_FAILED : error;
}

#else
int kshim_qcom_reboot_parse(const void *fdt, struct kshim_qcom_reboot_config *config)
{
    (void)fdt; (void)config;
    return KSHIM_QCOM_REBOOT_UNSUPPORTED;
}
int kshim_qcom_reboot_request(const void *fdt, enum kshim_qcom_reboot_target target)
{
    (void)fdt; (void)target;
    return KSHIM_QCOM_REBOOT_UNSUPPORTED;
}
#endif
