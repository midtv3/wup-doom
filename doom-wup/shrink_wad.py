#!/usr/bin/env python3
"""Shrink a Doom IWAD for the GamePad port without changing any kept lump.

Keeps: E1M1 and its geometry, all music, sounds, sprites, textures, graphics.
Drops: the other level markers/geometry (E1M2..E1M9) and DEMO1..DEMO3, which
the patched engine no longer needs for the title screen.

Usage: python shrink_wad.py DOOM1.WAD small.wad
"""
import hashlib
import struct
import sys

LEVEL_DATA = {
    b"THINGS", b"LINEDEFS", b"SIDEDEFS", b"VERTEXES", b"SEGS",
    b"SSECTORS", b"NODES", b"SECTORS", b"REJECT", b"BLOCKMAP",
}
DEMOS = {b"DEMO1", b"DEMO2", b"DEMO3"}
KEEP_LEVEL = b"E1M1"


def is_level(name):
    return len(name) == 4 and name[:1] == b"E" and name[1:2].isdigit() \
        and name[2:3] == b"M" and name[3:4].isdigit()


def main():
    src, dst = sys.argv[1], sys.argv[2]
    data = open(src, "rb").read()
    magic = data[:4]
    if magic not in (b"IWAD", b"PWAD"):
        sys.exit("not a WAD file")
    count = struct.unpack_from("<i", data, 4)[0]
    diroff = struct.unpack_from("<i", data, 8)[0]

    lumps = []
    for i in range(count):
        p = diroff + 16 * i
        off, size = struct.unpack_from("<ii", data, p)
        name = data[p + 8:p + 16].split(b"\0")[0]
        lumps.append((name, off, size))

    kept, dropped = [], []
    level = None
    for name, off, size in lumps:
        if is_level(name):
            level = name
            keep = name == KEEP_LEVEL
        elif name in LEVEL_DATA and level is not None:
            keep = level == KEEP_LEVEL
        else:
            level = None
            keep = name not in DEMOS
        (kept if keep else dropped).append((name, off, size))

    out = bytearray(magic + struct.pack("<ii", len(kept), 0))
    new = []
    seen = {}                          # (size, sha1) -> offset of identical data
    dedup = 0
    for name, off, size in kept:
        blob = data[off:off + size]
        key = (size, hashlib.sha1(blob).digest()) if size else None
        if key is not None and key in seen:
            new.append((name, seen[key], size))
            dedup += size
            continue
        while len(out) % 4:
            out.append(0)
        new.append((name, len(out), size))
        if key is not None:
            seen[key] = len(out)
        out += blob
    struct.pack_into("<i", out, 8, len(out))
    for name, off, size in new:
        out += struct.pack("<ii", off, size) + name.ljust(8, b"\0")[:8]

    open(dst, "wb").write(out)

    saved = sum(size for _, _, size in dropped)
    print("%s: %d -> %d bytes (%.2f MB -> %.2f MB, saved %.2f MB)"
          % (dst, len(data), len(out), len(data) / 1048576, len(out) / 1048576, saved / 1048576))
    print("kept %d lumps, dropped %d, deduplicated %d bytes" % (len(kept), len(dropped), dedup))
    print("dropped levels:", [n.decode() for n, _, _ in dropped if is_level(n)])
    print("dropped demos:", [n.decode() for n, _, _ in dropped if n in DEMOS])
    print("music kept:", [n.decode() for n, _, _ in kept if n.startswith(b"D_")])


if __name__ == "__main__":
    main()
