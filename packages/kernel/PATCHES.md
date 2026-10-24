# Patches

Patches applied on top of BASE.env. Each entry's `source` is a URL pinned to a
commit, or `armada` for original work; a URL source with no `notes` is verbatim.
`notes` explain modifications or important differences from linked upstream work.
A patch entry's `upstream` is `local` for an Armada-authored change, `unknown` when
no equivalent submission was found, or a permanent URL to the upstream submission.

- `patches/0002-qcom-dispcc-sm8550-Fix-disp_cc_mdss_mdp_clk_src.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/patches/linux/0002-qcom-dispcc-sm8550-Fix-disp_cc_mdss_mdp_clk_src.patch
  upstream: unknown
- `patches/0004-drm-msm-a6xx-Enable-IFPC-on-Adreno-740.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/patches/linux/0004-drm-msm-a6xx-Enable-IFPC-on-Adreno-740.patch
  upstream: unknown
- `patches/0010-msm-resource-cleanup.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/packages/linux/patches/7.0/0010-msm-resource-cleanup.patch
  upstream: unknown
  notes: Armada initialises cstate before the num_mixers reset it adds; the ROCKNIX version writes through an uninitialised pointer.
- `patches/0048-drm-msm-dsi-reparent-byte-pixel-src-to-xo-on-disable.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8750/patches/linux/0048-drm-msm-dsi-reparent-byte-pixel-src-to-xo-on-disable.patch
  upstream: unknown
- `patches/0048a-drm-msm-dsi-round-byte-clock-rate-after-reparenting-to-PLL.patch`
  source: https://git.kernel.org/torvalds/c/2028280686f4fa78e2f1f6dede4b6c1fd782b9e3
  upstream: https://lore.kernel.org/r/20260903-fix-eliza-dsi-v1-1-3474a6c9f2e0@oss.qualcomm.com
  notes: Context of the struct msm_dsi_host hunk refreshed to apply after `0048`.
- `patches/0048b-drm-msm-dsi-fix-pll-init-in-bonded-mode.patch`
  source: armada
  upstream: local
- `patches/0049-drm-msm-dpu-panel-opt-in-8bpc-dither.patch`
  source: armada
  upstream: local
  notes: Armada wrote this patch; it has not been submitted upstream.
- `patches/0066-drm-msm-dpu-enable-inline-rotation.patch`
  source: https://github.com/ROCKNIX/distribution/blob/60bb58c1db053255b700f3953b72d619ec5aa85d/projects/ROCKNIX/devices/SM8550/patches/linux/0066-drm-msm-dpu-enable-sm8550-inline-rotation.patch
  upstream: unknown
  notes: Armada merged the ROCKNIX SM8550, SM8650 and SM8750 patches into one for the combined kernel, with one shared feature mask and rotation config.
- `patches/0067-drm-msm-dpu-enable-qseed-detail-enhancer.patch`
  source: https://github.com/ROCKNIX/distribution/blob/60bb58c1db053255b700f3953b72d619ec5aa85d/projects/ROCKNIX/devices/SM8550/patches/linux/0067-drm-msm-dpu-enable-qseed-detail-enhancer.patch
  upstream: unknown
- `patches/0068-drm-msm-dpu-lutdma-dspp-igc-gamut.patch`
  source: armada
  upstream: local
- `patches/0069-dt-bindings-display-msm-dpu-lutdma-reg.patch`
  source: armada
  upstream: local
- `patches/0070-arm64-dts-qcom-dpu-lutdma-reg.patch`
  source: armada
  upstream: local
- `patches/0076-drm-msm-dpu-expose-abgr-xbgr-2101010-on-planes.patch`
  source: armada
  upstream: local
- `patches/0077-drm-msm-dpu-plane-igc-3d-lut-color-pipelines.patch`
  source: armada
  upstream: local
- `patches/0078-drm-msm-dpu-fix-vblank-timestamps-on-command-mode-panels.patch`
  source: armada
  upstream: local
- `patches/0016-rp5-smooth-brightness-adjustment.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8250/patches/linux/0016-rp5-smooth-brightness-adjustment.patch
  upstream: unknown
  notes: Armada limited the ROCKNIX brightness change to SM8250 panels. A [related upstream proposal](https://lore.kernel.org/r/20260706180753.408753-1-kavansmith82@gmail.com) uses a different implementation.
- `patches/0028-drm-panel-Add-panel-driver-for-Chipone-ICNA35XX-base.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8750/patches/linux/0028-drm-panel-Add-panel-driver-for-Chipone-ICNA35XX-base.patch
  upstream: https://lore.kernel.org/r/20260607-icna35xx-v4-2-64de514add34@gmail.com
  notes: The carried implementation differs from the linked upstream submission and is ported to Linux 7.2's managed DRM panel allocator.
- `patches/0051-gpu-panel-add-Pocket-ACE-panel-driver.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/patches/linux/0051-gpu-panel-add-Pocket-ACE-panel-driver.patch
  upstream: unknown
  notes: Ported to Linux 7.2's managed DRM panel allocator.
- `patches/0052-gpu-panel-add-Pocket-DMG-panel-driver.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/patches/linux/0052-gpu-panel-add-Pocket-DMG-panel-driver.patch
  upstream: unknown
  notes: Ported to Linux 7.2's managed DRM panel allocator.
- `patches/0053-gpu-panel-st7703-add-Pocket-DS-lower-panel.patch`
  source: https://github.com/ROCKNIX/distribution/blob/7651a1b97d762d2c7dc335fa82a7a254c3a12927/projects/ROCKNIX/devices/SM8550/patches/linux/0053-gpu-panel-st7703-add-Pocket-DS-lower-panel.patch
  upstream: https://lore.kernel.org/r/20260723-b4-st7703-pocketds-lower-v1-2-e3db246589f4@gmail.com
- `patches/0055_Synaptics-TD4328-LCD-panel.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/patches/linux/0055_Synaptics-TD4328-LCD-panel.patch
  upstream: https://lore.kernel.org/r/20240424-ayn-odin2-initial-v1-4-e0aa05c991fd@gmail.com
  notes: The carried implementation predates the linked upstream submission and is ported to Linux 7.2's managed DRM panel allocator.
- `patches/0056_Xm-Plus-XM91080G-panel.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/patches/linux/0056_Xm-Plus-XM91080G-panel.patch
  upstream: unknown
  notes: Ported to Linux 7.2's managed DRM panel allocator.
- `patches/0057_DDIC-CH13726A-panel.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/patches/linux/0057_DDIC-CH13726A-panel.patch
  upstream: unknown
  notes: Retains additional compatibles and the 120 ms sleep-out delay, using Linux 7.2's managed DRM panel allocator.
- `patches/0059-drm-panel-add-brightness-levels-helper.patch`
  source: armada
  upstream: not submitted
- `patches/0060-drm-panel-icna35xx-luminance-linear-backlight-scale.patch`
  source: armada
  upstream: not submitted
- `patches/0060a-drm-panel-icna35xx-add-mangmi-pocket-max.patch`
  source: https://github.com/ROCKNIX/distribution/blob/807c74fc46c124891d8aa9b57a68533d38e9a6b2/projects/ROCKNIX/devices/SM8250/patches/linux/0057_Chipone-ICNA35XX-panel.patch
  upstream: unknown
  notes: Ports the Pocket Max init sequence, four vendor modes and per-panel shutdown delays onto Armada's Linux 7.2 managed-panel driver while retaining Armada's luminance-linear backlight scale.
- `patches/0106-drm-panel-il97680a-luminance-linear-backlight-scale.patch`
  source: armada
  upstream: not submitted
- `patches/0062-gpu-drm-panel-add-wt0630-panel.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/patches/linux/0062-gpu-drm-panel-add-wt0630-panel.patch
  upstream: https://lore.kernel.org/r/20260625-topic-sm8650-ayaneo-pocket-s2-r63419-v8-2-8570e692143e@linaro.org
  notes: Ported to Linux 7.2's managed DRM panel allocator.
- `patches/0063-gpu-drm-panel-add-wt0600-1k-panel.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/patches/linux/0063-gpu-drm-panel-add-wt0600-1k-panel.patch
  notes: Standalone single-DSI driver for the 1080x1920 60Hz WT0600 panel (Pocket S 1K) from ROCKNIX PR #3007.
- `patches/0104-drm-panel-Add-Retroid-Pocket-6-panel.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/patches/linux/0104-drm-panel-Add-Retroid-Pocket-6-panel.patch
  upstream: unknown
  notes: Ported to Linux 7.2's managed DRM panel allocator.
- `patches/0105-drm-panel-Add-Retroid-Pocket-Nova-panel.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/patches/linux/0105-drm-panel-Add-Retroid-Pocket-Nova-panel.patch
  upstream: unknown
  notes: Ported to Linux 7.2's managed DRM panel allocator.
- `patches/0104a-drm-panel-visionox-vtdr6130-add-sm8250.patch`
  source: https://github.com/armada-os/armada/issues/327
  upstream: unknown
  notes: Adds support for Visionox VTDR6130 panel revisions found in the Retroid Pocket Flip 2, derived from the affected device's live Android device tree. Reuses the Pocket 6 initialization sequence and DSC configuration with the Flip 2 regulator supplies.
- `patches/0064_Mangmi-Air-Y-Pro-panel.patch`
  source: https://github.com/ROCKNIX/distribution/blob/807c74fc46c124891d8aa9b57a68533d38e9a6b2/projects/ROCKNIX/devices/SM8250/patches/linux/0064_Mangmi-Air-Y-Pro-panel.patch
  upstream: unknown
- `patches/0058_AYN-Odin2-Mini--backlight.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/patches/linux/0058_AYN-Odin2-Mini--backlight.patch
  upstream: unknown
  notes: Armada refreshed only the Makefile context for Linux 7.1's backlight object list; driver behavior is unchanged.
- `patches/0060-backlight-Add-SY7758-LED-driver.patch`
  source: https://patchwork.kernel.org/project/dri-devel/patch/20260529-topic-sm8650-ayaneo-pocket-s2-sy7758-v5-2-03aacd49747c@linaro.org/
  upstream: https://lore.kernel.org/r/178300990349.2239788.13080024963462152507.b4-ty@b4
  notes: Armada made the enable GPIO optional (`devm_gpiod_get_optional`). The AYANEO Pocket DS wires no discrete enable line to its SY7758, and requiring one left the whole lower DSI chain deferred; boards that do have one are unaffected. The binding patch carries the matching change.
- `patches/0060-dt-bindings-silergy-sy7758.patch`
  source: https://patchwork.kernel.org/project/dri-devel/patch/20260529-topic-sm8650-ayaneo-pocket-s2-sy7758-v5-1-03aacd49747c@linaro.org/
  upstream: https://lore.kernel.org/r/178300990349.2239788.13080024963462152507.b4-ty@b4
  notes: Armada dropped `enable-gpios` from the required list to match the driver change above; the AYANEO Pocket DS reaches its SY7758 without a separately described enable pin.
- `patches/0054-regulator-add-sgm3804-i2c-regulator-for-panel-power-.patch`
  source: armada
  upstream: local
  notes: Armada adds compatibility for existing flat fixed-5-V nodes; child-regulator nodes use the Linux 7.2 upstream behavior unchanged.
- `patches/0056-backlight-aw99706-enable-hwen-before-chip-id-read.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8750/patches/linux/0056-backlight-aw99706-enable-hwen-before-chip-id-read.patch
  upstream: unknown
- `patches/0057-backlight-aw99706-konkr-cfg7.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8750/patches/linux/0057-backlight-aw99706-konkr-cfg7.patch
  upstream: unknown
  notes: Armada scopes the CFG7 write to the `konkr,pocket-fit-elite` compatible, so the existing KONKR behavior is kept and this board-specific value is not applied to Lenovo Elden.
- `patches/0066-regulator-add-qcom-pm8150l-lcdb.patch`
  source: https://github.com/ROCKNIX/distribution/blob/807c74fc46c124891d8aa9b57a68533d38e9a6b2/projects/ROCKNIX/devices/SM8250/patches/linux/0066-regulator-add-qcom-pm8150l-lcdb.patch
  upstream: unknown
- `patches/0015-touchscreen-edt-ft5x06-allow-to-override-input-name.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8250/patches/linux/0015-touchscreen-edt-ft5x06-allow-to-override-input-name.patch
  upstream: https://lore.kernel.org/r/20260409-ft5x06-label-v1-1-21e8a9ae9a60@gmail.com
- `patches/0030-input-rmi4-add-reset-gpio.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/patches/linux/0030-input-rmi4-add-reset-gpio.patch
  upstream: https://lore.kernel.org/r/20250210050220.634497-2-felix@kaechele.ca
- `patches/0032-rmi4-silence-spam-irq-errors.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/patches/linux/0032-rmi4-silence-spam-irq-errors.patch
  upstream: unknown
- `patches/0053-add-hynitron-touchscreen.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/patches/linux/0053-add-hynitron-touchscreen.patch.disabled
  upstream: unknown
  notes: Armada retains this disabled ROCKNIX driver for the Pocket DMG and refreshed only its Makefile context for Linux 7.1's Himax entry; driver behavior is unchanged.
- `patches/0053a-input-hynitron-port-to-gpiod.patch`
  source: armada
  upstream: local
  notes: Armada added this follow-up because Linux 7.1 removed the legacy OF GPIO-number API; it ports the retained Hynitron driver to GPIO descriptors.
- `patches/0053b-input-hynitron-restore-gpio-init-order.patch`
  source: armada
  upstream: local
  notes: Armada added this follow-up to preserve the vendor driver's pinctrl, GPIO-direction, power, and reset ordering after the GPIO-descriptor port.
- `patches/0065-input-touchscreen-add-hynitron-cst66xx.patch`
  source: https://github.com/ROCKNIX/distribution/blob/807c74fc46c124891d8aa9b57a68533d38e9a6b2/projects/ROCKNIX/devices/SM8250/patches/linux/0065-input-touchscreen-add-hynitron-cst66xx.patch
  upstream: unknown
- `patches/0054-input-goodix-override-resolution-from-dt.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/patches/linux/0054-input-goodix-override-resolution-from-dt.patch
  upstream: unknown
  notes: Armada replaced ROCKNIX's automatic resolution-mismatch heuristic and hardcoded fallback with explicit `goodix,native-size-x` and `goodix,native-size-y` properties. Scaling is dormant unless a device opts in with both native and standard touchscreen dimensions.
- `patches/0054a-dt-bindings-input-goodix-document-input-device-label.patch`
  source: armada
  upstream: local
  notes: Documents the optional Goodix `label` property used to name the input device.
- `patches/0054b-input-goodix-allow-overriding-input-device-name.patch`
  source: armada
  upstream: local
  notes: Allows the Pocket DS lower GT911 to expose its `bottom_touchscreen` label as the input device name.
- `patches/0029-Input-edt-ft5x06-add-no_regmap_bulk_read-option.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8750/patches/linux/0029-Input-edt-ft5x06-add-no_regmap_bulk_read-option.patch
  upstream: https://lore.kernel.org/r/20260723-b4-ft5426-v1-3-d4b4e32be042@gmail.com
- `patches/0059_AYN-Odin2-Mini--hynitron--cstxxx.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/patches/linux/0059_AYN-Odin2-Mini--hynitron--cstxxx.patch
  upstream: unknown
- `patches/0060-input-touchscreen-add-synaptics-dsx-kconfig.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/patches/linux/0060-input-touchscreen-add-synaptics-dsx-kconfig.patch
  upstream: unknown
- `patches/0060-input-touchscreen-add-synaptics-dsx-driver.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/patches/linux/0060-input-touchscreen-add-synaptics-dsx-driver.patch
  upstream: unknown
- `patches/0140-input-touchscreen-add-novatek-nvt36xxx-spi.patch`
  source: https://github.com/h0cheung/tb322fc-linux-kernel/commit/6f97250ead155fc3b980856323e99a4623dae73d
  upstream: unknown
  notes: Verbatim import of the touchscreen driver and its binding plus the Kconfig/Makefile registration; no board DTS, panel, or firmware bundled.
- `patches/0058-input-joystick-add-ayaneo-mcu-joystick.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8750/patches/linux/0058-input-joystick-add-ayaneo-mcu-joystick.patch
  upstream: unknown
- `patches/0069-input-misc-add-konkr-sysbtn-MCU-system-buttons.patch`
  source: https://github.com/ROCKNIX/distribution/blob/024028b4f3fdd139ae1e575efb25914b8f76595d/projects/ROCKNIX/devices/SM8750/patches/linux/0069-input-misc-add-konkr-sysbtn-MCU-system-buttons.patch
  upstream: unknown
  notes: Updated to ROCKNIX #3089, which adds the MCU-rendered stick lighting effects (sysfs "effect": static/breath/rainbow) and fixes the PM notifier to replay the active lighting mode after resume instead of only the last static colour.
- `patches/0060_Mangmi-Pocket-Max-SPI-joypad.patch`
  source: https://github.com/ROCKNIX/distribution/blob/807c74fc46c124891d8aa9b57a68533d38e9a6b2/projects/ROCKNIX/devices/SM8250/patches/linux/0060_Mangmi-Pocket-Max-SPI-joypad.patch
  upstream: unknown
- `patches/0060b-input-mangmi-pocket-max-keep-trigger-conversion-independent-of-calibration.patch`
  source: armada
  upstream: local
- `patches/0031_input--Add-driver-for-RSInput-Gamepad.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/patches/linux/0031_input--Add-driver-for-RSInput-Gamepad.patch
  upstream: unknown
- `patches/0508-input-rsinput-add-pm-resume-to-reinit-mcu-after-suspend.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8750/patches/linux/0508-input-rsinput-add-pm-resume-to-reinit-mcu-after-suspend.patch
  upstream: unknown
- `patches/0515-input-rsinput-reassemble-uart-frames-instead-of-dropping-them.patch`
  source: armada
  upstream: not submitted
  notes: serdev delivers arbitrary byte batches and the old parser dropped any batch that was not exactly one frame, visible as resume-correlated checksum mismatches on the rsinput gamepad devices.
- `patches/0504-Enable-64-bit-processes-to-use-compat-input-syscalls.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8250/patches/linux/0504-Enable-64-bit-processes-to-use-compat-input-syscalls.patch
  upstream: unknown
  notes: Armada rebased the unchanged compat-input prctl behavior onto Linux 7.1's current task-structure, UAPI, and prctl switch layout.
- `patches/0506-usbcore-add-interrupt-interval-override.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/patches/linux/0506-usbcore-add-interrupt-interval-override.patch
  upstream: unknown
- `patches/0071-HACK-fix-usb-boot-hang.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/patches/linux/0071-HACK-fix-usb-boot-hang.patch
  upstream: unknown
  notes: ROCKNIX carries this workaround in its SM8550 and SM8650 kernels. Armada preserves the normal xHCI readiness handshake on every other SoC.
- `patches/1000-input-misc-qcom-hv-haptics-sm8750.patch`
  source: https://github.com/ROCKNIX/distribution/blob/47e077d0bbc714abb05afea6b68144b6f28ee19a/projects/ROCKNIX/devices/SM8750/patches/linux/1000-input-misc-qcom-hv-haptics-sm8750.patch
  upstream: unknown
- `patches/1001-input-qcom-haptics-update-assign-str.patch`
  source: armada
  upstream: local
  notes: Armada updates the vendored driver for Linux 7.1's one-argument `__assign_str()` API.
- `patches/1001-haptics-driver-support-periodic-sine-and-fixes.patch`
  source: https://github.com/ROCKNIX/distribution/blob/47e077d0bbc714abb05afea6b68144b6f28ee19a/projects/ROCKNIX/devices/SM8750/patches/linux/1001-haptics-driver-support-periodic-sine-and-fixes.patch
  upstream: unknown
- `patches/1002-input-rsinput-sm8750-ff.patch`
  source: https://github.com/ROCKNIX/distribution/blob/47e077d0bbc714abb05afea6b68144b6f28ee19a/projects/ROCKNIX/devices/SM8750/patches/linux/1002-input-rsinput-sm8750-ff.patch
  upstream: unknown
  notes: Armada rebased ROCKNIX's force-feedback bridge because its final hunk is malformed and it targets a different RSInput revision. ROCKNIX enables rumble for every RSInput device; Armada gates it with a DT property set only by the supported SM8550 and SM8750 trees that also instantiate the Qualcomm haptics device. The SM8550 common tree intentionally covers both AYN and Retroid products.
- `patches/1003-input-haptics-fix-rumble-bridge-atomic-sleep.patch`
  source: armada
  upstream: local
  notes: The imported bridge called the haptics driver's sleeping upload, playback, and erase callbacks under spin_lock_irq, which emitted "BUG: scheduling while atomic" on every rumble stop and intermittently hard-locked the Odin 3 (reproduced on hardware). The bridge now serializes with a mutex and is process-context only, RSInput defers its atomic playback callback to a work item, and the haptics suspend path cancels the pending stop and set-gain workers so they cannot fire into resume. ROCKNIX carries the identical bug as of 2026-08-04; worth upstreaming once soak-tested.
- `patches/1004-input-haptics-stop-zero-amplitude-immediately.patch`
  source: armada
  upstream: local
  notes: The imported periodic-sine compatibility patch changed zero-amplitude updates to gain 1, then stopped the motor five seconds later. The configured brake pattern made that stop a distinct delayed tap. Zero-amplitude updates now erase immediately, a following playback request cannot restart stale motor state when no effect is loaded, and in-place gain updates balance their transient runtime-PM reference.
- `patches/1005-input-rsinput-quiesce-the-mcu-across-system-sleep.patch`
  source: https://github.com/shuuri-labs/pocknix-os/blob/d2544c1481b55e9dec18ff74a8751d4a67b351f8/kernel/sm8550/patches/20-sm8550/1004-input-rsinput-suspend-resume-gamepad-mcu.patch
  upstream: local
  notes: Carries only the suspend-side MCU quiesce from the source patch; the source's resume and pm_ops additions are omitted because this tree already has both and rsinput_init_commands() performs the full power-on itself.
- `patches/1006-tty-serial-qcom-geni-mask-non-console-irq-on-suspend.patch`
  source: https://github.com/thorch-os/thorch/blob/2614a262d7de3f31bd47a0c92981461146663847/packages/linux-thorch/patches/0010-tty-serial-qcom-geni-mask-non-console-irq-on-suspend.patch
  upstream: unknown
- `patches/0534-serial-qcom-geni-add-force-suspend-resume-to-system-sleep-callbacks.patch`
  source: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=d0cd9c8d0fd59bc7d140f3d60cf02e1d80376dab
  upstream: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=d0cd9c8d0fd59bc7d140f3d60cf02e1d80376dab
  notes: Backported to Linux 7.2 on top of 1006, re-enabling the masked IRQ when the force suspend or force resume fails.
- `patches/1007-input-rsinput-drop-the-mcu-supply-across-system-sleep.patch`
  source: armada
  upstream: local
- `patches/1300-input-rsinput-axis-deadzone.patch`
  source: armada
  upstream: local
  notes: Armada replaced ROCKNIX's global experimental Odin 3 range and deadzone defaults with a per-device `axis-deadzone` property. The Odin 3 DTS supplies its range and deadzone explicitly.
- `patches/1301-input-rsinput-decouple-trigger-reference-from-calibration.patch`
  source: armada
  upstream: local
  notes: Keeps the RSInput MCU trigger release reference independent of the calibrated evdev maximum, fixing incomplete travel and deteriorating repeated calibration on AYN devices (issue #423).
- `patches/0059-ASoC-aw88395-lib-skip-monitor-sections-in-V1-ACF-parse.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8750/patches/linux/0059-ASoC-aw88395-lib-skip-monitor-sections-in-V1-ACF-parse.patch
  upstream: unknown
- `patches/0063-ASoC-wcd939x-route-usbss-on-integrated-jack-insert.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8750/patches/linux/0063-ASoC-wcd939x-route-usbss-on-integrated-jack-insert.patch
  upstream: unknown
- `patches/0064-ASoC-wcd-mbhc-clear-stale-jack-report-on-mechanical-removal.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8750/patches/linux/0064-ASoC-wcd-mbhc-clear-stale-jack-report-on-mechanical-removal.patch
  upstream: unknown
- `patches/0036_ASoC--qcom--sc8280xp-Add-support-for-Primary-I2S.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/patches/linux/0036_ASoC--qcom--sc8280xp-Add-support-for-Primary-I2S.patch
  upstream: https://lore.kernel.org/r/20251008-topic-sm8x50-next-hdk-i2s-v2-3-6b7d38d4ad5e@linaro.org
- `patches/0032-ASoC-codecs-aw88166-AYN-Products-Specific-modificati.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8750/patches/linux/0032-ASoC-codecs-aw88166-AYN-Products-Specific-modificati.patch
  upstream: unknown
- `patches/0030-leds-Add-driver-for-HEROIC-HTR3212.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8750/patches/linux/0030-leds-Add-driver-for-HEROIC-HTR3212.patch
  upstream: unknown
- `patches/0061_leds-aw200xx-vdd-supply.patch`
  source: https://github.com/ROCKNIX/distribution/blob/807c74fc46c124891d8aa9b57a68533d38e9a6b2/projects/ROCKNIX/devices/SM8250/patches/linux/0061_leds-aw200xx-vdd-supply.patch
  upstream: unknown
- `patches/0054_sn3112-pwm-driver.patch`
  source: https://github.com/ROCKNIX/distribution/blob/4609c5017f350e6e2307ec909e328454d5bec062/projects/ROCKNIX/devices/SM8550/patches/linux/0054_sn3112-pwm-driver.patch
  upstream: https://lore.kernel.org/r/20240424-ayn-odin2-initial-v1-2-e0aa05c991fd@gmail.com
  notes: Includes ROCKNIX #3110's fix for the arm64 probe crash: set_bit()/clear_bit() on a cast uint8_t[3] alignment-faults under LSE atomics, replaced with plain bitwise ops under priv->lock. The matching Odin 2 DTS change re-enables the sn3112 nodes that were disabled to dodge the crash.
- `patches/20260424_neil_armstrong_arm64_dts_qcom_sm8_456_50_add_missing_cx_power_domain_to_gcc.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/patches/linux/20260424_neil_armstrong_arm64_dts_qcom_sm8_456_50_add_missing_cx_power_domain_to_gcc.patch
  upstream: https://lore.kernel.org/r/20260615-topic-sm8x50-tie-gcc-to-cx-v2-0-6b5752dd4747@linaro.org
- `patches/v9_20260729_qualcomm_crypto_qce_runtime_pm_interconnect.patch`
  source: https://lore.kernel.org/r/20260729110455.641256-1-kuldeep.singh@oss.qualcomm.com
  upstream: https://lore.kernel.org/r/20260729110455.641256-1-kuldeep.singh@oss.qualcomm.com
  notes: Refreshed only its include context for Linux 7.2; runtime-PM and interconnect behavior is unchanged.
- `patches/0504-mailbox-qcom-ipcc-mask-summary-irq-for-suspend-to-ram.patch`
  source: https://github.com/ROCKNIX/distribution/commit/f955f5b6137554253e4d52bbe72f9b4936fb253d
  upstream: not submitted
  notes: Reworked from ROCKNIX's blanket IRQF_NO_SUSPEND removal: the summary
  irq is masked (unlazy) only for suspend-to-RAM on the SM8550/SM8750
  compatibles, so suspend-to-idle keeps the doorbell live and charging can
  start while asleep.
- `patches/0505-msm_gem-lock-before-put_iova_spaces.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8250/patches/linux/0505-msm_gem-lock-before-put_iova_spaces.patch
  upstream: unknown
- `patches/0204-thermal-qcom-tsens-mask-lower-threshold-irqs-across-suspend.patch`
  source: armada
  upstream: local
  notes: Lower threshold crossings during the suspend transition cannot be serviced while thermal zones are quiesced, so the level IRQ storms and aborts suspend; masking LOWER across the transition removes the cause and keeps upper/critical wake armed.
- `patches/0510-soc-qcom-ice-unwind-clocks-on-resume-failure.patch`
  source: armada
  upstream: local
- `patches/0511-scsi-ufs-qcom-reenable-irq-on-host-reset-failure.patch`
  source: armada
  upstream: local
- `patches/0533-scsi-ufs-ufs-qcom-enable-only-lane-clocks-in-lane-clock-apis.patch`
  source: https://git.kernel.org/pub/scm/linux/kernel/git/mkp/scsi.git/commit/?id=f07317a8d57f382ec505597816271dd72ffa20c7
  upstream: https://git.kernel.org/pub/scm/linux/kernel/git/mkp/scsi.git/commit/?id=f07317a8d57f382ec505597816271dd72ffa20c7
- `patches/0535-scsi-ufs-core-fast-abort-unsupported-query-idns.patch`
  source: https://git.kernel.org/pub/scm/linux/kernel/git/mkp/scsi.git/commit/?id=d1fa5cea5dcd72a9ec21b835c572a28c0c3f4fbf
  upstream: https://git.kernel.org/pub/scm/linux/kernel/git/mkp/scsi.git/commit/?id=d1fa5cea5dcd72a9ec21b835c572a28c0c3f4fbf
- `patches/0536-scsi-ufs-core-dynamically-disable-timestamp-on-unsupported-devices.patch`
  source: https://git.kernel.org/pub/scm/linux/kernel/git/mkp/scsi.git/commit/?id=00eec343c7306be7f1103f5002abd77496937ff9
  upstream: https://git.kernel.org/pub/scm/linux/kernel/git/mkp/scsi.git/commit/?id=00eec343c7306be7f1103f5002abd77496937ff9
- `patches/0512-PCI-qcom-skip-L23-ready-poll-on-SM8550.patch`
  source: armada
  upstream: local
- `patches/0513-PCI-qcom-honour-an-opp-suspend-opp-as-the-non-s2ram-memory-floor.patch`
  source: armada
  upstream: not submitted
  notes: On OPP-scaling platforms the OPP carries the only PCIe memory-path votes, so dropping it to NULL on non-S2RAM suspend leaves the RPMh sleep set with no DDR/LLCC contract and the AOP never resumes. Deliberately does not populate `pcie->icc_mem` on OPP platforms: `qcom_pcie_icc_opp_update()` prefers an `icc_mem` handle over the OPP branch, so providing one silently disables the post-link-training OPP update and pins the OPP at the probe-time maximum.
- `patches/0522-PCI-host-common-let-only-endpoints-veto-d3cold.patch`
  source: armada
  upstream: local
- `patches/0523-regulator-qcom-rpmh-add-suspend-state-support.patch`
  source: https://github.com/thorch-os/thorch/blob/82e7472e6cad5c08a55c3aef92ef5be218621b2c/packages/linux-thorch/patches/0218-regulator-qcom-rpmh-add-suspend-state-support.patch
  upstream: unknown
  notes: Uses the PMIC's own bypass mode (`pmic_bypass_mode`) instead of `PMIC4_BOB_MODE_PASS`, which is wrong on PMIC5.
- `patches/0524-regulator-core-apply-mem-state-for-s2idle.patch`
  source: https://github.com/thorch-os/thorch/blob/82e7472e6cad5c08a55c3aef92ef5be218621b2c/packages/linux-thorch/patches/0219-regulator-core-apply-mem-state-for-s2idle.patch
  upstream: unknown
- `patches/0525-regulator-core-log-missing-suspend-configuration-at-debug.patch`
  source: armada
  upstream: local
- `patches/0526-regulator-apply-state-mem-in-s2idle-only-for-opted-in-drivers.patch`
  source: armada
  upstream: local
- `patches/0514-PCI-qcom-honor-iommu-map-cell-count.patch`
  source: https://lkml.iu.edu/2609.0/16096.html
  upstream: https://lore.kernel.org/r/20260907143349.317495-1-mani@kernel.org
- `patches/0514a-of-property-honor-iommu-cells-in-iommu-map-devlinks.patch`
  source: armada
  upstream: local
- `patches/0001-pcie-update-sm8550-dtsi.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/patches/linux/0001-pcie-update-sm8550-dtsi.patch
  upstream: https://lore.kernel.org/r/20260611-wake-v2-33-2744251b1181@oss.qualcomm.com
- `patches/0520-arm64-dts-qcom-sm8550-add-a-pcie-suspend-opp.patch`
  source: armada
  upstream: not submitted
  notes: Companion DT for 0513. `opp-hz` is synthetic and `opp-level` is omitted so the OPP can never be selected for a trained link by either match in `qcom_pcie_icc_opp_update()`. pcie1 is disabled in the SM8550 board DTS files that use this table and gets no suspend OPP.
- `patches/0521-arm64-dts-qcom-sm8750-add-a-pcie-suspend-opp.patch`
  source: armada
  upstream: local
- `patches/0527-arm64-dts-qcom-sm8650-add-a-pcie-suspend-opp.patch`
  source: armada
  upstream: local
- `patches/0120-20250728_konradybcio_gpu_cc_power_requirements_reality_check.patch`
  source: https://github.com/ROCKNIX/distribution/blob/ef264a238d5e2ba960145e3fda663dc27de49a80/projects/ROCKNIX/devices/SM8550/patches/linux/0120-20250728_konradybcio_gpu_cc_power_requirements_reality_check.patch
  upstream: https://lore.kernel.org/r/20250728-topic-gpucc_power_plumbing-v1-22-09c2480fe3e6@oss.qualcomm.com
- `patches/0121-pmdomain-qcom-rpmhpd-presync-floor-gmu-rails.patch`
  source: https://github.com/ROCKNIX/distribution/blob/ef264a238d5e2ba960145e3fda663dc27de49a80/projects/ROCKNIX/devices/SM8550/patches/linux/0121-pmdomain-qcom-rpmhpd-presync-floor-gmu-rails.patch
  source: https://github.com/ROCKNIX/distribution/blob/ef264a238d5e2ba960145e3fda663dc27de49a80/projects/ROCKNIX/devices/SM8750/patches/linux/0052-pmdomain-qcom-rpmhpd-presync-floor-gmu-rails.patch
  upstream: unknown
  notes: Armada applies the presync_floor and skip_retention_level flags via private descriptors (gfx_gmu for SM8550+SM8750, gmxc_sm8750) so GMU rail pre-sync behavior cannot change on other SoCs; ROCKNIX instead flags the shared gfx/gmxc structs, which would leak onto every SoC sharing them in a combined kernel. SM8250 and SM8650 have no Linux-side voter on these rails (their gpucc/gmu nodes reference only gpucc GDSCs), so the flags would be inert there today; revisit the scoping if the upstream gpucc power plumbing series lands and adds voters on more SoCs.
- `patches/0122-interconnect__qcom__sm8550__Enable_QoS_configuration.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/patches/linux/0122-interconnect__qcom__sm8550__Enable_QoS_configuration.patch
  upstream: unknown
- `patches/0200-ASoC-wcd938x-add-DMIC-DAPM-inputs.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/patches/linux/0200-ASoC-wcd938x-add-DMIC-DAPM-inputs.patch
  upstream: unknown
- `patches/1020-wifi-ath12k-send-the-computed-scan-priority-to-the-f.patch`
  source: https://github.com/ROCKNIX/distribution/blob/10b09e8df73b2b07ff04a243ded7425e41cd67fa/projects/ROCKNIX/devices/SM8550/patches/linux/1020-wifi-ath12k-send-the-computed-scan-priority-to-the-f.patch
  upstream: unknown
- `patches/1021-wifi-ath12k-convert-scan-timeout-to-wiphy-delayed-work.patch`
  source: https://lkml.iu.edu/2609.1/17957.html
  upstream: https://lkml.iu.edu/2609.1/17957.html
- `patches/0500-ROCKNIX-set-boot-fanspeed.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/patches/linux/0500-ROCKNIX-set-boot-fanspeed.patch
  upstream: unknown
- `patches/0501-ROCKNIX-fix-wifi-and-bt-mac.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/patches/linux/0501-ROCKNIX-fix-wifi-and-bt-mac.patch
  upstream: unknown
  notes: Armada refreshed only the ath12k insertion context for Linux 7.1's rate-table macros; MAC derivation behavior is unchanged.
- `patches/0503-ROCKNIX-battery-name.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/patches/linux/0503-ROCKNIX-battery-name.patch
  upstream: unknown
- `patches/0901-power-supply-qcom-battmgr-fix-charge-unit.patch`
  source: https://lkml.iu.edu/2608.3/10893.html
  upstream: https://lkml.iu.edu/2608.3/10893.html
  notes: Carries the upstream SM8350-class fix that initializes the charge unit to mAh so CHARGE_FULL* is available to userspace.
- `patches/0902-power-supply-qcom-battmgr-expose-charge-now.patch`
  source: https://lkml.iu.edu/2608.3/10890.html
  upstream: https://lkml.iu.edu/2608.3/10890.html
  notes: Carries the upstream SM8350/SM8550 mapping of the firmware remaining-charge counter to CHARGE_NOW.
- `patches/0903-power-supply-qcom-battmgr-expose-the-charge-current-limit.patch`
  source: armada
  upstream: local
- `patches/0900-power-supply-qcom-battmgr-log-usb-adapter-type.patch`
  source: armada
  upstream: local
  notes: Armada wrote this diagnostic patch to log the USB type and Qualcomm firmware's adapter type when either value changes.
- `patches/0001-pcie-update-sm8650-dtsi.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8650/patches/linux/0001-pcie-update-sm8650-dtsi.patch
  upstream: https://lore.kernel.org/r/20260611-wake-v2-35-2744251b1181@oss.qualcomm.com
- `patches/0006-add-hw_params-callback-function-to-drm_connector_hdmi_audio_ops.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/patches/linux/0006-add-hw_params-callback-function-to-drm_connector_hdmi_audio_ops.patch
  upstream: https://lore.kernel.org/r/20250925040530.20731-1-liujianfeng1994@gmail.com
- `patches/0007-ASoC-qcom-audioreach-compute-active-channel-maps.patch`
  source: https://lore.kernel.org/r/20260728-topic-sm8650-ayaneo-pocket-s2-wsa2-fix-v3-1-b29f44720178@linaro.org
  upstream: https://lore.kernel.org/r/178550094533.136247.8108653420741029312.b4-ty@b4
- `patches/0008-ASoC-dt-bindings-qcom-sm8250-Add-Ayaneo-Pocket-S2.patch`
  source: https://lore.kernel.org/r/20260728-topic-sm8650-ayaneo-pocket-s2-wsa2-fix-v3-2-b29f44720178@linaro.org
  upstream: https://lore.kernel.org/r/178550094533.136247.8108653420741029312.b4-ty@b4
- `patches/0009-ASoC-qcom-sc8280xp-add-Ayaneo-Pocket-S2-card.patch`
  source: https://lore.kernel.org/r/20260728-topic-sm8650-ayaneo-pocket-s2-wsa2-fix-v3-3-b29f44720178@linaro.org
  upstream: https://lore.kernel.org/r/178550094533.136247.8108653420741029312.b4-ty@b4
  notes: Backported to Armada's Linux 7.2 machine-driver layout; the accepted WSA2 channel map and constraints are unchanged.
- `patches/0010-ASoC-qcom-add-KONKR-Pocket-FIT-WSA2-card.patch`
  source: armada
  upstream: local
  notes: Pocket FIT uses the same WSA2-only speaker wiring as Pocket S2. Armada adds a board-specific compatible backed by the accepted Pocket S2 channel mapping.
- `patches/0011-ASoC-qcom-audioreach-use-wsa-interface-for-sm8650-wsa2.patch`
  source: armada
  upstream: local
  notes: Restores the WSA2-to-WSA interface rewrite that ROCKNIX bundled inside its replaced channel-map workaround; the SM8650 topology declares the speaker backend as WSA2, which the shipped ADSP firmware rejects. Gated to `qcom,sm8650`; remove once a corrected topology stops declaring WSA2.
- `patches/v2_20260420_neil_armstrong_arm64_qcom_sm8650_misc_enhancements.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8650/patches/linux/v2_20260420_neil_armstrong_arm64_qcom_sm8650_misc_enhancements.patch
  upstream: https://lore.kernel.org/r/20260615-topic-sm8650-upstream-cpu-props-v3-0-eeb6e9fa7581@linaro.org
- `patches/0063-gpu-drm-panel-add-pocket-fit-panel.patch`
  source: https://github.com/ROCKNIX/distribution/blob/5cff2f7918ca6bd56d8a884f0837309529a058bc/projects/ROCKNIX/devices/SM8750/patches/linux/0055-gpu-drm-panel-add-pocket-fit-panel.patch
  upstream: unknown
  notes: Uses the four-mode SM8750 variant and Linux 7.2's managed DRM panel allocator.
- `patches/0064-input-touchscreen-add-rocknix-chipone-tddi.patch`
  source: https://github.com/ROCKNIX/chipone_tddi/commit/af27029fa2b27c4a77d16809298ed5d03c9da5a6
  upstream: unknown
  notes: ROCKNIX ships this full driver as an out-of-tree module for the SM8650 KONKR Pocket FIT. Armada converts the pinned fork to an in-tree patch; its C and header files are unchanged, while Kconfig and Kbuild are adapted for the combined kernel. The fork includes ROCKNIX's Linux 7.1 GPIO compatibility update.
- `patches/0014-sm8250-ath11k-fix-wifi-mac.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8250/patches/linux/0014-fix-wifi-and-bt-mac.patch
  upstream: unknown
  notes: Armada carries only the ath11k portion of the shared ROCKNIX patch; the Bluetooth and SoC-serial changes are supplied by `0501-ROCKNIX-fix-wifi-and-bt-mac.patch`.
- `patches/0067-arm64-dts-qcom-pm8150l-add-lcdb-node.patch`
  source: https://github.com/ROCKNIX/distribution/blob/063e530e58d380aa17a7c608f0853898cc3911a5/projects/ROCKNIX/devices/SM8250/patches/linux/0067-arm64-dts-qcom-pm8150l-add-lcdb-node.patch
  upstream: unknown
- `patches/0004-pm8150b.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8250/patches/linux/0004-pm8150b.patch
  upstream: unknown
- `patches/0009-qcom-spmi-haptics.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8250/patches/linux/0009-qcom-spmi-haptics.patch
  upstream: unknown
  notes: Armada refreshed only the Kconfig insertion context so the SM8250 haptics driver follows Linux 7.1's high-voltage haptics entry; driver behavior is unchanged.
- `patches/0008-retroid-gamepad.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8250/patches/linux/0008-retroid-gamepad.patch
  upstream: unknown
- `patches/0013-add-force-feedback.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8250/patches/linux/0013-add-force-feedback.patch
  upstream: unknown
- `patches/0008a-input-retroid-keep-trigger-conversion-independent-of-calibration.patch`
  source: armada
  upstream: local
- `patches/0063_Mangmi-Pocket-Max-HL7139-charge-pump.patch`
  source: https://github.com/ROCKNIX/distribution/blob/807c74fc46c124891d8aa9b57a68533d38e9a6b2/projects/ROCKNIX/devices/SM8250/patches/linux/0063_Mangmi-Pocket-Max-HL7139-charge-pump.patch
  upstream: unknown
- `patches/0011-qcom-pm8150b-charger.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8250/patches/linux/0011-qcom-pm8150b-charger.patch
  upstream: unknown
  notes: Armada refreshed only the Makefile context for Linux 7.2's expanded Surface power-supply entries; charger and fuel-gauge behavior is unchanged.
- `patches/0005-sm8250-uart.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8250/patches/linux/0005-sm8250-uart.patch
  upstream: unknown
- `patches/9998-sm8250-gpu-tuning.patch`
  source: https://github.com/ROCKNIX/distribution/blob/f3e2b6187cd2ae2dc5122edfebe84228a314f2c5/projects/ROCKNIX/devices/SM8250/patches/linux/9998-gpu-tuning.patch
  upstream: unknown
  notes: Contents are verbatim; Armada renamed the file from the ROCKNIX `9998-gpu-tuning.patch`.
- `patches/0001-msm-dsi-restore-wide_bus-bpp-calculation.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8250/patches/linux/0001-msm-dsi-restore-wide_bus-bpp-calculation.patch
  upstream: unknown
  notes: Armada limited the ROCKNIX change to SM8250 so it does not alter the shared DSI path on other supported SoCs.
- `patches/0012-ASoC-qcom-q6asm-dai-Change-some-default-periods.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8250/patches/linux/0012-ASoC-qcom-q6asm-dai-Change-some-default-periods.patch
  upstream: unknown
- `patches/0062_wsa881x-shared-powerdown-gpio.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8250/patches/linux/0062_wsa881x-shared-powerdown-gpio.patch
  upstream: unknown
  notes: Ported to Linux 7.2's `GPIOD_OUT_LOW` initialization while retaining the non-exclusive request needed by boards whose two amplifiers share one powerdown GPIO.
- `patches/0100-revert-force-16bit-audio.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8250/patches/linux/0100-revert-force-16bit-audio.patch
  upstream: unknown
- `patches/0300-batocera-fix-headphone-jack-detection.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8250/patches/linux/0300-batocera-fix-headphone-jack-detection.patch
  upstream: unknown
  notes: Armada limited the ROCKNIX headphone-jack workaround to SM8250 so it does not alter shared codec behavior on other supported SoCs.
- `patches/0102-arm64-dts-qcom-pm8150-Add-nvmem-support-for-PM8150-R.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8250/patches/linux/0102-arm64-dts-qcom-pm8150-Add-nvmem-support-for-PM8150-R.patch
  upstream: unknown
- `patches/9999-sm8250-disable-coresight-stm.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8250/patches/linux/9999-remove-log-spam.patch
  upstream: unknown
  notes: Armada carries only the ROCKNIX device-tree change that disables CoreSight STM and omits the broader log-suppression changes from the source patch.
- `patches/0301-drm-panel-add-ayaneo-ar18-panel.patch`
  source: https://github.com/ROCKNIX/distribution/blob/aa418ad72e3e661b3834f46168bdf183276df2ac/projects/ROCKNIX/devices/SM8550/patches/linux/0064-gpu-drm-panel-add-ar16-4inch-panel.patch
  upstream: not submitted
  notes: Rewritten for the AYANEO Pocket MICRO 2 ST7123 panel on the mipi_dsi_multi_context API; timings and init sequence from the vendor device tree.
- `patches/0302-input-touchscreen-add-sitronix-st7123.patch`
  source: https://github.com/raspberrypi/linux/commit/6d4adb6bfea64581dff1c670a17333b63b8ba05d
  upstream: not submitted
  notes: Waveshare's Sitronix ST7123 driver from the Raspberry Pi kernel (rpi-6.18.y), with chip id 0x87 accepted for the AYANEO Pocket MICRO 2.
- `patches/0303-hid-add-ayaneo-pocket-micro2-force-feedback.patch`
  source: armada
  upstream: local
  notes: Rumble report format from the stock Android kernel's hid-ayaneo. Binds on every AYANEO 4001:0428 pad, rumble only on the Pocket MICRO 2.
- `patches/0304-ASoC-wcd938x-treat-failed-zdet-ramp-as-floating.patch`
  source: armada
  upstream: local
- `patches/0305-backlight-qcom-wled-always-disable-ovp-irq-at-probe.patch`
  source: armada
  upstream: local
- `patches/0026-dt-bindings-arm-qcom-ids-Add-SoC-ID-for-CQ8725S.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8750/patches/linux/0026-dt-bindings-arm-qcom-ids-Add-SoC-ID-for-CQ8725S.patch
  upstream: https://lore.kernel.org/r/20260605-cq8725s-soc-id-v1-1-bb1ef93de649@gmail.com
- `patches/0027-soc-qcom-socinfo-Add-CQ8725S-SoC-ID.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8750/patches/linux/0027-soc-qcom-socinfo-Add-CQ8725S-SoC-ID.patch
  upstream: https://lore.kernel.org/r/20260605-cq8725s-soc-id-v1-2-bb1ef93de649@gmail.com
- `patches/0034-arm64-dts-qcom-sm8750-Add-UART15.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8750/patches/linux/0034-arm64-dts-qcom-sm8750-Add-UART15.patch
  upstream: https://lore.kernel.org/r/20260605-sm8750-uart15-v1-1-93e660722e61@gmail.com
- `patches/0033-arm64-dts-qcom-sm8750-gpu-clock-controllers.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8750/patches/linux/0033-arm64-dts-qcom-sm8750-gpu-clock-controllers.patch
  upstream: https://lore.kernel.org/r/20260714-gpucc_dt_v6-v6-1-16bf5289572d@oss.qualcomm.com
- `patches/0038-arm64-dts-qcom-sm8750-add-GPU-nodes.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8750/patches/linux/0038-arm64-dts-qcom-sm8750-add-GPU-nodes.patch
  upstream: unknown
- `patches/0038a-arm64-dts-qcom-sm8750-map-A8xx-RSCC-registers.patch`
  source: armada
  upstream: local
  notes: Armada expands the SM8750 GPU register mapping to `0x6c000` so Linux 7.1's A8xx recovery path can access RSCC at GPU offset `0x50000` without faulting.
- `patches/0035-arm64-dts-qcom-sm8750-set-CPU-capacity-dmips-mhz.patch`
  source: https://github.com/ROCKNIX/distribution/blob/f3e2b6187cd2ae2dc5122edfebe84228a314f2c5/projects/ROCKNIX/devices/SM8750/patches/linux/0076-arm64-dts-qcom-sm8750-set-CPU-capacity-dmips-mhz.patch
  upstream: unknown
- `patches/0037-arm64-dts-qcom-sm8750-add-CPU-thermal-cooling.patch`
  source: https://github.com/ROCKNIX/distribution/blob/b1e9d07b251c5a013d303a518106c547db001b74/projects/ROCKNIX/devices/SM8750/patches/linux/0075-arm64-dts-qcom-sm8750-add-CPU-thermal-cooling.patch
  upstream: unknown
  notes: Linux 7.2 supplies the CPU cooling-cell properties; Armada retains the missing passive trips and cooling maps.
- `patches/0049-drm-msm-a8xx-add-adreno-830-catalog.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8750/patches/linux/0049-drm-msm-a8xx-add-adreno-830-catalog.patch
  upstream: unknown
- `patches/0039-wifi-ath12k-add-initial-hardware-definition-for-WCN7.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8750/patches/linux/0039-wifi-ath12k-add-initial-hardware-definition-for-WCN7.patch
  upstream: unknown
  notes: Armada refreshed only the hardware-parameter insertion context for Linux 7.2's expanded IPQ5332 entry; WCN7860 behavior is unchanged.
- `patches/0040-wifi-ath12k-send-QDSS-config-when-CNSS_QDSS_CFG_MISS.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8750/patches/linux/0040-wifi-ath12k-send-QDSS-config-when-CNSS_QDSS_CFG_MISS.patch
  upstream: unknown
- `patches/0041-wifi-ath12k-disable-CNSS_QDSS_CFG_MISS_V01-for-the-W.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8750/patches/linux/0041-wifi-ath12k-disable-CNSS_QDSS_CFG_MISS_V01-for-the-W.patch
  upstream: unknown
- `patches/0042-PCI-pwrctrl-pwrseq-add-support-for-WCN7860.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8750/patches/linux/0042-PCI-pwrctrl-pwrseq-add-support-for-WCN7860.patch
  upstream: unknown
- `patches/0043-power-sequencing-qcom-wcn-add-support-for-WCN7860.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8750/patches/linux/0043-power-sequencing-qcom-wcn-add-support-for-WCN7860.patch
  upstream: unknown
- `patches/0044-clk-qcom-gcc-sm8750-Do-not-turn-off-PCIe-GDSCs-durin.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8750/patches/linux/0044-clk-qcom-gcc-sm8750-Do-not-turn-off-PCIe-GDSCs-durin.patch
  upstream: https://lore.kernel.org/r/20260102-pci_gdsc_fix-v1-3-b17ed3d175bc@oss.qualcomm.com
- `patches/0045-Bluetooth-qca-add-WCN7860-support.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8750/patches/linux/0045-Bluetooth-qca-add-WCN7860-support.patch
  upstream: unknown
  notes: Armada rebased ROCKNIX's WCN7860 additions onto Linux 7.1 after upstream restructured the QCA Bluetooth switch cases and renamed the power-off helper.
- `patches/0066-scsi-ufs-ufs-qcom-add-sm8750-compatible.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8750/patches/linux/0066-scsi-ufs-ufs-qcom-add-sm8750-compatible.patch
  upstream: unknown
- `patches/0509-soc-qcom-pmic_glink_altmode-defer-until-mux-switch-ready.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8750/patches/linux/0509-soc-qcom-pmic_glink_altmode-defer-until-mux-switch-ready.patch
  upstream: unknown
  notes: Armada gates both deferrals on `qcom,sm8750`. ROCKNIX runs this only in its SM8750 kernel, where the mode-switch supplier registers late. Boards that declare no mode-switch at all (the AYANEO SM8550 family) would defer forever, which blocks the DP aux bridges, keeps the msm DRM aggregate from binding, and leaves the internal panel dark. Hardware-confirmed on the Pocket EVO. In Linux 7.2.6, a declared-but-late provider already returns `-EPROBE_DEFER` through the `IS_ERR()` path ahead of these NULL checks, so the gated checks are ROCKNIX's SM8750 fallback on top of that distinction rather than the primary defer mechanism.
- `patches/0519-usb-typec-ucsi-clear-USB-role.patch`
  source: https://github.com/ROCKNIX/distribution/commit/c9629c95a5655dcfb68e8f3aaf75f9967a9e9656
  upstream: unknown
- `patches/0517-usb-typec-mux-dont-swallow-EPROBE_DEFER.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8750/patches/linux/0517-usb-typec-mux-dont-swallow-EPROBE_DEFER.patch
  upstream: unknown
  notes: Retains only array initialization; Linux 7.2.6 includes the probe-deferral fixes.
- `patches/0603-ASoC-codecs-wcd939x-keep-the-codec-resumed-while-a-ja.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8750/patches/linux/0603-ASoC-codecs-wcd939x-keep-the-codec-resumed-while-a-ja.patch
  upstream: unknown
- `patches/0060-ASoC-qcom-sc8280xp-enable-MI2S-bit-clock-on-BE-startup.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8750/patches/linux/0060-ASoC-qcom-sc8280xp-enable-MI2S-bit-clock-on-BE-startup.patch
  upstream: unknown
  notes: Armada combined ROCKNIX's SM8750 clock-reference fix with Armada's Primary-MI2S clock handling while rebasing the shared driver onto Linux 7.1.
- `patches/0072-ASoC-lpass-rx-macro-add-HPH-PCM-mode.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8750/patches/linux/0072-ASoC-lpass-rx-macro-add-HPH-PCM-mode.patch
  upstream: unknown
- `patches/0073-ASoC-wcd939x-add-HPH-PCM-HiFi-mode.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8750/patches/linux/0073-ASoC-wcd939x-add-HPH-PCM-HiFi-mode.patch
  upstream: unknown
- `patches/0074-soundwire-qcom-PCM-data-port-format-enable.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8750/patches/linux/0074-soundwire-qcom-PCM-data-port-format-enable.patch
  upstream: unknown
- `patches/0075-ASoC-wsa884x-preserve-Pocket-S2-compander-gain.patch`
  source: armada
  upstream: local
  notes: AYANEO Pocket S2 and Pocket FIT fix. The WSA884x amplifier's POST_PMU sequence selects the SoundWire compander, but the unmute path immediately overrode it with the fixed CSR gain, discarding the boards' matched compander plus digital-volume configuration. The driver now keeps the compander selection when its port is enabled, gated on the `ayaneo,pocket-s2` and `konkr,pocket-fit` machine compatibles, so every other WSA884x machine keeps the original fixed-gain behavior. This requires a matching UCM that enables both macro companders, initializes both PA controls to 17 (hardware gain index 14, unity), and uses digital volume. Scoped to the Armada distribution; not a Linux upstream submission.
- `patches/0612-ROCKNIX-odin3-q6apm-start-mi2s-port-at-prepare.patch`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/patches/linux/0612-ROCKNIX-odin3-q6apm-start-mi2s-port-at-prepare.patch
  upstream: unknown
- `patches/0613-ASoC-q6apm-scope-prepare-start-to-MI2S.patch`
  source: armada
  upstream: local
  notes: Armada added this follow-up to limit `0612` to playback MI2S DAIs; without it, the ROCKNIX change would affect every playback DAI using the shared q6apm driver.
- `patches/0528-ASoC-qcom-sc8280xp-suspend-the-card-and-its-streams.patch`
  source: armada
  upstream: local
- `patches/0529-ASoC-qdsp6-q6apm-fail-the-graph-open-when-the-DSP-rejects-it.patch`
  source: armada
  upstream: local
- `patches/0530-ASoC-qdsp6-release-AudioReach-graphs-across-system-suspend.patch`
  source: armada
  upstream: local
- `patches/0532-hwmon-pwm-fan-optionally-run-the-fan-while-charging-in-s2idle.patch`
  source: armada
  upstream: local
- `patches/0540-arm64-signal-reapply-the-ssbs-policy-on-sigreturn.patch`
  source: armada
  upstream: local
- `patches/0618-drm-msm-dp-dont-fail-audio-prepare-when-display-off.patch`
  source: https://github.com/ROCKNIX/distribution/pull/3187
  upstream: unknown
  notes: Imported unchanged from the linked ROCKNIX pull request.
- `patches/0619-drm-msm-map-submitqueue-priority-onto-high-low.patch`
  source: armada
  upstream: local
- `patches/0620-drm-msm-a6xx-hfi-irq-clears-only-the-cm3-fault.patch`
  source: https://github.com/kettlelinux/kettlelinux/blob/d379ae1d0efeb8dd282647a1598efd719c6ecd4d/kernel/patches/40-kettle/1350-drm-msm-a6xx-hfi-irq-clears-only-the-cm3-fault.patch
  upstream: unknown
- `patches/0621-drm-msm-a6xx-bound-the-gmu-wait-for-a-fault-devcoredump.patch`
  source: https://github.com/kettlelinux/kettlelinux/blob/d379ae1d0efeb8dd282647a1598efd719c6ecd4d/kernel/patches/40-kettle/1351-drm-msm-a6xx-bound-the-gmu-wait-for-a-fault-devcoredump.patch
  upstream: unknown
- `patches/0622-drm-msm-a6xx-mask-the-hfi-interrupt-before-booting-the-gmu.patch`
  source: armada
  upstream: local
- `dts/qcs8550-ayaneo-pocketace.dts`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/linux/dts/qcom/qcs8550-ayaneo-pocketace.dts
- `dts/qcs8550-ayaneo-pocket-common.dtsi`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/linux/dts/qcom/qcs8550-ayaneo-pocket-common.dtsi
- `dts/qcs8550-ayaneo-pocketdmg.dts`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/linux/dts/qcom/qcs8550-ayaneo-pocketdmg.dts
- `dts/qcs8550-ayaneo-pocketds.dts`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/linux/dts/qcom/qcs8550-ayaneo-pocketds.dts
- `dts/qcs8550-ayaneo-pocketevo.dts`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/linux/dts/qcom/qcs8550-ayaneo-pocketevo.dts
- `dts/qcs8550-ayaneo-pockets1k.dts`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/linux/dts/qcom/qcs8550-ayaneo-pockets1k.dts
- `dts/qcs8550-ayaneo-pockets2k.dts`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/linux/dts/qcom/qcs8550-ayaneo-pockets2k.dts
- `dts/qcs8550-ayn-common.dtsi`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/linux/dts/qcom/qcs8550-ayn-common.dtsi
- `dts/qcs8550-ayn-odin2.dts`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/linux/dts/qcom/qcs8550-ayn-odin2.dts
- `dts/qcs8550-ayn-odin2mini.dts`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/linux/dts/qcom/qcs8550-ayn-odin2mini.dts
- `dts/qcs8550-ayn-odin2portal.dts`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/linux/dts/qcom/qcs8550-ayn-odin2portal.dts
- `dts/qcs8550-ayn-thor.dts`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/linux/dts/qcom/qcs8550-ayn-thor.dts
- `dts/qcs8550-retroidpocket-rp6.dts`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/linux/dts/qcom/qcs8550-retroidpocket-rp6.dts
- `dts/qcs8550-retroidpocket-rp6-top-dpad.dts`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/linux/dts/qcom/qcs8550-retroidpocket-rp6-top-dpad.dts
- `dts/qcs8550-retroidpocket-rpnova.dts`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8550/linux/dts/qcom/qcs8550-retroidpocket-rpnova.dts
- `dts/sm8650-ayaneo-common.dtsi`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8650/linux/dts/qcom/sm8650-ayaneo-common.dtsi
- `dts/sm8650-ayaneo-ps2.dts`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8650/linux/dts/qcom/sm8650-ayaneo-ps2.dts
- `dts/sm8650-konkr-pf.dts`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8650/linux/dts/qcom/sm8650-konkr-pf.dts
- `dts/sm8250-retroidpocket-common.dtsi`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8250/linux/dts/qcom/sm8250-retroidpocket-common.dtsi
- `dts/sm8250-ayaneo-pocket-micro2.dts`
  source: armada
  notes: Board file for the AYANEO Pocket MICRO 2; fan is left to armada-powerd (no thermal-zone include).
- `dts/sm8250-ayn-thorlite.dts`
  source: https://github.com/ROCKNIX/distribution/blob/dbea089dd83e156babbbcabc677117cef08f1148/projects/ROCKNIX/devices/SM8250/linux/dts/qcom/sm8250-ayn-thorlite.dts
  notes: Imported verbatim from ROCKNIX; SHA-256 `62a06545c46fe052c69699c20c8c6b330c4b64a3ecdb6b5ba99420868a78597d`. Authored by Philippe Simons; retains the original BSD-3-Clause SPDX identifier and Retroid Pocket copyright notice.
- `dts/sm8250-ayn-thorlite.dts.patch`
  source: armada
  notes: Armada removes the kernel touchscreen coordinate transforms after copying `dts/sm8250-ayn-thorlite.dts`; Gamescope/libinput maps the native digitizer coordinates to the rotated outputs, and applying both transforms makes the right edge behave as the top edge. This matches Armada's full Thor handling. Also enables the AYN button.
- `dts/sm8250-mangmi-air-y-pro.dts`
  source: https://github.com/ROCKNIX/distribution/blob/807c74fc46c124891d8aa9b57a68533d38e9a6b2/projects/ROCKNIX/devices/SM8250/linux/dts/qcom/sm8250-mangmi-air-y-pro.dts
  notes: Imported verbatim from ROCKNIX; SHA-256 `d98f1104f4fe29f9bf8c94692beb491e840332a851f1f6994f015973ba24c018`.
- `dts/sm8250-mangmi-pocket-max.dts`
  source: https://github.com/ROCKNIX/distribution/blob/807c74fc46c124891d8aa9b57a68533d38e9a6b2/projects/ROCKNIX/devices/SM8250/linux/dts/qcom/sm8250-mangmi-pocket-max.dts
  notes: Imported verbatim from ROCKNIX; SHA-256 `f6128a97cf510fcd9ecf568a2e512c3f6f4210ad43e4fb1f542740f4b140dfd2`.
- `dts/sm8250-mangmi-pocket-max.dts.patch`
  source: armada
  notes: Drops ROCKNIX's kernel thermal-zone fan include after copying the DTS; Armada powerd owns the PWM fan curve and must not race a second controller.
- `dts/sm8250-retroidpocket-flip2.dts`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8250/linux/dts/qcom/sm8250-retroidpocket-flip2.dts
- `dts/sm8250-retroidpocket-flip2-visionox.dts`
  source: armada
  notes: Support for Visionox display panel models
- `dts/sm8250-retroidpocket-rp5.dts`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8250/linux/dts/qcom/sm8250-retroidpocket-rp5.dts
- `dts/sm8250-retroidpocket-rp5-visionox.dts`
  source: armada
  notes: Support for Visionox display panel models
- `dts/sm8250-retroidpocket-rpmini.dts`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8250/linux/dts/qcom/sm8250-retroidpocket-rpmini.dts
- `dts/sm8250-retroidpocket-rpminiv2.dts`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8250/linux/dts/qcom/sm8250-retroidpocket-rpminiv2.dts
- `dts/sm8750-konkr-pf-elite.dts`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8750/linux/dts/qcom/sm8750-konkr-pf-elite.dts
- `dts/sm8750-konkr-pf-elite.dts.patch`
  source: armada
  notes: Armada adapts the Elite touchscreen node to the full ROCKNIX Chipone fork shared with the SM8650 Pocket FIT, keeps volume-up from waking the system, and adds the DisplayPort playback link required for USB-C DP Alt Mode audio; the touchscreen driver is selected only by those two device-tree nodes. Armada also marks PCIe WAKE# active-low and idles the codec rails in LPM during s2idle.
- `dts/cq8725s-ayn-common.dtsi`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8750/patches/linux/0046-arm64-dts-qcom-Add-AYN-CQ8725S-Common.patch
  notes: Armada extracted this DTS from the cited ROCKNIX patch and then applied later ROCKNIX DTS updates, including the Odin 3 haptics nodes from ROCKNIX commit `81a31e3d0f`.
- `dts/cq8725s-ayn-odin3.dts`
  source: https://github.com/ROCKNIX/distribution/blob/bcf3b5bc574990b96543484575b06f912153a715/projects/ROCKNIX/devices/SM8750/patches/linux/0047-arm64-dts-qcom-Add-AYN-Odin3.patch
  notes: Armada extracted this DTS from the cited ROCKNIX patch and then applied later ROCKNIX DTS updates.
- `dts/cq8725s-ayn-odin3.dts.patch`
  source: armada
  notes: Armada enables DPU dithering and codec-rail LPM sleep states after copying `dts/cq8725s-ayn-odin3.dts`.
- `dts/cq8725s-ayn-common.dtsi.patch`
  source: armada
  notes: Armada keeps volume-up from waking the system, marks Odin 3's RSInput node as connected to the Qualcomm haptics device, supplies the device's 1024 range, marks PCIe WAKE# active-low, and keeps the four switched WCN7860 PMU supply rails always-on because re-enabling them at resume can hang the SoC. The 70-count `axis-deadzone` (an unmeasured ROCKNIX bring-up value) was dropped; like the RP6 and every other RSInput device the Odin 3 now passes its stick value through, leaving deadzone policy to Steam Input and the game.
- `dts/qcs8550-ayaneo-pocket-common.dtsi.patch`
  source: armada
  notes: Armada keeps volume-up from waking the system, removes the SDHCI capability mask, marks PCIe WAKE# active-low, and idles the codec rail in LPM during s2idle after copying `dts/qcs8550-ayaneo-pocket-common.dtsi`.
- `dts/qcs8550-ayaneo-pocketace.dts.patch`
  source: armada
  notes: Armada applies this local patch after copying `dts/qcs8550-ayaneo-pocketace.dts`.
- `dts/qcs8550-ayaneo-pocketdmg.dts.patch`
  source: armada
  notes: Armada applies this local patch after copying `dts/qcs8550-ayaneo-pocketdmg.dts`.
- `dts/qcs8550-ayaneo-pockets1k.dts.patch`
  source: armada
  notes: Armada applies this local patch after copying `dts/qcs8550-ayaneo-pockets1k.dts`.
- `dts/qcs8550-ayaneo-pockets2k.dts.patch`
  source: armada
  notes: Armada applies this local patch after copying `dts/qcs8550-ayaneo-pockets2k.dts`.
- `dts/qcs8550-ayaneo-pocketds.dts.patch`
  source: armada
  notes: Armada applies this local patch after copying `dts/qcs8550-ayaneo-pocketds.dts`. It labels the lower GT911 input device as `bottom_touchscreen` and moves the lower panel onto the ST7703 driver contract: `vcc-supply` for the SGM3804 charge pump, and the former `enable-gpio` on TCA6408 GPIO0 re-modeled as a fixed regulator consumed as `iovcc-supply`. It also declares `vin-supply = <&tca6424_vcc>` on both expander-switched fixed regulators (`vdd_ts`, `panel1-iovcc`): the pca953x suspend callback disables its own VCC, and without the vin link the regulator core would cut the expander while the GT911 still holds VDDIO enabled, aborting async deep suspend mid Goodix screen-off write. It also adds a `gpio-keys-lid` node for the hinge hall sensor: stock Android reads it as a ROHM BU52053NVx (`qti.sensor.hall_effect`) through the SLPI sensor HAL rather than any Linux input driver, with no devicetree node of its own — its GPIO (TLMM 166, active-low, no internal pull; confirmed live via `/sys/kernel/debug/gpio` while toggling the lid) came from `dri_irq_num` in the vendor's `/vendor/etc/sensors/config/kailua_bu52053nvx_0.json` on-device.
- `dts/qcs8550-ayaneo-pocketevo.dts.patch`
  source: armada
  notes: Armada keeps the AYA Space, Menu, LC, and RC auxiliary keys from waking the system.
- `dts/qcs8550-ayn-common.dtsi.patch`
  source: armada
  notes: Armada keeps volume-up from waking the system, removes the SDHCI capability mask, and marks the shared RSInput node as connected to the PM8550B haptics device declared in the same common tree; this covers the AYN and Retroid products that inherit both nodes, including Pocket 6 and Nova. Armada also marks PCIe WAKE# active-low, idles the codec rails in LPM during s2idle, and powers RSInput from `vdd_mcu_3v3` instead of the shared `vreg_bob2`, without always-on, so the driver can cut the MCU in suspend.
- `dts/qcs8550-ayn-odin2portal.dts.patch`
  source: armada
  notes: Adds the back buttons from the Odin 2 DTS into the Odin 2 Portal DTS
- `dts/qcs8550-retroidpocket-rp6.dts.patch`
  source: armada
  notes: Armada switches Pocket 6 from ROCKNIX's Odin 2 fallback to audio firmware extracted from a Pocket 6 vendor image.
- `dts/qcs8550-retroidpocket-rpnova.dts.patch`
  source: armada
  notes: Armada disables the inherited Pocket 6 PWM backlight and removes its panel reference so Nova uses its panel driver’s DSI backlight.
- `dts/qcs8550-ayn-thor.dts.patch`
  source: armada
  notes: Armada fixes the hall-sensor pinctrl, makes only the lid-open edge wake, corrects touch orientation, and enables DPU dithering on the top panel after copying `dts/qcs8550-ayn-thor.dts`.
- `dts/sm8650-ayaneo-common.dtsi.patch`
  source: armada
  notes: Armada keeps volume-up from waking the system, wires the upstream SY7758 driver, marks PCIe WAKE# active-low, and idles the codec rails in LPM during s2idle after copying `dts/sm8650-ayaneo-common.dtsi`.
- `dts/sm8650-ayaneo-ps2.dts.patch`
  source: armada
  notes: Armada selects the accepted Pocket S2 WSA2 sound-card mapping after copying the ROCKNIX DTS.
- `dts/sm8650-konkr-pf.dts.patch`
  source: armada
  notes: Armada selects the shared WSA2 channel mapping through a Pocket FIT-specific sound-card compatible.

- `patches/0505-arm64-emulate-unaligned-atomics.patch`
  source: https://github.com/bylaws/linux/commit/7ae989a43ae7e3cb8007ac21c28dacc24c9d8320
  upstream: unknown
  notes: Rebased patch context for Linux 7.2.3 and Armada's compat-input patch; the unaligned-atomic handler is unchanged.
- `patches/0505a-arm64-unaligned-atomics-cover-the-load128-store-exclusive.patch`
  source: armada
  upstream: local
- `patches/0141-drm-panel-add-nt36536-and-dsc-packet-support.patch`
  source: https://github.com/h0cheung/tb322fc-linux-kernel/commit/6f97250ead155fc3b980856323e99a4623dae73d
  upstream: unknown
  notes: Literal NT36536 panel plus an opt-in DSC packet flag and its host handling.
- `patches/0142-drm-msm-elden-dsc12-10bpc.patch`
  source: https://github.com/h0cheung/tb322fc-linux-kernel/commit/6f97250ead155fc3b980856323e99a4623dae73d
  upstream: unknown
  notes: Elden-only DSC 1.2 and 10bpc configuration with the DPU calculations, retaining the other-board paths.
- `patches/0143-drm-msm-elden-cphy-link-clocks.patch`
  source: https://github.com/h0cheung/tb322fc-linux-kernel/commit/6f97250ead155fc3b980856323e99a4623dae73d
  upstream: unknown
  notes: Elden-only CPHY timing and clock setup, semantically layered on the existing `0048`, `0048a`, and `0048b`.
- `patches/0144-drm-msm-elden-bonded-command-dma.patch`
  source: https://github.com/h0cheung/tb322fc-linux-kernel/commit/6f97250ead155fc3b980856323e99a4623dae73d
  upstream: unknown
  notes: Elden bonded DSI command and DMA synchronization and error handling, retaining the existing non-Elden path.
- `patches/0145-dt-bindings-display-elden-nt36536-cphy.patch`
  source: https://github.com/h0cheung/tb322fc-linux-kernel/commit/6f97250ead155fc3b980856323e99a4623dae73d
  upstream: unknown
  notes: Panel binding plus the 14-byte CPHY timing property.
- `patches/0146-iommu-arm-smmu-elden-boot-handoff.patch`
  source: armada
  upstream: local
  notes: Runtime SMMU handoff for the preserved Elden display. Imports the live firmware SMR/S2CR and context-bank state instead of resetting the SMMU, maps the runtime `/chosen` simple-framebuffer into the display VM before the SID switch, and reserves the same rounded range from the DMA allocator. Scoped to `lenovo,elden` with the non-Adreno SM8750 application SMMU carrying `qcom,preserve-boot-mappings`. Derived from GEEKiDoS <geek_ds@foxmail.com> commit `dc7d14dc64c8d4f86220b30cd126b17d805c8474` and the `6f97250ead155fc3b980856323e99a4623dae73d` tb322fc tree.
- `dts/sm8750-lenovo-elden.dts`
  source: https://github.com/h0cheung/tb322fc-linux-kernel/blob/6f97250ead155fc3b980856323e99a4623dae73d/arch/arm64/boot/dts/qcom/sm8750-lenovo-elden.dts
  upstream: unknown
- `dts/sm8750-lenovo-elden-camera.dtsi`
  source: https://github.com/h0cheung/tb322fc-linux-kernel/blob/6f97250ead155fc3b980856323e99a4623dae73d/arch/arm64/boot/dts/qcom/sm8750.dtsi
  upstream: unknown
  notes: Extracts only the `cci0`, `cci1`, and `camss` nodes under `&soc` with one indentation normalization; it closes the labels the board DTS references and does not imply camera support.
- `dts/sm8750-lenovo-elden.dts.patch`
  source: armada
  notes: Runtime RAM and simplefb template; disables the unverified fixed reservations and enables the runtime SMMU preserved-mapping handoff from `0146`.
- `patches/0147-power-sequencing-qcom-wcn-add-wcn7861-pmu.patch`
  source: https://github.com/h0cheung/tb322fc-linux-kernel/blob/6f97250ead155fc3b980856323e99a4623dae73d/drivers/power/sequencing/pwrseq-qcom-wcn.c
  upstream: unknown
  notes: Local adaptation of the tb322fc WCN7861 PMU provider. Adds a `qcom,wcn7861-pmu` platform-data entry and a 7861-only AOSS/PDC initialisation that runs before any supply or enable vote; votes seven external supplies by voltage range and load so the variable RPMh rails are not left at their floor. The existing WCN7860 path is untouched. Bounded scope: the reference's Bluetooth reset timing and its OL-CPR / QMI dynamic-map and mutex code are omitted. The msm PCIe endpoint waits on ldo9 until the 7861 PMU binds.
- `patches/0153-drm-msm-elden-video-teardown.patch`
  source: https://github.com/h0cheung/tb322fc-linux-kernel/commit/6f97250ead155fc3b980856323e99a4623dae73d
  upstream: unknown
  notes: Elden-only video teardown ordering, narrowed to `lenovo,elden`: the master issues the pipeline cleanup, whose CTL flush video mode only consumes at a frame boundary, before stopping the timing engine; the slave skips the shared teardown, and its enable half marks the slave enabled without restarting the timing engine. Every other board keeps the existing disable ordering, cleanup placement and kickoff path.
- `patches/0154-clk-qcom-elden-reset-live-display.patch`
  source: https://github.com/retro98boy/armada/commit/c3e466807d545991d94c0c4548e58489d656b709
  upstream: unknown
  notes: SuperKali's dispcc "DisableDisplayHW" reset from the Armbian-fed armada tree, narrowed to `lenovo,elden`: at probe, assert the MDSS core, core INT2 and RSCC block resets, hold for 50 ms, then release them in reverse order before `clk_*_pll_configure()`. Only `linux/delay.h` and `linux/of.h` are added; no other probe behaviour changes.
- `patches/0155-drm-msm-elden-pll-digital-power.patch`
  source: https://github.com/retro98boy/armada/blob/c3e466807d545991d94c0c4548e58489d656b709/packages/kernel/patches/1403-drm-msm-dsi-phy-power-up-the-digital-top-before-star.patch
  upstream: unknown
  notes: Baryshkov's "power up the digital top before starting the 7nm PLL", narrowed to `lenovo,elden`: `PLL_SHUTDOWNB` handling is unchanged for every machine, and the additional `DIGTOP_PWRDN_B` set in `dsi_pll_enable_pll_bias()` and clear in `dsi_pll_disable_pll_bias()` runs only on that board. Refcounts, spinlocks and delays are preserved.
- `patches/0160-arm64-dts-qcom-kaanapali-add-the-GPU-SMMU-node.patch`
  source: https://github.com/joonhoekim/tb323fu-linux/commit/11a25bf89b65fff48112a91a0b25bc247e7736d8
  upstream: unknown
  notes: Imported unchanged from the pinned tb323fu-linux tree, with author headers and SPDX terms preserved. Part of the original Qualcomm Kaanapali GPU devicetree series queued upstream for v7.4; devicetree enablement only, not a new GPU driver.
- `patches/0161-arm64-dts-qcom-kaanapali-Add-QFPROM-node.patch`
  source: https://github.com/joonhoekim/tb323fu-linux/commit/11a25bf89b65fff48112a91a0b25bc247e7736d8
  upstream: unknown
  notes: Imported unchanged from the same pinned tree and series as `0160`, with author headers and SPDX terms preserved.
- `patches/0162-arm64-dts-qcom-Add-GPU-support-for-Kaanapali.patch`
  source: https://github.com/joonhoekim/tb323fu-linux/commit/11a25bf89b65fff48112a91a0b25bc247e7736d8
  upstream: unknown
  notes: Imported unchanged from the same pinned tree and series as `0160`, with author headers and SPDX terms preserved.
- `patches/0163-arm64-dts-qcom-kaanapali-Add-GPU-cooling.patch`
  source: https://github.com/joonhoekim/tb323fu-linux/commit/11a25bf89b65fff48112a91a0b25bc247e7736d8
  upstream: unknown
  notes: Imported unchanged from the same pinned tree and series as `0160`, with author headers and SPDX terms preserved.
- `patches/0164-Input-touchscreen-import-NT36536-host-download-SPI-d.patch`
  source: https://github.com/joonhoekim/tb323fu-linux/commit/11a25bf89b65fff48112a91a0b25bc247e7736d8
  upstream: unknown
  notes: Gen5 (Lenovo Y700 / TB323FU) touchscreen bring-up, imported byte-for-byte from the pinned tree as original `patches/0025`; author headers and SPDX terms preserved. This is a different driver from the Gen4 Novatek NT36XXX SPI driver (`0140`, `CONFIG_TOUCHSCREEN_NOVATEK_NVT36XXX_SPI`), with different compatibles; the two coexist.
- `patches/0165-input-touchscreen-NT36536-host-download-SPI-driver-N.patch`
  source: https://github.com/joonhoekim/tb323fu-linux/commit/11a25bf89b65fff48112a91a0b25bc247e7736d8
  upstream: unknown
  notes: Carries only the driver hookup: the Kconfig `source` line is re-anchored mid-file immediately before `TOUCHSCREEN_ST7123`, and the Makefile object line is appended after the last existing entry; the driver source itself is taken separately from the pinned Novatek tree. Author headers are preserved, and this is a rebased hookup rather than a byte-for-byte import. Distinct from the Gen4 Novatek driver `0140`.
- `patches/0166-input-nt36536-fixes-for-tb323fu.patch`
  source: https://github.com/joonhoekim/tb323fu-linux/commit/11a25bf89b65fff48112a91a0b25bc247e7736d8
  upstream: unknown
  notes: Gen5 TB323FU NT36536 fixes, imported byte-for-byte as original `patches/0027`.
- `patches/0167-input-nt36536-make-the-pen-a-tablet-libinput-accepts.patch`
  source: https://github.com/joonhoekim/tb323fu-linux/commit/11a25bf89b65fff48112a91a0b25bc247e7736d8
  upstream: unknown
  notes: Gen5 TB323FU NT36536 pen-as-tablet change, imported byte-for-byte as original `patches/0053`.
- `patches/0168-clk-qcom-dispcc-kaanapali-solve-display-artifacts-at.patch`
  source: https://github.com/joonhoekim/tb323fu-linux/commit/11a25bf89b65fff48112a91a0b25bc247e7736d8
  upstream: unknown
  notes: Gen5 Kaanapali dispcc display-artifact fix, imported byte-for-byte as original `patches/0009`.
- `patches/0169-arm64-dts-qcom-kaanapali-add-the-second-DSI-controll.patch`
  source: https://github.com/joonhoekim/tb323fu-linux/commit/11a25bf89b65fff48112a91a0b25bc247e7736d8
  upstream: unknown
  notes: Gen5 Kaanapali second DSI controller devicetree node, imported byte-for-byte as original `patches/0019`.
- `patches/0170-drm-panel-nt36523-add-DSC-and-the-Lenovo-TB323FU-CSO.patch`
  source: https://github.com/joonhoekim/tb323fu-linux/commit/11a25bf89b65fff48112a91a0b25bc247e7736d8
  upstream: unknown
  notes: Gen5 NT36523 DSC and Lenovo TB323FU CSOT panel support, imported byte-for-byte as original `patches/0020`. `0141` already supplies all slices in a packet, so no duplicate packet-support import is made.
- `patches/0171-backlight-aw99706-replay-board-registers-after-the-H.patch`
  source: https://github.com/joonhoekim/tb323fu-linux/commit/11a25bf89b65fff48112a91a0b25bc247e7736d8
  upstream: unknown
  notes: AW99706 board-register replay after hardware enable, rebased onto the current driver whose init path ends with the `0057` KONKR Pocket Fit Elite CFG7 override. That board block is kept intact and the optional `awinic,init-regs` replay is appended after it, so this is a rebased delta rather than a byte-for-byte import; author headers and SPDX terms preserved.
- `patches/0172-iommu-arm-smmu-qcom-kaanapali-MDSS-identity-domain.patch`
  source: https://github.com/joonhoekim/tb323fu-linux/commit/11a25bf89b65fff48112a91a0b25bc247e7736d8
  upstream: unknown
  notes: Kaanapali MDSS SMMU identity domain, imported byte-for-byte as original `patches/0029`.
- `patches/0173-drm-panel-nt36523-baldur-CSOT-60-and-30-Hz-modes.patch`
  source: https://github.com/joonhoekim/tb323fu-linux/commit/11a25bf89b65fff48112a91a0b25bc247e7736d8
  upstream: unknown
  notes: Baldur CSOT 60 Hz and 30 Hz modes, imported byte-for-byte as original `patches/0066`.
- `patches/0174-drm-panel-nt36523-baldur-add-90-Hz-and-165-Hz-modes.patch`
  source: runtime-fixes/0109-drm-panel-nt36523-baldur-add-90-Hz-and-165-Hz-modes.patch
  upstream: local
  notes: runtime-fixes 165 Hz replacement, imported byte-for-byte; this is the 90 Hz/165 Hz addition, distinct from the pinned tb323fu-linux imports above.
- `patches/0175-drm-msm-baldur-dsc12-video.patch`
  source: armada
  upstream: local
  notes: Baldur-only DSC 1.2 video-mode delta layered on 0142. Selects the 4:4:4 RC table and the existing first-line bpg offset for video-mode 10-bpc, adds the downstream DCE initial-line helper (active soft-slice count and bits_per_pixel >> 4), rounds the INTF compressed width up to match DSI (952 * 8 / 30 -> 254), enables FULL_ICH_PREC for Baldur, and advertises it on every Kaanapali DSC block. Derived from the GPL-2.0-only joonhoekim/tb323fu-linux tree at 11a25bf89b65fff48112a91a0b25bc247e7736d8 (legacy 0007/0008/0023); Elden paths remain byte-identical.
- `patches/0176-arm64-baldur-cpu-idle-topology.patch`
  source: https://github.com/joonhoekim/tb323fu-linux/commit/11a25bf89b65fff48112a91a0b25bc247e7736d8
  upstream: unknown
  notes: Baldur CPU idle and topology rebase of the pinned tree's legacy 0031, 0106, 0117 and 0118 onto the 7.2.6 preimages; every other machine is unchanged.
- `patches/0177-power-supply-battmgr-baldur-usb-current.patch`
  source: https://github.com/joonhoekim/tb323fu-linux/commit/8bf7d9240bb97353f4c6dca53e4a65c18ade7bcf
  upstream: unknown
  notes: Combines the unified SM8750 INPUT_CURRENT_LIMIT setter with the TB323FU 0033 direct USB-property protocol, gated to lenovo,baldur.
- `patches/0178-drm-msm-baldur-dsi-phy-runtime-pm.patch`
  source: https://github.com/joonhoekim/tb323fu-linux/commit/11a25bf89b65fff48112a91a0b25bc247e7736d8
  upstream: unknown
  notes: Merges the pinned tree's 0032 and 0050 PHY runtime-PM work for Baldur; other machines keep the pm_clk list.
- `patches/0179-clk-qcom-baldur-gdsc-runtime-reference.patch`
  source: armada
  upstream: local
  notes: GDSC controller runtime hold derived from the pinned tree's 0046 and 0103, registered only for lenovo,baldur; the registration-time hold is dropped through a devm action, so a failed probe or the device's removal unwinds it while a normal disable releases only its own reference.
- `patches/0180-drm-msm-baldur-fifo-pll-teardown.patch`
  source: https://github.com/joonhoekim/tb323fu-linux/commit/11a25bf89b65fff48112a91a0b25bc247e7736d8
  upstream: unknown
  notes: PPB FIFO sizing, VCO state guards and bonded slave-first teardown from the pinned tree's 0007, 0008 and 0097, gated to Baldur.
- `patches/0181-drm-msm-kaanapali-dpu-int2-domain.patch`
  source: https://github.com/joonhoekim/tb323fu-linux/commit/11a25bf89b65fff48112a91a0b25bc247e7736d8
  upstream: unknown
  notes: Bindings and DT from the Kaanapali DPU INT2 GDSC v2 series (0081 and 0083); the driver hunk is the series' 0082 rebased with Baldur gating.
- `patches/0182-iio-adc-qcom-gen3-shared-sdam-irq.patch`
  source: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=b39f9b4b21df871005a59e2aea814644896d0dd3
  upstream: https://patch.msgid.link/20260722-gen3_adc_tm-v4-2-011981f756c8@oss.qualcomm.com
  notes: 7.2 SDAM request error context refreshed; verbose error retained.
- `patches/0183-thermal-qcom-adc-tm5-gen3-hook.patch`
  source: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=948ee3a74f35644c16b6c100f19ff0799cebe0b5
  upstream: https://patch.msgid.link/20260722-gen3_adc_tm-v4-3-011981f756c8@oss.qualcomm.com
  notes: 7.2.6 backport places the Gen3 stanza after the Gen2 ADC_TM5 entries; the MBG entry and driver are not imported.
- `patches/0184-thermal-qcom-adc-tm5-gen3-driver.patch`
  source: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=cebe359188013aa54e3afd06cc6461c6ebbfb6d3
  upstream: https://patch.msgid.link/20260811145427.3089426-1-daniel.lezcano@kernel.org
  notes: The follow-up that adds the missing Gen3 ADC_TM driver file, imported whole.
- `patches/0185-iio-thermal-qcom-pmic5-gen4-adc.patch`
  source: https://github.com/joonhoekim/tb323fu-linux/commit/0a115277960eb7bbc533fbd2362e3cf83e1ee21f
  upstream: unknown
  notes: Squashed PMIC5 Gen4 ADC series backported to 7.2.6 on top of the shared-IRQ and Gen3 ADC_TM work; MTP, QRD and glymur board hunks omitted.
- `patches/0186-soc-qcom-baldur-gpr-apm.patch`
  source: armada
  upstream: local
  notes: Merges the pinned TB323FU tree's 0075, 0076 and 0077 onto the unified APR, Q6APM and Kaanapali ADSP code: the GPR callback is installed before probe and cleared on failure, packet sends carry the service source port, and Baldur's ADSP gprint entry joins the existing one. Other machines keep the current AudioReach flow.
- `patches/0187-ASoC-baldur-audio-paths.patch`
  source: armada
  upstream: local
  notes: Merges the pinned tree's 0034, 0035 and 0037 onto the unified machine driver and codecs: MI2S back ends use S32_LE when the board opts in, LPASS TX replays board tuning values, and WCD939x channel access follows the resolved channel id so HiFi PCM is kept. The Kaanapali sound card and existing headphone paths are unchanged.
- `patches/0188-wifi-ath12k-baldur-range-scans.patch`
  source: armada
  upstream: local
  notes: Merges runtime fixes 1022 and 1023 and the three band-index corrections of reg-three-fix.diff.txt onto the unified ath12k scan lifecycle. WCN7860 on Baldur and Elden splits range scans at the firmware boundary and runs associated scans as drained TX batches, and firmware recovery takes over an interrupted TX pause. Elden needs this extension after the WCN7860 support and firmware-initialization patches switch it from the older `PEACH/hw2.0` firmware path to the newer `WCN7860/hw2.0` firmware. The extension retains the WCN7860 HW2.0, single-pdev, single-radio and DBS_OR_SBS guards; other machines and hardware keep the existing scan path.
- `patches/0189-Bluetooth-qca-baldur-brahma.patch`
  source: armada
  upstream: local
  notes: Merges the pinned tree's 0030 BRAHMA flow onto the unified Bluetooth path for Baldur only: patch configuration before UART setup, DT.mbn segment drain with the board's delays, and the existing baudrate helper for the 3.2 Mbaud switch. Other boards, including the native WCN7860 flow, are unchanged.
- `patches/0190-drm-msm-baldur-vfp-switch.patch`
  source: armada
  upstream: local
  notes: Rebuilds the pinned tree's 0065 and 0079 on the unified DPU, DSI and interface code, gating porch-only mode switching to lenovo,baldur through dpu_crtc_can_update_vfp. Colour management, active and connector checks and the existing flush semantics are unchanged, and other machines never take the path.
- `patches/0191-drm-msm-baldur-idle-refresh.patch`
  source: https://github.com/joonhoekim/tb323fu-linux/commit/11a25bf89b65fff48112a91a0b25bc247e7736d8
  upstream: unknown
  notes: Kernel-side idle refresh from the pinned tree's legacy 0110, layered on the 0190 vertical-front-porch switch: the committed mode and its vtotal are untouched while the running DSI video interface's VFP is stretched from the mode's own refresh rate, and the INTFs are flushed outside a commit; the vtotal is restored and latched before system suspend and on encoder disable. `idle_refresh_enable()` returns before arming the base mode, encoder pointer, notifiers or work unless `lenovo,baldur`, and every other machine stays on the existing path. Author and licence terms are kept; this is a rebase, not a byte-for-byte import.
- `patches/0192-usb-baldur-runtime-pm.patch`
  source: https://github.com/joonhoekim/tb323fu-linux/commit/11a25bf89b65fff48112a91a0b25bc247e7736d8
  upstream: unknown
  notes: Merges the pinned tree's legacy 0036, 0059, 0060, 0061, 0062 and 0104 onto the unified USB, Type-C, UCSI, dwc3-qcom and f_ncm code, keeping the original author header and GPL-2.0-only terms: Kaanapali USB GDSCs retain across system suspend, dwc3 records that xHCI lost its state and the platform resume quirk consumes it, the alt-mode and UCSI notifications move to the freezable workqueue on Baldur, and Baldur's eUSB2 line selection arms eD-/eD+ remote wakeup non-lazily. The dwc3 lost-state flag and the xHCI resume quirk read it only on `lenovo,baldur`, the eUSB2 branches follow a `has_eusb2_phy` flag that only Baldur sets, and QUSB2 plus the `eusb2_wakeup` knob are unchanged. The NCM TX-timer restart on `NETDEV_TX_BUSY` is generic, with skb ownership unchanged.
- `patches/0193-drm-msm-baldur-new-state-bandwidth.patch`
  source: https://github.com/joonhoekim/tb323fu-linux/commit/11a25bf89b65fff48112a91a0b25bc247e7736d8
  upstream: unknown
  notes: Rebuilds the pinned tree's legacy 0105 on the unified DPU core-perf code: `_dpu_core_perf_calc_bw()` takes the crtc state being checked alongside the crtc, and `lenovo,baldur` aggregates that state's planes with `drm_atomic_crtc_state_for_each_plane_state()` so the vote covers the first frame after DPMS off/on or resume instead of the empty committed state. Every other machine keeps the committed-plane loop unchanged, and the bandwidth factors, `max_bw_high` limit, clock vote and perf tuning are not altered; this is a correctness fix, not a limit increase.
- `patches/0194-wifi-ath12k-baldur-rx-panic.patch`
  source: https://github.com/joonhoekim/tb323fu-linux/commit/11a25bf89b65fff48112a91a0b25bc247e7736d8
  upstream: unknown
  notes: Merges the pinned tree's legacy 0058 and 0113 onto the unified ath12k datapath. The wifi7 PCI bus wake-up callback returns `-EBUSY` before `mhi_device_get_sync()` while `panic_in_progress()`, only for `lenovo,baldur`; other machines keep the unconditional wake. From 0113, the top-up of a deficit larger than one eighth of the ring and posting reaped buffers back without a complete MSDU are common correctness fixes and are deliberately not gated. The new deferrable one-second watchdog is set up on every board because `ath12k_dp_rx_free()` stops it unconditionally, but only `lenovo,baldur` arms it; `ath12k_dp_rx_refill_wd_stop()` uses `timer_shutdown_sync()` and is also called from `ath12k_dp_cleanup()` before the descriptors are freed.
- `patches/0195-remoteproc-kaanapali-soccp.patch`
  source: https://github.com/torvalds/linux/blob/v7.3-rc4/drivers/remoteproc/qcom_q6v5_pas.c
  upstream: unknown
  notes: Adds the SoCCP remoteproc that 7.2.6 lacks even though it already reserves `soccp@a03d0000` and `soccp-dtb@a08d0000`: the `kaanapali_soccp_resource` and its `qcom,kaanapali-soccp-pas` match entry are copied from v7.3-rc4 (crash SMEM 656, `soccp.mbn`/`soccp_dtb.mbn`, PAS 51/0x41, cx/mx, soccp ssr/sysmon, auto_boot, early_boot), the DT side takes the upstream `smp2p-soccp` block (IPCC client 13, remote pid 19) and `remoteproc@d00000` node (six IRQs, cx/mx, the two existing reserved regions, stop/ping, glink edge) verbatim from https://github.com/torvalds/linux/blob/v7.3-rc4/arch/arm64/boot/dts/qcom/kaanapali.dtsi, and `IPCC_MPROC_SOCCP` is the only constant added to `qcom-ipcc.h`. The driver delta preserves the pinned tree's 0067 (handover IRQ enable/disable kept for `lenovo,baldur` only, `handover_issued` always set) and 0068 (the `dtb_pas_id` shutdown no longer masks a `pas_id` failure) semantics on this tree's `qcom_scm_pas_shutdown()`. The 7.3 driver refactor, the `firmware/qcom/qcom_pas` API and the glymur/nord resources are not imported; existing reserved-memory nodes, other Kaanapali boards and Gen4 are unchanged.
- `patches/0196-firmware-devfreq-kaanapali-memlat.patch`
  source: https://github.com/joonhoekim/tb323fu-linux/commit/11a25bf89b65fff48112a91a0b25bc247e7736d8
  upstream: unknown
  notes: Rebases that eight-patch series onto 7.2.6 without redesign: SCMI QCOM generic extension documentation, binding, vendor implementation and client header; the devfreq `target_freq` attribute flag, `track_remote` flag, remote observer governor and SCMI QCOM memlat driver; and the Kaanapali CPUCP mailbox, SRAM window and `scmi-1` vendor `protocol@80`. Original authorship is Pragnesh Papaniya, Sibi Sankar, Amir Vajid, Ramakrishna Gottimukkula and Jia Yang per those files; their tags are preserved and no sign-off is added. The live Gen5 kernel already runs LLCC, DDR and DDR_QOS DVFS through this governor and its config plus DT already carry the nodes, so this restores existing function rather than introducing a new policy. The driver binds only vendor protocol 0x80 and the Kaanapali compatible; PDP0 SCMI and the CPU cpufreq path are untouched.
- `patches/0197-drm-msm-dp-restore-kaanapali-DP0-and-idle-guard.patch`
  source: devices/lenovo-tb323fu/kernel/tb323fu-linux-11a25bf8 (legacy 0051)
  upstream: unknown
  notes: Restores the Kaanapali DP0 descriptor at 0x09ad2000 and its exact compatible match on 7.2.6, completing the DPU component aggregate. Retains Joonhoe Kim attribution. The 7.2.6 atomic-disable callback still calls PUSH_IDLE unconditionally, so also restores the dp->power_on guard to avoid accessing an unclocked link after failed training. No display timing, performance policy, or Gen4 DT changes.
- `patches/0198-clk-qcom-gpucc-kaanapali-mark-cx-gdsc-votable.patch`
  source: https://github.com/torvalds/linux/commit/55981579b27b6541aeeab81c768f248dba0491ac
  upstream: https://github.com/torvalds/linux/commit/55981579b27b6541aeeab81c768f248dba0491ac
  notes: Restores the legacy TB323FU 0071 backport omitted from the 7.2.6 tree. The Kaanapali `gpu_cc_cx_gdsc` descriptor drops `POLL_CFG_GDSCR` and gains `VOTABLE` while keeping `RETAIN_FF_ENABLE`. The patch is byte-for-byte legacy 0071 and changes only the Kaanapali GPU clock controller, not SM8750. It is a descriptor correction and makes no claim to have reproduced or fixed the observed GPU/GMU freeze.
- `patches/0199-clk-qcom-common-complete-runtime-suspend-on-probe.patch`
  source: https://git.kernel.org/torvalds/c/a5da6c5697a50a5956ae7645e52d0007ac8b3395
  upstream: https://lore.kernel.org/r/20260427-gfx-clk-fixes-v2-3-797e54b3d464@oss.qualcomm.com
  notes: Byte-for-byte import of the upstream fix for `qcom_cc_really_probe`: with `use_rpm`, the probe reference is released with `pm_runtime_put_sync()` so runtime suspend completes synchronously before probe returns. This is the prerequisite for 0200, which calls `pm_runtime_disable()` after registration; with the previous `pm_runtime_put()` the disable could prevent suspend from completing and leave the controller and its rails on. The change applies to every qcom clock controller that sets `use_rpm`; 0200 is the Kaanapali GXCLK-specific follow-up.
- `patches/0200-clk-qcom-gxclkctl-remove-ifpc-rail-votes.patch`
  source: https://git.kernel.org/torvalds/c/1c99638642c2137d7e699e7b2ab400df7ef15774
  upstream: https://lore.kernel.org/r/20260427-gfx-clk-fixes-v2-4-797e54b3d464@oss.qualcomm.com
  notes: Upstream GXCLK probe change rebased onto 7.2.6. Only the include hunk is refreshed: this tree's `gxclkctl-kaanapali.c` has no `linux/mod_devicetable.h`, so that context line is dropped and `linux/pm_runtime.h` is added after `linux/platform_device.h`. `qcom_cc_probe()`'s return value is kept in `ret`, failure returns early, and `pm_runtime_disable()` runs after successful registration. Author, message and sign-offs are unchanged; the existing `gdsc_gx_disable` power-off callback is not touched. Applies only to GXCLK (Kaanapali and its compatibles), while 0199 is generic to all `use_rpm` users. Depends on 0199 so the runtime suspend has already completed.
- `patches/0201-drm-msm-dpu-clear-pending-peripheral-flush-state.patch`
  source: https://github.com/torvalds/linux/commit/a5b5cc909931572aec446e129c035b76b3f0c1fa
  upstream: https://lore.kernel.org/r/20260828065440.140410-1-saimzst@gmail.com
  notes: Verbatim upstream one-line backport. `dpu_hw_ctl_clear_pending_flush()` now resets the existing stale cached peripheral flush mask together with the other child masks. This relates to the PERIPH flush bit 30, not the current DSC bit 22, and makes no claim to fix the current DSC timeout.
- `patches/0202-drm-msm-adreno-increase-gmu-firmware-init-timeout.patch`
  source: https://github.com/torvalds/linux/commit/ba970587a0e1203b6a0934b5b9174886b4c4d24c
  upstream: https://lore.kernel.org/r/20260912150915.28700-1-robin.clark@oss.qualcomm.com
  notes: Verbatim upstream one-line backport raising the GMU firmware init poll timeout from 10 ms to 100 ms to match kgsl's margin. The previously recorded GX_BW vote failure occurred after firmware init, so no causal claim is made.
- `patches/0203-pmdomain-qcom-rpmhpd-skip-retention-by-default.patch`
  source: https://github.com/torvalds/linux/commit/7178817f190496f2bfdec181fb4aa72832448468
  upstream: https://github.com/torvalds/linux/commit/7178817f190496f2bfdec181fb4aa72832448468
  notes: Upstream retention fix rebased onto the post-0121 tree. It removes the `skip_retention_level` documentation, field and all four initializers (`mxc`, `mxc_ao`, and 0121's `gfx_gmu`/`gmxc_sm8750`) because the field no longer exists, and makes `rpmhpd_update_level_mapping()` skip `RPMH_REGULATOR_LEVEL_RETENTION` unconditionally for every rpmhpd mapping. 0121's `presync_floor` field and its SM8550/SM8750 selectors are retained; SM8850 is not given presync_floor. Existing Kaanapali GFX/GMXC/MMCX can be enabled through a child GDSC before an explicit OPP is set. This is the generic upstream fix, not an older-SoC presync workaround. Author, message and sign-offs are unchanged apart from a bracketed modification note.
- `patches/0204-drm-msm-baldur-flush-shared-ctl-once.patch`
  source: armada
  upstream: local
  notes: Baldur-specific shared-CTL flush handling based on the vendor SDE `needs_single_flush` helper at https://github.com/OnePlusOSS/android_kernel_modules_and_devicetree_oneplus_sm8850/tree/5ab2a689ff87d7d28c511f1762cf41c1b90d965a (revision `5ab2a689ff87d7d28c511f1762cf41c1b90d965a`; `sde_encoder_phys.h`, `sde_encoder.c`, `sde_encoder_phys_vid.c`). Where two enabled video-mode DSI physical encoders with DSC share one CTL, the kickoff aggregates their pending flush bits and triggers the existing master path once instead of flushing the same CTL for each physical encoder. Command mode, writeback, single-encoder, split-CTL and incomplete sets keep the existing decision, and no DSC parameters, timeout, locking or wait logic changes.
- `patches/0205-drm-msm-defer-ring-and-vm-free-with-rcu.patch`
  source: https://github.com/torvalds/linux/commit/01c8d1f385f788f1bbbbb7687c4386d614281218
  upstream: https://github.com/torvalds/linux/commit/01c8d1f385f788f1bbbbb7687c4386d614281218
  notes: Verbatim upstream backport. Defers the ring and VM teardown through RCU so an in-flight submission cannot observe freed objects during cleanup. Generic DRM/MSM correctness fix, not a Kaanapali- or Baldur-specific change.
- `patches/0206-drm-msm-adreno-release-autosuspend-on-cleanup.patch`
  source: https://github.com/torvalds/linux/commit/6fbbf1e152f34ad3913e4a6476680aba672c5068
  upstream: https://github.com/torvalds/linux/commit/6fbbf1e152f34ad3913e4a6476680aba672c5068
  notes: Verbatim upstream backport. Releases the autosuspend reference in the adreno cleanup path so a partial or failed teardown does not leave the device's runtime-PM reference held. Generic correctness fix applied to all adreno devices.
- `patches/0207-thermal-step-wise-release-cleared-trip-vote.patch`
  source: https://github.com/torvalds/linux/commit/ec0d89150a9381d591344a9f6f5428655c227a7f
  upstream: https://github.com/torvalds/linux/commit/ec0d89150a9381d591344a9f6f5428655c227a7f
  notes: Verbatim upstream backport. Releases the step-wise governor's vote when a trip point is cleared, so the governor no longer holds a stale cooling state. Fixes a vote leak only; cooling maps and thermal policy are unchanged.
- `patches/0208-input-aw86927-accept-aw86937.patch`
  source: devices/lenovo-tb323fu/kernel/tb323fu-linux-11a25bf8 (legacy 0038)
  upstream: unknown
  notes: Extends the AW86927 haptic driver's chip-ID matching so the AW86937 variant also probes. Chip-ID handling only; no haptic waveform, gain or timing changes.
- `patches/0209-leds-rgb-add-aw22127-driver.patch`
  source: devices/lenovo-tb323fu/kernel/tb323fu-linux-11a25bf8 (legacy 0048)
  upstream: unknown
  notes: Adds the AW22127 RGB LED controller driver and its LED-class registration. New driver plus its config symbol only; no existing LED driver is modified.
- `patches/0210-drm-msm-snapshot-sspp-rec-blocks.patch`
  source: devices/lenovo-tb323fu/kernel/tb323fu-linux-11a25bf8 (legacy 0052)
  upstream: unknown
  notes: Includes the SSPP rectangle blocks in the DRM/MSM snapshot so a devcoredump carries each pipe's source-rect state. Debug capture only; no plane programming or flush behaviour changes.
- `patches/0211-clk-qcom-kaanapali-fix-pcie-rscc-enable-bits.patch`
  source: https://github.com/torvalds/linux/commit/0bfb542fc3368371cefb4cf45ef21c3ba2d37c3f
  upstream: https://github.com/torvalds/linux/commit/0bfb542fc3368371cefb4cf45ef21c3ba2d37c3f
  notes: Verbatim upstream backport correcting the PCIe RSCC enable bit positions in the Kaanapali clock controller. Upstream correctness fix for the clock descriptor; no rate or vote policy is added.
- `patches/0212-clk-qcom-kaanapali-force-pcie-pipe-memory.patch`
  source: devices/lenovo-tb323fu/kernel/tb323fu-linux-11a25bf8 (legacy 0072)
  upstream: unknown
  notes: Forces the PCIe pipe memory on for the Kaanapali clock controller so the pipe clock remains usable while the link is active. Kaanapali-gated; SM8750 and other SoCs are not touched.
- `patches/0213-clk-qcom-branch-clear-enable-on-failure.patch`
  source: devices/lenovo-tb323fu/kernel/tb323fu-linux-11a25bf8 (legacy 0073)
  upstream: unknown
  notes: Clears the branch enable bit when a clock enable fails, so a failed enable does not leave the branch half-enabled. Generic clock-framework path shared by all branches; no per-SoC policy.
- `patches/0214-pci-qcom-baldur-disable-slot-hotplug-capabilities.patch`
  source: devices/lenovo-tb323fu/kernel/tb323fu-linux-11a25bf8 (legacy 0049)
  upstream: unknown
  notes: Clears the slot hotplug capability bits the Baldur root complex does not implement, so the PCIe core does not advertise hotplug on that controller. Baldur only; no link-training or power sequencing changes.
- `patches/0215-phy-qcom-qmp-combo-add-kaanapali-usb3-dp.patch`
  source: devices/lenovo-tb323fu/kernel/tb323fu-linux-11a25bf8 (legacy 0096)
  upstream: unknown
  notes: Adds the Kaanapali USB3/DP combo PHY description and its compatible so the QMP combo driver binds on that SoC. Kaanapali-gated; the SM8750 and SM8650 tables are left as they are.
- `patches/0216-phy-qcom-qmp-combo-balance-pipe-clock-on-error.patch`
  source: devices/lenovo-tb323fu/kernel/tb323fu-linux-11a25bf8 (legacy 0099)
  upstream: unknown
  notes: Balances the pipe clock reference on the QMP combo PHY error path, so a failed setup does not leave the clock enabled. Generic error-path correctness fix.
- `patches/0217-phy-qcom-qmp-combo-keep-kaanapali-dp-lanes.patch`
  source: devices/lenovo-tb323fu/kernel/tb323fu-linux-11a25bf8 (legacy 0100)
  upstream: unknown
  notes: Keeps the DP lane configuration programmed on the Kaanapali combo PHY instead of dropping it during reconfiguration. Kaanapali-gated; no lane count or link-rate change.
- `patches/0218-drm-msm-dp-handle-phy-and-link-status-errors.patch`
  source: devices/lenovo-tb323fu/kernel/tb323fu-linux-11a25bf8 (legacy 0101)
  upstream: unknown
  notes: Propagates PHY and link-status failures out of the DP link-training path instead of continuing as if training succeeded. Generic DP correctness fix; display timing and power policy are untouched.
- `patches/0219-drm-msm-dp-reset-sink-link-capabilities.patch`
  source: devices/lenovo-tb323fu/kernel/tb323fu-linux-11a25bf8 (legacy 0102)
  upstream: unknown
  notes: Resets the cached sink link capabilities before re-reading them, so a stale capability set is not reused across a replug. Generic DP correctness fix.
- `patches/0220-drm-msm-dp-baldur-balance-plugged-runtime-pm.patch`
  source: devices/lenovo-tb323fu/kernel/tb323fu-linux-11a25bf8 (legacy 0098)
  upstream: unknown
  notes: Balances runtime PM around the plugged state on the Baldur DP controller. Baldur only; no runtime-PM behaviour is changed for other boards or for the DSI path.
- `patches/0221-drm-msm-dp-baldur-retrain-after-quick-replug.patch`
  source: devices/lenovo-tb323fu/kernel/tb323fu-linux-11a25bf8 (legacy 0057)
  upstream: unknown
  notes: Retrains the DP link after a quick replug on Baldur, where the previous link state can still look valid. Baldur only; no change to link-training parameters or timeouts for other boards.
- `patches/0222-arm64-dts-qcom-kaanapali-describe-ufs-mcq.patch`
  source: devices/lenovo-tb323fu/kernel/tb323fu-linux-11a25bf8 (legacy 0119)
  upstream: unknown
  notes: Describes the UFS multi-circular-queue interrupts and queue properties in the Kaanapali DTSI. Kaanapali devicetree only; no driver or queue-depth change is included.
- `patches/0223-Bluetooth-qca-baldur-preserve-configured-address.patch`
  source: armada
  upstream: local
  notes: On lenovo,baldur, preserve a public Bluetooth address already configured through MGMT instead of replacing it during the existing 0501 serial-address setup. Other machines retain the existing serial-derived address behavior. The paired startup helper waits for management readiness and confirms the address by reading it back; a request acknowledgement alone is not treated as success.
