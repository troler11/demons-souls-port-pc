"""Write libsre_exports.inc (NID -> base-0 OPD address) for lle/libsre_glue.cpp
from lift_prx.py's libsre_exports.json.

usage: python gen_libsre_exports.py work/lle/out/libsre_exports.json work/lle/libsre_exports.inc
"""
import json
import sys

d = json.load(open(sys.argv[1]))
base = int(d['base'], 16)
rows = []
for lib, m in d['exports'].items():
    for nid, opd in m.items():
        rows.append('    { %s, 0x%08X },  /* %s */' % (nid, int(opd, 16) - base, lib))
open(sys.argv[2], 'w').write('/* GENERATED from libsre_exports.json: NID -> base-0 OPD vaddr. */\n'
                             + '\n'.join(rows) + '\n')
print(len(rows), 'exports')
