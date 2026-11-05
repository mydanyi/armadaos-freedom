#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""
Host-side checks for the DPU LUTDMA layout, version selection and MMIO
sequences.

The C harness includes the production dpu_hw_reg_dma.c and only supplies
small stand-ins for the kernel headers and MMIO accessors. No function body
is copied or rewritten here; a missing production symbol is a hard failure
before the compiler is even invoked.
"""

import os
import shlex
import shutil
import subprocess
import tempfile
import textwrap
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
KERNEL_TREE = Path(os.environ["KERNEL_TREE"])
DPU_DIR = KERNEL_TREE / "drivers" / "gpu" / "drm" / "msm" / "disp" / "dpu1"
SOURCE = DPU_DIR / "dpu_hw_reg_dma.c"
HEADER = DPU_DIR / "dpu_hw_reg_dma.h"

SHIMS = {
    "linux/types.h": """
        #ifndef _SHIM_LINUX_TYPES_H
        #define _SHIM_LINUX_TYPES_H
        #include <stdint.h>
        #include <stdbool.h>
        #include <stddef.h>
        #include <stdlib.h>
        #include <string.h>

        typedef unsigned char u8;
        typedef unsigned short u16;
        typedef unsigned int u32;
        typedef unsigned long long u64;
        typedef int s32;
        typedef unsigned int __le32;
        typedef unsigned int gfp_t;

        #define __iomem
        #define BIT(n) (1U << (n))
        #define ALIGN(x, a) (((x) + (a) - 1) & ~((a) - 1))
        #define lower_32_bits(n) ((u32)(n))
        #define cpu_to_le32(x) (x)
        #define IS_ERR(p) ((uintptr_t)(p) >= (uintptr_t)-4095)
        #define PTR_ERR(p) ((long)(intptr_t)(p))
        #define ERR_PTR(e) ((void *)(intptr_t)(e))
        #define GFP_KERNEL 0

        #define EPERM 1
        #define ENOMEM 12
        #define EBUSY 16
        #define EINVAL 22
        #define ENOSPC 28
        #define ETIMEDOUT 110
        #define EOPNOTSUPP 95
        #endif
    """,
    "linux/io.h": """
        #ifndef _SHIM_LINUX_IO_H
        #define _SHIM_LINUX_IO_H
        #define wmb() do { } while (0)
        #endif
    """,
    "linux/iopoll.h": r"""
        #ifndef _SHIM_LINUX_IOPOLL_H
        #define _SHIM_LINUX_IOPOLL_H
        #include <linux/types.h>

        extern int mock_poll_fail;

        /*
         * One read, then the production condition. The harness flips
         * mock_poll_fail to exercise the timeout paths without waiting.
         */
        #define read_poll_timeout(op, val, cond, delay_us, timeout_us, lazy, hw, off) \
            ({ (val) = op((hw), (off)); \
               mock_poll_fail ? -ETIMEDOUT : ((cond) ? 0 : -ETIMEDOUT); })
        #endif
    """,
    "linux/seq_file.h": """
        #ifndef _SHIM_LINUX_SEQ_FILE_H
        #define _SHIM_LINUX_SEQ_FILE_H
        #include <linux/types.h>

        struct seq_file {
            char *buf;
            size_t len;
            size_t cap;
        };

        int seq_printf(struct seq_file *s, const char *fmt, ...);
        #endif
    """,
    "drm/drm_managed.h": """
        #ifndef _SHIM_DRM_MANAGED_H
        #define _SHIM_DRM_MANAGED_H
        #include <linux/types.h>

        struct drm_device;

        void *drmm_kzalloc(struct drm_device *dev, size_t size, gfp_t flags);
        #endif
    """,
    "dpu_hw_mdss.h": """
        #ifndef _SHIM_DPU_HW_MDSS_H
        #define _SHIM_DPU_HW_MDSS_H
        #include <linux/types.h>

        /* DPU block ids start at one, as in dpu_hw_mdss.h */
        enum dpu_ctl {
            CTL_0 = 1,
            CTL_1,
            CTL_2,
            CTL_3,
            CTL_4,
            CTL_5,
            CTL_MAX
        };

        #define DPU_DBG_MASK_TOP 0x1
        #endif
    """,
    "dpu_hw_catalog.h": """
        #ifndef _SHIM_DPU_HW_CATALOG_H
        #define _SHIM_DPU_HW_CATALOG_H
        #include <linux/types.h>

        struct dpu_reg_dma_cfg {
            u32 version;
            u32 len;
            u32 db_off;
            u32 sb_off;
        };
        #endif
    """,
    "dpu_hw_util.h": """
        #ifndef _SHIM_DPU_HW_UTIL_H
        #define _SHIM_DPU_HW_UTIL_H
        #include <linux/types.h>

        struct dpu_hw_blk_reg_map {
            char *blk_addr;
            u32 log_mask;
        };
        #endif
    """,
    "dpu_hwio.h": r"""
        #ifndef _SHIM_DPU_HWIO_H
        #define _SHIM_DPU_HWIO_H
        #include <linux/types.h>

        u32 mock_reg_read(char *addr);
        void mock_reg_write(char *addr, u32 val);

        #define DPU_REG_READ(hw, offset) mock_reg_read((hw)->blk_addr + (offset))
        #define DPU_REG_WRITE(hw, offset, val) \
            mock_reg_write((hw)->blk_addr + (offset), (val))

        #define DRM_ERROR_RATELIMITED(fmt, ...) do { } while (0)
        #define DRM_ERROR(fmt, ...) do { } while (0)
        #endif
    """,
    "msm_drv.h": """
        #ifndef _SHIM_MSM_DRV_H
        #define _SHIM_MSM_DRV_H
        #endif
    """,
    "msm_gem.h": """
        #ifndef _SHIM_MSM_GEM_H
        #define _SHIM_MSM_GEM_H
        #include <linux/types.h>

        struct drm_device;
        struct drm_gpuvm;
        struct drm_gem_object;

        #define MSM_BO_WC 0

        void *msm_gem_kernel_new(struct drm_device *dev, size_t size, u32 flags,
                                 struct drm_gpuvm *vm,
                                 struct drm_gem_object **bo, u64 *iova);
        void msm_gem_kernel_put(struct drm_gem_object *bo, struct drm_gpuvm *vm);
        #endif
    """,
}

HARNESS = r"""
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MOCK_SPACE_SIZE (2U * 1024U * 1024U)
#define CTL_BASE 0x100000U

static unsigned char mock_space[MOCK_SPACE_SIZE] __attribute__((aligned(16)));
static char *mock_base = (char *)mock_space;
int mock_poll_fail;

struct mock_op {
    int write;
    unsigned int off;
    unsigned int val;
};

static struct mock_op mock_ops[1024];
static int mock_nops;

/* the production translation unit, not a copy of it */
#include "dpu_hw_reg_dma.c"

static unsigned int mock_off(char *addr)
{
    return (unsigned int)(addr - mock_base);
}

u32 mock_reg_read(char *addr)
{
    unsigned int off = mock_off(addr);

    if (off >= MOCK_SPACE_SIZE) {
        printf("read outside mock space: %#x\n", off);
        abort();
    }
    if (mock_nops < (int)(sizeof(mock_ops) / sizeof(mock_ops[0]))) {
        mock_ops[mock_nops].write = 0;
        mock_ops[mock_nops].off = off;
        mock_ops[mock_nops].val = 0;
        mock_nops++;
    }
    return *(u32 *)(mock_space + off);
}

void mock_reg_write(char *addr, u32 val)
{
    unsigned int off = mock_off(addr);

    if (off >= MOCK_SPACE_SIZE) {
        printf("write outside mock space: %#x\n", off);
        abort();
    }
    if (mock_nops < (int)(sizeof(mock_ops) / sizeof(mock_ops[0]))) {
        mock_ops[mock_nops].write = 1;
        mock_ops[mock_nops].off = off;
        mock_ops[mock_nops].val = val;
        mock_nops++;
    }
    *(u32 *)(mock_space + off) = val;
}

static void mock_reset(void)
{
    memset(mock_space, 0, sizeof(mock_space));
    mock_nops = 0;
    mock_poll_fail = 0;
}

static void mock_set(unsigned int off, u32 val)
{
    *(u32 *)(mock_space + off) = val;
}

static int mock_write_seen(unsigned int off, u32 val)
{
    int i;

    for (i = 0; i < mock_nops; i++)
        if (mock_ops[i].write && mock_ops[i].off == off &&
            mock_ops[i].val == val)
            return 1;
    return 0;
}

static int mock_read_seen(unsigned int off)
{
    int i;

    for (i = 0; i < mock_nops; i++)
        if (!mock_ops[i].write && mock_ops[i].off == off)
            return 1;
    return 0;
}

static int mock_any_write(unsigned int lo, unsigned int hi)
{
    int i;

    for (i = 0; i < mock_nops; i++)
        if (mock_ops[i].write && mock_ops[i].off >= lo &&
            mock_ops[i].off < hi)
            return 1;
    return 0;
}

void *drmm_kzalloc(struct drm_device *dev, size_t size, gfp_t flags)
{
    (void)dev;
    (void)flags;
    return calloc(1, size);
}

void *msm_gem_kernel_new(struct drm_device *dev, size_t size, u32 flags,
                         struct drm_gpuvm *vm, struct drm_gem_object **bo,
                         u64 *iova)
{
    void *p;

    (void)dev;
    (void)flags;
    (void)vm;
    p = calloc(1, size);
    if (!p)
        return ERR_PTR(-ENOMEM);
    if (bo)
        *bo = (struct drm_gem_object *)(uintptr_t)1;
    if (iova)
        *iova = 0x100000;
    return p;
}

void msm_gem_kernel_put(struct drm_gem_object *bo, struct drm_gpuvm *vm)
{
    (void)bo;
    (void)vm;
}

int seq_printf(struct seq_file *s, const char *fmt, ...)
{
    va_list ap;
    size_t left;
    int n;

    if (!s || !s->buf || s->len >= s->cap)
        return 0;
    left = s->cap - s->len;
    va_start(ap, fmt);
    n = vsnprintf(s->buf + s->len, left, fmt, ap);
    va_end(ap);
    if (n > 0) {
        if ((size_t)n >= left)
            s->len = s->cap - 1;
        else
            s->len += n;
    }
    return n;
}

#define REQUIRE(c) do { \
    if (!(c)) { \
        printf("FAIL line %d: %s\n", __LINE__, #c); \
        return 1; \
    } \
} while (0)

static void setup_dma(struct dpu_reg_dma *d, struct dpu_reg_dma_cfg *c,
                      const struct dpu_reg_dma_layout *l, u32 ver,
                      u32 len, u32 db, u32 sb)
{
    memset(d, 0, sizeof(*d));
    c->version = ver;
    c->len = len;
    c->db_off = db;
    c->sb_off = sb;
    d->cfg = c;
    d->layout = l;
    d->db.blk_addr = mock_base + db;
    d->sb.blk_addr = mock_base + sb;
}

static void setup_buf(struct dpu_reg_dma_buf *b, u32 *v, u32 size)
{
    memset(b, 0, sizeof(*b));
    b->vaddr = v;
    b->size = size;
    b->iova = 0x1234000;
    b->index = 8;
}

static int test_layout_v2(void)
{
    REQUIRE(dpu_reg_dma_v2.decode_sel == 0x180ac114);
    REQUIRE(dpu_reg_dma_v2.queue0(CTL_0) == 0x14);
    REQUIRE(dpu_reg_dma_v2.queue1(CTL_0) == 0x1c);
    REQUIRE(dpu_reg_dma_v2.queue0(CTL_5) == 0x14 + (CTL_5 - CTL_0) * 0x10);
    REQUIRE(dpu_reg_dma_v2.intr_status(CTL_0) == 0x160);
    REQUIRE(dpu_reg_dma_v2.intr_clear(CTL_0) == 0x1a0);
    REQUIRE(dpu_reg_dma_v2.done_mask(CTL_0) == BIT(21));
    REQUIRE(dpu_reg_dma_v2.done_mask(CTL_3) == BIT(24));
    REQUIRE(dpu_reg_dma_v2.db_done_mask(CTL_0) == BIT(16));
    REQUIRE(dpu_reg_dma_v2.error_status(CTL_5) == 0x170);
    REQUIRE(dpu_reg_dma_v2.error_clear(CTL_5) == 0x1b0);
    REQUIRE(dpu_reg_dma_v2.trigger_off == 0xd4);
    REQUIRE(dpu_reg_dma_v2.trigger_mask(CTL_4) == BIT(0));
    return 0;
}

static int test_layout_v3(void)
{
    REQUIRE(dpu_reg_dma_v3.decode_sel == 0x18180114);
    REQUIRE(dpu_reg_dma_v3.queue0(CTL_0) == 0x1000);
    REQUIRE(dpu_reg_dma_v3.queue1(CTL_0) == 0x1008);
    REQUIRE(dpu_reg_dma_v3.queue0(CTL_5) == 0x6000);
    REQUIRE(dpu_reg_dma_v3.intr_status(CTL_0) == 0x1044);
    REQUIRE(dpu_reg_dma_v3.intr_clear(CTL_0) == 0x1048);
    REQUIRE(dpu_reg_dma_v3.done_mask(CTL_0) == BIT(4));
    REQUIRE(dpu_reg_dma_v3.db_done_mask(CTL_0) == BIT(3));
    REQUIRE(dpu_reg_dma_v3.error_status(CTL_5) == 0x170);
    REQUIRE(dpu_reg_dma_v3.error_clear(CTL_5) == 0x1b0);
    REQUIRE(dpu_reg_dma_v3.trigger_off == 0xd4);
    REQUIRE(dpu_reg_dma_v3.trigger_mask(CTL_5) == BIT(0));
    return 0;
}

static int test_layout_v4(void)
{
    REQUIRE(dpu_reg_dma_v4.decode_sel == 0x18360114);
    REQUIRE(dpu_reg_dma_v4.queue0(CTL_0) == 0x1000);
    REQUIRE(dpu_reg_dma_v4.queue0(CTL_1) == 0x2000);
    REQUIRE(dpu_reg_dma_v4.queue1(CTL_0) == dpu_reg_dma_v4.queue0(CTL_0));
    REQUIRE(dpu_reg_dma_v4.queue1(CTL_5) == dpu_reg_dma_v4.queue0(CTL_5));
    REQUIRE(dpu_reg_dma_v4.intr_status(CTL_0) == 0x1044);
    REQUIRE(dpu_reg_dma_v4.intr_clear(CTL_0) == 0x1048);
    REQUIRE(dpu_reg_dma_v4.done_mask(CTL_0) == BIT(3));
    REQUIRE(dpu_reg_dma_v4.db_done_mask(CTL_0) == BIT(3));
    REQUIRE(dpu_reg_dma_v4.error_status(CTL_0) == 0x1060);
    REQUIRE(dpu_reg_dma_v4.error_clear(CTL_0) == 0x105c);
    REQUIRE(dpu_reg_dma_v4.error_status(CTL_5) == 0x6060);
    REQUIRE(dpu_reg_dma_v4.error_clear(CTL_5) == 0x605c);
    REQUIRE(dpu_reg_dma_v4.trigger_off == 0x900);
    REQUIRE(dpu_reg_dma_v4.trigger_mask(CTL_0) == BIT(0));
    REQUIRE(dpu_reg_dma_v4.trigger_mask(CTL_5) == BIT(5));
    return 0;
}

static int test_ctl_valid_bounds(void)
{
    struct dpu_reg_dma d;
    struct dpu_reg_dma_cfg c;

    setup_dma(&d, &c, &dpu_reg_dma_v4, DPU_REG_DMA_VER_4_0, 0x7000, 0, 0x800);
    REQUIRE(dpu_reg_dma_ctl_valid(&d, CTL_0));
    REQUIRE(dpu_reg_dma_ctl_valid(&d, CTL_5));
    REQUIRE(!dpu_reg_dma_ctl_valid(&d, CTL_0 - 1));
    REQUIRE(!dpu_reg_dma_ctl_valid(&d, CTL_MAX));

    /* sb_off == len maps no SB window, so no subtraction is allowed */
    c.len = 0x800;
    c.sb_off = 0x800;
    REQUIRE(!dpu_reg_dma_ctl_valid(&d, CTL_0));

    /* db_off == len maps no DB window either */
    c.len = 0x1000;
    c.db_off = 0x1000;
    c.sb_off = 0;
    REQUIRE(!dpu_reg_dma_ctl_valid(&d, CTL_0));

    /* one CTL fits the shortened SB window, the next does not */
    c.len = 0x2000;
    c.db_off = 0;
    c.sb_off = 0x800;
    REQUIRE(dpu_reg_dma_ctl_valid(&d, CTL_0));
    REQUIRE(!dpu_reg_dma_ctl_valid(&d, CTL_1));

    setup_dma(&d, &c, &dpu_reg_dma_v3, DPU_REG_DMA_VER_3_0, 0x7000, 0, 0x800);
    REQUIRE(dpu_reg_dma_ctl_valid(&d, CTL_5));

    setup_dma(&d, &c, &dpu_reg_dma_v2, DPU_REG_DMA_VER_2_0, 0x800, 0, 0x400);
    REQUIRE(dpu_reg_dma_ctl_valid(&d, CTL_5));
    return 0;
}

static int test_sb_v3(void)
{
    struct dpu_reg_dma d;
    struct dpu_reg_dma_cfg c;
    struct dpu_reg_dma_buf b;
    u32 cmd[64];

    mock_reset();
    setup_dma(&d, &c, &dpu_reg_dma_v3, DPU_REG_DMA_VER_3_0, 0x7000, 0, 0x800);
    setup_buf(&b, cmd, sizeof(cmd));

    REQUIRE(kick_off(&d, CTL_0, &b, false, false) == 0);
    REQUIRE(mock_write_seen(0x800 + 0x1008, 0x1234000));
    REQUIRE(mock_write_seen(0x800 + 0x100c, CMD_WRITE | 2));
    REQUIRE(!mock_write_seen(0x800 + 0x1048, BIT(4)));
    REQUIRE(!mock_any_write(CTL_BASE, CTL_BASE + 0x2000));

    mock_reset();
    setup_buf(&b, cmd, sizeof(cmd));
    REQUIRE(kick_off(&d, CTL_0, &b, false, true) == 0);
    REQUIRE(mock_write_seen(0x800 + 0x100c, CMD_WRITE | CMD_LAST | 2));
    REQUIRE(mock_write_seen(0x800 + 0x1048, BIT(4)));
    REQUIRE(!mock_any_write(CTL_BASE, CTL_BASE + 0x2000));
    return 0;
}

static int test_db_v3(void)
{
    struct dpu_reg_dma d;
    struct dpu_reg_dma_cfg c;
    struct dpu_reg_dma_buf b;
    u32 cmd[64];

    mock_reset();
    setup_dma(&d, &c, &dpu_reg_dma_v3, DPU_REG_DMA_VER_3_0, 0x7000, 0, 0x800);
    setup_buf(&b, cmd, sizeof(cmd));

    REQUIRE(kick_off(&d, CTL_0, &b, true, true) == 0);
    REQUIRE(mock_write_seen(0x1000, 0x1234000));
    REQUIRE(mock_write_seen(0x1004, CMD_WRITE | CMD_LAST | 2));
    REQUIRE(mock_write_seen(0x1048, BIT(3)));
    REQUIRE(!mock_write_seen(0x1048, BIT(4)));
    REQUIRE(!mock_any_write(CTL_BASE, CTL_BASE + 0x2000));
    return 0;
}

static int test_sb_v4(void)
{
    struct dpu_reg_dma d;
    struct dpu_reg_dma_cfg c;
    struct dpu_reg_dma_buf b;
    u32 cmd[64];

    mock_reset();
    setup_dma(&d, &c, &dpu_reg_dma_v4, DPU_REG_DMA_VER_4_0, 0x7000, 0, 0x800);
    setup_buf(&b, cmd, sizeof(cmd));

    REQUIRE(kick_off(&d, CTL_0, &b, false, false) == 0);
    REQUIRE(mock_write_seen(0x800 + 0x1000, 0x1234000));
    REQUIRE(mock_write_seen(0x800 + 0x1004, CMD_WRITE | 2));
    REQUIRE(!mock_write_seen(0x800 + 0x1048, BIT(3)));

    mock_reset();
    setup_buf(&b, cmd, sizeof(cmd));
    REQUIRE(kick_off(&d, CTL_0, &b, false, true) == 0);
    REQUIRE(mock_write_seen(0x800 + 0x1004, CMD_WRITE | CMD_LAST | 2));
    REQUIRE(mock_write_seen(0x800 + 0x1048, BIT(3)));
    REQUIRE(!mock_any_write(CTL_BASE, CTL_BASE + 0x2000));
    return 0;
}

static int test_db_v4(void)
{
    struct dpu_reg_dma d;
    struct dpu_reg_dma_cfg c;
    struct dpu_reg_dma_buf b;
    u32 cmd[64];

    mock_reset();
    setup_dma(&d, &c, &dpu_reg_dma_v4, DPU_REG_DMA_VER_4_0, 0x7000, 0, 0x800);
    setup_buf(&b, cmd, sizeof(cmd));

    REQUIRE(kick_off(&d, CTL_0, &b, true, true) == 0);
    REQUIRE(mock_write_seen(0x1000, 0x1234000));
    REQUIRE(mock_write_seen(0x1004, CMD_WRITE | CMD_LAST | 2));
    REQUIRE(mock_write_seen(0x1048, BIT(3)));
    REQUIRE(!mock_any_write(CTL_BASE, CTL_BASE + 0x2000));
    return 0;
}

static int test_error_legacy(void)
{
    struct dpu_reg_dma d;
    struct dpu_reg_dma_cfg c;
    struct dpu_reg_dma_buf b;
    u32 cmd[64];

    mock_reset();
    setup_dma(&d, &c, &dpu_reg_dma_v3, DPU_REG_DMA_VER_3_0, 0x7000, 0, 0x800);
    setup_buf(&b, cmd, sizeof(cmd));
    mock_set(0x800 + 0x170, 0x5);

    REQUIRE(kick_off(&d, CTL_0, &b, false, false) == 0);
    REQUIRE(mock_read_seen(0x800 + 0x170));
    REQUIRE(mock_write_seen(0x800 + 0x1b0, REG_DMA_ERROR_CLEAR_MASK));
    REQUIRE(!mock_read_seen(0x800 + 0x60));
    REQUIRE(!mock_write_seen(0x800 + 0x5c, REG_DMA_ERROR_CLEAR_MASK));
    return 0;
}

static int test_error_v4(void)
{
    struct dpu_reg_dma d;
    struct dpu_reg_dma_cfg c;
    struct dpu_reg_dma_buf b;
    u32 cmd[64];

    mock_reset();
    setup_dma(&d, &c, &dpu_reg_dma_v4, DPU_REG_DMA_VER_4_0, 0x7000, 0, 0x800);
    setup_buf(&b, cmd, sizeof(cmd));
    mock_set(0x1060, 0x5);

    REQUIRE(kick_off(&d, CTL_0, &b, true, false) == 0);
    REQUIRE(mock_read_seen(0x1060));
    REQUIRE(mock_write_seen(0x105c, REG_DMA_ERROR_CLEAR_MASK));
    REQUIRE(!mock_read_seen(0x170));
    REQUIRE(!mock_write_seen(0x1b0, REG_DMA_ERROR_CLEAR_MASK));

    mock_reset();
    setup_buf(&b, cmd, sizeof(cmd));
    mock_set(0x800 + 0x1060, 0x7);
    REQUIRE(kick_off(&d, CTL_0, &b, false, false) == 0);
    REQUIRE(mock_read_seen(0x800 + 0x1060));
    REQUIRE(mock_write_seen(0x800 + 0x105c, REG_DMA_ERROR_CLEAR_MASK));
    REQUIRE(!mock_read_seen(0x170));
    return 0;
}

static int test_flush_db_v4(void)
{
    struct dpu_reg_dma d;
    struct dpu_reg_dma_cfg c;
    struct dpu_hw_blk_reg_map ctl_hw;
    u32 cmd[64];

    mock_reset();
    setup_dma(&d, &c, &dpu_reg_dma_v4, DPU_REG_DMA_VER_4_0, 0x7000, 0, 0x800);
    ctl_hw.blk_addr = mock_base + CTL_BASE;

    setup_buf(&d.last_cmd_db[CTL_0], cmd, sizeof(cmd));
    mock_set(0x1044, BIT(3));
    REQUIRE(dpu_reg_dma_flush_db(&d, CTL_0, &ctl_hw) == 0);
    REQUIRE(cmd[0] == dpu_reg_dma_v4.decode_sel);
    REQUIRE(cmd[1] == 0);
    REQUIRE(mock_write_seen(0x1000, 0x1234000));
    REQUIRE(mock_write_seen(0x1004, CMD_WRITE | CMD_LAST | 2));
    REQUIRE(mock_write_seen(CTL_BASE + 0x900, BIT(0)));
    REQUIRE(!mock_any_write(CTL_BASE + 0xd4, CTL_BASE + 0xd8));

    setup_buf(&d.last_cmd_db[CTL_1], cmd, sizeof(cmd));
    mock_set(0x2044, BIT(3));
    REQUIRE(dpu_reg_dma_flush_db(&d, CTL_1, &ctl_hw) == 0);
    REQUIRE(mock_write_seen(CTL_BASE + 0x900, BIT(1)));
    REQUIRE(!mock_any_write(CTL_BASE + 0xd4, CTL_BASE + 0xd8));
    return 0;
}

static int test_flush_db_legacy(void)
{
    struct dpu_reg_dma d;
    struct dpu_reg_dma_cfg c;
    struct dpu_hw_blk_reg_map ctl_hw;
    u32 cmd[64];

    mock_reset();
    setup_dma(&d, &c, &dpu_reg_dma_v3, DPU_REG_DMA_VER_3_0, 0x7000, 0, 0x800);
    ctl_hw.blk_addr = mock_base + CTL_BASE;
    setup_buf(&d.last_cmd_db[CTL_0], cmd, sizeof(cmd));
    mock_set(0x1044, BIT(3));
    REQUIRE(dpu_reg_dma_flush_db(&d, CTL_0, &ctl_hw) == 0);
    REQUIRE(cmd[0] == dpu_reg_dma_v3.decode_sel);
    REQUIRE(mock_write_seen(CTL_BASE + 0xd4, BIT(0)));
    REQUIRE(!mock_any_write(CTL_BASE + 0x900, CTL_BASE + 0x904));

    mock_reset();
    setup_dma(&d, &c, &dpu_reg_dma_v2, DPU_REG_DMA_VER_2_0, 0x800, 0, 0x400);
    ctl_hw.blk_addr = mock_base + CTL_BASE;
    setup_buf(&d.last_cmd_db[CTL_0], cmd, sizeof(cmd));
    mock_set(0x160, BIT(16));
    REQUIRE(dpu_reg_dma_flush_db(&d, CTL_0, &ctl_hw) == 0);
    REQUIRE(mock_write_seen(CTL_BASE + 0xd4, BIT(0)));
    REQUIRE(!mock_any_write(CTL_BASE + 0x900, CTL_BASE + 0x904));
    return 0;
}

static int test_last_cmd_sb_no_trigger(void)
{
    struct dpu_reg_dma d;
    struct dpu_reg_dma_cfg c;
    u32 cmd[64];

    mock_reset();
    setup_dma(&d, &c, &dpu_reg_dma_v3, DPU_REG_DMA_VER_3_0, 0x7000, 0, 0x800);
    setup_buf(&d.last_cmd[CTL_0], cmd, sizeof(cmd));
    d.last_cmd[CTL_0].ctl_idx = 0;

    REQUIRE(dpu_reg_dma_last_cmd_sb(&d, CTL_0) == 0);
    REQUIRE(mock_write_seen(0x800 + 0x1008, 0x1234000));
    REQUIRE(mock_write_seen(0x800 + 0x100c, CMD_WRITE | CMD_LAST | 2));
    REQUIRE(mock_write_seen(0x800 + 0x1048, BIT(4)));
    REQUIRE(!mock_any_write(CTL_BASE, CTL_BASE + 0x2000));
    return 0;
}

static int test_buf_wait_timeout(void)
{
    struct dpu_reg_dma d;
    struct dpu_reg_dma_cfg c;
    struct dpu_reg_dma_buf b;
    u32 cmd[64];

    mock_reset();
    setup_dma(&d, &c, &dpu_reg_dma_v3, DPU_REG_DMA_VER_3_0, 0x7000, 0, 0x800);
    setup_buf(&b, cmd, sizeof(cmd));
    b.ctl_idx = CTL_1;

    mock_poll_fail = 1;
    REQUIRE(dpu_reg_dma_buf_wait(&d, &b) == -ETIMEDOUT);
    REQUIRE(b.ctl_idx == CTL_1);
    REQUIRE(b.iova == 0x1234000);

    mock_poll_fail = 0;
    mock_set(0x800 + 0x2044, BIT(4));
    REQUIRE(dpu_reg_dma_buf_wait(&d, &b) == 0);
    REQUIRE(b.ctl_idx == 0);
    return 0;
}

static int test_flush_timeout_ownership(void)
{
    struct dpu_reg_dma d;
    struct dpu_reg_dma_cfg c;
    struct dpu_hw_blk_reg_map ctl_hw;
    u32 cmd[64];

    mock_reset();
    setup_dma(&d, &c, &dpu_reg_dma_v3, DPU_REG_DMA_VER_3_0, 0x7000, 0, 0x800);
    ctl_hw.blk_addr = mock_base + CTL_BASE;
    setup_buf(&d.last_cmd_db[CTL_0], cmd, sizeof(cmd));

    mock_poll_fail = 1;
    REQUIRE(dpu_reg_dma_flush_db(&d, CTL_0, &ctl_hw) == -ETIMEDOUT);
    REQUIRE(d.db_stuck);
    REQUIRE(d.last_cmd_db[CTL_0].vaddr == cmd);
    REQUIRE(d.last_cmd_db[CTL_0].iova == 0x1234000);
    REQUIRE(dpu_reg_dma_flush_db(&d, CTL_0, &ctl_hw) == -EBUSY);
    return 0;
}

static int test_init_versions(void)
{
    struct dpu_reg_dma_cfg c;
    struct dpu_reg_dma *d;
    struct drm_device *dev = NULL;
    struct drm_gpuvm *vm = NULL;

    mock_reset();
    memset(&c, 0, sizeof(c));
    c.version = DPU_REG_DMA_VER_4_0;
    c.len = 0x7000;
    c.db_off = 0;
    c.sb_off = 0x800;
    d = dpu_reg_dma_init(dev, vm, mock_space, &c);
    REQUIRE(d && !IS_ERR(d));
    REQUIRE(d->layout == &dpu_reg_dma_v4);
    dpu_reg_dma_destroy(d);

    c.version = DPU_REG_DMA_VER_3_0;
    d = dpu_reg_dma_init(dev, vm, mock_space, &c);
    REQUIRE(d && !IS_ERR(d));
    REQUIRE(d->layout == &dpu_reg_dma_v3);
    dpu_reg_dma_destroy(d);

    c.version = DPU_REG_DMA_VER_2_0;
    c.len = 0x800;
    c.sb_off = 0x400;
    d = dpu_reg_dma_init(dev, vm, mock_space, &c);
    REQUIRE(d && !IS_ERR(d));
    REQUIRE(d->layout == &dpu_reg_dma_v2);
    dpu_reg_dma_destroy(d);

    c.version = 0x50000;
    d = dpu_reg_dma_init(dev, vm, mock_space, &c);
    REQUIRE(IS_ERR(d));
    REQUIRE(PTR_ERR(d) == -EOPNOTSUPP);
    return 0;
}

static int test_debugfs_v4(void)
{
    struct dpu_reg_dma d;
    struct dpu_reg_dma_cfg c;
    struct seq_file s;
    char out[8192];
    int i;

    mock_reset();
    setup_dma(&d, &c, &dpu_reg_dma_v4, DPU_REG_DMA_VER_4_0, 0x7000, 0, 0x800);
    for (i = CTL_0; i < CTL_MAX; i++)
        d.last_cmd[i].iova = 0x100000 + i;
    mock_set(0x800 + 0x1060, 0x5);
    mock_set(0x800 + 0x2060, 0x6);

    memset(out, 0, sizeof(out));
    s.buf = out;
    s.len = 0;
    s.cap = sizeof(out);
    dpu_reg_dma_debugfs_show(&d, &s);

    REQUIRE(strstr(out, "ctl0"));
    REQUIRE(strstr(out, "ctl1"));
    REQUIRE(strstr(out, "err_status 0x00000005"));
    REQUIRE(strstr(out, "err_status 0x00000006"));
    REQUIRE(!strstr(out, "intr4_status"));
    REQUIRE(mock_read_seen(0x800 + 0x1060));
    REQUIRE(mock_read_seen(0x800 + 0x2060));
    REQUIRE(!mock_read_seen(0x170));
    REQUIRE(!mock_read_seen(0x1b0));
    return 0;
}

struct host_test {
    const char *name;
    int (*fn)(void);
};

static const struct host_test host_tests[] = {
    { "layout_v2", test_layout_v2 },
    { "layout_v3", test_layout_v3 },
    { "layout_v4", test_layout_v4 },
    { "ctl_valid_bounds", test_ctl_valid_bounds },
    { "sb_v3", test_sb_v3 },
    { "db_v3", test_db_v3 },
    { "sb_v4", test_sb_v4 },
    { "db_v4", test_db_v4 },
    { "error_legacy", test_error_legacy },
    { "error_v4", test_error_v4 },
    { "flush_db_v4", test_flush_db_v4 },
    { "flush_db_legacy", test_flush_db_legacy },
    { "last_cmd_sb_no_trigger", test_last_cmd_sb_no_trigger },
    { "buf_wait_timeout", test_buf_wait_timeout },
    { "flush_timeout_ownership", test_flush_timeout_ownership },
    { "init_versions", test_init_versions },
    { "debugfs_v4", test_debugfs_v4 },
};

int main(int argc, char **argv)
{
    size_t i;

    if (argc < 2) {
        for (i = 0; i < sizeof(host_tests) / sizeof(host_tests[0]); i++)
            printf("%s\n", host_tests[i].name);
        return 0;
    }

    for (i = 0; i < sizeof(host_tests) / sizeof(host_tests[0]); i++) {
        if (!strcmp(argv[1], host_tests[i].name)) {
            int rc = host_tests[i].fn();

            printf("%s: %s\n", host_tests[i].name, rc ? "FAIL" : "ok");
            return rc;
        }
    }

    fprintf(stderr, "unknown test: %s\n", argv[1]);
    return 2;
}
"""

TEST_NAMES = [
    "layout_v2",
    "layout_v3",
    "layout_v4",
    "ctl_valid_bounds",
    "sb_v3",
    "db_v3",
    "sb_v4",
    "db_v4",
    "error_legacy",
    "error_v4",
    "flush_db_v4",
    "flush_db_legacy",
    "last_cmd_sb_no_trigger",
    "buf_wait_timeout",
    "flush_timeout_ownership",
    "init_versions",
    "debugfs_v4",
]


class RegDmaV4Tests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not SOURCE.is_file() or not HEADER.is_file():
            raise AssertionError(
                f"production source missing: {SOURCE} / {HEADER}")

        header = HEADER.read_text(encoding="utf-8")
        source = SOURCE.read_text(encoding="utf-8")
        if "DPU_REG_DMA_VER_4_0" not in header:
            raise AssertionError(
                "dpu_hw_reg_dma.h has no DPU_REG_DMA_VER_4_0")
        if "dpu_reg_dma_v4" not in source:
            raise AssertionError(
                "dpu_hw_reg_dma.c has no v4 layout")

        cls.tmp = tempfile.TemporaryDirectory(prefix="regdma-v4-")
        cls.addClassCleanup(cls.tmp.cleanup)
        root = Path(cls.tmp.name)
        prod_copy = root / "production"
        prod_copy.mkdir(parents=True, exist_ok=True)
        (prod_copy / "dpu_hw_reg_dma.c").write_text(source, encoding="utf-8")
        (prod_copy / "dpu_hw_reg_dma.h").write_text(header, encoding="utf-8")
        shim = root / "shim"
        for name, body in SHIMS.items():
            path = shim / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(textwrap.dedent(body).lstrip(), encoding="utf-8")

        harness = root / "harness.c"
        harness.write_text(HARNESS.lstrip(), encoding="utf-8")

        cc = shlex.split(os.environ.get("CC", "cc"))
        if not cc or not shutil.which(cc[0]):
            raise AssertionError(f"C compiler not found: {cc}")
        cls.exe = root / "harness"
        cmd = cc + [
            "-std=gnu11", "-O1", "-g", "-Wall", "-Wextra",
            "-Wno-unused-parameter", "-Wno-pointer-arith",
            "-I", str(shim), "-I", str(prod_copy),
            str(harness), "-o", str(cls.exe),
        ]
        proc = subprocess.run(cmd, capture_output=True, text=True)
        if proc.returncode:
            raise AssertionError(
                "harness build failed\n" + proc.stdout + proc.stderr)

        listed = subprocess.run([str(cls.exe)], capture_output=True, text=True)
        if listed.returncode:
            raise AssertionError("harness enumeration failed\n" + listed.stderr)
        missing = set(TEST_NAMES) - set(listed.stdout.split())
        if missing:
            raise AssertionError(f"harness is missing tests: {sorted(missing)}")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def _run(self, name):
        proc = subprocess.run([str(self.exe), name], capture_output=True,
                              text=True)
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)

    def test_layout_v2(self):
        self._run("layout_v2")

    def test_layout_v3(self):
        self._run("layout_v3")

    def test_layout_v4(self):
        self._run("layout_v4")

    def test_ctl_valid_bounds(self):
        self._run("ctl_valid_bounds")

    def test_sb_v3(self):
        self._run("sb_v3")

    def test_db_v3(self):
        self._run("db_v3")

    def test_sb_v4(self):
        self._run("sb_v4")

    def test_db_v4(self):
        self._run("db_v4")

    def test_error_legacy(self):
        self._run("error_legacy")

    def test_error_v4(self):
        self._run("error_v4")

    def test_flush_db_v4(self):
        self._run("flush_db_v4")

    def test_flush_db_legacy(self):
        self._run("flush_db_legacy")

    def test_last_cmd_sb_no_trigger(self):
        self._run("last_cmd_sb_no_trigger")

    def test_buf_wait_timeout(self):
        self._run("buf_wait_timeout")

    def test_flush_timeout_ownership(self):
        self._run("flush_timeout_ownership")

    def test_init_versions(self):
        self._run("init_versions")

    def test_debugfs_v4(self):
        self._run("debugfs_v4")


if __name__ == "__main__":
    unittest.main(verbosity=2)
