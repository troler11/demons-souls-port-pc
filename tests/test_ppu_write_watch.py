"""Exercise the actual loader watch code, including diagnostic side effects.

Requires clang-cl in a Windows developer environment. Only the watch slice is
compiled; callbacks are stubs and the test never starts the game or touches saves.
"""
import json
import os
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]
WATCH_VARS = (
    'PPU_WWATCH', 'PPU_WWATCH_LEN', 'PPU_WWATCH_MAX', 'PPU_WW_GUARD',
    'PPU_WVAL', 'PPU_WVAL_LO', 'PPU_WVAL_HI', 'PPU_WVAL_GATE',
)


@unittest.skipUnless(os.name == 'nt', 'Windows native watch harness')
class WriteWatchTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.out = ROOT / os.environ.get('PPU_WATCH_TEST_DIR', 'work/ppu_write_watch_test')
        cls.out.mkdir(parents=True, exist_ok=True)
        loader = (ROOT / 'third_party/ps3recomp/runtime/ppu/ppu_loader.cpp').read_text(encoding='utf-8')
        start = loader.index('/* Called once PPU_WWATCH has been parsed')
        end = loader.index('/* See the note on the forward declaration above ppu_stwcx32. */', start)
        watch = loader[start:end]
        # Test-only counter at entry to the real slow function; all production
        # conditions, parsing, logging, caps and guard callbacks stay untouched.
        needle = 'static void barrier_watch_hit_slow(uint32_t a, uint32_t v, int width, void* ra)\n{'
        assert watch.count(needle) == 1
        watch = watch.replace(needle, needle + '\n    ++slow_calls;')
        prefix = r'''
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define PPU_THREAD_LOCAL thread_local
struct ppu_context { uint64_t lr; unsigned thread_id; };
static ppu_context context = {0x123456, 6};
thread_local ppu_context* g_active_ctx = &context;
uint32_t g_barrier_sync_watch, g_ww_lo, g_ww_hi;
static unsigned slow_calls, guards, dumps;
uint32_t ppu_prof_resolve_host(void*) { return 0xABCDEF; }
void ppu_dump_guest_stack(ppu_context*, const char*) { ++dumps; }
void ppu_guard_page(uint32_t) { ++guards; }
'''
        suffix = r'''
int main(int argc, char** argv) {
    if (argc > 1 && !strcmp(argv[1], "dynamic")) {
        barrier_watch_hit(0x10040, 1, 4, 0);
        g_barrier_sync_watch = 0x10000;
        for (uint32_t a : {0x1003fu, 0x10040u, 0x100bfu, 0x100c0u})
            barrier_watch_hit(a, 2, 4, 0);
        g_barrier_sync_watch = 0;
        barrier_watch_hit(0x10040, 3, 4, 0);
        g_barrier_sync_watch = 0x20000;
        barrier_watch_hit(0x20040, 4, 4, 0);
    } else {
        for (uint32_t a : {0x1001fu, 0x10020u, 0x10023u, 0x1003cu, 0x10040u})
            barrier_watch_hit(a, 0x1234, 4, 0);
        // Same two-half watch contract as vm_write64, including low-word hits.
        barrier_watch_hit(0x1001c, 0x9876, 4, 0);
        barrier_watch_hit(0x10020, 0x1234, 4, 0);
    }
    printf("{\"slow\":%u,\"guards\":%u,\"dumps\":%u,\"lo\":%u,\"hi\":%u}\n",
           slow_calls, guards, dumps, g_ww_lo, g_ww_hi);
}
'''
        source = cls.out / 'watch.cpp'
        source.write_text(prefix + '\n#include <initializer_list>\n' + watch + suffix, encoding='utf-8')
        cls.exe = cls.out / 'watch.exe'
        subprocess.run(['clang-cl', '/nologo', '/O2', '/EHsc', str(source),
                        '/Fo:' + str(cls.out / 'watch.obj'), '/Fe:' + str(cls.exe)], check=True)

    def run_case(self, fast, variables=None, scenario='writes'):
        env = dict(os.environ)
        for k in (*WATCH_VARS, 'PPU_WRITE_WATCH_FAST'):
            env.pop(k, None)
        env['PPU_WRITE_WATCH_FAST'] = str(fast)
        env.update(variables or {})
        p = subprocess.run([str(self.exe), scenario], env=env, capture_output=True,
                           text=True, check=True)
        return json.loads(p.stdout), p.stderr

    def compare(self, variables=None, scenario='writes'):
        old, old_log = self.run_case(0, variables, scenario)
        new, new_log = self.run_case(1, variables, scenario)
        self.assertEqual(old_log, new_log)
        self.assertEqual({k:v for k,v in old.items() if k != 'slow'},
                         {k:v for k,v in new.items() if k != 'slow'})
        return old, new, new_log

    def test_no_watch_skips_only_diagnostic_helper(self):
        old, new, log = self.compare()
        self.assertEqual((old['slow'], new['slow']), (7, 0))
        self.assertEqual(log, '')

    def test_dynamic_watch_arms_disarms_and_rearms(self):
        old, new, log = self.compare(scenario='dynamic')
        self.assertEqual((old['slow'], new['slow']), (7, 5))
        self.assertEqual(log.count('[sync-write]'), 3)

    def test_address_watch_boundaries_halves_and_window(self):
        old, new, log = self.compare({'PPU_WWATCH':'10023'})
        self.assertEqual(new['slow'], 7)
        self.assertEqual((new['lo'], new['hi']), (0x10020, 0x10040))
        self.assertEqual(log.count('[ww] 0x'), 4)
        self.assertEqual(new['dumps'], 1)
        _, wide, wide_log = self.compare({'PPU_WWATCH':'0x10023', 'PPU_WWATCH_LEN':'64'})
        self.assertEqual(wide['hi'], 0x10060)
        self.assertEqual(wide_log.count('[ww] 0x'), 5)

    def test_guard_and_print_cap(self):
        _, state, log = self.compare({'PPU_WWATCH':'10023', 'PPU_WW_GUARD':'1', 'PPU_WWATCH_MAX':'2'})
        self.assertEqual(state['guards'], 1)
        self.assertEqual(log.count('[ww] 0x'), 2)
        self.assertIn('print cap 2 reached', log)

    def test_value_filter_and_file_gate(self):
        variables = {'PPU_WVAL':'0x1234','PPU_WVAL_LO':'0x10020','PPU_WVAL_HI':'0x10040'}
        _, state, log = self.compare(variables)
        self.assertEqual(log.count('[wv]'), 4)
        self.assertEqual(state['dumps'], 3)
        gate = self.out / 'gate.flag'
        gate.write_text('armed')
        try:
            _, _, open_log = self.compare(dict(variables, PPU_WVAL_GATE=str(gate)))
            self.assertEqual(open_log, log)
        finally:
            gate.unlink()
        _, _, closed_log = self.compare(dict(variables, PPU_WVAL_GATE=str(gate)))
        self.assertEqual(closed_log, '')

    def test_each_diagnostic_presence_preserves_full_path(self):
        for name in WATCH_VARS:
            with self.subTest(name=name):
                _, new, _ = self.compare({name:'0'})
                self.assertEqual(new['slow'], 7)


if __name__ == '__main__':
    unittest.main()
