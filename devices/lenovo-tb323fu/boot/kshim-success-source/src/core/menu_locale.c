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
        [KSHIM_TEXT_DESC_TF]        = "启动安装在 TF 卡中的 ArmadaOS，请先插入对应 TF 卡。",
        [KSHIM_TEXT_DESC_DIAGNOSTIC] = "启动 Linux 诊断环境，用于检查启动和硬件问题。",
        [KSHIM_TEXT_DESC_ADVANCED]  = "查看可用的高级启动模式。",
        [KSHIM_TEXT_DESC_RESTART]   = "重新启动设备。",
        [KSHIM_TEXT_DESC_OFF]       = "关闭设备电源。",
        [KSHIM_TEXT_DESC_RECOVERY]  = "进入 Android 恢复模式，进行系统维护。",
        [KSHIM_TEXT_DESC_FASTBOOT]  = "进入 Fastboot，通过 USB 连接电脑进行维护或刷写。",
        [KSHIM_TEXT_DESC_EDL]       = "进入高通 9008 下载模式，供电脑端工具恢复或刷写。",
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

        [KSHIM_TEXT_NAME_ANDROID]     = "Android 原厂",
        [KSHIM_TEXT_NAME_INTERNAL]    = "机身 ArmadaOS",
        [KSHIM_TEXT_NAME_TF]          = "TF 卡 ArmadaOS",
        [KSHIM_TEXT_NAME_DIAGNOSTIC]  = "Linux 诊断模式",

        [KSHIM_TEXT_LANGUAGE_LABEL]   = "语言 / Language",
        [KSHIM_TEXT_LANGUAGE_STATE]   = "简体中文",
        [KSHIM_TEXT_LANGUAGE_DESC]    = "选择菜单显示语言。",
        [KSHIM_TEXT_LANGUAGE_SELECT]  = "选择",
        [KSHIM_TEXT_DESC_LANG_ZH]     = "使用简体中文菜单。",
        [KSHIM_TEXT_DESC_LANG_EN]     = "使用英文菜单。",
        [KSHIM_TEXT_LANG_ZH_NAME]     = "简体中文",
        [KSHIM_TEXT_LANG_EN_NAME]     = "English",
        [KSHIM_TEXT_CREDIT]           = "适配维护：B站「单翼」｜免费使用，谨防收费诈骗。",

        [KSHIM_TEXT_INFO_LABEL]       = "设备信息",
        [KSHIM_TEXT_INFO_DESC]        = "查看设备硬件信息（只读）。",
        [KSHIM_TEXT_INFO_BOARD]       = "机型",
        [KSHIM_TEXT_INFO_SOC]         = "SoC",
        [KSHIM_TEXT_INFO_MODEL]       = "DT 型号",
        [KSHIM_TEXT_INFO_COMPATIBLE]  = "DT 兼容列表",
        [KSHIM_TEXT_INFO_RAM]         = "DT 报告内存",
        [KSHIM_TEXT_INFO_FRAMEBUFFER] = "帧缓冲",
        [KSHIM_TEXT_INFO_VERSION]     = "kShimC 版本",
        [KSHIM_TEXT_INFO_RAM_PREFIX]  = "DT 报告",
        [KSHIM_TEXT_UNKNOWN]          = "未知",

        [KSHIM_TEXT_SETTINGS_LABEL]   = "设置",
        [KSHIM_TEXT_SETTINGS_DESC]    = "本次启动有效。",
        [KSHIM_TEXT_SETTINGS_INTERNAL] = "更改会保存到机身存储。",
        [KSHIM_TEXT_SETTINGS_TF] = "更改会保存到 TF 卡。",
        [KSHIM_TEXT_SETTINGS_SAVE_FAILED] = "无法确认保存。",
        [KSHIM_TEXT_AB] = "安卓启动槽切换",
        [KSHIM_TEXT_AB_DESC] = "A/B 是什么？\n"
            "A、B 是安卓更新使用的两套启动分区。它们不代表安卓和 Linux；选择系统请返回主菜单。\n\n"
            "什么时候需要切换？\n"
            "平时无需更改。更新或启动异常时，可在确认另一槽有兼容系统后尝试；切换不保证能修复故障。\n\n"
            "切换会做什么？\n"
            "选择目标槽并再次确认后，会保存选择并重启。不会复制系统、恢复备份或清空个人文件。",
        [KSHIM_TEXT_AB_A] = "改用安卓启动槽 A",
        [KSHIM_TEXT_AB_B] = "改用安卓启动槽 B",
        [KSHIM_TEXT_AB_CURRENT] = "这是设备当前使用的安卓启动槽，无需重复选择。\n\n"
            "A、B 用于切换安卓更新的启动分区，不是安卓和 Linux 的对应关系。要选择启动系统，请返回主菜单。",
        [KSHIM_TEXT_AB_CURRENT_A] = "安卓启动槽 A（当前）",
        [KSHIM_TEXT_AB_CURRENT_B] = "安卓启动槽 B（当前）",
        [KSHIM_TEXT_AB_UNAVAILABLE] = "此槽的启动文件或启动状态未通过检查，当前不能切换到它。\n\n"
            "本次未更改启动槽。请先修复该槽的启动文件，再尝试切换。",
        [KSHIM_TEXT_AB_CONFIRM] = "确认后会保存所选安卓启动槽，并立即重启；以后也会使用该槽，直到再次切换。\n\n"
            "这不会复制系统、恢复备份或清空个人文件。另一槽仍须有兼容的启动文件和系统；切换不保证能修复故障。\n\n"
            "选择“返回”可取消，本次不会更改启动槽。",
        [KSHIM_TEXT_AB_FAILED] = "安卓启动槽检查或切换失败，请先检查启动文件和启动状态。",
        [KSHIM_TEXT_AB_RESTART] = "需要重新启动设备。",
        [KSHIM_TEXT_DEVICE_LABEL]     = "设备",
        [KSHIM_TEXT_DEVICE_DESC]      = "选择本次启动使用的机型。",
        [KSHIM_TEXT_DEFAULT_LABEL]    = "默认启动项",
        [KSHIM_TEXT_DEFAULT_DESC]     = "选择默认启动的系统。",
        [KSHIM_TEXT_DEVICE_AUTO_NAME] = "自动识别",
        [KSHIM_TEXT_DEVICE_AUTO_DESC] = "按实际检测到的机型启动。",
        [KSHIM_TEXT_DEVICE_TB322FC_NAME] = "Y700 四代 TB322FC",
        [KSHIM_TEXT_DEVICE_TB322FC_DESC] = "按 TB322FC 机型启动。",
        [KSHIM_TEXT_DEVICE_TB323FU_NAME] = "Y700 五代 TB323FU",
        [KSHIM_TEXT_DEVICE_TB323FU_DESC] = "按 TB323FU 机型启动。",
        [KSHIM_TEXT_BOARD_TB322FC_NAME]  = "Y700 四代",
        [KSHIM_TEXT_BOARD_TB323FU_NAME]  = "Y700 五代",
        [KSHIM_TEXT_SETTINGS_DEVICE_AUTO_FMT]   = "设备：自动（%s）",
        [KSHIM_TEXT_SETTINGS_DEVICE_MANUAL_FMT] = "设备：%s",
        [KSHIM_TEXT_SETTINGS_DEFAULT_FMT]       = "默认启动项：%s",
        [KSHIM_TEXT_DEFAULT_MANIFEST] = "镜像默认",
        [KSHIM_TEXT_DEFAULT_LAST] = "记住上一次启动的系统",
        [KSHIM_TEXT_DEFAULT_LAST_DESC] = "下次默认启动上一次选择的系统。",
        [KSHIM_TEXT_UNAVAILABLE]      = "镜像未提供对应启动内容。",
        [KSHIM_TEXT_ACTION_UNAVAILABLE] = "不可用",
        [KSHIM_TEXT_DESC_BACK_PARENT] = "返回上一级。",
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
        [KSHIM_TEXT_DESC_TF]        = "Start ArmadaOS installed on the TF card. Insert the matching card first.",
        [KSHIM_TEXT_DESC_DIAGNOSTIC] = "Start the Linux diagnostic environment to investigate boot and hardware problems.",
        [KSHIM_TEXT_DESC_ADVANCED]  = "Choose an available advanced boot mode.",
        [KSHIM_TEXT_DESC_RESTART]   = "Restart the device.",
        [KSHIM_TEXT_DESC_OFF]       = "Turn off the device.",
        [KSHIM_TEXT_DESC_RECOVERY]  = "Enter Android recovery mode for system maintenance.",
        [KSHIM_TEXT_DESC_FASTBOOT]  = "Enter Fastboot and connect to a computer over USB for maintenance or flashing.",
        [KSHIM_TEXT_DESC_EDL]       = "Enter Qualcomm 9008 download mode for recovery or flashing with a computer.",
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

        [KSHIM_TEXT_NAME_ANDROID]     = "Stock Android",
        [KSHIM_TEXT_NAME_INTERNAL]    = "Internal ArmadaOS",
        [KSHIM_TEXT_NAME_TF]          = "TF card ArmadaOS",
        [KSHIM_TEXT_NAME_DIAGNOSTIC]  = "Linux diagnostics",

        [KSHIM_TEXT_LANGUAGE_LABEL]   = "语言 / Language",
        [KSHIM_TEXT_LANGUAGE_STATE]   = "English",
        [KSHIM_TEXT_LANGUAGE_DESC]    = "Choose the menu display language.",
        [KSHIM_TEXT_LANGUAGE_SELECT]  = "Select",
        [KSHIM_TEXT_DESC_LANG_ZH]     = "Use Simplified Chinese menus.",
        [KSHIM_TEXT_DESC_LANG_EN]     = "Use English menus.",
        [KSHIM_TEXT_LANG_ZH_NAME]     = "简体中文",
        [KSHIM_TEXT_LANG_EN_NAME]     = "English",
        [KSHIM_TEXT_CREDIT]           = "Adapted by Danyi on Bilibili · Free to use. Beware of paid scams.",

        [KSHIM_TEXT_INFO_LABEL]       = "Device info",
        [KSHIM_TEXT_INFO_DESC]        = "View read-only device hardware information.",
        [KSHIM_TEXT_INFO_BOARD]       = "Device model",
        [KSHIM_TEXT_INFO_SOC]         = "SoC",
        [KSHIM_TEXT_INFO_MODEL]       = "DT model",
        [KSHIM_TEXT_INFO_COMPATIBLE]  = "DT compatible",
        [KSHIM_TEXT_INFO_RAM]         = "DT-reported RAM",
        [KSHIM_TEXT_INFO_FRAMEBUFFER] = "Framebuffer",
        [KSHIM_TEXT_INFO_VERSION]     = "kShimC version",
        [KSHIM_TEXT_INFO_RAM_PREFIX]  = "DT reported",
        [KSHIM_TEXT_UNKNOWN]          = "Unknown",

        [KSHIM_TEXT_SETTINGS_LABEL]   = "Settings",
        [KSHIM_TEXT_SETTINGS_DESC]    = "Applies to this boot only.",
        [KSHIM_TEXT_SETTINGS_INTERNAL] = "Changes are saved to internal storage.",
        [KSHIM_TEXT_SETTINGS_TF] = "Changes are saved to the TF card.",
        [KSHIM_TEXT_SETTINGS_SAVE_FAILED] = "Could not confirm save.",
        [KSHIM_TEXT_AB] = "Android boot slots",
        [KSHIM_TEXT_AB_DESC] = "What are A/B slots?\n"
            "Android updates use two sets of boot partitions, A and B. They do not mean Android and Linux; choose your OS in the main menu.\n\n"
            "When should I switch?\n"
            "Normally, leave this unchanged. For update or boot problems, only try another slot with a compatible system. Switching may not fix the problem.\n\n"
            "What will happen?\n"
            "Choose a slot, then confirm to save it and restart. This does not copy a system, restore a backup, or erase personal files.",
        [KSHIM_TEXT_AB_A] = "Use Android boot slot A",
        [KSHIM_TEXT_AB_B] = "Use Android boot slot B",
        [KSHIM_TEXT_AB_CURRENT] = "This is the Android boot slot currently in use. There is no need to select it again.\n\n"
            "A and B are boot partition sets used for Android updates, not Android and Linux. Choose your OS in the main menu.",
        [KSHIM_TEXT_AB_CURRENT_A] = "Android slot A (current)",
        [KSHIM_TEXT_AB_CURRENT_B] = "Android slot B (current)",
        [KSHIM_TEXT_AB_UNAVAILABLE] = "This slot did not pass the boot-file or boot-state checks and cannot be selected.\n\n"
            "The active slot has not changed. Repair the slot's boot files before trying again.",
        [KSHIM_TEXT_AB_CONFIRM] = "Confirming saves the selected Android boot slot and restarts immediately. It remains selected until another slot change.\n\n"
            "This does not copy a system, restore a backup, or erase personal files. The other slot must have compatible boot files and a system. Switching may not fix a boot problem.\n\n"
            "Choose Back to cancel without changing the boot slot.",
        [KSHIM_TEXT_AB_FAILED] = "Android boot-slot check or switch failed. Check the boot files and boot state.",
        [KSHIM_TEXT_AB_RESTART] = "Restart the device to continue.",
        [KSHIM_TEXT_DEVICE_LABEL]     = "Device",
        [KSHIM_TEXT_DEVICE_DESC]      = "Choose the device used for this boot.",
        [KSHIM_TEXT_DEFAULT_LABEL]    = "Default boot entry",
        [KSHIM_TEXT_DEFAULT_DESC]     = "Choose the default system to boot.",
        [KSHIM_TEXT_DEVICE_AUTO_NAME] = "Auto detect",
        [KSHIM_TEXT_DEVICE_AUTO_DESC] = "Boot for the actually detected device.",
        [KSHIM_TEXT_DEVICE_TB322FC_NAME] = "Y700 Gen4 TB322FC",
        [KSHIM_TEXT_DEVICE_TB322FC_DESC] = "Boot for the TB322FC device.",
        [KSHIM_TEXT_DEVICE_TB323FU_NAME] = "Y700 Gen5 TB323FU",
        [KSHIM_TEXT_DEVICE_TB323FU_DESC] = "Boot for the TB323FU device.",
        [KSHIM_TEXT_BOARD_TB322FC_NAME]  = "Y700 Gen4",
        [KSHIM_TEXT_BOARD_TB323FU_NAME]  = "Y700 Gen5",
        [KSHIM_TEXT_SETTINGS_DEVICE_AUTO_FMT]   = "Device: Auto (%s)",
        [KSHIM_TEXT_SETTINGS_DEVICE_MANUAL_FMT] = "Device: %s",
        [KSHIM_TEXT_SETTINGS_DEFAULT_FMT]       = "Default boot entry: %s",
        [KSHIM_TEXT_DEFAULT_MANIFEST] = "Image default",
        [KSHIM_TEXT_DEFAULT_LAST] = "Remember last booted system",
        [KSHIM_TEXT_DEFAULT_LAST_DESC] = "Next time, boot the system selected last.",
        [KSHIM_TEXT_UNAVAILABLE]      = "No matching image content is provided.",
        [KSHIM_TEXT_ACTION_UNAVAILABLE] = "Unavailable",
        [KSHIM_TEXT_DESC_BACK_PARENT] = "Return to the previous menu.",
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
    [KSHIM_MENU_ROLE_SETTINGS]   = KSHIM_TEXT_SETTINGS_DESC,
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
