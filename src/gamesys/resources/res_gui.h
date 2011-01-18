#ifndef DM_GAMESYSTEM_RES_GUI_H
#define DM_GAMESYSTEM_RES_GUI_H

#include <dlib/array.h>
#include <resource/resource.h>
#include <gameobject/gameobject.h>
#include <render/font_renderer.h>
#include "../proto/gui_ddf.h"

namespace dmGameSystem
{
    struct GuiScenePrototype
    {
        dmGuiDDF::SceneDesc*          m_SceneDesc;
        const char*                   m_Script;
        dmArray<dmRender::HFontMap>   m_FontMaps;
        dmArray<dmGraphics::HTexture> m_Textures;
        const char*                   m_Path;
    };

    dmResource::CreateResult ResCreateSceneDesc(dmResource::HFactory factory,
                                          void* context,
                                          const void* buffer, uint32_t buffer_size,
                                          dmResource::SResourceDescriptor* resource,
                                          const char* filename);

    dmResource::CreateResult ResDestroySceneDesc(dmResource::HFactory factory,
                                           void* context,
                                           dmResource::SResourceDescriptor* resource);

    dmResource::CreateResult ResCreateGuiScript(dmResource::HFactory factory,
                                          void* context,
                                          const void* buffer, uint32_t buffer_size,
                                          dmResource::SResourceDescriptor* resource,
                                          const char* filename);

    dmResource::CreateResult ResDestroyGuiScript(dmResource::HFactory factory,
                                           void* context,
                                           dmResource::SResourceDescriptor* resource);

}

#endif // DM_GAMESYSTEM_RES_GUI_H
