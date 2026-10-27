/* Cache changes, endian/width/restart transitions and memory budget. */
#include <assert.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "../third_party/ps3recomp/libs/video/rsx_index_range.h"
static uint32_t rng = 0x570b3a1;
static uint32_t random32(void) { rng ^= rng << 13; rng ^= rng >> 17; return rng ^= rng << 5; }
static int ascending(const void* a, const void* b) {
    uint32_t x = *(const uint32_t*)a, y = *(const uint32_t*)b;
    return (x > y) - (x < y);
}
static void put_index(uint8_t* p, unsigned stride, uint32_t value)
{
    for (unsigned i = 0; i < stride; i++) p[i] = (uint8_t)(value >> (8*(stride-i-1)));
}
static void oracle(const uint8_t* p, uint32_t count, int is32, int restart,
    uint32_t rv, uint32_t* lo, uint32_t* hi)
{
    uint32_t sorted[512], used = 0, stride = is32 ? 4 : 2;
    assert(count <= 512);
    for (uint32_t i = 0; i < count; i++) {
        uint32_t v = 0;
        for (uint32_t k = 0; k < stride; k++) v = (v << 8) | p[i*stride+k];
        if (!restart || v != rv) sorted[used++] = v;
    }
    qsort(sorted, used, sizeof(uint32_t), ascending);
    *lo = used ? sorted[0] : UINT32_MAX; *hi = used ? sorted[used-1] : 0;
}
static void compare(const uint8_t* p, uint32_t count, int is32, int restart, uint32_t rv)
{
    uint32_t lo, hi, expected_lo, expected_hi;
    oracle(p, count, is32, restart, rv, &expected_lo, &expected_hi);
    ld_index_range_scan_scalar(p, count, is32, restart, rv, &lo, &hi);
    assert(lo == expected_lo && hi == expected_hi);
#if defined(__AVX2__)
    ld_index_range_scan_avx2(p, count, is32, restart, rv, &lo, &hi);
    assert(lo == expected_lo && hi == expected_hi);
#endif
    ld_index_range_get(p, count, is32, restart, rv, &lo, &hi, 1);
    assert(lo == expected_lo && hi == expected_hi);
    ld_index_range_get(p, count, is32, restart, rv, &lo, &hi, 0);
    assert(lo == expected_lo && hi == expected_hi);
}
int main(void)
{
    uint8_t buffer[4096]; uint64_t mutations = 0;
    for (unsigned trial = 0; trial < 2048; trial++) {
        for (unsigned i = 0; i < sizeof(buffer); i++) buffer[i] = (uint8_t)random32();
        uint8_t* p = buffer + (trial & 7);
        uint32_t count = 1 + random32() % 256;
        int is32 = trial & 1, restart = (trial >> 1) & 1;
        uint32_t rv = trial & 4 ? UINT32_MAX : 0;
        if (!(trial % 11)) memset(p, 0xff, count * (is32 ? 4 : 2));
        compare(p, count, is32, restart, rv);
        uint64_t hits = s_index_range.hits;
        compare(p, count, is32, restart, rv); assert(s_index_range.hits == hits + 1);
        for (unsigned i = 0; i < count * (is32 ? 4u : 2u); i++) {
            p[i] ^= 0x80;
            uint64_t misses = s_index_range.misses;
            compare(p, count, is32, restart, rv);
            assert(s_index_range.misses == misses + 1);
            p[i] ^= 0x80; mutations++;
        }
        /* Same host address with a different width, length or restart state. */
        compare(p, count, !is32, restart, rv);
        compare(p, count - 1, is32, restart, rv);
        compare(p, count, is32, !restart, rv);
        compare(p, count, is32, restart, rv ^ 0xffffffffu);
        assert(s_index_range.retained <= LD_IR_BUDGET);
        if (!(trial % 73)) ld_index_range_clear();
    }
    uint32_t lo, hi;
    /* Mixed restart vectors, empty vectors followed by valid tails, and
     * unsigned 32-bit values above INT_MAX. */
    for (int wide = 0; wide <= 1; wide++) for (unsigned count = 1; count <= 80; count++) {
        const unsigned stride = wide ? 4 : 2;
        const uint32_t rv = wide ? 0x80000003u : 0xbeefu;
        for (unsigned i = 0; i < count; i++) put_index(buffer+i*stride, stride, rv);
        compare(buffer, count, wide, 1, rv);
        put_index(buffer+(count-1)*stride, stride, 0);
        compare(buffer, count, wide, 1, rv);
        put_index(buffer, stride, wide ? UINT32_MAX : 0xffffu);
        compare(buffer, count, wide, 1, rv);
    }
    ld_index_range_get(NULL, 0, 1, 0, 0, &lo, &hi, 1);
    assert(lo == UINT32_MAX && hi == 0);
    ld_index_range_clear();
#ifdef _WIN32
    SYSTEM_INFO system; GetSystemInfo(&system);
    const size_t page = system.dwPageSize;
    uint8_t* guarded = (uint8_t*)VirtualAlloc(NULL, page*2, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    assert(guarded); DWORD old;
    assert(VirtualProtect(guarded+page, page, PAGE_NOACCESS, &old));
    memset(guarded, 0xff, page);
    for (unsigned count = 0; count <= 257; count++) for (int wide = 0; wide <= 1; wide++) {
        const uint8_t* p = guarded+page-count*(wide ? 4u : 2u);
        compare(p, count, wide, 0, UINT32_MAX);
        compare(p, count, wide, 1, wide ? UINT32_MAX : 0xffffu);
        compare(p, count, wide, 1, 0x10000u);
    }
    ld_index_range_clear();
    assert(VirtualFree(guarded, 0, MEM_RELEASE));
#endif
    uint8_t* arena = (uint8_t*)calloc(16, LD_IR_MAX_SPAN); assert(arena);
    for (unsigned i = 0; i < 16; i++) {
        ld_index_range_get(arena + i*LD_IR_MAX_SPAN, LD_IR_MAX_SPAN/4, 1, 0, 0, &lo, &hi, 1);
        assert(lo == 0 && hi == 0 && s_index_range.retained <= LD_IR_BUDGET);
    }
    assert(s_index_range.budget_skips);
    ld_index_range_get(arena, LD_IR_MAX_SPAN/4+1, 1, 0, 0, &lo, &hi, 1);
    assert(lo == 0 && hi == 0 && s_index_range.plain);
    ld_index_range_clear(); assert(!s_index_range.retained);
    free(arena);
    printf("index ranges: 2048 fixtures, %llu byte mutations, restart/width/reuse/budget passed\n", (unsigned long long)mutations);
    return 0;
}
