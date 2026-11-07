# kShimC

kShimC 是 MultiBootKernelPatcher 所用通用 Shim 运行时（runtime）协议的一种实现。
打包器通过 BL 进入所附运行时。kShimC 保留 bootloader 参数，并在 `x4` 中给出 Linux 基址、在 `x5` 中给出 Manifest 地址、在 `x6` 中给出大小。它校验 Manifest；检测到受支持的连续启动画面（splash）时通过 LVGL 呈现其可执行条目，没有可用启动画面时按 DT 选择 GENI 或 PL011 串口。默认 Manifest 运行时仍为固定 4 MiB 槽位。

本源码快照由 ArmadaOS Freedom 的 Y700 四代与五代配置共用。它位于 `lenovo-tb323fu` 目录下并不表示只适用于五代。设备相关的打包说明见[四代引导说明](../../../lenovo-tb322fc/boot/README.md)与[五代引导说明](../star-chart-build/README.md)。

运行时使用 picolibc 1.8.11 与 LVGL 9.2.2。硬件库来自 `lib/CrDK`，固定在 `b6c3335136b7f6b28271917de11e8f11982975fc`。`ports/crdk/prepare.py` 会校验并把记录的补丁应用到每个构建目录；它从不修改子模块或 mu_aloha 工作区。参见 [CrDK 移植层与来源](ports/crdk/README.md)。

## 构建

安装 AArch64 GCC 工具链、CMake、Ninja、Python 3.12+ 和 kconfiglib（`genconfig` 需在 `PATH` 中）。选择配置前先恢复固定版本依赖。

本仓库中的快照省略了顶层依赖目录与 Git 元数据，对它执行 `git submodule update` 不会恢复依赖。请按共享的[依赖准备指引](DEPENDENCIES.md)恢复 `lib/CrDK`、`lib/picolibc`、`third_party/lvgl` 和 `lib/libfdt`，再选择配置；该指引对四代与五代同样适用。保留完整 Git 元数据的独立上游 kShimC 仓库可用 `git submodule update --init --recursive` 准备依赖，其后的构建命令相同：

```sh
python3 tools/build_runtime.py --config configs/hdk8250_defconfig --out build/manifest
```

产物为 `build/manifest/kShimC.bin`；脚本校验固定 4 MiB Manifest 槽位并拒绝未定义符号。同一命令也接受 Andromeda、HDK8150 与 HDK8450 配置。Manifest 运行时不需要固件载荷或 mu_aloha 路径。CMake 在配置阶段解析 Kconfig，并按能力选择 CrDK 与 UI 源码；配置变化后由 `cmake --build` 重新生成。固件使用启动 DT 获取设备地址。

两代 Y700 配置使用同一条命令，只是配置名与输出目录不同。在 Freedom 仓库根目录执行：

```sh
cd devices/lenovo-tb323fu/boot/kshim-success-source
python3 tools/build_runtime.py --config configs/tb322fc_android_defconfig --out build/tb322fc-android --jobs 4
python3 tools/build_runtime.py --config configs/tb323fu_unified_defconfig --out build/tb323fu-unified --jobs 4
```

### Y700 机型配置

| 机型 | 运行时配置 | boot 打包 |
|---|---|---|
| Y700 四代 / TB322FC | `configs/tb322fc_android_defconfig` | [Android 或 Linux 布局](../../../lenovo-tb322fc/boot/README.md) |
| Y700 五代 / TB323FU | `configs/tb323fu_unified_defconfig` | [统一内核外置 initramfs](../star-chart-build/README.md) |

两个机型配置都启用 STAR_CHART、简体中文、实体按键、高级操作、设备信息与持久化设置。设置保存到机身 UFS 尾部 2 GiB 备份分区内的专用配置区，该配置区须由安装器预先准备。四代关闭调试控制台与启动计时；五代启用启动计时。Linux 与 Android 菜单条目来自所选机型的 manifest 与载荷。

四代、五代的“高级启动模式”提供“A/B槽位切换”。启动器与 Android 共用设备的 A/B 槽位，A、B 不分别代表 Android 和 Linux。页面显示当前槽位，确认页同时显示当前槽和目标槽，默认选中“返回”。读取槽位信息后，选择另一可用槽并确认重启；当前槽和缺少启动链文件的槽不可选择。切换前检查所有 LUN 的主备 GPT、当前槽、UFS Boot ID 对应关系及待处理恢复/OTA 状态，更新槽位属性、分区类型 GUID 和 Boot LUN 后读回核验。I/O 失败会尝试恢复原状态；无法确认恢复成功时禁止继续交接系统。此操作需要持续供电，不能保证断电时跨 LUN 更新的原子性。TB710FU 尚无对应的原生 UFS 后端，不显示该入口。

A/B 切换不重打包或改写 boot 内容。启动器本身更新后，仍须经系统包构建与安装器的 boot 重打包、适用签名和校验流程；4 MiB 的 `kShimC.bin` 不是可直接刷入 boot 分区的镜像。

### 五代外置 initramfs 配置

`configs/tb323fu_unified_defconfig` 是面向外置 initrd Manifest 工作的 TB323FU 配置。它带有共享启动器菜单：可持久保存的设备选择、默认启动项与语言设置，以及只读的设备信息行。默认启动项可选择“记住上一次启动的系统”。其余选项均保留：STAR_CHART 风格、UI 逆时针旋转 270 度、243712 字节 LVGL 缓冲、无入场动画、带条目详情的简体中文、Android 基座措辞、带 QCOM 重启目标的电源与高级操作、启动计时，以及两个去抖采样的 SPMI 按键；触屏、SMP、UEFI 测试菜单与 QEMU 平台保持关闭。

Linux 安装了 `armada-reboot-edl` 工具后，可用 `sudo armada-reboot-edl --check` 检查入口，再用 `sudo armada-reboot-edl --reboot` 请求启动器进入 9008。五代的电脑端仍须加载配套八文件引导才能访问存储。

```sh
python3 tools/build_runtime.py --config configs/tb323fu_unified_defconfig --out build/tb323fu-unified
```

参见[四代状态](../../../lenovo-tb322fc/README.md)与[五代状态](../../README.md)。

## 打包多个内核

先构建父级 `MultiBootKernelPatcher` 仓库，然后创建 INI 文件：

```ini
[Pack]
Shim=../../kShimC/build/manifest/kShimC.bin
Output=build/FakeKernel
Default=Image-A
Timeout=5000

[Image-A]
Name=Android Linux
Type=Linux
Path=Image
BaseImage=true
Align=21

[Image-B]
Name=Recovery Linux
Type=Linux
Path=/path/to/recovery-Image
BaseImage=false
Align=21

[Image-UEFI]
Name=UEFI
Type=FreeExec
Path=/path/to/UEFI.fd
BaseImage=false
CopyTo=0x9FC00000
CopySizeMax=0x00300000

[Manifest]
Type=Manifest
```

用该配置运行打包器：

```sh
../../build/MultiBootKernelPatcher ../../Config/Shim.Sample.cfg
```

附加镜像为可选项。路径相对于配置文件解析。必须恰好有一个镜像设置 `BaseImage=true`；`Default` 引用镜像段。读取时，没有 `CopyTo` 的条目按 ARM64 Linux Image 校验；`Align` 可选，其 `text_offset` 放置约束始终生效。带 `CopyTo` 的条目还必须设置 `CopySizeMax`；文件超出该上限时打包器拒绝该条目，否则将其复制到给定地址，并以原始启动参数从该地址启动。复制条目默认使用 4 字节打包镜像对齐，因此 UEFI 不需要显式 `Align`。`Align` 是 2 到 21 的以 2 为底的对齐指数，并假定打包后的 Base Image 加载在 2 MiB 对齐地址。`Timeout=0` 表示无限等待串口输入。
生成的布局遵循通用 Shim 协议：

```text
[Base Image slot][KShim 4 MiB slot][aligned images ...][manifest]
```

Linux 槽位至少为 ARM64 Image 头部有效的 `image_size`。第一条指令变为指向所附 runtime 的 BL；原 Linux 分支保留为第二条指令。头部偏移 `0x20`、`0x28` 和 `0x30` 分别保存 manifest 偏移、manifest 大小和 base image 槽位大小。附加 Linux Image 相对于 2 MiB 对齐基址遵循其 ARM64 `text_offset`。复制条目使用其精确文件大小，KShim 在复制后同步数据与指令缓存。可变长 manifest 追加在打包镜像末尾。启动时通过 PL011 输入显示的编号并按 Enter；Escape 或超时选择默认项。

## QEMU

```sh
qemu-system-aarch64 \
	-machine virt,gic-version=3 \
	-cpu cortex-a57 -smp 1 -m 1024M \
	-nographic -no-reboot \
	-kernel build/FakeKernel \
	-append 'console=ttyAMA0,115200 earlycon=pl011,mmio32,0x09000000'
```


## 板级菜单与输入

`configs/` 保存能力配置。DT 中的启动画面保留区与稳定的 SDE source/mixer 寄存器提供帧缓冲地址、尺寸、行跨度和格式。支持的布局为线性 RGB565、32 位 RGB/BGR，以及经校验的共享帧缓冲分屏扫描。启动画面缺失或存在歧义时回退到串口选择。kShimC 不初始化 DSI 硬件。音量加向上、音量减向下、电源键确认。触屏可选择卡片；在支持触屏输入的配置上，由单独的 Boot 按钮确认。Y700 配置关闭启动器触屏并使用实体音量/电源键，这与 Linux 内的触屏支持是两件不同的事。

菜单风格是 `src/Kconfig` 与 `src/ui/styles/` 中的编译期选项。两个 Y700 配置都选择 STAR_CHART，包含横屏导航、条目详情、语言选择和实体按键提示。当前行为见 [Y700 菜单指引](docs/tb323fu-boot-menu.md)。其余主题文档描述的是早期预设系列及其渲染工具，不决定已安装主题。

STAR_CHART 使用 `src/ui/fonts/lxgw_wenkai_menu.ksf`，即 LXGW 文楷菜单子集。其他风格使用 Noto Sans CJK SC 子集。CMake 选择匹配的字库 blob；LVGL 的 TinyTTF 渲染文字。字体许可与来源记录保留在 `src/ui/fonts/`。修改菜单文字时，用 `tools/gen_ui_font.py` 重新生成对应子集，并更新[所需菜单字符](../star-chart-build/menu-required-text.txt)。

MIX 3 5G 配置为 `andromeda`：ST FTM5（不是 FocalTech）、0x49 上的 GENI I2C、1080 x 2340。其总线、时钟、GPIO、CmdDB 与 RPMh 设置独立于 HDK8150。该协议也有 SPI 传输，但尚未确认该手机存在 SPI 触屏版本。此引导驱动不刷写触屏固件。源码版本与寄存器来源记录在 `ports/crdk` 与 `configs/andromeda-provenance.json`。从 GitHub 获取的 FTM5 驱动源码以哈希固定在 `third_party/st-ftm5/UPSTREAM.json`。IRQ/RESET GPIO122/54 使用厂商的 8 mA 上拉配置；上电时序使能 GPIO37，等待 4 ms，使能固定的 1.8 V RPMh 电源轨，然后复位脉冲 10 ms。

触屏输入路径把每次 FIFO 接触变化都交给 LVGL，因此按下与释放落在同一次读取中的轻触仍能选择卡片或确认 Boot 按钮。控制器复位、总线错误或关闭会取消该接触而不产生点击。FTM5 也处理 ENTER 丢失的移动手指，并把悬停视为已释放。串口控制台打印板卡/控制器名称以及初始化或输入错误码，用于板级调试。

载荷交接前，运行时停止触屏扫描、归还占用的总线资源、等待绘制线程结束、用 PSCI 确认次级 CPU_OFF，并在关闭 MMU 前清理运行时拥有的可写 WB 映射。随后停止 GENI UART 活动并恢复继承的设置。保留/不可映射内存从不作为普通 RAM 映射；显式的 framebuffer/CmdDB 使用者使用 NC 映射。关闭失败会阻止交接。PSCI 不可用时回退到在启动 CPU 上渲染。

## 板卡识别

`src/dt/device_info.c` 对显式板卡与 SoC 标识使用精确根匹配。根 `compatible` 条目必须精确匹配。不带 Lenovo 标识的原厂板卡仅凭一个实测的活跃面板参照才被接受，绝不依据型号文字、打包载荷、单独的 SoC、数字 phandle 或任何待机面板：精确的 `/soc/qcom,dsi-display-primary` 节点（单一 `qcom,dsi-display` compatible、`primary` label、其自身与祖先均可用）必须把其单 cell 的 `qcom,dsi-default-panel` 解析为唯一一个已使能面板，且节点名与 `qcom,mdss-dsi-panel-name` 精确匹配。该指纹用于 Elden CPHY 面板（精确 `qcom,sunp` 根），以及 Baldur DPHY 面板（精确 `qcom,canoep` 根且无 SM8750 系列条目）。任何其他布局，包括大小写、前缀、后缀、primary 或面板的近似匹配，都保持 UNKNOWN：只有这些实测形态有证据。

## 验证

```sh
cmake -S tests -B build/host -G Ninja -DCMAKE_C_COMPILER=clang
cmake --build build/host -j8
ctest --test-dir build/host --output-on-failure
python3 tools/test_qemu_smp.py
python3 tools/test_crdk_upstream.py --edk2 /path/to/edk2
python3 tests/manifest_qemu.py --runtime build/manifest/kShimC.bin
python3 tests/manifest_ui_qemu.py --build build/qemu-manifest-ui
python3 tools/test_qemu_lvgl_failstop.py --build build/qemu-lvgl-failstop
```

主机测试套件使用 ASan 与 UBSan。QEMU 在 EL1/HVC 与 EL2/SMC 下以 1、2、4、8 个 CPU 执行真实 AArch64 PSCI 路径。Manifest 回归使用同级打包器，随后启动其默认 Linux 镜像、另一个 Linux 镜像和一个复制的 FreeExec 镜像；它还校验单独的 payload DT，并拒绝超过 Manifest 条目长度或结构损坏的 DTB。`manifest_ui_qemu.py` 还会以四个 PSCI CPU 运行真实 LVGL 帧缓冲菜单，并校验所选 payload 观察到变化的帧缓冲、已关闭的 MMU，以及所有次级 CPU 上的 CPU_OFF。fail-stop 检查注入绘制工作线程 join 失败，并校验不会复用已释放的 worker。这些检查不是对手机触屏总线、显示或生产固件 PSCI 权限的硬件验证。

## 构建板级测试镜像

独立测试在板卡的 Kernel 区保留 40 MiB，并嵌入一个与板卡匹配的 UEFI FD。`UEFI A` 与 `UEFI B` 启动同一个 FD。每次构建都会把嵌入的 FD 与保存的构建报告、目标设备、DEBUG 设置、GENI 串口库和非零调试 PCD 对照检查。

```sh
python3 tools/build_hdk8150.py --aloha /path/to/mu_aloha_platforms --out build/hdk8150 \
  --uefi-boot build/uefi-hdk8150/source-uefi-boot.img \
  --uefi-build-dir build/uefi-hdk8150 --require-debug-uart
python3 tools/build_hdk8450.py --aloha /path/to/mu_aloha_platforms --out build/hdk8450 \
  --uefi-boot build/uefi-hdk8450/source-uefi-boot.img \
  --uefi-build-dir build/uefi-hdk8450 --require-debug-uart
python3 tools/build_andromeda_dt.py
python3 tools/build_andromeda.py --aloha /path/to/mu_aloha_platforms --out build/andromeda \
  --uefi-boot build/uefi-andromeda/corrected/source-uefi-boot.img \
  --uefi-build-dir build/uefi-andromeda --require-debug-uart \
  --dtb-bundle build/andromeda-dt/android-andromeda-dtbs.bin
```

保存的 Andromeda FD 已按 DEBUG+UART 校验，但其原始来源启动镜像带有错误的 8450 尺寸 BootShim 头。`corrected/` 保存为 0x9fc00000 / 3 MiB 重建的包装器，使用字节完全相同的 FD；其 `provenance.json` 记录两个镜像。原始快照被保留。
四个 Andromeda 基础树保留手机既有的 DTBO 选择；打包前会重放 32 种 base/overlay 组合。不刷写任何 DTBO。

HDK8250 绑定与硬件表使用固定的 OnePlus `oneplus/SM8250_Q_10.0` 源码 `cad09e061ef6cf689a4a1e54d27562e0f042236a`。HDK8250 的 ST FTM4 触屏控制器不受支持；其驱动与厂商参照快照已移除。菜单仍可通过实体音量与电源键操作。触屏输入支持按 DT 属性族选择并独立校验芯片 ID 的 FTM5 控制器。

```sh
python3 tools/build_hdk8250.py --out build/hdk8250
```

这条命令生成一个不捆绑 UEFI 的 4 MiB Manifest 运行时。显示布局在启动时探测。上面的独立打包工具只有在读取板卡内存保留区并校验用户提供的 UEFI 包时才需要显式 `--aloha`；它们不会重建或编辑该工作区。

独立 copy-in 会校验其暂存页不与传入载荷重叠，并在写入前拒绝不安全的加载器位置。`tests/boot_emulation.py` 检查重叠与不相交复制、原始参数、栈、BSS、重定位、两种 UEFI 选择以及该拒绝路径。

## TB323FU Android 触摸兼容

`tb323fu_unified_defconfig` 明确启用 `CONFIG_KSHIM_TB323FU_ANDROID_TOUCH_4M8=y`。重编 runtime 时须保留该配置；仅保留兼容函数源码或依赖默认关闭的 CMake 试验开关，都不能保证 Android 交接路径会调用它。

处理仅对实际识别为 TB323FU 的原厂 Android 启动路径生效：校验 NVT 节点及原频率后，就地将控制器的 `spi-max-frequency` 从 19.2 MHz 调为 4.8 MHz，并在交接前清理所改 DT 的缓存。它不修改 Linux DT，也不替代 UFS 设置保存或启动器自身触摸驱动。

`android_touch_profile_test` 使用真实 Kconfig 和 `main.c` 预处理结果检查五代正常构建保留调用、四代配置不启用该处理；`android_touch_spi_trial_test` 检查 DT 修改范围与拒绝条件。签名与真机触摸复测步骤见[五代引导构建说明](../star-chart-build/README.md#android-触摸兼容与回归)。

## 许可证

除文件中另有许可声明外，kShimC 原创源码采用 **GNU 通用公共许可证第 3 版**（`GPL-3.0-only`）。许可全文见 [LICENSE](LICENSE)。

第三方代码、子模块与字体资产保留各自的许可证及版权声明。文件中已有的逐文件许可继续有效，包括 MIT、libfdt 的双重许可以及字体的 SIL Open Font License。本项目许可证不替代这些声明，也不改变这些组件原有的授权。
