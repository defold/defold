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
#include <dmsdk/render/render.h>
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
        uint32_t                 m_MaxComponentInstances;
    };

    static const char* LightTypeToStr(dmRender::LightType type)
    {
        switch (type)
        {
            case dmRender::LIGHT_TYPE_DIRECTIONAL: return "directional";
            case dmRender::LIGHT_TYPE_POINT:       return "point";
            case dmRender::LIGHT_TYPE_SPOT:        return "spot";
            case dmRender::LIGHT_TYPE_AMBIENT:     return "ambient";
            default:                               return "<unknown>";
        }
    }

    static dmGameObject::CreateResult CompLightNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        LightContext* context = (LightContext*) params.m_Context;
        LightWorld* world = new LightWorld;
        uint32_t comp_count = dmMath::Min(params.m_MaxComponentInstances, context->m_MaxLightCount);
        world->m_Components.SetCapacity(comp_count);
        world->m_MaxComponentInstances = params.m_MaxComponentInstances;
        *params.m_World = world;
        dmLogInfo("TEMP LIGHT CompLightNewWorld context=%p render_context=%p max_light_count=%u max_component_instances=%u world=%p component_capacity=%u",
                  (void*) context,
                  (void*) context->m_RenderContext,
                  context->m_MaxLightCount,
                  params.m_MaxComponentInstances,
                  (void*) world,
                  world->m_Components.Capacity());
        return dmGameObject::CREATE_RESULT_OK;
    }

    static dmRender::LightType LightType(LightContext* context, LightResource* light_resource)
    {
        return dmRender::GetLightType(context->m_RenderContext, GetLightPrototype(light_resource));
    }

    static dmVMath::Vector3 AmbientContribution(LightContext* context, LightResource* light_resource)
    {
        dmRender::HLightPrototype prototype = GetLightPrototype(light_resource);
        if (dmRender::GetLightType(context->m_RenderContext, prototype) != dmRender::LIGHT_TYPE_AMBIENT)
        {
            return dmVMath::Vector3(0.0f, 0.0f, 0.0f);
        }

        dmVMath::Vector4 color = dmRender::GetLightColor(context->m_RenderContext, prototype);
        return dmVMath::Vector3(color.getX(), color.getY(), color.getZ()) * dmRender::GetLightIntensity(context->m_RenderContext, prototype);
    }

    static bool CreateRenderLightInstance(LightContext* context, LightComponent* light)
    {
        dmRender::HLightPrototype prototype = GetLightPrototype(light->m_LightResource);
        dmLogInfo("TEMP LIGHT CreateRenderLightInstance begin context=%p render_context=%p component=%p resource=%p prototype=%u max_light_count=%u",
                  (void*) context,
                  (void*) context->m_RenderContext,
                  (void*) light,
                  (void*) light->m_LightResource,
                  (uint32_t) prototype,
                  context->m_MaxLightCount);
        light->m_LightInstance = dmRender::NewLightInstance(context->m_RenderContext, prototype);
        if (light->m_LightInstance == 0)
        {
            dmLogError("TEMP LIGHT CreateRenderLightInstance failed context=%p render_context=%p component=%p resource=%p prototype=%u max_light_count=%u",
                       (void*) context,
                       (void*) context->m_RenderContext,
                       (void*) light,
                       (void*) light->m_LightResource,
                       (uint32_t) prototype,
                       context->m_MaxLightCount);
            ShowFullBufferError("Light", LIGHT_MAX_COUNT_KEY, (int) context->m_MaxLightCount);
            return false;
        }
        dmLogInfo("TEMP LIGHT CreateRenderLightInstance success context=%p render_context=%p component=%p resource=%p prototype=%u instance=%u",
                  (void*) context,
                  (void*) context->m_RenderContext,
                  (void*) light,
                  (void*) light->m_LightResource,
                  (uint32_t) prototype,
                  (uint32_t) light->m_LightInstance);
        return true;
    }

    static bool EnsureComponentCapacity(LightWorld* world, bool is_ambient)
    {
        if (!world->m_Components.Full())
        {
            return true;
        }

        if (!is_ambient || world->m_Components.Capacity() >= world->m_MaxComponentInstances)
        {
            return false;
        }

        // Ambient lights can grow the component array up to the normal max
        // component instance limit because they do not consume render light slots.
        uint32_t remaining = world->m_MaxComponentInstances - world->m_Components.Capacity();
        world->m_Components.OffsetCapacity((int32_t) dmMath::Min(16U, remaining));
        return true;
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
        LightResource* light_resource = (LightResource*) params.m_Resource;
        dmRender::HLightPrototype prototype = GetLightPrototype(light_resource);
        dmRender::LightType light_type = LightType(context, light_resource);
        bool is_ambient = light_type == dmRender::LIGHT_TYPE_AMBIENT;

        dmLogInfo("TEMP LIGHT CompLightCreate begin context=%p render_context=%p world=%p world_size=%u world_capacity=%u resource=%p prototype=%u type=%s instance=%p user_data=%p",
                  (void*) context,
                  (void*) context->m_RenderContext,
                  (void*) world,
                  world->m_Components.Size(),
                  world->m_Components.Capacity(),
                  (void*) light_resource,
                  (uint32_t) prototype,
                  LightTypeToStr(light_type),
                  (void*) params.m_Instance,
                  (void*) params.m_UserData);

        if (!EnsureComponentCapacity(world, is_ambient))
        {
            dmLogError("TEMP LIGHT CompLightCreate capacity failed world=%p world_size=%u world_capacity=%u max_component_instances=%u type=%s max_light_count=%u",
                       (void*) world,
                       world->m_Components.Size(),
                       world->m_Components.Capacity(),
                       world->m_MaxComponentInstances,
                       LightTypeToStr(light_type),
                       context->m_MaxLightCount);
            if (is_ambient)
            {
                ShowFullBufferError("Light", world->m_Components.Capacity());
            }
            else
            {
                ShowFullBufferError("Light", LIGHT_MAX_COUNT_KEY, (int) context->m_MaxLightCount);
            }
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }

        LightComponent* light  = new LightComponent;
        memset(light, 0, sizeof(LightComponent));

        light->m_Instance      = params.m_Instance;
        light->m_LightResource = light_resource;

        if (!is_ambient && !CreateRenderLightInstance(context, light))
        {
            delete light;
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }

        world->m_Components.Push(light);
        *params.m_UserData = (uintptr_t) light;
        dmLogInfo("TEMP LIGHT CompLightCreate success context=%p render_context=%p world=%p world_size=%u component=%p light_instance=%u resource=%p prototype=%u type=%s",
                  (void*) context,
                  (void*) context->m_RenderContext,
                  (void*) world,
                  world->m_Components.Size(),
                  (void*) light,
                  (uint32_t) light->m_LightInstance,
                  (void*) light->m_LightResource,
                  (uint32_t) prototype,
                  LightTypeToStr(light_type));
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

                if (light->m_LightInstance)
                {
                    dmLogInfo("TEMP LIGHT CompLightDestroy delete render instance context=%p render_context=%p component=%p resource=%p prototype=%u light_instance=%u",
                              (void*) context,
                              (void*) context->m_RenderContext,
                              (void*) light,
                              (void*) light->m_LightResource,
                              (uint32_t) GetLightPrototype(light->m_LightResource),
                              (uint32_t) light->m_LightInstance);
                    dmRender::DeleteLightInstance(context->m_RenderContext, light->m_LightInstance);
                }

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
        dmVMath::Vector3 ambient_light(0.0f, 0.0f, 0.0f);
        for (uint32_t i = 0; i < num_components; ++i)
        {
            LightComponent* light = world->m_Components[i];
            if (!light->m_AddedToUpdate)
            {
                continue;
            }

            bool is_ambient = LightType(context, light->m_LightResource) == dmRender::LIGHT_TYPE_AMBIENT;
            if (is_ambient)
            {
                if (light->m_LightInstance)
                {
                    // The resource may have been reloaded from a buffered light type
                    // to ambient. Ambient lights are accumulated instead of instanced.
                    dmRender::DeleteLightInstance(context->m_RenderContext, light->m_LightInstance);
                    light->m_LightInstance = 0;
                }
                ambient_light += AmbientContribution(context, light->m_LightResource);
                continue;
            }

            if (light->m_LightInstance == 0 && !CreateRenderLightInstance(context, light))
            {
                dmLogError("TEMP LIGHT CompLightLateUpdate retry allocation failed context=%p render_context=%p component=%p resource=%p prototype=%u",
                           (void*) context,
                           (void*) context->m_RenderContext,
                           (void*) light,
                           (void*) light->m_LightResource,
                           (uint32_t) GetLightPrototype(light->m_LightResource));
                continue;
            }

            dmVMath::Point3 position = dmGameObject::GetWorldPosition(light->m_Instance);
            dmVMath::Quat rotation = dmGameObject::GetWorldRotation(light->m_Instance);
            dmVMath::Vector3 world_scale = dmGameObject::GetWorldScale(light->m_Instance);
            float scale_x = dmMath::Abs(world_scale.getX());
            float scale_y = dmMath::Abs(world_scale.getY());
            float scale_z = dmMath::Abs(world_scale.getZ());
            float scale = dmMath::Min(scale_x, dmMath::Min(scale_y, scale_z));

            dmRender::SetLightInstance(context->m_RenderContext, light->m_LightInstance, position, rotation, scale);
        }
        dmRender::SetAmbientLight(context->m_RenderContext, ambient_light);
        return dmGameObject::UPDATE_RESULT_OK;
    }

    static dmGameObject::Result CompLightTypeCreate(const dmGameObject::ComponentTypeCreateCtx* ctx, dmGameObject::ComponentType* type)
    {
        LightContext* light_context = new LightContext;
        HContextRegistry context_registry = dmGameObject::ComponentGetContextRegistry(ctx);
        light_context->m_Factory = ctx->m_Factory;
        light_context->m_RenderContext = (dmRender::HRenderContext) ContextRegistryGet(context_registry, RENDER_CONTEXT_NAME);
        light_context->m_MaxLightCount = (uint32_t) dmMath::Max(0, dmConfigFile::GetInt(ctx->m_Config, LIGHT_MAX_COUNT_KEY, 64));

        dmLogInfo("TEMP LIGHT CompLightTypeCreate context=%p context_registry=%p render_context=%p factory=%p max_light_count=%u",
                  (void*) light_context,
                  (void*) context_registry,
                  (void*) light_context->m_RenderContext,
                  (void*) light_context->m_Factory,
                  light_context->m_MaxLightCount);

        dmRender::SetLightBufferCount(light_context->m_RenderContext, light_context->m_MaxLightCount);

        ComponentTypeSetPrio(type, 1000);
        ComponentTypeSetReadsTransforms(type, true);

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

        dmLogInfo("TEMP LIGHT CompLightTypeDestroy context=%p render_context=%p max_light_count=%u",
                  (void*) light_context,
                  (void*) light_context->m_RenderContext,
                  light_context->m_MaxLightCount);
        delete light_context;
        return dmGameObject::RESULT_OK;
    }
}

DM_DECLARE_COMPONENT_TYPE(ComponentTypeLight, "lightc", dmGameSystem::CompLightTypeCreate, dmGameSystem::CompLightTypeDestroy);
