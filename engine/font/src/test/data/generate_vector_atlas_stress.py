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

"""Regenerate vector_atlas_stress.ttf with fontTools (not needed to run tests)."""

from pathlib import Path

from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen


# 800 distinct glyph indices share one 256-curve outline through composites,
# keeping the fixture small while overflowing the production Slug band atlas.
names = [".notdef"] + [f"g{i}" for i in range(800)]
pen = TTGlyphPen(None)
for index in range(64):
    radius = 350 - index * 3
    pen.moveTo((450 + radius, 450))
    pen.qCurveTo((450 + radius, 450 + radius), (450, 450 + radius))
    pen.qCurveTo((450 - radius, 450 + radius), (450 - radius, 450))
    pen.qCurveTo((450 - radius, 450 - radius), (450, 450 - radius))
    pen.qCurveTo((450 + radius, 450 - radius), (450 + radius, 450))
    pen.closePath()
glyphs = {".notdef": pen.glyph()}
for name in names[1:]:
    pen = TTGlyphPen(glyphs)
    pen.addComponent(".notdef", (1, 0, 0, 1, 0, 0))
    glyphs[name] = pen.glyph()

builder = FontBuilder(1000, isTTF=True)
builder.setupGlyphOrder(names)
builder.setupCharacterMap({0xE000 + index: name for index, name in enumerate(names[1:])})
builder.setupGlyf(glyphs)
builder.setupHorizontalMetrics({name: (900, 100) for name in names})
builder.setupHorizontalHeader(ascent=900, descent=-100)
builder.setupNameTable({
    "familyName": "Vector Atlas Stress",
    "styleName": "Regular",
    "uniqueFontIdentifier": "DefoldVectorAtlasStress",
    "fullName": "Vector Atlas Stress Regular",
    "psName": "VectorAtlasStress-Regular",
    "copyright": "Copyright 2020-2026 The Defold Foundation",
    "licenseDescription": "Licensed under the Defold License version 1.0.",
    "licenseInfoURL": "https://www.defold.com/license",
})
builder.setupOS2(sTypoAscender=900, sTypoDescender=-100, usWinAscent=900, usWinDescent=100)
builder.setupPost()
builder.setupMaxp()
builder.setupHead(created=2082844800, modified=2082844800)
builder.font.recalcTimestamp = False
builder.save(Path(__file__).with_name("vector_atlas_stress.ttf"))
