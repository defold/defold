#ifndef DM_GAMESYS_RES_MODEL_H
#define DM_GAMESYS_RES_MODEL_H

#include <stdint.h>

#include <resource/resource.h>

#include <render/render.h>

#include "res_mesh.h"

namespace dmGameSystem
{
    struct Model
    {
        Mesh* m_Mesh;
        dmRender::HMaterial m_Material;
        dmGraphics::HTexture m_Textures[dmRender::RenderObject::MAX_TEXTURE_COUNT];
    };

    dmResource::Result ResPreloadModel(dmResource::HFactory factory, dmResource::HPreloadHintInfo hint_info,
                                      void* context,
                                      const void* buffer, uint32_t buffer_size,
                                      void** preload_data,
                                      const char* filename);

    dmResource::Result ResCreateModel(dmResource::HFactory factory,
                                      void* context,
                                      const void* buffer, uint32_t buffer_size,
                                      void* preload_data,
                                      dmResource::SResourceDescriptor* resource,
                                      const char* filename);

    dmResource::Result ResDestroyModel(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource);

    dmResource::Result ResRecreateModel(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename);
}

#endif // DM_GAMESYS_RES_MODEL_H
