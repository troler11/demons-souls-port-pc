/* Link against the real SDK archive, not a replacement rdch implementation. */
#include "../third_party/ps3recomp/runtime/spu/spu_context.h"
#include <windows.h>
extern u128 spu_rdch(spu_context*, uint32_t);
extern void spu_wrch(spu_context*, uint32_t, u128);
extern uint32_t spu_rchcnt(spu_context*, uint32_t);
extern void spu_ch_wake(spu_context*);
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
static spu_context ctx;
static u128 received;
static volatile LONG generation, completed;
static void write_ch(unsigned ch, unsigned value) {
    spu_wrch(&ctx, ch, spu_make_preferred_u32(value));
}
static unsigned read_events(void) { return spu_rdch(&ctx, SPU_RdEventStat)._u32[0]; }
static DWORD WINAPI reader(void* unused) {
    (void)unused; received = spu_rdch(&ctx, SPU_RdEventStat); return 0;
}
static DWORD WINAPI race(void* arg) {
    for (LONG i = 1; i <= 10000; i++) {
        while (InterlockedCompareExchange(&generation, 0, 0) < i) YieldProcessor();
        if (arg) write_ch(SPU_WrEventAck, 0x400);
        else InterlockedOr((volatile LONG*)&ctx.event_status, 1);
        InterlockedIncrement(&completed);
    }
    return 0;
}
int main(void) {
    _putenv_s("SPU_CH_BLOCK", "1");
    _putenv_s("SPU_WRCH_DIRECT", "0");
    InitializeSRWLock((SRWLOCK*)&ctx.ch_wait_lock);
    InitializeConditionVariable((CONDITION_VARIABLE*)&ctx.ch_wait_cv);
    ctx.event_status = 0x401;
    write_ch(SPU_WrEventMask, 0x400);
    CHECK(spu_rchcnt(&ctx, SPU_RdEventStat) == 1);
    CHECK(read_events() == 0x400);
    CHECK(ctx.event_status == 0x401); /* Reads do not consume hidden events. */
    write_ch(SPU_WrEventAck, 0x400);
    CHECK(ctx.event_status == 1 && spu_rchcnt(&ctx, SPU_RdEventStat) == 0);
    write_ch(SPU_WrEventMask, 1);
    CHECK(read_events() == 1);
    write_ch(SPU_WrEventAck, 1);
    CHECK(ctx.event_status == 0);

    ctx.event_status = 0x400;
    write_ch(SPU_WrEventMask, 0);
    CHECK(spu_rchcnt(&ctx, SPU_RdEventStat) == 0 && read_events() == 0);
    CHECK(ctx.event_status == 0x400);
    write_ch(SPU_WrEventMask, 0x400);
    ctx.int_enable = 0; /* Interrupt enable does not change the channel mask. */
    CHECK(read_events() == 0x400);

    ctx.event_status = 1;
    HANDLE h = CreateThread(NULL, 0, reader, NULL, 0, NULL);
    CHECK(h && WaitForSingleObject(h, 50) == WAIT_TIMEOUT);
    InterlockedOr((volatile LONG*)&ctx.event_status, 2);
    spu_ch_wake(&ctx);
    CHECK(WaitForSingleObject(h, 50) == WAIT_TIMEOUT); /* Only masked events. */
    InterlockedOr((volatile LONG*)&ctx.event_status, 0x400);
    spu_ch_wake(&ctx);
    CHECK(WaitForSingleObject(h, 2000) == WAIT_OBJECT_0);
    CloseHandle(h);
    CHECK(received._u32[0] == 0x400 && ctx.event_status == 0x403);
    CHECK(!received._u32[1] && !received._u32[2] && !received._u32[3]);
    write_ch(SPU_WrEventAck, 0x400);
    CHECK(ctx.event_status == 3 && spu_rchcnt(&ctx, SPU_RdEventStat) == 0);

    HANDLE threads[] = {CreateThread(NULL, 0, race, (void*)1, 0, NULL),
                        CreateThread(NULL, 0, race, NULL, 0, NULL)};
    CHECK(threads[0] && threads[1]);
    for (LONG i = 1; i <= 10000; i++) {
        ctx.event_status = 0x400;
        InterlockedExchange(&generation, i);
        while (InterlockedCompareExchange(&completed, 0, 0) < 2*i) YieldProcessor();
        CHECK(ctx.event_status == 1);
    }
    CHECK(WaitForMultipleObjects(2, threads, TRUE, 2000) == WAIT_OBJECT_0);
    CloseHandle(threads[0]); CloseHandle(threads[1]);
    puts("PASS: event mask, pending bits, ACK, channel readiness, blocking wake and 10000 producer races");
    return 0;
}
