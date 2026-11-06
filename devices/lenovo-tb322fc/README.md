# 联想拯救者 Y700 四代

ArmadaOS Freedom 为 Y700 四代提供 kShimC 多系统启动、Steam 游戏模式和面向平板的输入与外设支持。本页记录 TB322FC 的功能状态、已知问题和开发入口。

| 项目 | 配置 |
|---|---|
| 型号 | TB322FC |
| SoC | Qualcomm SM8750 |
| 设备树标识 | `lenovo,elden`、`qcom,sm8750` |
| 内核 | ArmadaOS Freedom 统一 Linux 7.2.6 |
| Armada Control 配置 | `lenovo-tb322fc`，无风扇平板 |

## 功能状态

| 功能 | 当前状态 |
|---|---|
| kShimC 启动菜单 | 中文菜单、方向和音量／电源键操作已确认 |
| Android 启动 | 可从菜单进入原厂 Android |
| 机身 Linux | 可进入 ArmadaOS 和 Steam 游戏模式 |
| 触屏 | 坐标已修正，点击和滑动已确认正常 |
| Steam 会话重开 | 显示初始化修正后，正常退出并重开 Steam 的画面与触屏已确认正常 |
| G9 手柄 | 基础按键、单按／双击 Home 及 RGB 控制已确认正常 |
| Armada Control | 使用四代独立配置，隐藏风扇相关入口 |

这些状态对应已完成的实机操作验证；长期稳定性及其它显示切换场景仍在验证中。

## G9 手柄

四代适配旧版 G9（USB `3537:1134`）：

- 摇杆、十字键和 ABXY 可正常操作。
- 单按 Home 打开原菜单，双击 Home 打开右侧快捷菜单。
- 在 Armada Control 的 RGB 页面控制灯光开关、颜色和亮度。

按键通过独立的 InputPlumber 设备配置与 `lenovo_legion_g9` 映射接入，灯光复用 GCM HID 后端。匹配同时考虑平板型号和手柄 ID，与五代的 G9 2026（`3537:1136`）配置分开维护。

## 已知问题与待完成项

- **蓝牙：** WCN7861 驱动与五份固件已接入正式打包流程；新镜像的冷启动、配对、重连及休眠恢复待真机验证，见[蓝牙固件与构建说明](bluetooth.md)。

- **充电与双 USB：** 充电功率上报、快充兼容性及长边／短边端口行为仍需完善。
- **安装与更新：** 公共安装流程仍在完善，OTA 源码策略与钩子已接入，完整的引导载荷同步及系统更新尚未通过端到端验证。
- **启动器：** 手动／自动机型选择与默认启动项仅在本次启动内生效，持久化暂缓。TF 卡启动及高级启动操作不在上述已确认范围内。
- **手柄扩展功能：** 独立背键、IMU、震动、其它手柄模式及插拔／休眠恢复尚未完整验证。
- **相机：** 仅补齐设备树控制节点，功能未验证。

## 内核与启动

四代与[五代](../lenovo-tb323fu/README.md)共用 [`packages/kernel`](../../packages/kernel/) 中的 Linux 7.2.6 基础内核及 Freedom 补丁集。统一构建产生内核、完整模块树和两代各自的设备树；驱动中的板级行为按机型匹配。

四代使用 `lenovo,elden` / SM8750，五代使用 `lenovo,baldur` / SM8850。两代仍需各自的设备树和启动载荷，**boot 镜像不能互刷**。统一内核并不意味着两代已经共用安装包或完成 OTA 验证。

四代 Steam 会话重开的显示修正由 `0153`、`0154`、`0155` 内核补丁提供，按 `lenovo,elden` 匹配；五代显示支持使用独立的 Baldur 匹配。

引导构建见[四代引导说明](boot/README.md)，其中分别介绍原厂 Android 基座打包和机身 Linux／诊断环境打包。四代使用 `tb322fc_android_defconfig` 启动器配置，不能直接套用五代的 DTB 和离线打包布局。

系统更新目标为 Freedom 的 `ghcr.io/mydanyi/armadaos-freedom`，支持 `stable`、`beta`、`testing` 渠道。四代的引导同步由 [`armada-kshim-sync`](../../system_files/usr/libexec/armada/armada-kshim-sync) 及其配套工具处理；渠道配置与完整 OTA 验证是两项工作，当前验证范围见上方已知问题。

## 源码与来源

| 文件 | 用途 |
|---|---|
| [设备配置](../../system_files/usr/lib/armada/devices/lenovo-tb322fc.conf) | 机型识别、显示旋转和触屏选择 |
| [板级 DTS](../../packages/kernel/dts/sm8750-lenovo-elden.dts) | 保留上游原始设备树 |
| [DTS 适配补丁](../../packages/kernel/dts/sm8750-lenovo-elden.dts.patch) | 启动交接与运行时适配 |
| [相机伴生片段](../../packages/kernel/dts/sm8750-lenovo-elden-camera.dtsi) | 补齐板级 DTS 引用的控制节点 |
| [内核补丁](../../packages/kernel/patches/) | 显示、内存映射等驱动适配 |

板级 DTS 原样取自 [h0cheung/tb322fc-linux-kernel](https://github.com/h0cheung/tb322fc-linux-kernel) 的固定提交 [`6f97250ead155fc3b980856323e99a4623dae73d`](https://github.com/h0cheung/tb322fc-linux-kernel/blob/6f97250ead155fc3b980856323e99a4623dae73d/arch/arm64/boot/dts/qcom/sm8750-lenovo-elden.dts)，保留 BSD-3-Clause SPDX 和 Qualcomm 版权；本项目的适配通过独立补丁维护。

<details>
<summary>设备树适配细节</summary>

相机伴生片段来自同一固定提交的 `sm8750.dtsi` 第 3072–3309 行，摘出 `cci0`、`cci1`、`camss` 三个完整节点并置于 `&soc {}` 下，仅归一化缩进，保留源文件的 BSD-3-Clause SPDX 与 Qualcomm 版权。它补齐 `&camss`、`&cci0`、`&cci0_i2c0`、`&cci1`、`&cci1_i2c0` 标签，不新增驱动。

DTS 适配补丁的主要内容：

- 紧接 `#include "sm8750-pmics.dtsi"` 之后包含相机伴生片段。
- 删除 `memory@a0000000` 节点及其 RAM 注释：内存节点由运行时引导 DT（kShim handoff_memory）重建，原作者的固定 RAM map 未在本机确认。
- `chosen` 保留 `#address-cells`、`#size-cells`、`ranges` 与 simple-framebuffer 模板；`framebuffer` 去掉 unit-address 与固定 `reg`/`width`/`height`/`stride`/`format`，改加 `status = "disabled"`，由运行时 simplefb 处理器按模板填真实地址与尺寸。不向 framebuffer 添加 `memory-region`。
- 给 `reserved-memory` 下的 `ramoops@c2000000` 与 `splash-region@fc800000` 加 `status = "disabled"`，保留其地址元数据但停用，因为这些固定地址未经验证。
- 从 `&mdss` 删除 `memory-region = <&splash_mem>` 与 `iommus`，其余内容不变。
- 保留原 DTS 的 `&apps_smmu` 覆盖（含 `qcom,preserve-boot-mappings`）：该属性的消费方是 `../../packages/kernel/patches/0146-iommu-arm-smmu-elden-boot-handoff.patch`，本目录不重复实现，也不引入全局 SMMU 变通。
- 其余内容（音频、WiFi、USB、面板、触摸等）保持不变。

</details>
