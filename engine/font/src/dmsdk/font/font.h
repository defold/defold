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

#if !defined(DMSDK_FONT_H)
#define DMSDK_FONT_H

#include <stdint.h>

/*# SDK Font API documentation
 *
 * Font API for loading a font (truetype), getting glyph metrics and bitmap/sdf data
 *
 * @document
 * @name Font
 * @namespace dmFont
 * @language C++
 */

namespace dmFont
{
    /*#
     * FontResult
     * @enum
     * @name FontResult
     * @member RESULT_OK
     * @member RESULT_NOT_SUPPORTED
     * @member RESULT_ERROR
     */
    enum FontResult
    {
        RESULT_OK,
        RESULT_NOT_SUPPORTED,
        RESULT_ERROR,
    };

    /*#
     * FontType
     * @enum
     * @name FontType
     * @member FONT_TYPE_UNKNOWN -1
     * @member FONT_TYPE_STBTTF
     * @member FONT_TYPE_STBOTF
     * @member FONT_TYPE_MAX
     */
    enum FontType
    {
        FONT_TYPE_UNKNOWN = -1,
        FONT_TYPE_STBTTF,
        FONT_TYPE_STBOTF,
        FONT_TYPE_MAX
    };

    /*#
     * GlyphBitmapFlags
     * @enum
     * @name GlyphBitmapFlags
     * @member GLYPH_BM_FLAG_COMPRESSION_NONE 0
     * @member GLYPH_BM_FLAG_COMPRESSION_DEFLATE 1
     */
    enum GlyphBitmapFlags
    {
        GLYPH_BM_FLAG_COMPRESSION_NONE = 0,
        GLYPH_BM_FLAG_COMPRESSION_DEFLATE = 1,
    };

    /*#
     * Holds the bitmap data of a glyph.
     * If there's an associated image, it is of size width * height * channels.
     * @struct
     * @name GlyphBitmap
     * @member m_Width [type: uint16_t] The glyph image width
     * @member m_Height [type: uint16_t] The glyph image height
     * @member m_Channels [type: uint16_t] The number of channels in the glyph image
     * @member m_Flags [type: uint8_t] Flags describing the data. See `dmFont::GlyphBitmapFlags`.
     * @member m_Data [type: uint8_t*] The bitmap data, or null if no data available.
     */
    struct GlyphBitmap
    {
        uint8_t*    m_Data;
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
     * @name Glyph
     * @member m_Bitmap [type: GlyphBitmap] The bitmap data of the glyph.
     * @member m_Codepoint [type: uint32_t] The unicode code point
     * @member m_Width [type: float] The glyph bounding width
     * @member m_Height [type: float] The glyph bounding height
     * @member m_Advance [type: float] The advance step of the glyph (in pixels)
     * @member m_LeftBearing [type: float] The left bearing of the glyph (in pixels)
     * @member m_Ascent [type: float] The ascent of the glyph. (in pixels)
     * @member m_Descent [type: float] The descent of the glyph. Positive! (in pixels)
     */
    struct Glyph
    {
        GlyphBitmap m_Bitmap;
        uint32_t    m_Codepoint; // Unicode code point
        float       m_Width;
        float       m_Height;
        float       m_Advance;
        float       m_LeftBearing;
        float       m_Ascent;
        float       m_Descent;
    };

    /*#
    * Font handle. Holds the info about a loaded font
    * @name HFont
    * @typedef
    */
    typedef struct Font* HFont;

    // ****************************************************************
    // Fonts

    /*#
     * Loads a font using a path
     * @name LoadFontFromPath
     * @param path [type: const char*] The path to the resource
     * @return font [type: dmFont::HFont] The loaded font, or null if it failed to load.
     */
    HFont LoadFontFromPath(const char* path);

    /*#
     * Loads a font from memory
     * @name LoadFontFromMemory
     * @param name [type: const char*] The name of the resource. For easier debugging
     * @param data [type: void*] The raw data
     * @param data_size [type: uint32_t] The length of the data (in bytes)
     * @param allocate [type: bool] If true, the font may allocate a copy of the data (if needed)
     * @return font [type: dmFont::HFont] The loaded font, or null if it failed to load.
     */
    HFont LoadFontFromMemory(const char* name, void* data, uint32_t data_size, bool allocate);

    /*#
     * Destroys a loaded font
     * @name DestroyFont
     * @param font [type: dmFont::HFont] The font to deallocate
     */
    void  DestroyFont(HFont font);

    /*#
     * Gets the specific implementation of the loaded font
     * @name GetType
     * @param font [type: dmFont::HFont] The font
     * @return type [type: dmFont::FontType] The type
     */
    FontType GetType(HFont font);

    /*#
     * Gets the path of the loaded font
     * @name GetPath
     * @param font [type: dmFont::HFont] The font
     * @return path [type: const char*] The path
     */
    const char* GetPath(HFont font);

    /*#
     * Get the scale factor from a given pixel size.
     * @name GetPixelScaleFromSize
     * @param font [type: dmFont::HFont] The font
     * @param size [type: float] The font size (in pixel height)
     * @return scale [type: float] The scale factor
     */
    float GetPixelScaleFromSize(HFont font, float size);

    /*#
     * Get the max ascent of the font
     * @name GetAscent
     * @param font [type: dmFont::HFont] The font
     * @param scale [type: float] The scale factor
     * @return ascent [type: float] The max ascent
     */
    float GetAscent(HFont font, float scale);

    /*#
     * Get the max descent of the font
     * @name GetDescent
     * @param font [type: dmFont::HFont] The font
     * @param scale [type: float] The scale factor
     * @return descent [type: float] The max descent
     */
    float GetDescent(HFont font, float scale);

    /*#
     * Get the line gap of the font
     * @name GetLineGap
     * @param font [type: dmFont::HFont] The font
     * @param scale [type: float] The scale factor
     * @return line_gap [type: float] The line gap
     */
    float GetLineGap(HFont font, float scale);

    /*#
     * Get the bytes used by this resource
     * @name GetResourceSize
     * @param font [type: dmFont::HFont] The font
     * @return size [type: uint32_t] The resource size
     */
    uint32_t GetResourceSize(HFont font);

    // ****************************************************************
    // Glyphs

    /*#
     * Holds the bitmap data of a glyph.
     * If there's an associated image, it is of size width * height * channels.
     * @struct
     * @name GlyphOptions
     * @member m_Scale [type: float] The font scale
     * @member m_GenerateImage [type: bool] If true, generates an SDF image, and fills out the glyph.m_Bitmap structure.
     * @member m_StbttSDFPadding [type: float] The sdk padding value (valid for FONT_TYPE_STBTTF fonts)
     * @member m_StbttSDFOnEdgeValue [type: int] Where the edge value is located (valid for FONT_TYPE_STBTTF fonts)
     */
    struct GlyphOptions
    {
        float m_Scale; // Point to Size scale
        bool  m_GenerateImage;

        // stbtt options (see stbtt_GetGlyphSDF)
        float m_StbttSDFPadding;
        int   m_StbttSDFOnEdgeValue;

        GlyphOptions()
        : m_Scale(1.0f)
        , m_GenerateImage(false)
        , m_StbttSDFPadding(3.0f)
        , m_StbttSDFOnEdgeValue(190)
        {
        }
    };

    /*#
     * Get the metrics and possibly the rasterized image data of a glyph
     * @name GetGlyph
     * @param font [type: dmFont::HFont] The font
     * @param codepoint [type: uint32_t] The unicode code point
     * @param options (in) [type: dmFont::GlyphOptions*] The glyph options
     * @param glyph (out) [type: dmFont::Glyph*] The glyph
     * @return result [type: dmFont::FontResult] The result
     */
    FontResult GetGlyph(HFont font, uint32_t codepoint, GlyphOptions* options, Glyph* glyph);

    /*#
     * Free the bitmap of the glyph
     * @name FreeGlyph
     * @param font [type: dmFont::HFont] The font
     * @param glyph [type: dmFont::Glyph*] The glyph
     * @return result [type: dmFont::FontResult] The result
     */
    // In case a bit map was allocated, and you wish to free the memory
    FontResult FreeGlyph(HFont font, Glyph* glyph);

} // namespace

#endif // DMSDK_FONT_H
