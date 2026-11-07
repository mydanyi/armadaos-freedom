/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <device_info.h>
#include <storage_settings.h>
#include <stdbool.h>
#include <storage_ab.h>
int storage_settings_native_open(enum device_board actual_board,
                                  const struct kshim_config_values *defaults);
/* Records the selected system after UI teardown, preserving incoming MMU
 * state. An unsafe controller/MMU cleanup forbids subsequent payload handoff. */
int storage_settings_native_note_boot(uint32_t target);
struct kshim_settings_session *storage_settings_native_session(void);
bool storage_settings_native_handoff_safe(void);
int storage_settings_native_export(void *fdt);
bool storage_settings_native_ab_supported(void);
int storage_settings_native_ab_status(struct kshim_ab_status *);
int storage_settings_native_ab_select(unsigned target,unsigned expected_current);
