"""Build actual renderer table functions with observable descriptor-copy stubs.

Run from an x64 developer terminal. This tests ring/lookup semantics, not D3D12
GPU execution; use isolated gameplay/debug runs for the latter.
"""
import argparse
from pathlib import Path
import re
import subprocess
import os

ROOT = Path(__file__).resolve().parents[1]


def function(source, name):
    start = re.search(r'^static [^\n]+\b' + name + r'\(', source, re.M).start()
    brace = source.index('{', start)
    # Selected functions have balanced braces even in their comments/strings.
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end] + '\n'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--out', required=True, type=Path)
    args = parser.parse_args()
    folder = args.out.resolve()
    folder.mkdir(parents=True, exist_ok=True)
    source = (ROOT/'third_party/ps3recomp/libs/video/rsx_live_draw.c').read_text(encoding='utf-8')
    assert source.count('ld_descriptor_tables_reset();') == 5, 'audit every ring reset and shutdown'
    begin = source.index('static u32 g_srv_cpu_gen;')
    end = source.index('static void srv_write(', begin)
    generated = function(source, 'ld_debug_flag_on') + source[begin:end]
    for name in ['srv_cpu', 'srv_table', 'smp_cpu', 'sampler_table', 'sampler_table_needs_flush']:
        if name != 'srv_cpu': generated += function(source, name)
    # srv_cpu is defined before the new globals and is needed by srv_cpu_w.
    generated = function(source, 'srv_cpu') + generated
    (folder/'descriptor_functions.inc').write_text(generated, encoding='utf-8')
    exe = folder/'descriptor_tables.exe'
    subprocess.run(['clang-cl','/O2','/EHsc','/D_CRT_SECURE_NO_WARNINGS',
                    '/I'+str(ROOT/'third_party/ps3recomp/libs/video'), '/I'+str(folder),
                    str(ROOT/'tests/descriptor_tables.cpp'), '/Fo:'+str(folder/'descriptor_tables.obj'),
                    '/Fe:'+str(exe)], check=True, cwd=folder)
    for mode in ['0','1','unset','no-srv-reuse']:
        subprocess.run([str(exe),mode], check=True,
                       env={**os.environ,'RSX_D3D_DEBUG':'','RSX_D3D_GBV':''})


if __name__ == '__main__':
    main()
