# Infinity scheduler overlay

Build from `packages/kernel` with:

```sh
KERNEL_OVERLAYS=infinity ./scripts/build-kernel.sh
```

This optional overlay replaces the normal fair CPU scheduler and includes
Infinity RT and DRM GPU scheduling changes. It enables a 1000 Hz kernel tick.
The default build remains unchanged.

The release suffix is `-freedom-infinity`; modules therefore install alongside
the original kernel. Keep the original boot image and matching modules before
deploying a candidate.

## Freedom Control

The builder stages `lib/modules/<kernel-release>/freedom-scheduler` containing
`infinity-v1`. Freedom Control and armada-powerd inspect only the running kernel's
marker to show its default scheduler. Saved EEVDF/Infinity selections follow the
running kernel without rewriting the user's settings.

`CONFIG_SCHED_CLASS_EXT=y` preserves support for COSMOS and LAVD. These replace
normal CPU scheduling; Infinity's RT and GPU changes remain active. Infinity is
not a sched_ext program, and there is no `scx_infinity` executable. Boot the
original kernel to restore the original EEVDF configuration.

## Sources

The 19 patches are copied without changes from
[home:h-cheung:tb322fc/kernel-source, revision 10](https://api.opensuse.org/public/source/home:h-cheung:tb322fc/kernel-source?rev=10).
Original authorship and patch headers are retained. The scheduler source used
for comparison is [galpt/infinity-sched-new at 16d5df5](https://github.com/galpt/infinity-sched-new/tree/16d5df5cb0cef0e624914fba7f884a022da8b92c).
Patches are applied in filename order by the existing overlay build mechanism.

## Checks

`python3 tests/test_infinity_package.py` executes the packaging marker block
with temporary staging directories, checking opt-in selection and the kernel
release guard. It does not build or boot a kernel. Performance and device
stability require separate hardware testing.
