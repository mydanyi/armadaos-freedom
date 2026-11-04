# Y700 五代组件来源与固定版本

本页记录 TB323FU 的板级来源，以及四代、五代共用的启动器依赖。四代板级 DTS 的来源另见[四代源码与来源](../../lenovo-tb322fc/README.md#源码与来源)。当前内核统一从 `packages/kernel/` 构建，下面的旧内核快照用于保留移植来源。

## 系统和板级内核

| 组件 | 来源 / 固定版本 |
|---|---|
| ArmadaOS 维护起点 | [armada-os/armada](https://github.com/armada-os/armada)，`887fd306b17b3060a70cc749e09630e1a640af14` |
| 板级源码 | [joonhoekim/tb323fu-linux](https://github.com/joonhoekim/tb323fu-linux)，`11a25bf89b65fff48112a91a0b25bc247e7736d8` |
| 当前统一 Linux 内核 | **7.2.6**；版本入口为 [`packages/kernel/BASE.env`](../../../packages/kernel/BASE.env)，四代与五代的板级支持共用 [`packages/kernel`](../../../packages/kernel/) 的补丁集和构建流程 |
| 历史板级内核基线 | `93f51579e7df248780214094418f205253383cc5`，v7.3-rc4 / `7.3.0-rc4-tb323fu-t42`；作为设备支持回移来源，保留原始快照 |
| G9 基础映射和 RGB 协议参考 | [enij90/armada-tb321fu](https://github.com/enij90/armada-tb321fu)，`62076530bf1ff34295d671016658965e54e597f1` |

板级来源的许可证和 `kernel/PROVENANCE.md` 保留在原源码目录。`kernel/runtime-fixes/` 的补丁与固定快照分别维护；165 Hz 补丁替换原 `0109`，不是在它之后重复追加。

## 蓝牙固件

Baldur 设备树请求 BRAHMA 系列的 `brhbtfw20.mbn` 和 `brhbtnv20.bin`，不能用 GNG 系列固件替代。`tb323fu-bluetooth-prepare` 在蓝牙启动前，只读挂载同一设备保留的原厂 `bluetooth_a` 分区（不可用或文件不完整时尝试 `bluetooth_b`），先读取 `image/` 下的这两个文件到临时目录，卸载源分区后再放入固件搜索路径的 `qca/` 目录。加载程序仅在 `lenovo,baldur` 上执行；这两份固件来自设备自身，不随仓库分发，因此需要保留原厂蓝牙固件分区。

## kShimC 和打包器

引导基于 [sunflower2333/kShimC feature/2610](https://github.com/sunflower2333/kShimC/tree/feature/2610)，打包器基于 [Project-Aloha/DualBootKernelPatcher feature/2610](https://github.com/Project-Aloha/DualBootKernelPatcher/tree/feature/2610)。

[`boot/kshim-success-source/`](../boot/kshim-success-source/README.md) 保存两代共用的 STAR_CHART 菜单源码。四代使用 `configs/tb322fc_android_defconfig`，五代统一内核使用 `configs/tb323fu_unified_defconfig`；构建时显式选择配置，不能把快照内的 `.config` 当作两代通用配置。原源码归档 SHA-256 为 `3f8dc6d08e3945c874d8f1ab0789917db9781f62f77db98fecec547c6b9b1c5e`；文本在 Git 中统一为 LF，此哈希不用于比较重新生成的 Git 归档。

| 依赖 | 固定版本 |
|---|---|
| Project-Aloha/CrDK | `b6c3335136b7f6b28271917de11e8f11982975fc` |
| picolibc/picolibc | `f7edb0869e64b9d687f46961adeef7154921fe92` |
| lvgl/lvgl | `7f07a129e8d77f4984fff8e623fd5be18ff42e74` |
| libfdt 恢复目录 | [sunflower2333/kShimC](https://github.com/sunflower2333/kShimC) 的 `82ce6eaa10c36e64cfd3f16799172d854f9a955b`，仅取 `lib/libfdt` |
| 打包器上游基线 | `2276163d066596cb0750e76cbfeec5c193b1779f` |
| 打包器本地基线 | `aa819b0d1be76e83256f64cf77c0f6dfe4174eea`，另需 `patches/boot/` 中的共享 PayloadFrom 修改 |

CrDK 需要 `ports/crdk/` 的补丁层。源码归档不包含顶层 `lib/`、`third_party/`、`.git/`，现有 `.gitmodules` 不足以自动还原全部依赖。三个 Git 依赖与 `lib/libfdt` 的恢复命令、版本核对和校验统一见[共享依赖准备指引](../boot/kshim-success-source/DEPENDENCIES.md)。对应入口见[四代引导说明](../../lenovo-tb322fc/boot/README.md)和[五代引导说明](../boot/star-chart-build/README.md)，系统内工具来源见 [kshim-tools](../../../packages/kshim-tools/README.md)。

### libfdt 恢复基线

`lib/libfdt` 按公开 `sunflower2333/kShimC` 的固定提交 `82ce6eaa10c36e64cfd3f16799172d854f9a955b` 单独恢复，只取该提交下的 `lib/libfdt`。这是经历史实物比对选定的 libfdt 恢复来源，不是整个 Freedom STAR_CHART 快照的原始提交；不要据此声称该提交对应本快照，也不要把当前上游源码整体覆盖到本快照。

该目录包含 `include/libfdt_env.h`，它是配套的 kShim freestanding 适配头。公开版本缺少历史构建适配的一行 `#include <stdlib.h>`，恢复后需在 `#include <stdint.h>` 之后补上；适配后的 LF 文件 SHA-256 为 `132cb14b8f0b323e5701d4bfcb0ebba4b1f35726bfdfa6739f18ad638d20e582`。完整的导出、`sed` 与 `sha256sum -c` 校验步骤见[共享依赖准备指引](../boot/kshim-success-source/DEPENDENCIES.md)。

恢复后的 `lib/libfdt/UPSTREAM-check.json` 只记录 dtc v1.7.0 的 `fdt_check.c` 校验，不能据此认为整个目录原样来自 dtc v1.7.0。顶层 CMake 直接编译 `lib/libfdt` 下的源码并引用其 `include/` 头目录，构建不需要另装系统 dtc 头。

`patches/boot/kshim-local-commits.mbox` 是旧阶段的增量记录，不是当前 STAR 快照的完整补丁集，不能无条件重复应用。Android payload、原厂 boot、分区备份及构建生成的镜像不随仓库分发。

### 快照许可同步（2026-10-14）

在上述 STAR_CHART 快照上同步了本项目维护的 kShimC 仓库中以下两项变更：

| 内容 | kShimC 来源提交 |
|---|---|
| 原创源码采用 `GPL-3.0-only`，保留第三方及字体的独立许可 | [`0cc6e62e18cfea269d795f9a620a8406a40d5a80`](https://github.com/sunflower2333/kShimC/commit/0cc6e62e18cfea269d795f9a620a8406a40d5a80) |
| 删除 FTM4 驱动、头文件、专用测试及构建和运行时接入；DT 探测拒绝 FTM4，保留 FTM5 和实体按键回退 | [`6527d063c03b3e31a55b5f77b77f0fe3a42b1606`](https://github.com/sunflower2333/kShimC/commit/6527d063c03b3e31a55b5f77b77f0fe3a42b1606) |

许可全文保存在 [`boot/kshim-success-source/LICENSE`](../boot/kshim-success-source/LICENSE)，与上述来源提交的文件一致。这是许可及对应清理的定向同步，不代表快照等同于当前 `feature/2610` 的全部代码；保留已有 STAR_CHART 菜单、TB323FU 适配及构建配置。原源码归档哈希记录的是同步前的历史输入，不是当前目录的哈希。

## 字体

WenKai 菜单资产来自 LXGW WenKai Regular v1.522，固定提交 `e8b5b48b79f19f29aa68b0a178eab3472ea9f7e8`。资产目录保留来源 JSON、OFL 文本和菜单子集；新增翻译时需重新验证字形覆盖。
