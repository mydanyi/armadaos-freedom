/* Runtime menu locale regression test for KShimC.
 *
 * Standalone main() using only assert/stdio/string plus the public
 * menu_locale.h surface and the menu_text.h display macros. No framework,
 * no mocks. Built with CONFIG_KSHIM_MENU_LANGUAGE_SWITCH=1 and
 * CONFIG_KSHIM_MENU_CHINESE=1 shared with src/core/menu_locale.c, so the
 * session language starts at ZH_CN and every text macro must follow the
 * runtime selection rather than the compile-time locale.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "menu_locale.h"
#include "menu_text.h"

#define KSHIM_TEST_GUARD 0xA5u

static int contains(const char *haystack, const char *needle)
{
    return strstr(haystack, needle) != NULL;
}

/* Every legal id resolves to a non-empty string; invalid ids are empty. */
static void check_text_ids(void)
{
    int id;

    for (id = 0; id < (int)KSHIM_TEXT_COUNT; ++id) {
        const char *text = kshim_menu_text((kshim_menu_text_id)id);
        assert(text != NULL);
        assert(text[0] != '\0');
    }
    assert(strcmp(kshim_menu_text((kshim_menu_text_id)-1), "") == 0);
    assert(strcmp(kshim_menu_text(KSHIM_TEXT_COUNT), "") == 0);
    assert(strcmp(kshim_menu_text((kshim_menu_text_id)(KSHIM_TEXT_COUNT + 1)), "") == 0);
}

/* Out-of-enum languages are rejected and never change the selection. */
static void check_language_rejection(void)
{
    kshim_menu_language_t before = kshim_menu_get_language();

    assert(kshim_menu_set_language((kshim_menu_language_t)-1) == -1);
    assert(kshim_menu_get_language() == before);

    assert(kshim_menu_set_language((kshim_menu_language_t)(KSHIM_MENU_LANG_EN + 1)) == -1);
    assert(kshim_menu_get_language() == before);
}

/* Compact manually specified role fixtures; deliberately not the whole
 * production text table. */
struct role_case {
    kshim_menu_role_t role;
    const char *zh_name;
    const char *en_name;
    const char *zh_desc_fragment;
    const char *en_desc_fragment;
};

static const struct role_case role_cases[] = {
    { KSHIM_MENU_ROLE_ANDROID,    "启动 Android",   "Start Android",     "原有",     "original Android" },
    { KSHIM_MENU_ROLE_INTERNAL,   "启动机身 ArmadaOS",  "Start internal ArmadaOS", "机身存储", "internal storage" },
    { KSHIM_MENU_ROLE_TF,         "启动 TF 卡 ArmadaOS", "Start TF card ArmadaOS",  "TF 卡",    "TF card" },
    { KSHIM_MENU_ROLE_DIAGNOSTIC, "Linux 诊断模式", "Linux diagnostics", "详细启动", "detailed Linux" },
    { KSHIM_MENU_ROLE_DEVICE_INFO, "设备信息",       "Device info",       "硬件信息", "hardware information" },
    /* Appended session-settings roles: every one keeps its own localized
     * label/description and the earlier numeric values are unchanged. */
    { KSHIM_MENU_ROLE_SETTINGS,       "设置",              "Settings",
      "各项设置", "launcher settings" },
    { KSHIM_MENU_ROLE_DEVICE,         "机型选择",          "Device model",
      "机型",     "model used" },
    { KSHIM_MENU_ROLE_DEFAULT,        "默认启动项",         "Default boot entry",
      "倒计时", "countdown" },
    { KSHIM_MENU_ROLE_DEVICE_AUTO,    "自动识别",           "Auto detect",
      "检测",     "detected" },
    { KSHIM_MENU_ROLE_DEVICE_TB322FC, "Y700 四代（TB322FC）", "Y700 Gen4 (TB322FC)",
      "TB322FC", "TB322FC" },
    { KSHIM_MENU_ROLE_DEVICE_TB323FU, "Y700 五代（TB323FU）", "Y700 Gen5 (TB323FU)",
      "TB323FU", "TB323FU" },
};

#define ROLE_CASE_COUNT (sizeof role_cases / sizeof role_cases[0])

/* Verify the explicit role labels/descriptions and record the returned name
 * pointers for the caller's cross-language comparison. */
static void check_roles_in_language(int chinese, const char **names_out)
{
    size_t i;

    for (i = 0; i < ROLE_CASE_COUNT; ++i) {
        const struct role_case *rc = &role_cases[i];
        const char *name = kshim_menu_role_name(rc->role, NULL);
        const char *desc = kshim_menu_role_description(rc->role, NULL);

        assert(name != NULL);
        assert(desc != NULL);
        if (chinese) {
            assert(strcmp(name, rc->zh_name) == 0);
            assert(contains(desc, rc->zh_desc_fragment));
        } else {
            assert(strcmp(name, rc->en_name) == 0);
            assert(contains(desc, rc->en_desc_fragment));
        }
        names_out[i] = name;
    }
}

/* UNKNOWN/BASE/out-of-range roles must hand back the caller's own pointers
 * verbatim: no inference from displayed label text. */
static void check_role_fallbacks(void)
{
    char fallback_name[] = "Android 原厂";
    char fallback_desc[] = "fallback description text";

    assert(kshim_menu_role_name(KSHIM_MENU_ROLE_UNKNOWN, fallback_name) == fallback_name);
    assert(kshim_menu_role_description(KSHIM_MENU_ROLE_UNKNOWN, fallback_desc) == fallback_desc);

    assert(kshim_menu_role_name(KSHIM_MENU_ROLE_UNKNOWN, NULL) == NULL);
    assert(kshim_menu_role_description(KSHIM_MENU_ROLE_UNKNOWN, NULL) == NULL);

    assert(kshim_menu_role_name(KSHIM_MENU_ROLE_BASE, fallback_name) == fallback_name);

    assert(kshim_menu_role_name((kshim_menu_role_t)-1, fallback_name) == fallback_name);
    assert(kshim_menu_role_description((kshim_menu_role_t)-1, fallback_desc) == fallback_desc);
    /* KSHIM_MENU_ROLE_MAX is now the last valid role, so MAX + 1 is the first
     * out-of-range value the fallback contract must reject. */
    assert(kshim_menu_role_name(KSHIM_MENU_ROLE_BOOT_SLOT, fallback_name) == fallback_name);
    assert(kshim_menu_role_description(KSHIM_MENU_ROLE_BOOT_SLOT, fallback_desc) == fallback_desc);
    assert(kshim_menu_role_name((kshim_menu_role_t)(KSHIM_MENU_ROLE_MAX + 1), fallback_name) == fallback_name);
    assert(kshim_menu_role_description((kshim_menu_role_t)(KSHIM_MENU_ROLE_MAX + 1), fallback_desc) == fallback_desc);
}

/* The appended roles must not have shifted any earlier numeric value. */
static void check_role_values(void)
{
    assert(KSHIM_MENU_ROLE_UNKNOWN == 0);
    assert(KSHIM_MENU_ROLE_BASE == 1);
    assert(KSHIM_MENU_ROLE_ANDROID == 2);
    assert(KSHIM_MENU_ROLE_INTERNAL == 3);
    assert(KSHIM_MENU_ROLE_TF == 4);
    assert(KSHIM_MENU_ROLE_DIAGNOSTIC == 5);
    assert(KSHIM_MENU_ROLE_ADVANCED == 6);
    assert(KSHIM_MENU_ROLE_LANGUAGE == 7);
    assert(KSHIM_MENU_ROLE_RESTART == 8);
    assert(KSHIM_MENU_ROLE_OFF == 9);
    assert(KSHIM_MENU_ROLE_RECOVERY == 10);
    assert(KSHIM_MENU_ROLE_FASTBOOT == 11);
    assert(KSHIM_MENU_ROLE_EDL == 12);
    assert(KSHIM_MENU_ROLE_BACK == 13);
    assert(KSHIM_MENU_ROLE_LANG_ZH == 14);
    assert(KSHIM_MENU_ROLE_LANG_EN == 15);
    assert(KSHIM_MENU_ROLE_DEVICE_INFO == 16);
    assert(KSHIM_MENU_ROLE_SETTINGS == 17);
    assert(KSHIM_MENU_ROLE_DEVICE == 18);
    assert(KSHIM_MENU_ROLE_DEFAULT == 19);
    assert(KSHIM_MENU_ROLE_DEVICE_AUTO == 20);
    assert(KSHIM_MENU_ROLE_DEVICE_TB322FC == 21);
    assert(KSHIM_MENU_ROLE_DEVICE_TB323FU == 22);
    assert(KSHIM_MENU_ROLE_BOOT_SLOT == 23);
    assert(KSHIM_MENU_ROLE_MAX == KSHIM_MENU_ROLE_BOOT_SLOT);
}

/* The changed/added session-settings strings are non-empty in both languages
 * and keep their exact visible text; the returned pointers are recorded so the
 * caller can prove the two languages actually differ. */
static void check_settings_texts(int chinese, const char **out)
{
    static const char *const zh[] = {
        "默认启动项", "自动识别", "Y700 四代（TB322FC）", "Y700 五代（TB323FU）",
        "镜像默认", "机型选择：%s", "默认启动项：%s", "返回上一级。",
    };
    static const char *const en[] = {
        "Default boot entry", "Auto detect", "Y700 Gen4 (TB322FC)", "Y700 Gen5 (TB323FU)",
        "Image default", "Device model: %s", "Default boot entry: %s",
        "Return to the previous menu.",
    };
    const kshim_menu_text_id ids[8] = {
        KSHIM_TEXT_DEFAULT_LABEL, KSHIM_TEXT_DEVICE_AUTO_NAME,
        KSHIM_TEXT_DEVICE_TB322FC_NAME, KSHIM_TEXT_DEVICE_TB323FU_NAME,
        KSHIM_TEXT_DEFAULT_MANIFEST, KSHIM_TEXT_SETTINGS_DEVICE_MANUAL_FMT,
        KSHIM_TEXT_SETTINGS_DEFAULT_FMT, KSHIM_TEXT_DESC_BACK_PARENT,
    };
    size_t i;

    for (i = 0; i < 8; ++i) {
        const char *text = kshim_menu_text(ids[i]);
        assert(text != NULL && text[0] != '\0');
        assert(strcmp(text, chinese ? zh[i] : en[i]) == 0);
        out[i] = text;
    }
}

/* Countdown output plus the snprintf short-buffer contract. `expected` is the
 * full manually specified visible line, never derived from the SUT. */
static void check_countdown(const char *expected, const char *name)
{
    size_t full = strlen(expected);
    char buf[128];
    unsigned char guarded[64];
    unsigned char zeroed[16];
    const size_t cap = 4;
    char *dst;
    int rc;
    size_t i;

    rc = kshim_menu_format_countdown(buf, sizeof buf, 3, name);
    assert(rc == (int)full);
    assert(strcmp(buf, expected) == 0);

    /* Truncated write: guards untouched, NUL at capacity-1, prefix correct,
     * and the return value is the full would-have-written length. */
    memset(guarded, KSHIM_TEST_GUARD, sizeof guarded);
    dst = (char *)&guarded[2];
    rc = kshim_menu_format_countdown(dst, cap, 3, name);
    assert(rc == (int)full);
    assert(dst[cap - 1] == '\0');
    assert(strncmp(dst, expected, cap - 1) == 0);
    assert(guarded[0] == KSHIM_TEST_GUARD);
    assert(guarded[1] == KSHIM_TEST_GUARD);
    for (i = 2 + cap; i < sizeof guarded; ++i) {
        assert(guarded[i] == KSHIM_TEST_GUARD);
    }

    /* capacity 0 with a non-NULL buffer: nothing written, full length back. */
    memset(zeroed, KSHIM_TEST_GUARD, sizeof zeroed);
    rc = kshim_menu_format_countdown((char *)zeroed, 0, 3, name);
    assert(rc == (int)full);
    for (i = 0; i < sizeof zeroed; ++i) {
        assert(zeroed[i] == KSHIM_TEST_GUARD);
    }
}

int main(void)
{
    const char *zh_names[ROLE_CASE_COUNT];
    const char *en_names[ROLE_CASE_COUNT];
    const char *zh_settings[8];
    const char *en_settings[8];
    size_t i;

    /* 1. Initial runtime state must be Chinese (CONFIG_KSHIM_MENU_CHINESE=1). */
    assert(kshim_menu_get_language() == KSHIM_MENU_LANG_ZH_CN);
    assert(strcmp(KSHIM_MENU_TEXT_TITLE("fallback"), "KShimC 启动器") == 0);
    assert(contains(KSHIM_MENU_TEXT_HELP, "音量"));

    check_text_ids();
    check_language_rejection();
    check_countdown("3 秒后启动ArmadaOS", "ArmadaOS");
    check_countdown("3 秒后自动启动", NULL);
    check_countdown("3 秒后自动启动", "");
    check_roles_in_language(1, zh_names);
    check_role_fallbacks();
    check_role_values();
    check_settings_texts(1, zh_settings);

    /* 2. Switch to English: getter and every runtime macro must follow. */
    assert(kshim_menu_set_language(KSHIM_MENU_LANG_EN) == 0);
    assert(kshim_menu_get_language() == KSHIM_MENU_LANG_EN);
    assert(strcmp(KSHIM_MENU_TEXT_TITLE("fallback"), "KShimC Launcher") == 0);
    assert(contains(KSHIM_MENU_TEXT_HELP, "Volume"));
    assert(contains(KSHIM_MENU_TEXT_HELP, "Power"));
    assert(contains(KSHIM_MENU_TEXT_STATUS, "Select a system"));
    assert(contains(KSHIM_MENU_TEXT_CREDIT, "Danyi"));
    assert(contains(KSHIM_MENU_TEXT_CREDIT, "Free to use"));
    assert(strcmp(KSHIM_MENU_TEXT_LANGUAGE_STATE, "English") == 0);

    check_text_ids();
    check_language_rejection();
    check_countdown("Booting ArmadaOS in 3 s", "ArmadaOS");
    check_countdown("Booting in 3 s", NULL);
    check_countdown("Booting in 3 s", "");
    check_roles_in_language(0, en_names);
    check_role_fallbacks();
    check_settings_texts(0, en_settings);

    /* 2. The explicit role labels must actually differ across languages. */
    for (i = 0; i < ROLE_CASE_COUNT; ++i) {
        assert(strcmp(zh_names[i], en_names[i]) != 0);
    }
    /* 2. The changed settings strings differ across languages too. */
    for (i = 0; i < 8; ++i) {
        assert(strcmp(zh_settings[i], en_settings[i]) != 0);
    }

    /* 1. Switch back to Chinese and confirm the macros are restored. */
    assert(kshim_menu_set_language(KSHIM_MENU_LANG_ZH_CN) == 0);
    assert(kshim_menu_get_language() == KSHIM_MENU_LANG_ZH_CN);
    assert(strcmp(KSHIM_MENU_TEXT_TITLE("fallback"), "KShimC 启动器") == 0);
    assert(contains(KSHIM_MENU_TEXT_HELP, "音量"));

    printf("menu_locale runtime regression: ALL TESTS PASSED\n");
    return 0;
}
