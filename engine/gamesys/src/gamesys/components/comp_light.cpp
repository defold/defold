// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include <string.h>

#include <dlib/math.h>
#include <dlib/log.h>
#include <dlib/object_pool.h>

#include <gameobject/component.h>

#include <dmsdk/gamesys/resources/res_light.h>
#include <dmsdk/resource/resource.h>

namespace dmGameSystem
{
    struct LightComponent
    {
        LightResource* m_Resource;
        uint8_t       m_AddedToUpdate : 1;
    };

    struct LightWorld
    {
        dmObjectPool<LightComponent> m_Components;
    };

    static dmGameObject::CreateResult CompLightNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        LightWorld* world = new LightWorld();
        uint32_t comp_count = dmMath::Min(params.m_MaxComponentInstances, 256u);
        world->m_Components.SetCapacity(comp_count);
        *params.m_World = world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    static dmGameObject::CreateResult CompLightDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        delete (LightWorld*)params.m_World;
        return dmGameObject::CREATE_RESULT_OK;
    }

    static dmGameObject::CreateResult CompLightCreate(const dmGameObject::ComponentCreateParams& params)
    {
        LightWorld* world = (LightWorld*)params.m_World;
        if (world->m_Components.Full())
        {
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }

        uint32_t index = world->m_Components.Alloc();
        LightComponent* component = &world->m_Components.Get(index);
        component->m_Resource = (LightResource*)params.m_Resource;
        component->m_AddedToUpdate = 0;

        *params.m_UserData = (uintptr_t)index;
        return dmGameObject::CREATE_RESULT_OK;
    }

    static void* CompLightGetComponent(const dmGameObject::ComponentGetParams& params)
    {
        LightWorld* world = (LightWorld*)params.m_World;
        return &world->m_Components.Get(params.m_UserData);
    }

    static dmGameObject::CreateResult CompLightDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        LightWorld* world = (LightWorld*)params.m_World;
        uint32_t index = (uint32_t)*params.m_UserData;
        if (index < world->m_Components.Capacity())
        {
            world->m_Components.Free(index, false);
        }
        return dmGameObject::CREATE_RESULT_OK;
    }

    static dmGameObject::CreateResult CompLightAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params)
    {
        LightWorld* world = (LightWorld*)params.m_World;
        uint32_t index = (uint32_t)*params.m_UserData;
        LightComponent* component = &world->m_Components.Get(index);
        component->m_AddedToUpdate = 1;
        return dmGameObject::CREATE_RESULT_OK;
    }

    static dmGameObject::Result CompLightcInit(const dmGameObject::ComponentTypeCreateCtx* ctx, dmGameObject::ComponentType* type)
    {
        dmResource::HResourceType data_resource_type;
        dmResource::Result r = dmResource::GetTypeFromExtension(ctx->m_Factory, "lightc", &data_resource_type);
        if (r != dmResource::RESULT_OK)
        {
            dmLogWarning("Unable to get resource type for '%s': %s", "lightc", dmResource::ResultToString(r));
            return dmGameObject::RESULT_UNKNOWN_ERROR;
        }
        type->m_ResourceType = data_resource_type;

        dmGameObject::ComponentTypeSetPrio(type, 1000);
        dmGameObject::ComponentTypeSetContext(type, 0);
        dmGameObject::ComponentTypeSetHasUserData(type, true);
        dmGameObject::ComponentTypeSetReadsTransforms(type, false);

        dmGameObject::ComponentTypeSetNewWorldFn(type, CompLightNewWorld);
        dmGameObject::ComponentTypeSetDeleteWorldFn(type, CompLightDeleteWorld);
        dmGameObject::ComponentTypeSetCreateFn(type, CompLightCreate);
        dmGameObject::ComponentTypeSetDestroyFn(type, CompLightDestroy);
        dmGameObject::ComponentTypeSetAddToUpdateFn(type, CompLightAddToUpdate);
        dmGameObject::ComponentTypeSetGetFn(type, CompLightGetComponent);

        return dmGameObject::RESULT_OK;
    }

    static dmGameObject::Result CompLightcExit(const dmGameObject::ComponentTypeCreateCtx* ctx, dmGameObject::HComponentType type)
    {
        return dmGameObject::RESULT_OK;
    }
}

DM_DECLARE_COMPONENT_TYPE(ComponentTypeLight, "lightc", dmGameSystem::CompLightcInit, dmGameSystem::CompLightcExit);
