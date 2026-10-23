/* SPDX-License-Identifier: MIT */
#include <qcom_dt.h>
#include <Library/cr_splash.h>
#include <string.h>
#if KSHIM_DIAGNOSTIC_RESOURCES_ONLY
#include <stdio.h>
static struct kshim_splash_trace g_kshim_splash_trace;
const struct kshim_splash_trace *kshim_splash_trace_get(void)
{
    return &g_kshim_splash_trace;
}
static struct kshim_splash_snapshot g_kshim_splash_snapshot;

const struct kshim_splash_snapshot *kshim_splash_snapshot_get(void)
{
    return &g_kshim_splash_snapshot;
}

void kshim_splash_snapshot_capture(const struct CrSplashResources *r, const struct CrIo *io)
{
    memset(&g_kshim_splash_snapshot, 0, sizeof(g_kshim_splash_snapshot));
    if (!r || !io || !io->read32) return;
    if (!r->lm_source_select) return;
    if (r->ctl_count != 6 || r->mixer_count != 8 ||
        r->pipe_count != 10 || r->stage_count != 11) return;
    if (r->mixer_id[0] != 0 || r->mixer_id[1] != 1) return;
    if (r->pipe_id[0] != 0 || r->pipe_id[1] != 1) return;
    uint64_t base = r->base;
    if (base == 0) return;
    if ((uintptr_t)base != base) return;
    uint64_t size = r->size;
    if (size > UINTPTR_MAX ||
        base > UINTPTR_MAX - size ||
        (base & (uintptr_t)3u) != 0u) {
        return;
    }

    uint64_t tmp[63];
    unsigned n = 0;
    {
        uint64_t c = r->ctl[0];
        tmp[n++] = c + 0xf4u;
        tmp[n++] = c + 0x12cu;
        tmp[n++] = c + 0x130u;
    }
    for (unsigned i = 0; i < 2; ++i) {
        uint64_t lm = r->mixer[i];
        tmp[n++] = lm + 0x00u;
        tmp[n++] = lm + 0x04u;
        tmp[n++] = lm + 0x14u;
        for (unsigned s = 0; s < 11; ++s)
            tmp[n++] = lm + (uint64_t)r->stage[s] + 4u;
    }
    for (unsigned i = 0; i < 2; ++i) {
        uint64_t p = (uint64_t)r->pipe[i] + 0x1000u;
        static const uint32_t rect0_offs[16] = {
            0x0u, 0x4u, 0x8u, 0x10u, 0x14u, 0x18u, 0x1cu, 0x20u,
            0x30u, 0x34u, 0x38u, 0x3cu, 0x4cu, 0x5cu, 0x64u, 0x74u
        };
        for (unsigned k = 0; k < 16; ++k) tmp[n++] = p + rect0_offs[k];
    }
    if (n != 63) return;

    uint32_t offs[63];
    for (unsigned i = 0; i < 63; ++i) {
        uint64_t v = tmp[i];
        uint64_t addr = base + v;
        if (addr < base) return;
        uint64_t rel = addr - base;
        if ((rel & 0x3u)) return;
        if (rel > 0xffffffffu) return;
        if (rel + 4u > size) return;
        if ((uintptr_t)(base + rel) != base + rel) return;
        offs[i] = (uint32_t)rel;
    }

    for (unsigned i = 0; i < 3; ++i) {
        uint32_t w = io->read32(io->context, (uintptr_t)(base + (uint64_t)offs[i]));
        g_kshim_splash_snapshot.words[i] = w;
        uint32_t want = (i == 0u) ? 6u : (i == 1u) ? 0x30000u : 3u;
        if (w != want) return;
    }
    for (unsigned i = 3; i < 63; ++i)
        g_kshim_splash_snapshot.words[i] =
            io->read32(io->context, (uintptr_t)(base + (uint64_t)offs[i]));
    g_kshim_splash_snapshot.valid = true;

    for (unsigned i = 0; i < 3; ++i) {
        uint32_t w = io->read32(io->context, (uintptr_t)(base + (uint64_t)offs[i]));
        uint32_t want = (i == 0u) ? 6u : (i == 1u) ? 0x30000u : 3u;
        if (w != want) {
            g_kshim_splash_snapshot.stable = false;
            return;
        }
    }
    bool stable = true;
    for (unsigned i = 3; i < 63; ++i) {
        uint32_t w = io->read32(io->context, (uintptr_t)(base + (uint64_t)offs[i]));
        if (w != g_kshim_splash_snapshot.words[i]) stable = false;
    }
    g_kshim_splash_snapshot.stable = stable;
}

int kshim_splash_snapshot_report(const struct kshim_splash_snapshot *s,
                                 unsigned slot, char *out, size_t capacity)
{
    if (!s || !out || capacity < 81) return -1;
    if (!s->valid) return -1;
    if (slot > 7) return -1;
    int n = snprintf(out, capacity, "r18:%u:%u:", slot, s->stable ? 1u : 0u);
    if (n != 8) return -1;
    for (unsigned i = 0; i < 8; ++i) {
        unsigned idx = slot * 8u + i;
        uint32_t w = idx < 63u ? s->words[idx] : 0u;
        int m = snprintf(out + n, capacity - (size_t)n, "%08x", (unsigned)w);
        if (m != 8) return -1;
        n += m;
    }
    if ((size_t)n != 72) return -1;
    return n;
}

struct kshim_splash_trace_ctx {
    const struct CrIo *io;
    uintptr_t base;
};
static uint32_t trace_read32(void *context, uintptr_t address)
{
    struct kshim_splash_trace_ctx *ctx = context;
    struct kshim_splash_trace *t = &g_kshim_splash_trace;
    uint32_t value = ctx->io->read32(ctx->io->context, address);
    if (t->count < 15) t->first[t->count] = value;
    t->values[t->count % 5] = value;
    t->last_offset = (uint32_t)(address - ctx->base);
    ++t->count;
    return value;
}
int kshim_splash_trace_report(const struct kshim_splash_trace *t, int status, char *out, size_t capacity)
{
    if (!t || !out || capacity < 81) return -1;
    int n;
    if (status == -1 && t->count == 0 && t->dt_stage != 0) {
        /* A DT failure before any MMIO read: report the last stage reached,
           the root compatible string-list count (signed; negative marks a
           malformed list) and whether an exact "qcom,sunp" entry was seen. */
        n = snprintf(out, capacity, "r14:dt=%u:cc=%d:sun=%u",
                     (unsigned)t->dt_stage, t->root_compatible_count,
                     t->root_has_sunp ? 1u : 0u);
    } else if (status == -95 && t->count == 15 &&
               (t->last_offset == 0x104224u || t->last_offset == 0x45224u)) {
        int wide = -1;
        for (int i = 3; i < 15; ++i) {
            if (t->first[i] >> 16) {
                wide = i;
                break;
            }
        }
        if (wide >= 0) {
            n = snprintf(out, capacity, "r14:wide:i=%u:v=%x",
                         (unsigned)wide, (unsigned)t->first[wide]);
        } else {
            n = snprintf(out, capacity,
                "r14:%08x%08x%08x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x",
                (unsigned)t->first[0], (unsigned)t->first[1], (unsigned)t->first[2],
                (unsigned)t->first[3], (unsigned)t->first[4], (unsigned)t->first[5],
                (unsigned)t->first[6], (unsigned)t->first[7], (unsigned)t->first[8],
                (unsigned)t->first[9], (unsigned)t->first[10], (unsigned)t->first[11],
                (unsigned)t->first[12], (unsigned)t->first[13], (unsigned)t->first[14]);
        }
    } else {
        n = snprintf(out, capacity, "r14:s=%d:n=%u:o=%x:v=%x,%x,%x,%x,%x",
                     status, (unsigned)t->count, (unsigned)t->last_offset,
                     (unsigned)t->values[(t->count + 0) % 5],
                     (unsigned)t->values[(t->count + 1) % 5],
                     (unsigned)t->values[(t->count + 2) % 5],
                     (unsigned)t->values[(t->count + 3) % 5],
                     (unsigned)t->values[(t->count + 4) % 5]);
    }
    if (n >= 0 && (size_t)n < capacity && n <= 80) return n;
    return -1;
}
#endif
static int offsets(const void *f, int n, const char *name, uint32_t *out, unsigned capacity)
{
    int len;
    const fdt32_t *p = fdt_getprop(f, n, name, &len);
    if (!p || len <= 0 || len % 4 || (unsigned)len / 4 > capacity) return -1;
    for (int i = 0; i < len / 4; ++i) out[i] = fdt32_to_cpu(p[i]);
    return len / 4;
}
/* The single available cont_splash_region reservation, or -1. */
int kshim_dt_splash_reservation(const void *f, struct dt_range *memory, bool *no_map)
{
    int reserved = fdt_path_offset(f, "/reserved-memory"), node, found = -1;
    if (!memory || reserved < 0 || !dt_available(f, reserved)) return -1;
    fdt_for_each_subnode(node, f, reserved) {
        const char *label = dt_string(f, node, "label");
        const char *name = fdt_get_name(f, node, NULL);
        if (!dt_available(f, node) ||
            !((label && !strcmp(label, "cont_splash_region")) ||
              (name && !strncmp(name, "cont_splash_region@", 19)))) continue;
        if (found >= 0 || dt_reg(f, node, 0, memory)) return -1;
        found = node;
    }
    if (found < 0) return -1;
    if (no_map) *no_map = fdt_getprop(f, found, "no-map", NULL) != NULL;
    return 0;
}
/* TB710FU / SM8650: ABL scans one linear framebuffer out through several
 * VIG/DMA pipes side by side (measured: 4 x VIG 800x2000 from 0xd5100000,
 * stride 12800, ARGB-family 0x237ff), one LM per pipe.  Take the SSPPs that
 * an active CTL stages on any of its mixers and require that they tile one
 * contiguous WxH buffer: same base/stride/format/unpack/height, x offsets
 * 0..W without gaps, buffer inside cont_splash_region.  Pipes left
 * programmed but not staged are ignored.  Routing and pipes are read twice;
 * any change -> CR_BUS_AGAIN.  Read-only. */
struct tb710fu_pipe { uint32_t src, xy, out_xy, addr, stride, format, unpack, op; };
static int tb710fu_read_pipes(const struct CrSplashResources *r, const struct CrIo *io,
                              struct tb710fu_pipe *pp)
{
    for (unsigned p = 0; p < r->pipe_count; ++p) {
        uint64_t b = r->base + r->pipe[p];
        pp[p] = (struct tb710fu_pipe){
            .src = io->read32(io->context, (uintptr_t)(b + 0x00)),
            .xy = io->read32(io->context, (uintptr_t)(b + 0x08)),
            .out_xy = io->read32(io->context, (uintptr_t)(b + 0x10)),
            .addr = io->read32(io->context, (uintptr_t)(b + 0x14)),
            .stride = io->read32(io->context, (uintptr_t)(b + 0x24)),
            .format = io->read32(io->context, (uintptr_t)(b + 0x30)),
            .unpack = io->read32(io->context, (uintptr_t)(b + 0x34)),
            .op = io->read32(io->context, (uintptr_t)(b + 0x38))};
    }
    return 0;
}
/* Pipe ids (r->pipe_id numbering) staged on an active CTL, legacy CTL layer
 * routing as in active_legacy(): VIG0-3 and DMA0-1 in CTL_LAYER(m) plus the
 * CTL_LAYER_EXT overflow bits, DMA2-5 in CTL_LAYER_EXT2.  Multirect
 * (CTL_LAYER_EXT3) is not decoded and is refused. */
static int tb710fu_staged_pipes(const struct CrSplashResources *r, const struct CrIo *io,
                                unsigned *ids)
{
    static const unsigned shift[] = {0, 3, 6, 26, 18, 21};
    static const unsigned ext_shift[] = {0, 2, 4, 6, 16, 18};
    *ids = 0;
    for (unsigned c = 0; c < r->ctl_count; ++c) {
        uint64_t b = r->base + r->ctl[c];
        if (!io->read32(io->context, (uintptr_t)(b + 0xf4)) &&
            !(io->read32(io->context, (uintptr_t)(b + 0x14)) & 15)) continue;
        for (unsigned m = 0; m < r->mixer_count; ++m) {
            uint32_t layer = io->read32(io->context, (uintptr_t)(b + (m == 5 ? 0x24 : m * 4)));
            uint32_t ext = io->read32(io->context, (uintptr_t)(b + 0x40 + m * 4));
            uint32_t ext2 = io->read32(io->context, (uintptr_t)(b + 0x70 + m * 4));
            uint32_t ext3 = io->read32(io->context, (uintptr_t)(b + 0xa0 + m * 4));
            if (ext3) return CR_BUS_UNSUPPORTED;
            for (unsigned id = 0; id < 10; ++id) {
                bool used = id < 6 ? ((layer >> shift[id]) & 7) || ((ext >> ext_shift[id]) & 1)
                                   : ((ext2 >> ((id - 6) * 4)) & 15);
                if (used) *ids |= 1u << id;
            }
        }
    }
    return 0;
}
static int tb710fu_splash_read(const struct CrSplashResources *r, const struct CrIo *io,
                               struct CrSplash *fb)
{
    struct tb710fu_pipe a[10], b[10];
    if (!io || !io->read32 || r->pipe_count == 0 || r->pipe_count > 10) return CR_BUS_INVALID;
    for (unsigned p = 0; p < r->pipe_count; ++p)
        if ((r->pipe[p] & 3) || (uint64_t)r->pipe[p] + 0x3c > r->size) return CR_BUS_INVALID;
    if (r->ctl_count == 0 || r->mixer_count == 0 || r->mixer_count > 6) return CR_BUS_INVALID;
    for (unsigned c = 0; c < r->ctl_count; ++c)
        if ((r->ctl[c] & 3) || (uint64_t)r->ctl[c] + 0xf8 > r->size) return CR_BUS_INVALID;
    for (unsigned p = 0; p < r->pipe_count; ++p)
        if (r->pipe_id[p] >= 10) return CR_BUS_INVALID;
    unsigned ids, ids_again;
    int rs = tb710fu_staged_pipes(r, io, &ids);
    if (rs) return rs;
    if (!ids) return CR_BUS_UNSUPPORTED;
    tb710fu_read_pipes(r, io, a);
    int first = -1; unsigned used = 0, width = 0, h = 0, bpp = 4;
    unsigned nseg = 0; uint32_t seg_x[10], seg_w[10];
    for (unsigned p = 0; p < r->pipe_count; ++p) {
        const struct tb710fu_pipe *s = &a[p];
        if (!(ids & (1u << r->pipe_id[p]))) continue;
        if (!s->src || !s->addr) return CR_BUS_UNSUPPORTED;
        if (first < 0) first = (int)p;
        const struct tb710fu_pipe *f0 = &a[first];
        if (s->addr != f0->addr || s->stride != f0->stride || s->format != f0->format ||
            s->unpack != f0->unpack || (s->src >> 16) != (f0->src >> 16) ||
            (s->xy >> 16) || (s->op & ~((1u << 31) | (1u << 4)))) return CR_BUS_UNSUPPORTED;
        seg_x[nseg] = s->xy & 0xffff; seg_w[nseg] = s->src & 0xffff; ++nseg;
        width += s->src & 0xffff; ++used;
    }
    if (first < 0 || used == 0) return CR_BUS_UNSUPPORTED;
    /* segments must tile [0, width) exactly */
    for (unsigned x = 0; x < width;) {
        unsigned k;
        for (k = 0; k < nseg; ++k) if (seg_x[k] == x) break;
        if (k == nseg || seg_w[k] == 0) return CR_BUS_UNSUPPORTED;
        x += seg_w[k];
    }
    const struct tb710fu_pipe *f0 = &a[first];
    enum CrSplashFormat fmt;
    uint32_t v = f0->format & ~(1u << 8);
    if (v != 0x236ff) return CR_BUS_UNSUPPORTED;
    if (f0->unpack == 0x03020001) fmt = (f0->format & 256) ? CR_SPLASH_ARGB8888 : CR_SPLASH_XRGB8888;
    else if (f0->unpack == 0x03010002) fmt = (f0->format & 256) ? CR_SPLASH_ABGR8888 : CR_SPLASH_XBGR8888;
    else return CR_BUS_UNSUPPORTED;
    h = f0->src >> 16;
    uint32_t stride = f0->stride & 0xffff;
    if (!h || h > 8192 || !width || width > 8192 || (uint64_t)width * bpp > stride) return CR_BUS_INVALID;
    uint64_t size = (uint64_t)stride * h;
    if (!r->reserved_size || f0->addr < r->reserved_base ||
        (uint64_t)f0->addr + size > r->reserved_base + r->reserved_size) return CR_BUS_INVALID;
    tb710fu_read_pipes(r, io, b);
    for (unsigned p = 0; p < r->pipe_count; ++p)
        if (memcmp(&a[p], &b[p], sizeof(a[p]))) return CR_BUS_AGAIN;
    if (tb710fu_staged_pipes(r, io, &ids_again) || ids_again != ids) return CR_BUS_AGAIN;
    *fb = (struct CrSplash){.address = f0->addr, .size = (size_t)size, .width = width,
                           .height = h, .stride = stride, .format = fmt};
    return 0;
}
int kshim_dt_splash(const void *f, const struct CrIo *io,
                    kshim_framebuffer_config_t *fb, kshim_mmu_region_t *mmio)
{
#if KSHIM_DIAGNOSTIC_RESOURCES_ONLY
    memset(&g_kshim_splash_trace, 0, sizeof(g_kshim_splash_trace));
    memset(&g_kshim_splash_snapshot, 0, sizeof(g_kshim_splash_snapshot));
    /* Stage 1: parameter checking. */
    g_kshim_splash_trace.dt_stage = 1;
#endif
    if (!fb || !mmio) return -1;
    memset(fb, 0, sizeof(*fb)); memset(mmio, 0, sizeof(*mmio));
    /* Parse the root "compatible" once, up front. The complete string list is
       validated by its count first, so a malformed tail cannot hide behind an
       early exact entry; only a valid list is then searched for the exact
       entry "qcom,sunp" (never a substring, prefix or casefold match). The
       count/bool are reused for the Sun classification and the diagnostics. */
    int root_compatible_count = fdt_stringlist_count(f, 0, "compatible");
    bool root_has_sunp = root_compatible_count > 0 &&
        fdt_stringlist_search(f, 0, "compatible", "qcom,sunp") >= 0;
    /* TB710FU: SM8650 (DPU 10, legacy CTL layer routing) lists 10 SSPPs and
     * 10 mixer slots (6 real + 0xf0f dummies) in its downstream SDE node. */
    bool pineapple = root_compatible_count > 0 &&
        fdt_stringlist_search(f, 0, "compatible", "qcom,pineapple") >= 0;
#if KSHIM_DIAGNOSTIC_RESOURCES_ONLY
    g_kshim_splash_trace.root_compatible_count = root_compatible_count;
    g_kshim_splash_trace.root_has_sunp = root_has_sunp;
    /* Stage 2: reservation lookup. */
    g_kshim_splash_trace.dt_stage = 2;
#endif
    struct dt_range memory = {0};
    if (kshim_dt_splash_reservation(f, &memory, NULL)) return -1;
#if KSHIM_DIAGNOSTIC_RESOURCES_ONLY
    /* Stage 3: SDE enumeration. */
    g_kshim_splash_trace.dt_stage = 3;
#endif
    int node, sde = -1;
    for (node = fdt_node_offset_by_compatible(f, -1, "qcom,sde-kms"); node >= 0;
         node = fdt_node_offset_by_compatible(f, node, "qcom,sde-kms")) {
        if (!dt_available(f, node)) continue;
        if (sde >= 0) return -1;
        sde = node;
    }
    if (sde < 0) return -1;
#if KSHIM_DIAGNOSTIC_RESOURCES_ONLY
    /* Stage 4: mdp_phys decoding. */
    g_kshim_splash_trace.dt_stage = 4;
#endif
    struct dt_range reg;
    if (dt_reg_named(f, sde, "mdp_phys", &reg)) return -1;
    struct CrSplashResources resources = {.base = reg.base, .size = reg.size,
        .reserved_base = memory.base, .reserved_size = memory.size};
    /* The root is Sun when the validated "compatible" list has an exact
       "qcom,sunp" entry; the list was parsed and matched once, above. */
    bool sun_root = root_has_sunp;
    /* Distinguish an absent qcom,sde-hw-version from a malformed present one:
       a present property must be exactly one u32, and any getprop failure other
       than NOTFOUND rejects before any MMIO is issued. */
#if KSHIM_DIAGNOSTIC_RESOURCES_ONLY
    /* Stage 5: hw-version parsing/checks. */
    g_kshim_splash_trace.dt_stage = 5;
#endif
    int vlen;
    const fdt32_t *vprop = fdt_getprop(f, sde, "qcom,sde-hw-version", &vlen);
    uint32_t version = 0;
    if (!vprop) {
        if (vlen != -FDT_ERR_NOTFOUND) return -1;
    } else {
        if (vlen != 4) return -1;
        version = fdt32_to_cpu(vprop[0]);
    }
    bool version_present = vprop != NULL;
    /* An exact Sun root must not carry a present version other than 0xc0000000;
       never mask that conflict by falling back. */
    if (version_present && version != 0xc0000000u && sun_root) return -1;
    bool canoe = version_present && version == 0xd0000000u;
    bool sun_explicit = version_present && version == 0xc0000000u;
    /* Only an absent version plus the exact Sun root enables the fallback, and
       that fallback path is additionally fingerprint-checked below. */
    bool sun_fallback = !version_present && sun_root;
    bool sun = sun_explicit || sun_fallback;
    /* LM source-select is shared by Sun (DPU12) and Canoe (DPU13); the separate
       REC SSPP view is Canoe-only, so the two capabilities stay independent. */
    bool lm_source_select = sun || canoe;
    resources.lm_source_select = lm_source_select;
    resources.sspp_rec_separated = canoe;
    int pipes = offsets(f, sde, "qcom,sde-sspp-off", resources.pipe,
                        (lm_source_select || pineapple) ? 10 : 8);
    int ctls = offsets(f, sde, "qcom,sde-ctl-off", resources.ctl, 8);
#if KSHIM_DIAGNOSTIC_RESOURCES_ONLY
    /* Stage 6: mixer offset parsing/compaction. */
    g_kshim_splash_trace.dt_stage = 6;
#endif
    int count, raw_n = 0;
    uint32_t raw[12];
    if (lm_source_select) {
        raw_n = offsets(f, sde, "qcom,sde-mixer-off", raw, 12);
        if (raw_n < 1) return -1;
        count = 0;
        for (int i = 0; i < raw_n; ++i) {
            if (raw[i] == 0 || raw[i] == 0xf0f) continue;
            if (i >= 8) return -1;
            /* Keep the DT slot as the hardware LM index: dummy slots (0 / 0xf0f)
               still consume positions, so a compacted index would mis-address LMs. */
            resources.mixer_id[count] = (uint8_t)i;
            resources.mixer[count] = raw[i];
            ++count;
        }
        if (count < 1) return -1;
    } else if (pineapple) {
        /* Keep only the leading real LMs (CTL layer index == LM index). */
        raw_n = offsets(f, sde, "qcom,sde-mixer-off", raw, 12);
        count = 0;
        for (int i = 0; i < raw_n; ++i) {
            if (raw[i] == 0 || raw[i] == 0xf0f) break;
            if (count == 6) return -1;
            resources.mixer[count++] = raw[i];
        }
        for (int i = count; i < raw_n; ++i)
            if (raw[i] != 0 && raw[i] != 0xf0f) return -1;
    } else {
        count = offsets(f, sde, "qcom,sde-mixer-off", resources.mixer, 6);
    }
#if KSHIM_DIAGNOSTIC_RESOURCES_ONLY
    /* Stage 7: pipe/CTL/mixer count validation. */
    g_kshim_splash_trace.dt_stage = 7;
#endif
    if (pipes < 1 || ctls < 1 || count < 1) return -1;
    resources.pipe_count = pipes; resources.ctl_count = ctls; resources.mixer_count = count;
    if (lm_source_select) {
#if KSHIM_DIAGNOSTIC_RESOURCES_ONLY
        /* Stage 8: blend-table parsing. */
        g_kshim_splash_trace.dt_stage = 8;
#endif
        int stages = offsets(f, sde, "qcom,sde-mixer-blend-op-off", resources.stage, 11);
        uint32_t blendstages;
        if (stages < 1 || dt_u32(f, sde, "qcom,sde-mixer-blendstages", &blendstages) ||
            blendstages != (uint32_t)stages) return -1;
        resources.stage_count = stages;
    }
#if KSHIM_DIAGNOSTIC_RESOURCES_ONLY
    /* Stage 9: SSPP type table. */
    g_kshim_splash_trace.dt_stage = 9;
#endif
    unsigned vig = 0, dma = 0, dma_max = (lm_source_select || pineapple) ? 6 : 4;
    int types = fdt_stringlist_count(f, sde, "qcom,sde-sspp-type");
    for (int i = 0; i < pipes; ++i) {
        int len;
        const char *type = fdt_stringlist_get(f, sde, "qcom,sde-sspp-type", i, &len);
        if (!type) return -1;
        if (len == 3 && !memcmp(type, "vig", 3) && vig < 4) resources.pipe_id[i] = vig++;
        else if (len == 3 && !memcmp(type, "dma", 3) && dma < dma_max) resources.pipe_id[i] = 4 + dma++;
        else return -1;
    }
    struct CrSplash scanout;
    if (pineapple && !lm_source_select) {
        resources.pipe_count = pipes;
        int pstatus = tb710fu_splash_read(&resources, io, &scanout);
        if (pstatus) return pstatus;
        goto have_scanout;
    }
    if (sun_fallback) {
#if KSHIM_DIAGNOSTIC_RESOURCES_ONLY
        /* Stage 10: Sun fallback shape validation. */
        g_kshim_splash_trace.dt_stage = 10;
#endif
        /* Fingerprint only the measured .096 Sun shape: 10 pipes, 6 CTLs, 12 raw
           mixer slots with the first 8 valid and the last 4 dummy, 8 effective
           mixers, an 11-stage blend table, and exactly 4 VIG + 6 DMA type
           strings with no trailing extras. Explicit-version paths keep the
           ordinary capacity validation instead of these fallback restrictions. */
        if (pipes != 10 || ctls != 6 || count != 8 || resources.stage_count != 11 ||
            types != 10 || vig != 4 || dma != 6 || raw_n != 12) return -1;
        for (int i = 0; i < 8; ++i)
            if (resources.mixer_id[i] != (uint8_t)i) return -1;
        for (int i = 8; i < 12; ++i)
            if (raw[i] != 0 && raw[i] != 0xf0f) return -1;
    }
#if KSHIM_DIAGNOSTIC_RESOURCES_ONLY
    /* Stage 11: CrSplashRead. */
    g_kshim_splash_trace.dt_stage = 11;
    int status;
    if (io && io->read32) {
        struct kshim_splash_trace_ctx ctx = {.io = io, .base = (uintptr_t)resources.base};
        const struct CrIo proxy = {.context = &ctx, .read32 = trace_read32};
        status = CrSplashRead(&resources, &proxy, &scanout);
    } else {
        status = CrSplashRead(&resources, io, &scanout);
    }
#else
    int status = CrSplashRead(&resources, io, &scanout);
#endif
#if KSHIM_DIAGNOSTIC_RESOURCES_ONLY
    if (io && io->read32 && status == -95 &&
        g_kshim_splash_trace.count == 15 &&
        g_kshim_splash_trace.last_offset == 0x104224u &&
        g_kshim_splash_trace.first[0] == 6 &&
        g_kshim_splash_trace.first[1] == 0x30000u &&
        g_kshim_splash_trace.first[2] == 3 &&
        g_kshim_splash_trace.first[9] == 0x4140u) {
        kshim_splash_snapshot_capture(&resources, io);
    }
#endif
    if (status) return status;
have_scanout:;
    static const kshim_fb_format_t formats[] = {KSHIM_FB_FORMAT_RGB565, KSHIM_FB_FORMAT_XRGB8888,
        KSHIM_FB_FORMAT_ARGB8888, KSHIM_FB_FORMAT_XBGR8888, KSHIM_FB_FORMAT_ABGR8888};
    *fb = (kshim_framebuffer_config_t){.render_address = scanout.address,
        .width = scanout.width, .height = scanout.height, .stride = scanout.stride,
        .format = formats[scanout.format], .buffer_size = scanout.size,
        .bpp = scanout.format == CR_SPLASH_RGB565 ? 16 : 32,
        .foreground = UINT32_MAX, .background = scanout.format == CR_SPLASH_RGB565 ? 0 : 0xff000000};
    *mmio = (kshim_mmu_region_t){reg.base, reg.size, KSHIM_MMU_DEVICE, KSHIM_MMU_READ};
    return 0;
}
