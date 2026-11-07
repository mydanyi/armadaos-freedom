/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <storage_ufs.h>
#include <storage_settings.h>
#include <storage_ab.h>
/* Zero-initialize once. All members, arena and preflight context must remain
 * alive through the menu. Never clear an unsafe or owned backend. */
struct kshim_ufs_settings {
    struct kshim_settings_session settings;
    struct kshim_ufs_host host;
    struct kshim_ufs_lun lun;
    struct kshim_ufs_ops ops;
    void *arena, *preflight_context;
    int (*preflight)(void *, uint64_t *dma_address);
    uint32_t board, block_size, save_attempts;
    uint64_t block_count;
    int unsafe, load_status, begin_status, release_status;
    uint8_t scratch[KSHIM_STORAGE_SCRATCH_BYTES];
};
/* Load only from an existing provisioned internal store. The controller is
 * released before return. Every save reclaims it, revalidates the same media
 * identity, writes one settings slot, and releases again. Preflight must prove
 * current DMA mapping and actual-board identity before EVERY claim. */
int kshim_ufs_settings_open(struct kshim_ufs_settings *,
    const struct kshim_ufs_ops *, void *arena, uint32_t actual_board,
    int (*preflight)(void *, uint64_t *), void *preflight_context,
    const struct kshim_config_values *defaults);
bool kshim_ufs_settings_handoff_safe(const struct kshim_ufs_settings *);
int kshim_ufs_settings_ab(struct kshim_ufs_settings *, struct kshim_ab_status *,
                           bool change, unsigned target, unsigned expected_current);
