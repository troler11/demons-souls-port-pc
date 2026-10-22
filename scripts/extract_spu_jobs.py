"""Cut the raw SPU programs the port lifts besides the ELF images it finds on
its own, from your own decrypted EBOOT.ELF and libsre.prx.

  EBOOT.ELF  0x0177B480 (0x180 bytes)  the SPURS memcpy job
  EBOOT.ELF  0x0177B600 (0x810 bytes)  the job with text fingerprint BD46853E5ADE5098
  libsre.prx 0x21580 / 0x23780 / 0x2A280  the SPURS policy modules A, B and C,
                                          loaded at LS 0xA00

Each is written as a .bin next to a small JSON of how to wrap it into an SPU
ELF (scripts/windows/prepare.ps1 runs ps3recomp's wrap_spu_elf.py with it).

usage: python extract_spu_jobs.py EBOOT.ELF libsre.prx OUT_DIR
"""
import json
import os
import struct
import sys

eboot_path, sre_path, out = sys.argv[1:4]
os.makedirs(out, exist_ok=True)
eboot = open(eboot_path, 'rb').read()
sre = open(sre_path, 'rb').read()


def eboot_read(va, n):
    phoff, = struct.unpack('>Q', eboot[0x20:0x28])
    phes, = struct.unpack('>H', eboot[0x36:0x38])
    phnum, = struct.unpack('>H', eboot[0x38:0x3A])
    for i in range(phnum):
        t, _, off, v, _, fs, _, _ = struct.unpack('>IIQQQQQQ', eboot[phoff + i * phes:phoff + (i + 1) * phes])
        if t == 1 and v <= va and va + n <= v + fs:
            return eboot[off + va - v:off + va - v + n]
    sys.exit('0x%X is not in a loaded segment of %s -- is this BLUS30443 1.00?' % (va, eboot_path))


jobs = [
    ('spujob_memcpy_177B480', eboot_read(0x0177B480, 0x180), dict(base='0x0', entry='0x0')),
    ('spujob_BD46853E5ADE5098', eboot_read(0x0177B600, 0x810), dict(base='0x0', entry='0x0')),
    ('sre_pmA_21580', sre[0x21580:0x21580 + 0x2B80 - 0xA00],
     dict(base='0xA00', entry='0xA00', symbols='f_A00:0xA00:0,f_A34:0xA34:0,f_288C:0x288C:0')),
    ('sre_pmB_23780', sre[0x23780:0x23780 + 0x2800 - 0xA00],
     dict(base='0xA00', entry='0xA00', symbols='f_A00:0xA00:0,f_A20:0xA20:0,f_2344:0x2344:0')),
    ('sre_pmC_2A280', sre[0x2A280:0x2A280 + 0x4800 - 0xA00],
     dict(base='0xA00', entry='0xA00', symbols='f_A00:0xA00:0,f_2B00:0x2B00:0,f_465C:0x465C:0')),
]
for name, data, wrap in jobs:
    open(os.path.join(out, name + '.bin'), 'wb').write(data)
    json.dump(wrap, open(os.path.join(out, name + '.json'), 'w'))
    print('%-26s %5d bytes' % (name, len(data)))
