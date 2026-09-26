#!/usr/bin/env python3
# Copyright 2020-2026 The Defold Foundation
# Licensed under the Defold License version 1.0 (https://www.defold.com/license).
"""Generate the labelled source grid and invoke the offline texture encoder."""
import argparse
from pathlib import Path
import subprocess
from PIL import Image, ImageDraw


def grid():
    # Fixed glyphs keep the fixture independent of installed fonts and rasterizers.
    glyphs = {
        'A': (14,17,17,31,17,17,17), 'B': (30,17,17,30,17,17,30),
        'C': (14,17,16,16,16,17,14), 'D': (30,17,17,17,17,17,30),
        'E': (31,16,16,30,16,16,31), 'F': (31,16,16,30,16,16,16),
        'G': (14,17,16,23,17,17,14), 'H': (17,17,17,31,17,17,17),
        '1': (4,12,4,4,4,4,14), '2': (14,17,1,2,4,8,31),
        '3': (30,1,1,14,1,1,30), '4': (2,6,10,18,31,2,2),
        '5': (31,16,16,30,1,1,30), '6': (14,16,16,30,17,17,14),
        '7': (31,1,2,4,8,8,8), '8': (14,17,17,14,17,17,14),
    }
    colors = ((85,139,123), (246,230,151), (243,173,112), (218,99,87))
    image = Image.new('RGBA', (256, 256))
    draw = ImageDraw.Draw(image)
    for row in range(8):
        for column in range(8):
            x, y = column * 32, row * 32
            draw.rectangle((x, y, x + 31, y + 31), fill=colors[(column - row) % 4] + (255,))
            for index, character in enumerate(chr(65 + row) + str(column + 1)):
                for gy, bits in enumerate(glyphs[character]):
                    for gx in range(5):
                        if bits & (1 << (4 - gx)):
                            left, top = x + 5 + index * 12 + gx * 2, y + 9 + gy * 2
                            draw.rectangle((left, top, left + 1, top + 1), fill=(18,24,21,255))
    return image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--encoder', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parent
    source = grid()
    source.save(root / 'testimage.png')
    source.save(root.parent / 'graphics_reference' / 'texture_rgba.png')
    # All GPU payloads are top-down, one mip, tightly packed.
    (root / 'testimage.rgba').write_bytes(source.tobytes())
    subprocess.run([str(args.encoder.resolve()), str(root)], check=True)


if __name__ == '__main__':
    main()
