import importlib.util
import struct
import unittest
from pathlib import Path

spec = importlib.util.spec_from_file_location('hooks', Path(__file__).parents[1] / 'scripts/apply_title_hooks.py')
hooks = importlib.util.module_from_spec(spec)
spec.loader.exec_module(hooks)


def elf_for(words):
    elf = bytearray(0x200)
    struct.pack_into('>Q', elf, 0x20, 0x40)
    struct.pack_into('>HH', elf, 0x36, 56, 1)
    struct.pack_into('>IIQQQQQQ', elf, 0x40, 1, 5, 0x100, 0x10000, 0, 0x100, 0x100, 4)
    for i, word in enumerate(words):
        struct.pack_into('>I', elf, 0x100 + 4 * i, word)
    return bytes(elf)


class HooksTest(unittest.TestCase):
    def test_override_survives_regeneration_and_is_idempotent(self):
        text = '#include "ppu_recomp.h"\nvoid func_00010000(ppu_context* ctx) {\n return;\n}\n'
        result = hooks.patch_unit(text, elf_for([0]), {0x10000: 'des_move_post'}, [])
        self.assertIn('des_move_post(ctx);', result)
        self.assertEqual(result, hooks.patch_unit(result, elf_for([0]), {0x10000: 'des_move_post'}, []))

    def test_identical_loads_use_instruction_ordinal(self):
        load = '{ uint32_t tmp = vm_read32(ctx->gpr[31] + 0x17C); float ftmp; memcpy(&ftmp, &tmp, 4); ctx->fpr[0] = ftmp; }'
        text = '#include "ppu_recomp.h"\nvoid func_00010000(ppu_context* ctx) {\n' + load + '\n' + load + '\n}\nvoid func_00010008(ppu_context* ctx) {\n}\n'
        insn = 48 << 26 | 31 << 16 | 0x17C
        result = hooks.patch_unit(text, elf_for([insn, insn]), {}, [0x10004])
        self.assertIn(load + '\n' + load + ' des_cam_rate(ctx, 0x00010004u);', result)
        self.assertEqual(result, hooks.patch_unit(result, elf_for([insn, insn]), {}, [0x10004]))

    def test_wrong_binary_is_rejected(self):
        text = '#include "ppu_recomp.h"\nvoid func_00010000(ppu_context* ctx) {\n}\n'
        with self.assertRaises(ValueError):
            hooks.patch_unit(text, elf_for([0]), {}, [0x10000])


if __name__ == '__main__':
    unittest.main()
