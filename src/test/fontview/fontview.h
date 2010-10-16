#ifndef DM_RENDER_FONTVIEW_H
#define DM_RENDER_FONTVIEW_H

#include <stdint.h>

#include <resource/resource.h>

#include <graphics/graphics_device.h>

#include "../../fontrenderer.h"
#include "../../render/render.h"

#include "default/proto/render/render_ddf.h"

struct Context
{
    const char* m_TestString;
    dmRenderDDF::FontDesc* m_FontDesc;
    dmResource::HFactory m_Factory;
    dmRender::HFont m_Font;
    dmRender::HImageFont m_ImageFont;
    dmGraphics::HVertexProgram m_VertexProgram;
    dmGraphics::HFragmentProgram m_FragmentProgram;
    dmRender::HRenderWorld m_RenderWorld;
    dmRender::RenderContext m_RenderContext;
    dmRender::HRenderPass m_RenderPass;
    dmRender::HFontRenderer m_FontRenderer;
    uint32_t m_ScreenWidth;
    uint32_t m_ScreenHeight;
};

bool Init(Context* context, int argc, char *argv[]);
int32_t Run(Context* context);
void Finalize(Context* context);

dmResource::CreateResult ResImageFontCreate(dmResource::HFactory factory,
                                      void* context,
                                      const void* buffer, uint32_t buffer_size,
                                      dmResource::SResourceDescriptor* resource,
                                      const char* filename);

dmResource::CreateResult ResImageFontDestroy(dmResource::HFactory factory,
                                       void* context,
                                       dmResource::SResourceDescriptor* resource);

dmResource::CreateResult ResVertexProgramCreate(dmResource::HFactory factory,
                                             void* context,
                                             const void* buffer, uint32_t buffer_size,
                                             dmResource::SResourceDescriptor* resource,
                                             const char* filename);

dmResource::CreateResult ResVertexProgramDestroy(dmResource::HFactory factory,
                                              void* context,
                                              dmResource::SResourceDescriptor* resource);

dmResource::CreateResult ResFragmentProgramCreate(dmResource::HFactory factory,
                                               void* context,
                                               const void* buffer, uint32_t buffer_size,
                                               dmResource::SResourceDescriptor* resource,
                                               const char* filename);

dmResource::CreateResult ResFragmentProgramDestroy(dmResource::HFactory factory,
                                                void* context,
                                                dmResource::SResourceDescriptor* resource);

#endif // DM_RENDER_FONTVIEW_H
