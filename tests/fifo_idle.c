/* Differential stream replay of the real idle certificate. The baseline always
 * consumes the test stream and publishes progress/references. The candidate
 * uses the production shortcut; compare observable state after every step.
 * GPU/FIFO decoding remains covered by gameplay integration, not this model. */
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
typedef uint32_t u32;
typedef uint64_t u64;
static const char* gcm_getenv(const char* name) { return getenv(name); }
static int live = 1, engine;
static int rsx_live_draw_enabled(void) { return live; }
static int rsx_draw_engine_enabled(void) { return engine; }
#include "../third_party/ps3recomp/libs/video/gcm_fifo_idle.h"
enum { LABEL, REF, PARK, ACQUIRE };
typedef struct { u32 kind, value; } command;
typedef struct {
    u32 get, put, guest_get, label, external, context, context_out, call, map, drained;
    int syscall_mode;
    command words[256];
    u32 refs[4096], head, tail;
    u64 published_hash, published, draws, full, fast;
} model;

static void drain(model* m, int candidate)
{
    const u32 mapped = m->map + m->get * 4;
    if (candidate && gcm_fifo_idle_matches(m->get, m->put, m->guest_get,
            m->context, m->context_out, m->call, m->drained, mapped, m->syscall_mode)) {
        m->fast++;
    } else {
        if (candidate) gcm_fifo_idle_invalidate();
        m->full++;
        if (!m->context && !m->syscall_mode) return;
        while (m->get != m->put) {
            const command* c = &m->words[m->get];
            if (c->kind == PARK || (c->kind == ACQUIRE && m->external != c->value)) break;
            if (c->kind == LABEL) { m->label = c->value; m->draws++; }
            if (c->kind == REF) m->refs[m->tail++ % 4096] = c->value;
            m->get = (m->get + 1) % 256;
        }
        m->guest_get = m->get;
        m->drained = m->map + m->get * 4;
        if (candidate) gcm_fifo_idle_arm(m->get, m->put, m->context,
            m->context_out, m->call, m->drained, m->syscall_mode);
    }
    /* This service must run even on certified empty calls. */
    if (m->head != m->tail) {
        m->published_hash = (m->published_hash ^ m->refs[m->head++ % 4096]) * 1099511628211ull;
        m->published++;
    }
}
static void compare(model* a, model* b)
{
    drain(a, 0); drain(b, 1);
    assert(a->get == b->get && a->put == b->put && a->guest_get == b->guest_get);
    assert(a->drained == b->drained && a->label == b->label && a->draws == b->draws);
    assert(a->head == b->head && a->tail == b->tail);
    assert(a->published == b->published && a->published_hash == b->published_hash);
}
static void push(model* a, model* b, u32 kind, u32 value)
{
    assert((a->put + 1) % 256 != a->get);
    a->words[a->put] = b->words[b->put] = (command){kind, value};
    a->put = b->put = (a->put + 1) % 256;
}
int main(int argc, char** argv)
{
    _putenv_s("PS3_GCM_NO_TITLE_RECYCLE", "1");
    _putenv_s("GCM_FIFO_IDLE", (argc > 1 && !strcmp(argv[1], "default")) ? "" : "1");
    if (argc > 1) {
        if (!strcmp(argv[1], "default")) { /* Keep the default disabled. */ }
        else if (!strcmp(argv[1], "no_live")) live = 0;
        else if (!strcmp(argv[1], "engine")) engine = 1;
        else if (!strcmp(argv[1], "recycle")) _putenv_s("PS3_GCM_NO_TITLE_RECYCLE", "");
        else _putenv_s(argv[1], !strcmp(argv[1], "GCM_FIFO_IDLE") ? "0" : "1");
        assert(!gcm_fifo_idle_enabled());
        printf("fallback passed: %s\n", argv[1]);
        return 0;
    }
    assert(gcm_fifo_idle_enabled());
    model a = {0}, b = {0};
    a.context = b.context = 0x1000; a.context_out = b.context_out = 0x2000;
    a.map = b.map = 0x40000000; a.published_hash = b.published_hash = 1;
    compare(&a, &b); compare(&a, &b); assert(b.fast == 1);
    /* An active parked command is patched in place with identical PUT. */
    push(&a, &b, PARK, 0); compare(&a, &b);
    u64 fast = b.fast;
    for (int i = 0; i < 20; i++) compare(&a, &b);
    assert(b.fast == fast && a.get != a.put);
    a.words[a.get] = b.words[b.get] = (command){LABEL, 123};
    compare(&a, &b); assert(a.label == 123);
    /* Semaphore release without a PUT update must be retried too. */
    push(&a, &b, ACQUIRE, 99); compare(&a, &b);
    fast = b.fast; compare(&a, &b); assert(b.fast == fast);
    a.external = b.external = 99; compare(&a, &b); assert(a.get == a.put);
    /* Several references remain queued after draining to empty. */
    for (int i = 0; i < 64; i++) push(&a, &b, REF, (u32)i ^ 0xffffffffu);
    for (int i = 0; i < 64; i++) compare(&a, &b);
    assert(a.published == 64 && a.head == a.tail);
    u32 random = 0x1265aba7u;
    for (unsigned step = 0; step < 250000; step++) {
        random ^= random << 13; random ^= random >> 17; random ^= random << 5;
        switch (random % 17) {
        case 0: a.guest_get = b.guest_get = a.get ^ 1u; break;
        case 1: a.map = b.map = a.map ^ 0x100000u; break;
        case 2: a.context = b.context = a.context ^ 0x1000u; break;
        case 3: a.context_out = b.context_out = a.context_out ^ 0x1000u; break;
        case 4: a.call = b.call = random; break;
        case 5: a.syscall_mode = b.syscall_mode = !a.syscall_mode; break;
        case 6: a.drained = b.drained = a.drained ^ 4u; break;
        case 7: gcm_fifo_idle_invalidate(); break;
        default:
            if (a.context && (a.put + 1) % 256 != a.get)
                push(&a, &b, (random & 1) ? REF : LABEL, random);
            break;
        }
        compare(&a, &b);
    }
    a.context = b.context = 0x1000; a.syscall_mode = b.syscall_mode = 0;
    for (int i = 0; i < 500; i++) compare(&a, &b);
    assert(b.fast > 100 && a.full > b.full && a.head == a.tail);
    printf("FIFO idle differential replay: 250000 steps, %llu shortcuts, %llu references; parked patches, semaphore release, GET repair, remapping, context/reset and publication order passed\n",
        (unsigned long long)b.fast, (unsigned long long)b.published);
    return 0;
}
