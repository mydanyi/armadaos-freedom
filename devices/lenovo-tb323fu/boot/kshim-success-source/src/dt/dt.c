/* SPDX-License-Identifier: MIT */
#include <dt.h>
#include <string.h>

#define DT_PARENT_INDEX_MAX 8192
struct dt_parent_index_entry { int node, parent; uint32_t compatible_offset; int compatible_length; };
static struct dt_parent_index_entry dt_parent_index[DT_PARENT_INDEX_MAX];
static const void *dt_parent_index_fdt;
static int dt_parent_index_count = -1;

void dt_parent_index_begin(const void *f)
{
    dt_parent_index_fdt = NULL;
    dt_parent_index_count = -1;
    int node = -1, depth = -1, count = 0, stack[64];
    while ((node = fdt_next_node(f, node, &depth)) >= 0) {
        if (depth < 0) break;
        if (depth >= 64 || count >= DT_PARENT_INDEX_MAX) {
            dt_parent_index_count = -1;
            return;
        }
        stack[depth] = node;
        dt_parent_index[count].node = node;
        dt_parent_index[count].parent = depth ? stack[depth - 1] : -FDT_ERR_NOTFOUND;
        int len;
        const void *prop = fdt_getprop(f, node, "compatible", &len);
        dt_parent_index[count].compatible_offset =
            prop ? (uint32_t)((const char *)prop - (const char *)f) : 0;
        dt_parent_index[count].compatible_length = len;
        ++count;
    }
    if (node < 0) {
        dt_parent_index_count = -1;
        return;
    }
    dt_parent_index_fdt = f;
    dt_parent_index_count = count;
}

void dt_parent_index_end(void)
{
    dt_parent_index_fdt = NULL;
    dt_parent_index_count = -1;
}

int dt_parent_offset(const void *fdt, int offset)
{
    if (dt_parent_index_fdt == fdt && dt_parent_index_count >= 0) {
        int lo = 0, hi = dt_parent_index_count - 1;
        while (lo <= hi) {
            int mid = lo + (hi - lo) / 2;
            if (dt_parent_index[mid].node == offset) return dt_parent_index[mid].parent;
            if (dt_parent_index[mid].node < offset) lo = mid + 1;
            else hi = mid - 1;
        }
    }
    return fdt_parent_offset(fdt, offset);
}

int dt_node_offset_by_compatible(const void *f, int start, const char *compatible)
{
    if (dt_parent_index_fdt == f && dt_parent_index_count >= 0) {
        int i = 0;
        if (start >= 0) {
            int lo = 0, hi = dt_parent_index_count - 1, found = -1;
            while (lo <= hi) {
                int mid = lo + (hi - lo) / 2;
                if (dt_parent_index[mid].node == start) { found = mid; break; }
                if (dt_parent_index[mid].node < start) lo = mid + 1;
                else hi = mid - 1;
            }
            if (found < 0) return fdt_node_offset_by_compatible(f, start, compatible);
            i = found + 1;
        }
        for (; i < dt_parent_index_count; ++i) {
            if (!dt_parent_index[i].compatible_offset) {
                int len = dt_parent_index[i].compatible_length;
                if (len < 0 && len != -FDT_ERR_NOTFOUND) return len;
                continue;
            }
            const char *p = (const char *)f + dt_parent_index[i].compatible_offset;
            if (fdt_stringlist_contains(p, dt_parent_index[i].compatible_length, compatible))
                return dt_parent_index[i].node;
        }
        return -FDT_ERR_NOTFOUND;
    }
    return fdt_node_offset_by_compatible(f, start, compatible);
}

int dt_validate(const void *fdt, size_t available)
{
    if (!fdt || ((uintptr_t)fdt & 7) || available < sizeof(struct fdt_header))
        return -FDT_ERR_TRUNCATED;
    if (fdt_check_header(fdt)) return -FDT_ERR_BADMAGIC;
    if (fdt_totalsize(fdt) > available) return -FDT_ERR_TRUNCATED;
    return fdt_check_full(fdt, available);
}
const char *dt_string(const void *f, int node, const char *property)
{
    int len;
    const char *p = fdt_getprop(f, node, property, &len);
    if (!p || len <= 0 || p[len - 1] || memchr(p, 0, len - 1)) return NULL;
    return p;
}
int dt_string_index(const void *f, int node, const char *property, const char *name)
{
    int len;
    const char *p = fdt_getprop(f, node, property, &len);
    if (!p || len <= 0 || p[len - 1]) return -1;
    int result = -1, index = 0;
    for (int off = 0; off < len; ++index) {
        size_t n = strlen(p + off);
        if (!n) return -1;
        if (!strcmp(p + off, name)) {
            if (result >= 0) return -1;
            result = index;
        }
        off += (int)n + 1;
    }
    return result;
}
bool dt_available(const void *f, int node)
{
    while (node >= 0) {
        int len;
        const void *status = fdt_getprop(f, node, "status", &len);
        if (status) {
            const char *s = dt_string(f, node, "status");
            if (!s || (strcmp(s, "okay") && strcmp(s, "ok"))) return false;
        } else if (len != -FDT_ERR_NOTFOUND) return false;
        if (!node) return true;
        node = dt_parent_offset(f, node);
    }
    return false;
}
int dt_u32(const void *f, int node, const char *property, uint32_t *value)
{
    int len;
    const fdt32_t *p = fdt_getprop(f, node, property, &len);
    if (!p || len != 4 || !value) return -1;
    *value = fdt32_to_cpu(*p);
    return 0;
}
int dt_bool(const void *f, int node, const char *property, bool *value)
{
    int len;
    const void *p = fdt_getprop(f, node, property, &len);
    if (!value || (p && len != 0) || (!p && len != -FDT_ERR_NOTFOUND)) return -1;
    *value = p != NULL;
    return 0;
}
int dt_phandle(const void *f, uint32_t handle)
{
    if (!handle || handle == UINT32_MAX) return -1;
    int found = -1;
    for (int n = fdt_next_node(f, -1, NULL); n >= 0; n = fdt_next_node(f, n, NULL)) {
        uint32_t h, legacy;
        int alen, blen;
        const fdt32_t *ap = fdt_getprop(f, n, "phandle", &alen);
        const fdt32_t *bp = fdt_getprop(f, n, "linux,phandle", &blen);
        if ((ap && alen != 4) || (bp && blen != 4)) return -1;
        bool a = ap != NULL;
        bool b = bp != NULL;
        if (a) h = fdt32_to_cpu(*ap);
        if (b) legacy = fdt32_to_cpu(*bp);
        if (a && b && h != legacy) return -1;
        if ((a && h == handle) || (b && legacy == handle)) {
            if (found >= 0) return -1;
            found = n;
        }
    }
    return found >= 0 && dt_available(f, found) ? found : -1;
}
int dt_reference(const void *f, int node, const char *prop, int index)
{
    int len;
    const fdt32_t *p = fdt_getprop(f, node, prop, &len);
    if (!dt_available(f, node) || !p || len <= 0 || len % 4 || index < 0 || index >= len / 4)
        return -1;
    return dt_phandle(f, fdt32_to_cpu(p[index]));
}
int dt_specifier(const void *f, int node, const char *prop,
                 const char *cells_property, unsigned index, struct dt_spec *spec)
{
    int len;
    const fdt32_t *p = fdt_getprop(f, node, prop, &len);
    if (!spec || !dt_available(f, node) || !p || len <= 0 || len % 4) return -1;
    bool found = false;
    for (unsigned off = 0, item = 0; off < (unsigned)len / 4; ++item) {
        uint32_t cells;
        int provider = dt_phandle(f, fdt32_to_cpu(p[off++]));
        if (provider < 0 || dt_u32(f, provider, cells_property, &cells) ||
            cells > 8 || cells > (unsigned)len / 4 - off) return -1;
        if (item == index) {
            spec->node = provider; spec->count = cells;
            for (unsigned i = 0; i < cells; ++i) spec->args[i] = fdt32_to_cpu(p[off + i]);
            found = true;
        }
        off += cells;
    }
    return found ? 0 : -1;
}
static int cells(const void *f, int node, const char *prop, int fallback)
{
    int len;
    const fdt32_t *p = fdt_getprop(f, node, prop, &len);
    if (!p) return len == -FDT_ERR_NOTFOUND ? fallback : -1;
    if (len != 4 || fdt32_to_cpu(*p) > 2) return -1;
    return (int)fdt32_to_cpu(*p);
}
static uint64_t value(const fdt32_t *p, int n)
{
    uint64_t v = 0;
    while (n--) v = (v << 32) | fdt32_to_cpu(*p++);
    return v;
}
bool dt_contains(struct dt_range r, uint64_t a, uint64_t s)
{
    return r.size && s && r.base <= UINT64_MAX - r.size && a >= r.base &&
        s <= r.size && a - r.base <= r.size - s;
}
int dt_raw_reg(const void *f, int node, unsigned index, uint64_t *a, uint64_t *s)
{
    if (!a || !s || !dt_available(f, node)) return -1;
    int parent = dt_parent_offset(f, node), len;
    if (parent < 0) return -1;
    int ac = cells(f, parent, "#address-cells", 2), sc = cells(f, parent, "#size-cells", 1);
    if (ac < 1 || sc < 0) return -1;
    const fdt32_t *p = fdt_getprop(f, node, "reg", &len);
    int tuple = ac + sc;
    if (!p || len <= 0 || len % (tuple * 4) || index >= (unsigned)len / (tuple * 4)) return -1;
    p += index * tuple;
    *a = value(p, ac); *s = value(p + ac, sc);
    return *a <= UINT64_MAX - *s ? 0 : -1;
}
int dt_reg(const void *f, int node, unsigned index, struct dt_range *r)
{
    if (!r || dt_raw_reg(f, node, index, &r->base, &r->size) || !r->size) return -1;
    for (int bus = dt_parent_offset(f, node); bus > 0; bus = dt_parent_offset(f, bus)) {
        int parent = dt_parent_offset(f, bus), len;
        int ac = cells(f, bus, "#address-cells", 2), sc = cells(f, bus, "#size-cells", 1);
        int pc = cells(f, parent, "#address-cells", 2);
        if (ac < 1 || sc < 1 || pc < 1) return -1;
        const fdt32_t *p = fdt_getprop(f, bus, "ranges", &len);
        if (!p) return -1; /* An absent ranges property is not identity mapping. */
        if (!len) continue;
        int tuple = ac + pc + sc;
        if (len < 0 || len % (tuple * 4)) return -1;
        bool found = false;
        uint64_t translated = 0;
        for (int off = 0; off < len / 4; off += tuple) {
            struct dt_range window = {value(p + off, ac), value(p + off + ac + pc, sc)};
            uint64_t target = value(p + off + ac, pc);
            if (!dt_contains(window, r->base, r->size)) continue;
            if (found || target > UINT64_MAX - window.size) return -1;
            translated = target + r->base - window.base;
            found = true;
        }
        if (!found) return -1;
        r->base = translated;
    }
    return 0;
}
int dt_reg_named(const void *f, int node, const char *name, struct dt_range *r)
{
    int index = dt_string_index(f, node, "reg-names", name);
    int names = fdt_stringlist_count(f, node, "reg-names"), len;
    int parent = dt_parent_offset(f, node);
    int ac = cells(f, parent, "#address-cells", 2), sc = cells(f, parent, "#size-cells", 1);
    const void *reg = fdt_getprop(f, node, "reg", &len);
    if (index < 0 || names < 1 || !reg || ac < 1 || sc < 0 ||
        len <= 0 || len % (4 * (ac + sc)) || names != len / (4 * (ac + sc))) return -1;
    /* Reject duplicate names even when the requested entry itself is unique. */
    for (int i = 0; i < names; ++i) {
        const char *entry = fdt_stringlist_get(f, node, "reg-names", i, NULL);
        if (!entry || dt_string_index(f, node, "reg-names", entry) != i) return -1;
    }
    return dt_reg(f, node, (unsigned)index, r);
}
int dt_reservations(const void *f, struct dt_range *ranges, bool *no_map,
                    size_t capacity, size_t *count)
{
    if (!ranges || !no_map || !count) return -1;
    *count = 0;
    int n = fdt_num_mem_rsv(f);
    if (n < 0) return -1;
    for (int i = 0; i < n; ++i) {
        if (*count == capacity || fdt_get_mem_rsv(f, i, &ranges[*count].base, &ranges[*count].size)) return -1;
        if (!ranges[*count].size || ranges[*count].base > UINT64_MAX - ranges[*count].size) return -1;
        no_map[(*count)++] = true;
    }
    int parent = fdt_path_offset(f, "/reserved-memory");
    if (parent < 0) return parent == -FDT_ERR_NOTFOUND ? 0 : -1;
    if (!dt_available(f, parent)) return 0;
    int node;
    fdt_for_each_subnode(node, f, parent) {
        if (!dt_available(f, node)) continue;
        bool flag;
        if (dt_bool(f, node, "no-map", &flag)) return -1;
        int len;
        if (!fdt_getprop(f, node, "reg", &len)) {
            if (len != -FDT_ERR_NOTFOUND) return -1;
            continue; /* Dynamic pools have not been allocated by this shim. */
        }
        int ac = cells(f, parent, "#address-cells", 2), sc = cells(f, parent, "#size-cells", 1);
        if (ac < 1 || sc < 1 || len <= 0 || len % (4 * (ac + sc))) return -1;
        for (int i = 0; i < len / (4 * (ac + sc)); ++i) {
            uint64_t base, size;
            if (dt_raw_reg(f, node, (unsigned)i, &base, &size)) return -1;
            /* skip unallocated firmware reserved-memory placeholders (base==size==0);
             * SM8650 (qcom,pineapple) also carries size-0 nodes with a nonzero
             * base (e.g. mpss_dsm_region@86b00000) that reserve nothing. */
            if (size == 0 && (base == 0 ||
                              fdt_node_check_compatible(f, 0, "qcom,pineapple") == 0))
                continue;
            if (*count == capacity || dt_reg(f, node, i, &ranges[*count])) return -1;
            no_map[(*count)++] = flag;
        }
    }
    return 0;
}
