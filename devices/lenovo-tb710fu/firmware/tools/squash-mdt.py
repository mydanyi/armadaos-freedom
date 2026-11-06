#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Join a Qualcomm split firmware image (NAME.mdt + NAME.bNN) into one NAME.mbn.

Same result as linux-msm/pil-squasher: the output starts with the .mdt (ELF
header, program headers and hash segment), and every program segment with file
data is written at its p_offset from NAME.bNN, where NN is the segment index.
Segments without file data have no .bNN and are skipped.

usage: squash-mdt.py NAME.mdt NAME.mbn
"""
import struct
import sys
from pathlib import Path


def program_headers(data):
    if data[:4] != b"\x7fELF":
        raise SystemExit("not an ELF file")
    is64 = data[4] == 2
    if is64:
        phoff, = struct.unpack_from("<Q", data, 0x20)
        phentsize, phnum = struct.unpack_from("<HH", data, 0x36)
        fmt = "<IIQQQQQQ"  # type, flags, offset, vaddr, paddr, filesz, memsz, align
    else:
        phoff, = struct.unpack_from("<I", data, 0x1C)
        phentsize, phnum = struct.unpack_from("<HH", data, 0x2A)
        fmt = "<IIIIIIII"  # type, offset, vaddr, paddr, filesz, memsz, flags, align
    for i in range(phnum):
        f = struct.unpack_from(fmt, data, phoff + i * phentsize)
        offset, filesz = (f[2], f[5]) if is64 else (f[1], f[4])
        yield i, offset, filesz


def main():
    mdt_path, out_path = Path(sys.argv[1]), Path(sys.argv[2])
    mdt = mdt_path.read_bytes()
    out = bytearray(mdt)
    for index, offset, filesz in program_headers(mdt):
        if filesz == 0:
            continue
        seg_path = mdt_path.with_suffix(f".b{index:02d}")
        if not seg_path.exists():
            raise SystemExit(f"missing {seg_path.name} for segment {index} ({filesz} bytes)")
        seg = seg_path.read_bytes()
        if len(seg) != filesz:
            raise SystemExit(f"{seg_path.name}: {len(seg)} bytes, program header says {filesz}")
        end = offset + filesz
        if len(out) < end:
            out.extend(b"\0" * (end - len(out)))
        out[offset:end] = seg
    out_path.write_bytes(out)


if __name__ == "__main__":
    main()
