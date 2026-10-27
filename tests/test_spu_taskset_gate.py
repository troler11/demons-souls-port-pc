"""Compile the real parked-table/commit functions and exercise a native waiter.

The rest of spu_workload.c depends on the full guest runtime. Extract only
these functions unchanged so this test uses actual Windows SRW/CV wakeups.
Run in the Visual Studio developer environment: python tests/test_spu_taskset_gate.py
"""
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
KIT = ROOT / 'third_party/ps3recomp'
OUT = ROOT / 'work/perf2_20261023'

source = (KIT / 'runtime/spu/spu_workload.c').read_text()
table = source.split('#define SPU_PARKED_MAX', 1)[1].split('/* Event bits owed', 1)[0]
commit = source.split('void spu_taskset_line_committed(uint32_t line)', 1)[1].split('/* WAIT_SIGNAL', 1)[0]
header = (KIT / 'libs/spurs/spurs_taskset.h').read_text()
offset = re.search(r'CSTS_SIGNALLED\s*=\s*(0x[0-9a-fA-F]+)', header)[1]
harness = r'''
#include <windows.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
static SRWLOCK s_sig_lock = SRWLOCK_INIT;
static CONDITION_VARIABLE s_sig_cv;
static HANDLE ready;
static DWORD WINAPI wait_thread(void* unused) {
    (void)unused;
    AcquireSRWLockExclusive(&s_sig_lock);
    SetEvent(ready);
    const BOOL woke = SleepConditionVariableSRW(&s_sig_cv, &s_sig_lock, 2000, 0);
    ReleaseSRWLockExclusive(&s_sig_lock);
    return woke ? 0 : 1;
}
'''
harness += f'\n#define CSTS_SIGNALLED {offset}\n#define SPU_PARKED_MAX{table}\n'
harness += f'void spu_taskset_line_committed(uint32_t line){commit}\n'
harness += r'''
int main(void) {
    ready = CreateEventA(NULL, FALSE, FALSE, NULL);
    assert(!s_parked_armed);
    spu_taskset_line_committed(0x40001000);
    assert(!s_parked_armed);
    for (unsigned n = 0; n < 32; n++) {
        spu_taskset_parked_add(0x40001000, n, 0);
        assert(s_parked_armed && s_parked[0]);
        HANDLE t = CreateThread(NULL, 0, wait_thread, NULL, 0, NULL);
        assert(t && WaitForSingleObject(ready, 2000) == WAIT_OBJECT_0);
        spu_taskset_line_committed((0x40001000 + CSTS_SIGNALLED) & ~127u);
        assert(WaitForSingleObject(t, 2000) == WAIT_OBJECT_0);
        DWORD rc = 1; GetExitCodeThread(t, &rc); assert(rc == 0);
        CloseHandle(t);
        spu_taskset_parked_del(0x40001000, n);
        assert(s_parked_armed && !s_parked[0]); /* sticky, no lost first-wake gate */
    }
    CloseHandle(ready);
    puts("SPU parked gate: inactive table and 32 native CV wakeups passed");
    return 0;
}
'''
OUT.mkdir(parents=True, exist_ok=True)
test = OUT / 'taskset_gate.c'
test.write_text(harness)
subprocess.run(['clang-cl', '/O2', '/D_WIN32', str(test), f'/Fo:{OUT / "taskset_gate.obj"}',
                f'/Fe:{OUT / "taskset_gate.exe"}'], check=True)
subprocess.run([str(OUT / 'taskset_gate.exe')], check=True)
