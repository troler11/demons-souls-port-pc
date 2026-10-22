"""Add the linker's TOC-switch trampolines to ps3recomp's function list.

Demon's Souls links code with two TOCs. Calls into the second one go through 16-byte
stubs `std r2,0x28(r1); addis r2,r2,hi; addi r2,r2,lo; b target` that sit between
functions and have no OPD entry. The lifter drops a `bl` to such a stub as a call to
"non-code" but keeps the caller's `ld r2,0x28(r1)`, so r2 is restored from a stale slot.
Usage: add_toc_trampolines.py EBOOT.ELF functions.json out.json
"""
import json, struct, sys

elf, fin, fout = sys.argv[1:4]
d = open(elf, 'rb').read()
phoff, = struct.unpack('>Q', d[32:40]); phnum, = struct.unpack('>H', d[56:58])
text = None
for i in range(phnum):
    t, fl, off, va, _, fs, _, _ = struct.unpack('>IIQQQQQQ', d[phoff + i * 56:phoff + i * 56 + 56])
    if t == 1 and fl & 1: text = (off, va, fs)
off, va, fs = text
funcs = json.load(open(fin))
starts = {int(f['start'], 16) for f in funcs}
main_toc = funcs[1]['toc']
added = 0
for o in range(off, off + fs - 16, 4):
    w0, w1, w2, w3 = struct.unpack('>4I', d[o:o + 16])
    if w0 == 0xF8410028 and w1 >> 16 == 0x3C42 and w2 >> 16 == 0x3842 and w3 >> 26 == 18 and not w3 & 1:
        a = va + o - off
        if a not in starts:
            funcs.append({'start': hex(a), 'end': hex(a + 16), 'toc': main_toc, 'opd': '0x0'})
            added += 1
funcs.sort(key=lambda f: int(f['start'], 16))
json.dump(funcs, open(fout, 'w'), indent=1)
print(f'added {added} TOC trampolines -> {fout}')
