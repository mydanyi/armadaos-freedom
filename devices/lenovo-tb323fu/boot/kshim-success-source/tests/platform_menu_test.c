/* Exercise the real platform selection loop; hardware and UI are scripted. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#define CONFIG_KSHIM_FRAMEBUFFER 1
#define CONFIG_KSHIM_LVGL 1
#define CONFIG_QCOM_KEYS 1
#define CONFIG_QCOM_KEY_DEBOUNCE_SAMPLES 2
#define CONFIG_KSHIM_SMP 0
#define CONFIG_KSHIM_SMP_SELFTEST 0
#define CONFIG_KSHIM_TOUCH 0
#define CONFIG_KSHIM_DIAGNOSTIC_UI_ONLY 0
#define CONFIG_KSHIM_QEMU_PLATFORM 0
/* Compile-time debug-console switch.  The default profile keeps the console on
 * (matching the production Kconfig default); the dedicated OFF target forces 0
 * so the real loop sees the same gate the TB322FC auto-return path requires. */
#ifndef CONFIG_KSHIM_DEBUG_CONSOLE
#define CONFIG_KSHIM_DEBUG_CONSOLE 1
#endif
#ifndef CONFIG_KSHIM_MENU_CHINESE
#define CONFIG_KSHIM_MENU_CHINESE 0
#endif
#define CONFIG_KSHIM_MENU_ENTRY1 "Payload 1"
#define CONFIG_KSHIM_MENU_ENTRY2 "Payload 2"
#ifndef CONFIG_KSHIM_MENU_POWER_ACTIONS
#define CONFIG_KSHIM_MENU_POWER_ACTIONS 1
#endif
/* Only the dedicated language target defines this; the legacy profiles keep
 * the old grouping-only fixture untouched. */
#ifndef CONFIG_KSHIM_MENU_LANGUAGE_SWITCH
#define CONFIG_KSHIM_MENU_LANGUAGE_SWITCH 0
#endif

#include "../src/core/platform.c"

enum { MAX_EVENTS = 32, MAX_STEPS = 12000, MAX_PAGES = 16 };
typedef struct { uint64_t at; int index; int interacted; } ui_event_t;
typedef struct { uint64_t at; char value; } serial_event_t;
typedef struct { uint64_t at; int result; QcomKeyEvent event; } key_event_t;
typedef struct { size_t count, initial; bool advanced; } page_event_t;
/* The Device-info tests capture every rendered page generically so the legacy
 * positional payload/power assertions never apply to them.  The value strings
 * are owned by the platform loop's stack frame, so they are copied here before
 * the call returns; a NULL description/action is tracked with an explicit flag. */
/* The snapshot bound must hold a full home page at the 49-payload cap (51/53
 * rows), so it tracks the production entry limit instead of a small constant. */
/* The Device-info summary is now one localized multi-line aggregate held in a
 * single description slot, so the slot must fit the whole <=2048-byte buffer;
 * 192 would silently clip it in the snapshot. */
/* The session-settings cases walk several pages (home / advanced / settings /
 * device / default / language and back), so the snapshot ring must hold more
 * than the original eight info pages. The name column is widened so a full
 * localized settings value is never truncated by the snapshot itself. */
enum { INFO_SNAPS = 24, INFO_SNAP_ROWS = KSHIM_MENU_MAX_ENTRIES,
       INFO_TEXT_LEN = 2048, INFO_ACTION_LEN = 32 };
typedef struct {
    size_t count, initial;
    char names[INFO_SNAP_ROWS][96];
    char descriptions[INFO_SNAP_ROWS][INFO_TEXT_LEN];
    char actions[INFO_SNAP_ROWS][INFO_ACTION_LEN];
    bool desc_null[INFO_SNAP_ROWS];
    bool action_null[INFO_SNAP_ROWS];
    kshim_menu_role_t roles[INFO_SNAP_ROWS];
} info_snap_t;
#if CONFIG_KSHIM_MENU_LANGUAGE_SWITCH
/* Per-page snapshot so the language tests can inspect names, descriptions and
 * action labels after capture. 53 rows fit 49 payloads plus the advanced,
 * language and two power rows without editing any production header. */
enum { LANG_ROW_CAP = 53, LANG_PAGES = 8 };
typedef struct {
    int kind; /* 0 = home, 1 = advanced, 2 = language */
    size_t count, initial;
    char names[LANG_ROW_CAP][32];
    const char *descriptions[LANG_ROW_CAP];
    const char *actions[LANG_ROW_CAP];
} lang_page_t;
#endif
static struct {
    uint64_t now;
    unsigned steps;
    bool serial, interacted, ui_alive, advanced, advanced_page;
    int pending, psci_result, ui_init_result, entries_result;
    int64_t reset_result, off_result;
    unsigned init_calls, entries_calls, entries_fail_at, frames, deinit_calls, handoff_calls;
    unsigned psci_calls, reset_calls, off_calls, countdown_zero, countdown_started, status_calls;
    unsigned fb_calls, keys_build_calls;
    unsigned keys_adapter_calls, keys_begin_calls, keys_end_calls;
    unsigned keys_init_calls, keys_debounce_calls;
    /* Every QcomKeysPoll / kshim_console_try_read entry is counted so the OFF
     * gate can prove the serial loop was never entered. */
    unsigned key_poll_calls, console_read_calls;
    bool keys_scope_active;
    int keys_adapter_result, keys_parse_result, keys_init_result;
    size_t keys_parse_count;
    QcomKeyFdtAdapter *keys_adapter;
    unsigned reboot_calls;
    int reboot_result, reboot_target, handoff_result;
    uint64_t advanced_mask;
    uint32_t countdown_first;
    size_t payload_count, menu_count, initial;
    const void *psci_fdt, *reboot_fdt;
    /* 53 covers the language-enabled worst case: 49 payloads plus the advanced,
     * language and two power rows. */
    char status[160], labels[53][32], payload_labels[50][32];
    const char *names[50], *descriptions[50], *entry_descriptions[53];
    const char *entry_action_labels[53];
    kshim_menu_role_t roles[53]; size_t role_count; unsigned role_calls;
    /* Per-payload board evidence for the session-settings entry point; zero
     * (DEVICE_BOARD_UNKNOWN) means the caller has no matching evidence. */
    enum device_board payload_boards[50];
    ui_event_t ui[MAX_EVENTS]; size_t ui_count, ui_next;
    serial_event_t chars[MAX_EVENTS]; size_t char_count, char_next;
    key_event_t keys[MAX_EVENTS]; size_t key_count, key_next;
    page_event_t pages[MAX_PAGES]; size_t page_count;
    bool info_mode;
    struct device_info info_fixture;
    info_snap_t info_snaps[INFO_SNAPS]; size_t info_snap_count;
#if CONFIG_KSHIM_MENU_LANGUAGE_SWITCH
    lang_page_t snap[LANG_PAGES]; size_t snap_count;
#endif
    struct kshim_resources resources;
} h;
static unsigned fdt_cookie;
static const QcomKeyDescriptor key_descriptors[] = {
    {.Base = QCOM_KEY_SPMI_RESOURCE(0, 0, 0, 0x1300),
     .RegisterOffset = 0x10, .Mask = 128, .Code = QCOM_KEYCODE_POWER,
     .Action = QCOM_KEY_ACTION_SELECT, .Source = QCOM_KEY_SOURCE_PON,
     .ActiveLow = 0, .DebounceSamples = 3},
    {.Base = 0xf165000, .RegisterOffset = 4, .Mask = 1,
     .Code = QCOM_KEYCODE_VOLUMEUP, .Action = QCOM_KEY_ACTION_UP,
     .Source = QCOM_KEY_SOURCE_GPIO, .ActiveLow = 1, .DebounceSamples = 4},
    {.Base = QCOM_KEY_SPMI_RESOURCE(0, 0, 0, 0x1300),
     .RegisterOffset = 0x10, .Mask = 64, .Code = QCOM_KEYCODE_VOLUMEDOWN,
     .Action = QCOM_KEY_ACTION_DOWN, .Source = QCOM_KEY_SOURCE_PON,
     .ActiveLow = 0, .DebounceSamples = 5},
};

static void budget(void) { assert(++h.steps < MAX_STEPS); }
static uint64_t boot_fdt(void) { return (uint64_t)(uintptr_t)&fdt_cookie; }
static size_t expected_count(void)
{
    return h.payload_count + (CONFIG_KSHIM_MENU_POWER_ACTIONS ? 2U : 0U);
}
static void setup(bool serial, size_t count)
{
    memset(&h, 0, sizeof(h));
#if CONFIG_KSHIM_MENU_LANGUAGE_SWITCH
    /* The locale module keeps a session language; reset it to the configured
     * default so each case starts from a known locale. */
    (void)kshim_menu_set_language(CONFIG_KSHIM_MENU_CHINESE ?
        KSHIM_MENU_LANG_ZH_CN : KSHIM_MENU_LANG_EN);
#endif
    h.serial = serial;
    h.pending = -1;
    h.payload_count = count;
    h.keys_parse_count = 1;
    h.reset_result = h.off_result = -1;
    h.resources.framebuffer.render_address = 0x10000U;
    h.resources.framebuffer.width = 800;
    h.resources.framebuffer.height = 600;
    h.resources.framebuffer.stride = 3200;
    h.resources.framebuffer.bpp = 32;
    h.resources.framebuffer.format = KSHIM_FB_FORMAT_ARGB8888;
    for (size_t i = 0; i < count; ++i) {
        snprintf(h.payload_labels[i], sizeof(h.payload_labels[i]), "Payload %u", (unsigned)i + 1U);
        h.names[i] = h.payload_labels[i];
    }
}
static void ui_at(uint64_t at, int index, int interacted)
{
    assert(h.ui_count < MAX_EVENTS);
    h.ui[h.ui_count++] = (ui_event_t){at, index, interacted};
}
static void char_at(uint64_t at, char value)
{
    assert(h.char_count < MAX_EVENTS);
    h.chars[h.char_count++] = (serial_event_t){at, value};
}
static void text_at(uint64_t at, const char *text)
{
    while (*text) char_at(at, *text++);
}
__attribute__((unused))
static void key_at(uint64_t at, int result, QcomKeyEventType type,
                   QcomKeyAction action, uint8_t pressed)
{
    assert(h.key_count < MAX_EVENTS);
    h.keys[h.key_count++] = (key_event_t){at, result,
        {.Type = type, .Action = action, .Pressed = pressed}};
}
static void expect_psci_init(void)
{
    assert(h.psci_calls == (CONFIG_KSHIM_MENU_POWER_ACTIONS ? 1U : 0U));
    if (CONFIG_KSHIM_MENU_POWER_ACTIONS)
        assert(h.psci_fdt == (const void *)(uintptr_t)boot_fdt());
}
#if !CONFIG_KSHIM_MENU_LANGUAGE_SWITCH
static size_t choose(size_t initial, uint32_t timeout)
{
    size_t selected = SIZE_MAX;
    int rc = kshim_platform_select(boot_fdt(), h.names, h.payload_count,
                                    initial, timeout, &selected);
    assert(rc == 0);
    assert(selected < h.payload_count);
    expect_psci_init();
    assert(h.deinit_calls == 1U && h.handoff_calls == 1U);
    if (!h.serial && h.ui_init_result == 0 && h.entries_result == 0) {
        assert(h.init_calls == 1U && h.entries_calls == 1U);
        assert(h.menu_count == expected_count());
    }
    return selected;
}
#endif

uint64_t get_current_time(void) { return h.now; }
uint64_t delay(uint64_t us)
{
    budget();
    h.now += us / 1000U ? us / 1000U : 1U;
    return 0;
}
struct kshim_resources *kshim_resources_get(void) { return &h.resources; }
kshim_fb_status_t kshim_fb_init(kshim_framebuffer_t *fb,
                              const kshim_framebuffer_config_t *config)
{
    ++h.fb_calls;
    if (h.serial) return KSHIM_FB_ERR_INVALID_ARGUMENT;
    memset(fb, 0, sizeof(*fb));
    fb->render_address = config->render_address;
    fb->width = config->width; fb->height = config->height;
    fb->stride = config->stride; fb->format = config->format; fb->bpp = config->bpp;
    return KSHIM_FB_OK;
}
static int key_parse_output(QcomKeyDescriptor *descriptors, size_t capacity,
                            size_t *count)
{
    assert(descriptors && count && capacity == QCOM_KEYS_MAX);
    assert(h.keys_parse_count <= sizeof(key_descriptors) / sizeof(key_descriptors[0]));
    ++h.keys_build_calls;
    for (size_t i = 0; i < h.keys_parse_count; ++i)
        descriptors[i] = key_descriptors[i];
    *count = h.keys_parse_count;
    return h.keys_parse_result;
}
/* Keep the old path linkable so missing scope handling fails behaviorally. */
int QcomKeysBuildFromDeviceTree(const void *fdt, QcomKeyDescriptor *descriptors,
                               size_t capacity, size_t *count)
{
    assert(fdt == (const void *)(uintptr_t)boot_fdt());
    return key_parse_output(descriptors, capacity, count);
}
int QcomKeyFdtAdapterInit(QcomKeyFdtAdapter *adapter, const void *fdt)
{
    assert(adapter && fdt == (const void *)(uintptr_t)boot_fdt());
    assert(!h.keys_scope_active);
    ++h.keys_adapter_calls;
    if (h.keys_adapter_result != QCOM_KEYS_OK)
        return h.keys_adapter_result;
    *adapter = (QcomKeyFdtAdapter){.Blob = fdt, .Ops = {.Context = adapter}};
    h.keys_adapter = adapter;
    return QCOM_KEYS_OK;
}
void dt_parent_index_begin(const void *fdt)
{
    assert(!h.keys_scope_active && h.keys_adapter_result == QCOM_KEYS_OK);
    assert(h.keys_adapter_calls == h.keys_begin_calls + 1);
    assert(h.keys_adapter && h.keys_adapter->Blob == fdt);
    assert(fdt == (const void *)(uintptr_t)boot_fdt());
    ++h.keys_begin_calls;
    h.keys_scope_active = true;
}
int QcomKeysBuildFromFdt(const QcomKeyFdtOps *ops, QcomKeyDescriptor *descriptors,
                        size_t capacity, size_t *count)
{
    assert(h.keys_scope_active && h.keys_adapter);
    assert(ops == &h.keys_adapter->Ops && ops->Context == h.keys_adapter);
    return key_parse_output(descriptors, capacity, count);
}
void dt_parent_index_end(void)
{
    assert(h.keys_scope_active);
    assert(h.keys_build_calls == h.keys_begin_calls);
    ++h.keys_end_calls;
    h.keys_scope_active = false;
}
int QcomKeysInitEx(QcomKeys *keys, const QcomKeyDescriptor *descriptors,
                  size_t count, QcomKeyRead32 read32,
                  QcomKeyRead32Status read_status, void *context)
{
    assert(!h.keys_scope_active && h.keys_begin_calls == h.keys_end_calls);
    assert(keys == &Keys && descriptors && count == h.keys_parse_count && count > 0);
    assert(read32 == kshim_qcom_key_read32);
    assert(read_status == kshim_qcom_key_read32_status && context == NULL);
    for (size_t i = 0; i < count; ++i) {
        const QcomKeyDescriptor *actual = &descriptors[i], *expected = &key_descriptors[i];
        assert(actual->Base == expected->Base);
        assert(actual->RegisterOffset == expected->RegisterOffset);
        assert(actual->Mask == expected->Mask && actual->Code == expected->Code);
        assert(actual->Action == expected->Action && actual->Source == expected->Source);
        assert(actual->ActiveLow == expected->ActiveLow);
        assert(actual->DebounceSamples == expected->DebounceSamples);
        assert(actual->Reserved[0] == expected->Reserved[0]);
        assert(actual->Reserved[1] == expected->Reserved[1]);
    }
    ++h.keys_init_calls;
    memset(keys, 0, sizeof(*keys));
    return h.keys_init_result;
}
void QcomKeysSetDebounce(QcomKeys *keys, uint8_t samples)
{
    assert(keys == &Keys && samples == 2 && !h.keys_scope_active);
    assert(h.keys_init_calls == h.keys_debounce_calls + 1);
    ++h.keys_debounce_calls;
}
uint32_t kshim_qcom_key_read32(void *context, uint64_t address)
{ (void)context; (void)address; assert(!"Unexpected hardware read"); return 0; }
int kshim_qcom_key_read32_status(void *context, uint64_t address, uint32_t *value)
{ (void)context; (void)address; (void)value; assert(!"Unexpected hardware read"); return -1; }
int QcomKeysPoll(QcomKeys *keys, QcomKeyEvent *event)
{
    (void)keys; budget(); ++h.key_poll_calls;
    /* Poison even NO_EVENT: the caller must check the real return code. */
    *event = (QcomKeyEvent){.Type = QCOM_KEY_EVENT_PRESS,
        .Action = QCOM_KEY_ACTION_SELECT, .Pressed = 1};
    if (h.key_next < h.key_count && h.keys[h.key_next].at <= h.now) {
        key_event_t e = h.keys[h.key_next++];
        *event = e.event;
        return e.result;
    }
    return QCOM_KEYS_NO_EVENT;
}
int kshim_console_try_read(char *character)
{
    budget(); ++h.console_read_calls;
    if (h.char_next < h.char_count && h.chars[h.char_next].at <= h.now) {
        *character = h.chars[h.char_next++].value;
        return 1;
    }
    return 0;
}
void kshim_console_write(const char *buffer, uint32_t length)
{ (void)buffer; (void)length; }
int kshim_mmu_is_enabled(void) { return 1; }
uint64_t kshim_mmu_root_table(void) { return 1; }
int kshim_mmu_configure_default(const void *fdt) { (void)fdt; return 0; }
int kshim_mmu_enable_current_cpu(void) { return 0; }
/* platform_select() re-maps the scanout and UI scratch non-cacheable once the
 * panel is known; the host build has no page tables, so the call is accepted. */
int kshim_mmu_add_region(uint64_t base, uint64_t size,
                         kshim_mmu_memory_type_t type, uint32_t permissions)
{ (void)base; (void)size; (void)type; (void)permissions; return 0; }
int kshim_mmu_prepare_handoff(void) { ++h.handoff_calls; return h.handoff_result; }
void kshim_boot_set_args(uint64_t fdt, uint64_t a, uint64_t b, uint64_t c)
{ (void)fdt; (void)a; (void)b; (void)c; }
void kshim_lvgl_set_scratch(void *base, size_t size) { (void)base; (void)size; }
int kshim_lvgl_init(kshim_lvgl_t *context, kshim_framebuffer_t *fb, QcomKeys *keys)
{
    (void)fb; (void)keys;
    ++h.init_calls;
    assert(!h.ui_alive);
    memset(context, 0, sizeof(*context));
    h.interacted = false;
    if (h.ui_init_result) return h.ui_init_result;
    h.ui_alive = true;
    return 0;
}
int kshim_lvgl_has_cjk_font(const kshim_lvgl_t *context) { (void)context; return 1; }
static void expect_description(const char *actual, const char *expected)
{
    if (expected) assert(actual && strcmp(actual, expected) == 0);
    else assert(actual == NULL);
}
static void expect_action_label(size_t row, const char *expected)
{
    assert(row < h.menu_count && h.entry_action_labels[row]);
    assert(strcmp(h.entry_action_labels[row], expected) == 0);
}
#if CONFIG_KSHIM_MENU_LANGUAGE_SWITCH
/* The language page always offers 简体中文 / English / Back, regardless of the
 * active locale, so the fixture recognises it by its two fixed leading rows. */
#define LANG_STR_CHINESE "简体中文"
#define LANG_STR_ENGLISH "English"
/* Runtime English labels differ from the compile-time (Chinese) macros. */
#define LANG_EN_ADVANCED "Advanced boot modes"
#define LANG_EN_ENTER    "Enter"
#define LANG_EN_RESTART  "Restart"
#define LANG_EN_OFF      "Power off"

static size_t lang_home_payloads(void)
{
    size_t rows = 0;
    for (size_t i = 0; i < h.payload_count; ++i)
        if (!(h.advanced_mask & (UINT64_C(1) << i))) ++rows;
    return rows;
}
static size_t lang_expected_home_rows(void)
{
    size_t rows = lang_home_payloads();
    if (h.advanced) ++rows; /* "Advanced boot modes" row */
    ++rows;                 /* language row */
#if CONFIG_KSHIM_MENU_POWER_ACTIONS
    rows += 2;              /* restart + power off */
#endif
    return rows;
}
static int lang_page_kind(const char *const *names, size_t count)
{
    if (count == 3 && strcmp(names[0], LANG_STR_CHINESE) == 0 &&
        strcmp(names[1], LANG_STR_ENGLISH) == 0)
        return 2;
    if (strcmp(names[count - 1], KSHIM_STR_BACK) == 0)
        return 1;
    return 0;
}
static void lang_capture(const char *const *names, const char *const *descriptions,
                         const char *const *action_labels, size_t count, size_t initial)
{
    assert(h.snap_count < LANG_PAGES);
    assert(count <= LANG_ROW_CAP);
    lang_page_t *page = &h.snap[h.snap_count++];
    memset(page, 0, sizeof(*page));
    page->kind = lang_page_kind(names, count);
    page->count = count;
    page->initial = initial;
    for (size_t i = 0; i < count; ++i) {
        assert(names[i] && names[i][0]);
        snprintf(page->names[i], sizeof(page->names[i]), "%s", names[i]);
        page->descriptions[i] = descriptions ? descriptions[i] : NULL;
        page->actions[i] = action_labels ? action_labels[i] : NULL;
    }
    if (page->kind == 0) {
        /* The language switch adds one home-menu row. */
        assert(count == lang_expected_home_rows());
    } else if (page->kind == 2) {
        assert(count == 3);
        assert(strcmp(page->names[2], KSHIM_STR_BACK) == 0);
    }
}
#endif
static void info_capture(const char *const *names, const char *const *descriptions,
                         const char *const *action_labels, size_t count, size_t initial)
{
    assert(h.info_snap_count < INFO_SNAPS);
    assert(count <= INFO_SNAP_ROWS);
    info_snap_t *snap = &h.info_snaps[h.info_snap_count++];
    memset(snap, 0, sizeof(*snap));
    snap->count = count;
    snap->initial = initial;
    for (size_t i = 0; i < count; ++i) {
        assert(names[i] && names[i][0]);
        snprintf(snap->names[i], sizeof(snap->names[i]), "%s", names[i]);
        if (descriptions && descriptions[i])
            snprintf(snap->descriptions[i], sizeof(snap->descriptions[i]), "%s",
                     descriptions[i]);
        else
            snap->desc_null[i] = true;
        if (action_labels && action_labels[i])
            snprintf(snap->actions[i], sizeof(snap->actions[i]), "%s",
                     action_labels[i]);
        else
            snap->action_null[i] = true;
    }
}
static const char *snap_desc(const info_snap_t *snap, size_t row)
{
    return snap->desc_null[row] ? NULL : snap->descriptions[row];
}
static const char *snap_action(const info_snap_t *snap, size_t row)
{
    return snap->action_null[row] ? NULL : snap->actions[row];
}
static size_t snap_row(const info_snap_t *snap, const char *name)
{
    for (size_t i = 0; i < snap->count; ++i)
        if (strcmp(snap->names[i], name) == 0) return i;
    assert(!"menu row not found");
    return 0;
}
/* Activation keeps the whole home page and only replaces the info row's
 * description with one localized multi-line summary of "Label: value" fields
 * joined by newlines.  These narrow locators read that aggregate without
 * mirroring the product formatter. */
static void summary_expect(const char *summary, const char *label,
                           const char *value)
{
    char needle[INFO_TEXT_LEN];
    int n = snprintf(needle, sizeof(needle), "%s: %s", label, value);
    assert(n > 0 && (size_t)n < sizeof(needle));
    assert(summary && strstr(summary, needle) != NULL);
}
/* Advance past the next "Label: value" starting at Cursor so the caller can
 * prove the fields appear in the documented order. */
static const char *summary_after(const char *cursor, const char *label,
                                 const char *value)
{
    char needle[INFO_TEXT_LEN];
    int n = snprintf(needle, sizeof(needle), "%s: %s", label, value);
    assert(n > 0 && (size_t)n < sizeof(needle));
    const char *hit = cursor ? strstr(cursor, needle) : NULL;
    assert(hit);
    return hit + n;
}
/* Locate a field value that never wraps (the model) after "Label: " and report
 * its length up to the next field newline. */
static const char *summary_field(const char *summary, const char *label,
                                 size_t *length)
{
    char needle[INFO_TEXT_LEN];
    int n = snprintf(needle, sizeof(needle), "%s: ", label);
    assert(n > 0 && (size_t)n < sizeof(needle));
    const char *hit = summary ? strstr(summary, needle) : NULL;
    assert(hit);
    const char *value = hit + n;
    const char *end = strchr(value, '\n');
    *length = end ? (size_t)(end - value) : strlen(value);
    return value;
}
int kshim_lvgl_set_entries_described(kshim_lvgl_t *context,
                                    const char *const *names,
                                    const char *const *descriptions,
                                    const char *const *action_labels,
                                    size_t count, size_t initial)
{
    (void)context;
    ++h.entries_calls;
    assert(count <= KSHIM_MENU_MAX_ENTRIES && initial < count);
    if (h.info_mode) {
        info_capture(names, descriptions, action_labels, count, initial);
        /* Keep the same fake contract as the normal path below. */
        h.menu_count = count; h.initial = initial; h.interacted = false;
        return h.entries_fail_at == h.entries_calls ? -1 : h.entries_result;
    }
#if CONFIG_KSHIM_MENU_LANGUAGE_SWITCH
    lang_capture(names, descriptions, action_labels, count, initial);
#endif
    if (!h.advanced) {
        assert(initial < h.payload_count);
        assert(count == expected_count());
    }
    h.menu_count = count; h.initial = initial; h.interacted = false;
    for (size_t i = 0; i < count; ++i) {
        assert(names[i] && names[i][0]);
        snprintf(h.labels[i], sizeof(h.labels[i]), "%s", names[i]);
        h.entry_descriptions[i] = descriptions ? descriptions[i] : NULL;
        h.entry_action_labels[i] = action_labels ? action_labels[i] : NULL;
        if (!h.advanced && i < h.payload_count) {
            assert(strcmp(names[i], h.names[i]) == 0);
            expect_description(h.entry_descriptions[i], h.descriptions[i]);
            expect_action_label(i, KSHIM_MENU_ACTION_BOOT);
        }
    }
#if CONFIG_KSHIM_MENU_ADVANCED_ACTIONS
    if (h.advanced) {
#if !CONFIG_KSHIM_MENU_LANGUAGE_SWITCH
        h.advanced_page = strcmp(names[count - 1], KSHIM_STR_BACK) == 0;
        size_t row = 0;
        for (size_t i = 0; i < h.payload_count; ++i) {
            bool advanced = (h.advanced_mask & (UINT64_C(1) << i)) != 0;
            if (advanced == h.advanced_page) {
                assert(row < count && strcmp(names[row], h.names[i]) == 0);
                expect_description(h.entry_descriptions[row], h.descriptions[i]);
                expect_action_label(row, KSHIM_MENU_ACTION_BOOT);
                ++row;
            }
        }
        if (h.advanced_page) {
            assert(count == row + 4);
            expect_description(h.entry_descriptions[row], KSHIM_MENU_DESC_RECOVERY);
            expect_action_label(row, KSHIM_MENU_ACTION_ENTER);
            assert(strcmp(names[row++], KSHIM_STR_RECOVERY) == 0);
            expect_description(h.entry_descriptions[row], KSHIM_MENU_DESC_FASTBOOT);
            expect_action_label(row, KSHIM_MENU_ACTION_ENTER);
            assert(strcmp(names[row++], KSHIM_STR_FASTBOOT) == 0);
            expect_description(h.entry_descriptions[row], KSHIM_MENU_DESC_EDL);
            expect_action_label(row, KSHIM_MENU_ACTION_ENTER);
            assert(strcmp(names[row++], KSHIM_STR_EDL) == 0);
            expect_description(h.entry_descriptions[row], KSHIM_MENU_DESC_BACK);
            expect_action_label(row, KSHIM_MENU_ACTION_BACK);
            assert(strcmp(names[row++], KSHIM_STR_BACK) == 0);
        } else {
            expect_description(h.entry_descriptions[row], KSHIM_MENU_DESC_ADVANCED);
            expect_action_label(row, KSHIM_MENU_ACTION_ENTER);
            assert(strcmp(names[row++], KSHIM_STR_ADVANCED) == 0);
#if CONFIG_KSHIM_MENU_POWER_ACTIONS
            expect_description(h.entry_descriptions[row], KSHIM_MENU_DESC_RESTART);
            expect_action_label(row, KSHIM_MENU_ACTION_RESTART);
            assert(strcmp(names[row++], KSHIM_STR_RESTART) == 0);
            expect_description(h.entry_descriptions[row], KSHIM_MENU_DESC_OFF);
            expect_action_label(row, KSHIM_MENU_ACTION_OFF);
            assert(strcmp(names[row++], KSHIM_STR_POWER_OFF) == 0);
#endif
            assert(count == row);
        }
#endif /* !CONFIG_KSHIM_MENU_LANGUAGE_SWITCH */
        assert(h.page_count < MAX_PAGES);
        h.pages[h.page_count++] = (page_event_t){count, initial, h.advanced_page};
    }
#endif
#if CONFIG_KSHIM_MENU_POWER_ACTIONS
    if (!h.advanced) {
        assert(strcmp(h.labels[h.payload_count], KSHIM_STR_RESTART) == 0);
        assert(strcmp(h.labels[h.payload_count + 1], KSHIM_STR_POWER_OFF) == 0);
        expect_description(h.entry_descriptions[h.payload_count], KSHIM_MENU_DESC_RESTART);
        expect_description(h.entry_descriptions[h.payload_count + 1], KSHIM_MENU_DESC_OFF);
        expect_action_label(h.payload_count, KSHIM_MENU_ACTION_RESTART);
        expect_action_label(h.payload_count + 1, KSHIM_MENU_ACTION_OFF);
    }
#endif
    return h.entries_fail_at == h.entries_calls ? -1 : h.entries_result;
}
int kshim_lvgl_set_entries(kshim_lvgl_t *context, const char *const *names,
                          size_t count, size_t initial)
{
    return kshim_lvgl_set_entries_described(context, names, NULL, NULL, count, initial);
}
void kshim_lvgl_set_entry_roles(kshim_lvgl_t *context,
                                const kshim_menu_role_t *roles, size_t count)
{
    (void)context;
    ++h.role_calls;
    assert(count <= KSHIM_MENU_MAX_ENTRIES);
    h.role_count = count;
    for (size_t i = 0; i < count; ++i)
        h.roles[i] = roles ? roles[i] : KSHIM_MENU_ROLE_UNKNOWN;
    if (h.info_mode && h.info_snap_count) {
        info_snap_t *snap = &h.info_snaps[h.info_snap_count - 1];
        assert(count == snap->count);
        for (size_t i = 0; i < count; ++i)
            snap->roles[i] = roles ? roles[i] : KSHIM_MENU_ROLE_UNKNOWN;
    }
}
void kshim_lvgl_set_countdown(kshim_lvgl_t *context, uint32_t timeout)
{
    (void)context;
    if (timeout) { h.countdown_first = timeout; ++h.countdown_started; }
    else ++h.countdown_zero;
}
void kshim_lvgl_set_status(kshim_lvgl_t *context, const char *text)
{
    (void)context; assert(text);
    ++h.status_calls;
    snprintf(h.status, sizeof(h.status), "%s", text);
}
void kshim_lvgl_frame(kshim_lvgl_t *context, uint32_t elapsed)
{
    (void)context; (void)elapsed; budget(); ++h.frames;
    assert(h.ui_alive);
    if (h.ui_next < h.ui_count && h.ui[h.ui_next].at <= h.now) {
        ui_event_t e = h.ui[h.ui_next++];
        if (e.index >= 0) h.pending = e.index;
        if (e.interacted) h.interacted = true;
    }
}
int kshim_lvgl_take_index(kshim_lvgl_t *context)
{ (void)context; int index = h.pending; h.pending = -1; return index; }
int kshim_lvgl_has_interacted(const kshim_lvgl_t *context)
{ (void)context; return h.interacted; }
void kshim_lvgl_deinit(kshim_lvgl_t *context)
{ (void)context; ++h.deinit_calls; h.ui_alive = false; }
int kshim_psci_init(const void *fdt)
{ ++h.psci_calls; h.psci_fdt = fdt; return h.psci_result; }
static void check_power_request(void)
{
    assert(CONFIG_KSHIM_MENU_POWER_ACTIONS);
    assert(h.psci_calls == 1 && h.psci_result == 0);
    assert(h.psci_fdt == (const void *)(uintptr_t)boot_fdt());
    assert(h.handoff_calls == (h.serial ? 1U : 0U));
    assert(h.deinit_calls == (h.serial ? 1U : 0U));
    assert(h.ui_alive == !h.serial);
}
static jmp_buf power_exit;
static int capture_power_off;
int64_t kshim_psci_request_system_reset(void)
{ check_power_request(); ++h.reset_calls; return h.reset_result; }
int64_t kshim_psci_request_system_off(void)
{ check_power_request(); ++h.off_calls; if(capture_power_off)longjmp(power_exit,1);return h.off_result; }

#if CONFIG_KSHIM_MENU_ADVANCED_ACTIONS
int kshim_qcom_reboot_request(const void *fdt, enum kshim_qcom_reboot_target target)
{
    assert(h.advanced);
    assert(fdt == (const void *)(uintptr_t)boot_fdt());
    assert(h.handoff_calls == (h.serial ? 1U : 0U));
    assert(h.deinit_calls == (h.serial ? 1U : 0U));
    assert(h.ui_alive == !h.serial);
    if (!h.serial) assert(h.advanced_page);
    ++h.reboot_calls;
    h.reboot_target = target;
    h.reboot_fdt = fdt;
    return h.reboot_result;
}

#if !CONFIG_KSHIM_MENU_LANGUAGE_SWITCH
static size_t choose_advanced(size_t initial, uint32_t timeout, uint64_t mask)
{
    size_t selected = SIZE_MAX;
    h.advanced = true;
    h.advanced_mask = mask;
    assert(kshim_platform_select_advanced(boot_fdt(), h.names, h.payload_count,
                                         initial, timeout, mask, &selected) == 0);
    assert(selected < h.payload_count);
    expect_psci_init();
    assert(h.deinit_calls == 1U && h.handoff_calls == 1U);
    if (!h.serial && h.ui_init_result == 0 && h.entries_result == 0) {
        assert(h.init_calls == 1U && h.entries_calls == h.page_count);
        assert(h.page_count >= 1U);
    }
    return selected;
}
#endif
#endif

/* The whole legacy suite depends on the unconditional choose helpers above, so
 * the language target swaps it out for the locale tests in the #else branch. */
#if !CONFIG_KSHIM_MENU_LANGUAGE_SWITCH
static size_t choose_described(const char *const *descriptions, size_t initial,
                              uint32_t timeout, uint64_t mask)
{
    size_t selected = SIZE_MAX;
#if CONFIG_KSHIM_MENU_ADVANCED_ACTIONS
    h.advanced = true;
    h.advanced_mask = mask;
#endif
    assert(kshim_platform_select_described(boot_fdt(), h.names, descriptions,
            h.payload_count, initial, timeout, mask, &selected) == 0);
    assert(selected < h.payload_count);
    expect_psci_init();
    assert(h.deinit_calls == 1U && h.handoff_calls == 1U);
    if (!h.serial) {
        assert(h.init_calls == 1U);
        assert(h.entries_calls == (h.advanced ? h.page_count : 1U));
        if (!h.advanced) assert(h.menu_count == expected_count());
    }
    return selected;
}

static void test_build_keys_scope(void)
{
    const void *fdt = (const void *)(uintptr_t)boot_fdt();

    setup(false, 1);
    assert(build_keys(NULL) == -1);
    assert(h.keys_adapter_calls == 0 && h.keys_build_calls == 0);
    assert(h.keys_begin_calls == 0 && h.keys_end_calls == 0 && !h.keys_scope_active);
    assert(h.keys_init_calls == 0 && h.keys_debounce_calls == 0);

    /* The old wrapper still links, but cannot satisfy this scope contract. */
    setup(false, 1);
    h.keys_parse_count = 3;
    assert(build_keys(fdt) == QCOM_KEYS_OK);
    assert(h.keys_adapter_calls == 1 && h.keys_build_calls == 1);
    assert(h.keys_begin_calls == 1 && h.keys_end_calls == 1 && !h.keys_scope_active);
    assert(h.keys_init_calls == 1 && h.keys_debounce_calls == 1);

    /* Validation must finish before begin; failure never opens a scope. */
    setup(false, 1);
    h.keys_adapter_result = QCOM_KEYS_EIO;
    assert(build_keys(fdt) == -1);
    assert(h.keys_adapter_calls == 1 && h.keys_build_calls == 0);
    assert(h.keys_begin_calls == 0 && h.keys_end_calls == 0 && !h.keys_scope_active);
    assert(h.keys_init_calls == 0 && h.keys_debounce_calls == 0);

    /* Partial output does not permit InitEx when descriptor construction fails. */
    setup(false, 1);
    h.keys_parse_result = QCOM_KEYS_ENOSPC;
    h.keys_parse_count = 2;
    assert(build_keys(fdt) == -1);
    assert(h.keys_adapter_calls == 1 && h.keys_build_calls == 1);
    assert(h.keys_begin_calls == 1 && h.keys_end_calls == 1 && !h.keys_scope_active);
    assert(h.keys_init_calls == 0 && h.keys_debounce_calls == 0);

    /* Real zero-key parsing returns EIO; also cover the explicit !count guard. */
    static const int zero_results[] = {QCOM_KEYS_EIO, QCOM_KEYS_OK};
    for (size_t i = 0; i < sizeof(zero_results) / sizeof(zero_results[0]); ++i) {
        setup(false, 1);
        h.keys_parse_result = zero_results[i];
        h.keys_parse_count = 0;
        assert(build_keys(fdt) == -1);
        assert(h.keys_adapter_calls == 1 && h.keys_build_calls == 1);
        assert(h.keys_begin_calls == 1 && h.keys_end_calls == 1 && !h.keys_scope_active);
        assert(h.keys_init_calls == 0 && h.keys_debounce_calls == 0);
    }

    /* InitEx runs after end and keeps its status plus the original debounce call. */
    setup(false, 1);
    h.keys_init_result = QCOM_KEYS_ENOSPC;
    assert(build_keys(fdt) == QCOM_KEYS_ENOSPC);
    assert(h.keys_adapter_calls == 1 && h.keys_build_calls == 1);
    assert(h.keys_begin_calls == 1 && h.keys_end_calls == 1 && !h.keys_scope_active);
    assert(h.keys_init_calls == 1 && h.keys_debounce_calls == 1);
}

static void test_described_payload_mapping(void)
{
    const uint64_t mask = (UINT64_C(1) << 1) | (UINT64_C(1) << 3);
    for (unsigned serial = 0; serial < 2; ++serial) {
        for (unsigned described = 0; described < 2; ++described) {
            setup(serial != 0, 4);
            /* Metadata follows original indexes, not label text or page rows. */
            h.names[0] = h.payload_labels[2];
            h.names[1] = h.payload_labels[0];
            h.names[2] = h.payload_labels[3];
            h.names[3] = h.payload_labels[1];
            if (described) {
                h.descriptions[0] = "Third payload metadata";
                h.descriptions[1] = NULL;
                h.descriptions[2] = "";
                h.descriptions[3] = "Second payload metadata";
            }
#if CONFIG_KSHIM_MENU_ADVANCED_ACTIONS
            if (serial) {
                text_at(5, "3\r"); text_at(30, "6\r");
                text_at(9000, "3\r"); text_at(9030, "2\r");
            } else {
                ui_at(5, 2, 0); ui_at(10, 5, 0);
                ui_at(9000, 2, 0); ui_at(9005, 1, 0);
            }
#else
            /* Disabled grouping ignores the mask but retains all metadata. */
            if (serial) text_at(5, "4\r");
            else ui_at(5, 3, 0);
#endif
            assert(choose_described(described ? h.descriptions : NULL,
                                    0, 8000, mask) == 3);
            assert(h.reboot_calls == 0 && h.reset_calls == 0 && h.off_calls == 0);
#if CONFIG_KSHIM_MENU_ADVANCED_ACTIONS
            assert(h.now >= 9005);
            if (!serial) {
                assert(h.page_count == 4 && h.pages[2].initial == 2);
                assert(h.countdown_started == 1 && h.countdown_zero >= 1);
                expect_description(h.entry_descriptions[0], NULL);
                expect_description(h.entry_descriptions[1], h.descriptions[3]);
            }
#else
            if (!serial) expect_description(h.entry_descriptions[3], h.descriptions[3]);
#endif
        }
    }
}

static void test_reject_count_50(void)
{
    for (unsigned serial = 0; serial < 2; ++serial) {
        setup(serial != 0, 50);
        struct { size_t before, value, after; } out = {11, 12345, 22};
        assert(kshim_platform_select(boot_fdt(), h.names, 50, 0, 20, &out.value) < 0);
        assert(out.before == 11 && out.value == 12345 && out.after == 22);
        assert(h.psci_calls == 0 && h.fb_calls == 0 && h.init_calls == 0);
        assert(h.steps == 0 && h.handoff_calls == 0 && h.deinit_calls == 0);
    }
}
static void test_ui_boundary_and_payload(void)
{
    setup(false, 49);
    ui_at(5, 48, 1);
    assert(choose(0, 20) == 48);
    assert(h.menu_count == (CONFIG_KSHIM_MENU_POWER_ACTIONS ? 51U : 49U));
    assert(h.reset_calls == 0 && h.off_calls == 0);
}
static void test_ui_timeout_and_interaction(void)
{
    setup(false, 2);
    assert(choose(1, 20) == 1); /* initial focus is not interaction */
    assert(h.now == 20 && h.countdown_zero == 0);
    setup(false, 2);
    ui_at(5, -1, 1);
    ui_at(60, 1, 1);
    assert(choose(0, 20) == 1);
    assert(h.now >= 60 && h.countdown_zero == 1 && h.countdown_first == 20);
    assert(h.status_calls && h.status[0]);
    setup(false, 2);
    ui_at(5, 100, 0);
    ui_at(10, 1, 0);
    assert(choose(0, 20) == 1);
    assert(h.ui_next == 2 && h.now == 10);
}
static void test_ui_init_failure_falls_back(void)
{
    setup(false, 2);
    h.ui_init_result = -1;
    text_at(5, "2\r");
    assert(choose(0, 20) == 1);
    assert(h.init_calls == 1 && h.entries_calls == 0);
    setup(false, 2);
    h.entries_result = -1;
    text_at(5, "2\r");
    assert(choose(0, 20) == 1);
    assert(h.init_calls == 1 && h.entries_calls == 1);
}
static void test_serial_timeout_and_digits(void)
{
    setup(true, 2);
    assert(choose(1, 20) == 1 && h.now == 20);
    setup(true, 2);
    char_at(5, '2'); char_at(60, '\r');
    assert(choose(0, 20) == 1 && h.now >= 60);
    setup(true, 10);
    char_at(5, '1'); char_at(10, '0'); char_at(60, '\n');
    assert(choose(0, 20) == 9 && h.now >= 60 && h.char_next == 3);
    setup(true, 2);
    text_at(5, "0x9"); /* none is a valid prefix for this menu (2 or 4 entries) */
    assert(choose(1, 20) == 1 && h.now == 20);
    setup(true, 2);
    char_at(5, 27);
    assert(choose(1, 20) == 1 && h.now == 5);
}
static void test_serial_key_contract(void)
{
    setup(true, 2);
    key_at(5, QCOM_KEYS_EVENT, QCOM_KEY_EVENT_PRESS, QCOM_KEY_ACTION_DOWN, 1);
    key_at(60, QCOM_KEYS_EVENT, QCOM_KEY_EVENT_PRESS, QCOM_KEY_ACTION_SELECT, 1);
    assert(choose(0, 20) == 1 && h.now >= 60);
    setup(true, 2);
    key_at(5, QCOM_KEYS_EVENT, QCOM_KEY_EVENT_PRESS, QCOM_KEY_ACTION_UP, 1);
    key_at(60, QCOM_KEYS_EVENT, QCOM_KEY_EVENT_PRESS, QCOM_KEY_ACTION_SELECT, 1);
    assert(choose(1, 20) == 0 && h.now >= 60);
    setup(true, 2);
    key_at(0, QCOM_KEYS_NO_EVENT, QCOM_KEY_EVENT_PRESS, QCOM_KEY_ACTION_DOWN, 1);
    key_at(5, QCOM_KEYS_EVENT, QCOM_KEY_EVENT_RELEASE, QCOM_KEY_ACTION_DOWN, 1);
    key_at(10, QCOM_KEYS_EVENT, QCOM_KEY_EVENT_PRESS, (QcomKeyAction)99, 1);
    key_at(15, QCOM_KEYS_EVENT, QCOM_KEY_EVENT_PRESS, QCOM_KEY_ACTION_DOWN, 0);
    assert(choose(1, 20) == 1 && h.now == 20 && h.key_next == 4);
}
static void test_serial_boundary(void)
{
    setup(true, 49);
#if CONFIG_KSHIM_MENU_POWER_ACTIONS
    text_at(0, "50\r");
    text_at(30, "51\r");
    text_at(70, "49\r");
    assert(choose(0, 20) == 48 && h.now >= 70);
    assert(h.reset_calls == 1 && h.off_calls == 1);
#else
    text_at(0, "49"); char_at(60, '\r');
    assert(choose(0, 20) == 48 && h.now >= 60);
    assert(h.reset_calls == 0 && h.off_calls == 0);
#endif
}
#if CONFIG_KSHIM_MENU_POWER_ACTIONS
static void test_power_actions(void)
{
    static const int64_t results[] = {-1, 0, 1};
    for (unsigned serial = 0; serial < 2; ++serial) {
        for (unsigned action = 0; action < 2; ++action) {
            for (unsigned result = 0; result < 3; ++result) {
                setup(serial != 0, 2);
                h.reset_result = h.off_result = results[result];
                if (serial) {
                    char_at(5, action ? '4' : '3'); char_at(10, '\r');
                    text_at(60, "2\r");
                } else {
                    ui_at(5, action ? 3 : 2, 0);
                    ui_at(35, -1, 0); /* idle after the old deadline */
                    ui_at(60, 1, 0);
                }
                assert(choose(0, 20) == 1 && h.now >= 60);
                assert(h.reset_calls == (action ? 0U : 1U));
                assert(h.off_calls == (action ? 1U : 0U));
                if (!serial) {
                    assert(h.countdown_zero == 1 && h.ui_next == 3);
                    assert(h.status_calls && h.status[0]);
                }
            }
            setup(serial != 0, 2);
            h.psci_result = -1;
            if (serial) {
                char_at(5, action ? '4' : '3'); char_at(10, '\r');
                text_at(60, "2\r");
            } else {
                ui_at(5, action ? 3 : 2, 0); ui_at(60, 1, 0);
            }
            assert(choose(0, 20) == 1 && h.now >= 60);
            assert(h.reset_calls == 0 && h.off_calls == 0);
            if (!serial) assert(h.countdown_zero == 1 && h.status_calls && h.status[0]);
        }
    }
}
static void test_serial_key_power_actions(void)
{
    setup(true, 2);
    key_at(5, QCOM_KEYS_EVENT, QCOM_KEY_EVENT_PRESS, QCOM_KEY_ACTION_DOWN, 1);
    key_at(10, QCOM_KEYS_EVENT, QCOM_KEY_EVENT_PRESS, QCOM_KEY_ACTION_SELECT, 1);
    key_at(35, QCOM_KEYS_EVENT, QCOM_KEY_EVENT_PRESS, QCOM_KEY_ACTION_DOWN, 1);
    key_at(40, QCOM_KEYS_EVENT, QCOM_KEY_EVENT_PRESS, QCOM_KEY_ACTION_SELECT, 1);
    key_at(65, QCOM_KEYS_EVENT, QCOM_KEY_EVENT_PRESS, QCOM_KEY_ACTION_UP, 1);
    key_at(70, QCOM_KEYS_EVENT, QCOM_KEY_EVENT_PRESS, QCOM_KEY_ACTION_UP, 1);
    key_at(75, QCOM_KEYS_EVENT, QCOM_KEY_EVENT_PRESS, QCOM_KEY_ACTION_SELECT, 1);
    assert(choose(1, 20) == 1 && h.now >= 75);
    assert(h.reset_calls == 1 && h.off_calls == 1 && h.key_next == 7);
}
static void test_serial_numeric_power_preserves_key_focus(void)
{
    for (unsigned action = 0; action < 2; ++action) {
        setup(true, 2);
        char_at(5, action ? '4' : '3'); char_at(10, '\r');
        key_at(60, QCOM_KEYS_EVENT, QCOM_KEY_EVENT_PRESS, QCOM_KEY_ACTION_SELECT, 1);
        assert(choose(1, 20) == 1 && h.now >= 60);
        assert(h.reset_calls == (action ? 0U : 1U));
        assert(h.off_calls == (action ? 1U : 0U));
        assert(h.key_next == 1);
    }
}
#endif
#if CONFIG_KSHIM_MENU_ADVANCED_ACTIONS
static void test_advanced_payload_mapping(void)
{
    for (unsigned serial = 0; serial < 2; ++serial) {
        setup(serial != 0, 4);
        if (serial) {
            text_at(5, "4\r");
            text_at(30, "1\r");
        } else {
            ui_at(5, 3, 0);
            ui_at(10, 0, 0);
        }
        assert(choose_advanced(1, 8000, UINT64_C(1) << 3) == 3);
        assert(h.reboot_calls == 0 && h.reset_calls == 0 && h.off_calls == 0);
        if (!serial) {
            assert(h.page_count == 2);
            assert(!h.pages[0].advanced && h.pages[0].count ==
                   (CONFIG_KSHIM_MENU_POWER_ACTIONS ? 6U : 4U));
            assert(h.pages[0].initial == 1);
            assert(h.pages[1].advanced && h.pages[1].count == 5);
            assert(h.pages[1].initial == 0);
        }

        /* Sparse payload groups must not return an index within the page. */
        setup(serial != 0, 4);
        if (serial) {
            text_at(5, "3\r");
            text_at(30, "2\r");
        } else {
            ui_at(5, 2, 0);
            ui_at(10, 1, 0);
        }
        assert(choose_advanced(0, 8000, (UINT64_C(1) << 1) |
                               (UINT64_C(1) << 3)) == 3);
    }
}

static void test_advanced_default_timeout(void)
{
    for (unsigned serial = 0; serial < 2; ++serial) {
        for (unsigned initial = 1; initial <= 3; initial += 2) {
            setup(serial != 0, 4);
            assert(choose_advanced(initial, 8000, UINT64_C(1) << 3) == initial);
            assert(h.now == 8000);
            assert(h.reboot_calls == 0 && h.reset_calls == 0 && h.off_calls == 0);
            if (!serial) {
                assert(h.page_count == 1 && h.countdown_zero == 0);
                assert(h.countdown_first == 8000 && h.countdown_started == 1);
                assert(h.pages[0].advanced == (initial == 3));
                assert(h.pages[0].initial == (initial == 3 ? 0U : 1U));
            }
        }
    }
}

static void test_advanced_back_cancels_timeout(void)
{
    setup(false, 4);
    ui_at(5, 3, 0);     /* Enter advanced without a separate interaction flag. */
    ui_at(10, 4, 0);    /* Back. */
    ui_at(9000, 3, 0);  /* Still waiting after the original eight-second deadline. */
    ui_at(9005, 0, 0);
    assert(choose_advanced(1, 8000, UINT64_C(1) << 3) == 3);
    assert(h.now >= 9005 && h.ui_next == 4 && h.page_count == 4);
    assert(!h.pages[2].advanced && h.pages[2].initial == 3);
    assert(h.countdown_zero >= 1 && h.countdown_started == 1);
    assert(h.reboot_calls == 0 && h.reset_calls == 0 && h.off_calls == 0);

    for (unsigned escape = 0; escape < 2; ++escape) {
        setup(true, 4);
        text_at(5, "4\r");
        if (escape) char_at(30, 27);
        else text_at(30, "5\r");
        /* SELECT must reopen advanced because Back restores that main focus. */
        key_at(9000, QCOM_KEYS_EVENT, QCOM_KEY_EVENT_PRESS,
               QCOM_KEY_ACTION_SELECT, 1);
        text_at(9020, "1\r");
        assert(choose_advanced(1, 8000, UINT64_C(1) << 3) == 3);
        assert(h.now >= 9020 && h.key_next == 1 && h.char_next == h.char_count);
        assert(h.reboot_calls == 0 && h.reset_calls == 0 && h.off_calls == 0);
    }

    /* Primary-page ESC keeps the legacy shortcut to the original default. */
    setup(true, 4);
    char_at(5, 27);
    assert(choose_advanced(1, 8000, UINT64_C(1) << 3) == 1 && h.now == 5);
}

static void test_advanced_reboot_failures(void)
{
    static const enum kshim_qcom_reboot_target targets[] = {
        KSHIM_QCOM_REBOOT_RECOVERY, KSHIM_QCOM_REBOOT_BOOTLOADER,
        KSHIM_QCOM_REBOOT_EDL,
    };
    static const int results[] = {
        KSHIM_QCOM_REBOOT_UNSUPPORTED, KSHIM_QCOM_REBOOT_INVALID_DT,
        KSHIM_QCOM_REBOOT_IO_FAILED, KSHIM_QCOM_REBOOT_VERIFY_FAILED,
        KSHIM_QCOM_REBOOT_RESET_RETURNED, KSHIM_QCOM_REBOOT_ROLLBACK_FAILED,
        0, 1, /* A successful hardware reset never returns. */
    };
    for (unsigned serial = 0; serial < 2; ++serial) {
        for (size_t target = 0; target < sizeof(targets) / sizeof(targets[0]); ++target) {
            for (size_t result = 0; result < sizeof(results) / sizeof(results[0]); ++result) {
                setup(serial != 0, 4);
                h.reboot_result = results[result];
                if (serial) {
                    text_at(5, "4\r");
                    char_at(30, (char)('2' + target)); char_at(35, '\r');
                    text_at(90, "1\r");
                } else {
                    ui_at(5, 3, 0);
                    ui_at(10, (int)target + 1, 0);
                    ui_at(90, 0, 0);
                }
                /* A failed mode switch must remain on advanced, past timeout. */
                assert(choose_advanced(1, 20, UINT64_C(1) << 3) == 3);
                assert(h.now >= 90 && h.reboot_calls == 1);
                assert(h.reboot_target == (int)targets[target]);
                assert(h.reboot_fdt == (const void *)(uintptr_t)boot_fdt());
                assert(h.reset_calls == 0 && h.off_calls == 0);
                if (!serial) {
                    assert(h.page_count == 2 && h.advanced_page);
                    assert(h.countdown_zero >= 1 && h.countdown_started == 1);
#if CONFIG_KSHIM_MENU_CHINESE
                    const char *expected = results[result] == KSHIM_QCOM_REBOOT_ROLLBACK_FAILED
                        ? "切换启动模式失败，恢复原设置失败" : "切换启动模式失败";
#else
                    const char *expected = results[result] == KSHIM_QCOM_REBOOT_ROLLBACK_FAILED
                        ? "Boot mode switch failed; restoring the original settings failed"
                        : "Boot mode switch failed";
#endif
                    assert(strcmp(h.status, expected) == 0);
                }
            }
        }
    }
}

#if CONFIG_KSHIM_MENU_POWER_ACTIONS
static void test_advanced_power_failures(void)
{
    static const int64_t results[] = {-1, 0, 1};
    for (unsigned serial = 0; serial < 2; ++serial) {
        for (unsigned action = 0; action < 2; ++action) {
            for (size_t result = 0; result < sizeof(results) / sizeof(results[0]); ++result) {
                setup(serial != 0, 4);
                h.reset_result = h.off_result = results[result];
                if (serial) {
                    char_at(5, action ? '6' : '5'); char_at(10, '\r');
                    text_at(60, "2\r");
                } else {
                    ui_at(5, action ? 5 : 4, 0);
                    ui_at(60, 1, 0);
                }
                assert(choose_advanced(1, 20, UINT64_C(1) << 3) == 1);
                assert(h.now >= 60 && h.reboot_calls == 0);
                assert(h.reset_calls == (action ? 0U : 1U));
                assert(h.off_calls == (action ? 1U : 0U));
                if (!serial) assert(h.page_count == 1 && !h.advanced_page);
            }
        }
    }
}
#endif

static void test_advanced_capacity(void)
{
    for (unsigned serial = 0; serial < 2; ++serial) {
        /* 48 ordinary payloads + advanced + two power actions fills 51 rows. */
        setup(serial != 0, 48);
        if (serial) text_at(5, "48\r");
        else ui_at(5, 47, 0);
        assert(choose_advanced(0, 20, 0) == 47);
        if (!serial) assert(h.pages[0].count ==
                            (CONFIG_KSHIM_MENU_POWER_ACTIONS ? 51U : 49U));

        /* 47 advanced payloads + three modes + Back also fills 51 rows. */
        setup(serial != 0, 49);
        if (serial) {
            text_at(5, "3\r"); text_at(30, "51\r");
            text_at(60, "3\r"); text_at(90, "47\r");
        } else {
            ui_at(5, 2, 0); ui_at(10, 50, 0);
            ui_at(15, 2, 0); ui_at(20, 46, 0);
        }
        assert(choose_advanced(48, 8000, (UINT64_C(1) << 47) - 1) == 46);
        if (!serial) {
            assert(h.page_count == 4 && h.pages[1].count == 51);
            assert(h.pages[2].initial == 2 && h.pages[3].count == 51);
        }
        assert(h.reboot_calls == 0 && h.reset_calls == 0 && h.off_calls == 0);

        static const struct { size_t count; uint64_t mask; } invalid[] = {
            {50, 0}, {49, (UINT64_C(1) << 48) - 1},
#if CONFIG_KSHIM_MENU_POWER_ACTIONS
            {49, 0},
#endif
        };
        for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
            setup(serial != 0, invalid[i].count);
            struct { size_t before, value, after; } out = {11, 12345, 22};
            assert(kshim_platform_select_advanced(boot_fdt(), h.names, h.payload_count,
                    0, 8000, invalid[i].mask, &out.value) == -1);
            assert(out.before == 11 && out.value == 12345 && out.after == 22);
            assert(h.psci_calls == 0 && h.fb_calls == 0 && h.init_calls == 0);
            assert(h.steps == 0 && h.handoff_calls == 0 && h.deinit_calls == 0);
            assert(h.reboot_calls == 0);
        }
    }
}

static void test_advanced_fallback_and_handoff(void)
{
    for (unsigned entries_fail = 0; entries_fail < 2; ++entries_fail) {
        setup(false, 4);
        if (entries_fail) h.entries_result = -1;
        else h.ui_init_result = -1;
        text_at(5, "4\r"); text_at(30, "1\r");
        assert(choose_advanced(1, 8000, UINT64_C(1) << 3) == 3);
        assert(h.init_calls == 1 && h.entries_calls == entries_fail);
    }
    /* A page rebuild failure must preserve that page and the cancelled timer. */
    setup(false, 4);
    h.entries_fail_at = 2;
    ui_at(5, 3, 0);
    text_at(9000, "1\r");
    assert(choose_advanced(1, 8000, UINT64_C(1) << 3) == 3);
    assert(h.now >= 9000 && h.entries_calls == 2 && h.page_count == 2);
    assert(h.pages[1].advanced && h.countdown_started == 1);
    assert(h.countdown_zero >= 1 && h.reboot_calls == 0);

    for (unsigned serial = 0; serial < 2; ++serial) {
        setup(serial != 0, 4);
        h.advanced = true;
        h.advanced_mask = UINT64_C(1) << 3;
        h.handoff_result = -1;
        ui_at(5, 1, 0);
        text_at(5, "2\r");
        size_t selected = 12345;
        assert(kshim_platform_select_advanced(boot_fdt(), h.names, 4, 1,
                8000, h.advanced_mask, &selected) == -2);
        assert(selected == (serial ? 12345U : 1U));
        assert(h.deinit_calls == 1 && h.handoff_calls == 1 && !h.ui_alive);
        assert(h.char_next == 0 && h.reboot_calls == 0);
    }
}
#endif

#else /* !CONFIG_KSHIM_MENU_LANGUAGE_SWITCH */

/* Home -> language -> English -> home again.  The reopened home page must be
 * rebuilt in English while the payload names survive unchanged. */
static void test_language_page_switch(void)
{
    setup(false, 4);
    h.advanced = true;
    h.advanced_mask = UINT64_C(1) << 3;
    ui_at(5, 4, 0);     /* Row four of the home menu is the language entry. */
    ui_at(10, 1, 0);    /* English is the second row of the language page. */
    ui_at(9000, 1, 0);  /* Reopened home page: choose the original payload. */
    size_t selected = SIZE_MAX;
    assert(kshim_platform_select_described(boot_fdt(), h.names, h.descriptions,
            4, 0, 8000, h.advanced_mask, &selected) == 0);
    assert(selected == 1);
    assert(h.reset_calls == 0 && h.off_calls == 0 && h.reboot_calls == 0);
    assert(h.now >= 9000);
    expect_psci_init();

    /* Each page is captured as it is (re)built: home, language, home. */
    assert(h.snap_count == 3);
    assert(h.snap[0].kind == 0 && h.snap[0].count == 7);
    assert(h.snap[1].kind == 2 && h.snap[1].count == 3);
    assert(strcmp(h.snap[1].names[0], LANG_STR_CHINESE) == 0);
    assert(strcmp(h.snap[1].names[1], LANG_STR_ENGLISH) == 0);
    assert(h.snap[2].kind == 0 && h.snap[2].count == 7);
    assert(h.snap[2].initial == 4); /* Focus returns to the language row. */

    /* The rebuilt home page now renders the selected locale. */
    assert(strcmp(h.snap[2].names[3], LANG_EN_ADVANCED) == 0);
    assert(h.snap[2].actions[3] && strcmp(h.snap[2].actions[3], LANG_EN_ENTER) == 0);
    assert(strcmp(h.snap[2].names[5], LANG_EN_RESTART) == 0);
    assert(strcmp(h.snap[2].names[6], LANG_EN_OFF) == 0);

    /* Payload names carry no locale, so they survive the switch unchanged. */
    assert(strcmp(h.snap[2].names[0], h.payload_labels[0]) == 0);
    assert(strcmp(h.snap[2].names[1], h.payload_labels[1]) == 0);
    assert(strcmp(h.snap[2].names[2], h.payload_labels[2]) == 0);
}

/* Back (UI row two / serial ESC) leaves the locale untouched. */
static void test_language_page_navigation(void)
{
    setup(false, 4);
    h.advanced = true;
    h.advanced_mask = UINT64_C(1) << 3;
    ui_at(5, 4, 0);
    ui_at(10, 2, 0);    /* Back row. */
    ui_at(9000, 1, 0);
    size_t selected = SIZE_MAX;
    assert(kshim_platform_select_described(boot_fdt(), h.names, h.descriptions,
            4, 0, 8000, h.advanced_mask, &selected) == 0);
    assert(selected == 1);
    assert(h.reset_calls == 0 && h.off_calls == 0 && h.reboot_calls == 0);
    assert(h.now >= 9000);
    assert(h.snap_count == 3);
    assert(h.snap[0].kind == 0 && h.snap[0].count == 7);
    assert(h.snap[1].kind == 2 && h.snap[1].count == 3);
    assert(h.snap[2].kind == 0 && h.snap[2].count == 7);

    setup(true, 4);
    h.advanced = true;
    h.advanced_mask = UINT64_C(1) << 3;
    text_at(5, "5\r");  /* Open the language entry from the serial menu. */
    char_at(30, 27);    /* ESC on the language page is the same Back. */
    text_at(60, "2\r");
    size_t serial = SIZE_MAX;
    assert(kshim_platform_select_described(boot_fdt(), h.names, h.descriptions,
            4, 0, 8000, h.advanced_mask, &serial) == 0);
    assert(serial == 1);
    assert(h.reset_calls == 0 && h.off_calls == 0 && h.reboot_calls == 0);
    assert(h.now >= 60);
}

/* Known roles resolve through the runtime table, while an UNKNOWN row keeps the
 * caller's exact text even when it matches a known localized label. */
static void test_language_localized_roles(void)
{
    static const kshim_menu_role_t roles[4] = {
        KSHIM_MENU_ROLE_UNKNOWN, KSHIM_MENU_ROLE_ANDROID,
        KSHIM_MENU_ROLE_INTERNAL, KSHIM_MENU_ROLE_TF,
    };
    setup(false, 4);
    h.advanced = true;
    h.advanced_mask = 0;
    for (size_t i = 0; i < 4; ++i) h.descriptions[i] = "caller description";
    /* The UNKNOWN row carries the localized Android label verbatim; the mapping
     * must come from the role, never from comparing displayed text. */
    h.names[0] = kshim_menu_role_name(KSHIM_MENU_ROLE_ANDROID, NULL);
    ui_at(5, 2, 0);
    size_t selected = SIZE_MAX;
    assert(kshim_platform_select_localized(boot_fdt(), h.names, h.descriptions, roles,
            4, 0, 20, 0, &selected) == 0);
    assert(selected == 2);
    assert(h.reset_calls == 0 && h.off_calls == 0 && h.reboot_calls == 0);
    expect_psci_init();
    /* 4 payloads + advanced + language + restart + power off. */
    assert(h.role_calls == 1 && h.role_count == 8);
    assert(h.roles[0] == KSHIM_MENU_ROLE_UNKNOWN);
    assert(h.roles[1] == KSHIM_MENU_ROLE_ANDROID);
    assert(h.roles[2] == KSHIM_MENU_ROLE_INTERNAL);
    assert(h.roles[3] == KSHIM_MENU_ROLE_TF);
    assert(h.roles[4] == KSHIM_MENU_ROLE_ADVANCED);
    assert(h.roles[5] == KSHIM_MENU_ROLE_LANGUAGE);
    assert(h.roles[6] == KSHIM_MENU_ROLE_RESTART);
    assert(h.roles[7] == KSHIM_MENU_ROLE_OFF);
    assert(strcmp(h.labels[0], h.names[0]) == 0);
    assert(strcmp(h.labels[1], kshim_menu_role_name(KSHIM_MENU_ROLE_ANDROID, NULL)) == 0);
    assert(strcmp(h.labels[2], kshim_menu_role_name(KSHIM_MENU_ROLE_INTERNAL, NULL)) == 0);
    assert(strcmp(h.labels[3], kshim_menu_role_name(KSHIM_MENU_ROLE_TF, NULL)) == 0);
    /* Known-role descriptions are re-derived, not the stale caller text. */
    assert(strcmp(h.entry_descriptions[0], "caller description") == 0);
    assert(strcmp(h.entry_descriptions[1],
        kshim_menu_role_description(KSHIM_MENU_ROLE_ANDROID, "caller description")) == 0);
}

/* The 49-payload input cap is preserved: with the language page and advanced
 * row the home page fills exactly 53 rows and 50 payloads are still rejected. */
static void test_language_capacity(void)
{
    setup(false, 49);
    h.advanced = true;
    h.advanced_mask = 0;
    ui_at(5, 48, 0);
    size_t selected = SIZE_MAX;
    assert(kshim_platform_select_described(boot_fdt(), h.names, h.descriptions,
            49, 0, 20, 0, &selected) == 0);
    assert(selected == 48);
    assert(h.reset_calls == 0 && h.off_calls == 0 && h.reboot_calls == 0);
    expect_psci_init();
    assert(h.snap_count == 1);
    assert(h.snap[0].kind == 0 && h.snap[0].count == 53);

    setup(false, 50);
    struct { size_t before, value, after; } out = {11, 12345, 22};
    assert(kshim_platform_select_localized(boot_fdt(), h.names, h.descriptions, NULL,
            50, 0, 20, 0, &out.value) < 0);
    assert(out.before == 11 && out.value == 12345 && out.after == 22);
    assert(h.psci_calls == 0 && h.fb_calls == 0 && h.init_calls == 0);
    assert(h.entries_calls == 0 && h.handoff_calls == 0 && h.deinit_calls == 0);
}

/* A failed page rebuild after a live language change falls back to the serial
 * menu without firing any dangerous action, and the selection still resolves
 * through the original payload index. */
static void test_language_ui_fallback(void)
{
    setup(false, 4);
    h.advanced = true;
    h.advanced_mask = UINT64_C(1) << 3;
    h.entries_fail_at = 3; /* home and language page succeed; rebuilt home fails */
    ui_at(5, 4, 0);
    ui_at(10, 1, 0);
    text_at(9000, "2\r");
    size_t selected = SIZE_MAX;
    assert(kshim_platform_select_described(boot_fdt(), h.names, h.descriptions,
            4, 0, 20, h.advanced_mask, &selected) == 0);
    assert(selected == 1);
    assert(h.reset_calls == 0 && h.off_calls == 0 && h.reboot_calls == 0);
    assert(h.snap_count == 3);
    assert(h.snap[2].kind == 0);
    assert(h.now >= 9000);
    expect_psci_init();
}

#endif /* CONFIG_KSHIM_MENU_LANGUAGE_SWITCH */

/* ---- Read-only Device info page (shared by every platform profile). ---- */
/* Every caller-owned input to kshim_platform_select_localized_info is pure
 * data, so the same fixture exercises the real loop in all profiles. */
static struct device_info fixture_info(void)
{
    static const char compatible[] = "lenovo,tb323fu\0qcom,sm8750\0";
    struct device_info info = {0};
    info.board = DEVICE_BOARD_TB323FU;
    info.soc = DEVICE_SOC_SM8750;
    snprintf(info.model, sizeof(info.model), "Lenovo TB323FU");
    info.model_known = true;
    memcpy(info.compatible, compatible, sizeof(compatible) - 1);
    info.compatible_length = sizeof(compatible) - 1;
    info.compatible_known = true;
    info.dt_reported_ram_bytes = UINT64_C(6) * 1024 * 1024 * 1024; /* 6144 MiB */
    info.ram_known = true;
    return info;
}
/* The Device info row sits after the payloads (and the advanced/language rows)
 * but before the two power rows; only the compile-time profile changes the
 * offsets, never the caller's data. */
static size_t info_home_row(void)
{
    size_t row = h.payload_count;
#if CONFIG_KSHIM_MENU_ADVANCED_ACTIONS
    ++row; /* "Advanced boot modes" */
#endif
#if CONFIG_KSHIM_MENU_LANGUAGE_SWITCH
    ++row; /* language row */
#endif
    return row;
}
static const char *expected_version(void)
{
#if defined(KSHIM_BUILD_VERSION)
    return KSHIM_BUILD_VERSION;
#else
    return kshim_menu_text(KSHIM_TEXT_UNKNOWN);
#endif
}
/* Open the info row, keep the whole home page (only that row's description
 * becomes the summary and its action becomes Back), confirm the same info row
 * to return home, then boot payload 1. The serial branch selects the info row
 * number twice (open, then Back) and postpones the payload selection past the
 * original eight-second deadline, proving that the read-only visit cancelled
 * the countdown. */
static void info_drive(unsigned serial, size_t *selected)
{
    if (serial) {
        char line[8];
        snprintf(line, sizeof(line), "%u\r", (unsigned)info_home_row() + 1U);
        text_at(5, line);     /* open the Device info summary row */
        text_at(30, serial == 2 ? "\x1b" : line); /* Back: number entry or Escape */
        text_at(9000, "2\r"); /* main page after the original deadline: payload 1 */
    } else {
        ui_at(5, (int)info_home_row(), 1);  /* interacted => cancels the countdown */
        ui_at(10, (int)info_home_row(), 0); /* confirm the summary row (Back) */
        ui_at(9000, 1, 0);                  /* main page: boot payload index 1 */
    }
    *selected = SIZE_MAX;
    assert(kshim_platform_select_localized_info(boot_fdt(), h.names, h.descriptions,
            NULL, &h.info_fixture, h.payload_count, 0, 8000, 0, selected) == 0);
}
/* Full summary rendering plus the read-only return path. */
static void test_device_info_page(void)
{
    static const kshim_menu_text_id label_ids[7] = {
        KSHIM_TEXT_INFO_BOARD, KSHIM_TEXT_INFO_SOC, KSHIM_TEXT_INFO_MODEL,
        KSHIM_TEXT_INFO_COMPATIBLE, KSHIM_TEXT_INFO_RAM,
        KSHIM_TEXT_INFO_FRAMEBUFFER, KSHIM_TEXT_INFO_VERSION,
    };
    const char *info_label = NULL;
    for (unsigned serial = 0; serial < 3; ++serial) {
        setup(serial != 0, 2);
        h.info_mode = true;
        h.info_fixture = fixture_info();
        size_t selected = SIZE_MAX;
        info_drive(serial, &selected);
        assert(selected == 1);
        assert(h.reset_calls == 0 && h.off_calls == 0 && h.reboot_calls == 0);
        /* Payload 1 is only chosen by the 9000 ms input, so leaving the info
         * page (Back number or Escape) never selects a payload early. */
        assert(h.now >= 9000);
        if (serial) {
            /* The serial page prints the summary instead of building LVGL rows,
             * so there are no GUI snapshots to inspect here. */
            assert(h.info_snap_count == 0);
            continue;
        }

        /* setup() resets the locale, so look the label up after it. */
        info_label = kshim_menu_text(KSHIM_TEXT_INFO_LABEL);
        assert(h.info_snap_count == 3); /* home, summary, home */
        const info_snap_t *home = &h.info_snaps[0];
        const info_snap_t *page = &h.info_snaps[1];
        const info_snap_t *again = &h.info_snaps[2];

        /* The entry is an ordinary main row, carries ENTER (not BOOT) and the
         * info role, and keeps its own localized description on home. */
        size_t row = info_home_row();
        assert(snap_row(home, info_label) == row);
        assert(home->roles[row] == KSHIM_MENU_ROLE_DEVICE_INFO);
        assert(snap_action(home, row) && strcmp(snap_action(home, row), KSHIM_MENU_ACTION_ENTER) == 0);
        assert(snap_desc(home, row) && strcmp(snap_desc(home, row), kshim_menu_text(KSHIM_TEXT_INFO_DESC)) == 0);
#if CONFIG_KSHIM_MENU_POWER_ACTIONS
        /* ...and it precedes the two power rows, which keep their own labels. */
        assert(strcmp(home->names[home->count - 2], KSHIM_STR_RESTART) == 0);
        assert(strcmp(home->names[home->count - 1], KSHIM_STR_POWER_OFF) == 0);
#endif

        /* Activation keeps every home row: same count, same focus, all names
         * and roles preserved and every row untouched except the info row. */
        assert(page->count == home->count);
        assert(page->initial == row);
        for (size_t i = 0; i < page->count; ++i) {
            assert(strcmp(page->names[i], home->names[i]) == 0);
            assert(page->roles[i] == home->roles[i]);
            if (i == row) continue;
            const char *pd = snap_desc(page, i), *hd = snap_desc(home, i);
            const char *pa = snap_action(page, i), *ha = snap_action(home, i);
            assert((pd == NULL) == (hd == NULL));
            if (pd) assert(strcmp(pd, hd) == 0);
            assert((pa == NULL) == (ha == NULL));
            if (pa) assert(strcmp(pa, ha) == 0);
        }
        /* Only the info row changes: description becomes the summary and the
         * action becomes Back (the summary is the DEVICE_INFO row + Back). */
        assert(snap_action(page, row) && strcmp(snap_action(page, row), KSHIM_MENU_ACTION_BACK) == 0);
        const char *summary = snap_desc(page, row);
        assert(summary && summary[0]);

        /* No standalone read-only field rows survive. */
        for (size_t i = 0; i < 7; ++i)
            for (size_t j = 0; j < page->count; ++j)
                assert(strcmp(page->names[j], kshim_menu_text(label_ids[i])) != 0);

        /* The aggregate carries all seven localized label/value pairs in order.
         * RAM is the localized RAM label plus the numeric DT-reported MiB, with
         * no duplicated prefix; values come from the record and the framebuffer. */
        char ram[64];
        snprintf(ram, sizeof(ram), "%llu MiB",
                 (unsigned long long)(h.info_fixture.dt_reported_ram_bytes / (UINT64_C(1024) * 1024)));
        const char *cursor = summary;
        cursor = summary_after(cursor, kshim_menu_text(KSHIM_TEXT_INFO_BOARD), "TB323FU");
        cursor = summary_after(cursor, kshim_menu_text(KSHIM_TEXT_INFO_SOC), "SM8750");
        cursor = summary_after(cursor, kshim_menu_text(KSHIM_TEXT_INFO_MODEL), "Lenovo TB323FU");
        cursor = summary_after(cursor, kshim_menu_text(KSHIM_TEXT_INFO_COMPATIBLE),
                               "lenovo,tb323fu\nqcom,sm8750");
        cursor = summary_after(cursor, kshim_menu_text(KSHIM_TEXT_INFO_RAM), ram);
        cursor = summary_after(cursor, kshim_menu_text(KSHIM_TEXT_INFO_FRAMEBUFFER),
                               "800x600 stride 3200");
        cursor = summary_after(cursor, kshim_menu_text(KSHIM_TEXT_INFO_VERSION),
                               expected_version());
        (void)cursor;

        /* Counting down is cancelled by the interaction; returning home keeps
         * the info focus and the original payload mapping/count untouched. */
        assert(h.countdown_started == 1 && h.countdown_zero >= 1);
        assert(h.status_calls && h.status[0]);
        assert(again->count == home->count);
        assert(again->initial == row);
        assert(snap_row(again, info_label) == row);
        assert(strcmp(again->names[0], h.names[0]) == 0);
    }
}
/* Every unknown field renders the localized Unknown inside the summary, never
 * empty text. */
static void test_device_info_unknown_values(void)
{
    static const kshim_menu_text_id label_ids[7] = {
        KSHIM_TEXT_INFO_BOARD, KSHIM_TEXT_INFO_SOC, KSHIM_TEXT_INFO_MODEL,
        KSHIM_TEXT_INFO_COMPATIBLE, KSHIM_TEXT_INFO_RAM,
        KSHIM_TEXT_INFO_FRAMEBUFFER, KSHIM_TEXT_INFO_VERSION,
    };
    setup(false, 1);
    h.info_mode = true;
    h.info_fixture = (struct device_info){0}; /* board/soc/model/compat/ram unknown */
    h.resources.framebuffer.width = h.resources.framebuffer.height = 0;
    h.resources.framebuffer.stride = 0;       /* framebuffer also unknown */
    const char *unknown = kshim_menu_text(KSHIM_TEXT_UNKNOWN);

    ui_at(5, (int)info_home_row(), 0);  /* open the summary */
    ui_at(10, (int)info_home_row(), 0); /* confirm the summary row (Back) */
    ui_at(15, 0, 0);                    /* boot payload index 0 */
    size_t selected = SIZE_MAX;
    assert(kshim_platform_select_localized_info(boot_fdt(), h.names, h.descriptions,
            NULL, &h.info_fixture, 1, 0, 20, 0, &selected) == 0);
    assert(selected == 0);
    assert(h.info_snap_count == 3);
    const info_snap_t *page = &h.info_snaps[1];
    assert(page->count == h.info_snaps[0].count);
    const char *summary = snap_desc(page, info_home_row());
    assert(summary && summary[0]);
    /* The six hardware fields are unknown; the version field is the build
     * string when VERSION is compiled in, otherwise the localized Unknown. */
    const char *cursor = summary;
    for (size_t i = 0; i < 6; ++i)
        cursor = summary_after(cursor, kshim_menu_text(label_ids[i]), unknown);
    cursor = summary_after(cursor, kshim_menu_text(label_ids[6]), expected_version());
    (void)cursor;
}
/* Compatible variants: several NUL-separated entries, an ellipsis-truncated
 * list and an ellipsis-truncated model, inspected as substrings of the single
 * aggregate summary. */
static void test_device_info_compatible_variants(void)
{
    const char *compat_label = kshim_menu_text(KSHIM_TEXT_INFO_COMPATIBLE);
    const char *model_label = kshim_menu_text(KSHIM_TEXT_INFO_MODEL);
    static const char three[] = "aa\0bb\0cc\0";

    /* Three whole entries are joined with newlines, in original order. */
    setup(false, 1);
    h.info_mode = true;
    h.info_fixture = fixture_info();
    memset(h.info_fixture.compatible, 0, sizeof(h.info_fixture.compatible));
    memcpy(h.info_fixture.compatible, three, sizeof(three) - 1);
    h.info_fixture.compatible_length = sizeof(three) - 1;
    h.info_fixture.compatible_known = true;
    ui_at(5, (int)info_home_row(), 0);
    ui_at(10, (int)info_home_row(), 0);
    ui_at(15, 0, 0);
    size_t selected = SIZE_MAX;
    assert(kshim_platform_select_localized_info(boot_fdt(), h.names, h.descriptions,
            NULL, &h.info_fixture, 1, 0, 20, 0, &selected) == 0);
    assert(selected == 0);
    summary_expect(snap_desc(&h.info_snaps[1], info_home_row()), compat_label, "aa\nbb\ncc");

    /* A truncated list and a truncated model both end in the U+2026 ellipsis. */
    setup(false, 1);
    h.info_mode = true;
    h.info_fixture = fixture_info();
    h.info_fixture.compatible_truncated = true;
    memset(h.info_fixture.model, 'M', sizeof(h.info_fixture.model) - 1);
    h.info_fixture.model[sizeof(h.info_fixture.model) - 1] = '\0';
    h.info_fixture.model_known = true;
    h.info_fixture.model_truncated = true;
    ui_at(5, (int)info_home_row(), 0);
    ui_at(10, (int)info_home_row(), 0);
    ui_at(15, 0, 0);
    assert(kshim_platform_select_localized_info(boot_fdt(), h.names, h.descriptions,
            NULL, &h.info_fixture, 1, 0, 20, 0, &selected) == 0);
    const char *summary = snap_desc(&h.info_snaps[1], info_home_row());
    size_t model_len = 0;
    const char *model = summary_field(summary, model_label, &model_len);
    assert(model_len == KSHIM_INFO_MODEL_MAX + 3);
    assert(strncmp(model, "MMMM", 4) == 0);
    /* The value is a substring of the aggregate summary, so only its own last
     * three bytes are the ellipsis; the next field starts right after. */
    assert(strncmp(model + model_len - 3, "\xe2\x80\xa6", 3) == 0);
    summary_expect(summary, compat_label, "lenovo,tb323fu\nqcom,sm8750\xe2\x80\xa6");

    /* Four tiny entries: only the first three lines render, with an ellipsis
     * marking the fourth on the last rendered line. */
    static const char four[] = "aa\0bb\0cc\0dd\0";
    setup(false, 1);
    h.info_mode = true;
    h.info_fixture = fixture_info();
    memset(h.info_fixture.compatible, 0, sizeof(h.info_fixture.compatible));
    memcpy(h.info_fixture.compatible, four, sizeof(four) - 1);
    h.info_fixture.compatible_length = sizeof(four) - 1;
    h.info_fixture.compatible_known = true;
    ui_at(5, (int)info_home_row(), 0);
    ui_at(10, (int)info_home_row(), 0);
    ui_at(15, 0, 0);
    assert(kshim_platform_select_localized_info(boot_fdt(), h.names, h.descriptions,
            NULL, &h.info_fixture, 1, 0, 20, 0, &selected) == 0);
    summary_expect(snap_desc(&h.info_snaps[1], info_home_row()), compat_label,
                   "aa\nbb\ncc\xe2\x80\xa6");

    /* A single oversized entry keeps its original prefix and ends with the
     * ellipsis; it must not become Unknown just because it does not fit. */
    setup(false, 1);
    h.info_mode = true;
    h.info_fixture = fixture_info();
    memset(h.info_fixture.compatible, 0, sizeof(h.info_fixture.compatible));
    for (size_t i = 0; i < 60; ++i)
        h.info_fixture.compatible[i] = (char)('a' + (int)(i % 26));
    h.info_fixture.compatible_length = 61; /* 60 characters plus the NUL */
    h.info_fixture.compatible_known = true;
    ui_at(5, (int)info_home_row(), 0);
    ui_at(10, (int)info_home_row(), 0);
    ui_at(15, 0, 0);
    assert(kshim_platform_select_localized_info(boot_fdt(), h.names, h.descriptions,
            NULL, &h.info_fixture, 1, 0, 20, 0, &selected) == 0);
    size_t over_len = 0;
    const char *oversized = summary_field(snap_desc(&h.info_snaps[1], info_home_row()),
                                          compat_label, &over_len);
    assert(over_len == 44 + 3);
    assert(strncmp(oversized, h.info_fixture.compatible, 44) == 0);
    assert(strncmp(oversized + 44, "\xe2\x80\xa6", 3) == 0);
}
/* The fixed-size pages still reject an overflow instead of dropping a row. */
static void test_device_info_capacity(void)
{
    size_t rows = 1; /* the Device info row */
#if CONFIG_KSHIM_MENU_POWER_ACTIONS
    rows += 2;
#endif
#if CONFIG_KSHIM_MENU_ADVANCED_ACTIONS
    rows += 1;
#endif
#if CONFIG_KSHIM_MENU_LANGUAGE_SWITCH
    rows += 1;
#endif
    /* The home page cannot exceed KSHIM_MENU_MAX_ENTRIES rows and the payload
     * input can never exceed 49, so the largest buildable count is the smaller
     * of the two bounds. */
    size_t max_payloads = KSHIM_MENU_MAX_ENTRIES - rows;
    if (max_payloads > 49U) max_payloads = 49U;

    /* Exactly at capacity the page still builds and the summary is readable. */
    setup(false, max_payloads);
    h.info_mode = true;
    h.info_fixture = fixture_info();
    ui_at(5, (int)info_home_row(), 0);
    ui_at(10, (int)info_home_row(), 0);
    ui_at(15, 0, 0);
    size_t selected = SIZE_MAX;
    assert(kshim_platform_select_localized_info(boot_fdt(), h.names, h.descriptions,
            NULL, &h.info_fixture, max_payloads, 0, 20, 0, &selected) == 0);
    assert(selected == 0);
    assert(h.info_snap_count == 3);
    assert(h.info_snaps[1].count == h.info_snaps[0].count);
    assert(h.info_snaps[0].count == max_payloads + rows);
    const char *summary = snap_desc(&h.info_snaps[1], info_home_row());
    assert(summary && summary[0] && strlen(summary) < INFO_TEXT_LEN);

    /* One row more is rejected before any peripheral or UI work starts. */
    setup(false, max_payloads + 1);
    h.info_fixture = fixture_info();
    struct { size_t before, value, after; } out = {11, 12345, 22};
    assert(kshim_platform_select_localized_info(boot_fdt(), h.names, h.descriptions,
            NULL, &h.info_fixture, max_payloads + 1, 0, 20, 0, &out.value) < 0);
    assert(out.before == 11 && out.value == 12345 && out.after == 22);
    assert(h.psci_calls == 0 && h.fb_calls == 0 && h.init_calls == 0);
    assert(h.entries_calls == 0 && h.handoff_calls == 0 && h.deinit_calls == 0);
}

#if CONFIG_KSHIM_MENU_LANGUAGE_SWITCH
/* A live language switch rebuilds the info labels and the localized Unknown
 * without disturbing the selected locale or the read-only values. */
static void test_device_info_language_refresh(void)
{
    setup(false, 2);
    /* The Device info entry itself is localized; remember its default-locale
     * text so the pre-switch home page can be checked against it. */
    char default_info[64];
    snprintf(default_info, sizeof(default_info), "%s",
             kshim_menu_text(KSHIM_TEXT_INFO_LABEL));
    h.info_mode = true;
    h.info_fixture = (struct device_info){0}; /* unknowns are locale-bound text */
    size_t lang_row = h.payload_count;
#if CONFIG_KSHIM_MENU_ADVANCED_ACTIONS
    ++lang_row; /* advanced row precedes the language row */
#endif

    ui_at(5, (int)lang_row, 0);         /* open the language page */
    ui_at(10, 1, 0);                    /* choose English */
    ui_at(15, (int)info_home_row(), 0); /* open Device info in English */
    ui_at(20, (int)info_home_row(), 0); /* confirm the same info row (Back) */
    ui_at(25, 1, 0);                    /* boot payload index 1 */
    size_t selected = SIZE_MAX;
    assert(kshim_platform_select_localized_info(boot_fdt(), h.names, h.descriptions,
            NULL, &h.info_fixture, 2, 0, 8000, 0, &selected) == 0);
    assert(selected == 1);
    assert(h.reset_calls == 0 && h.off_calls == 0 && h.reboot_calls == 0);

    /* Snapshots: home(default), language, home(EN), info(EN), home(EN). */
    assert(h.info_snap_count == 5);
    const info_snap_t *home = &h.info_snaps[0];
    const info_snap_t *home_en = &h.info_snaps[2];
    const info_snap_t *page_en = &h.info_snaps[3];
    const info_snap_t *again_en = &h.info_snaps[4];

    /* The pre-switch home keeps the default-locale Device info entry. */
    assert(strcmp(home->names[info_home_row()], default_info) == 0);
    assert(strcmp(home_en->names[info_home_row()], "Device info") == 0);

    /* Activation keeps the whole home page and the info focus; only that row's
     * description becomes the localized summary and its action becomes Back. */
    assert(page_en->count == home_en->count);
    assert(page_en->initial == (size_t)info_home_row());
    for (size_t i = 0; i < page_en->count; ++i)
        assert(strcmp(page_en->names[i], home_en->names[i]) == 0);
    assert(snap_action(page_en, info_home_row()) &&
           strcmp(snap_action(page_en, info_home_row()), KSHIM_MENU_ACTION_BACK) == 0);

    /* The same info row description is now English: the seven localized field
     * labels, the English Unknown for the unknown fields and this fixture's
     * real framebuffer value. */
    const char *unknown = kshim_menu_text(KSHIM_TEXT_UNKNOWN);
    const char *cursor = snap_desc(page_en, info_home_row());
    assert(cursor && cursor[0]);
    cursor = summary_after(cursor, kshim_menu_text(KSHIM_TEXT_INFO_BOARD), unknown);
    cursor = summary_after(cursor, kshim_menu_text(KSHIM_TEXT_INFO_SOC), unknown);
    cursor = summary_after(cursor, kshim_menu_text(KSHIM_TEXT_INFO_MODEL), unknown);
    cursor = summary_after(cursor, kshim_menu_text(KSHIM_TEXT_INFO_COMPATIBLE), unknown);
    cursor = summary_after(cursor, kshim_menu_text(KSHIM_TEXT_INFO_RAM), unknown);
    cursor = summary_after(cursor, kshim_menu_text(KSHIM_TEXT_INFO_FRAMEBUFFER),
                           "800x600 stride 3200");
    cursor = summary_after(cursor, kshim_menu_text(KSHIM_TEXT_INFO_VERSION),
                           expected_version());
    (void)cursor;

    /* Returning home keeps the info focus and the session language. */
    assert(again_en->count == home_en->count);
    assert(again_en->initial == (size_t)info_home_row());
    assert(strcmp(again_en->names[info_home_row()], "Device info") == 0);
    assert(kshim_menu_get_language() == KSHIM_MENU_LANG_EN);
}
#endif

/* ---- Debug-console OFF auto-return gate (TB322FC only). ---------------- */
/* These cases drive the real loop through the public Device info entry point
 * with h.info_mode so the generic page capture is reused.  The eligible record
 * carries only the board enum: the board must come from the record, never be
 * inferred from the model or SoC text. */
static const kshim_menu_role_t console_off_roles[2] = {
    KSHIM_MENU_ROLE_UNKNOWN, KSHIM_MENU_ROLE_ANDROID,
};
static struct device_info console_off_info(enum device_board board)
{
    struct device_info info = {0};
    info.board = board;
    return info;
}
/* Include the case and counters when an assertion fails. */
static void console_off_report(const char *case_name, int rc, size_t selected)
{
    printf("case=%s console=%d rc=%d selected=%zu keys_build=%u polls=%u "
           "reads=%u frames=%u now=%llu deinit=%u handoff=%u\n",
           case_name, CONFIG_KSHIM_DEBUG_CONSOLE, rc, selected,
           h.keys_build_calls, h.key_poll_calls, h.console_read_calls, h.frames,
           (unsigned long long)h.now, h.deinit_calls, h.handoff_calls);
}
/* The three pre-GUI failures in the order the loop can reach them: the
 * framebuffer is rejected before build_keys, LVGL init is rejected after one
 * build_keys, and the first home page is rejected after one build_keys. */
enum initial_failure { FAIL_FRAMEBUFFER, FAIL_LVGL_INIT, FAIL_FIRST_ENTRIES };
static void arm_initial_failure(unsigned kind)
{
    setup(kind == FAIL_FRAMEBUFFER, 2);
    h.info_mode = true;
    h.info_fixture = console_off_info(DEVICE_BOARD_TB322FC);
    if (kind == FAIL_LVGL_INIT) h.ui_init_result = -1;
    if (kind == FAIL_FIRST_ENTRIES) h.entries_fail_at = 1;
    /* Serial stays reachable: the same scripted keys are queued in both
     * profiles, but the OFF gate must never consume them. */
    key_at(5, QCOM_KEYS_EVENT, QCOM_KEY_EVENT_PRESS, QCOM_KEY_ACTION_UP, 1);
    key_at(10, QCOM_KEYS_EVENT, QCOM_KEY_EVENT_PRESS, QCOM_KEY_ACTION_SELECT, 1);
}
static void test_console_off_initial_failures(void)
{
    static const char *const names[3] = {"framebuffer", "lvgl_init", "first_entries"};
    for (unsigned kind = 0; kind < 3U; ++kind) {
        arm_initial_failure(kind);
        size_t selected = 12345;
        int rc = kshim_platform_select_localized_info(boot_fdt(), h.names, h.descriptions,
                console_off_roles, &h.info_fixture, 2, 1, 20, 0, &selected);
        console_off_report(names[kind], rc, selected);
#if CONFIG_KSHIM_DEBUG_CONSOLE
        /* Console ON: the original serial path runs and the keys pick payload 0. */
        assert(rc == 0 && selected == 0);
        assert(h.frames == 0);
        assert(h.keys_build_calls == (kind == FAIL_FRAMEBUFFER ? 1U : 2U));
        assert(h.key_poll_calls > 0 && h.console_read_calls > 0);
        assert(h.now > 0);
        assert(h.deinit_calls == 1 && h.handoff_calls == 1);
#else
        /* Console OFF with every gate satisfied: return the original initial
         * immediately, with no GUI, no serial work and no second key build. */
        assert(rc == 0 && selected == 1);
        assert(h.frames == 0 && h.now == 0);
        assert(h.keys_build_calls == (kind == FAIL_FRAMEBUFFER ? 0U : 1U));
        assert(h.key_poll_calls == 0 && h.console_read_calls == 0);
        assert(h.key_next == 0 && h.char_next == 0);
        assert(h.deinit_calls == 1 && h.handoff_calls == 1);
#endif
    }
}
/* A cleanup (handoff) failure must return -2 and never auto-select, in both
 * console profiles and for all three pre-GUI failures. */
static void test_console_off_initial_handoff_failure(void)
{
    static const char *const names[3] = {"handoff_framebuffer", "handoff_lvgl_init",
                                         "handoff_first_entries"};
    for (unsigned kind = 0; kind < 3U; ++kind) {
        arm_initial_failure(kind);
        h.handoff_result = -1;
        size_t selected = 12345;
        int rc = kshim_platform_select_localized_info(boot_fdt(), h.names, h.descriptions,
                console_off_roles, &h.info_fixture, 2, 1, 20, 0, &selected);
        console_off_report(names[kind], rc, selected);
        assert(rc == -2);
        assert(selected == 12345); /* sentinel preserved: never auto-selects */
        assert(h.frames == 0);
        assert(h.keys_build_calls == (kind == FAIL_FRAMEBUFFER ? 0U : 1U));
        assert(h.key_poll_calls == 0 && h.console_read_calls == 0);
        assert(h.deinit_calls == 1 && h.handoff_calls == 1);
    }
}
/* A successful GUI is never auto-returned, even on the eligible profile. */
static void test_console_off_success_gui(void)
{
    setup(false, 2);
    h.info_mode = true;
    h.info_fixture = console_off_info(DEVICE_BOARD_TB322FC);
    /* The interaction is the read-only Device info visit: open that same home
     * row (never the retired field index 0), then pick payload 0 from the page. */
    ui_at(5, (int)info_home_row(), 1); /* interacted: open Device info */
    ui_at(10, 0, 0);                   /* pick payload 0 through the live GUI */
    size_t selected = 12345;
    int rc = kshim_platform_select_localized_info(boot_fdt(), h.names, h.descriptions,
            console_off_roles, &h.info_fixture, 2, 1, 20, 0, &selected);
    console_off_report("success_gui", rc, selected);
    assert(rc == 0 && selected == 0);
    assert(h.frames > 0);
    assert(h.keys_build_calls == 1);
    assert(h.key_poll_calls == 0 && h.console_read_calls == 0);
    assert(h.deinit_calls == 1 && h.handoff_calls == 1);
}
/* A failure on a subpage rebuild (after the GUI main loop was entered) stays
 * interactive: the original serial menu runs, not the auto-return. */
static void test_console_off_subpage_rebuild_serial(void)
{
    setup(false, 2);
    h.info_mode = true;
    h.info_fixture = console_off_info(DEVICE_BOARD_TB322FC);
    h.entries_fail_at = 2; /* home builds; the Device info rebuild fails */
    ui_at(5, (int)info_home_row(), 1); /* open Device info after the home build */
    key_at(10, QCOM_KEYS_EVENT, QCOM_KEY_EVENT_PRESS, QCOM_KEY_ACTION_SELECT, 1);
    /* Confirm the read-only field (returns home on the Device info row), walk up
     * to payload 0, then confirm after the original deadline to show that the
     * cancelled timeout is retained and the default is never taken. */
    for (size_t i = 0; i < info_home_row(); ++i)
        key_at(15U + (uint64_t)i * 5U, QCOM_KEYS_EVENT, QCOM_KEY_EVENT_PRESS,
               QCOM_KEY_ACTION_UP, 1);
    key_at(9000, QCOM_KEYS_EVENT, QCOM_KEY_EVENT_PRESS, QCOM_KEY_ACTION_SELECT, 1);
    size_t selected = 12345;
    int rc = kshim_platform_select_localized_info(boot_fdt(), h.names, h.descriptions,
            console_off_roles, &h.info_fixture, 2, 1, 20, 0, &selected);
    console_off_report("subpage_rebuild", rc, selected);
    assert(rc == 0 && selected == 0);
    assert(h.frames > 0);
    assert(h.entries_calls == 2 && h.keys_build_calls == 2);
    assert(h.key_poll_calls > 0);
    assert(h.deinit_calls == 1 && h.handoff_calls == 1);
}
/* Every broken gate on the initial LVGL failure keeps the original serial menu. */
static void test_console_off_gate_negatives(void)
{
    struct device_info tb323fu = {0};
    struct device_info unknown_board = {0};
    struct device_info tb322fc = {0};
    tb323fu.board = DEVICE_BOARD_TB323FU;
    unknown_board.board = DEVICE_BOARD_UNKNOWN;
    tb322fc.board = DEVICE_BOARD_TB322FC;
    static const kshim_menu_role_t unknown_roles[2] = {
        KSHIM_MENU_ROLE_UNKNOWN, KSHIM_MENU_ROLE_UNKNOWN,
    };
    static const kshim_menu_role_t internal_roles[2] = {
        KSHIM_MENU_ROLE_UNKNOWN, KSHIM_MENU_ROLE_INTERNAL,
    };
    struct gate_case {
        const char *name;
        const struct device_info *info;
        const kshim_menu_role_t *roles;
    };
    const struct gate_case cases[] = {
        {"board_tb323fu", &tb323fu, console_off_roles},
        {"board_unknown", &unknown_board, console_off_roles},
        {"null_info", NULL, console_off_roles},
        {"null_roles", &tb322fc, NULL},
        {"role_unknown", &tb322fc, unknown_roles},
        {"role_non_android", &tb322fc, internal_roles},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        setup(false, 2);
        h.info_mode = true;
        if (cases[i].info) h.info_fixture = *cases[i].info;
        h.ui_init_result = -1; /* the initial LVGL failure the gate could catch */
        kshim_menu_role_t roles_before[2] = {
            KSHIM_MENU_ROLE_UNKNOWN, KSHIM_MENU_ROLE_UNKNOWN,
        };
        if (cases[i].roles) memcpy(roles_before, cases[i].roles, sizeof(roles_before));
        key_at(5, QCOM_KEYS_EVENT, QCOM_KEY_EVENT_PRESS, QCOM_KEY_ACTION_UP, 1);
        key_at(10, QCOM_KEYS_EVENT, QCOM_KEY_EVENT_PRESS, QCOM_KEY_ACTION_SELECT, 1);
        size_t selected = 12345;
        int rc = kshim_platform_select_localized_info(boot_fdt(), h.names, h.descriptions,
                cases[i].roles, cases[i].info ? &h.info_fixture : NULL,
                2, 1, 20, 0, &selected);
        console_off_report(cases[i].name, rc, selected);
        assert(rc == 0 && selected == 0); /* original serial path picks payload 0 */
        assert(h.keys_build_calls == 2 && h.key_poll_calls > 0);
        assert(h.key_next == 2);
        /* Caller inputs stay untouched; the nonzero default 1 drives UP -> 0. */
        if (cases[i].roles)
            assert(memcmp(roles_before, cases[i].roles, sizeof(roles_before)) == 0);
        assert(h.names[0] == h.payload_labels[0]);
        assert(h.names[1] == h.payload_labels[1]);
    }
}

/* ---- Session-settings entry point (new API). ------------------------- */
/* Every case drives the real loop through
 * kshim_platform_select_localized_settings with the generic info_mode page
 * capture; no production menu_build / menu_fill is called directly and no
 * second state machine is introduced. The compile-time profile provides the
 * advanced page, the language page, the info row and the two power rows, so
 * the fixed home layout below is stable for these tests. */
#if CONFIG_KSHIM_MENU_SESSION_SETTINGS && CONFIG_KSHIM_MENU_ADVANCED_ACTIONS && \
    CONFIG_KSHIM_MENU_LANGUAGE_SWITCH && CONFIG_KSHIM_MENU_POWER_ACTIONS
#define KSHIM_SETTINGS_MENU_TESTS 1

enum {
    SET_MAIN_ROWS = 8,
    SET_ROW_ANDROID = 0, SET_ROW_INTERNAL = 1, SET_ROW_TF = 2,
    SET_ROW_ADVANCED = 3, SET_ROW_SETTINGS = 4, SET_ROW_INFO = 5,
    SET_ROW_RESTART = 6, SET_ROW_OFF = 7,
    /* Back rows of the settings-family pages. */
    SET_SETTINGS_BACK = 3, SET_DEVICE_BACK = 3, SET_DEFAULT_BACK = 4,
    SET_LANGUAGE_BACK = 2,
};

static struct device_info settings_info(enum device_board board)
{
    struct device_info info = fixture_info();
    info.board = board;
    if (board == DEVICE_BOARD_TB322FC) {
        /* Real Gen4 record: the model and DT compatible must name the Gen4
         * platform, not the Gen5 fixture values. The compatible string keeps
         * the same shape (the trailing NUL is excluded from the length). */
        static const char compatible[] = "lenovo,tb322fc\0qcom,sm8750\0";
        snprintf(info.model, sizeof(info.model), "Lenovo TB322FC");
        memcpy(info.compatible, compatible, sizeof(compatible) - 1);
        info.compatible_length = sizeof(compatible) - 1;
    }
    return info;
}
static void settings_setup(unsigned serial, size_t count)
{
    setup(serial != 0, count);
    h.info_mode = true; /* reuse the generic page capture */
}
static size_t settings_run(const kshim_menu_role_t *roles,
                           const enum device_board *boards,
                           const struct device_info *info,
                           size_t count, size_t initial, uint32_t timeout,
                           uint64_t mask, int *rc)
{
    size_t selected = SIZE_MAX;
    *rc = kshim_platform_select_localized_settings(boot_fdt(), h.names, NULL, roles,
             info, boards, count, initial, timeout, mask, &selected);
    return selected;
}
/* A fixed boot-system row: localized role name, role recorded, Boot action when
 * usable or the short localized "unavailable" action plus the long note. */
static void settings_expect_system(const info_snap_t *snap, size_t row,
                                   kshim_menu_role_t role, int usable)
{
    assert(row < snap->count);
    assert(strcmp(snap->names[row], kshim_menu_role_name(role, NULL)) == 0);
    assert(snap->roles[row] == role);
    if (usable) {
        assert(snap_action(snap, row) &&
               strcmp(snap_action(snap, row), KSHIM_MENU_ACTION_BOOT) == 0);
    } else {
        assert(snap_action(snap, row) &&
               strcmp(snap_action(snap, row),
                      kshim_menu_text(KSHIM_TEXT_ACTION_UNAVAILABLE)) == 0);
        assert(snap_desc(snap, row) &&
               strcmp(snap_desc(snap, row), kshim_menu_text(KSHIM_TEXT_UNAVAILABLE)) == 0);
    }
}
/* A Default-page row: same rule but a selectable row uses the "select" action. */
static void settings_expect_default(const info_snap_t *snap, size_t row,
                                    kshim_menu_role_t role, int selectable)
{
    assert(row < snap->count);
    assert(strcmp(snap->names[row], kshim_menu_role_name(role, NULL)) == 0);
    assert(snap->roles[row] == role);
    if (selectable) {
        assert(snap_action(snap, row) &&
               strcmp(snap_action(snap, row),
                      kshim_menu_text(KSHIM_TEXT_LANGUAGE_SELECT)) == 0);
    } else {
        assert(snap_action(snap, row) &&
               strcmp(snap_action(snap, row),
                      kshim_menu_text(KSHIM_TEXT_ACTION_UNAVAILABLE)) == 0);
        assert(snap_desc(snap, row) &&
               strcmp(snap_desc(snap, row), kshim_menu_text(KSHIM_TEXT_UNAVAILABLE)) == 0);
    }
}
/* The new-API Back rows carry the short "Back" label and the parent-layer
 * description, never the old "back to the system boot options" text. */
static void settings_expect_back(const info_snap_t *snap, size_t row)
{
    assert(row < snap->count);
    assert(snap->roles[row] == KSHIM_MENU_ROLE_BACK);
    assert(strcmp(snap->names[row], KSHIM_MENU_ACTION_BACK) == 0);
    assert(snap_desc(snap, row) &&
           strcmp(snap_desc(snap, row), KSHIM_MENU_DESC_BACK_PARENT) == 0);
    assert(snap_action(snap, row) &&
           strcmp(snap_action(snap, row), KSHIM_MENU_ACTION_BACK) == 0);
}

/* Android-only image on a real Gen4 device: the 8 s timeout must auto-boot the
 * real Android row; the missing internal / TF / diagnostic rows stay visible
 * but unavailable, and the info row matches the real localized+info page. */
static void test_settings_home_default_android(void)
{
    static const kshim_menu_role_t roles[1] = { KSHIM_MENU_ROLE_ANDROID };
    static const enum device_board boards[1] = { DEVICE_BOARD_TB322FC };
    struct device_info info = settings_info(DEVICE_BOARD_TB322FC);
    int rc;

    settings_setup(0, 1);
    size_t selected = settings_run(roles, boards, &info, 1, 0, 8000, 0, &rc);
    assert(rc == 0 && selected == 0);
    assert(h.reset_calls == 0 && h.off_calls == 0 && h.reboot_calls == 0);
    expect_psci_init();
    assert(h.deinit_calls == 1U && h.handoff_calls == 1U);
    assert(h.countdown_started == 1 && h.countdown_first == 8000);

    assert(h.info_snap_count >= 1);
    const info_snap_t *home = &h.info_snaps[0];
    assert(home->count == SET_MAIN_ROWS);
    assert(home->initial == SET_ROW_ANDROID);
    settings_expect_system(home, SET_ROW_ANDROID, KSHIM_MENU_ROLE_ANDROID, 1);
    settings_expect_system(home, SET_ROW_INTERNAL, KSHIM_MENU_ROLE_INTERNAL, 0);
    settings_expect_system(home, SET_ROW_TF, KSHIM_MENU_ROLE_TF, 0);
    assert(home->roles[SET_ROW_ADVANCED] == KSHIM_MENU_ROLE_ADVANCED);
    assert(home->roles[SET_ROW_SETTINGS] == KSHIM_MENU_ROLE_SETTINGS);
    /* Same left-column Device info entry as the localized+info menu. */
    assert(home->roles[SET_ROW_INFO] == KSHIM_MENU_ROLE_DEVICE_INFO);
    assert(strcmp(home->names[SET_ROW_INFO], kshim_menu_text(KSHIM_TEXT_INFO_LABEL)) == 0);
    assert(snap_action(home, SET_ROW_INFO) &&
           strcmp(snap_action(home, SET_ROW_INFO), KSHIM_MENU_ACTION_ENTER) == 0);
    assert(home->roles[SET_ROW_RESTART] == KSHIM_MENU_ROLE_RESTART);
    assert(home->roles[SET_ROW_OFF] == KSHIM_MENU_ROLE_OFF);
}

/* The advanced page keeps its five fixed rows (missing diagnostic included). */
static void test_settings_advanced_page(void)
{
    static const kshim_menu_role_t roles[1] = { KSHIM_MENU_ROLE_ANDROID };
    static const enum device_board boards[1] = { DEVICE_BOARD_TB322FC };
    struct device_info info = settings_info(DEVICE_BOARD_TB322FC);
    int rc;

    settings_setup(0, 1);
    ui_at(5, SET_ROW_ADVANCED, 1); /* open advanced (interaction cancels timeout) */
    ui_at(10, 4, 0);               /* advanced Back -> home */
    ui_at(15, SET_ROW_ANDROID, 0); /* pick Android */
    size_t selected = settings_run(roles, boards, &info, 1, 0, 8000, 0, &rc);
    assert(rc == 0 && selected == 0);
    assert(h.info_snap_count == 3); /* home, advanced, home */
    const info_snap_t *adv = &h.info_snaps[1];
    assert(adv->count == 5);
    assert(adv->roles[0] == KSHIM_MENU_ROLE_DIAGNOSTIC);
    assert(snap_action(adv, 0) &&
           strcmp(snap_action(adv, 0), kshim_menu_text(KSHIM_TEXT_ACTION_UNAVAILABLE)) == 0);
    assert(adv->roles[1] == KSHIM_MENU_ROLE_RECOVERY);
    assert(snap_action(adv, 1) && strcmp(snap_action(adv, 1), KSHIM_MENU_ACTION_ENTER) == 0);
    assert(adv->roles[2] == KSHIM_MENU_ROLE_FASTBOOT);
    assert(adv->roles[3] == KSHIM_MENU_ROLE_EDL);
    assert(adv->roles[4] == KSHIM_MENU_ROLE_BACK);
    assert(h.info_snaps[2].count == SET_MAIN_ROWS);
}

/* The fixed system rows keep their order no matter how the caller orders the
 * manifest candidates, and every row returns the original caller index. */
static void test_settings_order_and_caller_index(void)
{
    static const kshim_menu_role_t roles_a[2] = { KSHIM_MENU_ROLE_ANDROID, KSHIM_MENU_ROLE_INTERNAL };
    static const enum device_board boards_a[2] = { DEVICE_BOARD_TB322FC, DEVICE_BOARD_TB322FC };
    static const kshim_menu_role_t roles_b[2] = { KSHIM_MENU_ROLE_INTERNAL, KSHIM_MENU_ROLE_ANDROID };
    static const kshim_menu_role_t roles_c[3] = {
        KSHIM_MENU_ROLE_TF, KSHIM_MENU_ROLE_ANDROID, KSHIM_MENU_ROLE_INTERNAL };
    static const enum device_board boards_c[3] = {
        DEVICE_BOARD_TB322FC, DEVICE_BOARD_UNKNOWN, DEVICE_BOARD_TB322FC };
    struct device_info info = settings_info(DEVICE_BOARD_TB322FC);
    int rc;

    /* Android first, then internal. */
    settings_setup(0, 2);
    ui_at(5, SET_ROW_ANDROID, 1);
    size_t selected = settings_run(roles_a, boards_a, &info, 2, 0, 8000, 0, &rc);
    assert(rc == 0 && selected == 0);
    const info_snap_t *home = &h.info_snaps[0];
    assert(home->count == SET_MAIN_ROWS);
    settings_expect_system(home, SET_ROW_ANDROID, KSHIM_MENU_ROLE_ANDROID, 1);
    settings_expect_system(home, SET_ROW_INTERNAL, KSHIM_MENU_ROLE_INTERNAL, 1);
    settings_expect_system(home, SET_ROW_TF, KSHIM_MENU_ROLE_TF, 0);

    /* Reversed order: same fixed rows, but Android now maps to caller 1. */
    settings_setup(0, 2);
    ui_at(5, SET_ROW_ANDROID, 1);
    selected = settings_run(roles_b, boards_a, &info, 2, 1, 8000, 0, &rc);
    assert(rc == 0 && selected == 1);
    home = &h.info_snaps[0];
    assert(home->count == SET_MAIN_ROWS);
    settings_expect_system(home, SET_ROW_ANDROID, KSHIM_MENU_ROLE_ANDROID, 1);
    settings_expect_system(home, SET_ROW_INTERNAL, KSHIM_MENU_ROLE_INTERNAL, 1);

    /* Three shuffled callers: the Android row returns caller 1, the internal
     * row returns caller 2, the TF row returns caller 0. */
    settings_setup(0, 3);
    ui_at(5, SET_ROW_ANDROID, 1);
    selected = settings_run(roles_c, boards_c, &info, 3, 1, 8000, 0, &rc);
    assert(rc == 0 && selected == 1);
    home = &h.info_snaps[0];
    settings_expect_system(home, SET_ROW_ANDROID, KSHIM_MENU_ROLE_ANDROID, 1);
    settings_expect_system(home, SET_ROW_INTERNAL, KSHIM_MENU_ROLE_INTERNAL, 1);
    settings_expect_system(home, SET_ROW_TF, KSHIM_MENU_ROLE_TF, 1);

    settings_setup(0, 3);
    ui_at(5, SET_ROW_INTERNAL, 1);
    selected = settings_run(roles_c, boards_c, &info, 3, 0, 8000, 0, &rc);
    assert(rc == 0 && selected == 2);
}

/* Board filtering: Auto on Gen4 resolves the internal row to the Gen4 payload;
 * manual Gen5 resolves it to the Gen5 payload; returning to Auto re-detects
 * Gen4 and resolves the internal row back to the Gen4 payload. */
static void test_settings_board_filter_manual_auto(void)
{
    static const kshim_menu_role_t roles[3] = {
        KSHIM_MENU_ROLE_INTERNAL, KSHIM_MENU_ROLE_INTERNAL, KSHIM_MENU_ROLE_ANDROID };
    static const enum device_board boards[3] = {
        DEVICE_BOARD_TB322FC, DEVICE_BOARD_TB323FU, DEVICE_BOARD_UNKNOWN };
    struct device_info info = settings_info(DEVICE_BOARD_TB322FC);
    int rc;

    settings_setup(0, 3);
    ui_at(5, SET_ROW_INTERNAL, 1);
    size_t selected = settings_run(roles, boards, &info, 3, 0, 8000, 0, &rc);
    assert(rc == 0 && selected == 0);

    /* Manual Gen5 -> internal resolves to the Gen5 caller (index 1). */
    settings_setup(0, 3);
    ui_at(5, SET_ROW_SETTINGS, 1);  /* home -> settings */
    ui_at(10, 0, 0);                /* settings -> device page */
    ui_at(15, 2, 0);                /* choose Gen5 */
    ui_at(20, SET_SETTINGS_BACK, 0);/* settings back -> home */
    ui_at(25, SET_ROW_INTERNAL, 0); /* internal row */
    selected = settings_run(roles, boards, &info, 3, 0, 8000, 0, &rc);
    assert(rc == 0 && selected == 1);

    /* Back to Auto -> detection is Gen4 again, internal resolves to index 0. */
    settings_setup(0, 3);
    ui_at(5, SET_ROW_SETTINGS, 1);
    ui_at(10, 0, 0);   /* settings -> device */
    ui_at(15, 2, 0);   /* manual Gen5 */
    ui_at(20, 0, 0);   /* settings -> device again */
    ui_at(25, 0, 0);   /* device Auto */
    ui_at(30, SET_SETTINGS_BACK, 0);
    ui_at(35, SET_ROW_INTERNAL, 0);
    selected = settings_run(roles, boards, &info, 3, 0, 8000, 0, &rc);
    assert(rc == 0 && selected == 0);
}

/* No board-safe Linux target: a NULL board array, UNKNOWN evidence, a
 * wrong-model image and a duplicated match all leave the internal row
 * unavailable; confirming it never returns a payload and only the explicit
 * Android choice exits. */
static void test_settings_no_linux_on_bad_board(void)
{
    static const enum device_board unknown_boards[2] = { DEVICE_BOARD_UNKNOWN, DEVICE_BOARD_UNKNOWN };
    static const enum device_board wrong_boards[2] = { DEVICE_BOARD_UNKNOWN, DEVICE_BOARD_TB323FU };
    static const kshim_menu_role_t roles2[2] = { KSHIM_MENU_ROLE_ANDROID, KSHIM_MENU_ROLE_INTERNAL };
    static const kshim_menu_role_t roles3[3] = {
        KSHIM_MENU_ROLE_ANDROID, KSHIM_MENU_ROLE_INTERNAL, KSHIM_MENU_ROLE_INTERNAL };
    static const enum device_board dup_boards[3] = {
        DEVICE_BOARD_UNKNOWN, DEVICE_BOARD_TB322FC, DEVICE_BOARD_TB322FC };
    struct device_info info = settings_info(DEVICE_BOARD_TB322FC);
    struct bad_case { const char *name; const kshim_menu_role_t *roles;
                      const enum device_board *boards; size_t count; };
    const struct bad_case cases[] = {
        { "null_boards", roles2, NULL, 2 },
        { "unknown_boards", roles2, unknown_boards, 2 },
        { "wrong_model", roles2, wrong_boards, 2 },
        { "duplicate_match", roles3, dup_boards, 3 },
    };

    for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); ++c) {
        int rc;
        settings_setup(0, cases[c].count);
        ui_at(5, SET_ROW_INTERNAL, 1);   /* try the unavailable internal row */
        ui_at(10, SET_ROW_ANDROID, 0);   /* then the real Android row */
        size_t selected = settings_run(cases[c].roles, cases[c].boards, &info,
                                       cases[c].count, 0, 8000, 0, &rc);
        assert(rc == 0 && selected == 0);
        assert(h.reboot_calls == 0 && h.reset_calls == 0 && h.off_calls == 0);
        assert(h.info_snap_count == 1);
        assert(h.info_snaps[0].count == SET_MAIN_ROWS);
        settings_expect_system(&h.info_snaps[0], SET_ROW_INTERNAL, KSHIM_MENU_ROLE_INTERNAL, 0);
        settings_expect_system(&h.info_snaps[0], SET_ROW_ANDROID, KSHIM_MENU_ROLE_ANDROID, 1);
    }
}

/* A generic (non-system) caller that carries a known board other than the
 * session's effective board keeps its role, name and row but is presented as
 * unavailable exactly like a wrong-model Linux image: it is never returned and
 * never arms the countdown. A same-board, UNKNOWN-board or NULL-board generic
 * caller keeps its explicit payload, and a manual model switch turns a
 * same-board generic caller into the mismatched form. */
static void test_settings_generic_board_bypass(void)
{
    static const kshim_menu_role_t roles[2] = { KSHIM_MENU_ROLE_UNKNOWN, KSHIM_MENU_ROLE_ANDROID };
    static const enum device_board mismatched[2] = { DEVICE_BOARD_TB323FU, DEVICE_BOARD_UNKNOWN };
    static const enum device_board same[2] = { DEVICE_BOARD_TB322FC, DEVICE_BOARD_UNKNOWN };
    static const enum device_board unknown[2] = { DEVICE_BOARD_UNKNOWN, DEVICE_BOARD_UNKNOWN };
    struct device_info info = settings_info(DEVICE_BOARD_TB322FC);
    char generic_name[96];
    int rc;

    /* The generic caller keeps the caller manifest name on either page. */
    settings_setup(0, 2);
    snprintf(generic_name, sizeof(generic_name), "%s",
             kshim_menu_role_name(KSHIM_MENU_ROLE_UNKNOWN, h.payload_labels[0]));

    /* Generic caller on the main page (mask main) and on the advanced page
     * (advanced mask): the mismatched board stays unavailable, the name is
     * preserved and only the safe Android row returns caller 1, with no
     * automatic return past the 8 s timeout. */
    for (unsigned adv = 0; adv < 2U; ++adv) {
        uint64_t mask = adv ? (UINT64_C(1) << 0) : 0;
        settings_setup(0, 2);
        if (adv) {
            ui_at(5, SET_ROW_ADVANCED, 1); /* generic off main -> advanced entry row3 */
            ui_at(10, 1, 0);               /* advanced generic row1: no-op */
            ui_at(15, 5, 0);               /* advanced Back row5 -> home */
        } else {
            ui_at(5, 3, 0);                /* main generic row3: no-op */
        }
        ui_at(9000, SET_ROW_ANDROID, 0);   /* Android row0 exits after the timeout */
        size_t selected = settings_run(roles, mismatched, &info, 2, 0, 8000, mask, &rc);
        assert(rc == 0 && selected == 1);
        assert(h.now >= 9000);
        assert(h.countdown_started == 0);
        assert(h.reset_calls == 0 && h.off_calls == 0 && h.reboot_calls == 0);
        const info_snap_t *page = adv ? &h.info_snaps[1] : &h.info_snaps[0];
        size_t grow = adv ? 1U : 3U;
        /* main gains one generic row; advanced gains one after the diagnostic. */
        assert(page->count == (adv ? 6U : (size_t)SET_MAIN_ROWS + 1U));
        assert(page->roles[grow] == KSHIM_MENU_ROLE_UNKNOWN);
        assert(strcmp(page->names[grow], generic_name) == 0);
        assert(snap_action(page, grow) &&
               strcmp(snap_action(page, grow),
                      kshim_menu_text(KSHIM_TEXT_ACTION_UNAVAILABLE)) == 0);
        assert(snap_desc(page, grow) &&
               strcmp(snap_desc(page, grow), kshim_menu_text(KSHIM_TEXT_UNAVAILABLE)) == 0);
    }

    /* Positive: a same-board, an UNKNOWN-board and a NULL-board generic caller
     * all keep their explicit payload and return the original caller 0. */
    {
        const struct { const enum device_board *boards; } cases[] = {
            { same }, { unknown }, { NULL },
        };
        for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); ++c) {
            settings_setup(0, 2);
            ui_at(5, 3, 0); /* main generic row3 returns caller 0 */
            size_t selected = settings_run(roles, cases[c].boards, &info, 2, 0, 8000, 0, &rc);
            assert(rc == 0 && selected == 0);
            assert(h.info_snap_count == 1);
            assert(h.info_snaps[0].count == (size_t)SET_MAIN_ROWS + 1U);
            assert(strcmp(h.info_snaps[0].names[3], generic_name) == 0);
        }
    }

    /* A manual model switch turns the now-mismatched generic caller into an
     * unavailable row (the generic row shifts the settings entry down one). */
    settings_setup(0, 2);
    ui_at(5, 5, 1);                  /* home settings row (shifted by the generic row) */
    ui_at(10, 0, 0);                 /* settings -> device */
    ui_at(15, 2, 0);                 /* manual Gen5 */
    ui_at(20, SET_SETTINGS_BACK, 0); /* settings back -> home */
    ui_at(25, 3, 0);                 /* generic row now unavailable: no-op */
    ui_at(9000, SET_ROW_ANDROID, 0); /* Android row exits */
    size_t selected = settings_run(roles, same, &info, 2, 0, 8000, 0, &rc);
    assert(rc == 0 && selected == 1);
    assert(h.countdown_started == 0);
    assert(h.reboot_calls == 0 && h.reset_calls == 0 && h.off_calls == 0);
}

/* The read-only record is never mutated and every path keeps the original boot
 * FDT for PSCI. */
/* Real menu transitions and real two-slot writes; only block IO is simulated. */
static uint8_t menu_storage_bytes[65536];
static unsigned menu_storage_writes, menu_storage_flushes;
static int menu_storage_fail, menu_storage_flush_fail;
static int menu_storage_read(void *ctx, uint64_t lba, uint32_t count, void *out)
{
    (void)ctx;
    assert(lba + count <= 16);
    memcpy(out, menu_storage_bytes + lba * 4096, count * 4096);
    return 0;
}
static int menu_storage_write(void *ctx, uint64_t lba, uint32_t count, const void *in)
{
    (void)ctx;
    assert((lba == 1 || lba == 2) && count == 1);
    ++menu_storage_writes;
    if (menu_storage_fail) return KSHIM_STORAGE_IO;
    memcpy(menu_storage_bytes + lba * 4096, in, count * 4096);
    return 0;
}
static int menu_storage_flush(void *ctx)
{
    (void)ctx; ++menu_storage_flushes; return menu_storage_flush_fail ? KSHIM_STORAGE_IO : 0;
}
static void test_settings_persistent_menu(void)
{
    const char *names[] = {"Android", "Internal"};
    const kshim_menu_role_t roles[] = {KSHIM_MENU_ROLE_ANDROID, KSHIM_MENU_ROLE_INTERNAL};
    const enum device_board boards[] = {DEVICE_BOARD_UNKNOWN, DEVICE_BOARD_TB322FC};
    struct device_info info = settings_info(DEVICE_BOARD_TB322FC);
    for (unsigned source = KSHIM_SETTINGS_INTERNAL; source <= KSHIM_SETTINGS_TF; ++source) {
        memset(menu_storage_bytes, 0, sizeof(menu_storage_bytes));
        menu_storage_writes = menu_storage_flushes = 0; menu_storage_fail = menu_storage_flush_fail = 0;
        struct kshim_blockdev dev = {0};
        dev.block_size = 4096; dev.block_count = 16;
        dev.read = menu_storage_read; dev.write = menu_storage_write; dev.flush = menu_storage_flush;
        struct kshim_settings_session session = {0};
        session.source = (enum kshim_settings_source)source;
        struct kshim_config_store *store = &session.medium[source - 1].config;
        store->dev = &dev; store->ready = 1; store->board_id = DEVICE_BOARD_TB322FC;
        store->scratch = session.medium[source - 1].scratch;
        memset(store->partition.uuid, 1, 16); memset(store->installation_uuid, 2, 16);
        struct kshim_config_values values = {BOOT_DEVICE_TB322FC, BOOT_TARGET_INTERNAL, KSHIM_MENU_LANG_EN, 0, 0};
        assert(kshim_settings_save(&session, &values) == 0);
        kshim_platform_bind_settings(&session);
        struct menu_selection menu = {0};
        assert(menu_build(&menu,names,NULL,roles,&info,boards,2,0,0,1,1,1) == 0);
        assert(menu.boot.device_choice == BOOT_DEVICE_TB322FC);
        assert(menu.boot.default_target == BOOT_TARGET_INTERNAL);
        assert(kshim_menu_get_language() == KSHIM_MENU_LANG_EN);
        assert(menu.default_ok && menu.original_initial == 1);
        assert(menu_storage_writes == 1); /* Loading and rebuilding never save. */
        size_t selected = SIZE_MAX; const char *failure = NULL;
        menu.page = MENU_PAGE_DEVICE;
        assert(menu_activate(&menu,BOOT_DEVICE_AUTO,&selected,&failure) == 2);
        assert(session.values.device_choice == BOOT_DEVICE_AUTO && session.sequence == 2);
        menu.page = MENU_PAGE_LANGUAGE;
        assert(menu_activate(&menu,0,&selected,&failure) == 2);
        assert(session.values.language == KSHIM_MENU_LANG_ZH_CN && session.sequence == 3);
        menu.page = MENU_PAGE_DEFAULT;
        assert(menu_activate(&menu,0,&selected,&failure) == 2);
        assert(session.values.default_target == BOOT_TARGET_ANDROID && session.sequence == 4);
        unsigned writes = menu_storage_writes;
        menu.page = MENU_PAGE_DEFAULT;
        assert(menu_activate(&menu,0,&selected,&failure) == 2);
        assert(menu_storage_writes == writes); /* Selecting the current value does not wear storage. */
        menu_storage_fail = 1;
        menu.page = MENU_PAGE_DEVICE;
        assert(menu_activate(&menu,BOOT_DEVICE_TB323FU,&selected,&failure) == 0);
        assert(failure && strcmp(failure,kshim_menu_text(KSHIM_TEXT_SETTINGS_SAVE_FAILED)) == 0);
        assert(menu.boot.device_choice == BOOT_DEVICE_AUTO && session.sequence == 4);
        assert(menu_storage_flushes == 4);
        struct kshim_config_values saved; uint64_t sequence;
        assert(kshim_config_load(store,&saved,&sequence) == 0);
        assert(saved.device_choice == BOOT_DEVICE_AUTO && saved.default_target == BOOT_TARGET_ANDROID &&
               saved.language == KSHIM_MENU_LANG_ZH_CN && sequence == 4);
        /* A write can reach storage even when flush reports failure. Re-selecting
         * the cached old value must write it back, not incorrectly skip the save. */
        menu_storage_fail = 0; menu_storage_flush_fail = 1;
        menu.page = MENU_PAGE_DEVICE;
        assert(menu_activate(&menu,BOOT_DEVICE_TB323FU,&selected,&failure) == 0);
        assert(session.sequence == 4 && menu.boot.device_choice == BOOT_DEVICE_AUTO);
        assert(kshim_config_load(store,&saved,&sequence) == 0);
        assert(saved.device_choice == BOOT_DEVICE_TB323FU && sequence == 5);
        menu_storage_flush_fail = 0;
        menu.page = MENU_PAGE_DEVICE;
        assert(menu_activate(&menu,BOOT_DEVICE_AUTO,&selected,&failure) == 2);
        assert(kshim_config_load(store,&saved,&sequence) == 0);
        assert(saved.device_choice == BOOT_DEVICE_AUTO && sequence == 6 && session.sequence == 6);
        info.board = DEVICE_BOARD_TB323FU;
        memset(&menu,0,sizeof(menu));
        assert(menu_build(&menu,names,NULL,roles,&info,boards,2,0,0,1,1,1) == 0);
        assert(menu.boot.device_choice == BOOT_DEVICE_AUTO && menu.boot.default_target == BOOT_TARGET_MANIFEST);
        info.board = DEVICE_BOARD_TB322FC;
        kshim_platform_bind_settings(NULL);
    }
}

static void test_settings_record_pristine(void)
{
    static const kshim_menu_role_t roles[2] = { KSHIM_MENU_ROLE_ANDROID, KSHIM_MENU_ROLE_INTERNAL };
    static const enum device_board boards[2] = { DEVICE_BOARD_UNKNOWN, DEVICE_BOARD_TB322FC };
    struct device_info info = settings_info(DEVICE_BOARD_TB322FC);
    struct device_info before = info;
    int rc;

    settings_setup(0, 2);
    ui_at(5, SET_ROW_ANDROID, 1);
    size_t selected = settings_run(roles, boards, &info, 2, 0, 8000, 0, &rc);
    assert(rc == 0 && selected == 0);
    assert(memcmp(&before, &info, sizeof(info)) == 0);
    assert(h.psci_calls == 1U && h.psci_fdt == (const void *)(uintptr_t)boot_fdt());
    assert(h.reboot_calls == 0);
}

/* The Default page: a resolved default focuses its own selectable row; an
 * unavailable default row is never focused and confirming it never returns a
 * payload; a valid default set returns home focused on that row. */
static void test_settings_remember_last(void)
{
    const char *names[]={"Android","Internal"};
    const kshim_menu_role_t roles[]={KSHIM_MENU_ROLE_ANDROID,KSHIM_MENU_ROLE_INTERNAL};
    const enum device_board boards[]={DEVICE_BOARD_UNKNOWN,DEVICE_BOARD_TB322FC};
    struct device_info info=settings_info(DEVICE_BOARD_TB322FC);
    struct kshim_settings_session session={0};
    memset(menu_storage_bytes,0,sizeof(menu_storage_bytes));
    menu_storage_writes=menu_storage_flushes=0;menu_storage_fail=menu_storage_flush_fail=0;
    struct kshim_blockdev dev={0};dev.block_size=4096;dev.block_count=16;
    dev.read=menu_storage_read;dev.write=menu_storage_write;dev.flush=menu_storage_flush;
    session.source=KSHIM_SETTINGS_INTERNAL;
    struct kshim_config_store *store=&session.medium[0].config;
    store->dev=&dev;store->ready=1;store->board_id=DEVICE_BOARD_TB322FC;
    store->scratch=session.medium[0].scratch;
    memset(store->partition.uuid,1,16);memset(store->installation_uuid,2,16);
    struct kshim_config_values values={0,BOOT_TARGET_INTERNAL,0,0, 0};
    assert(kshim_settings_save(&session,&values)==0);
    kshim_platform_bind_settings(&session);
    struct menu_selection menu={0};size_t selected=99;const char *failure=NULL;
    assert(menu_build(&menu,names,NULL,roles,&info,boards,2,0,0,1,1,1)==0);
    assert(menu.pages[MENU_PAGE_DEFAULT].actions[3]==MENU_DEFAULT_SET);
    assert(menu.pages[MENU_PAGE_DEFAULT].payloads[3]==BOOT_TARGET_LAST);
    menu.page=MENU_PAGE_DEFAULT;
    assert(menu_activate(&menu,3,&selected,&failure)==2);
    assert(session.values.default_target==BOOT_TARGET_LAST && session.values.last_target==BOOT_TARGET_INTERNAL);
    assert(menu.default_ok && menu.original_initial==1 && settings_default_row(&menu)==3);
    assert(kshim_settings_note_boot(&session,BOOT_TARGET_ANDROID)==0);
    unsigned writes=menu_storage_writes;
    memset(&menu,0,sizeof(menu));
    assert(menu_build(&menu,names,NULL,roles,&info,boards,2,1,0,1,1,1)==0);
    assert(menu.default_ok && menu.original_initial==0 && menu.boot.last_target==BOOT_TARGET_ANDROID);
    assert(menu_storage_writes==writes); /* Rebuilding never records a boot. */
    menu.page=MENU_PAGE_LANGUAGE;
    assert(menu_activate(&menu,1,&selected,&failure)==2);
    assert(session.values.last_target==BOOT_TARGET_ANDROID && session.values.language==1);
    menu.page=MENU_PAGE_DEFAULT;
    assert(menu_activate(&menu,1,&selected,&failure)==2);
    assert(session.values.default_target==BOOT_TARGET_INTERNAL && session.values.last_target==BOOT_TARGET_ANDROID);
    assert(menu.original_initial==1);
    menu_storage_fail=1;menu.page=MENU_PAGE_DEFAULT;
    assert(menu_activate(&menu,3,&selected,&failure)==0 && failure);
    assert(menu.boot.default_target==BOOT_TARGET_INTERNAL && session.values.default_target==BOOT_TARGET_INTERNAL);
    menu_storage_fail=0;
    values=(struct kshim_config_values){0,BOOT_TARGET_LAST,0,BOOT_TARGET_TF, 0};
    assert(kshim_settings_save(&session,&values)==0);
    memset(&menu,0,sizeof(menu));
    assert(menu_build(&menu,names,NULL,roles,&info,boards,2,1,0,1,1,1)==0);
    assert(!menu.default_ok && menu.boot.default_target==BOOT_TARGET_LAST);
    kshim_platform_bind_settings(NULL);
}
static void test_settings_default_page(void)
{
    static const kshim_menu_role_t roles[2] = { KSHIM_MENU_ROLE_ANDROID, KSHIM_MENU_ROLE_INTERNAL };
    static const enum device_board boards[2] = { DEVICE_BOARD_UNKNOWN, DEVICE_BOARD_TB322FC };
    struct device_info info = settings_info(DEVICE_BOARD_TB322FC);
    int rc;

    /* Manifest default = Android (caller 0): Default page focuses row 0. */
    settings_setup(0, 2);
    ui_at(5, SET_ROW_SETTINGS, 1);   /* home -> settings */
    ui_at(10, 1, 0);                 /* settings -> default page */
    ui_at(15, 2, 0);                 /* confirm the unavailable TF row: no-op */
    ui_at(20, 1, 0);                 /* choose internal default -> home */
    ui_at(25, SET_ROW_ANDROID, 0);   /* Android payload */
    size_t selected = settings_run(roles, boards, &info, 2, 0, 8000, 0, &rc);
    assert(rc == 0 && selected == 0);
    assert(h.reset_calls == 0 && h.off_calls == 0 && h.reboot_calls == 0);
    assert(h.info_snap_count == 4); /* home, settings, default, home(after set) */
    const info_snap_t *def = &h.info_snaps[2];
    assert(def->count == 5);
    assert(def->initial == 0);
    settings_expect_default(def, 0, KSHIM_MENU_ROLE_ANDROID, 1);
    settings_expect_default(def, 1, KSHIM_MENU_ROLE_INTERNAL, 1);
    settings_expect_default(def, 2, KSHIM_MENU_ROLE_TF, 0);
    settings_expect_back(def, SET_DEFAULT_BACK);
    /* A valid default set returns home focused on the internal row. */
    const info_snap_t *home = &h.info_snaps[3];
    assert(home->count == SET_MAIN_ROWS);
    assert(home->initial == SET_ROW_INTERNAL);

    /* Manifest default = internal (caller 1): Default page focuses row 1. */
    settings_setup(0, 2);
    ui_at(5, SET_ROW_SETTINGS, 1);
    ui_at(10, 1, 0);                 /* default page */
    ui_at(15, SET_DEFAULT_BACK, 0);  /* default back -> settings */
    ui_at(20, SET_SETTINGS_BACK, 0); /* settings back -> home */
    ui_at(25, SET_ROW_ANDROID, 0);
    selected = settings_run(roles, boards, &info, 2, 1, 8000, 0, &rc);
    assert(rc == 0 && selected == 0);
    assert(h.info_snap_count == 5);
    assert(h.info_snaps[2].initial == SET_ROW_INTERNAL);
}

/* A device change that invalidates the default keeps the stored value (shown
 * on the settings page), focuses a selectable Default row and never re-arms
 * the countdown. */
static void test_settings_default_invalidated(void)
{
    static const kshim_menu_role_t roles[2] = { KSHIM_MENU_ROLE_INTERNAL, KSHIM_MENU_ROLE_ANDROID };
    static const enum device_board boards[2] = { DEVICE_BOARD_TB322FC, DEVICE_BOARD_UNKNOWN };
    struct device_info info = settings_info(DEVICE_BOARD_TB322FC);
    int rc;

    settings_setup(0, 2);
    ui_at(5, SET_ROW_SETTINGS, 1);   /* home -> settings */
    ui_at(10, 0, 0);                 /* settings -> device */
    ui_at(15, 2, 0);                 /* manual Gen5 invalidates the internal default */
    ui_at(20, 1, 0);                 /* settings -> default page */
    ui_at(25, SET_DEFAULT_BACK, 0);  /* default back -> settings */
    ui_at(30, SET_SETTINGS_BACK, 0); /* settings back -> home */
    ui_at(35, SET_ROW_ANDROID, 0);   /* Android payload */
    size_t selected = settings_run(roles, boards, &info, 2, 0, 8000, 0, &rc);
    assert(rc == 0 && selected == 1);
    assert(h.info_snap_count == 7);
    /* Settings page after the device change still shows the internal default. */
    const info_snap_t *settings_page = &h.info_snaps[3];
    const char *default_row = settings_page->names[1];
    assert(strstr(default_row, kshim_menu_role_name(KSHIM_MENU_ROLE_INTERNAL, NULL)) != NULL);
    assert(strstr(default_row, kshim_menu_text(KSHIM_TEXT_DEFAULT_LABEL)) != NULL);
    /* The Default page no longer focuses an unavailable row: internal is now
     * unavailable, so the focus falls on the first selectable row. */
    const info_snap_t *def = &h.info_snaps[4];
    assert(def->count == 5);
    settings_expect_default(def, 0, KSHIM_MENU_ROLE_ANDROID, 1);
    settings_expect_default(def, 1, KSHIM_MENU_ROLE_INTERNAL, 0);
    assert(def->initial == 0);
    /* The countdown was never re-armed by the device change. */
    assert(h.countdown_started == 1);
}

/* The new-API Back rows on the settings-family pages use the short label and
 * the parent-layer description. */
static void test_settings_back_labels(void)
{
    static const kshim_menu_role_t roles[2] = { KSHIM_MENU_ROLE_ANDROID, KSHIM_MENU_ROLE_INTERNAL };
    static const enum device_board boards[2] = { DEVICE_BOARD_UNKNOWN, DEVICE_BOARD_TB322FC };
    struct device_info info = settings_info(DEVICE_BOARD_TB322FC);
    int rc;

    settings_setup(0, 2);
    ui_at(5, SET_ROW_SETTINGS, 1);    /* home -> settings */
    ui_at(10, 0, 0);                  /* settings -> device */
    ui_at(15, SET_DEVICE_BACK, 0);    /* device back -> settings */
    ui_at(20, 1, 0);                  /* settings -> default */
    ui_at(25, SET_DEFAULT_BACK, 0);   /* default back -> settings */
    ui_at(30, 2, 0);                  /* settings -> language */
    ui_at(35, SET_LANGUAGE_BACK, 0);  /* language back -> settings */
    ui_at(40, SET_SETTINGS_BACK, 0);  /* settings back -> home */
    ui_at(45, SET_ROW_ANDROID, 0);
    size_t selected = settings_run(roles, boards, &info, 2, 0, 8000, 0, &rc);
    assert(rc == 0 && selected == 0);
    assert(h.info_snap_count == 9);
    settings_expect_back(&h.info_snaps[1], SET_SETTINGS_BACK); /* settings */
    settings_expect_back(&h.info_snaps[2], SET_DEVICE_BACK);   /* device */
    settings_expect_back(&h.info_snaps[4], SET_DEFAULT_BACK);  /* default */
    settings_expect_back(&h.info_snaps[6], SET_LANGUAGE_BACK); /* language */
}

/* A manual device choice and the resolved default survive a language rebuild
 * in both directions, and the menu returns to the Settings page each time. */
static void test_settings_language_rebuild(void)
{
    static const kshim_menu_role_t roles[2] = { KSHIM_MENU_ROLE_ANDROID, KSHIM_MENU_ROLE_INTERNAL };
    static const enum device_board boards[2] = { DEVICE_BOARD_UNKNOWN, DEVICE_BOARD_TB322FC };
    struct device_info info = settings_info(DEVICE_BOARD_TB322FC);
    char expected_device[96], expected_default[96];
    int rc;

    settings_setup(0, 2);
    ui_at(5, SET_ROW_SETTINGS, 1);    /* settings */
    ui_at(10, 0, 0);                  /* settings -> device */
    ui_at(15, 1, 0);                  /* manual Gen4 (keeps internal valid) */
    ui_at(20, 2, 0);                  /* settings -> language */
    ui_at(25, 1, 0);                  /* English -> settings */
    ui_at(30, 2, 0);                  /* settings -> language */
    ui_at(35, 0, 0);                  /* Chinese -> settings */
    ui_at(40, SET_SETTINGS_BACK, 0);  /* settings back -> home */
    ui_at(45, SET_ROW_ANDROID, 0);
    size_t selected = settings_run(roles, boards, &info, 2, 1, 8000, 0, &rc);
    assert(rc == 0 && selected == 0);
    assert(kshim_menu_get_language() == KSHIM_MENU_LANG_ZH_CN);
    assert(h.info_snap_count == 9);

    /* Both intermediate settings pages were rebuilt as the Settings page. */
    const info_snap_t *settings_en = &h.info_snaps[5];
    assert(settings_en->count == 4);
    assert(settings_en->roles[0] == KSHIM_MENU_ROLE_DEVICE);
    assert(settings_en->roles[1] == KSHIM_MENU_ROLE_DEFAULT);
    assert(settings_en->roles[2] == KSHIM_MENU_ROLE_LANGUAGE);
    assert(settings_en->roles[3] == KSHIM_MENU_ROLE_BACK);

    /* The final Chinese rebuild still carries the manual Gen4 choice and the
     * internal default. */
    const info_snap_t *settings_zh = &h.info_snaps[7];
    assert(settings_zh->count == 4);
    snprintf(expected_device, sizeof(expected_device),
             kshim_menu_text(KSHIM_TEXT_SETTINGS_DEVICE_MANUAL_FMT),
             kshim_menu_text(KSHIM_TEXT_DEVICE_TB322FC_NAME));
    assert(strcmp(settings_zh->names[0], expected_device) == 0);
    snprintf(expected_default, sizeof(expected_default),
             kshim_menu_text(KSHIM_TEXT_SETTINGS_DEFAULT_FMT),
             kshim_menu_role_name(KSHIM_MENU_ROLE_INTERNAL, NULL));
    assert(strcmp(settings_zh->names[1], expected_default) == 0);
}

/* Serial: a wrong-model manifest Linux image at the original initial index is
 * never returned by the 8 s timeout or by main-page Escape; only the explicit
 * Android choice exits. */
static void test_settings_serial_manifest_guard(void)
{
    static const kshim_menu_role_t roles[2] = { KSHIM_MENU_ROLE_INTERNAL, KSHIM_MENU_ROLE_ANDROID };
    static const enum device_board boards[2] = { DEVICE_BOARD_TB323FU, DEVICE_BOARD_TB322FC };
    struct device_info info = settings_info(DEVICE_BOARD_TB322FC);
    int rc;

    settings_setup(1, 2);
    char_at(5, 27);        /* main Escape: no resolved default, must not pick */
    text_at(9000, "1\r");  /* later: the real Android row exits */
    size_t selected = settings_run(roles, boards, &info, 2, 0, 8000, 0, &rc);
    assert(rc == 0 && selected == 1);
    assert(h.now >= 9000);
    assert(h.countdown_started == 0);
    assert(h.reboot_calls == 0 && h.reset_calls == 0 && h.off_calls == 0);
}

/* Serial: after setting the device to Gen5 the stale internal default is
 * invalidated; main-page Escape must not boot it and the later Android row
 * exits instead. */
static void test_settings_serial_stale_guard(void)
{
    static const kshim_menu_role_t roles[2] = { KSHIM_MENU_ROLE_INTERNAL, KSHIM_MENU_ROLE_ANDROID };
    static const enum device_board boards[2] = { DEVICE_BOARD_TB322FC, DEVICE_BOARD_UNKNOWN };
    struct device_info info = settings_info(DEVICE_BOARD_TB322FC);
    int rc;

    settings_setup(1, 2);
    text_at(5, "5\r");     /* main -> settings */
    text_at(20, "1\r");    /* settings -> device */
    text_at(35, "3\r");    /* choose Gen5 (invalidates the internal default) */
    char_at(50, 27);       /* Escape on settings -> home */
    char_at(100, 27);      /* Escape on home: no resolved default, must not pick */
    text_at(5000, "1\r");  /* the Android row exits */
    size_t selected = settings_run(roles, boards, &info, 2, 0, 8000, 0, &rc);
    assert(rc == 0 && selected == 1);
    assert(h.now >= 5000);
    assert(h.countdown_started == 0);
    assert(h.reset_calls == 0 && h.off_calls == 0 && h.reboot_calls == 0);
}

/* Representative cross-page navigation: dump every captured page so the parent
 * can diff the real stdout against the approved STAR preview. */
static void settings_dump_pages(void)
{
    for (size_t i = 0; i < h.info_snap_count; ++i) {
        const info_snap_t *snap = &h.info_snaps[i];
        printf("SETPAGE %zu count=%zu initial=%zu\n", i, snap->count, snap->initial);
        for (size_t r = 0; r < snap->count; ++r)
            printf("  row=%zu role=%d name=\"%s\" desc=\"%s\" action=\"%s\"\n",
                   r, (int)snap->roles[r], snap->names[r],
                   snap_desc(snap, r) ? snap_desc(snap, r) : "(null)",
                   snap_action(snap, r) ? snap_action(snap, r) : "(null)");
    }
}
static void test_settings_navigation_dump(void)
{
    static const kshim_menu_role_t roles[2] = { KSHIM_MENU_ROLE_ANDROID, KSHIM_MENU_ROLE_INTERNAL };
    static const enum device_board boards[2] = { DEVICE_BOARD_UNKNOWN, DEVICE_BOARD_TB322FC };
    struct device_info info = settings_info(DEVICE_BOARD_TB322FC);
    int rc;

    /* The DT-reported RAM is synthetic here, so hide it rather than let the
     * review dump present a fabricated value as device evidence. */
    info.ram_known = false;
    info.dt_reported_ram_bytes = 0;

    settings_setup(0, 2);
    ui_at(5, SET_ROW_INFO, 1);        /* activate Device info row5 */
    ui_at(10, SET_ROW_INFO, 0);       /* back at the same row5 -> home */
    ui_at(15, SET_ROW_ADVANCED, 1);   /* open advanced (main row3) */
    ui_at(20, 4, 0);                  /* advanced Back row4 -> home */
    ui_at(25, SET_ROW_SETTINGS, 1);   /* settings */
    ui_at(30, 0, 0);                  /* device */
    ui_at(35, SET_DEVICE_BACK, 0);    /* -> settings */
    ui_at(40, 1, 0);                  /* default */
    ui_at(45, SET_DEFAULT_BACK, 0);   /* -> settings */
    ui_at(50, 2, 0);                  /* language */
    ui_at(55, SET_LANGUAGE_BACK, 0);  /* -> settings */
    ui_at(60, SET_SETTINGS_BACK, 0);  /* -> home */
    ui_at(65, SET_ROW_ANDROID, 0);
    size_t selected = settings_run(roles, boards, &info, 2, 0, 8000, 0, &rc);
    assert(rc == 0 && selected == 0);
    printf("SETPAGE-BEGIN real new-API GUI menu output (synthetic caller data "
           "including one internal Linux payload for testing; not an actual "
           "packaged image or device capture)\n");
    settings_dump_pages();
}

#if !CONFIG_KSHIM_DEBUG_CONSOLE
/* Console-off gate through the new entry point: only a real Gen4 device whose
 * original initial row is Android may auto-return; Gen5 must not widen the
 * gate, a subpage rebuild failure must keep the interactive serial menu, and a
 * handoff failure still reports -2. */
static void test_settings_console_off_gate(void)
{
    static const kshim_menu_role_t roles[2] = { KSHIM_MENU_ROLE_UNKNOWN, KSHIM_MENU_ROLE_ANDROID };
    static const enum device_board boards[2] = { DEVICE_BOARD_TB322FC, DEVICE_BOARD_TB322FC };
    static const char *const names[3] = { "framebuffer", "lvgl_init", "first_entries" };

    for (unsigned kind = 0; kind < 3U; ++kind) {
        settings_setup(kind == 0, 2);
        h.info_fixture = settings_info(DEVICE_BOARD_TB322FC);
        if (kind == 1) h.ui_init_result = -1;
        if (kind == 2) h.entries_fail_at = 1;
        key_at(5, QCOM_KEYS_EVENT, QCOM_KEY_EVENT_PRESS, QCOM_KEY_ACTION_UP, 1);
        key_at(10, QCOM_KEYS_EVENT, QCOM_KEY_EVENT_PRESS, QCOM_KEY_ACTION_SELECT, 1);
        size_t selected = 12345;
        int rc = kshim_platform_select_localized_settings(boot_fdt(), h.names, NULL, roles,
                &h.info_fixture, boards, 2, 1, 20, 0, &selected);
        printf("settings_console_off case=%s rc=%d selected=%zu build=%u polls=%u reads=%u\n",
               names[kind], rc, selected, h.keys_build_calls, h.key_poll_calls,
               h.console_read_calls);
        assert(rc == 0 && selected == 1);
        assert(h.frames == 0 && h.now == 0);
        assert(h.keys_build_calls == (kind == 0 ? 0U : 1U));
        assert(h.key_poll_calls == 0 && h.console_read_calls == 0);
        assert(h.key_next == 0 && h.char_next == 0);
        assert(h.deinit_calls == 1 && h.handoff_calls == 1);
    }

    /* Real Gen5: the gate must not fire; the interactive serial path runs. */
    settings_setup(0, 2);
    h.info_fixture = settings_info(DEVICE_BOARD_TB323FU);
    h.ui_init_result = -1;
    key_at(5, QCOM_KEYS_EVENT, QCOM_KEY_EVENT_PRESS, QCOM_KEY_ACTION_SELECT, 1);
    {
        size_t selected = 12345;
        int rc = kshim_platform_select_localized_settings(boot_fdt(), h.names, NULL, roles,
                &h.info_fixture, boards, 2, 1, 20, 0, &selected);
        printf("settings_console_off case=board_g5 rc=%d selected=%zu build=%u polls=%u\n",
               rc, selected, h.keys_build_calls, h.key_poll_calls);
        assert(rc == 0 && selected == 1);
        assert(h.keys_build_calls == 2 && h.key_poll_calls > 0);
    }

    /* A subpage rebuild failure after the GUI was entered keeps the serial
     * menu instead of auto-returning. */
    {
        static const kshim_menu_role_t aroles[1] = { KSHIM_MENU_ROLE_ANDROID };
        static const enum device_board aboards[1] = { DEVICE_BOARD_TB322FC };
        int rc;
        settings_setup(0, 1);
        h.info_fixture = settings_info(DEVICE_BOARD_TB322FC);
        h.entries_fail_at = 2; /* home ok; the settings rebuild fails */
        ui_at(5, SET_ROW_SETTINGS, 1);
        text_at(200, "\x1b");  /* serial: settings back -> home */
        text_at(300, "1\r");   /* serial: Android row */
        size_t selected = settings_run(aroles, aboards, &h.info_fixture, 1, 0, 20, 0, &rc);
        printf("settings_console_off case=subpage rc=%d selected=%zu frames=%u entries=%u\n",
               rc, selected, h.frames, h.entries_calls);
        assert(rc == 0 && selected == 0);
        assert(h.frames > 0);
        assert(h.entries_calls == 2);
        assert(h.deinit_calls == 1 && h.handoff_calls == 1);
    }

    /* Handoff failure still returns -2 with the sentinel preserved. */
    {
        static const kshim_menu_role_t aroles[1] = { KSHIM_MENU_ROLE_ANDROID };
        static const enum device_board aboards[1] = { DEVICE_BOARD_TB322FC };
        int rc;
        settings_setup(0, 1);
        h.info_fixture = settings_info(DEVICE_BOARD_TB322FC);
        h.ui_init_result = -1;
        h.handoff_result = -1;
        size_t selected = 12345;
        rc = kshim_platform_select_localized_settings(boot_fdt(), h.names, NULL, aroles,
                &h.info_fixture, aboards, 1, 0, 20, 0, &selected);
        printf("settings_console_off case=handoff rc=%d selected=%zu\n", rc, selected);
        assert(rc == -2 && selected == 12345);
        assert(h.deinit_calls == 1 && h.handoff_calls == 1);
    }
}
#endif /* !CONFIG_KSHIM_DEBUG_CONSOLE */

/* Manifest diagnostic default. */
/* Only BOOT_TARGET_MANIFEST
 * may honor its explicitly indexed, unique, board-safe DIAGNOSTIC caller as the
 * current-session automatic default. The diagnostic stays a fixed row on the
 * advanced page and must never become a selectable Default-page choice. A valid
 * diagnostic starts the session on its own advanced row and the 8 s countdown
 * auto-selects the ORIGINAL caller index, never the page row. They drive the
 * real loop only through settings_setup / settings_run (the public
 * kshim_platform_select_localized_settings) and the existing snapshot capture;
 * no private resolver is called and no logic is mirrored. */

/* A valid manifest diagnostic default must auto-select at 8000 ms. The fixture
 * is {ANDROID, DIAGNOSTIC} with the diagnostic carrying the detected TB322FC
 * board, initial=1 (the diagnostic caller), mask=2 and an explicit Android
 * fallback scripted after the deadline. Pre-fix production finds no candidate
 * for the diagnostic caller, cancels the timeout and starts on the MAIN page, so
 * the 9000 ms fallback picks Android (caller 0) instead of the diagnostic
 * (caller 1). */
static void test_settings_manifest_diagnostic_default(void)
{
    static const kshim_menu_role_t roles[2] = {
        KSHIM_MENU_ROLE_ANDROID, KSHIM_MENU_ROLE_DIAGNOSTIC };
    static const enum device_board boards[2] = {
        DEVICE_BOARD_UNKNOWN, DEVICE_BOARD_TB322FC };
    struct device_info info = settings_info(DEVICE_BOARD_TB322FC);
    int rc;

    /* GUI: the valid diagnostic default fires before the 9000 ms fallback. */
    settings_setup(0, 2);
    ui_at(9000, SET_ROW_ANDROID, 1); /* old production: MAIN row0 -> Android */
    size_t selected = settings_run(roles, boards, &info, 2, 1, 8000, 2, &rc);
    assert(rc == 0 && selected == 1);
    assert(h.now >= 8000 && h.now < 9000);
    assert(h.reboot_calls == 0 && h.reset_calls == 0 && h.off_calls == 0);
    assert(h.countdown_first == 8000 && h.countdown_started == 1);
    assert(h.info_snap_count >= 1);
    const info_snap_t *adv = &h.info_snaps[0];
    assert(adv->count == 5 && adv->initial == 0);
    assert(adv->roles[0] == KSHIM_MENU_ROLE_DIAGNOSTIC);
    assert(strcmp(adv->names[0],
                  kshim_menu_role_name(KSHIM_MENU_ROLE_DIAGNOSTIC, NULL)) == 0);
    assert(snap_action(adv, 0) &&
           strcmp(snap_action(adv, 0), KSHIM_MENU_ACTION_BOOT) == 0);

    /* Serial: the 9000 ms fallback is never consumed; the timeout auto-selects
     * the original diagnostic caller index. */
    settings_setup(1, 2);
    text_at(9000, "1\r");
    selected = settings_run(roles, boards, &info, 2, 1, 8000, 2, &rc);
    assert(rc == 0 && selected == 1);
    assert(h.now >= 8000 && h.now < 9000);
    assert(h.char_next == 0); /* the late fallback was never read */
    assert(h.reboot_calls == 0 && h.reset_calls == 0 && h.off_calls == 0);
}

/* An explicit Android default overrides a valid manifest diagnostic. The same
 * fixture starts on the diagnostic advanced page; the user walks
 * advanced->home->settings->default, picks Android and only the late explicit
 * Android choice exits. Documented GUI page sequence (snapshot indexes):
 *   0 advanced(diagnostic) 1 home -> 2 settings -> 3 default -> 4 home(Android). */
static void test_settings_explicit_android_overrides_diagnostic(void)
{
    static const kshim_menu_role_t roles[2] = {
        KSHIM_MENU_ROLE_ANDROID, KSHIM_MENU_ROLE_DIAGNOSTIC };
    static const enum device_board boards[2] = {
        DEVICE_BOARD_UNKNOWN, DEVICE_BOARD_TB322FC };
    struct device_info info = settings_info(DEVICE_BOARD_TB322FC);
    int rc;

    settings_setup(0, 2);
    ui_at(5, 4, 1);                  /* advanced Back row4 -> home (interaction) */
    ui_at(10, SET_ROW_SETTINGS, 0);  /* home -> settings */
    ui_at(15, 1, 0);                 /* settings -> default page */
    ui_at(20, 0, 0);                 /* choose the Android default -> home */
    ui_at(9000, SET_ROW_ANDROID, 0); /* late explicit Android row exits */
    size_t selected = settings_run(roles, boards, &info, 2, 1, 8000, 2, &rc);
    assert(rc == 0 && selected == 0);
    assert(h.now >= 9000);           /* late exit proves no re-armed countdown */
    assert(h.countdown_started == 1);
    assert(h.reboot_calls == 0 && h.reset_calls == 0 && h.off_calls == 0);

    assert(h.info_snap_count == 5);
    assert(h.info_snaps[0].roles[0] == KSHIM_MENU_ROLE_DIAGNOSTIC);
    const info_snap_t *def = &h.info_snaps[3];
    assert(def->count == 5); /* Android / internal / TF / remember / back */
    settings_expect_default(def, 0, KSHIM_MENU_ROLE_ANDROID, 1);
    settings_expect_default(def, 1, KSHIM_MENU_ROLE_INTERNAL, 0);
    settings_expect_default(def, 2, KSHIM_MENU_ROLE_TF, 0);
    settings_expect_back(def, SET_DEFAULT_BACK);
    for (size_t i = 0; i < def->count; ++i)
        assert(def->roles[i] != KSHIM_MENU_ROLE_DIAGNOSTIC);
    /* The rebuilt home focuses the resolved Android default (caller0 row0). */
    const info_snap_t *home = &h.info_snaps[4];
    assert(home->count == SET_MAIN_ROWS && home->initial == SET_ROW_ANDROID);

    /* Serial equivalent: Escape the advanced page home, then numeric navigation
     * sets the Android default and Escape on home resolves it (caller 0). */
    settings_setup(1, 2);
    char_at(5, 27);       /* advanced Back -> home */
    text_at(20, "5\r");   /* home -> settings */
    text_at(35, "2\r");   /* settings -> default */
    text_at(50, "1\r");   /* choose Android default -> home */
    char_at(100, 27);     /* Escape on home resolves the Android default */
    selected = settings_run(roles, boards, &info, 2, 1, 8000, 2, &rc);
    assert(rc == 0 && selected == 0);
    assert(h.now >= 100);
    assert(h.countdown_started == 0);
    assert(h.reboot_calls == 0 && h.reset_calls == 0 && h.off_calls == 0);
}

/* No invalid diagnostic may auto-select. Every case must fall through to the
 * explicit Android row only after the 8000 ms deadline with no countdown: a
 * wrong board, an UNKNOWN-board diagnostic, a NULL board array, an UNKNOWN
 * detected board and a duplicated matching diagnostic. */
static void test_settings_invalid_diagnostic_no_default(void)
{
    static const kshim_menu_role_t roles2[2] = {
        KSHIM_MENU_ROLE_ANDROID, KSHIM_MENU_ROLE_DIAGNOSTIC };
    static const enum device_board wrong_boards[2] = {
        DEVICE_BOARD_UNKNOWN, DEVICE_BOARD_TB323FU };
    static const enum device_board unknown_boards[2] = {
        DEVICE_BOARD_UNKNOWN, DEVICE_BOARD_UNKNOWN };
    static const enum device_board matched_boards[2] = {
        DEVICE_BOARD_UNKNOWN, DEVICE_BOARD_TB322FC };
    static const kshim_menu_role_t roles3[3] = {
        KSHIM_MENU_ROLE_ANDROID, KSHIM_MENU_ROLE_DIAGNOSTIC, KSHIM_MENU_ROLE_DIAGNOSTIC };
    static const enum device_board dup_boards[3] = {
        DEVICE_BOARD_UNKNOWN, DEVICE_BOARD_TB322FC, DEVICE_BOARD_TB322FC };
    struct device_info detected = settings_info(DEVICE_BOARD_TB322FC);
    struct device_info detected_unknown = settings_info(DEVICE_BOARD_UNKNOWN);

    struct bad_case {
        const char *name;
        const kshim_menu_role_t *roles;
        const enum device_board *boards;
        const struct device_info *info;
        size_t count;
    };
    const struct bad_case cases[] = {
        { "wrong_board", roles2, wrong_boards, &detected, 2 },
        { "unknown_board", roles2, unknown_boards, &detected, 2 },
        { "null_boards", roles2, NULL, &detected, 2 },
        { "detected_unknown", roles2, matched_boards, &detected_unknown, 2 },
        { "duplicate_diagnostic", roles3, dup_boards, &detected, 3 },
    };

    for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); ++c) {
        int rc;
        settings_setup(1, cases[c].count);
        text_at(9000, "1\r"); /* explicit Android row after the deadline */
        size_t selected = settings_run(cases[c].roles, cases[c].boards, cases[c].info,
                                       cases[c].count, 1, 8000, 0, &rc);
        assert(rc == 0 && selected == 0);
        assert(h.now >= 9000);
        assert(h.countdown_started == 0);
        assert(h.reboot_calls == 0 && h.reset_calls == 0 && h.off_calls == 0);
    }

    /* GUI: with no valid default the session starts on the MAIN page; the
     * diagnostic is only reachable after opening the advanced page, where it
     * stays unavailable and never returns a payload. */
    settings_setup(0, 2);
    ui_at(5, SET_ROW_ADVANCED, 1);   /* open advanced */
    ui_at(10, 4, 0);                 /* advanced Back row4 -> home */
    ui_at(9000, SET_ROW_ANDROID, 0); /* explicit Android row exits */
    int rc;
    size_t selected = settings_run(roles2, wrong_boards, &detected, 2, 1, 8000, 0, &rc);
    assert(rc == 0 && selected == 0);
    assert(h.now >= 9000);
    assert(h.reboot_calls == 0 && h.reset_calls == 0 && h.off_calls == 0);
    assert(h.info_snap_count >= 2);
    assert(h.info_snaps[0].count == SET_MAIN_ROWS);
    const info_snap_t *adv = &h.info_snaps[1];
    assert(adv->count == 5 && adv->roles[0] == KSHIM_MENU_ROLE_DIAGNOSTIC);
    assert(snap_action(adv, 0) &&
           strcmp(snap_action(adv, 0), kshim_menu_text(KSHIM_TEXT_ACTION_UNAVAILABLE)) == 0);
}

/* An initial caller index at or beyond the count is rejected before any
 * peripheral or UI work and never produces a selected payload. */
static void test_settings_initial_out_of_range(void)
{
    static const kshim_menu_role_t roles[3] = {
        KSHIM_MENU_ROLE_ANDROID, KSHIM_MENU_ROLE_ANDROID, KSHIM_MENU_ROLE_ANDROID };
    static const enum device_board boards[3] = {
        DEVICE_BOARD_UNKNOWN, DEVICE_BOARD_UNKNOWN, DEVICE_BOARD_UNKNOWN };
    struct device_info info = settings_info(DEVICE_BOARD_TB322FC);
    static const struct { size_t count, initial; } cases[] = {
        { 1, 1 }, { 2, 2 }, { 2, 3 },
    };

    for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); ++c) {
        int rc;
        settings_setup(0, cases[c].count);
        size_t selected = settings_run(roles, boards, &info, cases[c].count,
                                       cases[c].initial, 8000, 0, &rc);
        assert(rc != 0);
        assert(selected == SIZE_MAX);
        assert(h.psci_calls == 0 && h.fb_calls == 0 && h.init_calls == 0);
        assert(h.steps == 0 && h.handoff_calls == 0 && h.deinit_calls == 0);
        assert(h.reboot_calls == 0 && h.reset_calls == 0 && h.off_calls == 0);
    }
}

/* Ordinary internal / TF automatic defaults keep working unchanged: the
 * manifest resolution still returns the original caller index after 8 s. This
 * protects the fixed system rows against the diagnostic exception altering the
 * normal path. Fixture {ANDROID, TF, INTERNAL} with correct boards; initial 1
 * resolves to the TF caller and initial 2 to the internal caller. */
static void test_settings_internal_tf_default_timeout(void)
{
    static const kshim_menu_role_t roles[3] = {
        KSHIM_MENU_ROLE_ANDROID, KSHIM_MENU_ROLE_TF, KSHIM_MENU_ROLE_INTERNAL };
    static const enum device_board boards[3] = {
        DEVICE_BOARD_UNKNOWN, DEVICE_BOARD_TB322FC, DEVICE_BOARD_TB322FC };
    struct device_info info = settings_info(DEVICE_BOARD_TB322FC);
    static const size_t initials[] = { 1, 2 };

    for (unsigned serial = 0; serial < 2; ++serial) {
        for (size_t k = 0; k < sizeof(initials) / sizeof(initials[0]); ++k) {
            int rc;
            settings_setup(serial, 3);
            size_t selected = settings_run(roles, boards, &info, 3, initials[k], 8000, 0, &rc);
            assert(rc == 0 && selected == initials[k]);
            /* The eight-second figure is an auto-select deadline, not an exact
             * harness tick count, so accept any point in the deadline window. */
            assert(h.now >= 8000 && h.now < 9000);
            assert(h.reboot_calls == 0 && h.reset_calls == 0 && h.off_calls == 0);
            if (!serial) {
                assert(h.countdown_first == 8000 && h.countdown_started == 1);
                assert(h.info_snap_count == 1);
                assert(h.info_snaps[0].count == SET_MAIN_ROWS);
            }
        }
    }
}

#endif /* KSHIM_SETTINGS_MENU_TESTS */

#if KSHIM_AB_MENU
static int ab_available,ab_status_rc,ab_select_rc,ab_safe=1;
static unsigned ab_selects;
static struct kshim_ab_status ab_status={0,{1,1}};
bool storage_settings_native_ab_supported(void) { return ab_available; }
int storage_settings_native_ab_status(struct kshim_ab_status *s) { *s=ab_status;return ab_status_rc; }
bool storage_settings_native_handoff_safe(void) { return ab_safe; }
int storage_settings_native_ab_select(unsigned target,unsigned current)
{ assert(target==1&&current==0);++ab_selects;return ab_select_rc; }
static void test_ab_confirmation(void)
{
    const char *names[]={"Android","Internal"};
    const kshim_menu_role_t roles[]={KSHIM_MENU_ROLE_ANDROID,KSHIM_MENU_ROLE_INTERNAL};
    const enum device_board boards[]={DEVICE_BOARD_UNKNOWN,DEVICE_BOARD_TB323FU};
    struct device_info info=settings_info(DEVICE_BOARD_TB323FU);
    for(unsigned trial=0;trial<3;++trial){
        settings_setup(0,2);kshim_platform_bind_settings(NULL);ab_available=1;ab_selects=0;
        ab_select_rc=trial?-1:0;ab_safe=trial!=2;
        h.ui_alive=true;h.psci_calls=1;h.psci_fdt=(const void *)(uintptr_t)boot_fdt();
        struct menu_selection menu={0};
        assert(menu_build(&menu,names,NULL,roles,&info,boards,2,0,0,1,1,1)==0);
        menu.psci_ready=1;menu.page=MENU_PAGE_SETTINGS;
        size_t row=0;while(menu.pages[menu.page].actions[row]!=MENU_AB){++row;assert(row<menu.pages[menu.page].count);}
        const char *failure=NULL;size_t selected=SIZE_MAX;
        assert(menu_activate(&menu,row,&selected,&failure)==2&&!ab_selects);
        assert(menu.page==MENU_PAGE_AB&&menu.pages[MENU_PAGE_AB].initial==2);
        assert(menu.pages[MENU_PAGE_AB].actions[0]==MENU_UNAVAILABLE);
        assert(menu_activate(&menu,1,&selected,&failure)==2&&!ab_selects);
        assert(menu.page==MENU_PAGE_AB_CONFIRM&&menu.pages[menu.page].initial==1);
        assert(menu_activate(&menu,1,&selected,&failure)==2&&!ab_selects); /* cancel */
        assert(menu_activate(&menu,1,&selected,&failure)==2&&!ab_selects);
        int result=menu_activate(&menu,0,&selected,&failure);
        assert(ab_selects==1);
        if(trial==1){assert(result==0&&!h.reset_calls&&!menu.ab_restart_required);}
        else {
            assert(result==2&&h.reset_calls==1&&menu.ab_restart_required);
            assert(menu_activate(&menu,0,&selected,&failure)==0&&selected==SIZE_MAX);
        }
    }
    ab_available=0;ab_safe=1;ab_select_rc=0;
}
static void test_ab_serial_restart_lock(void)
{
    const char *names[]={"Android","Internal"};
    const kshim_menu_role_t roles[]={KSHIM_MENU_ROLE_ANDROID,KSHIM_MENU_ROLE_INTERNAL};
    const enum device_board boards[]={DEVICE_BOARD_UNKNOWN,DEVICE_BOARD_TB323FU};
    struct device_info info=settings_info(DEVICE_BOARD_TB323FU);
    for(unsigned timeout=0;timeout<2;++timeout){
        settings_setup(1,2);kshim_platform_bind_settings(NULL);ab_available=1;
        h.psci_calls=1;h.psci_fdt=(const void *)(uintptr_t)boot_fdt();
        h.deinit_calls=h.handoff_calls=1;
        struct menu_selection menu={0};size_t selected=SIZE_MAX;
        assert(menu_build(&menu,names,NULL,roles,&info,boards,2,0,1,1,1,1)==0);
        menu.psci_ready=1;menu.ab_restart_required=1;
        assert(menu.default_ok);
        if(!timeout)text_at(0,"\x1b");
        size_t off=0;
        while(menu.pages[MENU_PAGE_MAIN].actions[off]!=MENU_OFF)++off;
        char input[16];snprintf(input,sizeof(input),"%u\r",(unsigned)off+1);
        text_at(20,input);capture_power_off=1;
        if(!setjmp(power_exit)){
            (void)serial_select(&menu,&selected);
            assert(!"restart lock returned a payload");
        }
        capture_power_off=0;
        assert(h.off_calls==1&&selected==SIZE_MAX);
    }
    ab_available=0;
}
#endif

int main(void)
{
#if CONFIG_KSHIM_MENU_LANGUAGE_SWITCH
    test_language_page_switch();
    test_language_page_navigation();
    test_language_localized_roles();
    test_language_capacity();
    test_language_ui_fallback();
#else
    test_build_keys_scope();
    test_described_payload_mapping();
    test_reject_count_50();
    test_ui_boundary_and_payload();
    test_ui_timeout_and_interaction();
    test_ui_init_failure_falls_back();
    test_serial_timeout_and_digits();
    test_serial_key_contract();
    test_serial_boundary();
#if CONFIG_KSHIM_MENU_POWER_ACTIONS
    test_power_actions();
    test_serial_key_power_actions();
    test_serial_numeric_power_preserves_key_focus();
#endif
#if CONFIG_KSHIM_MENU_ADVANCED_ACTIONS
    test_advanced_payload_mapping();
    test_advanced_default_timeout();
    test_advanced_back_cancels_timeout();
    test_advanced_reboot_failures();
#if CONFIG_KSHIM_MENU_POWER_ACTIONS
    test_advanced_power_failures();
#endif
    test_advanced_capacity();
    test_advanced_fallback_and_handoff();
#endif
#endif /* CONFIG_KSHIM_MENU_LANGUAGE_SWITCH */
    test_device_info_page();
    test_device_info_unknown_values();
    test_device_info_compatible_variants();
    test_device_info_capacity();
#if CONFIG_KSHIM_MENU_LANGUAGE_SWITCH
    test_device_info_language_refresh();
#endif
    test_console_off_initial_failures();
    test_console_off_initial_handoff_failure();
    test_console_off_success_gui();
    test_console_off_subpage_rebuild_serial();
    test_console_off_gate_negatives();
#ifdef KSHIM_SETTINGS_MENU_TESTS
    test_settings_home_default_android();
    test_settings_advanced_page();
    test_settings_order_and_caller_index();
    test_settings_board_filter_manual_auto();
    test_settings_no_linux_on_bad_board();
    test_settings_generic_board_bypass();
    test_settings_record_pristine();
    test_settings_persistent_menu();
    test_settings_remember_last();
    test_settings_default_page();
    test_settings_default_invalidated();
    test_settings_back_labels();
    test_settings_language_rebuild();
    test_settings_serial_manifest_guard();
    test_settings_serial_stale_guard();
    test_settings_navigation_dump();
    test_settings_manifest_diagnostic_default();
    test_settings_explicit_android_overrides_diagnostic();
    test_settings_invalid_diagnostic_no_default();
    test_settings_initial_out_of_range();
    test_settings_internal_tf_default_timeout();
#if !CONFIG_KSHIM_DEBUG_CONSOLE
    test_settings_console_off_gate();
#endif
#endif /* KSHIM_SETTINGS_MENU_TESTS */
#if KSHIM_AB_MENU
    test_ab_confirmation();
    test_ab_serial_restart_lock();
#endif
    puts("Real platform menu regression checks passed");
    return 0;
}
