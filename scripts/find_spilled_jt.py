"""Find bctr dispatchers whose jump-table base is spilled to the stack:

    lwz rX,D(r2) ... std rX,S(r1) ... cmplwi rI,N ... ld r11,S(r1) ... lwzx/add ... mtctr ... bctr

Prints JT_EXTRA specs "disp:base:count" for ppu_lifter.py.
usage: python find_spilled_jt.py EBOOT.ELF out/EBOOT.functions_tramp.json
"""
import json
import struct
import sys

d = open(sys.argv[1], 'rb').read()
SEGS = [(0, 0x10000, 0x1832d48), (0x1840000, 0x1850000, 0x28609c)]


def r32(va):
    for off, base, sz in SEGS:
        if base <= va < base + sz - 3:
            o = off + va - base
            return struct.unpack('>I', d[o:o + 4])[0]
    return None


fs = json.load(open(sys.argv[2]))
funcs = sorted((int(f['start'], 0), int(f['end'], 0), int(f.get('toc', '0'), 0)) for f in fs)

specs = []
for lo, hi, toc in funcs:
    for va in range(lo, hi, 4):
        if r32(va) != 0x4E800420:          # bctr
            continue
        # ld rT,S(r1) within the 8 instructions before the bctr
        spill = None
        bound = None
        for k in range(1, 10):
            x = r32(va - 4 * k)
            if x is None:
                break
            if (x >> 26) == 58 and ((x >> 16) & 31) == 1 and (x & 3) == 0:
                spill = x & 0xFFFC
            if (x >> 26) == 10:            # cmpli
                bound = (x & 0xFFFF) + 1
        if spill is None or bound is None:
            continue
        # std rX,S(r1) earlier in the function, with rX loaded by lwz rX,D(r2)
        base = None
        for vb in range(va - 4, lo - 4, -4):
            x = r32(vb)
            if (x >> 26) == 62 and ((x >> 16) & 31) == 1 and (x & 0xFFFC) == spill:
                rx = (x >> 21) & 31
                for vc in range(vb - 4, max(lo, vb - 64) - 4, -4):
                    y = r32(vc)
                    if (y >> 26) == 32 and ((y >> 21) & 31) == rx and ((y >> 16) & 31) == 2:
                        disp = y & 0xFFFF
                        disp = disp - 0x10000 if disp & 0x8000 else disp
                        if toc:
                            base = r32(toc + disp)
                        break
                break
        if not base:
            continue
        ok = True
        for i in range(bound):
            w = r32(base + 4 * i)
            if w is None:
                ok = False
                break
            w = w - (1 << 32) if w & 0x80000000 else w
            if not (lo <= base + w < hi):
                ok = False
                break
        print(f'{"OK " if ok else "?? "} func 0x{lo:X} bctr 0x{va:X} base 0x{base:X} n {bound}')
        if ok:
            specs.append(f'0x{va:X}:0x{base:X}:{bound}')
print('JT_EXTRA=' + ','.join(specs))
