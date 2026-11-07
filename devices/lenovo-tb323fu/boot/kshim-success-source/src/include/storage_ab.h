/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <storage_block.h>

#define KSHIM_AB_LUNS 6
#define KSHIM_AB_UNCERTAIN (-30)
struct kshim_ab_media {
    struct kshim_blockdev lun[KSHIM_AB_LUNS];
    uint8_t boot_id[KSHIM_AB_LUNS];
    void *ctx;
    int (*get_boot_lun)(void *, uint32_t *);
    int (*set_boot_lun)(void *, uint32_t);
};
struct kshim_ab_status { unsigned current; unsigned bootable[2]; };
/* Qualcomm GPT slot layout, with zero AOSP misc control. No GPT repair,
 * unlock, AVB rewriting, or payload writes. A failed rollback is UNCERTAIN. */
int kshim_ab_inspect(const struct kshim_ab_media *, struct kshim_ab_status *);
int kshim_ab_select(const struct kshim_ab_media *, unsigned target, unsigned expected_current);
