# 联想小新 Pad Pro GT

ArmadaOS Freedom 统一内核与 kShimC 启动菜单对小新 Pad Pro GT 的适配。本页记录 TB710FU 的功能状态、已知问题、所需固件和开发入口。

| 项目 | 配置 |
|---|---|
| 型号 | TB710FU（小新 Pad Pro GT，3200×2000 120 Hz） |
| SoC | Qualcomm SM8650（骁龙 8 Gen 3，`qcom,pineapple`） |
| 设备树标识 | `lenovo,tb710fu`、`qcom,sm8650` |
| 内核 | ArmadaOS Freedom 统一 Linux 7.2.6（默认构建），DTB `sm8650-lenovo-tb710fu.dtb` |
| 引导 | kShimC 菜单放在 boot_a，与原厂 Android 共存；Bootloader 未解锁 |
| 已验证的用户空间 | SteamOS（SteamOS-ARM-Port 的 sm8650 镜像，装在 UFS 上）；**ArmadaOS 镜像尚未在本机验证** |

## 功能状态

以下为本仓库 `packages/kernel` 的 TB710FU 内核补丁在本机上的实测结果（测量时这些补丁还以内核 overlay 的形式构建，版本号带 `-armada-tb710fu`；并入统一内核后的设备树与之前逐项相同）。

| 功能 | 当前状态 |
|---|---|
| kShimC 启动菜单 | 显示、音量／电源键选择、超时默认项正常；gzip 压缩的 boot（解压后约 109 MB）被 ABL 接受 |
| Android 启动 | 菜单进入原厂 Android（HyperOS）：`verifiedbootstate=green`、`locked`，Magisk 正常 |
| 显示 | NT36532 双 DSI + DSC，3200×2000@120，msm 原生驱动 |
| GPU | Adreno 750，Mesa freedreno／Turnip；频率 231–903 MHz；IFPC 关闭 |
| 触屏与手写笔 | NT36532 触摸正常；Lenovo Tab Pen Plus 压感、倾斜、按键正常 |
| 扬声器 | 4 颗 AW882xx 功放经 TDM 出声，左右声道正常 |
| WiFi | WCN7850（ath12k），5 GHz 下载实测 4–11 MB/s |
| 蓝牙 | 使用本机地址（来自 DT `local-bd-address`） |
| 电池与充电信息 | pmic-glink／battmgr 正常上报 |
| USB | 作为设备端（USB NCM 网络）正常 |
| 游戏 | Steam 游戏模式；Proton ARM64 游戏可进入主菜单 |

## 已知问题与待完成项

- **用户空间：** 只在 SteamOS 上验证过。ArmadaOS（bootc／OSTree）需要能挂载 OSTree 部署的 initramfs 和引导同步，尚未适配。
- **USB 网络：** 重启后偶尔出现 USB 设备已枚举、但 macOS 不建立 NCM 网卡的情况，插拔一次数据线即可恢复，原因未查清。OTG 主机模式未测试。
- **休眠：** 统一内核下的 s2idle 睡眠／唤醒尚未验证。
- **音频：** 只验证了扬声器。耳机、麦克风未测试。前端必须使用 S16：24 位输入会让 ADSP 的音频处理出错。
- **WiFi／显示的板级变通：** 部分补丁来自原移植项目的调试过程（ath12k 的 CE 轮询、DSI 控制器寄存器、PHY 时序等），按 `lenovo,tb710fu` 隔离，只在本机生效。是否都必要尚未逐项验证，后续可以继续精简。
- **视频编解码（iris）、相机、外接显示（DP）：** 未启用。
- **固件：** 设备专用固件需要从本机分区提取，本仓库不提供，见下文。

## 内核与启动

TB710FU 的内核支持在统一内核的默认构建里，与其它机型共用同一个内核包：

```sh
bash packages/kernel/scripts/build-kernel.sh
```

启动用的 DTB 为 `dtb/qcom/sm8650-lenovo-tb710fu.dtb`。补丁在 `patches/series` 末尾的 TB710FU 一节，涉及公共驱动的代码都按 `lenovo,tb710fu`（或专用 compatible）判断，其它机型不受影响：

| 部分 | 补丁 | 设备树（`dts/sm8650-lenovo-tb710fu.dts.patch`） |
|---|---|---|
| 板级、显示、GPU、背光 | `071x`：SY7758 跟随芯片、NT36532 面板、msm DSI 命令触发口与 ABL 控制器/PHY 状态、DSC 宽度取整、A750 不启用 IFPC | kShimC framebuffer 模板、GPU 903 MHz、ADSP/CDSP 固件路径、触摸 compatible |
| 触摸与手写笔 | `072x`：NT36532 SPI 厂商驱动（`novatek,nt36532-ts-spi`）、开机打开笔扫描 | 笔 |
| 蓝牙与 WiFi | `073x`：DT 蓝牙地址、WCN7850 复位脉冲、ath12k 变通 | — |
| 扬声器 | `074x`：上游 TDM 时隙辅助函数（回移植）、q6apm TDM DAI、TDM 输出端点、4 声道上混、`lenovo,tb710fu-sndcard` | 4 颗 AW882xx 与声卡 |

板级 DTS 取自 MeteoricmeteorLL/tb710fu-linux 的固定提交 `16e800d4f6e5cf4a8517900a9e11eec0ebc91bd3`（该仓库 LICENSE 第 3 节：`kernel/sources/` 适用 GPL-2.0），只加了 SPDX 与来源注释；本项目的改动都在 `dts/sm8650-lenovo-tb710fu.dts.patch` 里。扬声器功放使用统一构建的 out-of-tree [`aw882xx`](../../packages/kernel/out-of-tree/aw882xx/README.md)（与五代相同的 0x2308 芯片）。

引导构建见 [TB710FU 引导说明](boot/README.md)。kShimC 的 TB710FU 支持在共享源码中，全部只在 `qcom,pineapple` 上生效：SM8650 的启动画面读取（ABL 用 4 个 VIG 管线拼接扫描输出）、跳过 size 为 0 的保留内存节点、重启白名单，以及交接前清理只读的 WB 映射；菜单配置为 `configs/tb710fu_android_defconfig`。**TB710FU 的 boot 镜像不能刷到其它机型，反之亦然。**

## 固件

以下文件需要从本机的 Android 分区或 linux-firmware 中取得，放到根文件系统的 `/usr/lib/firmware/`：

| 路径 | 来源 |
|---|---|
| `qcom/sm8650/xiaoxin/gt/adsp.mdt`、`adsp.bNN`、`adsp_dtb.mdt`… | 本机 `/vendor/firmware_mnt/image`（ADSP） |
| `qcom/sm8650/xiaoxin/gt/cdsp*.mdt`… | 同上（CDSP） |
| `qcom/sm8650/xiaoxin/gt/gen70900_zap.mbn` | 本机 `/vendor/firmware`（GPU zap） |
| `qcom/gen70900_sqe.fw`、`qcom/gmu_gen70900.bin` | linux-firmware 或本机 `/vendor/firmware` |
| `novatek/novatek_nt36532_fw.bin` | 本机 `/vendor/firmware/novatek_ts_fw.bin` |
| `aw882xx_acf.bin` | 功放配置，来自 tb710fu-linux 的 board-root |
| `qcom/sm8650/Lenovo-TB710FU-tplg.bin` | 镜像自带，由 [`audio_topology/sm8650/Lenovo-TB710FU.m4`](../../audio_topology/sm8650/Lenovo-TB710FU.m4) 生成；扬声器端点为 TDM 输出模块（`0x0700100E`） |
| `ath12k/WCN7850/hw2.0/{amss,board-2,m3}.bin` | 镜像自带；`board-2.bin` 末尾追加了本机一项（`subsystem-device=3378`），数据原样取自 Fedora `atheros-firmware-20261019`（即 linux-firmware），许可为同目录的 [`LICENSE.QualcommAtheros_ath10k`](../../system_files/usr/lib/firmware/ath12k/WCN7850/hw2.0/LICENSE.QualcommAtheros_ath10k)（linux-firmware `WHENCE` 中 `ath12k/WCN7850/hw2.0/board-2.bin` 对应的许可） |
| `qca/hmtbtfw20.tlv` 等 | linux-firmware（蓝牙） |

从本机提取固件的脚本见 [lich-wang/tb710fu-hyperos-steamos](https://github.com/lich-wang/tb710fu-hyperos-steamos)（`tools/collect_firmware.sh`）。

## 用户空间所需设置

- **WiFi：** 不需要额外设置。驱动按 PCI 子系统 ID 和芯片上报的 board-id（0xff）算出 `bus=pci,vendor=17cb,device=1107,subsystem-vendor=17cb,subsystem-device=3378,qmi-chip-id=2,qmi-board-id=255`，镜像的 `board-2.bin` 里有这一项。
- **扬声器：** 不需要额外设置。UCM（`/usr/share/alsa/ucm2/Lenovo/TB710FU/`）打开 `SECONDARY_TDM_RX_0 Audio Mixer MultiMedia1` 和 4 颗功放，功放音量与 Android 下相同（0x34/0x35 衰减 21 档，0x36/0x37 衰减 11 档）。PipeWire 的输出为 `alsa_output.platform-sound.HiFi__Speaker__sink`。
- **手写笔：** 补丁 0721 让驱动开机即打开笔扫描，并给坐标设好分辨率，桌面（KWin/libinput）可以直接使用；`/proc/support_pen` 仍可手动开关。游戏模式下 gamescope 不处理手写笔（tablet-tool）事件，需要另外把笔转成触摸。
- **PipeWire：** WirePlumber 规则 `53-lenovo-tb710fu-speakers.conf`（只匹配声卡名 `Lenovo-TB710FU`）把输出固定为 `S16LE`，并关闭空闲挂起：24 位输入会让 ADSP 的音频处理出错；每次从挂起恢复都会重新启动 4 颗功放，偶尔会锁不上 TDM 时钟。

## 源码与来源

| 组件 | 来源 |
|---|---|
| 板级 DTS、面板／触摸／WiFi／DSI 变通 | [MeteoricmeteorLL/tb710fu-linux](https://github.com/MeteoricmeteorLL/tb710fu-linux) `16e800d4f6e5cf4a8517900a9e11eec0ebc91bd3`；面板与触摸的点亮工作来自 [SpendyYT](https://github.com/SpendyYT) |
| NT36532 触摸驱动 | Novatek 厂商驱动（GPL-2.0-or-later），经上述项目移植 |
| TDM 时隙辅助函数 | 上游 Linux `1e844ecbc9cb`、`0a9e00d5ebdf`（原作者署名保留在补丁中） |
| TDM DAI 操作 | 参照上游 Linux `4d0840175893`，按本树（无 MI2S 时钟控制）改写 |
| TDM 端点参数 | 在本机 HyperOS 上抓取 AudioReach 用户空间实际下发的参数 |
| kShimC、DualBootKernelPatcher | 与五代共用，见[五代组件来源](../lenovo-tb323fu/docs/SOURCES.md) |
