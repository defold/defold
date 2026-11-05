#!/usr/bin/env python3
# Copyright 2020-2026 The Defold Foundation
# Copyright 2014-2020 King
# Copyright 2009-2014 Ragnar Svensson, Christian Murray
# Licensed under the Defold License version 1.0 (the "License"); you may not use
# this file except in compliance with the License.
#
# You may obtain a copy of the License, together with FAQs at
# https://www.defold.com/license
#
# Unless required by applicable law or agreed to in writing, software distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.

"""Regenerate data/color-test.ttf: outlines with analytically known COLR paints.

A: black/white gradient; B: white, clipped to half width; C: B's full outline;
D: half-transparent white over black; E: foreground; F/G: gray 128/8;
H: opaque black to transparent white; I: gray 8/128 gradient.
J-P use triangles: J: white; K: shared-clip white composite; L: separate clips;
M: shared-clip layer list; N: nested composite; O: two half-white fills in a
shared composite; P: single 75%-white fill.
Uses only Python's standard library; all font data is generated here.
"""

from pathlib import Path
from struct import pack, pack_into, unpack


def checksum(data):
    data += bytes(-len(data) % 4)
    return sum(unpack(f">{len(data) // 4}I", data)) & 0xffffffff


def write_font(tables, path):
    count = len(tables)
    power = 1 << (count.bit_length() - 1)
    directory = bytearray(pack(">IHHHH", 0x10000, count, power * 16, power.bit_length() - 1, (count - power) * 16))
    offset = 12 + 16 * count
    payload = bytearray()
    for tag, data in sorted(tables.items()):
        directory += pack(">4sIII", tag, checksum(data), offset, len(data))
        if tag == b"head":
            head_offset = offset
        padded = data + bytes(-len(data) % 4)
        payload += padded
        offset += len(padded)
    font = directory + payload
    pack_into(">I", font, head_offset + 8, (0xb1b0afba - checksum(font)) & 0xffffffff)
    path.write_bytes(font)


def u24(value):
    return value.to_bytes(3, "big")


def solid(palette, alpha=16384):
    return bytes([2]) + pack(">HH", palette, alpha)


def glyph(index, paint):
    return bytes([10]) + u24(6) + pack(">H", index) + paint


def gradient(first, last, last_alpha=16384):
    return (bytes([4]) + u24(16) + pack(">hhhhhh", 0, 0, 1000, 0, 0, 1000)
            + pack(">BH", 0, 2) + pack(">hHHhHH", 0, first, 16384, 16384, last, last_alpha))


def composite(source, backdrop):
    return bytes([32]) + u24(8) + bytes([3]) + u24(8 + len(source)) + source + backdrop


def make_font(path):
    count = 17  # .notdef and A-I are squares; J-P are right triangles.
    tables = {}
    head = bytearray(54)
    pack_into(">IIIIHH", head, 0, 0x10000, 0x10000, 0, 0x5f0f3cf5, 3, 1000)
    pack_into(">hhhhHHhhh", head, 36, 0, 0, 1000, 1000, 0, 8, 2, 1, 0)
    tables[b"head"] = head
    hhea = bytearray(36)
    pack_into(">IhhhHhhhhhh", hhea, 0, 0x10000, 1000, 0, 0, 1000, 0, 0, 1000, 1, 0, 0)
    pack_into(">H", hhea, 34, count)
    tables[b"hhea"] = hhea
    tables[b"maxp"] = pack(">IH13H", 0x10000, count, 4, 1, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0)
    tables[b"post"] = pack(">I", 0x30000) + bytes(28)
    os2 = bytearray(78)
    pack_into(">HhHH", os2, 0, 0, 1000, 400, 5)
    pack_into(">I", os2, 42, 1)  # Basic Latin.
    os2[58:62] = b"DFLD"
    pack_into(">HHHhhhHH", os2, 62, 0x40, 65, 80, 1000, 0, 0, 1000, 0)
    tables[b"OS/2"] = os2
    names = [(1, "Defold COLR Test"), (2, "Regular"), (4, "Defold COLR Test Regular"), (6, "DefoldCOLRTest")]
    records, strings = bytearray(), bytearray()
    for name_id, name in names:
        encoded = name.encode("utf-16-be")
        records += pack(">6H", 3, 1, 0x409, name_id, len(encoded), len(strings))
        strings += encoded
    tables[b"name"] = pack(">HHH", 0, len(names), 6 + 12 * len(names)) + records + strings
    outline = pack(">hhhhhHH", 1, 0, 0, 1000, 1000, 3, 0) + bytes([1] * 4) + pack(">8h", 0, 1000, 0, -1000, 0, 0, 1000, 0)
    triangle = pack(">hhhhhHH", 1, 0, 0, 1000, 1000, 2, 0) + bytes([1] * 3) + pack(">6h", 0, 1000, -1000, 0, 0, 1000)
    outlines = [outline] * 10 + [triangle] * 7
    offsets = [0]
    for shape in outlines:
        offsets.append(offsets[-1] + len(shape))
    tables[b"glyf"] = b"".join(outlines)
    tables[b"loca"] = pack(f">{count + 1}I", *offsets)
    tables[b"hmtx"] = pack(">Hh", 1000, 0) * count
    tables[b"cmap"] = pack(">HHHHI", 0, 1, 3, 10, 12) + pack(">HHIIIIII", 12, 0, 28, 0, 1, 65, 80, 1)
    tables[b"CPAL"] = pack(">HHHHIH", 0, 4, 1, 4, 14, 0) + bytes([0, 0, 0, 255, 255, 255, 255, 255, 128, 128, 128, 255, 8, 8, 8, 255])
    source, backdrop = glyph(0, solid(1, 8192)), glyph(0, solid(0))
    paints = [glyph(0, gradient(0, 1)), glyph(0, solid(1)), glyph(2, solid(1)), composite(source, backdrop),
              glyph(0, solid(0xffff)), glyph(0, solid(2)), glyph(0, solid(3)),
              glyph(0, gradient(0, 1, 0)), glyph(0, gradient(3, 2)),
              glyph(10, solid(1)), glyph(10, composite(solid(1), solid(1))),
              composite(glyph(10, solid(1)), glyph(10, solid(1))),
              glyph(10, bytes([1, 2]) + pack(">I", 0)),
              glyph(10, composite(solid(1), composite(solid(0), solid(0)))),
              glyph(10, composite(solid(1, 8192), solid(1, 8192))), glyph(10, solid(1, 12288))]
    records, data = bytearray(), bytearray()
    for index, paint in enumerate(paints, 1):
        records += pack(">HI", index, 4 + 6 * len(paints) + len(data))
        data += paint
    base_glyphs = pack(">I", len(paints)) + records + data
    # Only B has a clip box. C must ignore it when using B's raw outline.
    clip = pack(">BIHH", 1, 1, 2, 2) + u24(12) + pack(">Bhhhh", 1, 0, 0, 500, 1000)
    layers = pack(">III", 2, 12, 17) + solid(1) + solid(1)
    tables[b"COLR"] = pack(">HHIIHIIIII", 1, 0, 0, 0, 0, 34, 34 + len(base_glyphs) + len(clip), 34 + len(base_glyphs), 0, 0) + base_glyphs + clip + layers
    write_font(tables, path)


if __name__ == "__main__":
    make_font(Path(__file__).with_name("data") / "color-test.ttf")
