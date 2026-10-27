#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
typedef void (*spu_fn)(void*);
#include "../third_party/ps3recomp/runtime/spu/spu_regcache.h"
static void original0(void* p) { (void)p; }
static void original1(void* p) { (void)p; }
static void cached0(void* p) { (void)p; }
static void cached1(void* p) { (void)p; }
static void unrelated(void* p) { (void)p; }
static spu_fn expected0, expected1;
static DWORD WINAPI select_thread(void* unused)
{
    (void)unused;
    for (unsigned n = 0; n < 10000; n++) {
        assert(spu_rc_select(original0) == expected0);
        assert(spu_rc_select(original1) == expected1);
        assert(spu_rc_select(unrelated) == unrelated);
        assert(spu_rc_select(cached0) == cached0);
        assert(spu_rc_select(NULL) == NULL);
    }
    return 0;
}
int main(int argc, char** argv)
{
    const int enabled = argc > 1 && atoi(argv[1]) != 0;
    SetEnvironmentVariableA("SPU_REGCACHE", enabled ? "1" : "0");
    /* getenv uses the CRT environment, so update both for this native test. */
    _putenv_s("SPU_REGCACHE", enabled ? "1" : "0");
    const void* const pairs[] = {original0, cached0, original1, cached1};
    spu_rc_add_pairs(pairs, 2);
    expected0 = enabled ? cached0 : original0;
    expected1 = enabled ? cached1 : original1;
    HANDLE threads[8];
    for (unsigned n = 0; n < 8; n++) threads[n] = CreateThread(NULL, 0, select_thread, NULL, 0, NULL);
    assert(WaitForMultipleObjects(8, threads, TRUE, 10000) == WAIT_OBJECT_0);
    for (unsigned n = 0; n < 8; n++) CloseHandle(threads[n]);
    puts(enabled ? "register cache selection ON passed (8 threads)" : "register cache selection OFF passed (8 threads)");
}
