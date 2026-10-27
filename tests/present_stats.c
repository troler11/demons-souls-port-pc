/* Deterministic clock input for the actual submission-interval collector.
 * The independent expected distributions are checked by test_present_stats.py. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint64_t u64;
typedef int64_t LONGLONG;
#define LD_PRESENT_RING_CAP 16384u
typedef struct { u64 present_id; LONGLONG qpc; } ld_present_sample;
static ld_present_sample g_ld_present_ring[LD_PRESENT_RING_CAP];
static u64 g_ld_present_total;
static LONGLONG g_ld_qpc_frequency = 1000000;
#include "../third_party/ps3recomp/libs/video/rsx_present_stats.h"

static void push(LONGLONG qpc)
{
    const u64 id = ++g_ld_present_total;
    ld_present_sample* s = &g_ld_present_ring[(id - 1u) & (LD_PRESENT_RING_CAP - 1u)];
    s->present_id = id; s->qpc = qpc;
    ld_present_stats_record();
}
static void reset(void)
{
    memset(g_ld_present_ring, 0, sizeof(g_ld_present_ring));
    memset(&s_present_stats, 0, sizeof(s_present_stats));
    s_present_stats.enabled = 1;
    g_ld_present_total = 0;
}
int main(void)
{
    reset();
    for (int i = 0; i <= 500; i++) push((LONGLONG)i * 20000);
    reset();
    LONGLONG qpc = 0;
    for (int i = 0; i <= 600; i++) {
        if (i) qpc += i % 50 == 0 ? 100000 : 20000;
        push(qpc);
    }
    reset(); push(0);
    s_present_stats.enabled = 0;
    for (int i = 1; i <= 20000; i++) push((LONGLONG)i * 1000);
    s_present_stats.enabled = 1; ld_present_stats_record();
    /* A restarted renderer must start a new interval window. */
    g_ld_present_total = 0;
    for (int i = 0; i <= 250; i++) push((LONGLONG)i * 20000);
    return 0;
}
