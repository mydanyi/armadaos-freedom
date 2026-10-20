/* SPDX-License-Identifier: MIT */
#include <kshim.h>
#include <platform.h>
#include <string.h>
#include <qcom_dt.h>
#include <smp.h>
#include <smp_selftest.h>
#include <psci.h>
#include <kshim_mmu.h>
#include <st_ftm5.h>
#include <kshim_lvgl_os.h>
#include <kshim_console.h>
#include <timer.h>
#include <config.h>
#include <menu_text.h>
#include <framebuffer.h>
#include <lvgl_port.h>
#include <qcom_key_platform.h>
#include <qcom_keys.h>
#include <qcom_keys_fdt.h>
#include <uefi_boot.h>
#include <dt.h>
#include <device_info.h>
#include <boot_settings.h>
#include <boot_metrics.h>
#if CONFIG_KSHIM_MENU_ADVANCED_ACTIONS
#include <qcom_reboot.h>
#endif

/* With the runtime language switch on, every user-visible string resolves
 * through the locale table so the live selection flips platform text too; the
 * compile-time literals below stay authoritative when the switch is off. */
#if defined(CONFIG_KSHIM_MENU_LANGUAGE_SWITCH) && CONFIG_KSHIM_MENU_LANGUAGE_SWITCH
#define KSHIM_STR_KEYS_HINT      kshim_menu_text(KSHIM_TEXT_KEYS_HINT)
#define KSHIM_STR_KEYS_UNAVAIL   kshim_menu_text(KSHIM_TEXT_KEYS_UNAVAILABLE)
#define KSHIM_STR_AUTO_CANCELLED kshim_menu_text(KSHIM_TEXT_AUTO_CANCELLED)
#define KSHIM_STR_POWER_FAILED   kshim_menu_text(KSHIM_TEXT_POWER_FAILED)
#define KSHIM_STR_RESTART        kshim_menu_text(KSHIM_TEXT_RESTART)
#define KSHIM_STR_POWER_OFF      kshim_menu_text(KSHIM_TEXT_POWER_OFF)
#define KSHIM_STR_ADVANCED       kshim_menu_text(KSHIM_TEXT_ADVANCED)
#define KSHIM_STR_RECOVERY       kshim_menu_text(KSHIM_TEXT_RECOVERY)
#define KSHIM_STR_FASTBOOT       kshim_menu_text(KSHIM_TEXT_FASTBOOT)
#define KSHIM_STR_EDL            kshim_menu_text(KSHIM_TEXT_EDL)
#define KSHIM_STR_BACK           kshim_menu_text(KSHIM_TEXT_BACK)
#define KSHIM_STR_MODE_FAILED    kshim_menu_text(KSHIM_TEXT_MODE_FAILED)
#define KSHIM_STR_RESTORE_FAILED kshim_menu_text(KSHIM_TEXT_RESTORE_FAILED)
#define KSHIM_STR_MENU_TITLE     kshim_menu_text(KSHIM_TEXT_MENU_TITLE)
#define KSHIM_STR_MENU_PROMPT    kshim_menu_text(KSHIM_TEXT_MENU_PROMPT)
#define KSHIM_STR_MENU_DEFAULT   kshim_menu_text(KSHIM_TEXT_MENU_DEFAULT)
#define KSHIM_STR_HANDOFF_UNSAFE kshim_menu_text(KSHIM_TEXT_HANDOFF_UNSAFE)
#define KSHIM_STR_UNABLE_START   kshim_menu_text(KSHIM_TEXT_UNABLE_START)
#elif CONFIG_KSHIM_MENU_CHINESE
#define KSHIM_STR_KEYS_HINT      "音量键选择，电源键启动"
#define KSHIM_STR_KEYS_UNAVAIL   "音量键暂不可用"
#define KSHIM_STR_AUTO_CANCELLED "已取消自动启动，请手动选择"
#define KSHIM_STR_POWER_FAILED   "电源操作失败"
#define KSHIM_STR_RESTART        "重启"
#define KSHIM_STR_POWER_OFF      "关机"
#define KSHIM_STR_ADVANCED       "高级启动模式"
#define KSHIM_STR_RECOVERY       "Recovery 恢复模式"
#define KSHIM_STR_FASTBOOT       "Fastboot 模式"
#define KSHIM_STR_EDL            "9008 下载模式"
#define KSHIM_STR_BACK           "返回主菜单"
#define KSHIM_STR_MODE_FAILED    "切换启动模式失败"
#define KSHIM_STR_RESTORE_FAILED "切换启动模式失败，恢复原设置失败"
#define KSHIM_STR_MENU_TITLE     "KShimC 启动器"
#define KSHIM_STR_MENU_PROMPT    "输入序号并按回车确认："
#define KSHIM_STR_MENU_DEFAULT   " [默认]"
#define KSHIM_STR_HANDOFF_UNSAFE "Unsafe UI shutdown; refusing payload handoff"
#define KSHIM_STR_UNABLE_START   "Unable to start selected image"
#else
#define KSHIM_STR_KEYS_HINT      "Use vol keys, power to boot"
#define KSHIM_STR_KEYS_UNAVAIL   "Volume keys unavailable"
#define KSHIM_STR_AUTO_CANCELLED "Auto boot cancelled; choose an entry"
#define KSHIM_STR_POWER_FAILED   "Power action failed"
#define KSHIM_STR_RESTART        "Restart"
#define KSHIM_STR_POWER_OFF      "Power off"
#define KSHIM_STR_ADVANCED       "Advanced boot modes"
#define KSHIM_STR_RECOVERY       "Enter Recovery"
#define KSHIM_STR_FASTBOOT       "Enter Fastboot"
#define KSHIM_STR_EDL            "Enter 9008"
#define KSHIM_STR_BACK           "Back to main menu"
#define KSHIM_STR_MODE_FAILED    "Boot mode switch failed"
#define KSHIM_STR_RESTORE_FAILED "Boot mode switch failed; restoring the original settings failed"
#define KSHIM_STR_MENU_TITLE     "KShim boot menu"
#define KSHIM_STR_MENU_PROMPT    "Select kernel and press Enter: "
#define KSHIM_STR_MENU_DEFAULT   " [default]"
#define KSHIM_STR_HANDOFF_UNSAFE "Unsafe UI shutdown; refusing payload handoff"
#define KSHIM_STR_UNABLE_START   "Unable to start selected image"
#endif

/* Session-settings strings. These only drive the new entry point, so they may
 * resolve through the locale table with or without the runtime switch. */
#define KSHIM_STR_UNAVAILABLE kshim_menu_text(KSHIM_TEXT_UNAVAILABLE)

/* kShimC build version shown on the Device info page. The optional
 * KSHIM_BUILD_VERSION compile definition is already a quoted C string literal
 * (supplied by the build), so it is used directly; an absent definition is an
 * explicit NULL so the field renders as the localized Unknown. */
#if defined(KSHIM_BUILD_VERSION)
#define KSHIM_INFO_VERSION_TEXT KSHIM_BUILD_VERSION
#else
#define KSHIM_INFO_VERSION_TEXT NULL
#endif

/* Read-only Device info page: one main row plus a fixed set of field rows and
 * a final Back row. Values are formatted into menu-owned buffers so the
 * pointers stay valid for the whole menu session. */
#define KSHIM_INFO_FIELD_COUNT 7
#define KSHIM_INFO_VALUE_LEN 192
/* Compatible values render as at most three whole-entry lines; each line is
 * clipped to this many ASCII characters before the shared ellipsis marker. */
#define KSHIM_INFO_COMPATIBLE_LINES 3
#define KSHIM_INFO_COMPATIBLE_LINE_LEN 44
/* The DT model shares the same readable bound so a long string cannot fill the
 * page; any cut still lands on a UTF-8 boundary. */
#define KSHIM_INFO_MODEL_MAX 120
#define KSHIM_INFO_ELLIPSIS "\xe2\x80\xa6" /* U+2026, 3 UTF-8 bytes */

/* Enables the runtime language page and role driven payload labels. Guarded so
 * the plain (switch off) builds never reference the locale module. */
#if defined(CONFIG_KSHIM_MENU_LANGUAGE_SWITCH) && CONFIG_KSHIM_MENU_LANGUAGE_SWITCH
#define KSHIM_MENU_LANGUAGE_ENABLED 1
#else
#define KSHIM_MENU_LANGUAGE_ENABLED 0
#endif

__attribute__((weak)) int kshim_menu_activate(kshim_menu_action_t action)
{ (void)action; return -1; }
#if CONFIG_KSHIM_SMP
static int SmpInitialized;
#endif
static QcomKeys Keys;
static int build_keys(const void *fdt)
{
#if CONFIG_KSHIM_DIAGNOSTIC_UI_ONLY
    (void)fdt; return -1;
#elif CONFIG_QCOM_KEYS
    QcomKeyDescriptor descriptors[QCOM_KEYS_MAX];
    QcomKeyFdtAdapter adapter;
    size_t count = 0;
    if (!fdt || QcomKeyFdtAdapterInit(&adapter, fdt))
        return -1;
    /* The resources phase has already closed its DT index. Reopen it only
     * for parsing this validated blob, then release it before key setup. */
    dt_parent_index_begin(fdt);
    int status = QcomKeysBuildFromFdt(&adapter.Ops, descriptors, QCOM_KEYS_MAX, &count);
    dt_parent_index_end();
    if (status || !count)
        return -1;
    status = QcomKeysInitEx(&Keys, descriptors, count, kshim_qcom_key_read32,
                            kshim_qcom_key_read32_status, NULL);
    QcomKeysSetDebounce(&Keys, CONFIG_QCOM_KEY_DEBOUNCE_SAMPLES);
    return status;
#else
    (void)fdt; return -1;
#endif
}
#if CONFIG_KSHIM_FRAMEBUFFER && CONFIG_KSHIM_LVGL
static kshim_framebuffer_t Framebuffer;
static kshim_lvgl_t Lvgl;
static uint64_t LastTime;
#if CONFIG_KSHIM_DIAGNOSTIC_UI_ONLY
static int UiInit = -9;
static uint32_t UiFrames;
#if CONFIG_KSHIM_DIAGNOSTIC_KEYS_DESCRIPTORS_ONLY
static int UiKeysParse = -9;
static int UiKeysInit = -9;
static int UiKeysMmu = -9;
static uint64_t UiKeysMs;
static size_t UiKeysCount;
static QcomKeyDescriptor UiKeyDescriptors[QCOM_KEYS_MAX];
#if CONFIG_KSHIM_DIAGNOSTIC_GPIO_SINGLE_READ
static int UiGpioStatus = -9, UiGpioMmu = -9;
static uint32_t UiGpioValue, UiGpioReads;
#endif
#if CONFIG_KSHIM_DIAGNOSTIC_SPMI_CORE_SINGLE_READ && CONFIG_KSHIM_DIAGNOSTIC_GPIO_SINGLE_READ
static int UiSpmiCoreStatus = -9, UiSpmiCoreMmu = -9;
static uint32_t UiSpmiCoreRaw, UiSpmiCoreReads;
static uint32_t UiSpmiCoreCount, UiSpmiCoreController;
static uint64_t UiSpmiCoreAddress;
#endif
#if CONFIG_KSHIM_DIAGNOSTIC_PON_SINGLE_READ && CONFIG_KSHIM_DIAGNOSTIC_SPMI_CORE_SINGLE_READ && CONFIG_KSHIM_DIAGNOSTIC_GPIO_SINGLE_READ
static int UiPonStatus = -9, UiPonMmu = -9, UiPonController = -1, UiPonBus = -1, UiPonSid = -1, UiPonInit = 0;
static uint32_t UiPonValue, UiPonReads, UiPonRegister;
#endif
#if CONFIG_KSHIM_DIAGNOSTIC_MENU_KEYS && CONFIG_KSHIM_DIAGNOSTIC_GPIO_SINGLE_READ && CONFIG_KSHIM_DIAGNOSTIC_SPMI_CORE_SINGLE_READ && CONFIG_KSHIM_DIAGNOSTIC_PON_SINGLE_READ
static int UiInputStatus = -9, UiInputEnabled = 0, UiInputMmu = -9;
static uint32_t UiInputGpioReads, UiInputPonReads, UiInputErrors;
static int ui_menu_read32_status(void *context, uint64_t address, uint32_t *value)
{
    UiInputMmu = kshim_mmu_is_enabled();
    if (QCOM_KEY_IS_SPMI_TOKEN(address)) ++UiInputPonReads;
    else ++UiInputGpioReads;
    int status = kshim_qcom_key_read32_status(context, address, value);
    if (status) ++UiInputErrors;
    return status;
}
#endif
static void ui_prepare_key_descriptors(const void *fdt)
{
    UiKeysParse = -9;
    UiKeysInit = -9;
    UiKeysCount = 0;
    UiKeysMmu = kshim_mmu_is_enabled();
    uint64_t start = get_current_time();
    dt_parent_index_begin(fdt);
    UiKeysParse = QcomKeysBuildFromDeviceTree(fdt, UiKeyDescriptors, QCOM_KEYS_MAX, &UiKeysCount);
    dt_parent_index_end();
    if (UiKeysParse == 0 && UiKeysCount > 0 && UiKeysCount <= QCOM_KEYS_MAX)
        UiKeysInit = QcomKeysInitEx(&Keys, UiKeyDescriptors, UiKeysCount, NULL, NULL, NULL);
    UiKeysMs = get_current_time() - start;
#if CONFIG_KSHIM_DIAGNOSTIC_GPIO_SINGLE_READ
    UiGpioStatus = -9;
    UiGpioMmu = kshim_mmu_is_enabled();
    UiGpioValue = UINT32_MAX;
    UiGpioReads = 0;
    const QcomKeyDescriptor *gro = NULL;
    size_t gro_count = 0;
    if (UiKeysParse == 0 && UiKeysInit == 0 && UiKeysCount <= QCOM_KEYS_MAX) {
        for (size_t i = 0; i < UiKeysCount; ++i) {
            if (UiKeyDescriptors[i].Code == QCOM_KEYCODE_VOLUMEUP &&
                UiKeyDescriptors[i].Source == QCOM_KEY_SOURCE_GPIO) {
                gro = &UiKeyDescriptors[i];
                ++gro_count;
            }
        }
    }
    if (gro_count == 1 && gro && UiGpioMmu == 1 && !QCOM_KEY_IS_SPMI_TOKEN(gro->Base) &&
        gro->Base != 0 && gro->Base <= UINT64_MAX - gro->RegisterOffset) {
        uint64_t gro_address = gro->Base + gro->RegisterOffset;
        if (gro_address != 0 && (gro_address & 0x3) == 0) {
            UiGpioReads = 1;
            UiGpioStatus = kshim_qcom_key_read32_status(NULL, gro_address, &UiGpioValue);
        }
    }
#endif
#if CONFIG_KSHIM_DIAGNOSTIC_SPMI_CORE_SINGLE_READ && CONFIG_KSHIM_DIAGNOSTIC_GPIO_SINGLE_READ
    UiSpmiCoreStatus = -9;
    UiSpmiCoreMmu = kshim_mmu_is_enabled();
    UiSpmiCoreRaw = UINT32_MAX;
    UiSpmiCoreReads = 0;
    UiSpmiCoreCount = 0;
    UiSpmiCoreController = UINT32_MAX;
    UiSpmiCoreAddress = 0;
    struct kshim_resources *sr = kshim_resources_get();
    UiSpmiCoreCount = sr ? sr->spmi_count : 0;
    if (UiSpmiCoreMmu == 1 && UiKeysParse == 0 && UiKeysInit == 0 &&
        UiGpioStatus == 0 && UiGpioReads == 1 && sr != NULL && sr->spmi_status == 0 &&
        UiSpmiCoreCount > 0 && UiSpmiCoreCount <= KSHIM_SPMI_MAX) {
        uint32_t decoded = UINT32_MAX;
        size_t matches = 0;
        for (size_t i = 0; i < UiKeysCount && i < QCOM_KEYS_MAX; ++i) {
            const QcomKeyDescriptor *d = &UiKeyDescriptors[i];
            if (d->Code == QCOM_KEYCODE_POWER && d->Source == QCOM_KEY_SOURCE_PON &&
                QCOM_KEY_IS_SPMI_TOKEN(d->Base)) {
                decoded = QCOM_KEY_SPMI_TOKEN_CONTROLLER(d->Base);
                ++matches;
            }
        }
        if (matches == 1 && decoded < sr->spmi_count) {
            uint64_t base = sr->spmi[decoded].controller.Regions[SPMI_MEMORY_REGION_TYPE_CORE].BaseAddress;
            uint64_t size = sr->spmi[decoded].controller.Regions[SPMI_MEMORY_REGION_TYPE_CORE].Size;
            uint64_t offset = SPMI_PMIC_ARB_REG_VERSION;
            UiSpmiCoreController = decoded;
            if (base != 0 && size >= offset + sizeof(uint32_t) && base <= UINTPTR_MAX - offset) {
                uint64_t addr = base + offset;
                if (addr <= UINTPTR_MAX - 3 && (addr & 0x3) == 0) {
                    UiSpmiCoreAddress = addr;
                    UiSpmiCoreReads = 1;
                    UiSpmiCoreRaw = *(volatile uint32_t *)(uintptr_t)addr;
                    __asm__ volatile("dsb sy" ::: "memory");
                    UiSpmiCoreStatus = 0;
                }
            }
        }
    }
#endif
#if CONFIG_KSHIM_DIAGNOSTIC_PON_SINGLE_READ && CONFIG_KSHIM_DIAGNOSTIC_SPMI_CORE_SINGLE_READ && CONFIG_KSHIM_DIAGNOSTIC_GPIO_SINGLE_READ
    UiPonStatus = -9;
    UiPonMmu = kshim_mmu_is_enabled();
    UiPonValue = UINT32_MAX;
    UiPonReads = 0;
    UiPonRegister = 0;
    UiPonInit = 0;
    UiPonController = -1;
    UiPonBus = -1;
    UiPonSid = -1;
    struct kshim_resources *pr = kshim_resources_get();
    if (UiPonMmu == 1 && UiKeysParse == 0 && UiKeysInit == 0 &&
        UiSpmiCoreStatus == 0 && UiSpmiCoreReads == 1 &&
        pr != NULL && pr->spmi_status == 0 &&
        pr->spmi_count > 0 && pr->spmi_count <= KSHIM_SPMI_MAX &&
        UiSpmiCoreRaw >= UINT32_C(0x80000000) && UiSpmiCoreRaw < UINT32_C(0x80050000)) {
        const QcomKeyDescriptor *pd = NULL;
        size_t pmatches = 0;
        for (size_t i = 0; i < UiKeysCount && i < QCOM_KEYS_MAX; ++i) {
            const QcomKeyDescriptor *d = &UiKeyDescriptors[i];
            if (d->Code == QCOM_KEYCODE_POWER && d->Source == QCOM_KEY_SOURCE_PON &&
                QCOM_KEY_IS_SPMI_TOKEN(d->Base)) {
                pd = d;
                ++pmatches;
            }
        }
        if (pmatches == 1 && pd != NULL) {
            uint32_t ctrl = (uint32_t)QCOM_KEY_SPMI_TOKEN_CONTROLLER(pd->Base);
            uint32_t bus = (uint32_t)QCOM_KEY_SPMI_TOKEN_BUS(pd->Base);
            uint32_t sid = (uint32_t)QCOM_KEY_SPMI_TOKEN_SID(pd->Base);
            uint32_t baseReg = (uint32_t)QCOM_KEY_SPMI_TOKEN_ADDRESS(pd->Base);
            if (!(pd->Base & QCOM_KEY_PMIC_GPIO_FLAG) &&
                ctrl == UiSpmiCoreController && ctrl < pr->spmi_count &&
                !pr->spmi[ctrl].controller.HasInterrupt &&
                sid <= 15 && baseReg <= UINT16_MAX &&
                pd->RegisterOffset <= UINT16_MAX - baseReg) {
                UiPonController = (int)ctrl;
                UiPonBus = (int)bus;
                UiPonSid = (int)sid;
                UiPonRegister = baseReg + pd->RegisterOffset;
                UiPonReads = 1;
                UiPonStatus = kshim_qcom_key_read32_status(NULL, pd->Base + pd->RegisterOffset, &UiPonValue);
                UiPonInit = pr->spmi[ctrl].controller.Initialized ? 1 : 0;
            }
        }
    }
#endif
#if CONFIG_KSHIM_DIAGNOSTIC_MENU_KEYS && CONFIG_KSHIM_DIAGNOSTIC_GPIO_SINGLE_READ && CONFIG_KSHIM_DIAGNOSTIC_SPMI_CORE_SINGLE_READ && CONFIG_KSHIM_DIAGNOSTIC_PON_SINGLE_READ
    UiInputStatus = -9;
    UiInputEnabled = 0;
    UiInputGpioReads = 0;
    UiInputPonReads = 0;
    UiInputErrors = 0;
    UiInputMmu = kshim_mmu_is_enabled();
    if (UiInputMmu == 1 && UiKeysParse == 0 && UiKeysInit == 0 &&
        UiKeysCount > 0 && UiKeysCount <= QCOM_KEYS_MAX &&
        UiGpioStatus == 0 && UiGpioReads == 1 &&
        UiSpmiCoreStatus == 0 && UiSpmiCoreReads == 1 &&
        UiPonStatus == 0 && UiPonReads == 1 && UiPonInit == 1) {
        UiInputStatus = QcomKeysInitEx(&Keys, UiKeyDescriptors, UiKeysCount,
                                       kshim_qcom_key_read32, ui_menu_read32_status, NULL);
        if (UiInputStatus == 0)
            QcomKeysSetDebounce(&Keys, CONFIG_QCOM_KEY_DEBOUNCE_SAMPLES);
    }
#endif
}
static void ui_report_key_descriptors(void *fdt)
{
    char report[81];
    int n = snprintf(report, sizeof(report),
        "r25:parse=%d:init=%d:n=%u", UiKeysParse, UiKeysInit, (unsigned)UiKeysCount);
    if (n > 0 && n < (int)sizeof(report)) dt_probe_report_slot(fdt, 2, report);
    const uint32_t codes[3] = {QCOM_KEYCODE_VOLUMEUP, QCOM_KEYCODE_VOLUMEDOWN, QCOM_KEYCODE_POWER};
    size_t limit = UiKeysCount < QCOM_KEYS_MAX ? UiKeysCount : QCOM_KEYS_MAX;
    for (size_t slot = 0; slot < 3; ++slot) {
        const QcomKeyDescriptor *found = NULL;
        for (size_t i = 0; i < limit; ++i)
            if (UiKeyDescriptors[i].Code == codes[slot]) { found = &UiKeyDescriptors[i]; break; }
        if (found)
            n = snprintf(report, sizeof(report),
                "r25:k=%u:a=%u:s=%u:b=%llx:o=%x:m=%x:l=%u",
                (unsigned)found->Code, (unsigned)found->Action, (unsigned)found->Source,
                (unsigned long long)found->Base, (unsigned)found->RegisterOffset,
                (unsigned)found->Mask, (unsigned)found->ActiveLow);
        else
            n = snprintf(report, sizeof(report), "r25:k=%u:missing=1", (unsigned)codes[slot]);
        if (n > 0 && n < (int)sizeof(report)) dt_probe_report_slot(fdt, 3 + slot, report);
    }
#if CONFIG_KSHIM_DIAGNOSTIC_PON_SINGLE_READ && CONFIG_KSHIM_DIAGNOSTIC_SPMI_CORE_SINGLE_READ && CONFIG_KSHIM_DIAGNOSTIC_GPIO_SINGLE_READ
    n = snprintf(report, sizeof(report), "r32:rc=%d:v=%u:n=%u:mmu=%d:ctrl=%d:bus=%d:sid=%d:reg=%x:init=%d",
        UiPonStatus, (unsigned)UiPonValue, (unsigned)UiPonReads, UiPonMmu,
        UiPonController, UiPonBus, UiPonSid, (unsigned)UiPonRegister, UiPonInit);
#else
    n = snprintf(report, sizeof(report), "r26:mmu=%d:ms=%llu", UiKeysMmu, (unsigned long long)UiKeysMs);
#endif
    if (n > 0 && n < (int)sizeof(report)) dt_probe_report_slot(fdt, 6, report);
#if CONFIG_KSHIM_DIAGNOSTIC_GPIO_SINGLE_READ
#if CONFIG_KSHIM_DIAGNOSTIC_MENU_KEYS && CONFIG_KSHIM_DIAGNOSTIC_GPIO_SINGLE_READ && CONFIG_KSHIM_DIAGNOSTIC_SPMI_CORE_SINGLE_READ && CONFIG_KSHIM_DIAGNOSTIC_PON_SINGLE_READ
    n = snprintf(report, sizeof(report), "r33:rc=%d:en=%d:g=%u:p=%u:e=%u:mmu=%d:gr=%d:gv=%u:gn=%u",
        UiInputStatus, UiInputEnabled, (unsigned)UiInputGpioReads, (unsigned)UiInputPonReads,
        (unsigned)UiInputErrors, UiInputMmu, UiGpioStatus, (unsigned)UiGpioValue, (unsigned)UiGpioReads);
#else
    n = snprintf(report, sizeof(report), "r27:k=%u:rc=%d:v=%u:n=%u:mmu=%d",
        (unsigned)QCOM_KEYCODE_VOLUMEUP, UiGpioStatus, (unsigned)UiGpioValue,
        (unsigned)UiGpioReads, UiGpioMmu);
#endif
    if (n > 0 && n < (int)sizeof(report)) dt_probe_report_slot(fdt, 7, report);
#endif
#if CONFIG_KSHIM_DIAGNOSTIC_SPMI_CORE_SINGLE_READ && CONFIG_KSHIM_DIAGNOSTIC_GPIO_SINGLE_READ
    n = snprintf(report, sizeof(report), "r30:rc=%d:n=%u:cnt=%u:id=%u:addr=%llx:raw=%x:mmu=%d",
        UiSpmiCoreStatus, (unsigned)UiSpmiCoreReads, (unsigned)UiSpmiCoreCount,
        (unsigned)UiSpmiCoreController, (unsigned long long)UiSpmiCoreAddress,
        UiSpmiCoreRaw, UiSpmiCoreMmu);
    if (n > 0 && n < (int)sizeof(report)) dt_probe_report_slot(fdt, 8, report);
#endif
}
#endif
static void ui_probe_report(void *fdt, int close, size_t selected)
{
    if (kshim_mmu_is_enabled() != 0) return;
    struct kshim_resources *r = kshim_resources_get();
    char report[81];
    int n = snprintf(report, sizeof(report),
        "r24:init=%d:f=%u:close=%d:mmu=%d:sel=%u",
        UiInit, UiFrames, close, kshim_mmu_is_enabled(), (unsigned)selected);
    if (n > 0 && n < (int)sizeof(report)) dt_probe_report_slot(fdt, 0, report);
    n = snprintf(report, sizeof(report),
        "r24:fb=%llx:w=%u:h=%u:st=%u:fmt=%u",
        (unsigned long long)r->framebuffer.render_address,
        (unsigned)r->framebuffer.width, (unsigned)r->framebuffer.height,
        (unsigned)r->framebuffer.stride, (unsigned)r->framebuffer.format);
    if (n > 0 && n < (int)sizeof(report)) dt_probe_report_slot(fdt, 1, report);
#if CONFIG_KSHIM_DIAGNOSTIC_KEYS_DESCRIPTORS_ONLY
    ui_report_key_descriptors(fdt);
#endif
}
#endif
#if CONFIG_KSHIM_TOUCH
static kshim_touch_t Touch;
static struct CrQupContext TouchPlatform;
static kshim_ftm5_t Ftm5;
static int TouchAttempted, TouchReady;
static struct kshim_touch_resource *TouchResource;
static int touch_transfer(void *context, const uint8_t *command, size_t length,
                          uint8_t *response, size_t count)
{
    (void)context;
    struct CrI2cMessage messages[] = {{TouchResource->address, false, (uint8_t *)(uintptr_t)command, length},
                                    {TouchResource->address, true, response, count}};
    return CrGeniI2cTransfer(&TouchPlatform.i2c, messages, count ? 2 : 1, 20000);
}
static void touch_delay(void *context, uint32_t us)
{ (void)context; TouchPlatform.io.delay_us(TouchPlatform.io.context, us); }
static int touch_quiesce(void)
{
    if (!TouchAttempted) return 0;
    int status = kshim_ftm5_quiesce(&Ftm5);
    if (status || CrQupQuiesce(&TouchPlatform)) return -1;
    TouchAttempted = TouchReady = 0;
    return 0;
}
static void start_touch(struct kshim_resources *r)
{
    if (r->touch_status) return;
    /* Only the FTM5 generation has a runtime driver; unsupported generations
     * fall back to the volume-key menu without touching MMIO. */
    if (r->touch.generation != 5) return;
    TouchResource = &r->touch;
    kshim_touch_config_t cfg = {.source_width = r->touch.width, .source_height = r->touch.height,
        .output_width = Framebuffer.width, .output_height = Framebuffer.height,
        .flip_x = r->touch.flip_x, .flip_y = r->touch.flip_y,
        .emit = kshim_lvgl_touch, .emit_context = &Lvgl};
    int status = kshim_touch_init(&Touch, &cfg);
    if (!status) status = CrQupInit(&TouchPlatform, NULL, &r->touch.qup);
    TouchAttempted = 1;
    if (!status) {
        kshim_ftm5_transport_t t = {NULL, touch_transfer, touch_delay, KSHIM_FTM5_BUS_I2C};
        status = kshim_ftm5_init(&Ftm5, &t, &Touch);
    }
    TouchReady = status == 0;
    printf("Touch: FTM%u status %d\n", r->touch.generation, status);
    if (!TouchReady) { (void)touch_quiesce(); kshim_lvgl_set_status(&Lvgl, KSHIM_STR_KEYS_HINT); }
}
#endif
static int start_menu(const void *fdt)
{
    struct kshim_resources *r = kshim_resources_get();
    kshim_framebuffer_config_t config = r->framebuffer;
#if CONFIG_KSHIM_DIAGNOSTIC_UI_ONLY
    UiInit = -9;
    UiFrames = 0;
#endif
#if CONFIG_KSHIM_QEMU_PLATFORM
    /* Explicit virtual test surface; physical platforms require scanout evidence. */
    config = (kshim_framebuffer_config_t){.render_address = CONFIG_KSHIM_FB_RENDER_ADDRESS,
        .width = CONFIG_KSHIM_FB_WIDTH, .height = CONFIG_KSHIM_FB_HEIGHT,
        .stride = CONFIG_KSHIM_FB_STRIDE, .format = CONFIG_KSHIM_FB_FORMAT,
        .bpp = kshim_fb_format_bpp(CONFIG_KSHIM_FB_FORMAT), .foreground = UINT32_MAX};
    r->framebuffer = config;
#endif
    /* The panel reservation is declared no-map, so it must never be accessed
     * through a write-back alias: render into it non-cacheable.  With early
     * translation the RAM ranges are already mapped write-back, hence the
     * overwrite. */
    if (kshim_mmu_is_enabled() != 0 && config.render_address != 0U) {
        uint64_t bytes = config.buffer_size ? config.buffer_size :
            (uint64_t)(config.stride ? config.stride :
                (uint64_t)config.width * config.bpp / 8U) * config.height;
        if (bytes != 0U)
            (void)kshim_mmu_add_region(config.render_address, bytes,
                                       KSHIM_MMU_NORMAL_NC,
                                       KSHIM_MMU_READ | KSHIM_MMU_WRITE);
    }
    if (!config.render_address || kshim_fb_init(&Framebuffer, &config)) return -1;
    int keys = build_keys(fdt);
#if CONFIG_KSHIM_SMP
    int status = kshim_smp_init(fdt);
    SmpInitialized = status == 0;
    if (SmpInitialized) status = kshim_smp_start_workers(KSHIM_SMP_DEFAULT_TIMEOUT_US);
    printf("PSCI workers: %u (status %d)\n", (unsigned)kshim_smp_available_count(), status);
#endif
    if (!kshim_mmu_is_enabled() &&
        (kshim_mmu_root_table() || kshim_mmu_configure_default(fdt) == 0))
        (void)kshim_mmu_enable_current_cpu();
    kshim_boot_metrics_record_once(KSHIM_BOOT_STAGE_MMU);
    /* The UI scratch sits in the same no-map display reservation; map it
     * non-cacheable before LVGL writes its fonts and background there. */
    if (kshim_mmu_is_enabled() != 0 && r->ui_scratch.size != 0U)
        (void)kshim_mmu_add_region(r->ui_scratch.base, r->ui_scratch.size,
                                   KSHIM_MMU_NORMAL_NC,
                                   KSHIM_MMU_READ | KSHIM_MMU_WRITE);
    kshim_lvgl_set_scratch((void *)(uintptr_t)r->ui_scratch.base,
                           (size_t)r->ui_scratch.size);
#if CONFIG_KSHIM_DIAGNOSTIC_UI_ONLY && CONFIG_KSHIM_DIAGNOSTIC_KEYS_DESCRIPTORS_ONLY
    ui_prepare_key_descriptors(fdt);
#endif
#if CONFIG_KSHIM_DIAGNOSTIC_UI_ONLY
    (void)keys;
    QcomKeys *menu_keys = NULL;
#if CONFIG_KSHIM_DIAGNOSTIC_KEYS_DESCRIPTORS_ONLY && CONFIG_KSHIM_DIAGNOSTIC_MENU_KEYS && CONFIG_KSHIM_DIAGNOSTIC_GPIO_SINGLE_READ && CONFIG_KSHIM_DIAGNOSTIC_SPMI_CORE_SINGLE_READ && CONFIG_KSHIM_DIAGNOSTIC_PON_SINGLE_READ
    if (UiInputStatus == 0) menu_keys = &Keys;
    UiInputEnabled = menu_keys == &Keys ? 1 : 0;
#endif
#else
    QcomKeys *menu_keys = &Keys;
#endif
    int lvgl_status = kshim_lvgl_init(&Lvgl, &Framebuffer, menu_keys);
#if CONFIG_KSHIM_DIAGNOSTIC_UI_ONLY
    UiInit = lvgl_status;
#endif
    if (lvgl_status) return -2;
    printf("UI: %s font, scratch %#llx+%#llx\n",
           kshim_lvgl_has_cjk_font(&Lvgl) ? "Noto Sans CJK" : "fallback Latin",
           (unsigned long long)r->ui_scratch.base, (unsigned long long)r->ui_scratch.size);
#if CONFIG_KSHIM_DIAGNOSTIC_UI_ONLY
    kshim_lvgl_set_status(&Lvgl, KSHIM_STR_KEYS_HINT);
#else
    if (keys) kshim_lvgl_set_status(&Lvgl, KSHIM_STR_KEYS_UNAVAIL);
#endif
#if CONFIG_KSHIM_TOUCH
    start_touch(r);
#endif
    LastTime = get_current_time();
    return 0;
}
static void menu_frame(void)
{
#if CONFIG_KSHIM_TOUCH
    if (TouchReady) {
        int status = CrQupIrq(&TouchPlatform);
        if (status > 0) status = kshim_ftm5_poll(&Ftm5);
        if (status < 0) {
            TouchReady = 0; kshim_touch_cancel(&Touch);
            printf("Touch: input status %d\n", status);
            kshim_lvgl_set_status(&Lvgl, KSHIM_STR_KEYS_HINT);
        }
    }
#endif
    uint64_t now = get_current_time(), elapsed = now - LastTime;
    LastTime = now;
    kshim_lvgl_frame(&Lvgl, elapsed > UINT32_MAX ? UINT32_MAX : (uint32_t)elapsed);
#if CONFIG_KSHIM_DIAGNOSTIC_UI_ONLY
    ++UiFrames;
#endif
}
#endif
int kshim_platform_prepare_handoff(void)
{
#if CONFIG_KSHIM_FRAMEBUFFER && CONFIG_KSHIM_LVGL
#if CONFIG_KSHIM_TOUCH
    TouchReady = 0;
    if (touch_quiesce()) return -1;
#endif
    kshim_lvgl_deinit(&Lvgl);
#if CONFIG_KSHIM_SMP
    if (kshim_lvgl_os_prepare_shutdown()) return -2;
#endif
#endif
#if CONFIG_KSHIM_SMP
    if (SmpInitialized) {
        if (kshim_smp_prepare_handoff(KSHIM_SMP_DEFAULT_TIMEOUT_US)) return -2;
        SmpInitialized = 0;
        return 0;
    }
#endif
    return kshim_mmu_prepare_handoff();
}
/* Entries carry their action separately from the original manifest index. */
enum menu_action {
    MENU_PAYLOAD, MENU_ADVANCED, MENU_BACK, MENU_RESET, MENU_OFF,
    MENU_RECOVERY, MENU_FASTBOOT, MENU_EDL,
    MENU_LANGUAGE, MENU_LANG_ZH, MENU_LANG_EN,
    /* Appended: the read-only Device info entry and its read-only field rows. */
    MENU_DEVICE_INFO, MENU_INFO_FIELD,
    /* Appended: the session-settings pages. MENU_UNAVAILABLE marks a system
     * row that cannot be booted for the current effective board; it must never
     * return a payload. */
    MENU_SETTINGS, MENU_DEVICE, MENU_DEFAULT,
    MENU_DEVICE_SET, MENU_DEFAULT_SET, MENU_UNAVAILABLE,
};
struct menu_page {
    const char *names[KSHIM_MENU_MAX_ENTRIES];
    const char *descriptions[KSHIM_MENU_MAX_ENTRIES];
    const char *action_labels[KSHIM_MENU_MAX_ENTRIES];
    enum menu_action actions[KSHIM_MENU_MAX_ENTRIES];
    kshim_menu_role_t roles[KSHIM_MENU_MAX_ENTRIES];
    uint8_t payloads[KSHIM_MENU_MAX_ENTRIES];
    size_t count, initial;
};
/* Page indexes. 0..3 are the legacy localized+info pages; 4..6 are the
 * session-settings pages and only exist when menu->settings is set. */
enum menu_page_id {
    MENU_PAGE_MAIN = 0, MENU_PAGE_ADVANCED = 1, MENU_PAGE_LANGUAGE = 2,
    MENU_PAGE_INFO = 3, MENU_PAGE_SETTINGS = 4, MENU_PAGE_DEVICE = 5,
    MENU_PAGE_DEFAULT = 6, MENU_PAGE_COUNT = 7
};
struct menu_selection {
    struct menu_page pages[MENU_PAGE_COUNT];
    const void *fdt;
    /* Caller-owned inputs are cached so the pages can be rebuilt in place after
     * a language change; no translated text is retained across rebuilds. */
    const char *const *caller_names;
    const char *const *caller_descriptions;
    const kshim_menu_role_t *caller_roles;
    size_t caller_count;
    size_t page, advanced_row, language_row, device_info_row, original_initial;
    uint64_t advanced_mask;
    /* Optional caller-owned read-only record; NULL means the info row/page is
     * absent and the menu is byte-for-byte the old behavior. */
    const struct device_info *info;
    /* Lifetime covers the whole menu: descriptions point directly at these. */
    char info_values[KSHIM_INFO_FIELD_COUNT][KSHIM_INFO_VALUE_LEN];
    /* The read-only summary aggregate: every field as one "label: value" line,
     * rebuilt once per fill and shared by the info page description. */
    char info_summary[2048];
    uint32_t timeout;
    int advanced, language, psci_ready;
    /* ---- Session settings (new entry point only) ---- */
    int settings;
    /* payload_boards[i] is the board associated with caller payload i, or
     * DEVICE_BOARD_UNKNOWN when the caller has no matching evidence. */
    const enum device_board *payload_boards;
    struct boot_settings boot;
    enum device_board effective;
    /* The caller's original initial index, kept verbatim for the TB322FC
     * auto-return gate (settings must never widen it). */
    size_t initial_caller;
    /* Set once a unique, board-safe session default was resolved. */
    int default_ok;
    /* Compressed candidate array: the available boot-system targets and the
     * caller index each maps to. A candidate index is never returned to the
     * caller; only candidate_caller[] values are. */
    enum boot_default_target candidates[KSHIM_MENU_MAX_ENTRIES];
    size_t candidate_caller[KSHIM_MENU_MAX_ENTRIES];
    size_t candidate_count;
    /* Focus memory for the settings pages. */
    size_t settings_row, language_entry_row, device_entry_row, default_entry_row;
    /* Row names that carry the current value and must outlive the fill. */
    char setting_device_name[KSHIM_INFO_VALUE_LEN];
    char setting_default_name[KSHIM_INFO_VALUE_LEN];
};
static int menu_append(struct menu_page *page, const char *name, const char *description,
                       enum menu_action action, size_t payload, kshim_menu_role_t role)
{
    if (page->count >= KSHIM_MENU_MAX_ENTRIES || !name) return -1;
    size_t row = page->count++;
    page->names[row] = name;
    page->descriptions[row] = description;
    page->actions[row] = action;
    page->roles[row] = role;
    switch (action) {
    case MENU_PAYLOAD:
        page->action_labels[row] = KSHIM_MENU_ACTION_BOOT;
        break;
    case MENU_ADVANCED:
    case MENU_RECOVERY:
    case MENU_FASTBOOT:
    case MENU_EDL:
    case MENU_LANGUAGE:
    case MENU_DEVICE_INFO:
    case MENU_SETTINGS:
    case MENU_DEVICE:
    case MENU_DEFAULT:
        page->action_labels[row] = KSHIM_MENU_ACTION_ENTER;
        break;
    /* A field confirmation is read-only and returns home, so its label is the
     * same localized Back that menu_activate already performs. */
    case MENU_INFO_FIELD:
        page->action_labels[row] = KSHIM_MENU_ACTION_BACK;
        break;
    /* An unavailable system row cannot start: its button reads the short
     * localized "unavailable" label, not the Boot action. */
    case MENU_UNAVAILABLE:
        page->action_labels[row] = kshim_menu_text(KSHIM_TEXT_ACTION_UNAVAILABLE);
        break;
    case MENU_LANG_ZH:
    case MENU_LANG_EN:
    case MENU_DEVICE_SET:
    case MENU_DEFAULT_SET:
        page->action_labels[row] = kshim_menu_text(KSHIM_TEXT_LANGUAGE_SELECT);
        break;
    case MENU_RESET:
        page->action_labels[row] = KSHIM_MENU_ACTION_RESTART;
        break;
    case MENU_OFF:
        page->action_labels[row] = KSHIM_MENU_ACTION_OFF;
        break;
    case MENU_BACK:
        page->action_labels[row] = KSHIM_MENU_ACTION_BACK;
        break;
    }
    page->payloads[row] = (uint8_t)payload;
    return 0;
}
/* Append the U+2026 ellipsis when the buffer still has room for it. */
static void info_append_ellipsis(char *buffer, size_t capacity)
{
    size_t used = strlen(buffer);
    const size_t mark = sizeof(KSHIM_INFO_ELLIPSIS) - 1;

    if (used + mark + 1 <= capacity) {
        memcpy(buffer + used, KSHIM_INFO_ELLIPSIS, mark);
        buffer[used + mark] = '\0';
    } else if (capacity > mark + 1) {
        memcpy(buffer + capacity - 1 - mark, KSHIM_INFO_ELLIPSIS, mark);
        buffer[capacity - 1] = '\0';
    }
}
static void info_unknown(char *buffer, size_t capacity)
{
    snprintf(buffer, capacity, "%s", kshim_menu_text(KSHIM_TEXT_UNKNOWN));
}
/* Longest UTF-8 prefix of a NUL-terminated string that fits in `limit` bytes
 * without splitting a multibyte sequence. */
static size_t info_utf8_bound(const char *text, size_t limit)
{
    size_t len = 0;

    while (len < limit && text[len] != '\0') {
        unsigned char lead = (unsigned char)text[len];
        size_t width = lead < 0x80 ? 1 :
                       (lead & 0xE0) == 0xC0 ? 2 :
                       (lead & 0xF0) == 0xE0 ? 3 :
                       (lead & 0xF8) == 0xF0 ? 4 : 1;

        if (len + width > limit)
            break;
        len += width;
    }
    return len;
}
/* Build the bounded read-only summary from the fixed field set and add the one
 * main-menu Device info row before the power rows. Every value derives only
 * from the caller's read-only record and the already probed resources. */
static int menu_fill_device_info(struct menu_selection *menu)
{
    const struct device_info *info = menu->info;
    static const kshim_menu_text_id labels[KSHIM_INFO_FIELD_COUNT] = {
        KSHIM_TEXT_INFO_BOARD, KSHIM_TEXT_INFO_SOC, KSHIM_TEXT_INFO_MODEL,
        KSHIM_TEXT_INFO_COMPATIBLE, KSHIM_TEXT_INFO_RAM,
        KSHIM_TEXT_INFO_FRAMEBUFFER, KSHIM_TEXT_INFO_VERSION,
    };
    size_t i;

    /* Board identity comes only from the exact enum; a generic SoC never
     * implies TB322FC. */
    snprintf(menu->info_values[0], sizeof(menu->info_values[0]), "%s",
             info->board == DEVICE_BOARD_TB322FC ? "TB322FC" :
             info->board == DEVICE_BOARD_TB323FU ? "TB323FU" :
             kshim_menu_text(KSHIM_TEXT_UNKNOWN));
    snprintf(menu->info_values[1], sizeof(menu->info_values[1]), "%s",
             info->soc == DEVICE_SOC_SM8750 ? "SM8750" :
             info->soc == DEVICE_SOC_SM8850 ? "SM8850" :
             kshim_menu_text(KSHIM_TEXT_UNKNOWN));

    /* Original DT model, bounded to a readable length on a UTF-8 boundary with
     * a visible ellipsis when cut. The record guarantees NUL termination. */
    if (info->model_known && info->model[0]) {
        size_t cap = sizeof(menu->info_values[2]);
        size_t limit = cap - 1;
        size_t len;
        bool cut;

        if (limit > KSHIM_INFO_MODEL_MAX) limit = KSHIM_INFO_MODEL_MAX;
        len = info_utf8_bound(info->model, limit);
        cut = info->model_truncated || info->model[len] != '\0';
        memcpy(menu->info_values[2], info->model, len);
        menu->info_values[2][len] = '\0';
        if (cut)
            info_append_ellipsis(menu->info_values[2], cap);
    } else {
        info_unknown(menu->info_values[2], sizeof(menu->info_values[2]));
    }

    /* Original compatible list: walk the raw NUL-separated list by its length,
     * bounded by the record's own array, joining whole entries with newlines.
     * Never treat the blob as one %s string. At most three entry lines render;
     * a clipped entry keeps its original prefix and the shared ellipsis. */
    if (info->compatible_known && info->compatible_length) {
        char *out = menu->info_values[3];
        const size_t cap = sizeof(menu->info_values[3]);
        size_t total = info->compatible_length;
        size_t off = 0, used = 0;
        bool any = false, cut = info->compatible_truncated;
        unsigned lines = 0;

        /* A malformed length must never read past the record's own array. */
        if (total > sizeof(info->compatible)) {
            total = sizeof(info->compatible);
            cut = true;
        }
        while (off < total && lines < KSHIM_INFO_COMPATIBLE_LINES) {
            size_t remaining = total - off;
            size_t len = 0;
            size_t entry;

            while (len < remaining && info->compatible[off + len] != '\0')
                len++;
            if (len == 0) { off += 1; continue; }         /* skip empty entry */
            entry = len;
            if (entry > KSHIM_INFO_COMPATIBLE_LINE_LEN) {
                entry = KSHIM_INFO_COMPATIBLE_LINE_LEN;
                cut = true;
            }
            if (any)
                out[used++] = '\n';
            memcpy(out + used, info->compatible + off, entry);
            used += entry;
            any = true;
            ++lines;
            if (len == remaining) { cut = true; break; }  /* missing terminal NUL */
            if (entry != len) break;                      /* clipped entry line */
            off += len + 1;
        }
        if (off < total) cut = true;                      /* stopped at the line cap */
        out[used] = '\0';
        if (!any)
            info_unknown(out, cap);
        else if (cut)
            info_append_ellipsis(out, cap);
    } else {
        info_unknown(menu->info_values[3], sizeof(menu->info_values[3]));
    }

    /* DT-reported RAM as a whole MiB integer. The value stays a bare number so
     * the summary's own localized field label carries the "DT reported" note;
     * it is never presented as physical or available RAM. */
    if (info->ram_known) {
        snprintf(menu->info_values[4], sizeof(menu->info_values[4]), "%llu MiB",
                 (unsigned long long)(info->dt_reported_ram_bytes /
                                      (UINT64_C(1024) * UINT64_C(1024))));
    } else {
        info_unknown(menu->info_values[4], sizeof(menu->info_values[4]));
    }

    /* Resolution and stride from the already probed resource only; no MMIO. */
    {
        const struct kshim_resources *res = kshim_resources_get();
        const kshim_framebuffer_config_t *fb = res ? &res->framebuffer : NULL;

        if (res && res->splash_status == 0 && fb->width && fb->height && fb->stride) {
            snprintf(menu->info_values[5], sizeof(menu->info_values[5]),
                     "%ux%u stride %u", (unsigned)fb->width, (unsigned)fb->height,
                     (unsigned)fb->stride);
        } else {
            info_unknown(menu->info_values[5], sizeof(menu->info_values[5]));
        }
    }

    const char *version = KSHIM_INFO_VERSION_TEXT;

    if (version)
        snprintf(menu->info_values[6], sizeof(menu->info_values[6]), "%s",
                 version);
    else
        info_unknown(menu->info_values[6], sizeof(menu->info_values[6]));

    /* One bounded "label: value" block per field, rows joined by newlines; the
     * aggregate is the info page description. A short buffer is the existing
     * build failure, never a truncated value. */
    {
        size_t used = 0;
        for (i = 0; i < KSHIM_INFO_FIELD_COUNT; ++i) {
            int written = snprintf(menu->info_summary + used,
                                   sizeof(menu->info_summary) - used,
                                   "%s%s: %s", i ? "\n" : "",
                                   kshim_menu_text(labels[i]), menu->info_values[i]);
            if (written < 0 || (size_t)written >= sizeof(menu->info_summary) - used)
                return -1;
            used += (size_t)written;
        }
    }

    /* The home row precedes the power rows and remembers its focus index. */
    menu->device_info_row = menu->pages[0].count;
    if (menu_append(&menu->pages[0], kshim_menu_text(KSHIM_TEXT_INFO_LABEL),
                    kshim_menu_text(KSHIM_TEXT_INFO_DESC), MENU_DEVICE_INFO, 0,
                    KSHIM_MENU_ROLE_DEVICE_INFO))
        return -1;
    return 0;
}
/* ------------------------------------------------------------------ */
/* Session settings (only reached through the settings entry point)    */
/* ------------------------------------------------------------------ */

/* Localized marketing name for a board; UNKNOWN is never guessed. */
static const char *settings_board_name(enum device_board board)
{
    if (board == DEVICE_BOARD_TB322FC) return kshim_menu_text(KSHIM_TEXT_BOARD_TB322FC_NAME);
    if (board == DEVICE_BOARD_TB323FU) return kshim_menu_text(KSHIM_TEXT_BOARD_TB323FU_NAME);
    return kshim_menu_text(KSHIM_TEXT_UNKNOWN);
}
/* Localized platform code for a board. */
static const char *settings_board_code(enum device_board board)
{
    if (board == DEVICE_BOARD_TB322FC) return kshim_menu_text(KSHIM_TEXT_DEVICE_TB322FC_NAME);
    if (board == DEVICE_BOARD_TB323FU) return kshim_menu_text(KSHIM_TEXT_DEVICE_TB323FU_NAME);
    return kshim_menu_text(KSHIM_TEXT_UNKNOWN);
}
/* Row index of the selectable Default-page row that matches the resolved
 * session default. The three system rows are fixed (Android / internal / TF);
 * BASE shares the Android row. A resolved default focuses its own role's row
 * only when that row is actually selectable; otherwise - and whenever no
 * default resolved - the first selectable row owns the focus, so an
 * unavailable row is never the initial focus. The stored default target, the
 * caller initial and the timeout are never rewritten here. */
static size_t settings_default_row(const struct menu_selection *menu)
{
    const struct menu_page *page = &menu->pages[MENU_PAGE_DEFAULT];
    kshim_menu_role_t want = KSHIM_MENU_ROLE_UNKNOWN;
    size_t i;

    if (menu->default_ok) {
        kshim_menu_role_t role =
            (menu->caller_roles && menu->original_initial < menu->caller_count)
                ? menu->caller_roles[menu->original_initial]
                : KSHIM_MENU_ROLE_UNKNOWN;
        if (role == KSHIM_MENU_ROLE_ANDROID || role == KSHIM_MENU_ROLE_BASE)
            want = KSHIM_MENU_ROLE_ANDROID;
        else if (role == KSHIM_MENU_ROLE_INTERNAL)
            want = KSHIM_MENU_ROLE_INTERNAL;
        else if (role == KSHIM_MENU_ROLE_TF)
            want = KSHIM_MENU_ROLE_TF;
        for (i = 0; want != KSHIM_MENU_ROLE_UNKNOWN && i < page->count; ++i)
            if (page->roles[i] == want && page->actions[i] == MENU_DEFAULT_SET)
                return i;
    }
    for (i = 0; i < page->count; ++i)
        if (page->actions[i] == MENU_DEFAULT_SET)
            return i;
    return 0U;
}
/* True only when caller payload i carries a known board equal to the session's
 * effective board. UNKNOWN is never treated as a match. */
static int settings_payload_available(const struct menu_selection *menu, size_t i)
{
    enum device_board board;
    if (!menu->payload_boards || i >= menu->caller_count) return 0;
    board = menu->payload_boards[i];
    if (board != DEVICE_BOARD_TB322FC && board != DEVICE_BOARD_TB323FU) return 0;
    return board == menu->effective;
}
/* Resolve one Linux system role (INTERNAL / TF / DIAGNOSTIC) to its unique
 * board-available caller index. Returns 1 and sets *caller when exactly one
 * payload matches; 0 when none or several match, in which case the row must
 * stay unavailable and must never return a payload. */
static int settings_role_match(const struct menu_selection *menu,
                               kshim_menu_role_t role, size_t *caller)
{
    size_t i, matches = 0, first = 0;
    for (i = 0; i < menu->caller_count; ++i) {
        kshim_menu_role_t r = menu->caller_roles ? menu->caller_roles[i] : KSHIM_MENU_ROLE_UNKNOWN;
        if (r != role || !settings_payload_available(menu, i)) continue;
        if (matches == 0) first = i;
        ++matches;
    }
    if (matches != 1) return 0;
    *caller = first;
    return 1;
}
/* Recompute the effective board and the compressed candidate array. The
 * candidate array only holds real boot system targets, and candidate_caller[]
 * is the only thing ever turned back into a caller index. */
static void settings_recompute_candidates(struct menu_selection *menu)
{
    size_t i;
    enum device_board eff = DEVICE_BOARD_UNKNOWN;
    (void)boot_settings_effective_board(&menu->boot,
        menu->info ? menu->info->board : DEVICE_BOARD_UNKNOWN, &eff);
    menu->effective = eff;
    menu->candidate_count = 0;
    for (i = 0; i < menu->caller_count; ++i) {
        kshim_menu_role_t role = menu->caller_roles ? menu->caller_roles[i] : KSHIM_MENU_ROLE_UNKNOWN;
        enum boot_default_target target = BOOT_TARGET_MANIFEST;
        if (role == KSHIM_MENU_ROLE_ANDROID || role == KSHIM_MENU_ROLE_BASE) {
            target = BOOT_TARGET_ANDROID;
        } else if (role == KSHIM_MENU_ROLE_INTERNAL) {
            size_t caller = 0;
            if (!settings_role_match(menu, KSHIM_MENU_ROLE_INTERNAL, &caller) || caller != i) continue;
            target = BOOT_TARGET_INTERNAL;
        } else if (role == KSHIM_MENU_ROLE_TF) {
            size_t caller = 0;
            if (!settings_role_match(menu, KSHIM_MENU_ROLE_TF, &caller) || caller != i) continue;
            target = BOOT_TARGET_TF;
        } else {
            continue;
        }
        menu->candidates[menu->candidate_count] = target;
        menu->candidate_caller[menu->candidate_count] = i;
        ++menu->candidate_count;
    }
}
/* Resolve the session default (countdown / auto-boot target) from the stored
 * default target and the candidates. The caller initial is mapped to a
 * candidate index explicitly and is never itself used as one; when there is no
 * unique, board-safe resolution the session simply has no automatic target. */
static void settings_resolve_default(struct menu_selection *menu)
{
    size_t i, manifest_index = 0, sel = 0;
    int manifest_found = 0, rc;
    struct boot_settings probe;

    for (i = 0; i < menu->candidate_count; ++i) {
        if (menu->candidate_caller[i] == menu->initial_caller) {
            manifest_index = i;
            manifest_found = 1;
            break;
        }
    }
    menu->default_ok = 0;
    if (menu->boot.default_target == BOOT_TARGET_MANIFEST) {
        if (!manifest_found) {
            /* Manifest-only exception: the caller's own initial entry may be the
             * fixed diagnostic row, which no candidate covers. It is honored as
             * a one-session automatic default (never a configurable boot-system
             * target) only when the advanced page is enabled and that exact
             * initial caller is a unique, board-safe diagnostic. Any explicit
             * Android/internal/TF selection skips this branch entirely and goes
             * through the ordinary resolver below, so it still wins. */
            size_t caller = 0;
            if (menu->advanced &&
                settings_role_match(menu, KSHIM_MENU_ROLE_DIAGNOSTIC, &caller) &&
                caller == menu->initial_caller) {
                menu->original_initial = caller;
                menu->default_ok = 1;
            }
            return;
        }
        /* Confirming the manifest candidate is not enough: its own real target
         * must still be unique. A duplicate target (e.g. two Android rows)
         * leaves the session with no automatic default. */
        if (boot_settings_init(&probe) != BOOT_SETTINGS_OK) return;
        probe.default_target = menu->candidates[manifest_index];
        if (boot_settings_resolve_default(&probe, menu->candidates, menu->candidate_count, 0, &sel)
                != BOOT_SETTINGS_OK)
            return;
        if (sel >= menu->candidate_count || sel != manifest_index) return;
    } else {
        rc = boot_settings_resolve_default(&menu->boot, menu->candidates, menu->candidate_count,
                                           manifest_found ? manifest_index : 0, &sel);
        if (rc != BOOT_SETTINGS_OK || sel >= menu->candidate_count) return;
    }
    menu->original_initial = menu->candidate_caller[sel];
    menu->default_ok = 1;
}
/* Resolve one real boot system (ANDROID / INTERNAL / TF) to its unique caller
 * index through the very same candidates array the session default uses. A
 * temporary record asking for exactly this target makes the shared resolver
 * answer unique-or-nothing; no separate decision framework is introduced.
 * Returns 1 with *caller set for a single match, 0 for none or several. */
static int settings_system_caller(const struct menu_selection *menu,
                                  enum boot_default_target target, size_t *caller)
{
    struct boot_settings probe;
    size_t sel = 0;

    if (boot_settings_init(&probe) != BOOT_SETTINGS_OK) return 0;
    probe.default_target = target;
    if (boot_settings_resolve_default(&probe, menu->candidates, menu->candidate_count, 0, &sel)
            != BOOT_SETTINGS_OK)
        return 0;
    if (sel >= menu->candidate_count) return 0;
    *caller = menu->candidate_caller[sel];
    return 1;
}
/* Append one of the three fixed boot-system rows on the main page. A unique,
 * board-safe candidate keeps the row usable with its original caller index and
 * restores the resolved default focus; none or several keep the row visible but
 * unavailable, with the role's own name and no caller mapping. Sets *usable and
 * returns 0, or -1 on a full page. */
static int settings_append_system(struct menu_selection *menu, kshim_menu_role_t role,
                                  enum boot_default_target target, int *usable)
{
    struct menu_page *page = &menu->pages[MENU_PAGE_MAIN];
    const char *name = kshim_menu_role_name(role, NULL);
    size_t caller = 0, row = page->count;

    *usable = 0;
    if (settings_system_caller(menu, target, &caller)) {
        if (menu_append(page, name,
                        kshim_menu_role_description(role,
                            menu->caller_descriptions ? menu->caller_descriptions[caller] : NULL),
                        MENU_PAYLOAD, caller, role))
            return -1;
        if (menu->default_ok && caller == menu->original_initial) {
            menu->page = MENU_PAGE_MAIN;
            page->initial = row;
        }
        *usable = 1;
    } else if (menu_append(page, name, kshim_menu_text(KSHIM_TEXT_UNAVAILABLE),
                           MENU_UNAVAILABLE, 0, role)) {
        return -1;
    }
    return 0;
}
/* Fill the Settings "device" row name with the current value. */
static void settings_fill_device_name(const struct menu_selection *menu,
                                      char *buffer, size_t capacity)
{
    if (menu->boot.device_choice == BOOT_DEVICE_AUTO) {
        snprintf(buffer, capacity, kshim_menu_text(KSHIM_TEXT_SETTINGS_DEVICE_AUTO_FMT),
                 settings_board_name(menu->effective));
    } else {
        /* One full model name only; the format carries no second value. */
        snprintf(buffer, capacity, kshim_menu_text(KSHIM_TEXT_SETTINGS_DEVICE_MANUAL_FMT),
                 settings_board_code(menu->effective));
    }
}
/* Fill the Settings "default system" row name with the current value. */
static void settings_fill_default_name(const struct menu_selection *menu,
                                       char *buffer, size_t capacity)
{
    const char *value;
    if (menu->boot.default_target == BOOT_TARGET_ANDROID)
        value = kshim_menu_role_name(KSHIM_MENU_ROLE_ANDROID, NULL);
    else if (menu->boot.default_target == BOOT_TARGET_INTERNAL)
        value = kshim_menu_role_name(KSHIM_MENU_ROLE_INTERNAL, NULL);
    else if (menu->boot.default_target == BOOT_TARGET_TF)
        value = kshim_menu_role_name(KSHIM_MENU_ROLE_TF, NULL);
    else if (menu->caller_roles && menu->original_initial < menu->caller_count)
        value = kshim_menu_role_name(menu->caller_roles[menu->original_initial],
                                     kshim_menu_text(KSHIM_TEXT_DEFAULT_MANIFEST));
    else
        value = kshim_menu_text(KSHIM_TEXT_DEFAULT_MANIFEST);
    snprintf(buffer, capacity, kshim_menu_text(KSHIM_TEXT_SETTINGS_DEFAULT_FMT), value);
}
/* Build every page for the session-settings entry point. Pages 0..3 keep the
 * localized+info meaning; pages 4..6 are the new Settings / Device / Default
 * pages. */
static int menu_fill_settings(struct menu_selection *menu)
{
    size_t i, p;
    for (p = 0; p < MENU_PAGE_COUNT; ++p) {
        menu->pages[p].count = 0;
        menu->pages[p].initial = 0;
    }
    menu->page = MENU_PAGE_MAIN;
    menu->advanced_row = 0;
    menu->language_row = 0;
    menu->device_info_row = 0;
    menu->settings_row = 0;
    menu->language_entry_row = 0;
    menu->device_entry_row = 0;
    menu->default_entry_row = 0;

    /* The three real boot systems are fixed rows on the main page, in a stable
     * order independent of the caller order and of the advanced mask. Each one
     * resolves to its unique candidate; none or several keep the row visible
     * but unavailable. Android/BASE are never board-filtered. */
    {
        static const kshim_menu_role_t fixed_roles[3] = {
            KSHIM_MENU_ROLE_ANDROID, KSHIM_MENU_ROLE_INTERNAL, KSHIM_MENU_ROLE_TF};
        static const enum boot_default_target fixed_targets[3] = {
            BOOT_TARGET_ANDROID, BOOT_TARGET_INTERNAL, BOOT_TARGET_TF};
        size_t first_usable = KSHIM_MENU_MAX_ENTRIES, slot;
        int usable = 0;

        for (slot = 0; slot < 3; ++slot) {
            size_t row = menu->pages[MENU_PAGE_MAIN].count;
            if (settings_append_system(menu, fixed_roles[slot], fixed_targets[slot], &usable))
                return -1;
            if (usable && first_usable == KSHIM_MENU_MAX_ENTRIES) first_usable = row;
        }
        /* A resolved, board-safe default already owns the focus; otherwise the
         * first usable system does. Neither path rewrites the stored default
         * target nor arms the countdown. */
        if (!menu->default_ok && first_usable != KSHIM_MENU_MAX_ENTRIES) {
            menu->page = MENU_PAGE_MAIN;
            menu->pages[MENU_PAGE_MAIN].initial = first_usable;
        }
    }

    /* The diagnostic system is the first fixed row on the advanced page. */
    if (menu->advanced) {
        struct menu_page *page = &menu->pages[MENU_PAGE_ADVANCED];
        const char *name = kshim_menu_role_name(KSHIM_MENU_ROLE_DIAGNOSTIC, NULL);
        size_t caller = 0, row = page->count;
        if (settings_role_match(menu, KSHIM_MENU_ROLE_DIAGNOSTIC, &caller)) {
            if (menu_append(page, name,
                            kshim_menu_role_description(KSHIM_MENU_ROLE_DIAGNOSTIC,
                                menu->caller_descriptions ? menu->caller_descriptions[caller] : NULL),
                            MENU_PAYLOAD, caller, KSHIM_MENU_ROLE_DIAGNOSTIC))
                return -1;
            /* A resolved manifest diagnostic default focuses its own advanced
             * row, mirroring settings_append_system. It is a one-session
             * automatic default only: the stored target and the caller's
             * timeout are never rewritten and it never becomes a Default-page
             * choice. An absent or invalid diagnostic leaves this untouched. */
            if (menu->default_ok && caller == menu->original_initial) {
                menu->page = MENU_PAGE_ADVANCED;
                page->initial = row;
            }
        } else if (menu_append(page, name, kshim_menu_text(KSHIM_TEXT_UNAVAILABLE),
                               MENU_UNAVAILABLE, 0, KSHIM_MENU_ROLE_DIAGNOSTIC)) {
            return -1;
        }
    }

    /* Any other caller (an unknown Linux image or an unrecognised stock OS)
     * keeps its manifest name and original index, appended after the fixed
     * rows on the page the caller's mask selects. */
    for (i = 0; i < menu->caller_count; ++i) {
        kshim_menu_role_t role = menu->caller_roles ? menu->caller_roles[i] : KSHIM_MENU_ROLE_UNKNOWN;
        size_t page_index;
        enum device_board board;
        int board_known_other;
        if (role == KSHIM_MENU_ROLE_ANDROID || role == KSHIM_MENU_ROLE_BASE ||
            role == KSHIM_MENU_ROLE_INTERNAL || role == KSHIM_MENU_ROLE_TF ||
            role == KSHIM_MENU_ROLE_DIAGNOSTIC)
            continue;
        page_index = (menu->advanced && (menu->advanced_mask & (UINT64_C(1) << i)))
            ? (size_t)MENU_PAGE_ADVANCED : (size_t)MENU_PAGE_MAIN;
        /* A generic caller whose board evidence names a known board other than
         * the session's effective board must not stay an explicit payload: it
         * keeps its role, name and row but becomes the same unavailable row as a
         * wrong-model image. Same-board, UNKNOWN-board and absent-board callers
         * keep their explicit payload semantics; UNKNOWN is never a mismatch. */
        board = menu->payload_boards ? menu->payload_boards[i] : DEVICE_BOARD_UNKNOWN;
        board_known_other =
            (board == DEVICE_BOARD_TB322FC || board == DEVICE_BOARD_TB323FU) &&
            board != menu->effective;
        if (board_known_other) {
            if (menu_append(&menu->pages[page_index],
                            kshim_menu_role_name(role, menu->caller_names[i]),
                            kshim_menu_text(KSHIM_TEXT_UNAVAILABLE),
                            MENU_UNAVAILABLE, 0, role))
                return -1;
        } else if (menu_append(&menu->pages[page_index],
                               kshim_menu_role_name(role, menu->caller_names[i]),
                               kshim_menu_role_description(role,
                                   menu->caller_descriptions ? menu->caller_descriptions[i] : NULL),
                               MENU_PAYLOAD, i, role)) {
            return -1;
        }
    }

    if (menu->advanced) {
        menu->advanced_row = menu->pages[MENU_PAGE_MAIN].count;
        if (menu_append(&menu->pages[MENU_PAGE_MAIN], KSHIM_STR_ADVANCED, KSHIM_MENU_DESC_ADVANCED,
                        MENU_ADVANCED, 0, KSHIM_MENU_ROLE_ADVANCED) ||
            menu_append(&menu->pages[MENU_PAGE_ADVANCED], KSHIM_STR_RECOVERY, KSHIM_MENU_DESC_RECOVERY,
                        MENU_RECOVERY, 0, KSHIM_MENU_ROLE_RECOVERY) ||
            menu_append(&menu->pages[MENU_PAGE_ADVANCED], KSHIM_STR_FASTBOOT, KSHIM_MENU_DESC_FASTBOOT,
                        MENU_FASTBOOT, 0, KSHIM_MENU_ROLE_FASTBOOT) ||
            menu_append(&menu->pages[MENU_PAGE_ADVANCED], KSHIM_STR_EDL, KSHIM_MENU_DESC_EDL,
                        MENU_EDL, 0, KSHIM_MENU_ROLE_EDL) ||
            menu_append(&menu->pages[MENU_PAGE_ADVANCED], KSHIM_STR_BACK, KSHIM_MENU_DESC_BACK,
                        MENU_BACK, 0, KSHIM_MENU_ROLE_BACK))
            return -1;
    }

    /* Settings entry row on the main page. */
    menu->settings_row = menu->pages[MENU_PAGE_MAIN].count;
    if (menu_append(&menu->pages[MENU_PAGE_MAIN], KSHIM_MENU_LABEL_SETTINGS,
                    kshim_menu_text(KSHIM_TEXT_SETTINGS_DESC), MENU_SETTINGS, 0,
                    KSHIM_MENU_ROLE_SETTINGS))
        return -1;

    /* Read-only Device info row, exactly as the localized+info menu. */
    if (menu->info && menu_fill_device_info(menu)) return -1;

#if CONFIG_KSHIM_MENU_POWER_ACTIONS
    if (menu_append(&menu->pages[MENU_PAGE_MAIN], KSHIM_STR_RESTART, KSHIM_MENU_DESC_RESTART,
                    MENU_RESET, 0, KSHIM_MENU_ROLE_RESTART) ||
        menu_append(&menu->pages[MENU_PAGE_MAIN], KSHIM_STR_POWER_OFF, KSHIM_MENU_DESC_OFF,
                    MENU_OFF, 0, KSHIM_MENU_ROLE_OFF))
        return -1;
#endif

    /* Settings page: device / default / language / back. */
    settings_fill_device_name(menu, menu->setting_device_name, sizeof(menu->setting_device_name));
    menu->device_entry_row = menu->pages[MENU_PAGE_SETTINGS].count;
    if (menu_append(&menu->pages[MENU_PAGE_SETTINGS], menu->setting_device_name,
                    kshim_menu_text(KSHIM_TEXT_DEVICE_DESC), MENU_DEVICE, 0,
                    KSHIM_MENU_ROLE_DEVICE))
        return -1;
    settings_fill_default_name(menu, menu->setting_default_name, sizeof(menu->setting_default_name));
    menu->default_entry_row = menu->pages[MENU_PAGE_SETTINGS].count;
    if (menu_append(&menu->pages[MENU_PAGE_SETTINGS], menu->setting_default_name,
                    kshim_menu_text(KSHIM_TEXT_DEFAULT_DESC), MENU_DEFAULT, 0,
                    KSHIM_MENU_ROLE_DEFAULT))
        return -1;
#if KSHIM_MENU_LANGUAGE_ENABLED
    menu->language_entry_row = menu->pages[MENU_PAGE_SETTINGS].count;
    if (menu_append(&menu->pages[MENU_PAGE_SETTINGS], KSHIM_MENU_TEXT_LANGUAGE_LABEL,
                    kshim_menu_text(KSHIM_TEXT_LANGUAGE_DESC), MENU_LANGUAGE, 0,
                    KSHIM_MENU_ROLE_LANGUAGE))
        return -1;
#endif
    if (menu_append(&menu->pages[MENU_PAGE_SETTINGS], KSHIM_MENU_ACTION_BACK,
                    KSHIM_MENU_DESC_BACK_PARENT,
                    MENU_BACK, 0, KSHIM_MENU_ROLE_BACK))
        return -1;

    /* Device page: auto / TB322FC / TB323FU / back. */
    if (menu_append(&menu->pages[MENU_PAGE_DEVICE], KSHIM_MENU_LABEL_DEV_AUTO,
                    kshim_menu_text(KSHIM_TEXT_DEVICE_AUTO_DESC), MENU_DEVICE_SET, BOOT_DEVICE_AUTO,
                    KSHIM_MENU_ROLE_DEVICE_AUTO) ||
        menu_append(&menu->pages[MENU_PAGE_DEVICE], KSHIM_MENU_LABEL_DEV_322,
                    kshim_menu_text(KSHIM_TEXT_DEVICE_TB322FC_DESC), MENU_DEVICE_SET, BOOT_DEVICE_TB322FC,
                    KSHIM_MENU_ROLE_DEVICE_TB322FC) ||
        menu_append(&menu->pages[MENU_PAGE_DEVICE], KSHIM_MENU_LABEL_DEV_323,
                    kshim_menu_text(KSHIM_TEXT_DEVICE_TB323FU_DESC), MENU_DEVICE_SET, BOOT_DEVICE_TB323FU,
                    KSHIM_MENU_ROLE_DEVICE_TB323FU) ||
        menu_append(&menu->pages[MENU_PAGE_DEVICE], KSHIM_MENU_ACTION_BACK,
                    KSHIM_MENU_DESC_BACK_PARENT,
                    MENU_BACK, 0, KSHIM_MENU_ROLE_BACK))
        return -1;
    menu->pages[MENU_PAGE_DEVICE].initial = (size_t)menu->boot.device_choice;

    /* Default page: Android / internal / TF / back. A row is selectable only
     * when its target resolves to a unique candidate; otherwise it stays
     * visible but unavailable, exactly like the main system rows. */
    {
        static const kshim_menu_role_t default_roles[3] = {
            KSHIM_MENU_ROLE_ANDROID, KSHIM_MENU_ROLE_INTERNAL, KSHIM_MENU_ROLE_TF};
        static const enum boot_default_target default_targets[3] = {
            BOOT_TARGET_ANDROID, BOOT_TARGET_INTERNAL, BOOT_TARGET_TF};
        struct menu_page *page = &menu->pages[MENU_PAGE_DEFAULT];
        size_t caller = 0, slot;

        for (slot = 0; slot < 3; ++slot) {
            const char *name = kshim_menu_role_name(default_roles[slot], NULL);
            if (settings_system_caller(menu, default_targets[slot], &caller)) {
                if (menu_append(page, name,
                                kshim_menu_role_description(default_roles[slot], NULL),
                                MENU_DEFAULT_SET, default_targets[slot], default_roles[slot]))
                    return -1;
            } else if (menu_append(page, name, kshim_menu_text(KSHIM_TEXT_UNAVAILABLE),
                                   MENU_UNAVAILABLE, 0, default_roles[slot])) {
                return -1;
            }
        }
    }
    if (menu_append(&menu->pages[MENU_PAGE_DEFAULT], KSHIM_MENU_ACTION_BACK,
                    KSHIM_MENU_DESC_BACK_PARENT,
                    MENU_BACK, 0, KSHIM_MENU_ROLE_BACK))
        return -1;
    menu->pages[MENU_PAGE_DEFAULT].initial = settings_default_row(menu);

#if KSHIM_MENU_LANGUAGE_ENABLED
    /* Language page: zh / en / back, reachable from Settings. */
    if (menu_append(&menu->pages[MENU_PAGE_LANGUAGE], KSHIM_MENU_LABEL_LANG_ZH,
                    kshim_menu_text(KSHIM_TEXT_DESC_LANG_ZH), MENU_LANG_ZH, 0,
                    KSHIM_MENU_ROLE_LANG_ZH) ||
        menu_append(&menu->pages[MENU_PAGE_LANGUAGE], KSHIM_MENU_LABEL_LANG_EN,
                    kshim_menu_text(KSHIM_TEXT_DESC_LANG_EN), MENU_LANG_EN, 0,
                    KSHIM_MENU_ROLE_LANG_EN) ||
        menu_append(&menu->pages[MENU_PAGE_LANGUAGE], KSHIM_MENU_ACTION_BACK,
                    KSHIM_MENU_DESC_BACK_PARENT,
                    MENU_BACK, 0, KSHIM_MENU_ROLE_BACK))
        return -1;
    menu->pages[MENU_PAGE_LANGUAGE].initial =
        kshim_menu_get_language() == KSHIM_MENU_LANG_ZH_CN ? 0U : 1U;
#endif
    return 0;
}

/* Rebuild every page from the cached caller inputs for the active locale. */
static int menu_fill(struct menu_selection *menu)
{
    if (menu->settings) return menu_fill_settings(menu);
    for (size_t p = 0; p < 4; ++p) {
        menu->pages[p].count = 0;
        menu->pages[p].initial = 0;
    }
    for (size_t i = 0; i < menu->caller_count; ++i) {
        size_t page_index = menu->advanced && (menu->advanced_mask & (UINT64_C(1) << i)) ? 1U : 0U;
        struct menu_page *page = &menu->pages[page_index];
        if (i == menu->original_initial) {
            menu->page = page_index;
            page->initial = page->count;
        }
        kshim_menu_role_t role = menu->caller_roles ? menu->caller_roles[i] : KSHIM_MENU_ROLE_UNKNOWN;
        const char *name = menu->caller_names[i];
        const char *description = menu->caller_descriptions ? menu->caller_descriptions[i] : NULL;
#if KSHIM_MENU_LANGUAGE_ENABLED
        name = kshim_menu_role_name(role, name);
        description = kshim_menu_role_description(role, description);
#endif
        if (menu_append(page, name, description, MENU_PAYLOAD, i, role)) return -1;
    }
    if (menu->advanced) {
        menu->advanced_row = menu->pages[0].count;
        if (menu_append(&menu->pages[0], KSHIM_STR_ADVANCED, KSHIM_MENU_DESC_ADVANCED,
                        MENU_ADVANCED, 0, KSHIM_MENU_ROLE_ADVANCED) ||
            menu_append(&menu->pages[1], KSHIM_STR_RECOVERY, KSHIM_MENU_DESC_RECOVERY,
                        MENU_RECOVERY, 0, KSHIM_MENU_ROLE_RECOVERY) ||
            menu_append(&menu->pages[1], KSHIM_STR_FASTBOOT, KSHIM_MENU_DESC_FASTBOOT,
                        MENU_FASTBOOT, 0, KSHIM_MENU_ROLE_FASTBOOT) ||
            menu_append(&menu->pages[1], KSHIM_STR_EDL, KSHIM_MENU_DESC_EDL,
                        MENU_EDL, 0, KSHIM_MENU_ROLE_EDL) ||
            menu_append(&menu->pages[1], KSHIM_STR_BACK, KSHIM_MENU_DESC_BACK,
                        MENU_BACK, 0, KSHIM_MENU_ROLE_BACK)) return -1;
    }
#if KSHIM_MENU_LANGUAGE_ENABLED
    if (menu->language) {
        menu->language_row = menu->pages[0].count;
        if (menu_append(&menu->pages[0], KSHIM_MENU_TEXT_LANGUAGE_LABEL,
                        kshim_menu_text(KSHIM_TEXT_LANGUAGE_DESC), MENU_LANGUAGE, 0,
                        KSHIM_MENU_ROLE_LANGUAGE) ||
            menu_append(&menu->pages[2], KSHIM_MENU_LABEL_LANG_ZH,
                        kshim_menu_text(KSHIM_TEXT_DESC_LANG_ZH), MENU_LANG_ZH, 0,
                        KSHIM_MENU_ROLE_LANG_ZH) ||
            menu_append(&menu->pages[2], KSHIM_MENU_LABEL_LANG_EN,
                        kshim_menu_text(KSHIM_TEXT_DESC_LANG_EN), MENU_LANG_EN, 0,
                        KSHIM_MENU_ROLE_LANG_EN) ||
            menu_append(&menu->pages[2], KSHIM_STR_BACK, KSHIM_MENU_DESC_BACK,
                        MENU_BACK, 0, KSHIM_MENU_ROLE_BACK)) return -1;
    }
#endif
    /* Read-only Device info: one home row that precedes the power rows. A NULL
     * record leaves the pages exactly as before. */
    if (menu->info && menu_fill_device_info(menu)) return -1;
#if CONFIG_KSHIM_MENU_POWER_ACTIONS
    if (menu_append(&menu->pages[0], KSHIM_STR_RESTART, KSHIM_MENU_DESC_RESTART,
                    MENU_RESET, 0, KSHIM_MENU_ROLE_RESTART) ||
        menu_append(&menu->pages[0], KSHIM_STR_POWER_OFF, KSHIM_MENU_DESC_OFF,
                    MENU_OFF, 0, KSHIM_MENU_ROLE_OFF)) return -1;
#endif
    return 0;
}
static int menu_build(struct menu_selection *menu, const char *const *names,
                      const char *const *descriptions, const kshim_menu_role_t *roles,
                      const struct device_info *info,
                      const enum device_board *payload_boards,
                      size_t count, size_t initial, uint64_t mask, int advanced, int language,
                      int settings)
{
    if (!names || !count || initial >= count) return -1;
    if (settings) {
        /* The settings pages add fixed rows on top of the payload rows; the
         * per-page array bound is what actually limits count. */
        if (count > KSHIM_MENU_MAX_ENTRIES || count > 63U) return -1;
    } else if (count > 49U) {
        return -1;
    }
    if (mask >> count) return -1;
    menu->caller_names = names;
    menu->caller_descriptions = descriptions;
    menu->caller_roles = roles;
    menu->caller_count = count;
    menu->initial_caller = initial;
    menu->original_initial = initial;
    menu->advanced = advanced;
    menu->language = language;
    menu->advanced_mask = mask;
    menu->info = info;
    menu->advanced_row = 0;
    menu->language_row = 0;
    menu->device_info_row = 0;
    menu->settings = settings;
    menu->payload_boards = payload_boards;
    /* The legacy menu has no board filtering, so its initial row is always a
     * valid automatic target; the settings path proves that separately. */
    menu->default_ok = settings ? 0 : 1;
    if (settings) {
        struct boot_settings boot;
        if (boot_settings_init(&boot) != BOOT_SETTINGS_OK) return -1;
        menu->boot = boot;
        settings_recompute_candidates(menu);
        settings_resolve_default(menu);
    }
    return menu_fill(menu);
}
/* 1 = payload selected; 2 = changed page; 0 = returned firmware action failed. */
static int menu_activate(struct menu_selection *menu, size_t index,
                         size_t *selected, const char **failure)
{
    struct menu_page *page = &menu->pages[menu->page];
    enum menu_action action = page->actions[index];
    *failure = NULL;
    if (action == MENU_PAYLOAD) {
        *selected = page->payloads[index];
        return 1;
    }
    /* This flag belongs to the whole menu session, not to an individual page. */
    menu->timeout = 0;
    if (action == MENU_ADVANCED) {
        menu->page = 1U;
        menu->pages[1].initial = 0U;
        return 2;
    }
    if (action == MENU_DEVICE_INFO) {
        /* Snapshot the fully built home page (info row plus the power rows) so
         * every row keeps its exact name, role, payload and position; only the
         * info row becomes the read-only summary with a localized Back. */
        menu->pages[3] = menu->pages[0];
        menu->pages[3].descriptions[menu->device_info_row] = menu->info_summary;
        menu->pages[3].actions[menu->device_info_row] = MENU_INFO_FIELD;
        menu->pages[3].action_labels[menu->device_info_row] = KSHIM_MENU_ACTION_BACK;
        menu->pages[3].initial = menu->device_info_row;
        menu->page = 3U;
        return 2;
    }
    /* A field confirmation is read-only: same return as Back but with its own
     * action, so it can never fall through to a payload, reboot or power path. */
    if (action == MENU_INFO_FIELD) {
        menu->page = 0U;
        menu->pages[0].initial = menu->device_info_row;
        return 2;
    }
    if (action == MENU_BACK) {
        size_t from = menu->page;
        if (menu->settings) {
            /* Every BACK returns to its immediate parent layer. */
            switch (from) {
            case MENU_PAGE_ADVANCED:
                menu->page = MENU_PAGE_MAIN;
                menu->pages[MENU_PAGE_MAIN].initial = menu->advanced_row;
                break;
            case MENU_PAGE_INFO:
                menu->page = MENU_PAGE_MAIN;
                menu->pages[MENU_PAGE_MAIN].initial = menu->device_info_row;
                break;
            case MENU_PAGE_SETTINGS:
                menu->page = MENU_PAGE_MAIN;
                menu->pages[MENU_PAGE_MAIN].initial = menu->settings_row;
                break;
            case MENU_PAGE_DEVICE:
                menu->page = MENU_PAGE_SETTINGS;
                menu->pages[MENU_PAGE_SETTINGS].initial = menu->device_entry_row;
                break;
            case MENU_PAGE_DEFAULT:
                menu->page = MENU_PAGE_SETTINGS;
                menu->pages[MENU_PAGE_SETTINGS].initial = menu->default_entry_row;
                break;
            case MENU_PAGE_LANGUAGE:
                menu->page = MENU_PAGE_SETTINGS;
                menu->pages[MENU_PAGE_SETTINGS].initial = menu->language_entry_row;
                break;
            default:
                menu->page = MENU_PAGE_MAIN;
                break;
            }
            return 2;
        }
        menu->page = 0U;
        if (from == 3U)
            menu->pages[0].initial = menu->device_info_row;
        else
            menu->pages[0].initial = from == 2U ? menu->language_row : menu->advanced_row;
        return 2;
    }
    if (action == MENU_SETTINGS) {
        menu->page = MENU_PAGE_SETTINGS;
        menu->pages[MENU_PAGE_SETTINGS].initial = 0U;
        return 2;
    }
    if (action == MENU_DEVICE) {
        menu->page = MENU_PAGE_DEVICE;
        menu->pages[MENU_PAGE_DEVICE].initial = (size_t)menu->boot.device_choice;
        return 2;
    }
    if (action == MENU_DEFAULT) {
        menu->page = MENU_PAGE_DEFAULT;
        menu->pages[MENU_PAGE_DEFAULT].initial = settings_default_row(menu);
        return 2;
    }
    if (action == MENU_UNAVAILABLE) {
        /* Never returns a payload: the effective board has no matching image. */
        *failure = KSHIM_STR_UNAVAILABLE;
        printf("%s\n", *failure);
        return 0;
    }
    if (action == MENU_DEVICE_SET) {
        struct boot_settings next = menu->boot;
        next.device_choice = (enum boot_device_choice)page->payloads[index];
        if (boot_settings_validate(&next) != BOOT_SETTINGS_OK) {
            *failure = KSHIM_STR_UNAVAILABLE;
            return 0;
        }
        menu->boot = next;
        /* Re-filter and re-resolve the default so a stale initial can never
         * auto-boot a wrong-board Linux image. */
        settings_recompute_candidates(menu);
        settings_resolve_default(menu);
        if (menu_fill(menu)) return 2;
        menu->page = MENU_PAGE_SETTINGS;
        menu->pages[MENU_PAGE_SETTINGS].initial = menu->device_entry_row;
        return 2;
    }
    if (action == MENU_DEFAULT_SET) {
        enum boot_default_target old = menu->boot.default_target;
        menu->boot.default_target = (enum boot_default_target)page->payloads[index];
        settings_resolve_default(menu);
        if (boot_settings_validate(&menu->boot) != BOOT_SETTINGS_OK || !menu->default_ok) {
            /* Not a uniquely existing Android/internal/TF target: keep the
             * previous value and report the same "no matching content" note. */
            menu->boot.default_target = old;
            settings_resolve_default(menu);
            *failure = KSHIM_STR_UNAVAILABLE;
            printf("%s\n", *failure);
            return 0;
        }
        if (menu_fill(menu)) return 2;
        menu->page = MENU_PAGE_MAIN;
        return 2;
    }
#if KSHIM_MENU_LANGUAGE_ENABLED
    if (action == MENU_LANGUAGE) {
        menu->page = 2U;
        menu->pages[2].initial = 0U;
        return 2;
    }
    if (action == MENU_LANG_ZH || action == MENU_LANG_EN) {
        (void)kshim_menu_set_language(action == MENU_LANG_ZH ?
            KSHIM_MENU_LANG_ZH_CN : KSHIM_MENU_LANG_EN);
        /* Rebuilt in the new locale; device/default choices and the resolved
         * default are preserved because they live in menu->boot. */
        if (menu_fill(menu)) return 2;
        if (menu->settings) {
            menu->page = MENU_PAGE_SETTINGS;
            menu->pages[MENU_PAGE_SETTINGS].initial = menu->language_entry_row;
        } else {
            menu->page = 0U;
            menu->pages[0].initial = menu->language_row;
        }
        return 2;
    }
#endif
#if CONFIG_KSHIM_MENU_ADVANCED_ACTIONS
    if (action == MENU_RECOVERY || action == MENU_FASTBOOT || action == MENU_EDL) {
        enum kshim_qcom_reboot_target target = action == MENU_RECOVERY ?
            KSHIM_QCOM_REBOOT_RECOVERY : action == MENU_FASTBOOT ?
            KSHIM_QCOM_REBOOT_BOOTLOADER : KSHIM_QCOM_REBOOT_EDL;
        int result = kshim_qcom_reboot_request(menu->fdt, target);
        *failure = result == KSHIM_QCOM_REBOOT_ROLLBACK_FAILED ?
            KSHIM_STR_RESTORE_FAILED : KSHIM_STR_MODE_FAILED;
        printf("%s: %d\n", *failure, result);
        return 0;
    }
#endif
    *failure = KSHIM_STR_POWER_FAILED;
#if CONFIG_KSHIM_MENU_POWER_ACTIONS
    if (menu->psci_ready) {
        int64_t result = action == MENU_RESET ? kshim_psci_request_system_reset() :
                                                kshim_psci_request_system_off();
        printf("%s: %lld\n", *failure, (long long)result);
    } else
#endif
        printf("%s\n", *failure);
    return 0;
}
static void serial_show_page(const struct menu_page *page)
{
    printf("\n%s\n", KSHIM_STR_MENU_TITLE);
    for (size_t i = 0; i < page->count; ++i) {
        printf("  %u. %s%s\n", (unsigned)i + 1U, page->names[i],
               i == page->initial ? KSHIM_STR_MENU_DEFAULT : "");
        /* Read-only info rows print their value below the label so the field
         * text stays visible without the framebuffer UI; every other page
         * keeps its exact previous serial output. */
        if (page->actions[i] == MENU_INFO_FIELD && page->descriptions[i] &&
            page->descriptions[i][0])
            printf("     %s\n", page->descriptions[i]);
    }
    printf("%s", KSHIM_STR_MENU_PROMPT);
}
static void serial_cancel_timeout(struct menu_selection *menu)
{
    if (menu->timeout) printf("\n%s\n", KSHIM_STR_AUTO_CANCELLED);
    menu->timeout = 0;
}
static int serial_select(struct menu_selection *menu, size_t *selected)
{
    int keys_ready = build_keys(menu->fdt) == 0;
    struct menu_page *page = &menu->pages[menu->page];
    size_t current = page->initial, numeric = 0;
    serial_show_page(page);
    uint64_t start = get_current_time();
    for (;;) {
        int confirm = 0;
        size_t confirmed_index = current;
        char c;
        if (kshim_console_try_read(&c)) {
            if (c >= '0' && c <= '9') {
                size_t digit = (size_t)(c - '0');
                if (numeric > 0 || digit > 0) {
                    size_t next = numeric * 10U + digit;
                    if (next <= page->count) {
                        numeric = next;
                        kshim_console_write(&c, 1);
                        serial_cancel_timeout(menu);
                    }
                }
            } else if (c == '\r' || c == '\n') {
                if (numeric >= 1U && numeric <= page->count) {
                    confirmed_index = numeric - 1U;
                    confirm = 1;
                }
                numeric = 0;
            } else if (c == 27) {
                if (menu->page != 0U) {
                    /* Every subpage ends with the explicit Back action. The info
                     * page's trailing row may fall behind the home snapshot tail,
                     * so Escape targets its own row there; other pages keep the
                     * last row. */
                    confirmed_index = (menu->page == 3U)
                        ? menu->device_info_row
                        : page->count - 1U;
                    confirm = 1;
                    serial_cancel_timeout(menu);
                } else if (menu->default_ok) {
                    /* Only a proven, board-safe default may be auto-selected;
                     * with none resolved, Escape never guesses a system. */
                    *selected = menu->original_initial;
                    return 0;
                }
            }
        }
        if (!confirm && keys_ready) {
            QcomKeyEvent event;
            if (QcomKeysPoll(&Keys, &event) == QCOM_KEYS_EVENT &&
                event.Type == QCOM_KEY_EVENT_PRESS && event.Pressed) {
                int consumed = 0;
                if (event.Action == QCOM_KEY_ACTION_UP) {
                    current = current ? current - 1U : page->count - 1U;
                    consumed = 1;
                }
                if (event.Action == QCOM_KEY_ACTION_DOWN) {
                    current = (current + 1U) % page->count;
                    consumed = 1;
                }
                if (event.Action == QCOM_KEY_ACTION_SELECT) {
                    consumed = 1;
                    confirmed_index = current;
                    confirm = 1;
                }
                if (consumed) {
                    serial_cancel_timeout(menu);
                    printf("\n> %s\n", page->names[current]);
                }
            }
        }
        if (confirm) {
            const char *failure;
            int result = menu_activate(menu, confirmed_index, selected, &failure);
            if (result == 1) return 0;
            if (result == 2) {
                page = &menu->pages[menu->page];
                current = page->initial;
                serial_show_page(page);
            } else {
                printf("%s", KSHIM_STR_MENU_PROMPT);
            }
            numeric = 0;
            continue;
        }
        if (menu->timeout && menu->default_ok &&
            get_current_time() - start >= (uint64_t)menu->timeout) {
            *selected = menu->original_initial;
            return 0;
        }
        (void)delay(5000);
    }
}
static int platform_select(uint64_t fdt, const char *const *names,
                           const char *const *descriptions,
                           const kshim_menu_role_t *roles,
                           const struct device_info *info,
                           const enum device_board *payload_boards, size_t count,
                           size_t initial, uint32_t timeout_ms, uint64_t advanced_mask,
                           int advanced, int language, int settings, size_t *selected)
{
    if (!selected) return -1;
    struct menu_selection menu = {.fdt = (const void *)(uintptr_t)fdt,
                                  .timeout = timeout_ms, .info = info};
    if (menu_build(&menu, names, descriptions, roles, info, payload_boards, count, initial,
                   advanced_mask, advanced, language, settings)) return -1;
    /* No unique, board-safe default means no automatic selection at all; the
     * user must choose explicitly, and a wrong-board Linux image can never be
     * auto-booted. */
    if (settings && !menu.default_ok) menu.timeout = 0;
#if CONFIG_KSHIM_MENU_POWER_ACTIONS
    int pi = kshim_psci_init(menu.fdt);
    menu.psci_ready = pi == 0;
    if (pi) printf("PSCI init failed: %d\n", pi);
#endif
#if CONFIG_KSHIM_FRAMEBUFFER && CONFIG_KSHIM_LVGL
    struct menu_page *page = &menu.pages[menu.page];
    int status = start_menu(menu.fdt);
#if !CONFIG_KSHIM_DEBUG_CONSOLE
    /* True once the initial GUI page was shown and the interactive loop was
     * entered; a later subpage rebuild failure keeps it true. */
    int gui_entered = 0;
#endif
    if (!status && kshim_lvgl_set_entries_described(&Lvgl, page->names, page->descriptions,
                                                   page->action_labels, page->count,
                                                   page->initial) == 0) {
#if !CONFIG_KSHIM_DEBUG_CONSOLE
        gui_entered = 1;
#endif
        kshim_lvgl_set_entry_roles(&Lvgl, page->roles, page->count);
        kshim_boot_metrics_record_once(KSHIM_BOOT_STAGE_ENTRIES);
        kshim_lvgl_set_countdown(&Lvgl, menu.timeout);
        uint64_t start = get_current_time();
        for (;;) {
            menu_frame();
            if (menu.timeout && kshim_lvgl_has_interacted(&Lvgl)) {
                menu.timeout = 0;
                kshim_lvgl_set_countdown(&Lvgl, 0);
                kshim_lvgl_set_status(&Lvgl, KSHIM_STR_AUTO_CANCELLED);
            }
            int index = kshim_lvgl_take_index(&Lvgl);
            if (index >= 0 && (size_t)index < page->count) {
                const char *failure;
                int result = menu_activate(&menu, (size_t)index, selected, &failure);
                if (result == 1) break;
                kshim_lvgl_set_countdown(&Lvgl, 0);
                if (result == 2) {
                    page = &menu.pages[menu.page];
                    if (kshim_lvgl_set_entries_described(&Lvgl, page->names, page->descriptions,
                                                        page->action_labels, page->count,
                                                        page->initial)) {
                        /* Keep the selected page and cancelled timeout on serial fallback. */
                        status = -1;
                        break;
                    }
                    kshim_lvgl_set_entry_roles(&Lvgl, page->roles, page->count);
                } else {
                    kshim_lvgl_set_status(&Lvgl, failure);
                }
                continue;
            }
            if (menu.timeout && menu.default_ok &&
                get_current_time() - start >= (uint64_t)menu.timeout) {
                *selected = menu.original_initial;
                break;
            }
            (void)delay(5000);
        }
        if (!status) {
#if CONFIG_KSHIM_DIAGNOSTIC_UI_ONLY
            int close = kshim_platform_prepare_handoff();
            ui_probe_report((void *)(uintptr_t)fdt, close, *selected);
            return close ? -2 : 0;
#else
            return kshim_platform_prepare_handoff() ? -2 : 0;
#endif
        }
    }
#if CONFIG_KSHIM_DIAGNOSTIC_UI_ONLY
    {
        int close = kshim_platform_prepare_handoff();
        ui_probe_report((void *)(uintptr_t)fdt, close, initial);
        if (close) return -2;
    }
#else
    if (kshim_platform_prepare_handoff()) return -2;
#endif
#if !CONFIG_KSHIM_DEBUG_CONSOLE
    /* Approved minimal gate: the initial GUI never started on TB322FC and the
     * caller's original initial row is the Android payload. Cleanup above has
     * already succeeded, so resolve the caller's original index directly and
     * skip serial_select and its extra build_keys/poll for this case only. */
    if (!gui_entered && info && info->board == DEVICE_BOARD_TB322FC &&
        roles && menu.initial_caller < count &&
        roles[menu.initial_caller] == KSHIM_MENU_ROLE_ANDROID) {
        *selected = menu.initial_caller;
        return 0;
    }
#endif
#endif
    return serial_select(&menu, selected);
}
int kshim_platform_select(uint64_t fdt, const char *const *names, size_t count,
                           size_t initial, uint32_t timeout_ms, size_t *selected)
{
    /* Compatibility entry point: flat rows, no role labels, no language page. */
    return platform_select(fdt, names, NULL, NULL, NULL, NULL, count, initial, timeout_ms,
                           0, 0, 0, 0, selected);
}
int kshim_platform_select_advanced(uint64_t fdt, const char *const *names,
                                  size_t count, size_t initial,
                                  uint32_t timeout_ms, uint64_t advanced_mask,
                                  size_t *selected)
{
    return kshim_platform_select_described(fdt, names, NULL, count, initial,
                                           timeout_ms, advanced_mask, selected);
}
int kshim_platform_select_described(uint64_t fdt, const char *const *names,
                                   const char *const *descriptions, size_t count,
                                   size_t initial, uint32_t timeout_ms,
                                   uint64_t advanced_mask, size_t *selected)
{
    /* No caller roles: every payload keeps its own name/description text. */
#if CONFIG_KSHIM_MENU_ADVANCED_ACTIONS
    return platform_select(fdt, names, descriptions, NULL, NULL, NULL, count, initial, timeout_ms,
                            advanced_mask, 1, KSHIM_MENU_LANGUAGE_ENABLED, 0, selected);
#else
    (void)advanced_mask;
    return platform_select(fdt, names, descriptions, NULL, NULL, NULL, count, initial, timeout_ms,
                            0, 0, KSHIM_MENU_LANGUAGE_ENABLED, 0, selected);
#endif
}
int kshim_platform_select_localized(uint64_t fdt, const char *const *names,
                                    const char *const *descriptions,
                                    const kshim_menu_role_t *roles, size_t count,
                                    size_t initial, uint32_t timeout_ms,
                                    uint64_t advanced_mask, size_t *selected)
{
#if CONFIG_KSHIM_MENU_ADVANCED_ACTIONS
    return platform_select(fdt, names, descriptions, roles, NULL, NULL, count, initial, timeout_ms,
                            advanced_mask, 1, KSHIM_MENU_LANGUAGE_ENABLED, 0, selected);
#else
    (void)advanced_mask;
    return platform_select(fdt, names, descriptions, roles, NULL, NULL, count, initial, timeout_ms,
                            0, 0, KSHIM_MENU_LANGUAGE_ENABLED, 0, selected);
#endif
}
int kshim_platform_select_localized_info(uint64_t fdt, const char *const *names,
                                         const char *const *descriptions,
                                         const kshim_menu_role_t *roles,
                                         const struct device_info *info,
                                         size_t count, size_t initial,
                                         uint32_t timeout_ms,
                                         uint64_t advanced_mask, size_t *selected)
{
#if CONFIG_KSHIM_MENU_ADVANCED_ACTIONS
    return platform_select(fdt, names, descriptions, roles, info, NULL, count, initial, timeout_ms,
                            advanced_mask, 1, KSHIM_MENU_LANGUAGE_ENABLED, 0, selected);
#else
    (void)advanced_mask;
    return platform_select(fdt, names, descriptions, roles, info, NULL, count, initial, timeout_ms,
                            0, 0, KSHIM_MENU_LANGUAGE_ENABLED, 0, selected);
#endif
}
int kshim_platform_select_localized_settings(uint64_t fdt, const char *const *names,
                                             const char *const *descriptions,
                                             const kshim_menu_role_t *roles,
                                             const struct device_info *info,
                                             const enum device_board *payload_boards,
                                             size_t count, size_t initial,
                                             uint32_t timeout_ms,
                                             uint64_t advanced_mask, size_t *selected)
{
    /* Only this entry point turns the session settings on. */
#if CONFIG_KSHIM_MENU_ADVANCED_ACTIONS
    return platform_select(fdt, names, descriptions, roles, info, payload_boards, count, initial,
                            timeout_ms, advanced_mask, 1, KSHIM_MENU_LANGUAGE_ENABLED, 1, selected);
#else
    (void)advanced_mask;
    return platform_select(fdt, names, descriptions, roles, info, payload_boards, count, initial,
                            timeout_ms, 0, 0, KSHIM_MENU_LANGUAGE_ENABLED, 1, selected);
#endif
}
int kshim_platform_main(uint64_t fdt, uint64_t arg1, uint64_t arg2, uint64_t arg3)
{
    kshim_boot_set_args(fdt, arg1, arg2, arg3);
#if CONFIG_KSHIM_SMP_SELFTEST
    int status = kshim_smp_selftest((const void *)(uintptr_t)fdt);
    printf("SMP_SELFTEST %s (%d)\n", status == 0 ? "PASS" : "FAIL", status);
    kshim_psci_system_off();
#endif
    const char *names[] = {CONFIG_KSHIM_MENU_ENTRY1, CONFIG_KSHIM_MENU_ENTRY2};
    for (;;) {
        size_t selected;
        if (kshim_platform_select(fdt, names, 2, 0, 0, &selected) == -2) {
            printf("%s\n", KSHIM_STR_HANDOFF_UNSAFE);
            for (;;) (void)delay(100000);
        }
        if (kshim_menu_activate(selected ? KSHIM_MENU_RECOVERY : KSHIM_MENU_BOOT))
            printf("%s\n", KSHIM_STR_UNABLE_START);
    }
}
