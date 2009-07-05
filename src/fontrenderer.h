#ifndef FONTRENDERER_H
#define FONTRENDERER_H

#include <stdint.h>
#include <ddf/ddf.h>

namespace dmRender
{

    typedef struct SFontRenderer* HFontRenderer;

    HFontRenderer FontRendererNew(const char* file_name,
                                  uint32_t width, uint32_t height,
                                  uint32_t max_characters);

    void FontRendererDelete(HFontRenderer renderer);

    void FontRendererDrawString(HFontRenderer renderer, const char* string, uint16_t x0, uint16_t y0);

    void FontRendererFlush(HFontRenderer renderer);

}

#endif // FONTRENDERER_H

