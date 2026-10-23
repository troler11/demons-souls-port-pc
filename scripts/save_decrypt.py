r"""Decrypts a protected Demon's Souls save folder (the kind a real PS3, Apollo or
a save site gives you: PARAM.PFD + encrypted nUSER.DAT files) into a plain
folder that this port and RPCS3 read. Can also retarget an EU/JP save to the
US title id.

    python scripts\save_decrypt.py <in\BLES00932DEMONSS005> <out\BLUS30443DEMONSS005> [--title BLUS30443]

The keys are not in this repository. Put them in work\keys.conf (the same
names pfdtool's global.conf and games.conf use):

    syscon_manager_key=<32 hex digits>
    secure_file_id=<32 hex digits, Demon's Souls' own>

Needs pycryptodome (pip install pycryptodome).

Algorithm (pfdtool, flatz): each protected file's key is
AES-CBC-decrypt(syscon_manager_key, iv = hash key from the secure file id,
PFD entry key)[:16]; block i of the file is
AES-ECB-dec(key, c_i) XOR AES-ECB-enc(key, be64(i) || 0^8).
"""
import os, struct, sys
from Crypto.Cipher import AES

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def load_keys():
    path = os.path.join(ROOT, "work", "keys.conf")
    keys = {}
    if os.path.exists(path):
        for line in open(path, encoding="utf-8"):
            if "=" in line and not line.lstrip().startswith(";"):
                k, v = line.split("=", 1)
                keys[k.strip().lower()] = v.strip()
    for k in ("syscon_manager_key", "secure_file_id"):
        if len(keys.get(k, "")) != 32:
            sys.exit(f"missing {k} in {path} (see the top of this script)")
    return bytes.fromhex(keys["syscon_manager_key"]), bytes.fromhex(keys["secure_file_id"])


def secure_hash_key(sid):
    # 20 bytes: the 16-byte id with fixed bytes at 1, 2, 5 and 8.
    fixed = {1: 11, 2: 15, 5: 14, 8: 10}
    out, j = bytearray(20), 0
    for i in range(20):
        if i in fixed:
            out[i] = fixed[i]
        else:
            out[i] = sid[j]; j += 1
    return bytes(out)


def parse_pfd(d):
    if d[4:8] != b"PFDB":
        sys.exit("not a PARAM.PFD")
    off = 96
    n_res, _n_tot, n_used = struct.unpack(">QQQ", d[off:off + 24]); off += 24 + 8 * n_res
    ents = []
    for _ in range(n_used):
        name = d[off + 8:off + 73].split(b"\0")[0].decode()
        ents.append((name, d[off + 80:off + 144], struct.unpack(">Q", d[off + 0x108:off + 0x110])[0]))
        off += 0x110
    return ents


def decrypt(data, key, size):
    n = (len(data) + 15) // 16
    ecb = AES.new(key, AES.MODE_ECB)
    ks = ecb.encrypt(b"".join(struct.pack(">Q", i) + bytes(8) for i in range(n)))
    pt = ecb.decrypt(data.ljust(n * 16, b"\0"))
    return bytes(a ^ b for a, b in zip(pt, ks))[:size]


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    src, dst = sys.argv[1], sys.argv[2]
    title = sys.argv[sys.argv.index("--title") + 1] if "--title" in sys.argv else None
    syscon, sid = load_keys()
    iv = secure_hash_key(sid)[:16]
    keys = {}
    for name, ekey, size in parse_pfd(open(os.path.join(src, "PARAM.PFD"), "rb").read()):
        if name.upper() != "PARAM.SFO":
            keys[name.upper()] = (AES.new(syscon, AES.MODE_CBC, iv).decrypt(ekey)[:16], size)
    os.makedirs(dst, exist_ok=True)
    for f in sorted(os.listdir(src)):
        p = os.path.join(src, f)
        if not os.path.isfile(p) or f.upper() == "PARAM.PFD":
            continue
        d = open(p, "rb").read()
        if f.upper() in keys:
            d = decrypt(d, *keys[f.upper()])
            print(f"{f}: decrypted")
        if f.upper() == "PARAM.SFO" and title:
            old = os.path.basename(os.path.normpath(src))[:9]
            d = d.replace(old.encode(), title.encode())   # same length ids
        open(os.path.join(dst, f), "wb").write(d)
    # A character file starts 00 00 00 19; a wrong key gives noise.
    chars = [f for f in os.listdir(dst) if f.upper().endswith("USER.DAT")
             and open(os.path.join(dst, f), "rb").read(4) == b"\0\0\0\x19"]
    print(f"{len(keys)} files decrypted -> {dst}; characters: {', '.join(chars) or 'none (wrong key?)'}")


if __name__ == "__main__":
    main()
