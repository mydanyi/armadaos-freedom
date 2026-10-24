# Kernel overlays

Board support that should not touch the shared kernel lives here, one directory
per board. Nothing in this directory is used by the default build.

```sh
KERNEL_OVERLAYS="<name> [<name>...]" bash scripts/build-kernel.sh
```

For each named overlay, `scripts/build-kernel.sh` adds, after the shared
`patches/series`, `dts/` and config fragment:

| Path | Use |
|---|---|
| `<name>/patches/*.patch` | applied in name order with the same strict `-F0` checks as `patches/series` |
| `<name>/dts/*.dts`, `*.dtsi`, `*.dtso` | copied into `arch/arm64/boot/dts/qcom/` |
| `<name>/dts/*.mk` | Kbuild lines appended once to the qcom Makefile: `dtb-$(CONFIG_ARCH_QCOM) += x.dtb` and composite `x-dtbs := base.dtb y.dtbo`, which Kbuild merges with fdtoverlay; every `dtb-… += x.dtb` target is staged under `dtb/qcom/` |
| `<name>/config/*.config` | merged after `config/armada-kernel.config.overrides`, in name order; a later fragment overrides an earlier one, and only each symbol's last request is checked for surviving `=y` |

An overlay should set its own `CONFIG_LOCALVERSION`, so its kernel release and
module directory cannot be mistaken for the default build's.

`FAST=1` reuses a source tree only when it was patched for the same overlay
list (recorded in `linux-<version>.overlays` next to the tree); otherwise the
build stops and asks for a full rebuild.
