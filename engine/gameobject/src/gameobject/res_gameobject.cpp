// Copyright 2020-2022 The Defold Foundation
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

//#include "res_gameobject.h"

#include <dlib/log.h>

#include <dmsdk/resource/resource.h>
#include "gameobject_private.h"
#include "gameobject_props.h"
#include "gameobject_props_ddf.h"

#include "../proto/gameobject/gameobject_ddf.h"

namespace dmGameObject
{
    static void ReleaseResources(dmResource::HFactory factory, Prototype* prototype)
    {
        for (uint32_t i = 0; i < prototype->m_ComponentCount; ++i)
        {
            Prototype::Component& c = prototype->m_Components[i];
            dmResource::Release(factory, c.m_Resource);
            DestroyPropertyContainerCallback(c.m_PropertySet.m_UserData);
        }

        UnloadPropertyResources(factory, prototype->m_PropertyResources);
    }

    static dmResource::Result AcquireResources(dmResource::HFactory factory, dmGameObject::HRegister regist, dmGameObjectDDF::PrototypeDesc* proto_desc, Prototype* proto, const char* filename)
    {
        dmResource::Result res = LoadPropertyResources(factory, proto_desc->m_PropertyResources.m_Data, proto_desc->m_PropertyResources.m_Count, proto->m_PropertyResources);
        if(res != dmResource::RESULT_OK)
        {
            ReleaseResources(factory, proto);
            dmDDF::FreeMessage(proto_desc);
            return res;
        }

        proto->m_ComponentCount = 0;
        proto->m_Components = 0;
        if (proto_desc->m_Components.m_Count == 0)
        {
            return dmResource::RESULT_OK;
        }

        proto->m_Components = (Prototype::Component*)malloc(sizeof(Prototype::Component) * proto_desc->m_Components.m_Count);

        for (uint32_t i = 0; i < proto_desc->m_Components.m_Count; ++i)
        {
            dmGameObjectDDF::ComponentDesc& component_desc = proto_desc->m_Components[i];
            const char* component_resource = component_desc.m_Component;
            void* component;
            dmResource::Result fact_e = dmResource::Get(factory, component_resource, (void**) &component);

            bool id_used = false;
            dmhash_t id = 0;
            if (fact_e == dmResource::RESULT_OK)
            {
                id = dmHashString64(component_desc.m_Id);
                for (uint32_t j = 0; j < proto->m_ComponentCount; ++j)
                {
                    if (proto->m_Components[j].m_Id == id)
                    {
                        dmLogError("The id '%s' has already been used in the prototype %s.", component_desc.m_Id, filename);
                        id_used = true;
                    }
                }
            }

            if (id_used || fact_e != dmResource::RESULT_OK)
            {
                // Error, release created
                if (id_used)
                    dmResource::Release(factory, component);
                if (id_used)
                    return dmResource::RESULT_FORMAT_ERROR;
                else
                    return fact_e;
            }
            else
            {
                dmResource::ResourceType resource_type;
                fact_e = dmResource::GetType(factory, component, &resource_type);
                assert(fact_e == dmResource::RESULT_OK);
                uint32_t type_index;
                ComponentType* type = FindComponentType(regist, resource_type, &type_index);
                if (!type) {
                    dmLogError("Failed to find component type for '%s'/'%s'", component_desc.m_Id, component_desc.m_Component);
                }
                assert(type != 0x0);
                dmResource::SResourceDescriptor descriptor;
                fact_e = dmResource::GetDescriptor(factory, component_resource, &descriptor);
                assert(fact_e == dmResource::RESULT_OK);

                Prototype::Component c(component,
                                                                              resource_type,
                                                                              id,
                                                                              descriptor.m_NameHash,
                                                                              type,
                                                                              type_index,
                                                                              component_desc.m_Position,
                                                                              component_desc.m_Rotation,
                                                                              component_desc.m_Scale);
                c.m_PropertySet.m_GetPropertyCallback = PropertyContainerGetPropertyCallback;

                c.m_PropertySet.m_UserData = (uintptr_t)CreatePropertyContainerFromDDF(&component_desc.m_PropertyDecls);
                if (c.m_PropertySet.m_UserData == 0)
                {
                    return dmResource::RESULT_FORMAT_ERROR;
                }
                proto->m_Components[proto->m_ComponentCount++] = c;
            }
        }
        return dmResource::RESULT_OK;
    }

    static dmResource::Result ResGameObjectPreload(const dmResource::ResourcePreloadParams& params)
    {
        dmGameObjectDDF::PrototypeDesc* proto_desc;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmGameObjectDDF_PrototypeDesc_DESCRIPTOR, (void**)(&proto_desc));

        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        for (uint32_t i = 0; i < proto_desc->m_Components.m_Count; ++i)
        {
            dmGameObjectDDF::ComponentDesc& component_desc = proto_desc->m_Components[i];
            dmResource::PreloadHint(params.m_HintInfo, component_desc.m_Component);
        }

        dmGameObjectDDF::ComponentDesc* components = proto_desc->m_Components.m_Data;
        uint32_t n_components = proto_desc->m_Components.m_Count;
        for (uint32_t i = 0; i < n_components; ++i)
        {
            dmResource::PreloadHint(params.m_HintInfo, components[i].m_Component);
        }
        const char** resources = proto_desc->m_PropertyResources.m_Data;
        uint32_t n_resources = proto_desc->m_PropertyResources.m_Count;
        for (uint32_t i = 0; i < n_resources; ++i)
        {
            dmResource::PreloadHint(params.m_HintInfo, resources[i]);
        }

        *params.m_PreloadData = proto_desc;
        return dmResource::RESULT_OK;
    }

    static dmResource::Result ResGameObjectCreate(const dmResource::ResourceCreateParams& params)
    {
        HRegister regist = (HRegister) params.m_Context;
        dmGameObjectDDF::PrototypeDesc* proto_desc = (dmGameObjectDDF::PrototypeDesc*) params.m_PreloadData;

        Prototype* proto = new Prototype();
        dmResource::Result r = AcquireResources(params.m_Factory, regist, proto_desc, proto, params.m_Filename);
        if (r == dmResource::RESULT_OK) {
            params.m_Resource->m_Resource = (void*) proto;
        } else {
            ReleaseResources(params.m_Factory, proto);
            delete proto;
        }

        dmDDF::FreeMessage(proto_desc);
        return r;
    }

    static dmResource::Result ResGameObjectDestroy(const dmResource::ResourceDestroyParams& params)
    {
        Prototype* proto = (Prototype*) params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, proto);
        delete proto;
        return dmResource::RESULT_OK;
    }

    static dmResource::Result ResGameObjectRecreate(const dmResource::ResourceRecreateParams& params)
    {
        Register* regist = (Register*) params.m_Context;
        dmGameObjectDDF::PrototypeDesc* proto_desc;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &proto_desc);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }
        Prototype* temp = new Prototype();
        dmResource::Result r = AcquireResources(params.m_Factory, regist, proto_desc, temp, params.m_Filename);
        if (dmResource::RESULT_OK == r) {
            Prototype* proto = (Prototype*) params.m_Resource->m_Resource;
            Prototype::Component* c = proto->m_Components;
            uint32_t i = proto->m_ComponentCount;
            proto->m_Components = temp->m_Components;
            proto->m_ComponentCount = temp->m_ComponentCount;
            temp->m_Components = c;
            temp->m_ComponentCount = i;
            params.m_Resource->m_PrevResource = temp;
        } else {
            ReleaseResources(params.m_Factory, temp);
            delete temp;
        }
        dmDDF::FreeMessage(proto_desc);
        return r;
    }

    static dmResource::Result RegisterResourceTypeGameObject(dmResource::ResourceTypeRegisterContext& ctx)
    {
        // The engine.cpp creates the contexts for our built in types.
        void** context = ctx.m_Contexts->Get(ctx.m_NameHash);
        assert(context);
        return dmResource::RegisterType(ctx.m_Factory,
                                           ctx.m_Name,
                                           *context,
                                           ResGameObjectPreload,
                                           ResGameObjectCreate,
                                           0,
                                           ResGameObjectDestroy,
                                           ResGameObjectRecreate);
    }
}

DM_DECLARE_RESOURCE_TYPE(ResourceTypeGameObject, "goc", dmGameObject::RegisterResourceTypeGameObject, 0);
