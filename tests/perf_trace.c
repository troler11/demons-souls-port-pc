#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "rsx_perf_trace.h"
int main(int argc, char** argv)
{
    assert(argc == 2);
    _putenv_s("PS3_PERF_TRACE", !strcmp(argv[1], "off") ? "0" : "1");
    _putenv_s("PS3_PERF_TRACE_MIN_US", "1000");
    LARGE_INTEGER frequency, ticks;
    QueryPerformanceFrequency(&frequency); QueryPerformanceCounter(&ticks);
    const uint64_t f = (uint64_t)frequency.QuadPart;
    const uint64_t now = ((uint64_t)ticks.QuadPart / f) * 1000000ull + ((uint64_t)ticks.QuadPart % f) * 1000000ull / f;
    char start[64]; snprintf(start, sizeof start, "%llu", (unsigned long long)(now + 86400000000ull));
    _putenv_s("PS3_PERF_TRACE_START_US", !strcmp(argv[1], "future") ? start : "0");
    if (!strcmp(argv[1], "off")) {
        assert(!ps3_perf_trace_enabled() && !ps3_perf_trace_begin());
        ps3_perf_trace_emit("disabled", now, now + 1000, 7, 9);
        assert(!s_perf_trace.events); return 0;
    }
    assert(ps3_perf_trace_enabled());
    const uint64_t days = 365ull * 24 * 60 * 60;
    assert(ps3_perf_trace_qpc_us(days * f + f / 2) == days * 1000000ull + (f / 2) * 1000000ull / f);
    if (!strcmp(argv[1], "future")) {
        assert(!ps3_perf_trace_begin());
        ps3_perf_trace_emit("before-start", now, now + 1000, 7, 9);
        assert(!s_perf_trace.events); return 0;
    }
    uint64_t begin = ps3_perf_trace_begin(); assert(begin);
    ps3_perf_trace_end("zero", 0, 0, 0);
    ps3_perf_trace_span("below", begin, begin + 999, 7, 9);
    ps3_perf_trace_span("reversed", begin, begin - 1, 7, 9);
    assert(!s_perf_trace.events);
    ps3_perf_trace_span("threshold", begin, begin + 1000, 7, 9);
    assert(s_perf_trace.events == 1);
    s_perf_trace.events = 19999;
    ps3_perf_trace_emit("last", begin, begin + 2000, 11, 13);
    ps3_perf_trace_emit("over-cap", begin, begin + 2000, 11, 13);
    assert(s_perf_trace.events == 20001);
    return 0;
}
