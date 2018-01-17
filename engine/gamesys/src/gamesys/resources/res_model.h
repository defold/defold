#ifndef DM_GAMESYS_RES_MODEL_H
#define DM_GAMESYS_RES_MODEL_H

#include <stdint.h>

#include <resource/resource.h>
#include "res_rig_scene.h"
#include "model_ddf.h"

namespace dmGameSystem
{
    struct ModelResource
    {
        dmModelDDF::Model*      m_Model;
        RigSceneResource*       m_RigScene;
        dmRender::HMaterial     m_Material;
    };

    dmResource::Result ResModelPreload(const dmResource::ResourcePreloadParams& params);

    dmResource::Result ResModelCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResModelDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResModelRecreate(const dmResource::ResourceRecreateParams& params);
}

#endif // DM_GAMESYS_RES_MODEL_H
