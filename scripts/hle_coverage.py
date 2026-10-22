"""Game imports vs what ps3recomp implements: NID in the generated table, or the NID / quoted
name anywhere in runtime/ and libs/ sources (handlers registered by ps3_compute_nid or tables).
Usage: hle_coverage.py imports_named.json ppu_hle_nids.cpp PS3RECOMP_DIR"""
import collections, json, re, sys
from pathlib import Path
imports = json.load(open(sys.argv[1]))
have = {int(n, 16) for n in re.findall(r'ps3_hle_register\((0x[0-9A-Fa-f]+)', open(sys.argv[2]).read())}
src = ''
for d in ('runtime', 'libs'):
    for p in Path(sys.argv[3], d).rglob('*'):
        if p.suffix in ('.c', '.cpp', '.h') and 'tests' not in p.parts:
            src += p.read_text(errors='replace')
hexes = {int(h, 16) for h in re.findall(r'0x([0-9A-Fa-f]{8})', src)}
quoted = set(re.findall(r'"([A-Za-z_][A-Za-z0-9_]*)"', src))
per = collections.defaultdict(lambda: [0, 0, []])
for i in imports:
    p = per[i['lib']]; p[0] += 1
    nid, name = int(i['nid'], 16), i.get('name')
    if nid in have or nid in hexes or (name and name in quoted): p[1] += 1
    else: p[2].append(name or i['nid'])
tot = sum(p[0] for p in per.values()); ok = sum(p[1] for p in per.values())
print(f'covered {ok}/{tot} ({100*ok//tot}%)')
for lib, (n, c, miss) in sorted(per.items(), key=lambda x: x[1][1]-x[1][0]):
    print(f'  {lib:<16} {c:>3}/{n:<3}' + (f'  missing: {", ".join(miss)}' if miss else ''))
