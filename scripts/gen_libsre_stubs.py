"""Turn lift_prx.py's libsre_imports.json ({lib: {nid: stub}}) into the list
form ppu_lifter.py --hle-stubs reads ([{library, nid, stub}]).

usage: python gen_libsre_stubs.py work/lle/out/libsre_imports.json work/lle/out/libsre_hle_stubs.json
"""
import json
import sys

imports = json.load(open(sys.argv[1]))
rows = [{'library': lib, 'nid': nid, 'stub': stub}
        for lib, m in imports.items() for nid, stub in m.items()]
json.dump(rows, open(sys.argv[2], 'w'), indent=1)
print(len(rows), 'imports')
