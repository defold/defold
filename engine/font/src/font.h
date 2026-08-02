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

#ifndef DM_FONT_H
#define DM_FONT_H

#include <dmsdk/font/font.h>
#include "text_layout.h"

typedef HFont       (*FontLoadFromMemoryFn)(const char* name, const void* data, uint32_t data_size, bool allocate);
typedef void        (*FontDestroyFn)(HFont font);
typedef uint32_t    (*FontGetResourceSizeFn)(HFont font);
typedef float       (*FontGetScaleFromSizeFn)(HFont hfont, uint32_t size);
typedef float       (*FontGetAscentFn)(HFont hfont, float scale);
typedef float       (*FontGetDescentFn)(HFont hfont, float scale);
typedef float       (*FontGetLineGapFn)(HFont hfont, float scale);
typedef uint32_t    (*FontGetGlyphIndexFn)(HFont font, uint32_t codepoint);
typedef FontResult  (*FontGetGlyphFn)(HFont hfont, uint32_t glyph_index, const FontGlyphOptions* options, FontGlyph* glyph);
typedef FontResult  (*FontFreeGlyphFn)(HFont hfont, FontGlyph* glyph);

struct Font
{
    /**
     * Font implementation selected by the generic loader. The type determines
     * which optional backend features, such as full text shaping, are available.
     */
    FontType        m_Type;

    /**
     * Null-terminated source path used for diagnostics and resource identity.
     * The generic font layer owns this string and frees it after m_DestroyFont
     * has destroyed the backend object.
     */
    const char*     m_Path;

    /**
     * 32-bit hash of m_Path. Glyph caches combine this value with a glyph index
     * to identify glyphs from different font resources efficiently.
     */
    uint32_t        m_PathHash;

    /**
     * Create another font handled by this backend from an in-memory font file.
     * `name` identifies the source for diagnostics. When `allocate` is true the
     * returned font must retain its own copy of `data`; otherwise the caller
     * keeps the data alive for the lifetime of the returned font.
     */
    FontLoadFromMemoryFn        m_LoadFontFromMemory;

    /**
     * Destroy the concrete backend object and release everything owned by it,
     * including copied source data. It must not free m_Path, which remains owned
     * by the generic font layer until this callback returns.
     */
    FontDestroyFn               m_DestroyFont;

    /**
     * Return the backend-reported resource size in bytes. For file-backed fonts
     * this is the size of the retained font data and is used for resource memory
     * accounting.
     */
    FontGetResourceSizeFn       m_GetResourceSize;

    /**
     * Convert a requested pixel size to the scale applied to metrics and glyph
     * outlines in the font's native coordinate system.
     */
    FontGetScaleFromSizeFn      m_GetScaleFromSize;

    /** Return the font-wide ascent multiplied by `scale`. */
    FontGetAscentFn             m_GetAscent;

    /**
     * Return the font-wide descent multiplied by `scale`. The sign follows the
     * source font metric (TrueType fonts normally report a negative descent).
     */
    FontGetDescentFn            m_GetDescent;

    /** Return the recommended extra line spacing multiplied by `scale`. */
    FontGetLineGapFn            m_GetLineGap;

    /**
     * Map a Unicode code point to the backend's glyph index. Return zero when
     * the font has no glyph for the code point.
     */
    FontGetGlyphIndexFn         m_GetGlyphIndex;

    /**
     * Fill `glyph` with metrics for a backend glyph index and, when requested by
     * `options`, allocate and return its bitmap. Any bitmap allocation must be
     * releasable by m_FreeGlyph. Code-point lookup is performed by the generic
     * FontGetGlyph() wrapper before this callback is invoked.
     */
    FontGetGlyphFn              m_GetGlyph;

    /**
     * Release backend-owned data stored in `glyph`, notably the optional bitmap
     * allocated by m_GetGlyph. This does not destroy the font or the glyph value
     * supplied by the caller.
     */
    FontFreeGlyphFn             m_FreeGlyph;
};

/**
 * Treat a fully initialized backend Font object as an opaque font handle.
 * The Font object must remain the first member of any concrete backend type.
 */
HFont FontCreate(Font* font);

/** Return the text-layout implementation supported by this font backend. */
TextLayoutType FontGetLayoutType(HFont hfont);

#if defined(FONT_USE_HARFBUZZ)

typedef struct hb_font_t hb_font_t;

/** Return the HarfBuzz font owned by a TrueType/OpenType font backend. */
hb_font_t* FontGetHarfbuzzFontFromTTF(HFont hfont);

#endif


#endif // DM_FONT_H
