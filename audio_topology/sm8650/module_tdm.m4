# SPDX-License-Identifier: BSD-3-Clause
# Based on audioreach-topology audioreach/module_i2s.m4 (Copyright, Linaro Ltd, 2023).
dnl TDM sink endpoint: AR_MODULE_I2S_RX with MODULE_ID_TDM_SINK, the widget
dnl named audio_if_rx, the full DAI stream name passed in, and no SD line.
dnl The kernel takes the TDM slots, and with them the interface index, from
dnl the DAI (set_tdm_slot); the interface values here are its fallback.
include(`util/util.m4') dnl
ifdef(`MODULE_ID_TDM_SINK', , `define(`MODULE_ID_TDM_SINK', `0x0700100E')') dnl
dnl AR_MODULE_TDM_RX(index, sgidx, container-idx, iid, maxip-ports, max-op-ports, in-ports, outports, src-port, dst-port,
dnl hw-if-type, hw-if-idx, fmt, stream-name, dst-iid)
define(`AR_MODULE_TDM_RX',
`'
`SectionVendorTuples."NAME_PREFIX.audio_if_rx$1_tuples" {'
`        tokens "audioreach_tokens"'
`'
`        tuples."word.u32_data" {'
`                AR_TKN_U32_MODULE_INSTANCE_ID STR($4)'
`                AR_TKN_U32_MODULE_ID STR(MODULE_ID_TDM_SINK)'
`                AR_TKN_U32_MODULE_MAX_IP_PORTS STR($5)'
`                AR_TKN_U32_MODULE_MAX_OP_PORTS STR($6)'
`                AR_TKN_U32_MODULE_IN_PORTS STR($7)'
`                AR_TKN_U32_MODULE_OUT_PORTS STR($8)'
`                AR_TKN_U32_MODULE_SRC_OP_PORT_ID STR($9)'
`                AR_TKN_U32_MODULE_DST_IN_PORT_ID STR($10)'
`                AR_TKN_U32_MODULE_SRC_INSTANCE_ID STR($4)'
`                AR_TKN_U32_MODULE_DST_INSTANCE_ID STR($15)'
`                AR_TKN_U32_MODULE_HW_IF_TYPE STR($11)'
`                AR_TKN_U32_MODULE_HW_IF_IDX STR($12)'
`                AR_TKN_U32_MODULE_FMT_DATA STR($13)'
`        }'
`}'
`'
`SectionData."NAME_PREFIX.audio_if_rx$1_data" {'
`        tuples "NAME_PREFIX.audio_if_rx$1_tuples"'
`}'
`'
`SectionWidget."NAME_PREFIX.audio_if_rx$1" {'
`        index STR($1)'
`        type "aif_in"'
`        no_pm "true"'
`        stream_name "$14 Playback"'
`        subseq "10"'
`        data ['
`                "NAME_PREFIX.sub_graph$2_data"'
`                "NAME_PREFIX.container$3_data"'
`                "NAME_PREFIX.audio_if_rx$1_data"'
`        ]'
`}') dnl
