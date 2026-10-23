<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset=".github/assets/armada-mark-white.svg">
    <img src=".github/assets/armada-mark-black.svg" alt="Armada" width="112">
  </picture>
</p>

<h1 align="center">ArmadaOS Freedom</h1>

<p align="center"><strong>English</strong> · <a href="README.zh-CN.md">简体中文</a></p>

<p align="center"><strong>SteamOS-like Linux for ARM tablets</strong></p>

<p align="center">
  A tablet-focused fork of <a href="https://github.com/armada-os/armada">ArmadaOS</a>,
  bringing Steam Gaming Mode, FEX, Proton, and KDE Plasma to ARM64 tablets,
  with kShimC multi-boot.
</p>

<p align="center">
  <a href="https://github.com/armada-os/armada"><img alt="Based on ArmadaOS" src="https://img.shields.io/badge/based_on-ArmadaOS-18181a?style=flat"></a>
  <a href="#device-support"><img alt="Y700 Gen 4 and Gen 5 tablet adaptation" src="https://img.shields.io/badge/devices-Y700_Gen_4_%26_5-285580?style=flat"></a>
  <a href="LICENSE"><img alt="GPL-2.0-or-later license" src="https://img.shields.io/badge/license-GPL--2.0--or--later-18181a?style=flat"></a>
</p>

<p align="center">
  <a href="#device-support"><strong>Device support &amp; status</strong></a>
  ·
  <a href="#kshimc-multi-boot">Boot menu</a>
  ·
  <a href="#documentation">Documentation</a>
  ·
  <a href="https://armadaos.dev/">Upstream documentation</a>
</p>

## About ArmadaOS Freedom

ArmadaOS Freedom is a community-maintained tablet branch based on
[ArmadaOS](https://github.com/armada-os/armada). It retains the upstream gaming
and desktop foundation while adapting boot, display, power, and peripherals to
tablet hardware, and ships
[Freedom Control](https://github.com/mydanyi/freedom-control) as its control
plugin.

Highlights of the current adaptation:

- Steam Gaming Mode and KDE Plasma, with FEX and Proton from ArmadaOS.
- A shared kernel build with device-specific device trees, drivers, and hardware
  configuration.
- kShimC multi-boot with separate internal-storage and TF-card Linux entries.
- Device-specific display modes, touch, power, audio, and network configuration.
- Dual-USB support, plus battery and temperature reporting.
- Tablet touch input, plus G9 / G9 2026 controller mapping, RGB control, and
  double-Home quick access.
- Tablet-aware [Freedom Control](https://github.com/mydanyi/freedom-control),
  including hiding fan controls on fanless devices.

Feature availability varies by device; see the device pages below for details.
This repository maintains adaptation sources and patches. Tablet installation
images and OTA integration are still in development.

## Device support

Current device work covers Lenovo Legion Y700 Gen 4 and Gen 5. Both share a Linux
**7.2.6** kernel, with separate device trees and hardware configuration. The Lenovo
Xiaoxin Pad Pro GT (TB710FU, SM8650) is supported by an opt-in kernel overlay
and the shared kShimC launcher; its hardware is validated with a SteamOS
userspace, not yet with an ArmadaOS image.

| Device | Model / SoC | Documentation | Known issues |
|---|---|---|---|
| Lenovo Legion Y700 Gen 4 | TB322FC / SM8750 | [Gen 4 documentation](devices/lenovo-tb322fc/README.md) | [Gen 4 known issues](devices/lenovo-tb322fc/README.md#已知问题与待完成项) |
| Lenovo Legion Tab Gen 5 / Y700 Gen 5 | TB323FU / SM8850P | [Gen 5 documentation](devices/lenovo-tb323fu/README.md) | [Gen 5 known issues](devices/lenovo-tb323fu/README.md#已知限制) |
| Lenovo Xiaoxin Pad Pro GT | TB710FU / SM8650 | [TB710FU documentation](devices/lenovo-tb710fu/README.md) | [TB710FU known issues](devices/lenovo-tb710fu/README.md#已知问题与待完成项) |

Boot images remain model-specific and must not be cross-flashed.

## kShimC multi-boot

The tablet boot path uses **kShimC**, packaged with the boot payloads in the
Android boot image. It keeps the factory boot chain and presents a menu for:

- Android.
- ArmadaOS installed in internal storage.
- ArmadaOS installed on a TF / microSD card.
- Advanced boot: Linux diagnostics, Recovery, Fastboot, and 9008 / EDL.
- Settings, device information, language selection, restart, and shutdown.

The Linux kernel and device tree are boot payloads; Linux then mounts the root
filesystem selected by the internal-storage or TF-card entry. These are two
separate installation targets. See each device page for available boot entries.

<p align="center">
  <img src=".github/assets/kshimc-star-chart-en.png" alt="KShimC launcher — STAR_CHART theme, English menu" width="100%">
</p>

*STAR_CHART menu rendered from the current implementation; this is a software
preview, not a photograph of the tablet.*

Both generations share the [kShimC launcher sources](devices/lenovo-tb323fu/boot/kshim-success-source/README.md),
with separate device profiles and boot payloads. See the
[Gen 4 boot guide](devices/lenovo-tb322fc/boot/README.md) and
[Gen 5 boot guide](devices/lenovo-tb323fu/boot/star-chart-build/README.md)
for their build paths.

## Documentation

| I want to… | Guide |
|---|---|
| Build a boot image | [Gen 4 boot guide](devices/lenovo-tb322fc/boot/README.md) · [Gen 5 boot guide](devices/lenovo-tb323fu/boot/star-chart-build/README.md) |
| Inspect the installed boot tools | [kShimC packaging tools](packages/kshim-tools/README.md) |
| Check component origins and versions | [Gen 4 sources](devices/lenovo-tb322fc/README.md#源码与来源) · [Gen 5 sources](devices/lenovo-tb323fu/docs/SOURCES.md) |
| Learn the upstream system | [Using ArmadaOS](https://armadaos.dev/using-armada/) |
| Report a tablet-branch issue | [Freedom issues](https://github.com/mydanyi/armadaos-freedom/issues) |

Upstream installation instructions describe upstream-supported devices. Use the
model-specific documentation here for tablet installation and boot.

## Development

System images use ArmadaOS's bootc build foundation. The shared kernel lives in
[`packages/kernel/`](packages/kernel/), hardware profiles in
[`system_files/usr/lib/armada/devices/`](system_files/usr/lib/armada/devices/),
and device-specific resources and boot guides in [`devices/`](devices/).
Package development is documented in [`packages/`](packages/README.md). Recipes use
[just](https://just.systems/) and [Podman](https://podman.io/):

```console
$ git submodule update --init --recursive
$ just check     # Run the test suite and check recipe formatting
$ just packages  # Build the packages the image consumes
$ just build     # Build the bootc container; not a ready-to-flash tablet image
$ just --list    # List development recipes
```

### Freedom Control

The control plugin ships from the pinned
[Freedom Control](https://github.com/mydanyi/freedom-control) dependency at
`decky/freedom-control`. This repository no longer maintains an inline copy of
the plugin source, so a fresh clone needs the pinned revision:

```console
$ git submodule update --init --recursive
```

To move to a newer plugin revision, update the dependency deliberately and
commit the new pointer:

```console
$ git submodule update --remote decky/freedom-control
$ just check
$ git add decky/freedom-control
$ git commit -m "Update Freedom Control"
```

The plugin keeps its installed directory and its service and socket interfaces,
so existing settings and integrations continue to work. A few SM8850 companion
files — the power daemon, the TB323FU board profile, and the SM8850 power-profile
layer — come from the same dependency; the rest of the host OS remains here.

Follow the configuration and build guide for the target device. System updates
use Freedom's own repository; see each device page for boot synchronization
and OTA support status.

## Credits

- System foundation: [ArmadaOS](https://github.com/armada-os/armada), built on
  [Fedora bootc](https://github.com/bootc-dev/bootc).
- Boot menu: [kShimC](https://github.com/sunflower2333/kShimC/tree/feature/2610).
- Control plugin: [Freedom Control](https://github.com/mydanyi/freedom-control).
- Armada logo: [Rax](https://github.com/Raxcoms).

See the [upstream credits](https://armadaos.dev/project/credits/) for more
contributors and the [device documentation](#device-support) for component
origins and pinned versions. Individual components retain their source
attribution and licenses.

## License

Original armadaos-freedom software and scripts are licensed under **GPL-2.0-or-later**, unless a file or component specifies otherwise. Copyright (C) 2026 danyi and the armadaos-freedom contributors, for their original contributions. Inherited ArmadaOS code and other components retain their copyrights and applicable licenses. See [LICENSE](LICENSE) and [licensing and copyright](LICENSING.md) ([简体中文](LICENSING.zh-CN.md)).
