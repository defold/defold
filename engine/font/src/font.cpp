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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <dmsdk/dlib/dstrings.h>
#include <dmsdk/dlib/log.h>
#include <dmsdk/dlib/utf8.h>

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
    assert(hfont);
    Font* font = ToFont(hfont);
    const char* path = font->m_Path; // Keep it alive during destruction (for better debugging)

    switch(font->m_Type)
    {
    case FONT_TYPE_STBTTF:
    case FONT_TYPE_STBOTF:  DestroyFontTTF(font); break;
    default:                assert(false && "Unsupported font type");
    }
    free((void*)path);
}

static FontType GetFontType(const char* path)
{
    const char* ext = strrchr(path, '.');
    if (!ext)
        return FONT_TYPE_UNKNOWN;

    if (strcmp(ext, ".ttf") == 0 || strcmp(ext, ".TTF") == 0)
        return FONT_TYPE_STBTTF;
    if (strcmp(ext, ".otf") == 0 || strcmp(ext, ".OTF") == 0)
        return FONT_TYPE_STBOTF;

    return FONT_TYPE_UNKNOWN;
}

HFont LoadFontFromMemory(const char* path, void* data, uint32_t data_size, bool allocate)
{
    Font* font = 0;

    FontType type = GetFontType(path);
    switch(type)
    {
    case FONT_TYPE_STBTTF:
    case FONT_TYPE_STBOTF:  font = LoadFontFromMemoryTTF(path, data, data_size, allocate); break;
    default:
        {
            const char* ext = strrchr(path, '.');
            dmLogError("Unsupported file type: '%s'", ext?ext:"");
            return 0;
        }
    }

    if (font)
    {
        font->m_Type = type;
        font->m_Path = strdup(path);
    }

    return font;
}

HFont LoadFontFromPath(const char* path)
{
    uint32_t data_size;
    void* data = ReadFile(path, &data_size); // we pass the ownership to the font implementation
    if (!data)
    {
        return 0;
    }

    Font* font = LoadFontFromMemory(path, data, data_size, false);
    if (!font)
    {
        free((void*)data);
        return 0;
    }
    return font;
}

uint32_t GetResourceSize(HFont font)
{
    switch (font->m_Type)
    {
    case FONT_TYPE_STBTTF:
    case FONT_TYPE_STBOTF:  return GetResourceSizeTTF(font);
    default: assert(false && "Unsupported font type"); return 0;
    }
}

float GetPixelScaleFromSize(HFont font, float size)
{
    switch (font->m_Type)
    {
    case FONT_TYPE_STBTTF:
    case FONT_TYPE_STBOTF: return GetPixelScaleFromSizeTTF(font, size);
    default: assert(false && "Unsupported font type"); return 0;
    }
}

float GetAscent(HFont font, float scale)
{
    switch (font->m_Type)
    {
    case FONT_TYPE_STBTTF:
    case FONT_TYPE_STBOTF: return GetAscentTTF(font, scale);
    default: assert(false && "Unsupported font type"); return 0;
    }
}

float GetDescent(HFont font, float scale)
{
    switch (font->m_Type)
    {
    case FONT_TYPE_STBTTF:
    case FONT_TYPE_STBOTF: return GetDescentTTF(font, scale);
    default: assert(false && "Unsupported font type"); return 0;
    }
}

float GetLineGap(HFont font, float scale)
{
    switch (font->m_Type)
    {
    case FONT_TYPE_STBTTF:
    case FONT_TYPE_STBOTF: return GetLineGapTTF(font, scale);
    default: assert(false && "Unsupported font type"); return 0;
    }
}



FontResult GetGlyph(HFont font, uint32_t codepoint, GlyphOptions* options, Glyph* glyph)
{
    switch (font->m_Type)
    {
    case FONT_TYPE_STBTTF:
    case FONT_TYPE_STBOTF: return GetGlyphTTF(font, codepoint, options, glyph);
    default: assert(false && "Unsupported font type"); return RESULT_ERROR;
    }
}

FontResult FreeGlyph(HFont font, Glyph* glyph)
{
    switch (font->m_Type)
    {
    case FONT_TYPE_STBTTF:
    case FONT_TYPE_STBOTF: return FreeGlyphTTF(font, glyph);
    default: assert(false && "Unsupported font type"); return RESULT_ERROR;
    }
}

FontType GetType(HFont font)
{
    assert(font);
    return font->m_Type;
}

const char* GetPath(HFont font)
{
    assert(font);
    return font->m_Path;
}

static void Indent(int indent)
{
    for (int i = 0; i < indent; ++i)
    {
        printf("  ");
    }
}

void DebugGlyph(Glyph* glyph, int indent)
{
    char utf8[16];
    uint32_t len = dmUtf8::ToUtf8(glyph->m_Codepoint, utf8);
    utf8[len] = 0;

    Indent(indent);   printf("glyph: '%s' 0x%0X\n", utf8, glyph->m_Codepoint);
    Indent(indent+1); printf("width:    %.3f\n", glyph->m_Width);
    Indent(indent+1); printf("height:   %.3f\n", glyph->m_Height);
    Indent(indent+1); printf("advance:  %.3f\n", glyph->m_Advance);
    Indent(indent+1); printf("lbearing: %.3f\n", glyph->m_LeftBearing);
    Indent(indent+1); printf("ascent:   %.3f\n", glyph->m_Ascent);
    Indent(indent+1); printf("descent:  %.3f\n", glyph->m_Descent);
}

void DebugFont(HFont hfont, float scale, float padding, const char* text)
{
    int indent = 0;

    FontType type = GetType(hfont);
    printf("DebugFont: '%s' type: %d\n", GetPath(hfont), type);

    Indent(indent+1); printf("ascent:   %.3f\n", GetAscent(hfont, scale));
    Indent(indent+1); printf("descent:  %.3f\n", GetDescent(hfont, scale));
    Indent(indent+1); printf("line gap: %.3f\n", GetLineGap(hfont, scale));
    printf("\n");

    GlyphOptions options;
    options.m_Scale = scale;
    options.m_GenerateImage = true;

    options.m_StbttSDFPadding = padding;
    const char* cursor = text;
    uint32_t codepoint = 0;
    while ((codepoint = dmUtf8::NextChar(&cursor)))
    {
        dmFont::Glyph glyph;
        dmFont::GetGlyph(hfont, codepoint, &options, &glyph);
        dmFont::DebugGlyph(&glyph, indent+2);

        if (options.m_GenerateImage && (type == FONT_TYPE_STBTTF || type == FONT_TYPE_STBOTF))
        {
            if(glyph.m_Bitmap.m_Data)
            {
                Indent((indent+3)); printf("Bitmap: %u x %u x %u  flags: %u\n",
                                            glyph.m_Bitmap.m_Width, glyph.m_Bitmap.m_Height, glyph.m_Bitmap.m_Channels, glyph.m_Bitmap.m_Flags);
                DebugPrintBitmap(glyph.m_Bitmap.m_Data, glyph.m_Bitmap.m_Width, glyph.m_Bitmap.m_Height);
            }
        }

        dmFont::FreeGlyph(hfont, &glyph);
    }
}



} // namespace
