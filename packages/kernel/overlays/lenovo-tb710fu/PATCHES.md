# TB710FU overlay patches

Applied in name order after `patches/series`. `source` and `upstream` follow
the convention of `packages/kernel/PATCHES.md`.

## Board DTS

- `dts/sm8650-lenovo-tb710fu.dts`
  source: https://github.com/MeteoricmeteorLL/tb710fu-linux/blob/16e800d4f6e5cf4a8517900a9e11eec0ebc91bd3/kernel/sources/arch/arm64/boot/dts/qcom/sm8650-lenovo-tb710fu.dts
  upstream: unknown
  license: GPL-2.0-only. The file carries no notice of its own; the repository's [LICENSE](https://github.com/MeteoricmeteorLL/tb710fu-linux/blob/16e800d4f6e5cf4a8517900a9e11eec0ebc91bd3/LICENSE) (section 3) puts `kernel/sources/` under GPL-2.0. An SPDX line and an origin header were added; the rest is verbatim.
- `dts/sm8650-lenovo-tb710fu-board.dtso`
  source: armada
  notes: kShimC simple-framebuffer template, the 903 MHz GPU level, the unit's own ADSP/CDSP firmware paths, the TB710FU touch compatible; disables the original port's boot-log reservations and iris, and clears another unit's Bluetooth address.

## Display, GPU, backlight

- `patches/0101-dt-bindings-backlight-sy7758-add-follower.patch`
  source: https://github.com/MeteoricmeteorLL/tb710fu-linux/blob/16e800d4f6e5cf4a8517900a9e11eec0ebc91bd3/kernel/patches/TB710FU-full-tree.diff
  upstream: local
  notes: Documents `silergy,follower`, a second SY7758 that mirrors the primary chip's brightness.
- `patches/0102-backlight-sy7758-follower-chips.patch`
  source: https://github.com/MeteoricmeteorLL/tb710fu-linux/blob/16e800d4f6e5cf4a8517900a9e11eec0ebc91bd3/kernel/patches/TB710FU-full-tree.diff
  upstream: local
  notes: Follower chips register no backlight device and get every brightness update of the primary; boards with one chip are unchanged. A follower joins the mirror list only after its first brightness write succeeds. Burst mode is turned off on lenovo,tb710fu only.
- `patches/0103-drm-panel-add-novatek-nt36532.patch`
  source: https://github.com/MeteoricmeteorLL/tb710fu-linux/blob/16e800d4f6e5cf4a8517900a9e11eec0ebc91bd3/kernel/sources/drivers/gpu/drm/panel/panel-nt36532-wqhd-dual-dsi-dsc.c
  upstream: unknown
  notes: TB710FU NT36532 dual-DSI DSC panel, renamed panel-novatek-nt36532; Chinese comments translated, the empty remove callback and the placeholder module author dropped. Both links attach with devm_mipi_dsi_attach(); the physical size is 274x171 mm (12.7", 16:10) instead of the original 185x150.
- `patches/0104-drm-msm-dsi-bonded-trigger-on-dsi0.patch`
  source: https://github.com/MeteoricmeteorLL/tb710fu-linux/blob/16e800d4f6e5cf4a8517900a9e11eec0ebc91bd3/kernel/patches/TB710FU-full-tree.diff
  upstream: unknown
  notes: The original swapped the legacy bonded command trigger to DSI_0 for every board; here only lenovo,tb710fu does.
- `patches/0105-drm-msm-dsi-bootloader-controller-phy-state.patch`
  source: https://github.com/MeteoricmeteorLL/tb710fu-linux/blob/16e800d4f6e5cf4a8517900a9e11eec0ebc91bd3/kernel/patches/TB710FU-full-tree.diff
  upstream: unknown
  notes: Controller/PHY register state, vendor D-PHY timings, rescode and no sw_reset on underflow, gated on lenovo,tb710fu (the original applied them to every 7nm PHY and DSI host). The heartbeat, register dumps and TLMM pad reads are left out; only the 10 s re-application of the controller state remains.
- `patches/0106-drm-msm-dpu-dsc-width-round-up.patch`
  source: https://github.com/MeteoricmeteorLL/tb710fu-linux/blob/16e800d4f6e5cf4a8517900a9e11eec0ebc91bd3/kernel/patches/TB710FU-full-tree.diff
  upstream: unknown
- `patches/0107-drm-msm-a6xx-a750-without-ifpc.patch`
  source: armada
  upstream: local
  notes: The original disabled IFPC for every A750; a machine-matched catalog entry keeps it for the other SM8650 boards.

## Touch and pen

Novatek NT36532 touch IC with the Lenovo Tab Pen Plus, on SPI4. Its DT
compatible is set by the board overlay; the pen is enabled by the touch
overlay.

(added with the touch part)

## Bluetooth and WiFi

WCN7850 on PCIe0 (ath12k) and UART14 (btqca). No DT overlay: the board tree
describes both; the Bluetooth address is cleared by the board overlay.

(added with the Bluetooth and WiFi part)

## Speakers

Four Awinic AW882xx amplifiers on i2c_hub_4 fed by SECONDARY_TDM_RX_0
(8 x 32-bit slots, slots 0-3). The amplifier driver is the shared
out-of-tree aw882xx.

- `patches/0401-ASoC-qcom-common-add-DAI-node-TDM-slot-helpers.patch`
  source: https://git.kernel.org/torvalds/c/1e844ecbc9cb
  upstream: https://git.kernel.org/torvalds/c/1e844ecbc9cb
- `patches/0402-ASoC-qcom-common-distinguish-missing-and-invalid-TDM-slots.patch`
  source: https://git.kernel.org/torvalds/c/0a9e00d5ebdf
  upstream: https://git.kernel.org/torvalds/c/0a9e00d5ebdf
- `patches/0403-ASoC-qcom-q6apm-lpass-dais-add-TDM-DAI-operations.patch`
  source: https://git.kernel.org/torvalds/c/4d0840175893
  upstream: local (adapted from 4d0840175893 by Prasad Kumpatla)
  notes: Rewritten for this tree, which has no MI2S clock control (no set_sysclk); also records the TDM interface derived from the DAI id.
- `patches/0404-ASoC-qcom-audioreach-configure-TDM-sink-endpoint.patch`
  source: armada
  upstream: local
  notes: MODULE_ID_TDM_SINK with PARAM_ID_TDM_INTF_CFG, matching what the vendor AudioReach userspace sends on TB710FU (traced on the unit); replaces the original port's full 7.3 sound/soc/qcom backport. Needs a topology whose speaker endpoint is MODULE_ID_TDM_SINK (0x0700100E); a topology with I2S_SINK never reaches this code.
- `patches/0405-ASoC-qcom-q6apm-four-speakers.patch`
  source: armada
  upstream: local
  notes: On lenovo,tb710fu the stream MFC/data-logging modules run at 4ch/S32 and the MFC maps FL FR FL FR, so the stereo stream reaches all four TDM slots. This applies to every MFC on the board; the card has the speaker link only, and a capture or second playback link would have to narrow it.
- `patches/0406-ASoC-dt-bindings-qcom-sm8250-add-tb710fu-card.patch`
  source: armada
  upstream: local
- `patches/0407-ASoC-qcom-sc8280xp-add-tb710fu-card.patch`
  source: armada
  upstream: local
  notes: TDM BE fixup (4ch/S32) and DSP_B/IB_NF CPU format for `lenovo,tb710fu-sndcard` only. The TDM endpoint has no bit-clock inversion setting, so IB_NF is sent as plain DSP_B; the values match what the vendor userspace programs.
- `dts/sm8650-lenovo-tb710fu-audio.dtso`
  source: armada
  notes: TDM1 pins, the four amplifiers (reset GPIOs, sound channels) and the sound card, as on the vendor Android.
