#pragma once

#include <stddef.h>
#include <stdint.h>

#define KSHIM_MMU_MAX_EXTRA_REGIONS 100U

typedef enum {
    KSHIM_MMU_NORMAL_WB = 0,
    KSHIM_MMU_NORMAL_NC,
    KSHIM_MMU_DEVICE,
} kshim_mmu_memory_type_t;

enum {
    KSHIM_MMU_READ = 1U << 0,
    KSHIM_MMU_WRITE = 1U << 1,
    KSHIM_MMU_EXECUTE = 1U << 2,
};

typedef struct {
    uint64_t base;
    uint64_t size;
    kshim_mmu_memory_type_t type;
    uint32_t permissions;
} kshim_mmu_region_t;

typedef struct {
    const void *fdt;
    uint64_t framebuffer_base;
    uint64_t framebuffer_size;
    uint64_t uefi_base;
    uint64_t uefi_size;
    const kshim_mmu_region_t *extra_regions;
    size_t extra_region_count;
    uint8_t map_qemu_test_windows;
} kshim_mmu_config_t;

/* Early configuration, called from kshim_main() before the console, the DT
 * snapshot or any resource probing exists.  It maps, in this order:
 *   1. every MMIO window below the DRAM base (device-nGnRnE, RW),
 *   2. every RAM range the runtime DT declares (write-back, RW),
 *   3. the packed payload window [payload_base, payload_end) (write-back, RW),
 *   4. the running image (write-back, RWX),
 *   5. the runtime DT, if it falls outside every RAM range (write-back, RW).
 * The framebuffer is deliberately not mapped here: start_menu() adds the
 * exact scanout and UI-scratch ranges once the panel is known.  Everything is
 * derived from the runtime load base recorded by startup.S, because the
 * attached shim does not run at its link address. */
typedef struct {
    const void *fdt;
    uint64_t payload_base;
    uint64_t payload_end;
} kshim_mmu_early_config_t;

int kshim_mmu_configure(const kshim_mmu_config_t *config);
int kshim_mmu_configure_default(const void *fdt);
int kshim_mmu_configure_early(const kshim_mmu_early_config_t *config);
/* Map or overwrite a range while the MMU is already running.  The range is
 * recorded so a later kshim_mmu_prepare_handoff() flushes it. */
int kshim_mmu_add_region(uint64_t base, uint64_t size,
                         kshim_mmu_memory_type_t type, uint32_t permissions);
int kshim_mmu_enable_current_cpu(void);
int kshim_mmu_enable_secondary(uint64_t entry_el);
int kshim_mmu_is_enabled(void);
uint64_t kshim_mmu_entry_el(void);
uint64_t kshim_mmu_root_table(void);
int kshim_mmu_prepare_handoff(void);
void kshim_mmu_cpu_off_prepare(void);
/* Runtime load base of the image, or its link address when the build is
 * loaded where it was linked (standalone/QEMU). */
uint64_t kshim_mmu_image_base(void);

/* Compatibility entry used by the existing main path. */
void _arm64_mmu_setup(void);
