# SPDX-License-Identifier: BSD-3-Clause
# Lenovo Xiaoxin Pad Pro GT (TB710FU): four AW882xx speakers on
# SECONDARY_TDM_RX_0, fed by MultiMedia1. The card has no other link.
# Based on audioreach-topology SM8650-MTP.m4 (Copyright, Linaro Ltd, 2023).
include(`audioreach/audioreach.m4')
include(`audioreach/stream-subgraph.m4')
include(`audioreach/device-subgraph.m4')
include(`util/route.m4')
include(`util/mixer.m4')
include(`audioreach/tokens.m4')
dnl Playback MultiMedia1
STREAM_SG_PCM_ADD(audioreach/subgraph-stream-vol-playback.m4, FRONTEND_DAI_MULTIMEDIA1,
	`S16_LE', 48000, 48000, 2, 2,
	0x00004001, 0x00004001, 0x00006001, `110000')
dnl
dnl Speakers: SECONDARY TDM RX 0 (the kernel configures the slots)
DEVICE_SG_ADD(sm8650/subgraph-device-tdm-playback.m4, `Secondary TDM0', SECONDARY_TDM_RX_0,
	`S16_LE', 48000, 48000, 2, 2,
	LPAIF_INTF_TYPE_LPAIF, 1, 0, DATA_FORMAT_FIXED_POINT,
	0x00004040, 0x00004040, 0x00006050, `SECONDARY_TDM_RX_0')
dnl
STREAM_DEVICE_PLAYBACK_MIXER(SECONDARY_TDM_RX_0, ``SECONDARY_TDM_RX_0'', ``MultiMedia1'')
dnl
STREAM_DEVICE_PLAYBACK_ROUTE(SECONDARY_TDM_RX_0, ``SECONDARY_TDM_RX_0 Audio Mixer'', ``MultiMedia1, stream0.logger1'')
