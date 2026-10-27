"""Reapply BLUS30443 title hooks after lifting, validating each anchor first.

python scripts/apply_title_hooks.py work/game/recompiled work/game/EBOOT.ELF
Only changed translation units are written. Re-running is a no-op.
"""
import bisect
import re
import struct
import sys
from pathlib import Path

CAMERA_SITES = (0x119818, 0x1198D8, 0x119FAC, 0x11A018, 0x11B830,
                0x11C0E8, 0x11C2BC, 0x11D6E4, 0x11D820, 0x110808,
                0x123100, 0x123164, 0x124188, 0x12554C, 0x1256D4)
OVERRIDES = {0x273D38: 'des_move_post', 0x724E40: 'des_menu_tick'}
FUNCTION = re.compile(r'^void func_([0-9A-Fa-f]+)\(ppu_context\* ctx\) \{', re.M)
LOAD = re.compile(r'\{ uint32_t tmp = vm_read32\(ctx->gpr\[(\d+)\] \+ (-?(?:0x[0-9A-Fa-f]+|\d+))\); '
                  r'float ftmp; memcpy\(&ftmp, &tmp, 4\); ctx->fpr\[(\d+)\] = ftmp; \}')


def instruction(elf, address):
    phoff, = struct.unpack_from('>Q', elf, 0x20)
    stride, count = struct.unpack_from('>HH', elf, 0x36)
    for i in range(count):
        kind, _, off, va, _, size, _, _ = struct.unpack_from('>IIQQQQQQ', elf, phoff + i * stride)
        if kind == 1 and va <= address and address + 4 <= va + size:
            return struct.unpack_from('>I', elf, off + address - va)[0]
    raise ValueError(f'instruction 0x{address:X} is outside ELF segments')


def patch_unit(source, elf, overrides, sites):
    functions = list(FUNCTION.finditer(source))
    addresses = [int(m[1], 16) for m in functions]
    edits = []
    for address, symbol in overrides.items():
        i = addresses.index(address)
        begin = functions[i].end()
        end = functions[i + 1].start() if i + 1 < len(functions) else len(source)
        if re.match(r'\s*' + symbol + r'\(ctx\);\s*return;', source[begin:end]):
            continue
        edits.append((begin, begin, f'\n        {symbol}(ctx);\n        return;'))
    for address in sites:
        marker = f'des_cam_rate(ctx, 0x{address:08X}u);'
        if marker in source:
            continue
        insn = instruction(elf, address)
        if insn >> 26 != 48:
            raise ValueError(f'0x{address:X}: expected lfs; wrong game/version')
        fd, ra, displacement = (insn >> 21) & 31, (insn >> 16) & 31, insn & 0xFFFF
        if displacement & 0x8000:
            displacement -= 0x10000
        i = bisect.bisect_right(addresses, address) - 1
        begin = functions[i].end()
        end = functions[i + 1].start() if i + 1 < len(functions) else len(source)
        matches = [m for m in LOAD.finditer(source, begin, end)
                   if (int(m[1]), int(m[2], 0), int(m[3])) == (ra, displacement, fd)]
        if len(matches) > 1:
            # Identical loads can occur twice in one camera function. Match
            # their ordinal in the original instruction stream, never guess.
            stop = addresses[i + 1] if i + 1 < len(addresses) else address + 4
            originals = [pc for pc in range(addresses[i], stop, 4)
                         if instruction(elf, pc) == insn]
            if len(originals) != len(matches) or address not in originals:
                raise ValueError(f'0x{address:X}: ambiguous load anchors')
            match = matches[originals.index(address)]
        elif len(matches) == 1:
            match = matches[0]
        else:
            raise ValueError(f'0x{address:X}: load anchor missing')
        at = match.end()
        edits.append((at, at, ' ' + marker))
    declaration = ('extern "C" void des_move_post(ppu_context*);\n'
                   'extern "C" void des_menu_tick(ppu_context*);\n'
                   'extern "C" void des_cam_rate(ppu_context*, uint32_t);\n')
    if edits and declaration not in source:
        at = source.index('#include "ppu_recomp.h"')
        at = source.index('\n', at) + 1
        edits.append((at, at, declaration))
    for begin, end, value in sorted(edits, reverse=True):
        source = source[:begin] + value + source[end:]
    return source


def main():
    root, elf_path = map(Path, sys.argv[1:3])
    elf = elf_path.read_bytes()
    # The current split puts these functions in 000/001/003. Discover anchors
    # rather than relying on chunk numbers, which change after re-lifting.
    pending_overrides = dict(OVERRIDES)
    pending_sites = set(CAMERA_SITES)
    writes = []
    for path in sorted(root.glob('ppu_recomp_*.cpp')):
        source = path.read_bytes().decode('latin-1').replace('\r\n', '\n')
        functions = [int(m[1], 16) for m in FUNCTION.finditer(source)]
        if not functions:
            continue
        own = {a: s for a, s in pending_overrides.items() if a in functions}
        sites = {a for a in pending_sites if min(functions) <= a <= max(functions)}
        # The next function can be in the same unit, but none of these sites
        # is in the final function of a unit for the supported title lift.
        if own or sites:
            updated = patch_unit(source, elf, own, sites)
            if updated != source:
                writes.append((path, updated))
            for a in own:
                del pending_overrides[a]
            pending_sites -= sites
        if not pending_overrides and not pending_sites:
            break
    if pending_overrides or pending_sites:
        raise ValueError(f'missing hook functions/sites: {pending_overrides}, {pending_sites}')
    for path, text in writes:
        path.write_bytes(text.replace('\n', '\r\n').encode('latin-1'))
        print(f'patched {path.name}')
    print(f'title hooks verified: {len(OVERRIDES)} overrides, {len(CAMERA_SITES)} camera sites')


if __name__ == '__main__':
    main()
