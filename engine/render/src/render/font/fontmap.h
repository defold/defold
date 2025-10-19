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

#ifndef DM_RENDER_FONTMAP_H
#define DM_RENDER_FONTMAP_H

#include <stdint.h>

#include <dlib/hash.h>
#include <graphics/graphics.h>

#include <dmsdk/font/font.h>
#include <dmsdk/font/fontcollection.h>
#include <dmsdk/font/text_layout.h>
#include <render/font_ddf.h>

#include "../render.h"

namespace dmRender
{
    /**
     * A font map holds the glyph cache, and font information
     * @name HFontMap
     * @typedef
     */
    typedef struct FontMap* HFontMap;

    /**
     * When a glyph is missing from the font map, this callback is invoked.
     * @note may return no pointer to a glyph (if it's too costly to do synchronously)
     * @name FGlyphCacheMiss
     * @typedef
     */
    typedef FontResult (*FGlyphCacheMiss)(void* ctx, HFontMap font_map, HFont font, uint32_t glyph_index, FontGlyph**);

    /**
     * Font map parameters supplied to NewFontMap
     * @name FontMapParams
     * @struct
     */
    struct FontMapParams
    {
        /// Default constructor
        FontMapParams();

        HFontCollection m_FontCollection;
        dmhash_t        m_NameHash;

        FGlyphCacheMiss   m_OnGlyphCacheMiss;
        void*             m_OnGlyphCacheMissContext;

        /// Size of the font
        float m_Size;
        /// Offset of the shadow along the x-axis
        float m_ShadowX;
        /// Offset of the shadow along the y-axis
        float m_ShadowY;
        /// Max ascent of font
        float m_MaxAscent;
        /// Max descent of font, positive value
        float m_MaxDescent;
        /// Value to scale SDF texture values with
        float m_SdfSpread;
        /// Distance value where outline should end
        float m_SdfOutline;
        /// Distance value where shadow should end
        float m_SdfShadow;
        /// Font alpha
        float m_Alpha;
        /// Font outline alpha
        float m_OutlineAlpha;
        /// Font shadow alpha
        float m_ShadowAlpha;

        uint32_t m_CacheWidth;
        uint32_t m_CacheHeight;
        uint32_t m_CacheMaxWidth;
        uint32_t m_CacheMaxHeight;
        uint32_t m_CacheCellWidth;
        uint32_t m_CacheCellHeight;
        uint32_t m_CacheCellMaxAscent;
        uint8_t m_GlyphChannels; // How many bitmap channels
        uint8_t m_CacheCellPadding;
        uint8_t m_LayerMask;

        uint8_t m_IsMonospaced:1;
        uint8_t m_IsDynamic:1;
        uint8_t m_Padding:6;        // Note: Not C struct padding, but actual glyph padding.

        dmRenderDDF::FontTextureFormat m_ImageFormat;
    };

    // Represents a slot in the texture glyph cache
    // TODO: Make this private
    // TODO: Add api to get the UV quad for the glyph
    struct CacheGlyph
    {
        FontGlyph*  m_Glyph;
        uint32_t    m_Frame; // Age
        int16_t     m_X;     // The top left texel in the cache texture
        int16_t     m_Y;     // The top left texel in the cache texture
        // TODO: add page here as well
        // private
        uint64_t    m_GlyphKey;
    };


    /**
     * Create a new font map. The parameters struct is consumed and should not be read after this call.
     * @name NewFontMap
     * @param render_context Render context handle
     * @param graphics_context Graphics context handle
     * @param params Params used to initialize the font map
     * @return HFontMap on success. NULL on failure
     */
    HFontMap NewFontMap(dmRender::HRenderContext render_context, dmGraphics::HContext graphics_context, FontMapParams& params);

    /**
     * Get the font size
     * @name GetFontMapSize
     * @param font_map Font map handle
     * @return size [type: float] Size in pixels
     */
    float GetFontMapSize(dmRender::HFontMap font_map);

    /**
     * Check if the font is monospace (set by Fontc.java)
     * @name GetFontMapMonospaced
     * @note This is a legacy function
     * @param font_map Font map handle
     * @return true if font is monospace
     */
    bool GetFontMapMonospaced(dmRender::HFontMap font_map);

    /**
     * Get extra padding for monospace fonts (set by Fontc.java)
     * @name GetFontMapPadding
     * @note This is a legacy function
     * @param font_map Font map handle
     * @return padding (in pixels)
     */
    uint32_t GetFontMapPadding(dmRender::HFontMap font_map);

    /**
     * Delete a font map
     * @param font_map Font map handle
     */
    void DeleteFontMap(HFontMap font_map);

    /**
     * Get texture from a font map
     * @param font_map Font map handle
     * @return dmGraphics::HTexture Texture handle
     */
    dmGraphics::HTexture GetFontMapTexture(HFontMap font_map);

    /**
     * Set font map material
     * @param font_map Font map handle
     * @param material Material handle
     */
    void SetFontMapMaterial(HFontMap font_map, HMaterial material);

    /**
     * Get font map material
     * @param font_map Font map handle
     * @return HMaterial handle
     */
    HMaterial GetFontMapMaterial(HFontMap font_map);

    struct TextMetrics
    {
        float    m_Width;       /// Total string width
        float    m_Height;      /// Total string height
        float    m_MaxAscent;   /// Max ascent of font
        float    m_MaxDescent;  /// Max descent of font, positive value.
        uint32_t m_LineCount;   /// Number of lines of text
    };

    /**
     * Get text metrics for string
     * @param font_map Font map handle
     * @param text utf8 text to get metrics for
     * @param settings settings for getting the text metrics
     * @param metrics Metrics, out-value
     */
    void GetTextMetrics(HFontMap font_map, const char* text, TextLayoutSettings* settings, TextMetrics* metrics);

    /**
     * Get the resource size for fontmap
     * @param font_map Font map handle
     * @return size
     */
    uint32_t GetFontMapResourceSize(HFontMap font_map);

    /**
     * Set the user data assigned to this font map
     * @param font_map [type: HFontMap] Font map handle
     * @param user_data [type: void*] the user data
     */
    void SetFontMapUserData(HFontMap font_map, void* user_data);

    /**
     * Get the user data assigned to this font map
     * @param font_map [type: HFontMap] Font map handle
     * @return user_data [type: void*] the user data
     */
    void* GetFontMapUserData(HFontMap font_map);

    /**
     * Gets a struct representing a slot in the glyph texture cache
     * @param font_map [type: HFontMap] Font map handle
     * @param glyph_key [type: uint64_t] A key created by #MakeGlyphIndexKey()
     * @param frame [type: uint32_t] The current frame number. Used to for evicting old cache entries.
     * @return cache_glyph [type: CacheGlyph*] the glyph cache info
     */
    CacheGlyph* GetFromCache(HFontMap font_map, uint64_t glyph_key, uint32_t frame);

    /**
     * Checks if a codepoint is already stored within the cache
     * @param font_map [type: HFontMap] Font map handle
     * @param glyph_key [type: uint64_t] A key created by #MakeGlyphIndexKey()
     * @return result [type: bool] true if the glyph is already cached
     */
    bool IsInCache(HFontMap font_map, uint64_t glyph_key);

    /**
     * Adds a font glyph to the glyph texture cache
     * @name AddGlyphToCache
     * @param font_map [type: HFontMap] Font map handle
     * @param frame [type: uint32_t] The current frame number. Used to for evicting old cache entries.
     * @param glyph_key [type: uint64_t] A key created by #MakeGlyphIndexKey()
     * @param glyph [type: dmRender::FontGlyph*] The current frame number. Used to for evicting old cache entries.
     * @param g_offset_y [type: int32_t] The offset from the top of the cache cell. Used to align the glyph with the baseline.
     */
    void AddGlyphToCache(HFontMap font_map, uint32_t frame, uint64_t glyph_key, FontGlyph* glyph, int32_t g_offset_y);

    /** Checks if the glyph cache texture needs to be updated
     */
    void UpdateCacheTexture(HFontMap font_map);

    /** Get the font collection
     * @name GetFontCollection
     * @param font_map [type: HFontMap] Font map handlet
     * @return font_collection [type: HFontCollection] the font collection
     */
    HFontCollection GetFontCollection(dmRender::HFontMap font_map);

    /** Create a unique key fron the font and a glyph index
     * @name MakeGlyphIndexKey
     * @param font [type: HFont] the font
     * @param glyph_index [type: uint32_t] The glyph index inside the font
     * @return key [type: uint64_t] the key
     */
    uint64_t MakeGlyphIndexKey(HFont font, uint32_t glyph_index);

    /**
     * Get a glyph or create a new instance from the font map
     * @name GetOrCreateGlyphByIndex
     * @note Creates the glyph bitmap if the glyph wasn't already present
     * @param font_map [type: HFontMap] Font map handle
     * @param hfont [type: HFont] Font handle
     * @param glyph_index [type: uint32_t] The glyph index inside the font
     * @param glyph [type: FontGlyph**] Returns the pointer to existing or newly created glyph
     * @return result [type: FontResult] FONT_RESULT_OK if successful
     **/
    FontResult GetOrCreateGlyphByIndex(dmRender::HFontMap font_map, HFont font, uint32_t glyph_index, FontGlyph** glyph);

    /**
     * Get a glyph or create a new instance from the font map
     * @name AddGlyphByIndex
     * @param font_map [type: HFontMap] Font map handle
     * @param hfont [type: HFont] Font handle
     * @param glyph_index [type: uint32_t] The glyph index inside the font
     * @param glyph [type: FontGlyph**] Returns the pointer to existing or newly created glyph
     * @return result [type: FontResult] FONT_RESULT_OK if successful
     **/
    void AddGlyphByIndex(dmRender::HFontMap font_map, HFont font, uint32_t glyph_index, FontGlyph* glyph);

    void RemoveGlyphByIndex(dmRender::HFontMap font_map, HFont font, uint32_t glyph_index);

    // Used in unit tests
    FontResult GetOrCreateGlyph(dmRender::HFontMap font_map, HFont font, uint32_t codepoint, FontGlyph** glyph);
    bool VerifyFontMapMinFilter(dmRender::HFontMap font_map, dmGraphics::TextureFilter filter);
    bool VerifyFontMapMagFilter(dmRender::HFontMap font_map, dmGraphics::TextureFilter filter);
}

#endif // DM_RENDER_FONTMAP_H
