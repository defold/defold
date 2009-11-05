#ifndef FONTRENDERER_H
#define FONTRENDERER_H

#include <stdint.h>
#include <ddf/ddf.h>

namespace dmRender
{
    /**
     * Font handle
     */
    typedef struct SFont* HFont;

    /**
     * Create a new font
     * @param font Font buffer (ddf format)
     * @param font_size Font buffer size
     * @return HFont on success. NULL on failure
     */
    HFont NewFont(const void* font, uint32_t font_size);

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
     * @param width Width
     * @param height Height
     * @param max_characters Max characters to render
     * @return Font renderer handle
     *
     */
    HFontRenderer NewFontRenderer(HFont font,
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
     */
    void FontRendererDrawString(HFontRenderer renderer, const char* string, uint16_t x0, uint16_t y0);

    /**
     * Flush drawn characters
     * @param renderer Font renderer handle
     */
    void FontRendererFlush(HFontRenderer renderer);

}

#endif // FONTRENDERER_H

