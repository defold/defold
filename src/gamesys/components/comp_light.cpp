#include "comp_light.h"
#include "resources/res_light.h"

#include <dlib/log.h>
#include <dlib/hash.h>
#include <dlib/message.h>
#include <dlib/dstrings.h>
#include <gameobject/gameobject.h>
#include <vectormath/cpp/vectormath_aos.h>

namespace dmGameSystem
{
    using namespace Vectormath::Aos;

    dmGameObject::CreateResult CompLightNewWorld(void* context, void** world)
    {
        *world = new LightWorld;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompLightDeleteWorld(void* context, void* world)
    {
        LightWorld* light_world = (LightWorld*) world;
        delete light_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompLightCreate(dmGameObject::HCollection collection,
                                               dmGameObject::HInstance instance,
                                               void* resource,
                                               void* world,
                                               void* context,
                                               uintptr_t* user_data)
    {
        dmGameSystemDDF::LightDesc* light_desc = (dmGameSystemDDF::LightDesc*) resource;
        LightWorld* light_world = (LightWorld*) world;
        if (light_world->m_Lights.Full())
        {
            light_world->m_Lights.OffsetCapacity(16);
        }
        Light* light = new Light(instance, light_desc);
        light_world->m_Lights.Push(light);

        *user_data = (uintptr_t) light;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompLightDestroy(dmGameObject::HCollection collection,
                                                dmGameObject::HInstance instance,
                                                void* world,
                                                void* context,
                                                uintptr_t* user_data)
    {
        Light* light = (Light*) *user_data;
        LightWorld* light_world = (LightWorld*) world;
        for (uint32_t i = 0; i < light_world->m_Lights.Size(); ++i)
        {
            if (light_world->m_Lights[i] == light)
            {
                light_world->m_Lights.EraseSwap(i);
                delete light;
                return dmGameObject::CREATE_RESULT_OK;
            }
        }
        assert(false);
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompLightUpdate(dmGameObject::HCollection collection,
                                               const dmGameObject::UpdateContext* update_context,
                                               void* world,
                                               void* context)
    {
        LightWorld* light_world = (LightWorld*) world;
        char buf[sizeof(dmGameObject::InstanceMessageData) + sizeof(dmGameSystemDDF::SetLight) + 9];
        dmGameSystemDDF::SetLight* set_light = (dmGameSystemDDF::SetLight*) (buf + sizeof(dmGameObject::InstanceMessageData));

        dmMessage::HSocket socket_id = dmMessage::GetSocket("render");
        dmhash_t message_id = dmHashString64("set_light");

        for (uint32_t i = 0; i < light_world->m_Lights.Size(); ++i)
        {
            Light* light = light_world->m_Lights[i];
            Point3 position = dmGameObject::GetPosition(light->m_Instance);
            Quat rotation = dmGameObject::GetRotation(light->m_Instance);

            DM_SNPRINTF(buf + sizeof(dmGameObject::InstanceMessageData) + sizeof(dmGameSystemDDF::SetLight), 9, "%X", dmHashString32(light->m_LightDesc->m_Id));
            set_light->m_Light.m_Id = (const char*) sizeof(dmGameSystemDDF::SetLight);
            set_light->m_Light.m_Type = light->m_LightDesc->m_Type;
            set_light->m_Light.m_Intensity = light->m_LightDesc->m_Intensity;
            set_light->m_Light.m_Color = light->m_LightDesc->m_Color;
            set_light->m_Light.m_Range = light->m_LightDesc->m_Range;
            set_light->m_Light.m_Decay = light->m_LightDesc->m_Decay;
            set_light->m_Light.m_ConeAngle = light->m_LightDesc->m_ConeAngle;
            set_light->m_Light.m_PenumbraAngle = light->m_LightDesc->m_PenumbraAngle;
            set_light->m_Light.m_DropOff = light->m_LightDesc->m_DropOff;
            set_light->m_Position = position;
            set_light->m_Rotation = rotation;

            dmGameObject::InstanceMessageData* msg = (dmGameObject::InstanceMessageData*) buf;
            msg->m_BufferSize = sizeof(dmGameSystemDDF::SetLight) + 9;
            msg->m_DDFDescriptor = dmGameSystemDDF::SetLight::m_DDFDescriptor;
            msg->m_MessageId = message_id;

            dmMessage::Post(socket_id, message_id, buf, sizeof(buf));
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompLightOnMessage(dmGameObject::HInstance instance,
                                                  const dmGameObject::InstanceMessageData* message_data,
                                                  void* context,
                                                  uintptr_t* user_data)
    {
        return dmGameObject::UPDATE_RESULT_OK;
    }
}
