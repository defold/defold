#ifndef FONTRENDERER_H
#define FONTRENDERER_H

#include <stdint.h>

#include <ddf/ddf.h>

#include <graphics/graphics.h>

#include <render/font_ddf.h>

#include "render.h"

namespace dmRender
{
    /**
     * Glyph struct
     */
    struct Glyph
    {
        uint32_t    m_Character;
        /// Width of the glyph
        uint32_t    m_Width;
        /// Total advancement of the glyph, measured from left to the next glyph
        float       m_Advance;
        /// Where the glyph starts, measured from the left
        float       m_LeftBearing;
        /// How far up the glyph starts, measured from the bottom line
        uint32_t    m_Ascent;
        /// How far up the glyph reaches, measured from the top line
        uint32_t    m_Descent;
        /// X coordinate of the glyph in the map
        int32_t     m_X;
        /// Y coordinate of the glyph in the map
        int32_t     m_Y;

        bool        m_InCache;
        uint64_t    m_GlyphDataOffset;
        uint64_t    m_GlyphDataSize;
        uint32_t    m_Frame;
    };

    struct DM_ALIGNED(16) GlyphVertex
    {
        // NOTE: The struct *must* be 16-bytes aligned due to SIMD operations.
        float    m_Position[4];
        float    m_UV[2];
        uint32_t m_FaceColor;
        uint32_t m_OutlineColor;
        uint32_t m_ShadowColor;
        float    m_SdfParams[4];
    };

    /**
     * Font map parameters supplied to NewFontMap
     */
    struct FontMapParams
    {
        /// Default constructor
        FontMapParams();

        /// All glyphs represented in the map
        dmArray<Glyph> m_Glyphs;
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
        /// Font alpha
        float m_Alpha;
        /// Font outline alpha
        float m_OutlineAlpha;
        /// Font shadow alpha
        float m_ShadowAlpha;

        uint32_t m_CacheWidth;
        uint32_t m_CacheHeight;
        uint8_t m_GlyphChannels;
        void* m_GlyphData;

        uint32_t m_CacheCellWidth;
        uint32_t m_CacheCellHeight;
        uint8_t m_CacheCellPadding;

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
    };

    /**
     * Create a new font map. The parameters struct is consumed and should not be read after this call.
     * @param graphics_context Graphics context handle
     * @param params Params used to initialize the font map
     * @return HFontMap on success. NULL on failure
     */
    HFontMap NewFontMap(dmGraphics::HContext graphics_context, FontMapParams& params);

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
    void SetFontMap(HFontMap font_map, FontMapParams& params);

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

    void InitializeTextContext(HRenderContext render_context, uint32_t max_characters);
    void FinalizeTextContext(HRenderContext render_context);

    const int MAX_FONT_RENDER_CONSTANTS = 4;
    /**
     * Draw text params.
     */
    struct DrawTextParams
    {
        DrawTextParams();

        /// Transform from font space to world (origo in font space is the base line of the first glyph)
        Vectormath::Aos::Matrix4 m_WorldTransform;
        /// Color of the font face
        Vectormath::Aos::Vector4 m_FaceColor;
        /// Color of the outline
        Vectormath::Aos::Vector4 m_OutlineColor;
        /// Color of the shadow
        Vectormath::Aos::Vector4 m_ShadowColor;
        /// Text to draw in utf8-format
        const char* m_Text;
        /// Render constants
        dmRender::Constant m_RenderConstants[MAX_FONT_RENDER_CONSTANTS];
        /// The source blend factor
        dmGraphics::BlendFactor m_SourceBlendFactor;
        /// The destination blend factor
        dmGraphics::BlendFactor m_DestinationBlendFactor;
        /// Render order value. Passed to the render-key
        uint16_t    m_RenderOrder;
        /// Number render constants
        uint8_t     m_NumRenderConstants;
        /// Text render box width. Used for alignment and when m_LineBreak is true
        float       m_Width;
        /// Text render box height. Used for vertical alignment
        float       m_Height;
        /// Text line spacing
        float       m_Leading;
        /// Text letter spacing
        float       m_Tracking;
        /// True for linebreak
        bool        m_LineBreak;
        /// Horizontal alignment
        TextAlign m_Align;
        /// Vertical alignment
        TextVAlign m_VAlign;
        /// Stencil parameters
        StencilTestParams m_StencilTestParams;
        /// Stencil parameters set or not
        uint8_t m_StencilTestParamsSet : 1;
    };

    /**
     * Draw text
     * @param render_context Context to use when rendering
     * @param font_map Font map handle
     * @param params Parameters to use when rendering
     */
    void DrawText(HRenderContext render_context, HFontMap font_map, HMaterial material, uint64_t batch_key, const DrawTextParams& params);

    /**
     * Produces render list entries for all the previously DrawText:ed texts.
     * Multiple calls can be made with final=false, but one (last) call
     * with final=true must be made, so that the vertex buffers will be
     * written.
     *
     * @param final If this is the last call.
     * @param render_order Render order to write for the rendering
     * @param render_context Context to use when rendering
     */
    void FlushTexts(HRenderContext render_context, uint32_t major_order, uint32_t render_order, bool final);

    /**
     * Get text metrics for string
     * @param font_map Font map handle
     * @param text utf8 text to get metrics for
     * @param width max width. used only when line_break is true
     * @param line_break line break characters
     * @param metrics Metrics, out-value
     */
    void GetTextMetrics(HFontMap font_map, const char* text, float width, bool line_break, float leading, float tracking, TextMetrics* metrics);

    /**
     * Get the resource size for fontmap
     * @param font_map Font map handle
     * @return size
     */
    uint32_t GetFontMapResourceSize(HFontMap font_map);
}

#endif // FONTRENDERER_H
