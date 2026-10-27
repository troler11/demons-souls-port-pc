/* Exercise real atomic event acknowledgement against a concurrent producer. */
#include <assert.h>
#include "../third_party/ps3recomp/runtime/spu/spu_channel_direct.h"
#define ROUNDS 100000
static spu_context ctx;
static volatile long generation, done;
static DWORD WINAPI worker(void* opaque)
{
    const int ack = (int)(uintptr_t)opaque;
    u128 v = {{0}}; v._u32[0] = 0x400; v._u32[1] = 0xffffffffu;
    for (long i = 1; i <= ROUNDS; i++) {
        while (_InterlockedCompareExchange(&generation, 0, 0) < i) YieldProcessor();
        if (ack) assert(spu_wrch_direct_try(&ctx, SPU_WrEventAck, v));
        else _InterlockedOr((volatile long*)&ctx.event_status, 1);
        _InterlockedIncrement(&done);
    }
    return 0;
}
static DWORD WINAPI gate(void* opaque)
{
    const int expected = (int)(uintptr_t)opaque;
    for (int i = 0; i < 10000; i++) assert(spu_wrch_direct_enabled() == expected);
    return 0;
}
int main(int argc, char** argv)
{
    int expected = argc > 1 ? atoi(argv[1]) : 0;
    if (argc > 2) _putenv_s("SPU_CHHIST", argv[2]);
    _putenv_s("SPU_WRCH_DIRECT", argc > 1 ? (expected ? "1" : "0") : "");
    if (argc > 2) expected = 0;
    HANDLE threads[8];
    for (int i = 0; i < 8; i++) threads[i] = CreateThread(NULL, 0, gate, (void*)(uintptr_t)expected, 0, NULL);
    assert(WaitForMultipleObjects(8, threads, TRUE, 10000) == WAIT_OBJECT_0);
    for (int i = 0; i < 8; i++) CloseHandle(threads[i]);
    /* Unhandled writes must leave every byte of the context untouched. */
    static spu_context snapshot;
    memset(&ctx, 0x5a, sizeof(ctx)); memcpy(&snapshot, &ctx, sizeof(ctx));
    const unsigned fallback[] = {MFC_Cmd, MFC_WrTagUpdate, SPU_WrOutMbox,
        SPU_WrOutIntrMbox, SPU_WrDec, SPU_RdEventStat, 127};
    u128 v = {{0}}; v._u32[0] = 0xffffffffu;
    for (unsigned i = 0; i < sizeof(fallback)/sizeof(fallback[0]); i++) {
        assert(!spu_wrch_direct_try(&ctx, fallback[i], v));
        assert(!memcmp(&ctx, &snapshot, sizeof(ctx)));
    }
    assert(spu_wrch_direct_try(&ctx, MFC_TagID, v)); assert(ctx.mfc_tag == 31);
    memset(&ctx, 0, sizeof(ctx));
    threads[0] = CreateThread(NULL, 0, worker, (void*)1, 0, NULL);
    threads[1] = CreateThread(NULL, 0, worker, NULL, 0, NULL);
    for (long i = 1; i <= ROUNDS; i++) {
        ctx.event_status = 0x400;
        _InterlockedExchange(&generation, i);
        while (_InterlockedCompareExchange(&done, 0, 0) < i*2) YieldProcessor();
        assert(ctx.event_status == 1); /* unrelated producer bit must never be lost */
    }
    assert(WaitForMultipleObjects(2, threads, TRUE, 10000) == WAIT_OBJECT_0);
    CloseHandle(threads[0]); CloseHandle(threads[1]);
    puts("SPU direct channels: concurrent initialization, fallback and 100000 event races passed");
    return 0;
}
