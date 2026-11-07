/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <storage_block.h>
#include <stdbool.h>
/* Single-slot polled UFSHCI legacy transport. Board code must first establish
 * clocks/link and exclusive DMA ownership. dma_address is an established bus
 * mapping of the entire aligned arena, not an assumed CPU physical address.
 * Zero-initialize the host before its first claim. The arena/mapping must
 * survive any error until release returns success. */
#define KSHIM_UFS_DMA_BYTES 8192U
enum kshim_ufs_result {
    KSHIM_UFS_TIMEOUT = -20, KSHIM_UFS_BUSY = -21,
    KSHIM_UFS_UNSUPPORTED = -22, KSHIM_UFS_RESPONSE = -23,
    KSHIM_UFS_SCSI = -24, KSHIM_UFS_QUARANTINED = -25
};
struct kshim_ufs_ops {
    void *ctx;
    int (*read32)(void *, uint32_t offset, uint32_t *);
    int (*write32)(void *, uint32_t offset, uint32_t);
    void (*sync_device)(void *, void *, size_t);
    void (*sync_cpu)(void *, void *, size_t);
    uint64_t (*now_ms)(void *);
    void (*delay_us)(void *, uint32_t);
    /* Opt-in link management, default false: claim queries the MPHY TX FSM
     * state the firmware left behind and, when it is HIBERN8, exits it before
     * the first command; release restores the original state. Boards that
     * already own the link keep it false and retain the legacy semantics. */
    bool manage_link;
};
/* Link state for the opt-in manage_link path. phase is 0 when no link work
 * ran, 1 while querying the entry state, 2 while exiting HIBERN8, 3 while
 * confirming the awake state, 4 while re-entering HIBERN8, 5 while confirming
 * the restore, and 6 once the link is back in its entry state. uncertain
 * means a UIC operation was submitted but did not conclusively complete, or
 * completed without a known-successful result: it forbids handoff and forbids
 * a successful release. restore_hibern8 records that the entry state was
 * HIBERN8 and that we initiated the exit; it is retained until the re-entry
 * is confirmed. */
struct kshim_ufs_link_state {
    uint32_t initial_tx, current_tx, last_command, command_result;
    uint32_t last_is, power_status, phase;
    uint8_t uncertain, restore_hibern8;
};
/* Device power state for the explicit prepare path. phase is 0 when no power
 * work ran, 1 while querying the entry mode, 2 while the wake START STOP UNIT
 * is active, 3 while confirming the active mode, 4 while querying the current
 * mode before cleanup, 5 while restoring the entry mode, 6 while confirming
 * the restore, and 7 once the entry mode is back. initial_mode/current_mode
 * retain raw bCurrentPowerMode attribute values (0x11/0x22/0x33), not SSU
 * POWER CONDITION values (1/2/3), and are UINT32_MAX if the query failed. uncertain means an SSU
 * was submitted but did not conclusively complete, or completed without a
 * valid command result: it forbids handoff and forbids a successful
 * release. restore_mode is the original SSU condition (2/3), or zero when none is owed; it is
 * retained until the restore is confirmed and then cleared. */
struct kshim_ufs_power_state {
    uint32_t initial_mode, current_mode, phase;
    uint8_t uncertain, restore_mode;
};
struct kshim_ufs_host {
    struct kshim_ufs_ops ops;
    uint8_t *arena;
    uint64_t dma_address;
    uint32_t saved_base[2], saved_run, saved_ie, saved_ahit;
    uint8_t owned, poisoned, last_scsi_status, last_sense_key;
    /* Last actually submitted SCSI command and its autosense. A query never
     * replaces the opcode/LUN. last_sense_valid bit0 means the key is valid,
     * bit1 means the ASC/ASCQ pair is valid. */
    uint8_t last_scsi_opcode, last_scsi_lun, last_sense_asc, last_sense_ascq,
            last_sense_valid;
    struct kshim_ufs_link_state link;
    struct kshim_ufs_power_state power;
};
struct kshim_ufs_lun {
    struct kshim_blockdev block;
    struct kshim_ufs_host *host;
    uint8_t number;
};
int kshim_ufs_claim(struct kshim_ufs_host *, const struct kshim_ufs_ops *,
                    void *arena, size_t arena_bytes, uint64_t dma_address);
int kshim_ufs_nop(struct kshim_ufs_host *);
/* bBootLunEn: 1 selects Boot LU A, 2 selects Boot LU B. */
int kshim_ufs_get_boot_lun(struct kshim_ufs_host *, uint32_t *);
int kshim_ufs_set_boot_lun(struct kshim_ufs_host *, uint32_t);
/* Explicit opt-in device power preparation: call only after a successful
 * claim and NOP, and always pair with release. It never runs from claim or
 * open_lun. ACTIVE returns immediately with no power command; SLEEP and
 * POWERDOWN are woken with START STOP UNIT on the device WLUN and confirmed
 * by a fresh bCurrentPowerMode query. A failed call leaves the restore
 * obligation recorded for release. */
int kshim_ufs_prepare_device(struct kshim_ufs_host *);
int kshim_ufs_read_descriptor(struct kshim_ufs_host *, uint8_t id,
                              uint8_t index, void *out, uint16_t *length);
int kshim_ufs_open_lun(struct kshim_ufs_host *, uint8_t number,
                       bool allow_writes, struct kshim_ufs_lun *);
/* Success proves that our list stopped and all firmware register values were
 * restored. Failure forbids releasing the arena/mapping or payload handoff. */
int kshim_ufs_release(struct kshim_ufs_host *);
