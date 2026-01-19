// Copyright 2020-2026 The Defold Foundation
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
#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/log.h>
#include <dmsdk/dlib/utf8.h>

#include "util.h"
#include "font.h"
#include "font_private.h"
#include "font_ttf.h"

static inline Font* ToFont(HFont hfont)
{
    return (Font*)hfont;
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

void FontDestroy(HFont hfont)
{
    assert(hfont);
    Font* font = ToFont(hfont);
    const char* path = font->m_Path; // Keep it alive during destruction (for better debugging)
    font->m_DestroyFont(font);
    free((void*)path);
}

HFont FontLoadFromMemory(const char* path, void* data, uint32_t data_size, bool allocate)
{
    Font* font = 0;

    FontType type = GetFontType(path);
    switch(type)
    {
    case FONT_TYPE_STBTTF:
    case FONT_TYPE_STBOTF:  font = FontLoadFromMemoryTTF(path, data, data_size, allocate); break;
    default:
        {
            const char* ext = strrchr(path, '.');
            dmLogError("Unsupported file type: '%s'", ext?ext:"");
            return 0;
        }
    }

    if (font)
    {
        font->m_Type    = type;
        font->m_Path    = strdup(path);
        font->m_PathHash= dmHashString32(path);
    }

    return font;
}

HFont FontLoadFromPath(const char* path)
{
    uint32_t data_size;
    void* data = FontReadFile(path, &data_size); // we pass the ownership to the font implementation
    if (!data)
    {
        return 0;
    }

    Font* font = FontLoadFromMemory(path, data, data_size, true);
    free(data);
    if (!font)
    {
        free((void*)data);
        return 0;
    }
    return font;
}

uint32_t FontGetResourceSize(HFont font)
{
    return font->m_GetResourceSize(font);
}

float FontGetScaleFromSize(HFont font, float size)
{
    return font->m_GetScaleFromSize(font, size);
}

float FontGetAscent(HFont font, float scale)
{
    return font->m_GetAscent(font, scale);
}

float FontGetDescent(HFont font, float scale)
{
    return font->m_GetDescent(font, scale);
}

float FontGetLineGap(HFont font, float scale)
{
    return font->m_GetLineGap(font, scale);
}

uint32_t FontGetGlyphIndex(HFont font, uint32_t codepoint)
{
    return font->m_GetGlyphIndex(font, codepoint);
}

FontResult FontGetGlyph(HFont font, uint32_t codepoint, FontGlyphOptions* options, FontGlyph* glyph)
{
    uint32_t index = font->m_GetGlyphIndex(font, codepoint);
    if (!index)
    {
        return FONT_RESULT_ERROR;
    }
    FontResult r = font->m_GetGlyph(font, index, options, glyph);
    glyph->m_Codepoint = codepoint;
    return r;
}

FontResult FontGetGlyphByIndex(HFont font, uint32_t glyph_index, FontGlyphOptions* options, FontGlyph* glyph)
{
    return font->m_GetGlyph(font, glyph_index, options, glyph);
}

FontResult FontFreeGlyph(HFont font, FontGlyph* glyph)
{
    return font->m_FreeGlyph(font, glyph);
}

FontType FontGetType(HFont font)
{
    assert(font);
    return font->m_Type;
}

const char* FontGetPath(HFont font)
{
    assert(font);
    return font->m_Path;
}

uint32_t FontGetPathHash(HFont font)
{
    assert(font);
    return font->m_PathHash;
}

TextLayoutType FontGetLayoutType(HFont font)
{
#if defined(FONT_USE_HARFBUZZ) && defined(FONT_USE_SKRIBIDI)
    if (font->m_Type == FONT_TYPE_STBTTF || font->m_Type == FONT_TYPE_STBOTF)
        return TEXT_LAYOUT_TYPE_FULL;
#endif
    return TEXT_LAYOUT_TYPE_LEGACY;
}

static void Indent(int indent)
{
    for (int i = 0; i < indent; ++i)
    {
        printf("  ");
    }
}

void FontDebugGlyph(FontGlyph* glyph, int indent)
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

void FontDebug(HFont hfont, float scale, float padding, const char* text)
{
    int indent = 0;

    FontType type = FontGetType(hfont);
    printf("DebugFont: '%s' type: %d\n", FontGetPath(hfont), type);

    Indent(indent+1); printf("ascent:   %.3f\n", FontGetAscent(hfont, scale));
    Indent(indent+1); printf("descent:  %.3f\n", FontGetDescent(hfont, scale));
    Indent(indent+1); printf("line gap: %.3f\n", FontGetLineGap(hfont, scale));
    printf("\n");

    FontGlyphOptions options;
    options.m_Scale = scale;
    options.m_GenerateImage = true;

    options.m_StbttSDFPadding = padding;
    const char* cursor = text;
    uint32_t codepoint = 0;
    while ((codepoint = dmUtf8::NextChar(&cursor)))
    {
        FontGlyph glyph;
        FontGetGlyph(hfont, codepoint, &options, &glyph);
        FontDebugGlyph(&glyph, indent+2);

        if (options.m_GenerateImage && (type == FONT_TYPE_STBTTF || type == FONT_TYPE_STBOTF))
        {
            if(glyph.m_Bitmap.m_Data)
            {
                Indent((indent+3)); printf("Bitmap: %u x %u x %u  flags: %u\n",
                                            glyph.m_Bitmap.m_Width, glyph.m_Bitmap.m_Height, glyph.m_Bitmap.m_Channels, glyph.m_Bitmap.m_Flags);
                FontDebugPrintBitmap(glyph.m_Bitmap.m_Data, glyph.m_Bitmap.m_Width, glyph.m_Bitmap.m_Height);
            }
        }

        FontFreeGlyph(hfont, &glyph);
    }
}
