# 四代蓝牙固件与构建

TB322FC 使用 WCN7861（Peach/Brahma v2）。它在加载蓝牙固件前，还需要初始化外围处理器和 TME-L；仅增加设备树匹配或复制五代的 MBN 加载配置不足以完成初始化。

## 驱动与设备匹配

- 统一内核的 `0227-Bluetooth-qca-add-WCN7861-peripheral-setup.patch` 接入 WCN7861 识别、外围处理器固件加载、8 Mbps UART 初始化及对应的 IBS 配置。
- 实现移植自 [h0cheung/tb322fc-linux-kernel 的板级支持](https://github.com/h0cheung/tb322fc-linux-kernel/commit/dc7d14dc64c8d4f86220b30cd126b17d805c8474) 和 [WCN7861 初始化修复](https://github.com/h0cheung/tb322fc-linux-kernel/commit/ac6ae99d08b70ffad9c6733967e0ca8f52e1751d)，保留来源署名。
- 四代设备树使用 `qcom,wcn7861-bt`。NVM 路径在四代 DTS 适配补丁中指定；原始 DTS 保持原样。
- 删除参考 DTS 中固定的一台设备的蓝牙地址，沿用统一内核已有的设备序列号派生地址流程，避免所有安装共用同一地址。
- 五代继续使用现有的 `lenovo,baldur` 固件准备服务及其 MBN 路径；WCN7850、WCN7860 和 TB710FU 的既有处理保留。

## 固件文件

下列文件随 `system_files/usr/lib/firmware/` 进入镜像。内容与参考项目固定的 [固件清单](https://github.com/h0cheung/tb322fc-linux/blob/77e12f51b360fad770e6ac144cd4635949032ccb/firmware.json) 一致；这是设备厂商固件，不因收录而改为本项目的 GPL 授权。

| 相对路径 | 用途 | SHA-256 |
|---|---|---|
| `qca/brhbtfw20.tlv` | 蓝牙固件 | `837aea39d33e95a4ba272ef2a2306e5ecf9848853332d2f7b8002901d6af771d` |
| `qca/lenovo/tb322fc/brhbtnv20.bin` | 四代蓝牙 NVM | `f91354b0872f1f7680142df6852b2c8e67e3151ee2ad406b3d183a2fb539702e` |
| `qca/brhperifw20.tlv` | 外围处理器固件 | `9a8de09a127a5943f8c23d4ee6005531b900f085740a292c9ab5a20e76db6cb5` |
| `qca/brhperinv20.bin` | 外围处理器 NVM | `3c73215352ba6422a9f2639124d27f64ec83a4d2aa91b388439ca88ea31098d0` |
| `qca/tmel_peach_20.elf` | TME-L 固件 | `a175c8d9d0e6b38b5ae23ff0175da795b14b8268919463d2b6e1cb2bfeebc9f8` |

四代 NVM 使用独立目录，避免覆盖五代运行时准备的 `qca/brhbtnv20.bin`。

## 正式打包流程

1. 统一内核构建通过 `patches/series` 应用驱动补丁，并生成四代设备树及完整模块树。
2. `40-vendor-system-files.sh` 将五份固件随系统文件复制到镜像。
3. `edl-devices.json` 的四代 `bootRequirements` 固定固件路径和哈希，并要求 `hci_uart`、`btqca` 以及 WCN7861 设备匹配。
4. `55-generate-initramfs.sh` 使用同一清单把固件和驱动加入 initramfs，并在生成后重新解包校验。EDL 打包流程复用该校验；缺文件、哈希不符或驱动不支持 WCN7861 时停止出包。
5. 系统镜像、EDL 与 PR 工作流运行四代蓝牙回归，覆盖固件缺失／损坏、模块匹配和机型隔离。

构建检查不代表真机验收。四代冷启动、扫描、配对、断开重连和休眠恢复仍需在新镜像上验证。
