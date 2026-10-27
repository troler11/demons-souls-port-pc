"""Differential tests of original/cached SPU code using the actual helpers."""
import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('regcache', ROOT / 'scripts/spu_regcache.py')
rc = importlib.util.module_from_spec(spec)
spec.loader.exec_module(rc)

FIXTURE = r'''
void fixture_spu_func_00000040(spu_context* ctx);
extern void observe(spu_context* ctx);
extern void escape(u128* value);
void fixture_spu_func_00000000(spu_context* ctx) {
    ctx->gpr[3] = spu_a(ctx->gpr[3], ctx->gpr[4]);
    ctx->gpr[80]._u32[1] ^= ctx->gpr[5]._u32[2];
    ctx->gpr[9] = spu_ila(7);
loc_00000004:
    ctx->gpr[7] = spu_xor(ctx->gpr[7], ctx->gpr[3]);
    spu_ls_write128(ctx, ctx->gpr[1]._u32[0], ctx->gpr[7]);
    ctx->gpr[8] = spu_ls_read128(ctx, ctx->gpr[1]._u32[0]);
    ctx->gpr[3] = spu_a(ctx->gpr[3], ctx->gpr[8]);
    ctx->gpr[9] = spu_ai(ctx->gpr[9], -1);
    if (ctx->gpr[9]._u32[0]) goto loc_00000004;
    observe(ctx);
    if (ctx->gpr[4]._u32[0] & 1) ctx->gpr[55] = ctx->gpr[8];
    ctx->gpr[6] = spu_shufb(ctx->gpr[7], ctx->gpr[55], ctx->gpr[11]);
    if (ctx->gpr[4]._u32[0] & 1) { observe(ctx); }
    ctx->gpr[3] = spu_xor(ctx->gpr[3], ctx->gpr[55]);
    escape(&ctx->gpr[80]);
    ++ctx->gpr[81]._u32[0];
    if (ctx->gpr[4]._u32[0] & 2) return;
    ctx->gpr[12] = spu_ai(ctx->gpr[6], 13);
    SPU_TAILCALL(fixture_spu_func_00000040(ctx));
}
void fixture_spu_func_00000040(spu_context* ctx) {
    ctx->gpr[5] = spu_a(ctx->gpr[12], ctx->gpr[3]);
    observe(ctx);
    ctx->gpr[14] = spu_xor(ctx->gpr[80], ctx->gpr[55]);
}
'''

HARNESS = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "spu_helpers.h"
#define SPU_TAILCALL(call) __attribute__((musttail)) return call
static uint64_t trace;
static void bytes(const void* p, size_t n) {
    const unsigned char* b = p;
    for (size_t i = 0; i < n; i++) trace = (trace ^ b[i]) * 1099511628211ull;
}
void observe(spu_context* ctx) {
    bytes(ctx->gpr, sizeof ctx->gpr); bytes(ctx->ls, SPU_LS_SIZE);
    ctx->gpr[55] = spu_ai(ctx->gpr[55], 19);
    ctx->gpr[3] = spu_xor(ctx->gpr[3], ctx->gpr[55]);
}
void escape(u128* value) { value->_u32[2] ^= 0x12345678u; }
void spu_rc_add_pairs(const void* const* pairs, unsigned n) { (void)pairs; (void)n; }
'''
MAIN = r'''
int main(void) {
    static spu_context original, cached;
    static unsigned char ls0[SPU_LS_SIZE], ls1[SPU_LS_SIZE];
    unsigned seed = 42;
    for (unsigned round = 0; round < 300; round++) {
        for (unsigned i = 0; i < 128; i++) for (unsigned k = 0; k < 4; k++) {
            seed = seed * 1664525u + 1013904223u;
            original.gpr[i]._u32[k] = seed;
        }
        for (unsigned i = 0; i < SPU_LS_SIZE; i++) { seed = seed * 1664525u + 1013904223u; ls0[i] = (unsigned char)(seed >> 24); }
        original.gpr[4]._u32[0] = (original.gpr[4]._u32[0] & ~3u) | (round & 3u);
        cached = original; memcpy(ls1, ls0, sizeof ls0);
        original.ls = ls0; cached.ls = ls1;
        trace = 1469598103934665603ull;
        fixture_spu_func_00000000(&original);
        uint64_t expected_trace = trace;
        trace = 1469598103934665603ull;
        fixture_rc_spu_func_00000000(&cached);
        if (trace != expected_trace || memcmp(original.gpr, cached.gpr, sizeof original.gpr) || memcmp(ls0, ls1, sizeof ls0)) {
            fprintf(stderr, "cached/original mismatch at round %u\n", round); return 1;
        }
    }
    puts("SPU regcache: GPRs, LS, callback observations, branches and tails match (300 inputs)");
    return 0;
}
'''

class RegisterCacheTests(unittest.TestCase):
    def test_reject_unsupported_flow(self):
        for line in ('while (1) {', 'for (;;) {', '#if FLAG',
                     'if (ctx->gpr[4]._u32[0]++) goto loc_00000004;'):
            with self.subTest(line=line), self.assertRaises(ValueError):
                rc.xform_func([line])

    def test_generation_preserves_source_and_mtime(self):
        with tempfile.TemporaryDirectory(prefix='des-regcache-') as folder:
            root = Path(folder)
            image = root / 'fixture'
            image.mkdir()
            source = image / 'spu_recomp.c'
            source.write_bytes(FIXTURE.encode())
            with patch('sys.argv', ['spu_regcache.py', str(root)]):
                rc.main()
                target = image / 'spu_recomp_rc.c'
                stamp, output = target.stat().st_mtime_ns, target.read_bytes()
                rc.main()
            self.assertEqual(source.read_bytes(), FIXTURE.encode())
            self.assertEqual(target.read_bytes(), output)
            self.assertEqual(target.stat().st_mtime_ns, stamp)

    def test_writes_and_pointer_escapes(self):
        self.assertEqual(rc.ASSIGN.findall('ctx->gpr[80]._u32[1] ^= ctx->gpr[5]._u32[2];'), ['80'])
        self.assertFalse(rc.is_pure('escape(&ctx->gpr[80]);'))
        self.assertFalse(rc.is_pure('++ctx->gpr[81]._u32[0];'))

    def test_native_differential(self):
        transformed, count = rc.xform_file(FIXTURE)
        self.assertEqual(count, 2)
        transformed, _ = rc.twin(transformed, FIXTURE)
        with tempfile.TemporaryDirectory(prefix='des-regcache-') as folder:
            folder = Path(folder)
            src = folder / 'differential.c'
            src.write_text(HARNESS + FIXTURE + transformed + MAIN)
            exe = folder / 'differential.exe'
            subprocess.run(['clang-cl', '/nologo', '/O2', '/w', '/D_CRT_SECURE_NO_WARNINGS',
                            '/DSPU_LS_NO_HOOKS', '-msse4.1', '-mavx2',
                            '/I' + str(ROOT / 'third_party/ps3recomp/runtime/spu'),
                            '/I' + str(ROOT / 'third_party/ps3recomp/include'),
                            str(src), '/Fo:' + str(folder / 'differential.obj'), '/Fe:' + str(exe)], check=True)
            subprocess.run([str(exe)], check=True)

if __name__ == '__main__':
    unittest.main()
