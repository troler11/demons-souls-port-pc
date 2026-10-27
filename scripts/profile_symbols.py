"""Resolve PS3_SAMPLE RVA reports with the exact executable's linker map.

Use PS3_SAMPLE_FUNCTIONS=1 to aggregate wide helpers before the runtime's
top-25 cutoff. Percentages are wall-time samples of the selected thread,
not percentages of total CPU or predicted FPS improvements.
"""
import argparse
import bisect
from collections import Counter, defaultdict
import json
from pathlib import Path
import re


def resolve(log, link_map):
    base = int(re.search(r'Preferred load address is\s+([0-9a-fA-F]+)', link_map)[1], 16)
    symbols = defaultdict(list)
    for line in link_map.splitlines():
        match = re.match(r'\s+[0-9a-fA-F]{4}:[0-9a-fA-F]{8}\s+(\S+)\s+([0-9a-fA-F]{16})\b', line)
        if match:
            symbols[int(match[2], 16) - base].append(match[1])
    addresses = sorted(symbols)
    if not addresses:
        raise ValueError('No symbols in linker map')
    chunks = re.split(r'\[samp\] --- report at t=(\d+)ms ---', log)
    reports = []
    for i in range(1, len(chunks), 2):
        total = re.search(r'\[samp\] (\d+) samples', chunks[i + 1])
        if not total:
            continue
        counts = Counter()
        for rva, count in re.findall(r'\[samp\] rva 0x([0-9A-Fa-f]+)\s+[\d.]+% \((\d+)\)', chunks[i + 1]):
            index = bisect.bisect_right(addresses, int(rva, 16)) - 1
            if index >= 0:
                counts[symbols[addresses[index]][0]] += int(count)
        reports.append(dict(ms=int(chunks[i]), samples=int(total[1]), symbols=dict(counts)))
    return reports


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('map', type=Path)
    parser.add_argument('--from-ms', type=int, default=60000)
    parser.add_argument('--out', type=Path)
    args = parser.parse_args()
    reports = resolve(args.log.read_text(errors='replace'), args.map.read_text(errors='replace'))
    counts = Counter()
    total = 0
    for report in reports:
        if report['ms'] >= args.from_ms:
            total += report['samples']
            counts.update(report['symbols'])
    if not total:
        raise SystemExit('No reports after requested start time')
    print(f'{total} selected-thread wall-time samples after {args.from_ms} ms')
    if sum(counts.values()) > total:
        print('WARNING: RVA counts exceed sample count; an older sampler may double-count runtime frames')
    for name, count in counts.most_common(25):
        print(f'{count:6d} {100 * count / total:6.2f}% {name}')
    if args.out:
        args.out.write_text(json.dumps(dict(reports=reports, samples=total,
            symbols=dict(counts.most_common())), indent=2), encoding='utf-8')


if __name__ == '__main__':
    main()
