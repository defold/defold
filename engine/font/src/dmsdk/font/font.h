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

#ifndef DMSDK_FONT_H
#define DMSDK_FONT_H

#include <stdint.h>

/*# SDK Font API documentation
 *
 * Font API for loading a font (truetype), getting glyph metrics and bitmap/sdf data
 *
 * @document
 * @name Font
 * @path engine/font/src/dmsdk/font/font.h
 * @language C++
 */

/*#
 * FontResult
 * @enum
 * @name FontResult
 * @member FONT_RESULT_OK
 * @member FONT_RESULT_NOT_SUPPORTED
 * @member FONT_RESULT_ERROR
 */
enum FontResult
{
    FONT_RESULT_OK,
    FONT_RESULT_NOT_SUPPORTED,
    FONT_RESULT_ERROR,
};

/*#
 * FontType
 * @enum
 * @name FontType
 * @member FONT_TYPE_STBTTF
 * @member FONT_TYPE_STBOTF
 * @member FONT_TYPE_UNKNOWN = 0xFFFFFFFF
 */
enum FontType
{
    FONT_TYPE_STBTTF,
    FONT_TYPE_STBOTF,

    FONT_TYPE_UNKNOWN = 0xFFFFFFFF // used to make it 4 bytes size
};

/*#
 * FontGlyphBitmapFlags
 * @enum
 * @name FontGlyphBitmapFlags
 * @member GLYPH_BM_FLAG_COMPRESSION_NONE = 0
 * @member GLYPH_BM_FLAG_COMPRESSION_DEFLATE = 1
 */
enum FontGlyphBitmapFlags
{
    FONT_GLYPH_BM_FLAG_COMPRESSION_NONE = 0,
    FONT_GLYPH_BM_FLAG_COMPRESSION_DEFLATE = 1,
};

/*#
 * Holds the bitmap data of a glyph.
 * If there's an associated image, it is of size width * height * channels.
 * @struct
 * @name FontGlyphBitmap
 * @member m_Width [type: uint16_t] The glyph image width
 * @member m_Height [type: uint16_t] The glyph image height
 * @member m_Channels [type: uint8_t] The number of color channels
 * @member m_Flags [type: uint8_t] Flags describing the data. See #FontGlyphBitmapFlags.
 * @member m_Data [type: uint8_t*] The bitmap data, or null if no data available.
 * @member m_DataSize [type: uint32_t] The bitmap data size (e.g. if the data is compressed)
 */
struct FontGlyphBitmap
{
    uint8_t*    m_Data;
    uint32_t    m_DataSize;
    uint16_t    m_Width;
    uint16_t    m_Height;
    uint8_t     m_Channels;
    uint8_t     m_Flags;
};

/*#
 * Represents a glyph.
 * If there's an associated image, it is of size width * height * channels.
 *
 * @note The baseline of a glyph bitmap is calculated: `base = glyph.bitmap.height - glyph.ascent`
 *
 * @struct
 * @name FontGlyph
 * @member m_Bitmap [type: FontGlyphBitmap] The bitmap data of the glyph.
 * @member m_Codepoint [type: uint32_t] The unicode code point
 * @member m_Width [type: float] The glyph bounding width
 * @member m_Height [type: float] The glyph bounding height
 * @member m_Advance [type: float] The advance step of the glyph (in pixels)
 * @member m_LeftBearing [type: float] The left bearing of the glyph (in pixels)
 * @member m_Ascent [type: float] The ascent of the glyph. (in pixels)
 * @member m_Descent [type: float] The descent of the glyph. Positive! (in pixels)
 */
struct FontGlyph
{
    FontGlyphBitmap m_Bitmap;
    uint32_t        m_Codepoint;  // Unicode code point (0 if not available)
    uint32_t        m_GlyphIndex; // glyph index into the font
    float           m_Width;
    float           m_Height;
    float           m_Advance;
    float           m_LeftBearing;
    float           m_Ascent;
    float           m_Descent;
};

typedef struct Font* HFont;

// ****************************************************************
// Fonts

/*#
 * Loads a font using a path
 * @name FontLoadFromPath
 * @param path [type: const char*] The path to the resource
 * @return font [type: HFont] The loaded font, or null if it failed to load.
 */
HFont FontLoadFromPath(const char* path);

/*#
 * Loads a font from memory
 * @name FontLoadFromMemory
 * @param name [type: const char*] The name of the resource. For easier debugging
 * @param data [type: void*] The raw data
 * @param data_size [type: uint32_t] The length of the data (in bytes)
 * @param allocate [type: bool] If true, the font may allocate a copy of the data (if needed)
 * @return font [type: HFont] The loaded font, or null if it failed to load.
 */
HFont FontLoadFromMemory(const char* name, void* data, uint32_t data_size, bool allocate);

/*#
 * Destroys a loaded font
 * @name FontDestroy
 * @param font [type: HFont] The font to deallocate
 */
void  FontDestroy(HFont font);

/*#
 * Gets the specific implementation of the loaded font
 * @name FontGetType
 * @param font [type: HFont] The font
 * @return type [type: dmFont::FontType] The type
 */
FontType FontGetType(HFont font);

/*#
 * Gets the path of the loaded font
 * @name FontGetPath
 * @param font [type: HFont] The font
 * @return path [type: const char*] The path
 */
const char* FontGetPath(HFont font);

/*#
 * Gets the path hash of the loaded font
 * @note We use a 32bit hash to make it easier to pair with a glyph index into a 64-bit key
 * @name FontGetPathHash
 * @param font [type: HFont] The font
 * @return path [type: uint32_t] The path
 */
uint32_t FontGetPathHash(HFont font);

/*#
 * Get the scale factor from a given pixel size.
 * Used to convert from points to pixel size
 * @name FontGetScaleFromSize
 * @param font [type: HFont] The font
 * @param size [type: float] The font size (in pixel height)
 * @return scale [type: float] The scale factor
 */
float FontGetScaleFromSize(HFont font, float size);

/*#
 * Get the max ascent of the font
 * @name FontGetAscent
 * @param font [type: HFont] The font
 * @param scale [type: float] The scale factor
 * @return ascent [type: float] The max ascent
 */
float FontGetAscent(HFont font, float scale);

/*#
 * Get the max descent of the font
 * @name FontGetDescent
 * @param font [type: HFont] The font
 * @param scale [type: float] The scale factor
 * @return descent [type: float] The max descent
 */
float FontGetDescent(HFont font, float scale);

/*#
 * Get the line gap of the font
 * @name FontGetLineGap
 * @param font [type: HFont] The font
 * @param scale [type: float] The scale factor
 * @return line_gap [type: float] The line gap
 */
float FontGetLineGap(HFont font, float scale);

/*#
 * Get the bytes used by this resource
 * @name FontGetResourceSize
 * @param font [type: HFont] The font
 * @return size [type: uint32_t] The resource size
 */
uint32_t FontGetResourceSize(HFont font);

// ****************************************************************
// Glyphs

/*#
 * Get glyph index of a codepoint
 * @name FontGetGlyphIndex
 * @param font [type: HFont] The font
 * @param codepoint [type: uint32_t] The unicode code point
 * @return glyph_index [type: uint32_t] 0 if no index was found
 */
uint32_t FontGetGlyphIndex(HFont font, uint32_t codepoint);

/*#
 * Holds the bitmap data of a glyph.
 * If there's an associated image, it is of size width * height * channels.
 * @struct
 * @name FontGlyphOptions
 * @member m_Scale [type: float] The font scale
 * @member m_GenerateImage [type: bool] If true, generates an SDF image, and fills out the glyph.m_Bitmap structure.
 * @member m_StbttSDFPadding [type: int] The sdk padding value (valid for FONT_TYPE_STBTTF fonts)
 * @member m_StbttSDFOnEdgeValue [type: int] Where the edge value is located (valid for FONT_TYPE_STBTTF fonts)
 */
struct FontGlyphOptions
{
    float m_Scale; // Point to Size scale
    bool  m_GenerateImage;

    // stbtt options (see stbtt_GetGlyphSDF)
    float m_StbttSDFPadding;
    int   m_StbttSDFOnEdgeValue;

    FontGlyphOptions()
    : m_Scale(1.0f)
    , m_GenerateImage(false)
    , m_StbttSDFPadding(3)
    , m_StbttSDFOnEdgeValue(190)
    {
    }
};

/*#
 * Get the metrics of a glyph
 * @name FontGetGlyph
 * @param font [type: HFont] The font
 * @param codepoint [type: uint32_t] The unicode code point
 * @param options (in) [type: FontGlyphOptions*] The glyph options
 * @param glyph (out) [type: FontGlyph*] The glyph
 * @return result [type: FontFontResult] If successful, user must call FreeGlyph() on the result to clear any image data.
 */
FontResult FontGetGlyph(HFont font, uint32_t codepoint, FontGlyphOptions* options, FontGlyph* glyph);

/*#
 * Get the metrics of a glyph
 * @name FontGetGlyphByIndex
 * @param font [type: HFont] The font
 * @param glyph_index [type: uint32_t] The unicode code point
 * @param options (in) [type: FontGlyphOptions*] The glyph options
 * @param glyph (out) [type: FontGlyph*] The glyph
 * @return result [type: FontFontResult] If successful, user must call FreeGlyph() on the result to clear any image data.
 */
FontResult FontGetGlyphByIndex(HFont font, uint32_t glyph_index, FontGlyphOptions* options, FontGlyph* glyph);

/*#
 * Free the bitmap of the glyph
 * @name FontFreeGlyph
 * @param font [type: HFont] The font
 * @param glyph [type: dmFont::Glyph*] The glyph
 * @return result [type: dmFont::FontResult] The result
 */
FontResult FontFreeGlyph(HFont font, FontGlyph* glyph);

#endif // DMSDK_FONT_H
