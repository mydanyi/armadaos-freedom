# ALS bridge for Steam adaptive brightness

Steam's ambient-light discovery recognizes an IIO device named `als` with an
`in_illuminance_raw` value in lux. Qualcomm SSC sensors exposed through
`net.hadess.SensorProxy` do not provide that sysfs interface, so Steam leaves
its adaptive-brightness control unavailable even when the desktop receives
light readings.

This module registers that minimal IIO interface and exposes a root-writable
`/sys/module/als_bridge/parameters/lux` parameter. The image's
`armada-als-bridge` service claims the real SensorProxy light sensor and copies
its lux readings into the parameter. Steam remains responsible for adjusting
the backlight and for the user's adaptive-brightness setting.

The daemon loads the module only after obtaining a valid lux reading. A machine
without an ambient-light sensor, or one reporting uncalibrated `vendor` units,
does not gain a synthetic sensor. Property changes update the value immediately;
a five-second refresh covers missed signals and retries failed claims. Claims
are renewed when SensorProxy changes its D-Bus owner. Invalid, negative and
non-finite readings are ignored; values are bounded to IIO's signed integer
range.

`90-armada-als-bridge.rules` excludes only this module's device from SensorProxy.
Without that rule, SensorProxy can select the mirrored reading as its own input
and leave both the desktop and Steam stuck in a feedback loop. Physical sensors
with the same IIO name are not excluded.

## Build and integration

`packages/kernel/scripts/build-kernel.sh` builds the module against the same
kernel and installs it under `extra/als-bridge` before the final `depmod` and
kernel archive are generated. No DKMS installation or separate kernel patch is
needed. For a standalone build against an already prepared kernel:

```sh
make -C /path/to/linux O=/path/to/out ARCH=arm64 LLVM=1 M="$PWD" modules
```

The image installs `iio-sensor-proxy` and Python GObject bindings, copies the
helper/unit/udev rule through `system_files`, and enables
`armada-als-bridge.service`. The daemon uses the existing sensor stack; it does
not change its firmware, restart it, or depend on a board-specific Hexagon
service name. Stop the bridge before manually unloading the module.

Host regression tests:

```sh
bash tests/als-bridge-test.sh
```

These cover startup without a sensor, delayed discovery, claim failure/retry,
owner replacement, units becoming available after a claim, invalid samples,
missed updates, property invalidation and the real Gio callbacks on a private
D-Bus. The D-Bus tests require PyGObject and `dbus-daemon` and are skipped when
those host dependencies are unavailable. The service and rule can also be
checked with `systemd-analyze verify` and `udevadm verify`. Hardware verification
still requires a Freedom image on a device with a working SensorProxy light
sensor: check that the mirrored sysfs value follows light changes, then enable
adaptive brightness in Steam and repeat after restarting SensorProxy.

## Origin and license

Adapted from [tb322fc-linux's ALS bridge](https://github.com/h0cheung/tb322fc-linux/tree/cb85616d6d8ecba1f7b9cd3f39fbc4ad708d9d63/packages/als-bridge).
The kernel source keeps its `GPL-2.0` SPDX identifier (`GPL-2.0-only`). The
userspace integration follows this repository's `GPL-2.0-or-later` license.
Experimental ACPI firmware-path attributes from that implementation are not
needed by Steam's `als` discovery path and are omitted here.
