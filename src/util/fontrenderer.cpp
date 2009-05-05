#include "fontrenderer.h"
#include <vector>

#include <vectormath/cpp/vectormath_aos.h>
using namespace Vectormath::Aos;

#include "graphics_device.h"

#include "default/proto/graphics_util_ddf.h"

namespace GFXUtil
{

struct SFontVertex
{
    float m_Position[3];
    float m_UV[2];
};

struct SFontRenderer
{
    Font*                          m_Font;
    std::vector<SFontVertex>       m_Vertices;
    Matrix4                        m_Projection;
    GFXHTexture                    m_Texture;
    uint32_t                       m_MaxCharacters;
};

HFontRenderer FontRendererNew(const char* file_name,
                              uint32_t width, uint32_t height,
                              uint32_t max_characters)

{
    Font* f;
    DDFError e = DDFLoadMessageFromFile(file_name,
                                        &GFXUtil_Font_DESCRIPTOR, (void**) &f);
    assert( e == DDF_ERROR_OK );

    SFontRenderer* fr = new SFontRenderer();
    fr->m_Font = f;
    fr->m_Projection = Matrix4::orthographic( 0, width, height, 0, 10, -10 );
    fr->m_Vertices.reserve(max_characters * 6);
    fr->m_Texture = GFXCreateTexture(fr->m_Font->m_ImageWidth, fr->m_Font->m_ImageHeight, GFX_TEXTURE_FORMAT_LUMINANCE);
    fr->m_MaxCharacters = max_characters;

    GFXSetTextureData(fr->m_Texture, 0, fr->m_Font->m_ImageWidth, fr->m_Font->m_ImageHeight, 0,
                      GFX_TEXTURE_FORMAT_LUMINANCE, &fr->m_Font->m_ImageData[0], fr->m_Font->m_ImageData.m_Count);
    return fr;
}

void FontRendererDelete(HFontRenderer renderer)
{
    delete renderer;
}

void FontRendererDrawString(HFontRenderer renderer, const char* string, uint16_t x0, uint16_t y0)
{
    if (renderer->m_Vertices.size() * 6 >= renderer->m_MaxCharacters)
    {
        return;
    }

    int n = strlen(string);
    uint16_t x = x0;
    uint16_t y = y0;

    for (int i = 0; i < n; ++i)
    {
        char c = string[i];

        Font::Glyph g = renderer->m_Font->m_Glyphs[c];

        SFontVertex v1, v2, v3, v4;

        v1.m_Position[0] = x;
        v1.m_Position[1] = y;
        v1.m_Position[2] = 0;

        v2.m_Position[0] = x + g.m_Width;
        v2.m_Position[1] = y;
        v2.m_Position[2] = 0;

        v3.m_Position[0] = x + g.m_Width;
        v3.m_Position[1] = y + g.m_Height;
        v3.m_Position[2] = 0;

        v4.m_Position[0] = x;
        v4.m_Position[1] = y + g.m_Height;
        v4.m_Position[2] = 0;

        float im_recip = 1.0f / renderer->m_Font->m_ImageWidth;
        float ih_recip = 1.0f / renderer->m_Font->m_ImageHeight;

        v1.m_UV[0] = (g.m_X) * im_recip;
        v1.m_UV[1] = (g.m_Y) * ih_recip;

        v2.m_UV[0] = (g.m_X + g.m_Width) * im_recip;
        v2.m_UV[1] = (g.m_Y) * ih_recip;

        v3.m_UV[0] = (g.m_X + g.m_Width) * im_recip;
        v3.m_UV[1] = (g.m_Y + g.m_Height) * ih_recip;

        v4.m_UV[0] = (g.m_X) * im_recip;
        v4.m_UV[1] = (g.m_Y + g.m_Height) * ih_recip;

        renderer->m_Vertices.push_back(v1);
        renderer->m_Vertices.push_back(v2);
        renderer->m_Vertices.push_back(v3);

        renderer->m_Vertices.push_back(v3);
        renderer->m_Vertices.push_back(v4);
        renderer->m_Vertices.push_back(v1);

        x += g.m_Width;
    }
}

void FontRendererFlush(HFontRenderer renderer)
{
    GFXHContext context = GFXGetContext();
    Matrix4 ident = Matrix4::identity();

    GFXSetVertexConstantBlock(context, (const Vector4*)&renderer->m_Projection, 0, 4);
    GFXSetVertexConstantBlock(context, (const Vector4*)&ident, 4, 4);


    GFXSetVertexStream(context, 0, 3, GFX_TYPE_FLOAT,
                       sizeof(SFontVertex),
                       (void*) &renderer->m_Vertices[0].m_Position[0]);

    GFXSetVertexStream(context, 1, 2, GFX_TYPE_FLOAT,
                       sizeof(SFontVertex),
                       (void*) &renderer->m_Vertices[0].m_UV[0]);

    GFXSetTexture(context, renderer->m_Texture);

    GFXSetBlendFunc(context, GFX_BLEND_FACTOR_SRC_ALPHA, GFX_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
    GFXDisableState(context, GFX_DEPTH_TEST);
    GFXEnableState(context, GFX_BLEND);

    GFXDraw(context, GFX_PRIMITIVE_TRIANGLES, 0, renderer->m_Vertices.size());

    GFXEnableState(context, GFX_DEPTH_TEST);
    GFXDisableState(context, GFX_BLEND);

    GFXDisableVertexStream(context, 1);

    renderer->m_Vertices.clear();
}

}
