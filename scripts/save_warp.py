r"""Moves the character of a decrypted Demon's Souls save to a warp point, so
tests (or you) can start in any world. Offsets and warp table from
DeS-SaveEdit (Wulf2k), via DemonSave-PS3.

    python scripts\save_warp.py --list
    python scripts\save_warp.py <save folder> <warp number or part of its name>

e.g. python scripts\save_warp.py work\saves\ng\BLUS30443DEMONSS005 "Prison of Hope"

The game loads USER.DAT (the active copy), so it and every character slot
(nUSER.DAT starting 00 00 00 19) in the folder are patched.
"""
import json, os, struct, sys

WORLD, BLOCK = 0x4, 0x5
POS_SEL, POS_BASE = 0x21AE1, 0x21AE3
with open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "des_warps.json"), encoding="utf-8") as f:
    WARPS = json.load(f)


def warp(path, w):
    with open(path, "rb") as f:
        d = bytearray(f.read())
    off = POS_BASE + struct.unpack(">H", d[POS_SEL:POS_SEL + 2])[0]
    _, world, block, x, y, z, rot = w
    print(f"{os.path.basename(path)}: world {d[WORLD]} block {d[BLOCK]} -> {world} {block}")
    d[WORLD], d[BLOCK] = world, block
    d[off:off + 12] = struct.pack(">3f", x, y, z)
    d[off + 0x14:off + 0x18] = struct.pack(">f", rot)
    with open(path, "wb") as f:
        f.write(d)


def main():
    if len(sys.argv) < 2 or sys.argv[1] == "--list":
        for i, w in enumerate(WARPS):
            print(i, w[0])
        return
    folder, sel = sys.argv[1], sys.argv[2]
    w = WARPS[int(sel)] if sel.isdigit() else next(w for w in WARPS if sel.lower() in w[0].lower())
    print(w[0])
    for f in sorted(os.listdir(folder)):
        p = os.path.join(folder, f)
        if f.upper().endswith("USER.DAT"):
            with open(p, "rb") as fh:
                if fh.read(4) == b"\0\0\0\x19":
                    warp(p, w)


if __name__ == "__main__":
    main()
