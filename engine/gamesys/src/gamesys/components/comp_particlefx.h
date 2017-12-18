#ifndef DM_GAMESYS_COMP_PARTICLEFX_H
#define DM_GAMESYS_COMP_PARTICLEFX_H

#include <dlib/configfile.h>

#include <gameobject/gameobject.h>

#include <render/render.h>

namespace dmGameSystem
{
    dmGameObject::CreateResult CompParticleFXNewWorld(const dmGameObject::ComponentNewWorldParams& params);

    dmGameObject::CreateResult CompParticleFXDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);

    dmGameObject::CreateResult CompParticleFXCreate(const dmGameObject::ComponentCreateParams& params);

    dmGameObject::CreateResult CompParticleFXDestroy(const dmGameObject::ComponentDestroyParams& params);

    dmGameObject::CreateResult CompParticleFXAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params);

    dmGameObject::UpdateResult CompParticleFXUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result);

    dmGameObject::UpdateResult CompParticleFXRender(const dmGameObject::ComponentsRenderParams& params);

    dmGameObject::UpdateResult CompParticleFXOnMessage(const dmGameObject::ComponentOnMessageParams& params);

    void CompParticleFXOnReload(const dmGameObject::ComponentOnReloadParams& params);
}

#endif // DM_GAMESYS_COMP_PARTICLEFX_H
