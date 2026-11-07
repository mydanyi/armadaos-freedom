#pragma once

/* Runtime menu locale and text lookup for KShimC.
 *
 * This header owns the public surface of the menu locale module:
 *   - a session-only runtime language selection (no persistence, no redraw reset),
 *   - role -> localized name/description lookup with a caller supplied fallback,
 *   - one const text table keyed by kshim_menu_text_id,
 *   - the bilingual countdown formatter.
 *
 * The legacy compile-time strings in menu_text.h and platform.c stay
 * authoritative when CONFIG_KSHIM_MENU_LANGUAGE_SWITCH is off; the runtime
 * table only drives output when that switch is enabled. */

#include <stddef.h>
#include <stdint.h>

/* Firmware gets its locale from Kconfig, before any LVGL headers are read.
 * Generic host targets can still build without a generated board config. */
#if defined(__has_include)
#if __has_include(<config.h>)
#include <config.h>
#endif
#endif

/* ------------------------------------------------------------------ */
/* Runtime language                                                    */
/* ------------------------------------------------------------------ */

typedef enum kshim_menu_language_t {
    KSHIM_MENU_LANG_ZH_CN = 0,
    KSHIM_MENU_LANG_EN
} kshim_menu_language_t;

/* Returns the session language. It is initialised from
 * CONFIG_KSHIM_MENU_CHINESE and is never reset on a menu redraw. */
kshim_menu_language_t kshim_menu_get_language(void);

/* Selects the session language. Values outside the enum are rejected and the
 * current selection is left unchanged. Returns 0 on success, -1 if rejected. */
int kshim_menu_set_language(kshim_menu_language_t language);

/* ------------------------------------------------------------------ */
/* Menu roles                                                          */
/* ------------------------------------------------------------------ */

/* Fixed role sequence. UNKNOWN and BASE are never translated: callers keep
 * their own fallback label because an unrecognised stock OS is not
 * necessarily Android. Every other role maps to an explicit entry. */
typedef enum kshim_menu_role_t {
    KSHIM_MENU_ROLE_UNKNOWN = 0,
    KSHIM_MENU_ROLE_BASE,
    KSHIM_MENU_ROLE_ANDROID,
    KSHIM_MENU_ROLE_INTERNAL,
    KSHIM_MENU_ROLE_TF,
    KSHIM_MENU_ROLE_DIAGNOSTIC,
    KSHIM_MENU_ROLE_ADVANCED,
    KSHIM_MENU_ROLE_LANGUAGE,
    KSHIM_MENU_ROLE_RESTART,
    KSHIM_MENU_ROLE_OFF,
    KSHIM_MENU_ROLE_RECOVERY,
    KSHIM_MENU_ROLE_FASTBOOT,
    KSHIM_MENU_ROLE_EDL,
    KSHIM_MENU_ROLE_BACK,
    KSHIM_MENU_ROLE_LANG_ZH,
    KSHIM_MENU_ROLE_LANG_EN,
    /* Appended so every existing numeric role value is preserved. */
    KSHIM_MENU_ROLE_DEVICE_INFO,
    /* Session-settings rows, appended after DEVICE_INFO so the old value stays
     * the same. The three device-option roles exist so each option row keeps
     * its own localized label and can map to the shared device icon. */
    KSHIM_MENU_ROLE_SETTINGS,
    KSHIM_MENU_ROLE_DEVICE,
    KSHIM_MENU_ROLE_DEFAULT,
    KSHIM_MENU_ROLE_DEVICE_AUTO,
    KSHIM_MENU_ROLE_DEVICE_TB322FC,
    KSHIM_MENU_ROLE_DEVICE_TB323FU,
    /* Highest valid role; the role tables are sized with it + 1. */
    KSHIM_MENU_ROLE_MAX = KSHIM_MENU_ROLE_DEVICE_TB323FU
} kshim_menu_role_t;

/* Localized label for a known role. UNKNOWN and BASE return `fallback`
 * unchanged; a NULL fallback is returned as-is. Never matches on displayed
 * text. */
const char *kshim_menu_role_name(kshim_menu_role_t role, const char *fallback);

/* Localized one-line description for a known role. UNKNOWN returns `fallback`
 * unchanged. Descriptions localise independently of the label so the main
 * screen can describe its current role. */
const char *kshim_menu_role_description(kshim_menu_role_t role, const char *fallback);

/* Convenience macros: route through kshim_menu_role_name instead of
 * duplicating the localized strings. The fallback is unused for these known
 * roles, so NULL is passed. */
#define KSHIM_MENU_LABEL_ANDROID    kshim_menu_role_name(KSHIM_MENU_ROLE_ANDROID, NULL)
#define KSHIM_MENU_LABEL_INTERNAL   kshim_menu_role_name(KSHIM_MENU_ROLE_INTERNAL, NULL)
#define KSHIM_MENU_LABEL_TF         kshim_menu_role_name(KSHIM_MENU_ROLE_TF, NULL)
#define KSHIM_MENU_LABEL_DIAGNOSTIC kshim_menu_role_name(KSHIM_MENU_ROLE_DIAGNOSTIC, NULL)
#define KSHIM_MENU_LABEL_ADVANCED   kshim_menu_role_name(KSHIM_MENU_ROLE_ADVANCED, NULL)
#define KSHIM_MENU_LABEL_RECOVERY   kshim_menu_role_name(KSHIM_MENU_ROLE_RECOVERY, NULL)
#define KSHIM_MENU_LABEL_FASTBOOT   kshim_menu_role_name(KSHIM_MENU_ROLE_FASTBOOT, NULL)
#define KSHIM_MENU_LABEL_EDL        kshim_menu_role_name(KSHIM_MENU_ROLE_EDL, NULL)
#define KSHIM_MENU_LABEL_BACK       kshim_menu_role_name(KSHIM_MENU_ROLE_BACK, NULL)
#define KSHIM_MENU_LABEL_RESTART    kshim_menu_role_name(KSHIM_MENU_ROLE_RESTART, NULL)
#define KSHIM_MENU_LABEL_OFF        kshim_menu_role_name(KSHIM_MENU_ROLE_OFF, NULL)
#define KSHIM_MENU_LABEL_LANG_ZH    kshim_menu_role_name(KSHIM_MENU_ROLE_LANG_ZH, NULL)
#define KSHIM_MENU_LABEL_LANG_EN    kshim_menu_role_name(KSHIM_MENU_ROLE_LANG_EN, NULL)
#define KSHIM_MENU_LABEL_SETTINGS   kshim_menu_role_name(KSHIM_MENU_ROLE_SETTINGS, NULL)
#define KSHIM_MENU_LABEL_DEVICE     kshim_menu_role_name(KSHIM_MENU_ROLE_DEVICE, NULL)
#define KSHIM_MENU_LABEL_DEFAULT    kshim_menu_role_name(KSHIM_MENU_ROLE_DEFAULT, NULL)
#define KSHIM_MENU_LABEL_DEV_AUTO   kshim_menu_role_name(KSHIM_MENU_ROLE_DEVICE_AUTO, NULL)
#define KSHIM_MENU_LABEL_DEV_322    kshim_menu_role_name(KSHIM_MENU_ROLE_DEVICE_TB322FC, NULL)
#define KSHIM_MENU_LABEL_DEV_323    kshim_menu_role_name(KSHIM_MENU_ROLE_DEVICE_TB323FU, NULL)

/* ------------------------------------------------------------------ */
/* Text table                                                          */
/* ------------------------------------------------------------------ */

/* Every id resolves to a static string for the active language. The first
 * block mirrors menu_text.h, the second mirrors the KSHIM_STR_* keys in
 * platform.c, the rest covers the runtime language UI and reserved platform
 * keys. KSHIM_TEXT_COUNT is the invalid sentinel. */
typedef enum kshim_menu_text_id {
    /* menu_text.h strings */
    KSHIM_TEXT_TITLE = 0,
    KSHIM_TEXT_ACTION,
    KSHIM_TEXT_DETAIL,
    KSHIM_TEXT_POWEROFF,
    KSHIM_TEXT_HELP,
    KSHIM_TEXT_ACTION_BOOT,
    KSHIM_TEXT_ACTION_ENTER,
    KSHIM_TEXT_ACTION_RESTART,
    KSHIM_TEXT_ACTION_OFF,
    KSHIM_TEXT_ACTION_BACK,
    KSHIM_TEXT_DESC_BASE,
    KSHIM_TEXT_DESC_ANDROID,
    KSHIM_TEXT_DESC_GENERIC,
    KSHIM_TEXT_DESC_INTERNAL,
    KSHIM_TEXT_DESC_TF,
    KSHIM_TEXT_DESC_DIAGNOSTIC,
    KSHIM_TEXT_DESC_ADVANCED,
    KSHIM_TEXT_DESC_RESTART,
    KSHIM_TEXT_DESC_OFF,
    KSHIM_TEXT_DESC_RECOVERY,
    KSHIM_TEXT_DESC_FASTBOOT,
    KSHIM_TEXT_DESC_EDL,
    KSHIM_TEXT_DESC_BACK,
    KSHIM_TEXT_STATUS,

    /* platform.c KSHIM_STR_* strings */
    KSHIM_TEXT_MENU_TITLE,
    KSHIM_TEXT_MENU_PROMPT,
    KSHIM_TEXT_MENU_DEFAULT,
    KSHIM_TEXT_KEYS_HINT,
    KSHIM_TEXT_KEYS_UNAVAILABLE,
    KSHIM_TEXT_AUTO_CANCELLED,
    KSHIM_TEXT_POWER_FAILED,
    KSHIM_TEXT_MODE_FAILED,
    KSHIM_TEXT_RESTORE_FAILED,
    KSHIM_TEXT_HANDOFF_UNSAFE,
    KSHIM_TEXT_UNABLE_START,
    KSHIM_TEXT_RESTART,
    KSHIM_TEXT_POWER_OFF,
    KSHIM_TEXT_ADVANCED,
    KSHIM_TEXT_RECOVERY,
    KSHIM_TEXT_FASTBOOT,
    KSHIM_TEXT_EDL,
    KSHIM_TEXT_BACK,

    /* runtime role labels */
    KSHIM_TEXT_NAME_ANDROID,
    KSHIM_TEXT_NAME_INTERNAL,
    KSHIM_TEXT_NAME_TF,
    KSHIM_TEXT_NAME_DIAGNOSTIC,

    /* runtime language UI */
    KSHIM_TEXT_LANGUAGE_LABEL,
    KSHIM_TEXT_LANGUAGE_STATE,
    KSHIM_TEXT_LANGUAGE_DESC,
    KSHIM_TEXT_LANGUAGE_SELECT,
    KSHIM_TEXT_DESC_LANG_ZH,
    KSHIM_TEXT_DESC_LANG_EN,
    KSHIM_TEXT_LANG_ZH_NAME,
    KSHIM_TEXT_LANG_EN_NAME,
    KSHIM_TEXT_CREDIT,

    /* read-only Device info page */
    KSHIM_TEXT_INFO_LABEL,
    KSHIM_TEXT_INFO_DESC,
    KSHIM_TEXT_INFO_BOARD,
    KSHIM_TEXT_INFO_SOC,
    KSHIM_TEXT_INFO_MODEL,
    KSHIM_TEXT_INFO_COMPATIBLE,
    KSHIM_TEXT_INFO_RAM,
    KSHIM_TEXT_INFO_FRAMEBUFFER,
    KSHIM_TEXT_INFO_VERSION,
    KSHIM_TEXT_INFO_RAM_PREFIX,
    KSHIM_TEXT_UNKNOWN,

    /* Session-settings pages (appended; every earlier id keeps its value) */
    KSHIM_TEXT_SETTINGS_LABEL,
    KSHIM_TEXT_SETTINGS_DESC,
    KSHIM_TEXT_DEVICE_LABEL,
    KSHIM_TEXT_DEVICE_DESC,
    KSHIM_TEXT_DEFAULT_LABEL,
    KSHIM_TEXT_DEFAULT_DESC,
    KSHIM_TEXT_DEVICE_AUTO_NAME,
    KSHIM_TEXT_DEVICE_AUTO_DESC,
    KSHIM_TEXT_DEVICE_TB322FC_NAME,
    KSHIM_TEXT_DEVICE_TB322FC_DESC,
    KSHIM_TEXT_DEVICE_TB323FU_NAME,
    KSHIM_TEXT_DEVICE_TB323FU_DESC,
    KSHIM_TEXT_BOARD_TB322FC_NAME,
    KSHIM_TEXT_BOARD_TB323FU_NAME,
    /* printf format strings: the settings rows show their current value */
    KSHIM_TEXT_SETTINGS_DEVICE_AUTO_FMT,
    KSHIM_TEXT_SETTINGS_DEVICE_MANUAL_FMT,
    KSHIM_TEXT_SETTINGS_DEFAULT_FMT,
    KSHIM_TEXT_DEFAULT_MANIFEST,
    KSHIM_TEXT_DEFAULT_LAST,
    KSHIM_TEXT_DEFAULT_LAST_DESC,
    KSHIM_TEXT_UNAVAILABLE,
    /* Short button label for a row that cannot be used; the long
     * KSHIM_TEXT_UNAVAILABLE sentence stays the row description. */
    KSHIM_TEXT_ACTION_UNAVAILABLE,
    /* Parent-layer Back description for the new-API settings pages; the legacy
     * KSHIM_MENU_DESC_BACK ("back to the system boot options") stays with the
     * old page set. */
    KSHIM_TEXT_DESC_BACK_PARENT,

    KSHIM_TEXT_SETTINGS_INTERNAL,
    KSHIM_TEXT_SETTINGS_TF,
    KSHIM_TEXT_SETTINGS_SAVE_FAILED,
    KSHIM_TEXT_AB,
    KSHIM_TEXT_AB_DESC,
    KSHIM_TEXT_AB_A,
    KSHIM_TEXT_AB_B,
    KSHIM_TEXT_AB_CURRENT,
    KSHIM_TEXT_AB_CONFIRM,
    KSHIM_TEXT_AB_FAILED,
    KSHIM_TEXT_AB_RESTART,
    KSHIM_TEXT_COUNT
} kshim_menu_text_id;

/* Returns the static string for `id` in the active language. Unknown ids
 * return the empty string; valid ids always return a non-NULL string. */
const char *kshim_menu_text(kshim_menu_text_id id);

/* ------------------------------------------------------------------ */
/* Countdown formatting                                                */
/* ------------------------------------------------------------------ */

/* Writes a localized countdown into a caller-owned buffer with snprintf
 * semantics. A non-empty `name` yields "<n> 秒后启动<name>" / "Booting <name>
 * in <n> s"; NULL or empty yields "<n> 秒后自动启动" / "Booting in <n> s".
 * Returns whatever snprintf returns. No heap or shared scratch is used. */
int kshim_menu_format_countdown(char *buffer, size_t capacity, uint32_t seconds, const char *name);
