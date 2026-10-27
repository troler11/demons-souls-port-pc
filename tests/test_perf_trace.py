"""Oracle for enabled/disabled/window/threshold/cap and shared QPC units."""
import re
import subprocess
import sys


def main():
    for mode in ['off','future','on']:
        r = subprocess.run([sys.argv[1],mode], capture_output=True, text=True, check=True)
        rows = re.findall(r'\[perf-event\] kind=(\S+) qpc_us=(\d+) duration_us=(\d+) tid=(\d+) a=(\d+) b=(\d+)',r.stderr)
        if mode != 'on':
            assert not rows, r.stderr
        else:
            assert [(x[0],int(x[2]),int(x[4]),int(x[5])) for x in rows] == [('threshold',1000,7,9),('last',2000,11,13)],r.stderr
            assert all(int(x[1])>int(x[2]) and int(x[3])>0 for x in rows)
    print('Perf trace: off, start window, threshold, reversed span, zero, cap and long-uptime QPC conversion passed')


if __name__ == '__main__':
    main()
