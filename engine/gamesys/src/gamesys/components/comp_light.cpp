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

#include <gameobject/component.h>

#include <gamesys/gamesys.h>
#include <gamesys/gamesys_private.h>

#include <dmsdk/gamesys/resources/res_light.h>
#include <dmsdk/resource/resource.h>

namespace dmGameSystem
{
    static const char* LIGHT_MAX_COUNT_KEY = "light.max_count";

    struct LightContext
    {
        LightContext()
        {
            memset(this, 0, sizeof(*this));
        }
        dmRender::HRenderContext m_RenderContext;
        dmResource::HFactory     m_Factory;
        uint32_t                 m_MaxLightCount;
    };

    struct LightComponent
    {
        dmGameObject::HInstance  m_Instance;
        LightResource*           m_LightResource;
        dmRender::HLightInstance m_LightInstance;
        uint16_t                 m_AddedToUpdate : 1;
        uint16_t                                 : 15;
    };

    struct LightWorld
    {
        dmArray<LightComponent*> m_Components;
    };

    static dmGameObject::CreateResult CompLightNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        LightContext* context = (LightContext*) params.m_Context;
        LightWorld* world = new LightWorld;
        uint32_t comp_count = dmMath::Min(params.m_MaxComponentInstances, context->m_MaxLightCount);
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
        LightWorld* world = (LightWorld*) params.m_World;
        LightContext* context = (LightContext*)params.m_Context;

        if (world->m_Components.Full())
        {
            ShowFullBufferError("Light", LIGHT_MAX_COUNT_KEY, world->m_Components.Capacity());
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }

        LightComponent* light  = new LightComponent;
        memset(light, 0, sizeof(LightComponent));

        light->m_Instance      = params.m_Instance;
        light->m_LightResource = (LightResource*) params.m_Resource;
        light->m_LightInstance = dmRender::NewLightInstance(context->m_RenderContext, GetLightPrototype(light->m_LightResource));
        if (light->m_LightInstance == 0)
        {
            ShowFullBufferError("Light", LIGHT_MAX_COUNT_KEY, (int) context->m_MaxLightCount);
            delete light;
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }

        world->m_Components.Push(light);
        *params.m_UserData = (uintptr_t) light;
        return dmGameObject::CREATE_RESULT_OK;
    }

    static void* CompLightGetComponent(const dmGameObject::ComponentGetParams& params)
    {
        return (void*) params.m_UserData;
    }

    static dmGameObject::CreateResult CompLightDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        LightWorld* world = (LightWorld*) params.m_World;
        LightContext* context = (LightContext*)params.m_Context;
        LightComponent* light = (LightComponent*) *params.m_UserData;

        for (uint32_t i = 0; i < world->m_Components.Size(); ++i)
        {
            if (world->m_Components[i] == light)
            {
                world->m_Components.EraseSwap(i);

                dmRender::DeleteLightInstance(context->m_RenderContext, light->m_LightInstance);

                delete light;
                return dmGameObject::CREATE_RESULT_OK;
            }
        }
        return dmGameObject::CREATE_RESULT_OK;
    }

    static dmGameObject::CreateResult CompLightAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params)
    {
        LightComponent* light = (LightComponent*) *params.m_UserData;
        light->m_AddedToUpdate = true;
        return dmGameObject::CREATE_RESULT_OK;
    }

    static dmGameObject::UpdateResult CompLightLateUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
    {
        LightWorld* world = (LightWorld*) params.m_World;
        LightContext* context = (LightContext*)params.m_Context;

        uint32_t num_components = world->m_Components.Size();
        for (uint32_t i = 0; i < num_components; ++i)
        {
            LightComponent* light = world->m_Components[i];
            if (!light->m_AddedToUpdate)
            {
                continue;
            }

            dmVMath::Point3 position = dmGameObject::GetPosition(light->m_Instance);
            dmVMath::Quat rotation = dmGameObject::GetRotation(light->m_Instance);
            dmVMath::Vector3 world_scale = dmGameObject::GetWorldScale(light->m_Instance);
            float scale = dmMath::Min(world_scale.getX(), dmMath::Min(world_scale.getY(), world_scale.getZ()));

            dmRender::SetLightInstance(context->m_RenderContext, light->m_LightInstance, position, rotation, scale);
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    static dmGameObject::Result CompLightTypeCreate(const dmGameObject::ComponentTypeCreateCtx* ctx, dmGameObject::ComponentType* type)
    {
        LightContext* light_context = new LightContext;
        light_context->m_Factory = ctx->m_Factory;
        light_context->m_RenderContext = *(dmRender::HRenderContext*) ctx->m_Contexts.Get(dmHashString64("render"));
        light_context->m_MaxLightCount = (uint32_t) dmMath::Max(0, dmConfigFile::GetInt(ctx->m_Config, LIGHT_MAX_COUNT_KEY, 64));

        dmRender::SetLightBufferCount(light_context->m_RenderContext, light_context->m_MaxLightCount);

        ComponentTypeSetPrio(type, 1000);

        ComponentTypeSetContext(type, light_context);
        ComponentTypeSetNewWorldFn(type, CompLightNewWorld);
        ComponentTypeSetDeleteWorldFn(type, CompLightDeleteWorld);
        ComponentTypeSetCreateFn(type, CompLightCreate);
        ComponentTypeSetDestroyFn(type, CompLightDestroy);
        ComponentTypeSetAddToUpdateFn(type, CompLightAddToUpdate);
        ComponentTypeSetLateUpdateFn(type, CompLightLateUpdate);
        ComponentTypeSetGetFn(type, CompLightGetComponent);

        return dmGameObject::RESULT_OK;
    }

    static dmGameObject::Result CompLightTypeDestroy(const dmGameObject::ComponentTypeCreateCtx* ctx, dmGameObject::ComponentType* type)
    {
        LightContext* light_context = (LightContext*)dmGameObject::ComponentTypeGetContext(type);
        if (!light_context)
        {
            return dmGameObject::RESULT_OK;
        }

        delete light_context;
        return dmGameObject::RESULT_OK;
    }
}

DM_DECLARE_COMPONENT_TYPE(ComponentTypeLight, "lightc", dmGameSystem::CompLightTypeCreate, dmGameSystem::CompLightTypeDestroy);
