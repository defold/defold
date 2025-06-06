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

#include <assert.h>
#include <string.h>

#include <dlib/log.h>

#include "util.h"
#include "font.h"
#include "font_private.h"
#include "font_ttf.h"

namespace dmFont
{

static inline Font* ToFont(HFont hfont)
{
    return (Font*)hfont;
}

void DestroyFont(HFont hfont)
{
    Font* font = ToFont(hfont);
    if (font->m_Type == FONT_TYPE_STBTTF)
    {
        DestroyFontTTF(font);
    }
    else
    {
        assert(false && "Unsupported font type");
    }
}

static bool IsTTF(const char* path)
{
    const char* ext = strrchr(path, '.');
    if (!ext)
        return false;

    return strcmp(ext, ".ttf") == 0 || strcmp(ext, ".TTF") == 0;
}

HFont LoadFontFromPath(const char* path)
{
    uint32_t data_size;
    void* data = ReadFile(path, &data_size); // we pass the ownership to the font implementation

    if (IsTTF(path))
    {
        return LoadFontFromMemoryTTF(path, data, data_size, false);
    }

    const char* ext = strrchr(path, '.');
    dmLogError("Unsupported file type: '%s'", ext?ext:"");
    return 0;
}

HFont LoadFontFromMemory(const char* path, void* data, uint32_t data_size)
{
    if (IsTTF(path))
        return LoadFontFromMemoryTTF(path, data, data_size, false);

    const char* ext = strrchr(path, '.');
    dmLogError("Unsupported file type: '%s'", ext?ext:"");
    return 0;
}

uint32_t GetResourceSize(HFont hfont)
{
    Font* font = ToFont(hfont);
    switch (font->m_Type)
    {
    case FONT_TYPE_STBTTF: return GetResourceSizeTTF(font);
    default: assert(false && "Unsupported font type"); return 0;
    }
}

float GetPixelScaleFromSize(HFont hfont, float size)
{
    Font* font = ToFont(hfont);
    switch (font->m_Type)
    {
    case FONT_TYPE_STBTTF: return GetPixelScaleFromSizeTTF(font, size);
    default: assert(false && "Unsupported font type"); return 0;
    }
}

FontResult GetGlyph(HFont hfont, uint32_t codepoint, float scale, Glyph* glyph)
{
    Font* font = ToFont(hfont);
    switch (font->m_Type)
    {
    case FONT_TYPE_STBTTF: return GetGlyphTTF(font, codepoint, scale, glyph);
    default: assert(false && "Unsupported font type"); return RESULT_ERROR;
    }
}



} // namespace
