/* Runtime menu locale and text table.
 *
 * A single const table holds both languages; selection is a static session
 * language. With CONFIG_KSHIM_MENU_LANGUAGE_SWITCH the table follows the
 * runtime language, otherwise it falls back to the compile-time locale so the
 * legacy strings stay exactly compatible. */

#include <stdio.h>

#include "menu_locale.h"
#include "storage_probe.h"
#if defined(CONFIG_KSHIM_UFS_READ_PROBE) && CONFIG_KSHIM_UFS_READ_PROBE
#include "storage_ufs_read_native.h"
#endif

#if defined(CONFIG_KSHIM_MENU_LANGUAGE_SWITCH) && CONFIG_KSHIM_MENU_LANGUAGE_SWITCH
#define KSHIM_MENU_RUNTIME_TEXT 1
#else
#define KSHIM_MENU_RUNTIME_TEXT 0
#endif

#if defined(CONFIG_KSHIM_MENU_CHINESE) && CONFIG_KSHIM_MENU_CHINESE
#define KSHIM_MENU_LEGACY_LANGUAGE KSHIM_MENU_LANG_ZH_CN
#else
#define KSHIM_MENU_LEGACY_LANGUAGE KSHIM_MENU_LANG_EN
#endif

/* Session language: static, initialised from config, never reset on redraw. */
static kshim_menu_language_t g_kshim_menu_language = KSHIM_MENU_LEGACY_LANGUAGE;

/* Active language for output. */
static kshim_menu_language_t kshim_menu_active_language(void)
{
#if KSHIM_MENU_RUNTIME_TEXT
    return g_kshim_menu_language;
#else
    return (kshim_menu_language_t)KSHIM_MENU_LEGACY_LANGUAGE;
#endif
}

/* Language x id table. Chinese preserves the exact existing text. */
static const char *const kshim_menu_text_table[2][KSHIM_TEXT_COUNT] = {
    [KSHIM_MENU_LANG_ZH_CN] = {
        [KSHIM_TEXT_TITLE]          = "KShimC 启动器",
        [KSHIM_TEXT_ACTION]         = "启动",
        [KSHIM_TEXT_DETAIL]         = "选择系统或操作。\n音量键选择，电源键确认",
        [KSHIM_TEXT_POWEROFF]       = "关机",
        [KSHIM_TEXT_HELP]           = "音量＋／－切换 · 电源键确认",
        [KSHIM_TEXT_ACTION_BOOT]    = "启动",
        [KSHIM_TEXT_ACTION_ENTER]   = "进入",
        [KSHIM_TEXT_ACTION_RESTART] = "重启",
        [KSHIM_TEXT_ACTION_OFF]     = "关机",
        [KSHIM_TEXT_ACTION_BACK]    = "返回",
        [KSHIM_TEXT_DESC_BASE]      = "启动设备原有系统。",
        [KSHIM_TEXT_DESC_ANDROID]   = "启动设备原有的 Android 系统。",
        [KSHIM_TEXT_DESC_GENERIC]   = "启动所选系统。",
        [KSHIM_TEXT_DESC_INTERNAL]  = "启动安装在机身存储中的 ArmadaOS。",
        [KSHIM_TEXT_DESC_TF]        = "启动安装在 TF 卡中的 ArmadaOS。",
        [KSHIM_TEXT_DESC_DIAGNOSTIC] = "显示 Linux 的详细启动信息，用于排查系统启动问题。",
        [KSHIM_TEXT_DESC_ADVANCED]  = "查看支持的高级启动模式",
        [KSHIM_TEXT_DESC_RESTART]   = "重新启动设备。",
        [KSHIM_TEXT_DESC_OFF]       = "关闭设备电源。",
        [KSHIM_TEXT_DESC_RECOVERY]  = "进入设备原有的 Android 恢复模式，用于系统维护和恢复。",
        [KSHIM_TEXT_DESC_FASTBOOT]  = "进入 Fastboot 模式，通过 USB 连接电脑进行维护或刷写。",
        [KSHIM_TEXT_DESC_EDL]       = "进入高通 9008 下载模式，配合电脑端工具进行备份、恢复或刷写。",
        [KSHIM_TEXT_DESC_BACK]      = "返回系统启动选项。",
        [KSHIM_TEXT_STATUS]         = "选择要启动的系统",

        [KSHIM_TEXT_MENU_TITLE]     = "KShimC 启动器",
        [KSHIM_TEXT_MENU_PROMPT]    = "输入序号并按回车确认：",
        [KSHIM_TEXT_MENU_DEFAULT]   = " [默认]",
        [KSHIM_TEXT_KEYS_HINT]      = "音量键选择，电源键启动",
        [KSHIM_TEXT_KEYS_UNAVAILABLE] = "音量键暂不可用",
        [KSHIM_TEXT_AUTO_CANCELLED] = "已取消自动启动，请手动选择",
        [KSHIM_TEXT_POWER_FAILED]   = "电源操作失败",
        [KSHIM_TEXT_MODE_FAILED]    = "切换启动模式失败",
        [KSHIM_TEXT_RESTORE_FAILED] = "切换启动模式失败，恢复原设置失败",
        [KSHIM_TEXT_HANDOFF_UNSAFE] = "启动交接不安全，已中止启动。",
        [KSHIM_TEXT_UNABLE_START]   = "无法启动所选系统。",
        [KSHIM_TEXT_RESTART]        = "重启",
        [KSHIM_TEXT_POWER_OFF]      = "关机",
        [KSHIM_TEXT_ADVANCED]       = "高级启动模式",
        [KSHIM_TEXT_RECOVERY]       = "Recovery 恢复模式",
        [KSHIM_TEXT_FASTBOOT]       = "Fastboot 模式",
        [KSHIM_TEXT_EDL]            = "9008 下载模式",
        [KSHIM_TEXT_BACK]           = "返回主菜单",

        [KSHIM_TEXT_NAME_ANDROID]     = "启动 Android",
        [KSHIM_TEXT_NAME_INTERNAL]    = "启动机身 ArmadaOS",
        [KSHIM_TEXT_NAME_TF]          = "启动 TF 卡 ArmadaOS",
        [KSHIM_TEXT_NAME_DIAGNOSTIC]  = "Linux 诊断模式",

        [KSHIM_TEXT_LANGUAGE_LABEL]   = "语言 / Language",
        [KSHIM_TEXT_LANGUAGE_STATE]   = "简体中文",
        [KSHIM_TEXT_LANGUAGE_DESC]    = "选择启动器的显示语言。",
        [KSHIM_TEXT_LANGUAGE_SELECT]  = "选择",
        [KSHIM_TEXT_DESC_LANG_ZH]     = "使用简体中文菜单。",
        [KSHIM_TEXT_DESC_LANG_EN]     = "使用英文菜单。",
        [KSHIM_TEXT_LANG_ZH_NAME]     = "简体中文",
        [KSHIM_TEXT_LANG_EN_NAME]     = "English",
        [KSHIM_TEXT_CREDIT]           = "适配维护：B站「单翼」｜免费使用，谨防收费诈骗。",

        [KSHIM_TEXT_INFO_LABEL]       = "设备信息",
        [KSHIM_TEXT_INFO_DESC]        = "查看设备硬件信息",
        [KSHIM_TEXT_INFO_BOARD]       = "机型",
        [KSHIM_TEXT_INFO_SOC]         = "芯片",
        [KSHIM_TEXT_INFO_MODEL]       = "DT 型号",
        [KSHIM_TEXT_INFO_COMPATIBLE]  = "DT 兼容列表",
        [KSHIM_TEXT_INFO_RAM]         = "内存",
        [KSHIM_TEXT_INFO_FRAMEBUFFER] = "帧缓冲",
        [KSHIM_TEXT_INFO_VERSION]     = "启动器版本",
        [KSHIM_TEXT_INFO_RAM_PREFIX]  = "DT 报告",
        [KSHIM_TEXT_UNKNOWN]          = "未知",

        [KSHIM_TEXT_SETTINGS_LABEL]   = "设置",
        [KSHIM_TEXT_SETTINGS_DESC]    = "设置仅在本次启动期间有效。",
        [KSHIM_TEXT_SETTINGS_INTERNAL] = "设置会保存到机身存储。",
        [KSHIM_TEXT_SETTINGS_TF] = "设置会保存到 TF 卡。",
        [KSHIM_TEXT_SETTINGS_SAVE_FAILED] = "无法确认保存。",
        [KSHIM_TEXT_AB] = "A/B槽位切换",
        [KSHIM_TEXT_AB_DESC] = "设备具有 A、B 两个启动槽位，启动器与 Android 共用这套槽位。\n\n切换槽位会改变设备使用的引导文件，A 槽和 B 槽并不分别代表 Android 和 Linux。\n\n进入后可查看槽位状态并选择目标槽位。确认切换后，设备将重启。切换不会复制系统或清空个人文件，目标槽位需要具备可用的启动内容。",
        [KSHIM_TEXT_AB_A] = "切换到A槽",
        [KSHIM_TEXT_AB_B] = "切换到B槽",
        [KSHIM_TEXT_AB_CURRENT] = "设备当前使用 %s，无需切换。",
        [KSHIM_TEXT_AB_CURRENT_A] = "A槽（当前）",
        [KSHIM_TEXT_AB_CURRENT_B] = "B槽（当前）",
        [KSHIM_TEXT_AB_UNAVAILABLE] = "该槽位未通过启动检查，暂时无法切换。",
        [KSHIM_TEXT_AB_CONFIRM] = "确认后，设备将切换到 %s并立即重启，使用该槽位中的引导文件启动。\n\n启动器与 Android 共用所选槽位。具体进入哪个系统，取决于该槽位的启动内容和启动菜单。\n\n此操作不会复制系统、恢复备份或清空个人文件。",
        [KSHIM_TEXT_AB_FAILED] = "槽位检查或切换失败。",
        [KSHIM_TEXT_AB_RESTART] = "需要重新启动设备。",
        [KSHIM_TEXT_DEVICE_LABEL]     = "机型选择",
        [KSHIM_TEXT_DEVICE_DESC]      = "选择启动时使用的机型。选择“自动识别”时，由启动器识别设备。",
        [KSHIM_TEXT_DEFAULT_LABEL]    = "默认启动项",
        [KSHIM_TEXT_DEFAULT_DESC]     = "选择倒计时结束后自动启动的系统，也可以选择记住上一次启动的系统。",
        [KSHIM_TEXT_DEVICE_AUTO_NAME] = "自动识别",
        [KSHIM_TEXT_DEVICE_AUTO_DESC] = "使用启动器检测到的机型。通常保持此选项即可。",
        [KSHIM_TEXT_DEVICE_TB322FC_NAME] = "Y700 四代（TB322FC）",
        [KSHIM_TEXT_DEVICE_TB322FC_DESC] = "使用 Y700 四代 TB322FC 对应的启动配置。\n\n自动识别不正确时，可在这里手动选择。请按照设备的实际机型选择。",
        [KSHIM_TEXT_DEVICE_TB323FU_NAME] = "Y700 五代（TB323FU）",
        [KSHIM_TEXT_DEVICE_TB323FU_DESC] = "使用 Y700 五代 TB323FU 对应的启动配置。\n\n自动识别不正确时，可在这里手动选择。请按照设备的实际机型选择。",
        [KSHIM_TEXT_BOARD_TB322FC_NAME]  = "Y700 四代 TB322FC",
        [KSHIM_TEXT_BOARD_TB323FU_NAME]  = "Y700 五代 TB323FU",
        [KSHIM_TEXT_SETTINGS_DEVICE_AUTO_FMT]   = "机型选择：自动识别（%s）",
        [KSHIM_TEXT_SETTINGS_DEVICE_MANUAL_FMT] = "机型选择：%s",
        [KSHIM_TEXT_SETTINGS_DEFAULT_FMT]       = "默认启动项：%s",
        [KSHIM_TEXT_DEFAULT_MANIFEST] = "镜像默认",
        [KSHIM_TEXT_DEFAULT_LAST] = "记住上一次启动的系统",
        [KSHIM_TEXT_DEFAULT_LAST_DESC] = "下次默认启动上一次选择的系统。",
        [KSHIM_TEXT_UNAVAILABLE]      = "镜像未提供对应启动内容。",
        [KSHIM_TEXT_ACTION_UNAVAILABLE] = "不可用",
        [KSHIM_TEXT_DESC_BACK_PARENT] = "返回上一级。",
        [KSHIM_TEXT_SYSTEM_ANDROID] = "Android",
        [KSHIM_TEXT_SYSTEM_INTERNAL] = "机身 ArmadaOS",
        [KSHIM_TEXT_SYSTEM_TF] = "TF 卡 ArmadaOS",
        [KSHIM_TEXT_INFO_SLOT] = "当前A/B槽位",
        [KSHIM_TEXT_SLOT_A] = "A槽",
        [KSHIM_TEXT_SLOT_B] = "B槽",
        [KSHIM_TEXT_SLOT_STATE_FMT] = "当前槽位：%s",
        [KSHIM_TEXT_SLOT_TARGET_FMT] = "目标槽位：%s",
        [KSHIM_TEXT_AB_UNAVAILABLE_A] = "A槽（不可用）",
        [KSHIM_TEXT_AB_UNAVAILABLE_B] = "B槽（不可用）",
        [KSHIM_TEXT_AB_TARGET_DESC] = "将设备使用的启动槽位从 %s切换到 %s。\n\n启动器与 Android 共用这套槽位。切换后使用 %s中的引导文件，能够启动哪些系统，取决于该槽位中的启动内容。\n\n此操作不会复制系统、恢复备份或清空个人文件。选择后会进入确认页面，确认切换后设备将重启。",
        [KSHIM_TEXT_AB_CONFIRM_A] = "确认切换到A槽？",
        [KSHIM_TEXT_AB_CONFIRM_B] = "确认切换到B槽？",
        [KSHIM_TEXT_AB_CONFIRM_BACK_DESC] = "返回槽位选择页，保持当前槽位不变。",
        [KSHIM_TEXT_AB_BACK] = "返回高级启动模式",
        [KSHIM_TEXT_AB_BACK_DESC] = "返回高级启动模式选项。",
        [KSHIM_TEXT_ACTION_SWITCH_RESTART] = "切换并重启",
        [KSHIM_TEXT_SETTINGS_HOME_DESC] = "调整启动器的各项设置",
        [KSHIM_TEXT_SETTINGS_LANGUAGE_FMT] = "语言 / Language：%s",
        [KSHIM_TEXT_DEVICE_AUTO_FMT] = "自动识别（%s）",
        [KSHIM_TEXT_DEVICE_UNKNOWN] = "未知机型",
        [KSHIM_TEXT_BACK_SETTINGS] = "返回设置",
        [KSHIM_TEXT_BACK_SETTINGS_DESC] = "返回启动器设置。",

    },
    [KSHIM_MENU_LANG_EN] = {
        [KSHIM_TEXT_TITLE]          = "KShimC Launcher",
        [KSHIM_TEXT_ACTION]         = "Boot",
        [KSHIM_TEXT_DETAIL]         = "Select an image.\nPress Boot to continue.",
        [KSHIM_TEXT_POWEROFF]       = "Power off",
        [KSHIM_TEXT_HELP]           = "Volume keys to select, Power to confirm",
        [KSHIM_TEXT_ACTION_BOOT]    = "Boot",
        [KSHIM_TEXT_ACTION_ENTER]   = "Enter",
        [KSHIM_TEXT_ACTION_RESTART] = "Restart",
        [KSHIM_TEXT_ACTION_OFF]     = "Power off",
        [KSHIM_TEXT_ACTION_BACK]    = "Back",
        [KSHIM_TEXT_DESC_BASE]      = "Start the device's original system.",
        [KSHIM_TEXT_DESC_ANDROID]   = "Start the device's original Android system.",
        [KSHIM_TEXT_DESC_GENERIC]   = "Start the selected system.",
        [KSHIM_TEXT_DESC_INTERNAL]  = "Start ArmadaOS installed on internal storage.",
        [KSHIM_TEXT_DESC_TF]        = "Start ArmadaOS installed on the TF card.",
        [KSHIM_TEXT_DESC_DIAGNOSTIC] = "Show detailed Linux boot output to investigate startup problems.",
        [KSHIM_TEXT_DESC_ADVANCED]  = "View supported advanced boot modes.",
        [KSHIM_TEXT_DESC_RESTART]   = "Restart the device.",
        [KSHIM_TEXT_DESC_OFF]       = "Turn off the device.",
        [KSHIM_TEXT_DESC_RECOVERY]  = "Enter the device's original Android recovery mode for maintenance and recovery.",
        [KSHIM_TEXT_DESC_FASTBOOT]  = "Enter Fastboot mode and connect to a computer over USB for maintenance or flashing.",
        [KSHIM_TEXT_DESC_EDL]       = "Enter Qualcomm 9008 download mode for backup, recovery, or flashing with a computer.",
        [KSHIM_TEXT_DESC_BACK]      = "Return to the system boot options.",
        [KSHIM_TEXT_STATUS]         = "Select a system to boot",

        [KSHIM_TEXT_MENU_TITLE]     = "KShim boot menu",
        [KSHIM_TEXT_MENU_PROMPT]    = "Select kernel and press Enter: ",
        [KSHIM_TEXT_MENU_DEFAULT]   = " [default]",
        [KSHIM_TEXT_KEYS_HINT]      = "Use vol keys, power to boot",
        [KSHIM_TEXT_KEYS_UNAVAILABLE] = "Volume keys unavailable",
        [KSHIM_TEXT_AUTO_CANCELLED] = "Auto boot cancelled; choose an entry",
        [KSHIM_TEXT_POWER_FAILED]   = "Power action failed",
        [KSHIM_TEXT_MODE_FAILED]    = "Boot mode switch failed",
        [KSHIM_TEXT_RESTORE_FAILED] = "Boot mode switch failed; restoring the original settings failed",
        [KSHIM_TEXT_HANDOFF_UNSAFE] = "Unsafe boot handoff; boot aborted.",
        [KSHIM_TEXT_UNABLE_START]   = "Unable to start the selected system.",
        [KSHIM_TEXT_RESTART]        = "Restart",
        [KSHIM_TEXT_POWER_OFF]      = "Power off",
        [KSHIM_TEXT_ADVANCED]       = "Advanced boot modes",
        [KSHIM_TEXT_RECOVERY]       = "Enter Recovery",
        [KSHIM_TEXT_FASTBOOT]       = "Enter Fastboot",
        [KSHIM_TEXT_EDL]            = "Enter 9008",
        [KSHIM_TEXT_BACK]           = "Back to main menu",

        [KSHIM_TEXT_NAME_ANDROID]     = "Start Android",
        [KSHIM_TEXT_NAME_INTERNAL]    = "Start internal ArmadaOS",
        [KSHIM_TEXT_NAME_TF]          = "Start TF card ArmadaOS",
        [KSHIM_TEXT_NAME_DIAGNOSTIC]  = "Linux diagnostics",

        [KSHIM_TEXT_LANGUAGE_LABEL]   = "语言 / Language",
        [KSHIM_TEXT_LANGUAGE_STATE]   = "English",
        [KSHIM_TEXT_LANGUAGE_DESC]    = "Choose the launcher display language.",
        [KSHIM_TEXT_LANGUAGE_SELECT]  = "Select",
        [KSHIM_TEXT_DESC_LANG_ZH]     = "Use Simplified Chinese menus.",
        [KSHIM_TEXT_DESC_LANG_EN]     = "Use English menus.",
        [KSHIM_TEXT_LANG_ZH_NAME]     = "简体中文",
        [KSHIM_TEXT_LANG_EN_NAME]     = "English",
        [KSHIM_TEXT_CREDIT]           = "Adapted by Danyi on Bilibili · Free to use. Beware of paid scams.",

        [KSHIM_TEXT_INFO_LABEL]       = "Device info",
        [KSHIM_TEXT_INFO_DESC]        = "View device hardware information.",
        [KSHIM_TEXT_INFO_BOARD]       = "Device model",
        [KSHIM_TEXT_INFO_SOC]         = "Chip",
        [KSHIM_TEXT_INFO_MODEL]       = "DT model",
        [KSHIM_TEXT_INFO_COMPATIBLE]  = "DT compatible",
        [KSHIM_TEXT_INFO_RAM]         = "Memory",
        [KSHIM_TEXT_INFO_FRAMEBUFFER] = "Framebuffer",
        [KSHIM_TEXT_INFO_VERSION]     = "Launcher version",
        [KSHIM_TEXT_INFO_RAM_PREFIX]  = "DT reported",
        [KSHIM_TEXT_UNKNOWN]          = "Unknown",

        [KSHIM_TEXT_SETTINGS_LABEL]   = "Settings",
        [KSHIM_TEXT_SETTINGS_DESC]    = "Settings apply to this boot only.",
        [KSHIM_TEXT_SETTINGS_INTERNAL] = "Settings are saved to internal storage.",
        [KSHIM_TEXT_SETTINGS_TF] = "Settings are saved to the TF card.",
        [KSHIM_TEXT_SETTINGS_SAVE_FAILED] = "Could not confirm save.",
        [KSHIM_TEXT_AB] = "A/B slot switching",
        [KSHIM_TEXT_AB_DESC] = "The device has two boot slots, A and B. The launcher and Android share these slots.\n\nSwitching slots changes which boot files the device uses. A and B do not separately represent Android and Linux.\n\nOpen this page to view slot status and choose a target. Confirming the switch restarts the device. It does not copy a system or erase personal files. The target slot needs usable boot content.",
        [KSHIM_TEXT_AB_A] = "Switch to slot A",
        [KSHIM_TEXT_AB_B] = "Switch to slot B",
        [KSHIM_TEXT_AB_CURRENT] = "The device currently uses %s. No switch is needed.",
        [KSHIM_TEXT_AB_CURRENT_A] = "Slot A (current)",
        [KSHIM_TEXT_AB_CURRENT_B] = "Slot B (current)",
        [KSHIM_TEXT_AB_UNAVAILABLE] = "This slot did not pass the boot checks and cannot be selected.",
        [KSHIM_TEXT_AB_CONFIRM] = "Confirming switches the device to %s and restarts immediately using its boot files.\n\nThe launcher and Android share the selected slot. The system started depends on that slot's boot content and boot menu.\n\nThis does not copy a system, restore a backup, or erase personal files.",
        [KSHIM_TEXT_AB_FAILED] = "Slot check or switch failed.",
        [KSHIM_TEXT_AB_RESTART] = "Restart the device to continue.",
        [KSHIM_TEXT_DEVICE_LABEL]     = "Device model",
        [KSHIM_TEXT_DEVICE_DESC]      = "Choose the model used for booting. Auto detect uses the model detected by the launcher.",
        [KSHIM_TEXT_DEFAULT_LABEL]    = "Default boot entry",
        [KSHIM_TEXT_DEFAULT_DESC]     = "Choose the system to start when the countdown ends, or remember the last system started.",
        [KSHIM_TEXT_DEVICE_AUTO_NAME] = "Auto detect",
        [KSHIM_TEXT_DEVICE_AUTO_DESC] = "Use the model detected by the launcher. Normally, keep this selected.",
        [KSHIM_TEXT_DEVICE_TB322FC_NAME] = "Y700 Gen4 (TB322FC)",
        [KSHIM_TEXT_DEVICE_TB322FC_DESC] = "Use the boot configuration for Y700 Gen4 TB322FC.\n\nIf automatic detection is incorrect, select the actual model of your device here.",
        [KSHIM_TEXT_DEVICE_TB323FU_NAME] = "Y700 Gen5 (TB323FU)",
        [KSHIM_TEXT_DEVICE_TB323FU_DESC] = "Use the boot configuration for Y700 Gen5 TB323FU.\n\nIf automatic detection is incorrect, select the actual model of your device here.",
        [KSHIM_TEXT_BOARD_TB322FC_NAME]  = "Y700 Gen4 TB322FC",
        [KSHIM_TEXT_BOARD_TB323FU_NAME]  = "Y700 Gen5 TB323FU",
        [KSHIM_TEXT_SETTINGS_DEVICE_AUTO_FMT]   = "Device model: Auto detect (%s)",
        [KSHIM_TEXT_SETTINGS_DEVICE_MANUAL_FMT] = "Device model: %s",
        [KSHIM_TEXT_SETTINGS_DEFAULT_FMT]       = "Default boot entry: %s",
        [KSHIM_TEXT_DEFAULT_MANIFEST] = "Image default",
        [KSHIM_TEXT_DEFAULT_LAST] = "Remember last booted system",
        [KSHIM_TEXT_DEFAULT_LAST_DESC] = "Next time, boot the system selected last.",
        [KSHIM_TEXT_UNAVAILABLE]      = "No matching image content is provided.",
        [KSHIM_TEXT_ACTION_UNAVAILABLE] = "Unavailable",
        [KSHIM_TEXT_DESC_BACK_PARENT] = "Return to the previous menu.",
        [KSHIM_TEXT_SYSTEM_ANDROID] = "Android",
        [KSHIM_TEXT_SYSTEM_INTERNAL] = "Internal ArmadaOS",
        [KSHIM_TEXT_SYSTEM_TF] = "TF card ArmadaOS",
        [KSHIM_TEXT_INFO_SLOT] = "Current A/B slot",
        [KSHIM_TEXT_SLOT_A] = "slot A",
        [KSHIM_TEXT_SLOT_B] = "slot B",
        [KSHIM_TEXT_SLOT_STATE_FMT] = "Current slot: %s",
        [KSHIM_TEXT_SLOT_TARGET_FMT] = "Target slot: %s",
        [KSHIM_TEXT_AB_UNAVAILABLE_A] = "Slot A (unavailable)",
        [KSHIM_TEXT_AB_UNAVAILABLE_B] = "Slot B (unavailable)",
        [KSHIM_TEXT_AB_TARGET_DESC] = "Switch the device's boot slot from %s to %s.\n\nThe launcher and Android share these slots. The boot files in %s determine which systems can start.\n\nThis does not copy a system, restore a backup, or erase personal files. Selecting this opens a confirmation page; confirming restarts the device.",
        [KSHIM_TEXT_AB_CONFIRM_A] = "Confirm switch to slot A?",
        [KSHIM_TEXT_AB_CONFIRM_B] = "Confirm switch to slot B?",
        [KSHIM_TEXT_AB_CONFIRM_BACK_DESC] = "Return to slot selection without changing the current slot.",
        [KSHIM_TEXT_AB_BACK] = "Back to advanced boot modes",
        [KSHIM_TEXT_AB_BACK_DESC] = "Return to the advanced boot mode options.",
        [KSHIM_TEXT_ACTION_SWITCH_RESTART] = "Switch and restart",
        [KSHIM_TEXT_SETTINGS_HOME_DESC] = "Adjust the launcher settings.",
        [KSHIM_TEXT_SETTINGS_LANGUAGE_FMT] = "Language: %s",
        [KSHIM_TEXT_DEVICE_AUTO_FMT] = "Auto detect (%s)",
        [KSHIM_TEXT_DEVICE_UNKNOWN] = "Unknown model",
        [KSHIM_TEXT_BACK_SETTINGS] = "Back to settings",
        [KSHIM_TEXT_BACK_SETTINGS_DESC] = "Return to the launcher settings.",

    },
};

/* Role -> name id; KSHIM_TEXT_COUNT means "use the caller fallback". */
static const kshim_menu_text_id kshim_menu_role_name_table[KSHIM_MENU_ROLE_MAX + 1] = {
    [KSHIM_MENU_ROLE_UNKNOWN]    = KSHIM_TEXT_COUNT,
    [KSHIM_MENU_ROLE_BASE]       = KSHIM_TEXT_COUNT,
    [KSHIM_MENU_ROLE_ANDROID]    = KSHIM_TEXT_NAME_ANDROID,
    [KSHIM_MENU_ROLE_INTERNAL]   = KSHIM_TEXT_NAME_INTERNAL,
    [KSHIM_MENU_ROLE_TF]         = KSHIM_TEXT_NAME_TF,
    [KSHIM_MENU_ROLE_DIAGNOSTIC] = KSHIM_TEXT_NAME_DIAGNOSTIC,
    [KSHIM_MENU_ROLE_ADVANCED]   = KSHIM_TEXT_ADVANCED,
    [KSHIM_MENU_ROLE_LANGUAGE]   = KSHIM_TEXT_LANGUAGE_LABEL,
    [KSHIM_MENU_ROLE_RESTART]    = KSHIM_TEXT_RESTART,
    [KSHIM_MENU_ROLE_OFF]        = KSHIM_TEXT_POWER_OFF,
    [KSHIM_MENU_ROLE_RECOVERY]   = KSHIM_TEXT_RECOVERY,
    [KSHIM_MENU_ROLE_FASTBOOT]   = KSHIM_TEXT_FASTBOOT,
    [KSHIM_MENU_ROLE_EDL]        = KSHIM_TEXT_EDL,
    [KSHIM_MENU_ROLE_BACK]       = KSHIM_TEXT_BACK,
    [KSHIM_MENU_ROLE_LANG_ZH]    = KSHIM_TEXT_LANG_ZH_NAME,
    [KSHIM_MENU_ROLE_LANG_EN]    = KSHIM_TEXT_LANG_EN_NAME,
    [KSHIM_MENU_ROLE_DEVICE_INFO] = KSHIM_TEXT_INFO_LABEL,
    [KSHIM_MENU_ROLE_SETTINGS]   = KSHIM_TEXT_SETTINGS_LABEL,
    [KSHIM_MENU_ROLE_DEVICE]     = KSHIM_TEXT_DEVICE_LABEL,
    [KSHIM_MENU_ROLE_DEFAULT]    = KSHIM_TEXT_DEFAULT_LABEL,
    [KSHIM_MENU_ROLE_DEVICE_AUTO] = KSHIM_TEXT_DEVICE_AUTO_NAME,
    [KSHIM_MENU_ROLE_DEVICE_TB322FC] = KSHIM_TEXT_DEVICE_TB322FC_NAME,
    [KSHIM_MENU_ROLE_DEVICE_TB323FU] = KSHIM_TEXT_DEVICE_TB323FU_NAME,
    [KSHIM_MENU_ROLE_BOOT_SLOT] = KSHIM_TEXT_COUNT,
};

/* Role -> description id; KSHIM_TEXT_COUNT means "use the caller fallback". */
static const kshim_menu_text_id kshim_menu_role_desc_table[KSHIM_MENU_ROLE_MAX + 1] = {
    [KSHIM_MENU_ROLE_UNKNOWN]    = KSHIM_TEXT_COUNT,
    [KSHIM_MENU_ROLE_BASE]       = KSHIM_TEXT_DESC_BASE,
    [KSHIM_MENU_ROLE_ANDROID]    = KSHIM_TEXT_DESC_ANDROID,
    [KSHIM_MENU_ROLE_INTERNAL]   = KSHIM_TEXT_DESC_INTERNAL,
    [KSHIM_MENU_ROLE_TF]         = KSHIM_TEXT_DESC_TF,
    [KSHIM_MENU_ROLE_DIAGNOSTIC] = KSHIM_TEXT_DESC_DIAGNOSTIC,
    [KSHIM_MENU_ROLE_ADVANCED]   = KSHIM_TEXT_DESC_ADVANCED,
    [KSHIM_MENU_ROLE_LANGUAGE]   = KSHIM_TEXT_LANGUAGE_DESC,
    [KSHIM_MENU_ROLE_RESTART]    = KSHIM_TEXT_DESC_RESTART,
    [KSHIM_MENU_ROLE_OFF]        = KSHIM_TEXT_DESC_OFF,
    [KSHIM_MENU_ROLE_RECOVERY]   = KSHIM_TEXT_DESC_RECOVERY,
    [KSHIM_MENU_ROLE_FASTBOOT]   = KSHIM_TEXT_DESC_FASTBOOT,
    [KSHIM_MENU_ROLE_EDL]        = KSHIM_TEXT_DESC_EDL,
    [KSHIM_MENU_ROLE_BACK]       = KSHIM_TEXT_DESC_BACK,
    [KSHIM_MENU_ROLE_LANG_ZH]    = KSHIM_TEXT_DESC_LANG_ZH,
    [KSHIM_MENU_ROLE_LANG_EN]    = KSHIM_TEXT_DESC_LANG_EN,
    [KSHIM_MENU_ROLE_DEVICE_INFO] = KSHIM_TEXT_INFO_DESC,
    [KSHIM_MENU_ROLE_SETTINGS]   = KSHIM_TEXT_SETTINGS_HOME_DESC,
    [KSHIM_MENU_ROLE_DEVICE]     = KSHIM_TEXT_DEVICE_DESC,
    [KSHIM_MENU_ROLE_DEFAULT]    = KSHIM_TEXT_DEFAULT_DESC,
    [KSHIM_MENU_ROLE_DEVICE_AUTO] = KSHIM_TEXT_DEVICE_AUTO_DESC,
    [KSHIM_MENU_ROLE_DEVICE_TB322FC] = KSHIM_TEXT_DEVICE_TB322FC_DESC,
    [KSHIM_MENU_ROLE_DEVICE_TB323FU] = KSHIM_TEXT_DEVICE_TB323FU_DESC,
    [KSHIM_MENU_ROLE_BOOT_SLOT] = KSHIM_TEXT_COUNT,
};

kshim_menu_language_t kshim_menu_get_language(void)
{
    return g_kshim_menu_language;
}

int kshim_menu_set_language(kshim_menu_language_t language)
{
    if (language != KSHIM_MENU_LANG_ZH_CN && language != KSHIM_MENU_LANG_EN) {
        return -1;
    }
    g_kshim_menu_language = language;
    return 0;
}

const char *kshim_menu_text(kshim_menu_text_id id)
{
    const char *text;

    if ((unsigned)id >= (unsigned)KSHIM_TEXT_COUNT) {
        return "";
    }
#if defined(CONFIG_KSHIM_UFS_READ_PROBE) && CONFIG_KSHIM_UFS_READ_PROBE
    if (id == KSHIM_TEXT_TITLE) {
        static char probe_title[80];
        const struct kshim_ufs_read_report *read_report = storage_ufs_read_entry_report();
#if defined(CONFIG_KSHIM_UFS_LINK_WAKE) && CONFIG_KSHIM_UFS_LINK_WAKE
#if defined(CONFIG_KSHIM_UFS_SETTINGS) && CONFIG_KSHIM_UFS_SETTINGS
        snprintf(probe_title, sizeof(probe_title), "UFS Settings 11 s%u rc%d rel%d",
#else
        snprintf(probe_title, sizeof(probe_title), "UFS Read 10 s%u rc%d rel%d",
#endif
                 (unsigned)read_report->stage, read_report->status,
                 read_report->release_status);
#elif defined(CONFIG_KSHIM_UFS_READ_SCREEN) && CONFIG_KSHIM_UFS_READ_SCREEN
        snprintf(probe_title, sizeof(probe_title), "UFS Read 5 s%u rc%d rel%d",
                 (unsigned)read_report->stage, read_report->status,
                 read_report->release_status);
#else
        snprintf(probe_title, sizeof(probe_title), "UFS Read 4 s%u rc%d rel%d",
                 (unsigned)read_report->stage, read_report->status,
                 read_report->release_status);
#endif
        return probe_title;
    }
#elif defined(CONFIG_KSHIM_STORAGE_ENTRY_PROBE) && CONFIG_KSHIM_STORAGE_ENTRY_PROBE
    if (id == KSHIM_TEXT_TITLE) {
        static char probe_title[80];
        const struct storage_probe_snapshot *probe = storage_probe_entry_snapshot();
        snprintf(probe_title, sizeof(probe_title), "UFS Probe 3  b%u s%u n%u",
                 (unsigned)probe->board, (unsigned)probe->status, (unsigned)probe->entry_count);
        return probe_title;
    }
#endif
    text = kshim_menu_text_table[kshim_menu_active_language()][id];
    return text ? text : "";
}

const char *kshim_menu_role_name(kshim_menu_role_t role, const char *fallback)
{
    kshim_menu_text_id id;

    if ((unsigned)role > (unsigned)KSHIM_MENU_ROLE_MAX) {
        return fallback;
    }
    id = kshim_menu_role_name_table[role];
    if (id >= KSHIM_TEXT_COUNT) {
        return fallback;
    }
    return kshim_menu_text(id);
}

const char *kshim_menu_role_description(kshim_menu_role_t role, const char *fallback)
{
    kshim_menu_text_id id;

    if ((unsigned)role > (unsigned)KSHIM_MENU_ROLE_MAX) {
        return fallback;
    }
    id = kshim_menu_role_desc_table[role];
    if (id >= KSHIM_TEXT_COUNT) {
        return fallback;
    }
    return kshim_menu_text(id);
}

int kshim_menu_format_countdown(char *buffer, size_t capacity, uint32_t seconds, const char *name)
{
    int has_name = (name != NULL) && (name[0] != '\0');

    if (kshim_menu_active_language() == KSHIM_MENU_LANG_ZH_CN) {
        if (has_name) {
            return snprintf(buffer, capacity, "%u 秒后启动%s", (unsigned)seconds, name);
        }
        return snprintf(buffer, capacity, "%u 秒后自动启动", (unsigned)seconds);
    }
    if (has_name) {
        return snprintf(buffer, capacity, "Booting %s in %u s", name, (unsigned)seconds);
    }
    return snprintf(buffer, capacity, "Booting in %u s", (unsigned)seconds);
}
