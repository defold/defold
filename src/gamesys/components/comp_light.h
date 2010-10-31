#ifndef DM_GAMESYS_COMP_LIGHT_H
#define DM_GAMESYS_COMP_LIGHT_H

#include <dlib/array.h>
#include <gameobject/gameobject.h>
#include "gamesys_ddf.h"

namespace dmGameSystem
{
    struct Light
    {
        dmGameObject::HInstance      m_Instance;
        dmGameSystemDDF::LightDesc* m_LightDesc;
        Light(dmGameObject::HInstance instance, dmGameSystemDDF::LightDesc* light_desc)
        {
            m_Instance = instance;
            m_LightDesc = light_desc;
        }
    };

    struct LightWorld
    {
        dmArray<Light*> m_Lights;
    };

    dmGameObject::CreateResult CompLightNewWorld(void* context, void** world);

    dmGameObject::CreateResult CompLightDeleteWorld(void* context, void* world);

    dmGameObject::CreateResult CompLightCreate(dmGameObject::HCollection collection,
                                               dmGameObject::HInstance instance,
                                               void* resource,
                                               void* world,
                                               void* context,
                                               uintptr_t* user_data);

    dmGameObject::CreateResult CompLightDestroy(dmGameObject::HCollection collection,
                                                dmGameObject::HInstance instance,
                                                void* world,
                                                void* context,
                                                uintptr_t* user_data);

    dmGameObject::UpdateResult CompLightUpdate(dmGameObject::HCollection collection,
                                               const dmGameObject::UpdateContext* update_context,
                                               void* world,
                                               void* context);

    dmGameObject::UpdateResult CompLightOnMessage(dmGameObject::HInstance instance,
                                                  const dmGameObject::InstanceMessageData* message_data,
                                                  void* context,
                                                  uintptr_t* user_data);
}

#endif
