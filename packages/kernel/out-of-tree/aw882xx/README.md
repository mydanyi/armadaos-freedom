# aw882xx — Awinic smart amplifier driver (out of tree)

The TB323FU drives its speakers through two Awinic smart amplifiers (chip ID `0x2308`, I²C addresses 0x34 and 0x37). Upstream Linux
has no driver for this chip (it covers AW88081/83, AW88166, AW88261, AW88395 and AW88399), so the
speakers need this vendor driver, built as an external module `snd-soc-aw882xx.ko`. It is built alongside the
[ALS bridge](../als-bridge/README.md) as an out-of-tree module.

## Origin and version

| | |
|---|---|
| Upstream of this copy | Awinic's GPL driver as carried in [rockchip-linux/kernel](https://github.com/rockchip-linux/kernel) branch `develop-6.1`, `sound/soc/codecs/aw882xx/`, at commit `1feee0d9c0b20750eef52b06b9211a4a3a353895` |
| Driver version | `v1.15.0` (`AW882XX_DRIVER_VERSION` in `aw882xx.c`; the Android kernel on the tablet runs a later Awinic release, v2.0.0, whose source is not public) |
| Authors | AWINIC Technology Co., Ltd. (copyright 2019–2020, see each file's header) |
| License | `GPL-2.0-only` (`REUSE.toml`) — every file keeps its original SPDX line and copyright header. The SPDX lines say `GPL-2.0` (the deprecated name of `GPL-2.0-only`), while the notice text in the C files says "version 2 of the License, or (at your option) any later version"; both are Awinic's, kept as they are |

The register tables for other Awinic PIDs (1852, 2013, …) are part of the vendor driver and were kept as
they are.

## Changes in this copy

All marked `tb323fu` in the source:

- `linux/of_gpio.h` is gone: the reset/IRQ lines are taken with `devm_gpiod_get_optional()` (the DT names
  `reset-gpio`/`irq-gpio` still match), `aw882xx_gpio_request()` is empty.
- ASoC API of 7.x: `snd_soc_kcontrol_component` → `snd_kcontrol_chip()`, `snd_soc_unregister_component` →
  `snd_soc_unregister_component_by_driver()`, on 7.3 `snd_soc_register_component_d` (the old name is a
  `_Generic` macro now).
- I²C `probe` without the `id` argument; `struct class` has no `owner`; class attribute callbacks take `const`.
- `MODULE_IMPORT_NS(VFS_internal...)` dropped (the namespace no longer exists).
- `MODULE_DEVICE_TABLE(of, …)` added, so the module autoloads from the device tree.
- The register dump on a failed PLL check is `dev_dbg` instead of info (PipeWire's probe opens trigger it on
  every boot, about 1,500 lines).

## Building

Against a kernel build of this project's series (the sound core must be enabled; the board fragment does):

```sh
make -C /path/to/linux-tb323fu O=/path/to/out ARCH=arm64 LLVM=1 M=$PWD CONFIG_SND_SOC_AW882XX=m modules
```

The module goes into the modules tree of the same kernel build, in `extra/`:

```sh
install -D -m 0644 snd-soc-aw882xx.ko "$ROOT/lib/modules/<release>/extra/snd-soc-aw882xx.ko"
depmod -b "$ROOT" <release>
```

(`make … M=$PWD modules_install INSTALL_MOD_PATH=$ROOT` does the same.) Release module tarballs already
contain it.

## Firmware it needs at run time — not here

When the sound card binds, the driver requests the amplifier parameter file **`aw882xx_acf.bin`** through
the firmware loader. It is Lenovo/Awinic tuning data from the tablet's vendor partition, not redistributable,
and **not part of this repository**: extract it from your own tablet with
[`firmware/extract-on-device.sh`](../../../../devices/lenovo-tb323fu/kernel/tb323fu-linux-11a25bf8/tb323fu-linux-11a25bf89b65fff48112a91a0b25bc247e7736d8/firmware/extract-on-device.sh) (it lands in `/lib/firmware/aw882xx_acf.bin`). The
driver also knows calibration (`aw_cali.bin` on Android's persist partition) and per-PID monitor files; they
are not used on Linux here.
