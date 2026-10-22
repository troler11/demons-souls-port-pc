"""Wire the RPCS3 "Unlock FPS" patch for BLUS30443 (Gibbed, Whatcookie) into the
lifted PPU sources as calls to host hooks (des_port/stubs.cpp).

RPCS3 patches the instructions in memory; here the code is already C, so the two
patched instructions are replaced by hook calls:

  0x25ED8  stb r29,0x6ED(r31) -> stb r0,0x6ED(r31)   (60 fps, no frameskip)
  0x1B964  lfs f31,-0x7F78(r30) -> bl <code cave>    (f31 = real frame delta,
                                                      clamped to 50 ms)

The hooks fall back to the original behaviour unless DES_FPS_UNLOCK=1.
Idempotent: run again after every re-lift.

usage: python scripts/apply_fps_unlock.py [p2/recompiled/ppu_recomp_000.cpp]
"""
import sys

path = sys.argv[1] if len(sys.argv) > 1 else 'p2/recompiled/ppu_recomp_000.cpp'
src = open(path, encoding='latin-1').read()

DECL = ('extern "C" uint8_t des_fps_frameskip_flag(uint8_t orig, uint8_t unlocked);\n'
        'extern "C" double des_fps_frame_dt(double orig);\n')

PATCHES = [
    # (enclosing function, original line, replacement)
    ('void func_00025DD8(ppu_context* ctx) {',
     '        vm_write8(ctx->gpr[31] + 0x6ED, ctx->gpr[29]);\n',
     '        vm_write8(ctx->gpr[31] + 0x6ED, des_fps_frameskip_flag((uint8_t)ctx->gpr[29], (uint8_t)ctx->gpr[0])); /* fps-unlock 0x25ED8 */\n'),
    ('void func_0001B8D0(ppu_context* ctx) {',
     '        { uint32_t tmp = vm_read32(ctx->gpr[30] + -0x7F78); float ftmp; memcpy(&ftmp, &tmp, 4); ctx->fpr[31] = ftmp; }\n',
     '        { uint32_t tmp = vm_read32(ctx->gpr[30] + -0x7F78); float ftmp; memcpy(&ftmp, &tmp, 4); ctx->fpr[31] = des_fps_frame_dt(ftmp); } /* fps-unlock 0x1B964 */\n'),
]

if 'des_fps_frame_dt' in src:
    print('already applied')
    sys.exit(0)

for fn, old, new in PATCHES:
    start = src.index(fn)
    end = src.index('\nvoid func_', start + len(fn))
    i = src.index(old, start, end)
    src = src[:i] + new + src[i + len(old):]

inc = '#include "ppu_recomp.h"\n'
src = src.replace(inc, inc + DECL, 1)
open(path, 'w', encoding='latin-1', newline='\r\n').write(src)
print('applied')
