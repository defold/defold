"""Embed large binary fixtures without one C++ initializer per byte."""

import argparse
from pathlib import Path
import struct


def embed_binary(input_path, header_path, source_path, symbol, byte_order):
    data = input_path.read_bytes()
    header_path.parent.mkdir(parents=True, exist_ok=True)
    source_path.parent.mkdir(parents=True, exist_ok=True)
    header_path.write_text(
        f"#include <stdint.h>\n\n"
        f"extern unsigned char* {symbol};\n"
        f"extern uint32_t {symbol}_SIZE;\n", encoding="utf-8")

    # Match the target's byte order, including when the host is cross-compiling.
    word_format = "<Q" if byte_order == "LITTLE_ENDIAN" else ">Q"
    with source_path.open("w", encoding="utf-8", newline="\n") as source:
        source.write(
            '#include <stdint.h>\n#include "dlib/align.h"\n\n'
            f"static uint64_t DM_ALIGNED(16) {symbol}_DATA[] =\n{{\n")
        for offset in range(0, len(data), 64):
            chunk = data[offset:offset + 64]
            chunk += b"\0" * (-len(chunk) % 8)
            words = (f"0x{word:016x}ULL" for (word,) in struct.iter_unpack(word_format, chunk))
            source.write("    " + ", ".join(words) + ",\n")
        if not data:
            source.write("    0,\n")
        # Access through unsigned char preserves the binary representation. The
        # size excludes padding in the last word, and no runtime copying is needed.
        source.write(
            f"}};\nunsigned char* {symbol} = (unsigned char*) {symbol}_DATA;\n"
            f"uint32_t {symbol}_SIZE = {len(data)};\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--header", type=Path, required=True)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--symbol", required=True)
    parser.add_argument("--byte-order", choices=("LITTLE_ENDIAN", "BIG_ENDIAN"), required=True)
    args = parser.parse_args()
    embed_binary(args.input, args.header, args.source, args.symbol, args.byte_order)


if __name__ == "__main__":
    main()
