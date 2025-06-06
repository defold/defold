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

#if !defined(DM_FONT_H)
#define DM_FONT_H

#include <stdint.h>

namespace dmFont
{
    /*#
     * Represents a glyph.
     * If there's an associated image, it is of size width * height * channels.
     * @struct
     * @name FontGlyph
     * @member m_Width [type: float] The glyph bounding width
     * @member m_Height [type: float] The glyph bounding height
     * @member m_ImageWidth [type: int16_t] The glyph image width
     * @member m_ImageHeight [type: int16_t] The glyph image height
     * @member m_Channels [type: int16_t] The glyph image height
     * @member m_Advance [type: float] The advance step of the glyph (in pixels)
     * @member m_LeftBearing [type: float] The left bearing of the glyph (in pixels)
     * @member m_Ascent [type: float] The ascent of the glyph. (in pixels)
     * @member m_Descent [type: float] The descent of the glyph. Positive! (in pixels)
     */
    struct Glyph
    {
        float   m_Width;
        float   m_Height;
        // int16_t m_ImageWidth;
        // int16_t m_ImageHeight;
        // int16_t m_Channels;
        float   m_Advance;
        float   m_LeftBearing;
        float   m_Ascent;
        float   m_Descent;
    };

    typedef void* HFont;

    HFont LoadFontFromPath(const char* path);
    HFont LoadFontFromMemory(const char* name, void* data, uint32_t data_size);

    void  DestroyFont(HFont font);

    // For reporting at runtime. It returns the size of the font (in bytes)
    uint32_t GetResourceSize(HFont font);


    enum FontResult
    {
        RESULT_OK,
        RESULT_ERROR,
    };

    float      GetPixelScaleFromSize(HFont font, float size);
    FontResult GetGlyph(HFont font, uint32_t codepoint, float scale, Glyph* glyph);

} // namespace

#endif // DM_FONT_H
