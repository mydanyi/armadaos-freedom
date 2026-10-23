<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset=".github/assets/armada-mark-white.svg">
    <img src=".github/assets/armada-mark-black.svg" alt="Armada 标志" width="112">
  </picture>
</p>

<h1 align="center">ArmadaOS Freedom</h1>

<p align="center"><a href="README.md">English</a> · <strong>简体中文</strong></p>

<p align="center"><strong>面向 ARM 平板的 SteamOS 风格 Linux 系统</strong></p>

<p align="center">
  基于 <a href="https://github.com/armada-os/armada">ArmadaOS</a> 的平板适配分支，
  为 ARM64 平板带来 Steam 游戏模式、FEX、Proton 和 KDE Plasma 桌面，
  使用 kShimC 实现多系统启动。
</p>

<p align="center">
  <a href="https://github.com/armada-os/armada"><img alt="基于 ArmadaOS" src="https://img.shields.io/badge/基于-ArmadaOS-18181a?style=flat"></a>
  <a href="#设备支持"><img alt="Y700 四代与五代平板适配" src="https://img.shields.io/badge/当前适配-Y700_四代与五代-285580?style=flat"></a>
  <a href="LICENSE"><img alt="GPL-2.0-or-later 许可证" src="https://img.shields.io/badge/许可证-GPL--2.0--or--later-18181a?style=flat"></a>
</p>

<p align="center">
  <a href="#设备支持"><strong>设备支持与适配状态</strong></a>
  ·
  <a href="#kshimc-多系统启动">启动菜单</a>
  ·
  <a href="#文档导航">文档导航</a>
  ·
  <a href="https://armadaos.dev/">上游文档</a>
  ·
  <a href="#支持项目">支持项目</a>
</p>

## 关于 ArmadaOS Freedom

ArmadaOS Freedom 是基于 [ArmadaOS](https://github.com/armada-os/armada)、由社区维护的平板适配分支。我们保留上游的游戏和桌面系统基础，针对平板硬件调整启动、显示、电源和外设，控制插件则采用 [Freedom Control](https://github.com/mydanyi/freedom-control)。

目前已接入的功能：

- Steam 游戏模式与 KDE Plasma 桌面，沿用 ArmadaOS 的 FEX 和 Proton 支持。
- 统一内核构建，按机型加载设备树、驱动和硬件配置。
- kShimC 多系统启动，可分别选择机身存储和 TF 卡中的 Linux 系统。
- 按设备型号匹配的显示档位、触屏、电源、音频与网络配置。
- 双 USB 端口适配，以及电池和温度数据采集。
- 平板触屏输入，以及 G9／G9 2026 手柄按键映射、RGB 控制和双击 Home 打开右侧快捷菜单。
- 面向平板的 [Freedom Control](https://github.com/mydanyi/freedom-control) 适配，包括在无风扇设备上隐藏风扇控制。

各项功能的可用范围因设备而异，具体状态见下方设备文档。本仓库目前维护适配源码与补丁，平板安装镜像和 OTA 更新流程仍在完善中。

## 设备支持

目前适配联想拯救者 Y700 四代与五代，两代共用 Linux **7.2.6** 内核，并分别维护设备树和硬件配置。联想小新 Pad Pro GT（TB710FU，SM8650）通过按需启用的内核 overlay 和共用的 kShimC 启动器支持；它的硬件功能是在 SteamOS 用户空间下验证的，尚未用 ArmadaOS 镜像验证。

| 设备 | 型号 / SoC | 设备文档 | 已知问题 |
|---|---|---|---|
| 联想拯救者 Y700 四代 | TB322FC / SM8750 | [四代设备文档](devices/lenovo-tb322fc/README.md) | [四代已知问题](devices/lenovo-tb322fc/README.md#已知问题与待完成项) |
| 联想拯救者 Y700 五代 | TB323FU / SM8850P | [五代设备文档](devices/lenovo-tb323fu/README.md) | [五代已知问题](devices/lenovo-tb323fu/README.md#已知限制) |
| 联想小新 Pad Pro GT | TB710FU / SM8650 | [TB710FU 设备文档](devices/lenovo-tb710fu/README.md) | [TB710FU 已知问题](devices/lenovo-tb710fu/README.md#已知问题与待完成项) |

各机型的启动镜像仍需匹配各自设备，不能直接互刷。

## kShimC 多系统启动

平板使用 **kShimC** 启动器，与启动所需的内核等内容一起打包到 Android boot 镜像中。在保留原厂启动链的基础上，提供以下菜单：

- 启动 Android。
- 启动安装在机身存储中的 ArmadaOS。
- 启动安装在 TF／microSD 卡中的 ArmadaOS。
- 高级启动模式：Linux 诊断、Recovery 恢复模式、Fastboot 和 9008／EDL。
- 设置、设备信息、语言选择、重启和关机。

Linux 内核和设备树随 boot 镜像打包；内核启动后，根据所选条目挂载机身或 TF 卡上的根文件系统。机身安装和 TF 卡安装是两个独立目标，入口的可用范围见各机型文档。

<p align="center">
  <img src=".github/assets/kshimc-star-chart-zh.png" alt="KShimC 启动器：STAR_CHART 主题简体中文菜单" width="100%">
</p>

*上图为当前实现生成的 STAR_CHART 菜单渲染预览，非平板实拍。*

各机型共用 [kShimC 启动器源码](devices/lenovo-tb323fu/boot/kshim-success-source/README.md)，分别选择对应的机型配置和启动载荷。构建入口见[四代引导说明](devices/lenovo-tb322fc/boot/README.md)、[五代引导说明](devices/lenovo-tb323fu/boot/star-chart-build/README.md)与[小新 Pad Pro GT 引导说明](devices/lenovo-tb710fu/boot/README.md)。

## 文档导航

| 我想了解…… | 对应文档 |
|---|---|
| 如何构建启动镜像 | [四代引导说明](devices/lenovo-tb322fc/boot/README.md) · [五代引导说明](devices/lenovo-tb323fu/boot/star-chart-build/README.md) |
| 系统内使用哪些引导工具 | [kShimC 打包工具](packages/kshim-tools/README.md) |
| 组件来源与固定版本 | [四代源码与来源](devices/lenovo-tb322fc/README.md#源码与来源) · [五代来源记录](devices/lenovo-tb323fu/docs/SOURCES.md) |
| 上游系统如何使用 | [ArmadaOS 使用文档](https://armadaos.dev/using-armada/) |
| 反馈本分支的问题 | [Freedom 问题反馈](https://github.com/mydanyi/armadaos-freedom/issues) |

上游安装说明适用于上游支持的设备。平板的安装与启动方式请以本仓库对应机型的文档为准。

## 开发与构建

系统镜像沿用 ArmadaOS 的 bootc 构建基础；统一内核位于 [`packages/kernel/`](packages/kernel/)，机型配置位于 [`system_files/usr/lib/armada/devices/`](system_files/usr/lib/armada/devices/)，设备专用资源和引导说明位于 [`devices/`](devices/)。软件包开发说明见 [`packages/`](packages/README.md)，构建命令使用 [just](https://just.systems/) 和 [Podman](https://podman.io/)：

```console
$ git submodule update --init --recursive
$ just check     # 运行测试并检查构建配方格式
$ just packages  # 构建镜像使用的软件包
$ just build     # 构建 bootc 容器镜像；尚非可直接刷入平板的完整镜像
$ just --list    # 查看开发命令
```

### Freedom Control

控制插件来自固定版本的 [Freedom Control](https://github.com/mydanyi/freedom-control) 依赖，位于 `decky/freedom-control`。本仓库不再维护插件源码副本，因此首次检出后需要获取固定版本：

```console
$ git submodule update --init --recursive
```

需要升级插件版本时，请显式更新依赖并提交新的指针：

```console
$ git submodule update --remote decky/freedom-control
$ just check
$ git add decky/freedom-control
$ git commit -m "Update Freedom Control"
```

插件仍安装在原有目录，服务与套接字接口保持不变，已有设置和相关集成可继续使用。少数 SM8850 配套文件（电源守护进程、TB323FU 机型配置和 SM8850 电源档位层）同样取自该依赖，系统其余部分仍由本仓库维护。

设备适配请遵循对应机型的配置与构建说明。系统更新源使用 Freedom 自有仓库；各机型的引导同步和 OTA 支持进度见设备文档。

## 支持项目

ArmadaOS Freedom 目前靠热爱和自费投入持续推进。从启动、显示到供电和外设，每一项平板适配都需要反复测试，也需要长期跟进上游更新。

如果这些工作对你有帮助，欢迎通过支付宝或微信自愿赞助，支持设备测试、系统适配和持续维护。量力而行，谢谢每一份支持。

<p align="center">
  <img src=".github/assets/sponsor-zh.png" alt="支持 ArmadaOS Freedom 平板适配：支付宝和微信赞助二维码，自愿支持，量力而行" width="100%">
</p>

反馈问题、提交代码和完善文档，同样是在支持项目。

## 致谢

- 系统基础：[ArmadaOS](https://github.com/armada-os/armada)，基于 [Fedora bootc](https://github.com/bootc-dev/bootc)。
- 启动菜单：[kShimC](https://github.com/sunflower2333/kShimC/tree/feature/2610)。
- 控制插件：[Freedom Control](https://github.com/mydanyi/freedom-control)。
- Armada 标志：[Rax](https://github.com/Raxcoms)。

更多贡献者见[上游致谢](https://armadaos.dev/project/credits/)，设备适配的组件来源与固定版本见[对应设备文档](#设备支持)。各组件保留原有的来源署名和许可证。

## 许可证

除文件或组件另有声明外，armadaos-freedom 原创软件和脚本采用 **GPL-2.0-or-later** 许可证。我们的原创贡献由 danyi 及 armadaos-freedom 各贡献者享有版权（Copyright (C) 2026）。继承的 ArmadaOS 代码及其他组件保留各自的版权和适用许可。详见 [LICENSE](LICENSE) 和[许可与版权说明](LICENSING.zh-CN.md)（[English](LICENSING.md)）。
