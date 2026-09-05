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

#ifndef DM_FONT_GLYPHBANK_H
#define DM_FONT_GLYPHBANK_H

#include <dmsdk/font/font.h>

/*# A normalized, borrowed glyph-bank entry.
 *
 * Bitmap data remains owned by the provider and must stay valid until the
 * provider is updated or the font is destroyed.
 */
struct FontGlyphBankGlyph
{
    const uint8_t* m_Data;
    uint32_t       m_DataSize;
    uint32_t       m_Codepoint;
    float          m_Width;
    float          m_Advance;
    float          m_LeftBearing;
    float          m_Ascent;
    float          m_Descent;
    uint8_t        m_BitmapFlags;
};

/*# Returns the codepoint for a zero-based glyph index.
 * The index is always smaller than FontGlyphBankProvider::m_GlyphCount.
 */
typedef uint32_t (*FontGlyphBankGetCodepointFn)(void* context, uint32_t glyph_index);

/*# Returns a normalized glyph for a zero-based glyph index.
 * The output data is borrowed from the provider. Returns false for malformed
 * backing data.
 */
typedef bool (*FontGlyphBankGetGlyphFn)(void* context, uint32_t glyph_index, FontGlyphBankGlyph* glyph);

/*# Releases provider-owned context after the final font access.
 * A null callback denotes externally owned context.
 */
typedef void (*FontGlyphBankDestroyFn)(void* context);

/*# Zero-copy glyph-bank data provider.
 *
 * The provider and its backing context must remain at stable addresses for the
 * lifetime of the font. Its fields and context may be refreshed in place while
 * the font is not being accessed. FontCreateGlyphBank takes ownership of the
 * context only when m_Destroy is non-null and creation succeeds.
 */
struct FontGlyphBankProvider
{
    void*                       m_Context;
    FontGlyphBankGetCodepointFn m_GetCodepoint;
    FontGlyphBankGetGlyphFn     m_GetGlyph;
    FontGlyphBankDestroyFn      m_Destroy;
    uint64_t                    m_ResourceSize;
    uint32_t                    m_GlyphCount;
    uint32_t                    m_GlyphPadding;
    uint32_t                    m_GlyphChannels;
    float                       m_MaxAscent;
    float                       m_MaxDescent;
};

/*# Creates a prebaked glyph-bank font.
 *
 * The glyphs must be sorted by codepoint. Returned glyph bitmap data is always
 * marked as borrowed. Returns null if path or required provider callbacks are
 * invalid, or if allocation fails.
 */
HFont FontCreateGlyphBank(const char* path, FontGlyphBankProvider* provider);

#endif // DM_FONT_GLYPHBANK_H
