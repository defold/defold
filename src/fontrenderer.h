#ifndef FONTRENDERER_H
#define FONTRENDERER_H

#include <stdint.h>
#include <ddf/ddf.h>
#include <graphics/graphics_device.h>
#include <render/render.h>

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
    typedef struct SFont* HFont;

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
     * Set font vertex program
     * @param font Font handle
     * @param program Vertex program handle
     */
    void SetVertexProgram(HFont font, dmGraphics::HVertexProgram program);

    /**
     * Get font vertex program
     * @param font Font handle
     * @return Vertex program
     */
    dmGraphics::HVertexProgram GetVertexProgram(HFont font);

    /**
     * Get font fragment program
     * @param font Font handle
     * @return Fragment program
     */
    dmGraphics::HFragmentProgram GetFragmentProgram(HFont font);

    /**
     * Set font fragment program
     * @param font Font handle
     * @param program Fragment program handle
     */
    void SetFragmentProgram(HFont font, dmGraphics::HFragmentProgram program);

    /**
     * Delete a font
     * @param font Font handle
     */
    void DeleteFont(HFont font);

    /**
     * Font render handle
     */
    typedef struct SFontRenderer* HFontRenderer;

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
    HFontRenderer NewFontRenderer(HFont font, void* render_world,
                                  uint32_t width, uint32_t height,
                                  uint32_t max_characters);
    /**
     * Deletes a font renderer
     * @param renderer Font renderer handle
     */
    void DeleteFontRenderer(HFontRenderer renderer);

    /**
     * Draw string
     * @param renderer Font renderer handle
     * @param string String to render
     * @param x0 X
     * @param y0 Y
     * @param red Red color component
     * @param green Green color component
     * @param blue Blue color component
     */
    void FontRendererDrawString(HFontRenderer renderer, const char* string, uint16_t x0, uint16_t y0, float red, float green, float blue, float alpha);

    /**
     * Flush drawn characters
     * @param renderer Font renderer handle
     */
    void FontRendererFlush(HFontRenderer renderer);

    void FontRendererAddToRenderPass(HRenderPass renderpass, HFontRenderer renderer);

}

#endif // FONTRENDERER_H

