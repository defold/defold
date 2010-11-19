#ifndef FONTRENDERER_H
#define FONTRENDERER_H

#include <stdint.h>

#include <ddf/ddf.h>

#include <graphics/graphics_device.h>
#include <graphics/material.h>

#include "render.h"

// Windows defines DrawText
#undef DrawText

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

    void InitializeTextContext(HRenderContext render_context, uint32_t max_characters);
    void FinalizeTextContext(HRenderContext render_context);

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
     * @param render_context Context to use when rendering
     * @param params Parameters to use when rendering
     */
    void DrawText(HRenderContext render_context, HFont font, const DrawTextParams& params);
}

#endif // FONTRENDERER_H

