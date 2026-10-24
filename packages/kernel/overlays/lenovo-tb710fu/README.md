# Lenovo Xiaoxin Pad Pro GT (TB710FU) kernel overlay

SM8650 tablet, booted from the kShimC menu in boot_a next to the stock Android.
Nothing here is used by the default build; build it with

```sh
KERNEL_OVERLAYS=lenovo-tb710fu bash scripts/build-kernel.sh
```

The kernel release gets `-armada-tb710fu`. The DTB to boot is
`dtb/qcom/sm8650-lenovo-tb710fu-kshim.dtb`: the board tree
`dts/sm8650-lenovo-tb710fu.dts`, taken from MeteoricmeteorLL/tb710fu-linux
(GPL-2.0; only an SPDX and origin header added), with the `dts/*.dtso` overlays applied by fdtoverlay (`dts/*.mk`, in name order).

| Part | Patches | DT overlay | Config |
|---|---|---|---|
| Board, display, GPU, backlight | `01xx` | `sm8650-lenovo-tb710fu-board.dtso` | `10-board.config` |
| Touch and pen | `02xx` | `sm8650-lenovo-tb710fu-touch.dtso` | `20-touch.config` |
| Bluetooth and WiFi | `03xx` | — | — |
| Speakers | `04xx` | `sm8650-lenovo-tb710fu-audio.dtso` | — |

The speakers also need, at run time: an AudioReach topology whose speaker
endpoint is the TDM sink (0x0700100E; the original port's topology uses an
I2S sink), the amplifiers' `aw882xx_acf.bin`, and a 16-bit front end
(24-bit input crashes the ADSP audio process).  See
`devices/lenovo-tb710fu/` for the files and mixer settings.

Board-specific code paths are additionally gated on `lenovo,tb710fu` (or the
`lenovo,tb710fu-sndcard` / `novatek,nt36532-ts-spi` compatibles). Sources and
notes per patch: [PATCHES.md](PATCHES.md). Device status, firmware and boot
packaging: `devices/lenovo-tb710fu/`.
