/*
 * ps3recomp game project -- game-specific hooks and overrides.
 *
 * main.cpp is the runner and is game-agnostic. This file is where a port puts
 * everything that is not: the PRX loading hook it must answer, overrides for
 * firmware functions the HLE library gets wrong for this title, and patches
 * over recompiled code.
 *
 * It starts out as the empty version of all of that, which is what a game with
 * no lifted PRX and no overrides needs.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <windows.h>
#include <io.h>
#include "../third_party/ps3recomp/libs/video/rsx_perf_trace.h"

#include "ps3emu/ps3types.h"
#include "ps3emu/error_codes.h"

/* ---------------------------------------------------------------------------
 * PRX loading
 *
 * main.cpp calls this once, after the lifted function table is registered and
 * vm_base is live, before the game runs. A title that lifts a real system PRX
 * -- libsre, which is cellSpurs and cellSync -- loads its image into guest RAM
 * here and registers its exports. Most ports never need to: the HLE library
 * answers those modules instead.
 * -----------------------------------------------------------------------*/

#include "ppu_recomp.h"  /* ppu_context */

typedef void (*hle_ctx_fn)(ppu_context*);
extern "C" int des_title_fps_enabled(void);
extern "C" double des_title_frame_dt(double);
extern "C" uint8_t des_title_frameskip(uint8_t, uint8_t);
extern "C" void ps3_hle_register_ctx(uint32_t nid, const char* name, hle_ctx_fn fn);

/* sysPrxForUser sys_prx_register_library (NID 0x42B23552): Demon's Souls calls it during
 * init with its own export table. Nothing loads this title's PRX at run time, so there is
 * nothing to register; RPCS3 answers CELL_OK the same way. */
static void des_sys_prx_register_library(ppu_context* ctx)
{
    ctx->gpr[3] = CELL_OK;
}

/* cellSpurs cellSpursTaskAttributeSetExitCodeContainer (NID 0xA121A224): sets where a
 * task's exit code is written. The HLE SPURS task model does not report exit codes
 * through a container, so accepting the attribute is enough to let task creation go on. */
static void des_spurs_task_attr_set_exit_code_container(ppu_context* ctx)
{
    ctx->gpr[3] = ctx->gpr[3] ? CELL_OK : 0x80410911u;  /* CELL_SPURS_TASK_ERROR_NULL_POINTER */
}

/* ---------------------------------------------------------------------------
 * SPURS lock-free queue (CellSyncLFQueue), PPU side.
 *
 * Demon's Souls feeds its SPU worker tasks (taskset 0x48CBA400) through a PPU2SPU
 * LFQueue: the PPU pushes with _cellSpursLFQueuePushBody, the tasks pop with the
 * SPURS task library's own lock-line code and report through an event flag. The
 * runtime had no push at all (the call returned 0 and did nothing, so the workers
 * never got work and the flag never rose), and its initializer writes a layout
 * guessed from another title (depth at +0x0C, which is push1.m_h7/m_h8 here).
 *
 * Both are replaced with the real layout and algorithm, following RPCS3's
 * cellSync (syncLFQueueInitialize, _cellSyncLFQueueGetPushPointer,
 * _cellSyncLFQueueCompletePushPointer, _cellSyncLFQueuePushBody). Each of their
 * compare-and-swap steps runs as one transaction under the SPU lock-line lock, so
 * it cannot interleave with an SPU GETLLAR/PUTLLC on the queue line, and reserving
 * SPUs get their lost-reservation event.
 *
 *   0x00 pop1 {h1,h2,h3,h4}   0x08 push1 {h5,h6,h7,h8}   0x10 size  0x14 depth
 *   0x18 buffer (u64)         0x20 bs[4]  0x24 direction 0x28 v1    0x2C init
 *   0x30 push2.pack  0x32 hs1[15]  0x50 pop2.pack  0x52 hs2[15]
 *   0x70 eaSignal (u64)       0x78 v2     0x7C eq_id
 * -----------------------------------------------------------------------*/
extern "C" void spu_lockline_lock(void);
extern "C" void spu_lockline_unlock(void);
extern "C" int  spu_coh_is_reserved(uint32_t addr);
extern "C" void spu_coh_notify_write(uint32_t addr);
extern "C" s32  _cellSpursSendSignal(void* taskset, u32 taskId);
extern "C" volatile uint32_t g_spu_atom_watch_ea;   /* SPU atomic trace line (spu_channels.c) */

static uint16_t g16(uint32_t ea) { const uint8_t* p = vm_base + ea; return (uint16_t)(p[0] << 8 | p[1]); }
static uint32_t g32(uint32_t ea) { const uint8_t* p = vm_base + ea; return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }
static void w16(uint32_t ea, uint16_t v) { uint8_t* p = vm_base + ea; p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
static void w32(uint32_t ea, uint32_t v) { uint8_t* p = vm_base + ea; p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }

enum { LFQ_PPU2SPU = 2, LFQ_ANY2ANY = 3 };
enum : uint32_t { SYNC_ERROR_AGAIN = 0x80410101u, SYNC_ERROR_INVAL = 0x80410102u,
                  SYNC_ERROR_PERM = 0x80410109u, SYNC_ERROR_NULL_POINTER = 0x80410111u,
                  SYNC_ERROR_ALIGN = 0x80410110u };

/* Commit a lock-line transaction: tell any SPU reserving the queue line. Lock held. */
static void lfq_commit(uint32_t q)
{
    if (spu_coh_is_reserved(q)) spu_coh_notify_write(q);
}

/* DES_LFQ_WATCH=<seconds>: print every initialized queue's control words that often.
 * Queue addresses differ from run to run, so a fixed-address dump cannot follow them. */
static uint32_t s_lfq_seen[16];
static uint32_t s_lfq_last_item;   /* EA of the last pushed item (DES_LFQ_WATCH) */
static volatile long s_lfq_count;
static DWORD WINAPI des_lfq_watch_thread(LPVOID p)
{
    const unsigned secs = (unsigned)(uintptr_t)p;
    for (;;) {
        Sleep(secs * 1000u);
        for (long i = 0; i < s_lfq_count && i < 16; i++) {
            const uint32_t q = s_lfq_seen[i];
            fprintf(stderr, "[lfq] q=0x%08X pop1=%04X %04X %04X %04X push1=%04X %04X %04X %04X "
                            "push2=%04X pop2=%04X hs1=%04X %04X %04X hs2=%04X %04X %04X bs=%08X\n",
                    q, g16(q), g16(q + 2), g16(q + 4), g16(q + 6), g16(q + 8), g16(q + 10), g16(q + 12),
                    g16(q + 14), g16(q + 0x30), g16(q + 0x50), g16(q + 0x32), g16(q + 0x34), g16(q + 0x36),
                    g16(q + 0x52), g16(q + 0x54), g16(q + 0x56), g32(q + 0x20));
            const uint32_t fl = q - 0x100;   /* the event flag sits one line below the queue */
            fprintf(stderr, "[lfq]   flag=0x%08X %08X %08X %08X %08X %08X %08X %08X %08X\n", fl,
                    g32(fl), g32(fl + 4), g32(fl + 8), g32(fl + 12), g32(fl + 16), g32(fl + 20), g32(fl + 24), g32(fl + 28));
        }
        { const uint32_t it = s_lfq_last_item;
          if (it) { const uint32_t obj = g32(it + 16) & ~0x7Fu, flg = g32(it + 20) & ~0x7Fu;
            fprintf(stderr, "[lfq] item=0x%08X: %08X %08X %08X %08X %08X %08X %08X %08X\n", it,
                    g32(it), g32(it + 4), g32(it + 8), g32(it + 12), g32(it + 16), g32(it + 20), g32(it + 24), g32(it + 28));
            { const uint32_t dst = g32(it + 4), len = g32(it + 12);
              fprintf(stderr, "[lfq]   dst=0x%08X len=0x%X words: +0 %08X +4000 %08X +8000 %08X +C000 %08X end-16 %08X\n", dst, len,
                      g32(dst), len > 0x4000 ? g32(dst + 0x4000) : 0, len > 0x8000 ? g32(dst + 0x8000) : 0, len > 0xC000 ? g32(dst + 0xC000) : 0, g32(dst + len - 16)); }
            const uint32_t lines[2] = { obj, flg };
            for (uint32_t ln : lines) if (ln)
              fprintf(stderr, "[lfq]   line 0x%08X: %08X %08X %08X %08X %08X %08X %08X %08X\n", ln,
                      g32(ln), g32(ln + 4), g32(ln + 8), g32(ln + 12), g32(ln + 16), g32(ln + 20), g32(ln + 24), g32(ln + 28)); } }
        fflush(stderr);
    }
}

/* ---------------------------------------------------------------------------
 * Host-side Edge zlib decompression.
 *
 * Core.Res.Decompressor hands each 64 KB segment of a .dcx ("DCP EDGE") file to
 * five SPU worker tasks through the LFQueue. Lifted, one worker now and then
 * never returns from inflate (run-dependent: a different segment each time), its
 * stream counter never reaches zero, the event flag never rises and the loader
 * waits forever. The work itself is plain raw deflate, so it is done here
 * instead, on the pushing thread, with the worker's completion protocol (read
 * from the worker's code at LS 0x8D04..0x8D5C and 0xC038):
 *
 *   item: +0 src  +4 dst  +8 src size  +C dst size  +10 stream|1  +14 flag  +18 bits<<16
 *   after writing dst: if stream: old = stream.count--, and old == 1 sets `bits`
 *   on `flag`; without a stream the flag is set directly.
 *
 * Only queues initialized from the decompressor (lr 0x00C98D8C) are taken over;
 * DES_SPU_INFLATE=1 leaves them on the SPU.
 * -----------------------------------------------------------------------*/
extern "C" int stbi_zlib_decode_noheader_buffer(char* obuffer, int olen, const char* ibuffer, int ilen);
extern "C" s32 cellSpursEventFlagSet(void* eventFlag, u16 bits);
extern "C" uint32_t prx_resolve_export(uint32_t nid);
extern "C" uint64_t ppu_guest_call(uint32_t opd, uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3,
                                   uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
extern "C" int des_libsre_load(const char* image_path);   /* lle/libsre_glue.cpp */
static uint32_t s_inflate_queues[256];   /* the decompressor re-creates its queues: 20+ per boot */
static long s_inflate_queue_count;

static bool des_is_inflate_queue(uint32_t q)
{
    for (long i = 0; i < s_inflate_queue_count; i++) if (s_inflate_queues[i] == q) return true;
    return false;
}

/* An SPU task's cellSpursEventFlagSet on an SPU2PPU flag, done from the host.
 * The decompressor's flags are SPU2PPU, so libsre's PPU-side Set refuses them
 * (CELL_SPURS_TASK_ERROR_PERM) and its PPU-side Wait parks on eventQueueId until
 * an SPU posts there. This is what the SPU side does, on the layout RPCS3 spells
 * out (cellSpurs.h CellSpursEventFlag):
 *   ctrl (u64 @0) = events u16 | spuTaskPendingRecv u16 | ppuWaitMask u16 |
 *                   ppuWaitSlotAndMode u8 | ppuPendingRecv u8
 *   0x0F clearMode, 0x30 pendingRecvTaskEvents[16], 0x7C eventQueueId.
 * A PPU waiter whose condition the new bits meet gets them in pendingRecv[0],
 * ppuPendingRecv = 1 and one event on its queue (libsre's wait then clears
 * ppuPendingRecv itself). The ctrl word is committed with a 64-bit CAS so a
 * concurrent libsre ldarx/stdcx. on it sees the change and retries. */
extern "C" int sys_event_queue_push_by_id(uint32_t queue_id, uint64_t source, uint64_t d1, uint64_t d2, uint64_t d3);
static void des_spu_side_event_flag_set(uint32_t flag, u16 bits)
{
    volatile uint64_t* ctrl = (volatile uint64_t*)(vm_base + flag);
    bool notify = false;
    for (;;) {
        uint64_t raw_old = *ctrl;
        const uint64_t old = __builtin_bswap64(raw_old);
        u16 events = (u16)(old >> 48);
        const u16 spu_pending = (u16)(old >> 32);
        u16 wait_mask = (u16)(old >> 16);
        const uint8_t slot_mode = (uint8_t)(old >> 8);
        uint8_t ppu_pending = (uint8_t)old;
        events |= bits;
        notify = false;
        if (wait_mask && !ppu_pending) {
            const u16 rel = events & wait_mask;
            const bool and_mode = (slot_mode & 0x0F) == 1;   /* CELL_SPURS_EVENT_FLAG_AND */
            if ((wait_mask & ~rel) == 0 || (!and_mode && rel)) {
                w16(flag + 0x30, rel);                        /* pendingRecvTaskEvents[0] */
                if (vm_base[flag + 0x0F] == 0) events &= ~rel; /* CELL_SPURS_EVENT_FLAG_CLEAR_AUTO */
                wait_mask = 0;
                ppu_pending = 1;
                notify = true;
            }
        }
        const uint64_t nv = ((uint64_t)events << 48) | ((uint64_t)spu_pending << 32) |
                            ((uint64_t)wait_mask << 16) | ((uint64_t)slot_mode << 8) | ppu_pending;
        uint64_t raw_new = __builtin_bswap64(nv);
        if (__atomic_compare_exchange_n(ctrl, &raw_old, raw_new, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
            break;
    }
    if (spu_coh_is_reserved(flag)) { spu_lockline_lock(); spu_coh_notify_write(flag); spu_lockline_unlock(); }
    int rc = 0;
    if (notify) rc = sys_event_queue_push_by_id(g32(flag + 0x7C), 0xFFFFFFFF53505501ull, 0, 0, 0);
    { static int lg = -1; if (lg < 0) lg = getenv("DES_INFLATE_LOG") ? 1 : 0;
      if (lg) fprintf(stderr, "[des-flag] set 0x%08X bits=0x%04X -> ctrl=%016llX notify=%d queue=%u rc=%d\n", flag,
                     (unsigned)bits, (unsigned long long)__builtin_bswap64(*ctrl), notify ? 1 : 0, g32(flag + 0x7C), rc); }
}

static uint32_t be32h(const uint8_t* p) { return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }

/* item is a host copy of the 32-byte queue entry. */
static void des_host_inflate(const uint8_t* item)
{
    const uint64_t perf_begin = ps3_perf_trace_begin();
    const uint32_t src = be32h(item), dst = be32h(item + 4), slen = be32h(item + 8), dlen = be32h(item + 12);
    const uint32_t stream = be32h(item + 16) & ~1u, flag = be32h(item + 20);
    const u16 bits = (u16)(be32h(item + 24) >> 16);

    if (slen == dlen) {
        memmove(vm_base + dst, vm_base + src, dlen);              /* stored segment */
    } else {
        const int got = stbi_zlib_decode_noheader_buffer((char*)vm_base + dst, (int)dlen,
                                                         (const char*)vm_base + src, (int)slen);
        static int bad;
        if (got != (int)dlen && bad++ < 16)
            printf("[des] inflate src=0x%08X len=0x%X -> got %d of 0x%X\n", src, slen, got, dlen);
    }

    ps3_perf_trace_end("inflate-data", perf_begin, slen, dlen);
    const uint64_t signal_begin = ps3_perf_trace_begin();
    bool set = !stream;
    if (stream) {
        spu_lockline_lock();
        const uint32_t old = g32(stream);
        w32(stream, old - 1);
        if (spu_coh_is_reserved(stream)) spu_coh_notify_write(stream);
        spu_lockline_unlock();
        set = old == 1;
        { static int lg = -1; if (lg < 0) lg = getenv("DES_INFLATE_LOG") ? 1 : 0;
          if (lg) fprintf(stderr, "[des-inflate] dst=0x%08X len=0x%X stream=0x%08X count %u->%u flag=0x%08X\n",
                         dst, dlen, stream, old, old - 1, flag); }
        static int zero;
        if (old == 0 && zero++ < 8) printf("[des] inflate: stream 0x%08X counter was already 0\n", stream);
    }
    if (set && flag) {
        /* With the real SPURS kernel loaded the PPU waiter is libsre's own
         * cellSpursEventFlagWait, which sleeps on an lv2 event queue that only
         * the real Set posts to -- so go through it, not the HLE body. */
        const uint32_t real_set = prx_resolve_export(0xF5507729u);   /* cellSpursEventFlagSet */
        if (real_set) des_spu_side_event_flag_set(flag, bits);   /* SPU2PPU: the SPU's side of Set */
        else          cellSpursEventFlagSet((void*)(uintptr_t)flag, bits);
        static int n;
        if (n++ < 8) printf("[des] inflate done: %s set flag 0x%08X bits 0x%04X\n",
                            real_set ? "SPU-side" : "HLE", flag, (unsigned)bits);
    }
    ps3_perf_trace_end("inflate-signal", signal_begin, stream, flag);
}

static void des_spurs_lfqueue_initialize(ppu_context* ctx)
{
    if ((uint32_t)ctx->lr == 0x00C98D8Cu && !des_is_inflate_queue((uint32_t)ctx->gpr[4]) &&
        s_inflate_queue_count < 256)
        s_inflate_queues[s_inflate_queue_count++] = (uint32_t)ctx->gpr[4];
    { static int watch = -1;
      if (watch < 0) { const char* e = getenv("DES_LFQ_WATCH"); watch = e ? atoi(e) : 0;
                       if (watch > 0) CreateThread(NULL, 0, des_lfq_watch_thread, (LPVOID)(uintptr_t)watch, 0, NULL); }
      const uint32_t qa = (uint32_t)ctx->gpr[4]; bool known = false;
      for (long i = 0; i < s_lfq_count; i++) if (s_lfq_seen[i] == qa) known = true;
      if (!known && s_lfq_count < 16) s_lfq_seen[s_lfq_count++] = qa; }
    const uint32_t owner = (uint32_t)ctx->gpr[3], q = (uint32_t)ctx->gpr[4], buffer = (uint32_t)ctx->gpr[5];
    const uint32_t size = (uint32_t)ctx->gpr[6], depth = (uint32_t)ctx->gpr[7], dir = (uint32_t)ctx->gpr[8];
    if (!q || (size && !buffer)) { ctx->gpr[3] = SYNC_ERROR_NULL_POINTER; return; }
    if (size > 0x4000 || size % 16 || !depth || depth > 0x7FFF || dir > 3) { ctx->gpr[3] = SYNC_ERROR_INVAL; return; }
    if (q % 128 || buffer % 16) { ctx->gpr[3] = SYNC_ERROR_ALIGN; return; }

    spu_lockline_lock();
    memset(vm_base + q, 0, 128);
    w32(q + 0x10, size);
    w32(q + 0x14, depth);
    w32(q + 0x18, 0); w32(q + 0x1C, dir == LFQ_ANY2ANY ? buffer | 1 : buffer);
    w32(q + 0x24, dir);
    if (dir == LFQ_ANY2ANY) {
        vm_base[q + 0x20] = 0xFF; vm_base[q + 0x21] = 0xFF;
        w32(q + 0x28, 0xFFFFFFFFu);
        w16(q + 0x30, 0xFFFF); w16(q + 0x50, 0xFFFF);
    } else {
        w32(q + 0x20, 0xFFFFFFFFu);   /* bs[0..3] = -1, v1/push2/pop2 stay 0 */
    }
    w32(q + 0x70, 0); w32(q + 0x74, owner);   /* eaSignal: the taskset */
    lfq_commit(q);
    spu_lockline_unlock();
    printf("[des] LFQueueInitialize(owner=0x%08X q=0x%08X buf=0x%08X size=%u depth=%u dir=%u) lr=0x%08X tid=%u\n",
           owner, q, buffer, size, depth, dir, (uint32_t)ctx->lr, (unsigned)ctx->thread_id);
    ctx->gpr[3] = CELL_OK;
}

/* _cellSyncLFQueueGetPushPointer, PPU2SPU, without the event queue. */
static uint32_t lfq_get_push_pointer(uint32_t q, int32_t* pointer, uint32_t isBlocking)
{
    const int32_t depth = (int32_t)g32(q + 0x14);
    for (;;) {
        spu_lockline_lock();
        const int16_t h8 = (int16_t)g16(q + 0x0E);
        int32_t var2 = h8 - (int32_t)g16(q + 0x00);         /* - pop1.m_h1 */
        if (var2 < 0) var2 += depth * 2;
        if (var2 < depth) {
            *pointer = h8;
            w16(q + 0x0E, (uint16_t)(h8 + 1 >= depth * 2 ? 0 : h8 + 1));
            lfq_commit(q);
            spu_lockline_unlock();
            return CELL_OK;
        }
        spu_lockline_unlock();
        if (!isBlocking) return SYNC_ERROR_AGAIN;
        Sleep(0);                                            /* queue full: let the SPU pop */
    }
}

/* _cellSyncLFQueueCompletePushPointer, PPU2SPU. Returns the task to signal, or -1. */
static int32_t lfq_complete_push_pointer(uint32_t q, int32_t pointer)
{
    const int32_t depth = (int32_t)g32(q + 0x14);
    for (;;) {
        spu_lockline_lock();
        uint16_t pack = g16(q + 0x30);                       /* push2: three packed 5-bit fields */
        const uint16_t h5 = g16(q + 0x08), h6 = g16(q + 0x0A);  /* push3 */

        int32_t var1 = pointer - h5;
        if (var1 < 0) var1 += depth * 2;
        int32_t var2 = (int16_t)g16(q + 0x06) - (int32_t)g16(q + 0x00);   /* pop1.m_h4 - pop1.m_h1 */
        if (var2 < 0) var2 += depth * 2;

        int32_t var9_ = 15 - var1;
        var9_ = (var9_ & 0x30) ? 0 : 1 << var9_;
        const int32_t var9 = __builtin_clz((uint32_t)(uint16_t)~(var9_ | h6)) - 16;
        int32_t var5 = h6 | var9_;
        var5 = (var9 & 0x30) ? 0 : var5 << var9;
        int32_t var3 = h5 + var9;
        if (var3 >= depth * 2) var3 -= depth * 2;

        int32_t var4 = ((pack >> 10) & 0x1f) - ((pack >> 5) & 0x1f);
        if (var4 < 0) var4 += 0x1e;

        int32_t var6 = -1;
        if (var2 + var4 <= 15 && ((pack >> 10) & 0x1f) != (pack & 0x1f)) {
            int32_t var11 = (pack >> 10) & 0x1f;
            if (var11 >= 15) var11 -= 15;
            uint16_t var12 = (pack >> 10) & 0x1f;
            var12 = var12 == 0x1d ? 0 : (uint16_t)((var12 + 1) << 10);
            pack = (uint16_t)((pack & 0x83ff) | var12);
            var6 = g16(q + 0x32 + 2 * var11);                /* hs1[var11]: the waiting task */
        }

        /* RPCS3 commits push3 only when it signals, or once push2's first and third
         * fields agree (no pop waiter left to wake); otherwise it re-reads and retries. */
        const bool done = var6 != -1 || (pack & 0x1f) == ((pack >> 10) & 0x1f);
        w16(q + 0x30, pack);
        if (done) { w16(q + 0x08, (uint16_t)var3); w16(q + 0x0A, (uint16_t)var5); }
        lfq_commit(q);
        spu_lockline_unlock();
        if (done) return var6;
        Sleep(0);                                            /* a pop is in flight: let it finish */
    }
}

/* The work runs on a host thread, as it would on an SPU: completing it inside
 * the push (synchronously) signalled the stream before the decompressor had
 * registered it, and Dantelion panicked "pWork could not be found"
 * (Decompressor.cpp 421) / "invalid parameter on pUnit" (241). Items are copied
 * at push time because the caller reuses its buffer. */
struct des_inflate_job { uint8_t item[32]; };
static des_inflate_job  s_inflate_ring[256];
static unsigned         s_inflate_head, s_inflate_tail;
static CRITICAL_SECTION s_inflate_cs;
static CONDITION_VARIABLE s_inflate_cv;

static DWORD WINAPI des_inflate_worker(LPVOID)
{
    for (;;) {
        des_inflate_job job;
        EnterCriticalSection(&s_inflate_cs);
        while (s_inflate_head == s_inflate_tail) SleepConditionVariableCS(&s_inflate_cv, &s_inflate_cs, INFINITE);
        job = s_inflate_ring[s_inflate_tail++ % 256];
        LeaveCriticalSection(&s_inflate_cs);
        des_host_inflate(job.item);
    }
}

static void des_inflate_enqueue(uint32_t item_ea)
{
    static bool started;
    if (!started) {
        started = true;
        InitializeCriticalSection(&s_inflate_cs);
        InitializeConditionVariable(&s_inflate_cv);
        CreateThread(NULL, 0, des_inflate_worker, NULL, 0, NULL);
    }
    for (;;) {
        EnterCriticalSection(&s_inflate_cs);
        if (s_inflate_head - s_inflate_tail < 256) {
            memcpy(s_inflate_ring[s_inflate_head++ % 256].item, vm_base + item_ea, 32);
            WakeConditionVariable(&s_inflate_cv);
            LeaveCriticalSection(&s_inflate_cs);
            return;
        }
        LeaveCriticalSection(&s_inflate_cs);
        Sleep(0);
    }
}

static void des_spurs_lfqueue_push_body(ppu_context* ctx)
{
    { static int spu = -1; if (spu < 0) spu = getenv("DES_SPU_INFLATE") ? 1 : 0;
      const uint32_t q0 = (uint32_t)ctx->gpr[3], b0 = (uint32_t)ctx->gpr[4];
      if (!spu && q0 && b0 && des_is_inflate_queue(q0)) {
          static int n; if (n++ < 4) printf("[des] host inflate src=0x%08X dst=0x%08X\n", g32(b0), g32(b0 + 4));
          des_inflate_enqueue(b0);
          ctx->gpr[3] = CELL_OK;
          return;
      } }
    const uint32_t q = (uint32_t)ctx->gpr[3], buffer = (uint32_t)ctx->gpr[4], isBlocking = (uint32_t)ctx->gpr[5];
    if (!q || !buffer) { ctx->gpr[3] = SYNC_ERROR_NULL_POINTER; return; }
    if (q % 128 || buffer % 16) { ctx->gpr[3] = SYNC_ERROR_ALIGN; return; }
    if (g32(q + 0x24) != LFQ_PPU2SPU) { ctx->gpr[3] = SYNC_ERROR_PERM; return; }

    int32_t pos = 0;
    const uint32_t res = lfq_get_push_pointer(q, &pos, isBlocking);
    if (res) { static int nf; if (nf++ < 50) printf("[des] LFQueuePush q=0x%08X isBlocking=%u FAILED 0x%08X\n", q, isBlocking, res);
               ctx->gpr[3] = res; return; }

    const int32_t depth = (int32_t)g32(q + 0x14), size = (int32_t)g32(q + 0x10);
    const uint32_t slot = (g32(q + 0x1C) & ~1u) + (uint32_t)(size * (pos >= depth ? pos - depth : pos));
    memcpy(vm_base + slot, vm_base + buffer, (size_t)size);
    s_lfq_last_item = slot;
    { static int follow = -1; if (follow < 0) { const char* e = getenv("DES_ATOM_FOLLOW"); follow = e ? (e[0] == 'o' ? 2 : 1) : 0; }
      if (follow) g_spu_atom_watch_ea = g32(slot + (follow == 2 ? 16 : 20)) & ~0x7Fu; }   /* the item's stream object (o) or event flag */

    const int32_t task = lfq_complete_push_pointer(q, pos);
    static int n;
    if (n++ < 400) printf("[des] LFQueuePush item=%08X %08X obj=0x%08X cnt=%u slot=0x%08X q=0x%08X pos=%d -> signal task %d push1=%04X %04X %04X %04X push2=%04X pop1.h1=%04X tid=%u\n",
                          g32(slot), g32(slot + 4), g32(slot + 16) & ~1u, g32(g32(slot + 16) & ~1u), slot, q, pos, task, g16(q + 8), g16(q + 10), g16(q + 12), g16(q + 14), g16(q + 0x30), g16(q), (unsigned)ctx->thread_id);
    if (task != -1) _cellSpursSendSignal((void*)(uintptr_t)g32(q + 0x74), (u32)task);
    ctx->gpr[3] = CELL_OK;
}

/* DES_DUMP=<hex ea>:<hex len>:<seconds>: after that many seconds, print the guest
 * memory range as big-endian words. For looking at what an SPU or the RSX left in
 * memory once the title has settled into a wait. */
extern "C" uint8_t* vm_base;
static DWORD WINAPI des_dump_thread(LPVOID)
{
    /* Several ranges: DES_DUMP=<ea>:<len>:<s>[,<ea>:<len>]... (the delay comes from the first). */
    const char* spec = getenv("DES_DUMP");
    unsigned ea = 0, len = 0, secs = 0;
    if (sscanf(spec, "%x:%x:%u", &ea, &len, &secs) < 2) return 0;
    Sleep((secs ? secs : 20) * 1000u);
    for (const char* s = spec; s && *s; ) {
        if (sscanf(s, "%x:%x", &ea, &len) == 2)
            for (unsigned o = 0; o < len; o += 16) {
                const uint8_t* p = vm_base + ea + o;
                fprintf(stderr, "[des-dump] %08X: %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X\n",
                        ea + o, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7],
                        p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
            }
        s = strchr(s, ',');
        if (s) s++;
    }
    /* DES_SCAN=<hex word>:<hex ea>:<hex len>: list every aligned occurrence of the
     * word in that range (e.g. which command buffer holds a given JUMP). */
    { const char* sc = getenv("DES_SCAN"); unsigned w = 0, sea = 0, slen = 0;
      if (sc && sscanf(sc, "%x:%x:%x", &w, &sea, &slen) == 3)
          for (unsigned o = 0; o + 4 <= slen; o += 4)
              if (g32(sea + o) == w)
                  fprintf(stderr, "[des-scan] %08X at 0x%08X (prev %08X %08X)\n", w, sea + o,
                          o >= 8 ? g32(sea + o - 8) : 0, o >= 4 ? g32(sea + o - 4) : 0); }
    /* DES_SAVE=<hex ea>:<hex len>:<file>: also write that range raw, for offline scans. */
    { const char* sv = getenv("DES_SAVE"); unsigned sea = 0, slen = 0; char path[260] = {0};
      if (sv && sscanf(sv, "%x:%x:%259s", &sea, &slen, path) == 3) {
          FILE* f = fopen(path, "wb");
          if (f) { fwrite(vm_base + sea, 1, slen, f); fclose(f);
                   fprintf(stderr, "[des-save] 0x%08X+0x%X -> %s\n", sea, slen, path); } } }
    fflush(stderr);
    return 0;
}

/* DES_A2O_LR=<hex lo>:<hex hi>: log the guest caller of cellGcmAddressToOffset for
 * EAs in [lo,hi) -- whoever asks for an offset is about to point the RSX at it. */
extern "C" int32_t cellGcmAddressToOffset(uint32_t address, uint32_t* offset);
static void des_a2o(ppu_context* ctx)
{
    static unsigned lo = 1, hi;
    if (lo == 1) { const char* e = getenv("DES_A2O_LR"); lo = 0; hi = 0;
                   if (e) sscanf(e, "%x:%x", &lo, &hi); }
    const uint32_t ea = (uint32_t)ctx->gpr[3];
    if (ea >= lo && ea < hi)
        fprintf(stderr, "[des-a2o] ea=0x%08X lr=0x%08X r31=0x%08X tid=%lu\n", ea, (unsigned)ctx->lr,
                (unsigned)ctx->gpr[31], GetCurrentThreadId());
    ctx->gpr[3] = (uint64_t)(int64_t)cellGcmAddressToOffset(ea, (uint32_t*)(uintptr_t)(uint32_t)ctx->gpr[4]);
}

/* DES_HEAPDUMP=<hex heap>:<seconds>[:<period s>]: walk one of the title's heaps
 * (allocator func_00AC8078) and report free bytes, block count and the largest
 * free block. Layout read off the allocator: +0x1C..+0x20 = free-list bins
 * (8-byte sentinels; a node links through node+8, its successor in address
 * order is at node+4), +0x28 = per-block overhead. */
static DWORD WINAPI des_heapdump_thread(LPVOID)
{
    unsigned h = 0, secs = 0, per = 0;
    if (sscanf(getenv("DES_HEAPDUMP"), "%x:%u:%u", &h, &secs, &per) < 2) return 0;
    Sleep(secs * 1000u);
    for (;;) {
        const uint32_t b0 = g32(h + 0x1C), b1 = g32(h + 0x20), ovh = g32(h + 0x28);
        uint64_t free_total = 0; uint32_t largest = 0, nfree = 0, bins = 0;
        for (uint32_t b = b0; b < b1 && bins < 4096; b += 8, bins++) {
            uint32_t link = g32(b + 4), guard = 0;
            while (link && link != b && guard++ < 1000000) {
                const uint32_t node = link - 8;
                const uint32_t size = g32(node + 4) - node - ovh;
                free_total += size; nfree++;
                if (size > largest) largest = size;
                if (size >= 0x100000)
                    fprintf(stderr, "[des-heap]   bin %u (sentinel 0x%X) node 0x%X size 0x%X next 0x%X\n",
                            bins, b, node, size, g32(node + 4));
                link = g32(link + 4);
            }
        }
        fprintf(stderr, "[des-heap] 0x%X bins=%u free=%llu (%.1f MB) blocks=%u largest=0x%X ovh=0x%X hdr:",
                h, bins, (unsigned long long)free_total, free_total / 1048576.0, nfree, largest, ovh);
        for (int k = 0; k < 0x40; k += 4) fprintf(stderr, " %08X", g32(h + k));
        fprintf(stderr, "\n");
        if (!per) break;
        Sleep(per * 1000u);
    }
    return 0;
}

/* DES_WATCH=<hex ea>:<hex len>[:<seconds>]: log every guest write into that range
 * with the writing lifted function (and its callers), by write-protecting the host
 * page and single-stepping the faulting store. For "who fills this buffer" --
 * e.g. the fragment program the final pass reads, which stays zero. Other writes
 * to the same page pay the trap too, so keep the window small. */
extern "C" uint32_t ppu_guest_func_for_rip(uintptr_t rip);
static uintptr_t s_watch_page, s_watch_lo, s_watch_hi;
static volatile LONG s_watch_hits;
static __declspec(thread) int t_watch_rearm;
static __declspec(thread) uintptr_t t_watch_addr;   /* logged store, value printed after it */

static LONG CALLBACK des_watch_handler(EXCEPTION_POINTERS* x)
{
    const DWORD code = x->ExceptionRecord->ExceptionCode;
    if (code == EXCEPTION_ACCESS_VIOLATION && x->ExceptionRecord->ExceptionInformation[0] == 1) {
        const uintptr_t a = (uintptr_t)x->ExceptionRecord->ExceptionInformation[1];
        if ((a & ~(uintptr_t)0xFFF) != s_watch_page) return EXCEPTION_CONTINUE_SEARCH;
        static LONG s_watch_max = -1;
        if (s_watch_max < 0) { const char* e = getenv("DES_WATCH_MAX"); s_watch_max = e ? atol(e) : 400; }
        if (a >= s_watch_lo && a < s_watch_hi && InterlockedIncrement(&s_watch_hits) <= s_watch_max) {
            void* bt[12]; const USHORT nf = RtlCaptureStackBackTrace(0, 12, bt, NULL);
            LARGE_INTEGER qf, qt; QueryPerformanceFrequency(&qf); QueryPerformanceCounter(&qt);
            char line[512]; int o = snprintf(line, sizeof line, "[des-watch] t=%.3fms write ea=0x%08X tid=%lu rip=func_%08X <-",
                (double)qt.QuadPart * 1000.0 / (double)qf.QuadPart,
                (unsigned)(a - (uintptr_t)vm_base), GetCurrentThreadId(),
                ppu_guest_func_for_rip((uintptr_t)x->ContextRecord->Rip));
            for (USHORT i = 0; i < nf && o < (int)sizeof line - 16; i++) {
                const uint32_t g = ppu_guest_func_for_rip((uintptr_t)bt[i]);
                if (g) o += snprintf(line + o, sizeof line - o, " %08X", g);
            }
            /* Host writers (the RSX walker's label writes, SPU DMA): exe RVAs
             * of the faulting store and its callers, for the link map. */
            { const uintptr_t base = (uintptr_t)GetModuleHandleA(NULL);
              o += snprintf(line + o, sizeof line - o, " | host rva:");
              const uintptr_t rip = (uintptr_t)x->ContextRecord->Rip;
              if (rip - base < 0x40000000u) o += snprintf(line + o, sizeof line - o, " %llX", (unsigned long long)(rip - base));
              for (USHORT i = 0; i < nf && i < 8 && o < (int)sizeof line - 16; i++)
                  if ((uintptr_t)bt[i] - base < 0x40000000u)
                      o += snprintf(line + o, sizeof line - o, " %llX", (unsigned long long)((uintptr_t)bt[i] - base)); }
            fprintf(stderr, "%s\n", line);
            t_watch_addr = a;
        }
        DWORD old; VirtualProtect((void*)s_watch_page, 0x1000, PAGE_READWRITE, &old);
        x->ContextRecord->EFlags |= 0x100;   /* trap after the store, then re-arm */
        t_watch_rearm = 1;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (code == EXCEPTION_SINGLE_STEP && t_watch_rearm) {
        t_watch_rearm = 0;
        if (t_watch_addr) {
            const uint8_t* q = (const uint8_t*)(t_watch_addr & ~(uintptr_t)3);
            fprintf(stderr, "[des-watch]   -> 0x%08X now %02X%02X%02X%02X\n",
                    (unsigned)((uintptr_t)q - (uintptr_t)vm_base), q[0], q[1], q[2], q[3]);
            t_watch_addr = 0;
        }
        DWORD old; VirtualProtect((void*)s_watch_page, 0x1000, PAGE_READONLY, &old);
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static DWORD WINAPI des_watch_thread(LPVOID)
{
    unsigned ea = 0, len = 0, secs = 0;
    if (sscanf(getenv("DES_WATCH"), "%x:%x:%u", &ea, &len, &secs) < 2) return 0;
    Sleep((secs ? secs : 2) * 1000u);
    ppu_guest_func_for_rip(0);   /* build the lookup map outside the handler */
    s_watch_lo = (uintptr_t)vm_base + ea; s_watch_hi = s_watch_lo + len;
    s_watch_page = s_watch_lo & ~(uintptr_t)0xFFF;
    AddVectoredExceptionHandler(1, des_watch_handler);
    DWORD old; VirtualProtect((void*)s_watch_page, 0x1000, PAGE_READONLY, &old);
    fprintf(stderr, "[des-watch] armed 0x%08X..0x%08X\n", ea, ea + len);
    return 0;
}

extern "C" void spu_overlay_register_source(uint32_t content_ea, int image_id);
extern "C" void spu_taskset_register_task_elf(uint32_t elf_ea, int image_id, int policy_image_id);
extern "C" void ps3_hle_register(uint32_t nid, const char* name, void* handler);
extern "C" {
int cellAtracGetStreamDataInfo(uint32_t, uint32_t, uint32_t, uint32_t);
int cellAtracAddStreamData(uint32_t, uint32_t);
int cellAtracIsSecondBufferNeeded(uint32_t);
int cellAtracGetSecondBufferInfo(uint32_t, uint32_t, uint32_t);
int cellAtracSetSecondBuffer(uint32_t, uint32_t, uint32_t);
}

/* DES_FPS_UNLOCK=1: RPCS3's "Unlock FPS" patch for BLUS30443 (Gibbed,
 * Whatcookie), wired into the lifted code by scripts/apply_fps_unlock.py.
 * The game normally runs its update at a fixed 1/30 s step with frameskip;
 * the patch clears the frameskip flag and feeds the update the real frame
 * time, clamped to 50 ms like the patch's code cave. */
/* (Diagnostic: =2 applies only the frameskip flag, =3 only the delta time.) */
extern "C" uint8_t des_fps_frameskip_flag(uint8_t orig, uint8_t unlocked)
{
    return des_title_frameskip(orig, unlocked);
}
extern "C" double des_fps_frame_dt(double orig)
{
    return des_title_frame_dt(orig);
}

/* DES_FIND=<hex bytes>:<seconds>[:<repeat seconds>]: after that many seconds,
 * scan the guest's committed memory for the byte string and print each hit
 * with the 48 bytes that follow. For "where else does this vertex data live". */
static DWORD WINAPI des_find_thread(LPVOID)
{
    char pat_hex[256] = {0}; unsigned secs = 0, rep = 0;
    /* "??" in the pattern matches any byte. */
    if (sscanf(getenv("DES_FIND"), "%255[0-9A-Fa-f?]:%u:%u", pat_hex, &secs, &rep) < 2) return 0;
    uint8_t pat[128], any[128]; size_t n = 0;
    for (size_t i = 0; pat_hex[i] && pat_hex[i + 1] && n < sizeof pat; i += 2) {
        any[n] = pat_hex[i] == '?';
        unsigned b = 0; if (!any[n]) sscanf(pat_hex + i, "%2x", &b); pat[n++] = (uint8_t)b;
    }
    size_t first = 0; while (first < n && any[first]) first++;
    if (first == n) return 0;
    if (!n) return 0;
    for (;;) {
        Sleep(secs * 1000u);
        int hits = 0;
        for (uint64_t ea = 0; ea < 0x100000000ull && hits < 32;) {
            MEMORY_BASIC_INFORMATION mi;
            if (!VirtualQuery(vm_base + ea, &mi, sizeof mi)) break;
            const uint64_t region_end = (uint64_t)((uint8_t*)mi.BaseAddress + mi.RegionSize - vm_base);
            if (mi.State == MEM_COMMIT && !(mi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) {
                const uint8_t* p = vm_base + ea;
                const uint8_t* end = vm_base + region_end;
                for (; p + n + 48 <= end && hits < 32; p++)
                    if (p[first] == pat[first] && [&] { for (size_t k = 0; k < n; k++)
                            if (!any[k] && p[k] != pat[k]) return false; return true; }()) {
                        hits++;
                        fprintf(stderr, "[des-find] %08llX:", (unsigned long long)(p - vm_base));
                        for (int k = 0; k < 48; k++) fprintf(stderr, "%s%02X", (k % 4) ? "" : " ", p[k]);
                        fprintf(stderr, "\n");
                    }
            }
            ea = region_end;
        }
        fprintf(stderr, "[des-find] scan done, %d hit(s)\n", hits);
        if (!rep) break;
        secs = rep;
    }
    return 0;
}

extern "C" void ps3_load_prx_modules(void)
{
    if (getenv("DES_FIND")) CreateThread(NULL, 0, des_find_thread, NULL, 0, NULL);
    if (getenv("DES_WATCH")) CreateThread(NULL, 0, des_watch_thread, NULL, 0, NULL);
    if (getenv("DES_HEAPDUMP")) CreateThread(NULL, 0, des_heapdump_thread, NULL, 0, NULL);
    /* DES_ONE_LOG=1: send stdout (the title's TTY, the HLE modules) into stderr,
     * unbuffered, so both land in one log in the order they happened -- and a
     * killed run does not lose its buffered stdout tail. */
    if (getenv("DES_ONE_LOG")) { fflush(stdout); _dup2(_fileno(stderr), _fileno(stdout)); setvbuf(stdout, NULL, _IONBF, 0); }
    if (getenv("DES_DUMP")) CreateThread(NULL, 0, des_dump_thread, NULL, 0, NULL);
    /* cellAtrac streaming (the BGM): these NIDs were unresolved and returned 0
     * without filling their outputs, which corrupted MagicOrchestra's stream
     * state ("Stream Memory Free State Error" panic). */
    ps3_hle_register(0x2BFFF084u, "cellAtracGetStreamDataInfo", (void*)cellAtracGetStreamDataInfo);
    ps3_hle_register(0x46CFC013u, "cellAtracAddStreamData", (void*)cellAtracAddStreamData);
    ps3_hle_register(0x99EFE171u, "cellAtracIsSecondBufferNeeded", (void*)cellAtracIsSecondBufferNeeded);
    ps3_hle_register(0xBE07F05Eu, "cellAtracGetSecondBufferInfo", (void*)cellAtracGetSecondBufferInfo);
    ps3_hle_register(0x06DDB53Eu, "cellAtracSetSecondBuffer", (void*)cellAtracSetSecondBuffer);
#ifdef DES_HAVE_LIBSRE
    /* DES_LIBSRE=1 (or a path to the relocated image): run the real SPURS kernel
     * lifted from the firmware (lle/libsre_glue.cpp). */
    if (const char* sre = getenv("DES_LIBSRE")) {
        const char* path = (sre[0] && strcmp(sre, "1") != 0) ? sre : "../lle/out/libsre_image.bin";
        printf("[des] libsre LLE %s (%s)\n", des_libsre_load(path) ? "loaded" : "FAILED", path);
        /* The SPURS kernel DMAs three policy modules out of libsre into LS 0xA00
         * at runtime. They are lifted from p2/jobs/sre_pm*.elf; register their
         * source EAs (libsre at 0x30000000, file offset - 0x100) so a GET of one
         * makes its image resident instead of interpreting it -- interpretation
         * took ~4 host cores and held the game at ~9 fps. */
        { spu_overlay_register_source(0x30021480u, 519547630);   /* pmA, LS 0xA00..0x2B80 */
          spu_overlay_register_source(0x30023680u, 353941178);   /* pmB, LS 0xA00..0x2800 */
          spu_overlay_register_source(0x3002A180u, 321163202); } /* pmC, LS 0xA00..0x4800 */
        /* The taskset policy (pmB) loads the title's SPURS task ELFs (embedded in
         * the EBOOT, all based at LS 0x3000) by TaskInfo.elf. Map each ELF EA to
         * its lifted image so the tasks dispatch lifted instead of interpreted. */
        { static const struct { uint32_t elf; int img; } tasks[] = {
              { 0x01770280u, 412186069 }, { 0x0177C200u, 526173636 },
              { 0x0178CF00u, 270608691 }, { 0x01794E00u, 419731232 },
              { 0x0179CC00u, 462161439 }, { 0x017ACD00u, 317566170 },
              { 0x017BE200u, 352410531 }, { 0x017D5900u, 476022134 },
              { 0x017F9A00u, 401731410 }, { 0x0181C600u, 484885458 },
              { 0x01832680u, 380810790 } };
          for (const auto& t : tasks) spu_taskset_register_task_elf(t.elf, t.img, 353941178); }
    }
#endif
    /* Context handlers are dispatched before the generated NID table. */
    ps3_hle_register_ctx(0x42B23552u, "sys_prx_register_library", des_sys_prx_register_library);
    if (getenv("DES_A2O_LR")) ps3_hle_register_ctx(0x21AC3697u, "cellGcmAddressToOffset", des_a2o);
    ps3_hle_register_ctx(0xA121A224u, "cellSpursTaskAttributeSetExitCodeContainer",
                         des_spurs_task_attr_set_exit_code_container);
    ps3_hle_register_ctx(0x011EE38Bu, "_cellSpursLFQueueInitialize", des_spurs_lfqueue_initialize);
    ps3_hle_register_ctx(0x8A85674Du, "_cellSpursLFQueuePushBody", des_spurs_lfqueue_push_body);
    /* With the real libsre the attach binds an SPU port (syscall 251), so the
     * detach must go to libsre as well and release it (252): stubbing it leaked a
     * port per decompressor queue until attaches failed BUSY in the first map
     * load. Only the HLE-SPURS setup, where the attach binds nothing, keeps the
     * no-op (DES_LFQ_DETACH_STUB=1). */
    if (getenv("DES_LFQ_DETACH_STUB"))
        ps3_hle_register_ctx(0x73E06F91u, "cellSpursLFQueueDetachLv2EventQueue",
                             [](ppu_context* ctx) { ctx->gpr[3] = CELL_OK; });
    /* cellAudioSetPortLevel: volume only. */
    ps3_hle_register_ctx(0x56DFE179u, "cellAudioSetPortLevel", [](ppu_context* ctx) { ctx->gpr[3] = CELL_OK; });
    /* NP, signed out: the runtime reports sceNpManagerGetStatus OFFLINE (no psnr
     * server) yet answered the identity queries with a fake signed-in user, and
     * had no sceNpManagerGetAvatarUrl at all. Demon's Souls polled those every
     * frame. Answer as a PS3 that is not signed in does, as RPCS3 does:
     * SCE_NP_ERROR_OFFLINE, and ID_NOT_FOUND for the invitation count.
     * DES_NP_FAKE_ONLINE=1 keeps the runtime's answers. */
    if (!getenv("DES_NP_FAKE_ONLINE")) {
        auto offline = [](ppu_context* ctx) { ctx->gpr[3] = 0x8002AA0Cu; };   /* SCE_NP_ERROR_OFFLINE */
        ps3_hle_register_ctx(0xFE37A7F4u, "sceNpManagerGetNpId", offline);
        ps3_hle_register_ctx(0xF42C0DF8u, "sceNpManagerGetOnlineName", offline);
        ps3_hle_register_ctx(0xBE07C708u, "sceNpManagerGetOnlineId", offline);
        ps3_hle_register_ctx(0x36D0C2C5u, "sceNpManagerGetAvatarUrl", offline);
        ps3_hle_register_ctx(0xAF505DEFu, "sceNpBasicGetMatchingInvitationEntryCount",
                             [](ppu_context* ctx) { ctx->gpr[3] = 0x8002AA06u; });   /* SCE_NP_ERROR_ID_NOT_FOUND */
    }
}

/* ---------------------------------------------------------------------------
 * Example: Override a specific NID
 *
 * The module system resolves function imports by NID.  To replace a default
 * HLE stub with custom logic, register your function at init time.
 * -----------------------------------------------------------------------*/

/*
 * Example: override cellGcmSetFlipMode (NID 0xA53D12AE) to force vsync.
 *
 *   static void my_cellGcmSetFlipMode(u32 mode)
 *   {
 *       printf("[hook] cellGcmSetFlipMode: forcing VSYNC (was %u)\n", mode);
 *       // Force vsync regardless of what the game requested
 *       cellGcmSetFlipMode(CELL_GCM_DISPLAY_VSYNC);
 *   }
 *
 * Then in your init code:
 *
 *   ps3::modules::override_nid(0xA53D12AE,
 *       reinterpret_cast<void*>(my_cellGcmSetFlipMode));
 */

/* ---------------------------------------------------------------------------
 * Example: Hook into module loading
 *
 * You can intercept cellSysmoduleLoadModule to perform custom actions
 * when a particular module is loaded.
 * -----------------------------------------------------------------------*/

/*
 *   static s32 my_loadmodule_hook(u16 id)
 *   {
 *       printf("[hook] Module 0x%04X loaded\n", id);
 *
 *       if (id == CELL_SYSMODULE_AUDIO) {
 *           // Perform custom audio backend initialization
 *           init_my_audio_backend();
 *       }
 *
 *       // Call the original implementation
 *       return cellSysmoduleLoadModule(id);
 *   }
 */

/* ---------------------------------------------------------------------------
 * Game-specific patches
 *
 * Sometimes you need to patch recompiled code at specific addresses.
 * Use the patch system to modify behavior without editing generated code.
 * -----------------------------------------------------------------------*/

/*
 *   // Skip an integrity check at guest address 0x00812FA0
 *   ps3::patches::nop_range(0x00812FA0, 0x00812FB0);
 *
 *   // Replace a function call with a direct return
 *   ps3::patches::force_return(0x009A1000, 0);  // return 0
 */
