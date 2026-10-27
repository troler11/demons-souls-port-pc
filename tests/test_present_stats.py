"""Compare the native collector's output with independently sorted intervals."""
import math
import re
import statistics
import subprocess
import sys


def main():
    run = subprocess.run([sys.argv[1]], capture_output=True, text=True, check=True)
    rows = re.findall(
        r'recorded=(\d+) seconds=([\d.]+) fps=([\d.]+) '
        r'p50=([\d.]+)ms p95=([\d.]+)ms p99=([\d.]+)ms max=([\d.]+)ms',
        run.stderr,
    )
    expected = [[20.0] * 250] * 2
    window = []
    for i in range(1, 601):
        window.append(100.0 if i % 50 == 0 else 20.0)
        if sum(window) >= 5000:
            expected.append(window)
            window = []
    expected += [[1.0] * 16383, [20.0] * 250]
    assert len(rows) == len(expected), (rows, len(expected))
    for row, values in zip(rows, expected):
        ordered = sorted(values)
        reference = [len(values), sum(values) / 1000,
                     1000 / statistics.mean(values),
                     ordered[(len(values) - 1) // 2],
                     ordered[math.ceil(len(values) * .95) - 1],
                     ordered[math.ceil(len(values) * .99) - 1], max(values)]
        assert all(abs(float(a) - b) <= .00051 for a, b in zip(row, reference)), (row, reference)
    counts = re.findall(r'over33=(\d+) over50=(\d+)', run.stderr)
    assert len(counts) == len(expected)
    for (over33, over50), values in zip(counts, expected):
        assert int(over33) == sum(v > 1000 / 30 for v in values)
        assert int(over50) == sum(v > 50 for v in values)
    print(f'Present interval metrics: {len(rows)} windows match independent oracle; stalls, wrap, restart passed')


if __name__ == '__main__':
    main()
