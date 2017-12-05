#include "res_prototype.h"

#include <dlib/log.h>

#include "gameobject_private.h"
#include "gameobject_props.h"
#include "gameobject_props_ddf.h"

#include "../proto/gameobject/gameobject_ddf.h"

namespace dmGameObject
{
    static void DestroyPrototype(Prototype* prototype, dmResource::HFactory factory)
    {
        for (uint32_t i = 0; i < prototype->m_Components.Size(); ++i)
        {
            Prototype::Component& c = prototype->m_Components[i];
            dmResource::Release(factory, c.m_Resource);
            DestroyPropertySetUserData(c.m_PropertySet.m_UserData);
        }

        delete prototype;
    }

    dmResource::Result ResPrototypePreload(const dmResource::ResourcePreloadParams& params)
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

        *params.m_PreloadData = proto_desc;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResPrototypeCreate(const dmResource::ResourceCreateParams& params)
    {
        HRegister regist = (HRegister) params.m_Context;
        dmGameObjectDDF::PrototypeDesc* proto_desc = (dmGameObjectDDF::PrototypeDesc*) params.m_PreloadData;

        Prototype* proto = new Prototype();
        proto->m_Components.SetCapacity(proto_desc->m_Components.m_Count);

        for (uint32_t i = 0; i < proto_desc->m_Components.m_Count; ++i)
        {
            dmGameObjectDDF::ComponentDesc& component_desc = proto_desc->m_Components[i];
            const char* component_resource = component_desc.m_Component;
            void* component;
            dmResource::Result fact_e = dmResource::Get(params.m_Factory, component_resource, (void**) &component);

            bool id_used = false;
            dmhash_t id = 0;
            if (fact_e == dmResource::RESULT_OK)
            {
                id = dmHashString64(component_desc.m_Id);
                for (uint32_t j = 0; j < proto->m_Components.Size(); ++j)
                {
                    if (proto->m_Components[j].m_Id == id)
                    {
                        dmLogError("The id '%s' has already been used in the prototype %s.", component_desc.m_Id, params.m_Filename);
                        id_used = true;
                    }
                }
            }

            if (id_used || fact_e != dmResource::RESULT_OK)
            {
                // Error, release created
                if (id_used)
                    dmResource::Release(params.m_Factory, component);
                DestroyPrototype(proto, params.m_Factory);
                dmDDF::FreeMessage(proto_desc);
                if (id_used)
                    return dmResource::RESULT_FORMAT_ERROR;
                else
                    return fact_e;
            }
            else
            {
                dmResource::ResourceType resource_type;
                fact_e = dmResource::GetType(params.m_Factory, component, &resource_type);
                assert(fact_e == dmResource::RESULT_OK);
                uint32_t type_index;
                ComponentType* type = FindComponentType(regist, resource_type, &type_index);
                assert(type != 0x0);
                dmResource::SResourceDescriptor descriptor;
                fact_e = dmResource::GetDescriptor(params.m_Factory, component_resource, &descriptor);
                assert(fact_e == dmResource::RESULT_OK);

                Prototype::Component c(component,
                                                                              resource_type,
                                                                              id,
                                                                              descriptor.m_NameHash,
                                                                              type,
                                                                              type_index,
                                                                              component_desc.m_Position,
                                                                              component_desc.m_Rotation);
                c.m_PropertySet.m_GetPropertyCallback = GetPropertyCallbackDDF;
                bool r = CreatePropertySetUserData(&component_desc.m_PropertyDecls, &c.m_PropertySet.m_UserData);
                proto->m_Components.Push(c);
                if (!r)
                {
                    DestroyPrototype(proto, params.m_Factory);
                    dmDDF::FreeMessage(proto_desc);
                    return dmResource::RESULT_FORMAT_ERROR;
                }
            }
        }

        params.m_Resource->m_Resource = (void*) proto;

        dmDDF::FreeMessage(proto_desc);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResPrototypeDestroy(const dmResource::ResourceDestroyParams& params)
    {
        Prototype* proto = (Prototype*) params.m_Resource->m_Resource;
        DestroyPrototype(proto, params.m_Factory);
        return dmResource::RESULT_OK;
    }
}
