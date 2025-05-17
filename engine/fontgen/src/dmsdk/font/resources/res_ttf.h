// Copyright 2020-2025 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#ifndef DMSDK_FONT_RES_FONT_H
#define DMSDK_FONT_RES_FONT_H

#include <stdint.h>
#include <dmsdk/gamesys/resources/res_font.h>

namespace dmFont
{
    struct TTFResource;

    // PRIVATE

    /* Gets the path of the resource (for debugging)
    */
    const char* GetFontPath(TTFResource* resource);

    /*
     *
     */
    int CodePointToGlyphIndex(TTFResource* resource, int codepoint);

    /*
     * Calculate a scaling value based on the desired font height (in pixels)
     */
    float SizeToScale(TTFResource* resource, int size);

    /*
     * Gets the max ascent of the glyphs in the font
     */
    float GetAscent(TTFResource* resource, float scale);

    /*
     * Gets the max descent of the glyphs in the font
     */
    float GetDescent(TTFResource* resource, float scale);

    /*
     *
     */
    uint8_t* GenerateGlyphSdf(TTFResource* font, uint32_t glyph_index,
                            float scale, int padding, int edge,
                            dmGameSystem::FontGlyph* glyph);
}

#endif
