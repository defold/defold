#ifndef DM_GAMESYS_RES_SPINE_MODEL_H
#define DM_GAMESYS_RES_SPINE_MODEL_H

#include <stdint.h>

#include <resource/resource.h>
#include "res_rig_scene.h"
#include "spine_ddf.h"

namespace dmGameSystem
{
    struct SpineModelResource
    {
        dmGameSystemDDF::SpineModelDesc*    m_Model;
        RigSceneResource*                   m_RigScene;
        dmRender::HMaterial                 m_Material;
        dmGraphics::HTexture                m_Textures[dmRender::RenderObject::MAX_TEXTURE_COUNT];
    };

    dmResource::Result ResSpineModelPreload(const dmResource::ResourcePreloadParams& params);

    dmResource::Result ResSpineModelCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResSpineModelDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResSpineModelRecreate(const dmResource::ResourceRecreateParams& params);
}

#endif // DM_GAMESYS_RES_SPINE_MODEL_H
