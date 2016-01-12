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

    dmResource::Result ResPreloadModel(const dmResource::ResourcePreloadParams& params);

    dmResource::Result ResCreateModel(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResDestroyModel(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResRecreateModel(const dmResource::ResourceRecreateParams& params);
}

#endif // DM_GAMESYS_RES_MODEL_H
