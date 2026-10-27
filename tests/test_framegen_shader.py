"""Extract the renderer's real embedded HLSL for the GPU regression harness.

python tests/test_framegen_shader.py work/perf_20261023/framegen.hlsl
clang-cl /EHsc tests/framegen_gpu.cpp /Fe:work/perf_20261023/framegen_gpu.exe
work/perf_20261023/framegen_gpu.exe work/perf_20261023/framegen.hlsl
"""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def extract():
    source = (ROOT / 'third_party/ps3recomp/libs/video/rsx_live_draw.c').read_text(encoding='utf-8')
    source = source.split('static const char s_fg_src[] =', 1)[1].split('\n;', 1)[0]
    return ''.join(json.loads(line) for line in re.findall(r'^\s*("(?:[^"\\]|\\.)*")\s*$', source, re.M))


if __name__ == '__main__':
    Path(sys.argv[1]).write_text(extract(), encoding='utf-8')
