// Copyright 2020-2024 The Defold Foundation
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

#include <stdint.h>

#include <dlib/hash.h>
#include <graphics/graphics.h>

#include <render/font_ddf.h>

#include "render.h"

namespace dmRender
{
    // Old typedef, used internally. We want to migrate towards HFont
    typedef struct FontMap* HFontMap;

    struct FontMetrics
    {
        float m_MaxAscent;
        float m_MaxDescent;
        uint32_t m_MaxWidth;   // The widest one in terms of texels
        uint32_t m_MaxHeight;  // The tallest one in terms of texels
    };

    typedef dmRenderDDF::GlyphBank::Glyph FontGlyph;
    typedef FontGlyph*  (*FGetGlyph)(uint32_t codepoint, void* user_ctx);
    typedef void*       (*FGetGlyphData)(uint32_t codepoint, void* user_ctx, uint32_t* out_size, uint32_t* out_compression, uint32_t* out_width, uint32_t* out_height, uint32_t* out_channels);
    typedef uint32_t    (*FGetFontMetrics)(void* user_ctx, FontMetrics* metrics); // returns number of glyphs

    /**
     * Font map parameters supplied to NewFontMap
     */
    struct FontMapParams
    {
        /// Default constructor
        FontMapParams();

        FGetGlyph       m_GetGlyph;
        FGetGlyphData   m_GetGlyphData;
        FGetFontMetrics m_GetFontMetrics;

        dmhash_t        m_NameHash;

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
        /// Value to offset SDF texture values with
        float m_SdfOffset;
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
        uint32_t m_CacheCellWidth;
        uint32_t m_CacheCellHeight;
        uint32_t m_CacheCellMaxAscent;
        uint8_t m_GlyphChannels; // How many bitmap channels
        uint8_t m_CacheCellPadding;
        uint8_t m_LayerMask;

        uint8_t m_IsMonospaced:1;
        uint8_t m_Padding:7;

        dmRenderDDF::FontTextureFormat m_ImageFormat;
    };

    /**
     * Font metrics about a text string
     */
    struct TextMetrics
    {
        /// Total string width
        float m_Width;
        /// Total string height
        float m_Height;
        /// Max ascent of font
        float m_MaxAscent;
        /// Max descent of font, positive value
        float m_MaxDescent;
        /// Number of lines of text
        uint32_t m_LineCount;
    };

    /**
     * Input settings when getting text metrics
     */
    struct TextMetricsSettings
    {
        /// Max width. used only when line_break is true
        float m_Width;
        /// Allow line break
        bool m_LineBreak;
        ///
        float m_Leading;
        ///
        float m_Tracking;
    };

    // Represents a slot in the texture glyph cache
    // TODO: Make this private
    // TODO: Add api to get the UV quad for the glyph
    struct CacheGlyph
    {
        dmRender::FontGlyph* m_Glyph;
        uint32_t             m_Frame; // Age
        int16_t              m_X;
        int16_t              m_Y;
        // todo, add page here as well
    };


    /**
     * Create a new font map. The parameters struct is consumed and should not be read after this call.
     * @param graphics_context Graphics context handle
     * @param params Params used to initialize the font map
     * @return HFontMap on success. NULL on failure
     */
    HFontMap NewFontMap(dmRender::HRenderContext render_context, dmGraphics::HContext graphics_context, FontMapParams& params);

    void SetFontMapCacheSize(HFontMap font_map, uint32_t cell_width, uint32_t cell_height, uint32_t max_ascent);
    void GetFontMapCacheSize(HFontMap font_map, uint32_t* cell_width, uint32_t* cell_height, uint32_t* max_ascent);

    /**
     * Delete a font map
     * @param font_map Font map handle
     */
    void DeleteFontMap(HFontMap font_map);

    /**
     * Update the font map with the specified parameters. The parameters are consumed and should not be read after this call.
     * @param font_map Font map handle
     * @param params Parameters to update
     */
    void SetFontMap(HFontMap font_map, dmRender::HRenderContext render_context, dmGraphics::HContext graphics_context, FontMapParams& params);

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

    /**
     * Get text metrics for string
     * @param font_map Font map handle
     * @param text utf8 text to get metrics for
     * @param settings settings for getting the text metrics
     * @param metrics Metrics, out-value
     */
    void GetTextMetrics(HFontMap font_map, const char* text, TextMetricsSettings* settings, TextMetrics* metrics);

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
     * @return cache_glyph [type: CacheGlyph*] the glyph cache info
     */
    CacheGlyph* GetFromCache(HFontMap font_map, uint32_t c);

    /**
     * Checks if a codepoint is already stored within the cache
     * @param font_map [type: HFontMap] Font map handle
     * @param codepoint [type: uint32_t] The unicode codepoint
     * @return result [type: bool] true if the glyph is already cached
     */
    bool IsInCache(HFontMap font_map, uint32_t codepoint);

    /**
     * Adds a font glyph to the glyph texture cache
     * @param font_map [type: HFontMap] Font map handle
     * @param frame [type: uint32_t] The current frame number. Used to for evicting old cache entries.
     * @param glyph [type: dmRender::FontGlyph*] The current frame number. Used to for evicting old cache entries.
     * @param g_offset_y [type: int32_t] The offset from the top of the cache cell. Used to align the glyph with the baseline.
     */
    void AddGlyphToCache(HFontMap font_map, uint32_t frame, FontGlyph* glyph, int32_t g_offset_y);

    /**
     * Get the glyph from the font map given a unicode codepoint
     * @param font_map [type: HFontMap] Font map handle
     * @param codepoint [type: uint32_t] The unicode codepoint
     * @return glyph [type: FontGlyph*] the glyph
     */
    dmRender::FontGlyph* GetGlyph(dmRender::HFontMap font_map, uint32_t codepoint);

    /**
     * Get the rendered glyph image from the font map given a unicode codepoint.
     * @param font_map [type: HFontMap] Font map handle
     * @param codepoint [type: uint32_t] The unicode codepoint
     * @param out_size [out] [type: uint32_t*] The size of the image data payload
     * @param out_compression [out] [type: uint32_t*] Tells if the data is compressed. 1 = deflate, 0 = uncompressed.
     * @param out_width [out] [type: uint32_t*] The width of the final image
     * @param out_height [out] [type: uint32_t*] The height of the final image
     * @param out_channels [out] [type: uint32_t*] The number of channels of the final image
     * @return data [type: uint8_t*] the image data. See out_compression.
     */
    const uint8_t* GetGlyphData(dmRender::HFontMap font_map, uint32_t codepoint, uint32_t* out_size, uint32_t* out_compression, uint32_t* out_width, uint32_t* out_height, uint32_t* out_channels);

    // Used in unit tests
    bool VerifyFontMapMinFilter(dmRender::HFontMap font_map, dmGraphics::TextureFilter filter);
    bool VerifyFontMapMagFilter(dmRender::HFontMap font_map, dmGraphics::TextureFilter filter);
}

#endif // DM_FONT_H
