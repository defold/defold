#ifndef FONTRENDERER_H
#define FONTRENDERER_H

#include <stdint.h>

#include <ddf/ddf.h>

#include <graphics/graphics_device.h>
#include <graphics/material.h>

#include "render.h"

namespace dmRender
{
    /**
     * Image font handle
     */
    typedef void* HImageFont;

    /**
     * Create a new image font
     * @param font Font buffer (ddf format)
     * @param font_size Font buffer size
     * @return HFont on success. NULL on failure
     */
    HImageFont NewImageFont(const void* font, uint32_t font_size);

    /**
     * Delete a image font
     * @param font Image font handle
     */
    void DeleteImageFont(HImageFont font);


    /**
     * Font handle
     */
    typedef struct Font* HFont;

    /**
     * Create a new font
     * @param image_font Image font
     * @return Font handle on success. NULL on failure.
     */
    HFont NewFont(HImageFont image_font);

    /**
     * Get image font for font
     * @param font Font handle
     * @return HImageFont handle
     */
    HImageFont GetImageFont(HFont font);

    dmGraphics::HTexture GetTexture(HFont font);

    /**
     * Set font material
     * @param font Font handle
     * @param material Material handle
     */
    void SetMaterial(HFont font, dmGraphics::HMaterial material);

    /**
     * Get font material
     * @param font Font handle
     * @return Material handle
     */
    dmGraphics::HMaterial GetMaterial(HFont font);

    /**
     * Delete a font
     * @param font Font handle
     */
    void DeleteFont(HFont font);

    /**
     * Font render handle
     */
    typedef struct FontRenderer* HFontRenderer;

    /**
     * Create a new font renderer
     * @param font Font
     * @param render_world Render world context
     * @param width Width
     * @param height Height
     * @param max_characters Max characters to render
     * @return Font renderer handle
     *
     */
    HFontRenderer NewFontRenderer(HRenderContext render_context, HFont font,
                                  uint32_t width, uint32_t height,
                                  uint32_t max_characters);
    /**
     * Deletes a font renderer
     * @param renderer Font renderer handle
     */
    void DeleteFontRenderer(HFontRenderer renderer);


    /**
     * Draw text params.
     */
    struct DrawTextParams
    {
        DrawTextParams();

        /// Color of the font face
        Vectormath::Aos::Vector4 m_FaceColor;
        /// Color of the outline
        Vectormath::Aos::Vector4 m_OutlineColor;
        /// Color of the shadow
        Vectormath::Aos::Vector4 m_ShadowColor;
        /// Text to draw
        const char* m_Text;
        /// X of the baseline
        uint16_t m_X;
        /// Y of the baseline
        uint16_t m_Y;
    };

    /**
     * Draw text
     * @param renderer Font renderer handle
     * @param params Parameters to use when rendering
     */
    void FontRendererDrawText(HFontRenderer renderer, const DrawTextParams& params);

    /**
     * Clear vertex data
     * @param renderer Font renderer handle
     */
    void FontRendererClear(HFontRenderer renderer);
}

#endif // FONTRENDERER_H

