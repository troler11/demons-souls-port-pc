"""Verify that the exported patch reproduces the actual SDK on its pinned base."""
import io
import re
import subprocess
import tarfile
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
KIT = ROOT / 'third_party/ps3recomp'
BASE = '39a36296511c445707adabcd312f0b740cfbd694'


class RuntimePatchTest(unittest.TestCase):
    def test_patch_applies_to_clean_base_and_reproduces_sources(self):
        patch = ROOT / 'patches/ps3recomp.patch'
        data = patch.read_bytes()
        self.assertNotIn(b'\x00', data, 'git apply needs a text patch, not UTF-16')
        archive = subprocess.check_output(['git', '-C', str(KIT), 'archive', BASE])
        with tempfile.TemporaryDirectory(prefix='des-patch-check-') as folder:
            dest = Path(folder)
            with tarfile.open(fileobj=io.BytesIO(archive)) as tar:
                tar.extractall(dest, filter='data')
            subprocess.run(['git', 'apply', '--check', '--whitespace=nowarn', str(patch)], cwd=dest, check=True)
            subprocess.run(['git', 'apply', '--whitespace=nowarn', str(patch)], cwd=dest, check=True)
            paths = re.findall(rb'^diff --git a/\S+ b/(\S+)$', data, re.M)
            self.assertIn(b'libs/codec/sail_host_movie.c', paths)
            self.assertIn(b'libs/system/savedata_shutdown.h', paths)
            self.assertIn(b'libs/video/rsx_framegen_present.h', paths)
            self.assertIn(b'libs/video/rsx_geom_hash.h', paths)
            self.assertIn(b'libs/video/rsx_fifo_worker.h', paths)
            self.assertIn(b'libs/video/rsx_fifo_packet.h', paths)
            self.assertIn(b'libs/video/gcm_fifo_idle.h', paths)
            self.assertIn(b'libs/video/rsx_shader_meta.h', paths)
            self.assertIn(b'libs/video/rsx_shader_preload.h', paths)
            self.assertIn(b'libs/video/rsx_pipeline_library.h', paths)
            self.assertIn(b'libs/video/rsx_descriptor_index.h', paths)
            self.assertIn(b'libs/video/rsx_perf_trace.h', paths)
            self.assertIn(b'libs/video/rsx_pso_warmup.h', paths)
            self.assertIn(b'libs/video/rsx_index_range.h', paths)
            self.assertIn(b'libs/video/rsx_present_prof.h', paths)
            self.assertIn(b'libs/video/rsx_present_stats.h', paths)
            self.assertIn(b'libs/video/xxhash.h', paths)
            self.assertIn(b'libs/video/xxhash_LICENSE.txt', paths)
            self.assertIn(b'runtime/spu/spu_regcache.h', paths)
            self.assertIn(b'runtime/spu/spu_channel_direct.h', paths)
            for raw in paths:
                path = raw.decode('utf-8')
                self.assertEqual((dest / path).read_bytes().replace(b'\r\n', b'\n'),
                                 (KIT / path).read_bytes().replace(b'\r\n', b'\n'), path)


if __name__ == '__main__':
    unittest.main()
