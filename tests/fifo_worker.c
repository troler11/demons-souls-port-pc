/* Exercise the actual private queue, including backpressure and public-entry
 * ordering. No GPU needed: the callback models the renderer's recursive lock. */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
typedef uint32_t u32;
typedef uint64_t u64;
static int fail_thread;
static HANDLE test_thread(LPSECURITY_ATTRIBUTES a, SIZE_T size,
    LPTHREAD_START_ROUTINE fn, LPVOID p, DWORD flags, LPDWORD tid)
{ return fail_thread ? NULL : CreateThread(a, size, fn, p, flags, tid); }
#define CreateThread test_thread
#include "../third_party/ps3recomp/libs/video/rsx_fifo_worker.h"
#undef CreateThread
#include "../third_party/ps3recomp/libs/video/rsx_fifo_packet.h"
static CRITICAL_SECTION renderer;
static CRITICAL_SECTION observations;
static u64 observed = 1, expected = 1, executed, submitted;
static volatile LONG stop_observer;
void ps3_name_host_thread(const char* name) { (void)name; }
static void renderer_enter(void)
{ ld_fifo_enter(); ld_fifo_drain(); EnterCriticalSection(&renderer); }
static void renderer_leave(void)
{ LeaveCriticalSection(&renderer); ld_fifo_leave(); }
static void ld_fifo_execute(const ld_fifo_word* words, u32 count)
{
    renderer_enter();
    for (u32 i = 0; i < count; i++) {
        observed = (observed ^ ((u64)words[i].method << 32) ^ words[i].arg) * 1099511628211ull;
        executed++;
    }
    /* Nested renderer entry on the worker must not drain/wait for itself. */
    renderer_enter(); renderer_leave();
    if (s_fifo.on && !fail_thread) Sleep(1);
    renderer_leave();
}
static DWORD WINAPI observer(LPVOID p)
{
    (void)p;
    while (!InterlockedCompareExchange(&stop_observer, 0, 0)) {
        EnterCriticalSection(&observations);
        renderer_enter();
        assert(executed == submitted && observed == expected);
        renderer_leave();
        LeaveCriticalSection(&observations);
        Sleep(1);
    }
    return 0;
}
static void ld_fifo_direct(u32 method, u32 step, const u32* args, u32 count)
{
    for (u32 i = 0; i < count; i++) {
        const ld_fifo_word word = {method + i * step, args[i]};
        ld_fifo_execute(&word, 1);
    }
}
static void test_packets(void)
{
    assert(rsx_fifo_packet_safe(0, 0x200, 4, 100));
    assert(rsx_fifo_packet_safe(0, 0x1818, 0, 2047));
    assert(!rsx_fifo_packet_safe(1, 0x200, 4, 1));
    assert(!rsx_fifo_packet_safe(0, 0x50, 0, 1));
    assert(!rsx_fifo_packet_safe(0, 0x17fc, 4, 2));
    assert(!rsx_fifo_packet_safe(0, 0x1800, 0, 1));
    assert(!rsx_fifo_packet_safe(0, 0x1d6c, 4, 2));
    assert(!rsx_fifo_packet_safe(0, 0x1d74, 0, 1));
    assert(!rsx_fifo_packet_safe(0, 0x1ffc, 4, 2));
    assert(!rsx_fifo_packet_safe(0, 0x200, 4, 2048));
    assert(!rsx_fifo_packet_safe(0, 0xfffffffc, 4, 2));
}
int main(int argc, char** argv)
{
    test_packets();
    _putenv_s("LD_FIFO_ASYNC", argc > 1 ? argv[1] : "1");
    _putenv_s("LD_FIFO_PROF", "0");
    fail_thread = argc > 2;
    InitializeCriticalSection(&renderer);
    InitializeCriticalSection(&observations);
    HANDLE other = CreateThread(NULL, 0, observer, NULL, 0, NULL);
    assert(other);
    u32 args[2047];
    for (u32 batch = 0; batch < 260; batch++) {
        u32 count = 1u + (batch * 73u) % 2047u;
        const u32 step = (batch & 1u) ? 4u : 0u;
        EnterCriticalSection(&observations);
        ld_fifo_enter();
        for (u32 i = 0; i < count; i++) {
            args[i] = batch * 301u + i;
            expected = (expected ^ ((u64)(0x200u + i * step) << 32) ^ args[i]) * 1099511628211ull;
        }
        ld_fifo_packet(0x200, step, args, count);
        submitted += count;
        ld_fifo_leave();
        LeaveCriticalSection(&observations);
        /* Queued commands must not reference the caller's scratch buffer. */
        memset(args, 0xA5, sizeof args);
        if (batch % 11u == 0) {
            renderer_enter();
            assert(executed == submitted && observed == expected);
            renderer_leave();
        }
    }
    InterlockedExchange(&stop_observer, 1);
    assert(WaitForSingleObject(other, 10000) == WAIT_OBJECT_0);
    CloseHandle(other);
    ld_fifo_shutdown();
    args[0] = 123;
    expected = (expected ^ ((u64)0x200u << 32) ^ args[0]) * 1099511628211ull;
    ld_fifo_packet(0x200, 0, args, 1); submitted++;
    assert(executed == submitted && observed == expected);
    assert(executed == submitted && observed == expected);
    /* Shutdown is idempotent, and later ingress falls back synchronously. */
    ld_fifo_shutdown();
    puts("FIFO: copied commands, exact order, barriers, bounded backpressure, nested/concurrent entry and shutdown passed");
    return 0;
}
