"""Export the patched SDK as UTF-8, including the host movie source.

Never stages SDK files. Run after editing third_party/ps3recomp.
"""
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
KIT = ROOT / 'third_party/ps3recomp'
BASE = '39a36296511c445707adabcd312f0b740cfbd694'


def main():
    head = subprocess.check_output(['git', '-C', str(KIT), 'rev-parse', 'HEAD'], text=True).strip()
    if head != BASE:
        raise RuntimeError(f'Unexpected SDK base {head}; expected {BASE}')
    patch = subprocess.check_output(['git', '-C', str(KIT), 'diff', '--binary', BASE, '--'])
    # SDK additions are untracked in the embedded checkout. Include the
    # required sources without staging or changing the user's index.
    for name in ('libs/codec/sail_host_movie.c', 'libs/video/rsx_framegen_present.h',
                 'libs/system/savedata_shutdown.h',
                 'libs/video/rsx_geom_hash.h', 'libs/video/rsx_fifo_worker.h',
                 'libs/video/rsx_fifo_packet.h', 'libs/video/gcm_fifo_idle.h',
                 'libs/video/rsx_shader_meta.h',
                 'libs/video/rsx_shader_preload.h', 'libs/video/rsx_pipeline_library.h',
                 'libs/video/rsx_descriptor_index.h',
                 'libs/video/rsx_perf_trace.h',
                 'libs/video/rsx_pso_warmup.h',
                 'libs/video/rsx_index_range.h', 'libs/video/rsx_present_prof.h',
                 'libs/video/rsx_present_stats.h', 'libs/video/xxhash.h',
                 'libs/video/xxhash_LICENSE.txt', 'runtime/spu/spu_regcache.h',
                 'runtime/spu/spu_channel_direct.h', 'runtime/ppu/tests/test_vmx_fp_sse.c'):
        if f'diff --git a/{name} b/{name}'.encode() not in patch:
            source = (KIT / name).read_bytes().replace(b'\r\n', b'\n')
            lines = source.splitlines(keepends=True)
            patch += (f'diff --git a/{name} b/{name}\nnew file mode 100644\n'
                      f'--- /dev/null\n+++ b/{name}\n@@ -0,0 +1,{len(lines)} @@\n').encode()
            for line in lines:
                patch += b'+' + line
                if not line.endswith(b'\n'):
                    patch += b'\n\\ No newline at end of file\n'
    output = ROOT / 'patches/ps3recomp.patch'
    temporary = output.with_suffix('.patch.tmp')
    temporary.write_bytes(patch)
    subprocess.run(['git', '-C', str(KIT), 'apply', '--reverse', '--check',
                    '--whitespace=nowarn', str(temporary)], check=True)
    temporary.replace(output)
    print(f'Runtime patch verified: {len(patch)} bytes, UTF-8, SDK additions included')


if __name__ == '__main__':
    main()
