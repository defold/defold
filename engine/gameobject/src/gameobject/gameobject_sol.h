#ifndef GAMEOBJECT_SOL_H
#define GAMEOBJECT_SOL_H

#include "gameobject.h"

namespace dmGameObject
{
    CreateResult SolComponentNewWorld(const ComponentNewWorldParams& params);
    CreateResult SolComponentDeleteWorld(const ComponentDeleteWorldParams& params);
    
    CreateResult SolComponentCreate(const ComponentCreateParams& params);
    CreateResult SolComponentInit(const ComponentInitParams& params);
    CreateResult SolComponentFinal(const ComponentFinalParams& params);
    CreateResult SolComponentDestroy(const ComponentDestroyParams& params);
    
    CreateResult SolComponentAddToUpdate(const ComponentAddToUpdateParams& params);
    UpdateResult SolComponentUpdate(const ComponentsUpdateParams& params);
    UpdateResult SolComponentPostUpdate(const ComponentsPostUpdateParams& params);
    UpdateResult SolComponentRender(const ComponentsRenderParams& params);
    UpdateResult SolComponentOnMessage(const ComponentOnMessageParams& params);

    PropertyResult SolComponentSetProperties(const ComponentSetPropertiesParams& params);
    PropertyResult SolComponentSetProperty(const ComponentSetPropertyParams& params);
    PropertyResult SolComponentGetProperty(const ComponentGetPropertyParams& params, PropertyDesc& out_value);
    
    void SolComponentOnReload(const ComponentOnReloadParams& params);
}

#endif
