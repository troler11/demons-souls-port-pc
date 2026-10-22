"""Summary of a decrypted PS3 PPU executable: segments, imported modules, embedded SPU images.

Usage: python -I elf_report.py EBOOT.elf [imports.json]
"""
import collections
import json
import struct
import sys

PT = {1: 'LOAD', 4: 'NOTE', 7: 'TLS', 0x60000001: 'PROC_PARAM', 0x60000002: 'PRX_PARAM'}


def main():
    path = sys.argv[1]
    d = open(path, 'rb').read()
    assert d[:4] == b'\x7fELF' and d[4] == 2 and d[5] == 2, 'expected ELF64 big-endian'
    machine, = struct.unpack('>H', d[18:20])
    entry, phoff, shoff = struct.unpack('>QQQ', d[24:48])
    phentsize, phnum, shentsize, shnum, shstrndx = struct.unpack('>HHHHH', d[54:64])
    print(f'machine {machine:#x} ({"PPC64" if machine == 0x15 else "?"}), entry {entry:#x}, {phnum} segments, {shnum} sections')
    segs = []
    for i in range(phnum):
        t, flags, off, vaddr, paddr, filesz, memsz, align = struct.unpack('>IIQQQQQQ', d[phoff + i * phentsize:phoff + (i + 1) * phentsize])
        segs.append((t, flags, off, vaddr, filesz, memsz))
        print(f'  {PT.get(t, hex(t)):>10} flags {flags:#x} off {off:#9x} vaddr {vaddr:#10x} file {filesz:#9x} mem {memsz:#9x}')

    def at(addr, size=4):
        for t, _, off, vaddr, filesz, _ in segs:
            if t == 1 and vaddr <= addr and addr + size <= vaddr + filesz:
                return off + addr - vaddr
        raise ValueError(f'unmapped {addr:#x}')

    def cstr(addr):
        o = at(addr, 1)
        return d[o:d.index(b'\0', o)].decode('latin-1')

    # Imports: sys_process_prx_param -> library stubs (0x2C bytes each)
    prx = next(s for s in segs if s[0] == 0x60000002)
    size, magic, ver, _, ent_s, ent_e, stub_s, stub_e = struct.unpack('>IIIIIIII', d[prx[2]:prx[2] + 32])
    assert magic == 0x1b434cec, f'bad prx param magic {magic:#x}'
    mods = collections.OrderedDict()
    a = stub_s
    while a < stub_e:
        o = at(a, 0x2c)
        ssize = d[o]
        _, _, _, _, nfunc, nvar, _ = struct.unpack('>BBHHHHH', d[o:o + 12])
        name_p, nids_p, addrs_p, vnids_p = struct.unpack('>IIII', d[o + 16:o + 32])
        name = cstr(name_p)
        nids = [struct.unpack('>I', d[at(nids_p + 4 * k):at(nids_p + 4 * k) + 4])[0] for k in range(nfunc)]
        vn = [struct.unpack('>I', d[at(vnids_p + 4 * k):at(vnids_p + 4 * k) + 4])[0] for k in range(nvar)] if nvar else []
        mods[name] = {'functions': [f'{n:08x}' for n in nids], 'variables': [f'{n:08x}' for n in vn]}
        a += ssize or 0x2c
    total = sum(len(m['functions']) for m in mods.values())
    print(f'\n{len(mods)} imported libraries, {total} functions, {sum(len(m["variables"]) for m in mods.values())} variables')
    for n, m in sorted(mods.items(), key=lambda x: -len(x[1]['functions'])):
        print(f'  {len(m["functions"]):>4}  {n}')

    # Code size and rough function count: blr (4e800020) in executable LOAD segments
    code = sum(s[4] for s in segs if s[0] == 1 and s[1] & 1)
    blr = sum(d[s[2]:s[2] + s[4]].count(b'\x4e\x80\x00\x20') for s in segs if s[0] == 1 and s[1] & 1)
    print(f'\nexecutable bytes {code:,} (~{code // 4:,} instructions), blr count {blr:,} (rough function-exit count)')

    # Embedded SPU ELFs (ELF32 big-endian, e_machine 0x17)
    spu, pos = [], 0
    while (pos := d.find(b'\x7fELF\x01\x02\x01', pos + 1)) != -1:
        if struct.unpack('>H', d[pos + 18:pos + 20])[0] == 0x17:
            spu.append(pos)
    print(f'embedded SPU ELF images: {len(spu)}')
    # Strings hinting at SPU job frameworks
    for kw in (b'SPURS', b'spurs', b'SpursJob', b'Edge', b'EDGE', b'cellGcm', b'PhyreEngine', b'Havok', b'hkp'):
        print(f'  string {kw.decode():>12}: {d.count(kw)}')
    if len(sys.argv) > 2:
        json.dump({'modules': mods, 'spu_offsets': spu}, open(sys.argv[2], 'w'), indent=1)


if __name__ == '__main__':
    main()
