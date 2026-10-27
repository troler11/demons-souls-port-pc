/* Differential checks against the real RSX decoders, including shader mutation. */
#include <assert.h>
#include "rsx_shader_meta.h"
static unsigned rng = 0x519ab31;
static unsigned next(void) { rng ^= rng << 13; rng ^= rng >> 17; return rng ^= rng << 5; }
static void word(u8* p, u32 w) {
    const u32 b = (w << 16) | (w >> 16);
    p[0] = b >> 24; p[1] = b >> 16; p[2] = b >> 8; p[3] = b;
}
typedef struct { const u8* p; u32 bound; } mapping;
static const u8* resolve(void* opaque, u32 size) {
    mapping* m = (mapping*)opaque; return size <= m->bound ? m->p : NULL;
}
int main(void) {
    u8 fp[8192], vp[2048], copy[2048];
    rsx_fp_constant_block cached = {0}, original;
    unsigned checks = 0;
    /* Absolute masks, not only agreement between two decoders. Include every
     * texture opcode, all units (especially bit 15), branch words that look
     * like fetches, and inline data that must never become a sampled unit. */
    const unsigned fetches[] = {0x17,0x18,0x19,0x2f,0x31,0x33,0x34};
    for (unsigned op = 0; op < sizeof(fetches)/sizeof(fetches[0]); op++) {
        for (unsigned unit = 0; unit < 16; unit++) {
            memset(fp, 0, 64);
            word(fp, (fetches[op] << 24) | (unit << 17));
            word(fp+4, 2); /* constant payload follows this fetch */
            word(fp+16, (0x17u << 24) | (((unit+1)&15) << 17) | 1u);
            word(fp+32, (0x17u << 24) | (((unit+2)&15) << 17));
            word(fp+40, 0x80000000u); /* opcode high bit: branch, not TEX */
            word(fp+48, 1u << 24 | 1u); /* MOV/end */
            ld_fp_meta* e = ld_meta_fp_store(fp, 64); assert(e);
            assert(e->units == (1u << unit));
            assert(rsx_fp_texture_units(fp, 64) == (1u << unit));
            mapping m = {fp, 64};
            word(fp, (fetches[op] << 24) | (((unit+3)&15) << 17));
            assert(!ld_meta_fp_find(fp, resolve, &m));
            e = ld_meta_fp_store(fp, 64); assert(e && e->units == (1u << ((unit+3)&15)));
        }
    }
    ld_shader_meta_clear();
    for (unsigned trial = 0; trial < 3000; trial++) {
        unsigned n = 1 + next() % 128, size = 0;
        memset(fp, 0, sizeof(fp));
        for (unsigned i = 0; i < n; i++) {
            unsigned constant = next() & 1;
            unsigned opcode = (next() & 1) ? 0x17 : 1;
            word(fp + size, (opcode << 24) | ((next() & 15) << 17) | ((next() & 1) << 21) | (i == n - 1));
            word(fp + size + 4, constant ? 2 : 0);
            word(fp + size + 8, !constant && (next() & 1) ? 0x80000000u : 0);
            size += 16;
            if (constant) {
                for (unsigned k = 0; k < 4; k++) word(fp + size + 4*k, next());
                size += 16;
            }
        }
        assert(rsx_fp_program_size(fp, size) == size);
        ld_fp_meta* e = ld_meta_fp_store(fp, size); assert(e);
        mapping m = {fp, size};
        assert(ld_meta_fp_find(fp, resolve, &m) == e);
        assert(e->units == rsx_fp_texture_units(fp, size));
        u8 gamma[16]; unsigned mask = 0;
        for (unsigned i = 0; i < 16; i++) { gamma[i] = next() & 15; if (gamma[i]) mask |= 1u << i; }
        assert((e->gamma_candidates & mask) == rsx_fp_texture_gamma_units(fp, size, gamma));
        for (unsigned seed = 0; seed < 9; seed++) {
            u64 h = 1469598103934665603ull + seed;
            assert(ld_meta_fp_hash(e, h) == rsx_fp_structural_hash(fp, size, h));
        }
        unsigned long long before = g_rsx_fp_nonfinite_constants;
        assert(rsx_fp_collect_constants(fp, size, &original) == (int)e->constants);
        unsigned long long nonfinite = g_rsx_fp_nonfinite_constants - before;
        before = g_rsx_fp_nonfinite_constants;
        ld_meta_fp_constants(e, fp, &cached);
        assert(g_rsx_fp_nonfinite_constants - before == nonfinite);
        assert(!memcmp(&cached, &original, sizeof(cached)));
        for (unsigned i = 0; i < e->constants; i++) {
            word(fp + e->offsets[i], 0x7fc00001u);
            word(fp + e->offsets[i] + 4, next());
        }
        assert(ld_meta_fp_find(fp, resolve, &m) == e);
        u64 literal = ld_meta_fnv(fp, size, 1469598103934665603ull);
        u64 structural = ld_meta_fp_hash(e, 1469598103934665603ull);
        rsx_fp_collect_constants(fp, size, &original);
        ld_meta_fp_constants(e, fp, &cached);
        assert(!memcmp(&cached, &original, sizeof(cached)));
        if (e->constants) {
            fp[e->offsets[0]] ^= 1;
            assert(ld_meta_fnv(fp, size, 1469598103934665603ull) != literal);
            assert(rsx_fp_structural_hash(fp, size, 1469598103934665603ull) == structural);
            assert(ld_meta_fp_find(fp, resolve, &m) == e);
        }
        for (unsigned off = 0; off < size; off++) {
            unsigned is_constant = 0;
            for (unsigned k = 0; k < e->constants; k++) if (off >= e->offsets[k] && off < e->offsets[k] + 16u) is_constant = 1;
            if (is_constant) continue;
            fp[off] ^= 0x80; assert(!ld_meta_fp_matches(e, fp)); fp[off] ^= 0x80; checks++;
        }
        m.bound = size - 1; assert(!ld_meta_fp_find(fp, resolve, &m));
        m.bound = size; m.p = fp + 16; assert(!ld_meta_fp_find(fp, resolve, &m));
        memset(vp, 0, sizeof(vp));
        unsigned count = 1 + next() % 128;
        unsigned txl_mask = 0;
        for (unsigned i = 0; i < count; i++) {
            u32 d1 = next(), d2 = next(), d3 = next() & ~1u;
            if (((d1 >> 22) & 31u) == 0x19u) txl_mask |= 1u << ((d2 >> 8) & 3u);
            if (i == count - 1) d3 |= 1;
            memcpy(vp + i*16 + 4, &d1, 4); memcpy(vp + i*16 + 8, &d2, 4); memcpy(vp + i*16 + 12, &d3, 4);
        }
        ld_vp_meta* v = ld_meta_vp_get(vp, count*16); assert(v && v->size == count*16);
        assert(v->txl_mask == txl_mask);
        assert(!ld_meta_vp_get(vp, count*16 - 1));
        vp[(count-1)*16 + 12] ^= 1;
        assert(!ld_meta_vp_get(vp, count*16));
        vp[(count-1)*16 + 12] ^= 1;
        rsx_vp_input_analysis analysis;
        int parsed = rsx_vp_analyze_inputs(vp, count*16, &analysis);
        assert(v->input_mask == (parsed == (int)count && analysis.exact ? analysis.input_mask : 0xffffu));
        assert(v->hash == ld_meta_fnv(vp, count*16, 1469598103934665603ull));
        memcpy(copy, vp, count*16); assert(ld_meta_vp_get(copy, count*16) == v);
        vp[0] ^= 8; v = ld_meta_vp_get(vp, count*16); assert(v && !memcmp(v->bytes, vp, count*16));
        assert(s_shader_meta.retained <= LD_META_BUDGET);
        if (!(trial % 97)) ld_shader_meta_clear();
    }
    memset(fp, 0, 32); word(fp, 1); word(fp+4, 2); word(fp+8, 0x80000000u);
    assert(!ld_meta_fp_store(fp, 32)); /* ambiguous branch/CONST uses original path */
    assert(!ld_meta_fp_store(fp, 16));
    ld_shader_meta_clear(); assert(!s_shader_meta.retained);
    /* Retain independent large mappings until the actual shared budget is hit. */
    u8* arena = (u8*)calloc(512, 65536); assert(arena);
    unsigned rejected = 0;
    for (unsigned i = 0; i < 512; i++) {
        u8* p = arena + i*65536u; word(p + 65536u - 16u, 1);
        if (!ld_meta_fp_store(p, 65536)) rejected++;
        assert(s_shader_meta.retained <= LD_META_BUDGET);
    }
    assert(rejected);
    ld_shader_meta_clear(); assert(!s_shader_meta.retained);
    free(arena);
    printf("shader metadata: 3000 differential programs, %u instruction-byte mutations passed\n", checks);
    return 0;
}
