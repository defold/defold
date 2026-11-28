#!/usr/bin/env python3
# Copyright 2020-2026 The Defold Foundation
# Licensed under the Defold License version 1.0 (https://www.defold.com/license).
"""Generate complete mip chains and CPU-decoded references for the texture captures."""
import argparse
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from PIL import Image, ImageDraw


def grid():
    # Fixed glyphs keep the fixture independent of installed fonts and rasterizers.
    glyphs = {
        'A': (14, 17, 17, 31, 17, 17, 17), 'B': (30, 17, 17, 30, 17, 17, 30),
        'C': (14, 17, 16, 16, 16, 17, 14), 'D': (30, 17, 17, 17, 17, 17, 30),
        'E': (31, 16, 16, 30, 16, 16, 31), 'F': (31, 16, 16, 30, 16, 16, 16),
        'G': (14, 17, 16, 23, 17, 17, 14), 'H': (17, 17, 17, 31, 17, 17, 17),
        '1': (4, 12, 4, 4, 4, 4, 14), '2': (14, 17, 1, 2, 4, 8, 31),
        '3': (30, 1, 1, 14, 1, 1, 30), '4': (2, 6, 10, 18, 31, 2, 2),
        '5': (31, 16, 16, 30, 1, 1, 30), '6': (14, 16, 16, 30, 17, 17, 14),
        '7': (31, 1, 2, 4, 8, 8, 8), '8': (14, 17, 17, 14, 17, 17, 14),
    }
    colors = ((85, 139, 123), (246, 230, 151), (243, 173, 112), (218, 99, 87))
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
                            draw.rectangle((left, top, left + 1, top + 1), fill=(18, 24, 21, 255))
    return image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--encoder', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parent
    source = grid()
    encoder = args.encoder.resolve()
    formats = re.findall(r'^TEXTURE_FORMAT_CASE\((\w+), \w+, (\d+), (\d+), (\d+)\)',
                         (root / 'formats.inc').read_text(), re.MULTILINE)
    mip_tails = {suffix: bytearray() for suffix, _, _, _ in formats}
    with tempfile.TemporaryDirectory() as temporary:
        work = Path(temporary) / 'texture_formats'
        work.mkdir()
        (work.parent / 'graphics_reference').mkdir()
        for mip in range(1, source.width.bit_length()):
            size = source.width >> mip
            # PVRTC1 4bpp requires at least 8x8 texels of storage.
            encoded_size = max(8, size)
            gray = mip * 28
            (work / 'testimage.rgba').write_bytes(bytes((gray, gray, gray, 255)) * encoded_size**2)
            subprocess.run([encoder, work, str(encoded_size)], check=True)
            for suffix, width, height, block_bytes in formats:
                extent = encoded_size if suffix.startswith('pvrtc') else size
                columns = (extent + int(width) - 1) // int(width)
                rows = (extent + int(height) - 1) // int(height)
                # The small mips are solid, so their first blocks provide all logical texels.
                byte_count = columns * rows * int(block_bytes)
                payload = (work / ('testimage.' + suffix)).read_bytes()
                if len(payload) < byte_count:
                    raise ValueError(f'Incomplete {suffix} payload at mip {mip}')
                mip_tails[suffix].extend(payload[:byte_count])

        # Encode the labelled base last so its CPU references are the ones we publish.
        (work / 'testimage.rgba').write_bytes(source.tobytes())
        subprocess.run([encoder, work, str(source.width)], check=True)
        for suffix, tail in mip_tails.items():
            filename = 'testimage.' + suffix
            (root / filename).write_bytes((work / filename).read_bytes() + tail)
        for image in (work.parent / 'graphics_reference').glob('texture_*.png'):
            shutil.copyfile(image, root.parent / 'graphics_reference' / image.name)
        source.save(root / 'testimage.png')
        source.save(root.parent / 'graphics_reference' / 'texture_rgba.png')


if __name__ == '__main__':
    main()
