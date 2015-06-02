#include "res_prototype.h"

#include <dlib/log.h>

#include "gameobject_private.h"
#include "gameobject_props.h"
#include "gameobject_props_ddf.h"

#include "../proto/gameobject_ddf.h"

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

    dmResource::Result ResPrototypePreload(dmResource::HFactory factory,
                                                dmResource::HPreloadHintInfo hint_info,
                                                void* context,
                                                const void* buffer, uint32_t buffer_size,
                                                void** preload_data,
                                                const char* filename)
    {
        dmGameObjectDDF::PrototypeDesc* proto_desc;
        dmDDF::Result e = dmDDF::LoadMessage(buffer, buffer_size, &dmGameObjectDDF_PrototypeDesc_DESCRIPTOR, (void**)(&proto_desc));

        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        for (uint32_t i = 0; i < proto_desc->m_Components.m_Count; ++i)
        {
            dmGameObjectDDF::ComponentDesc& component_desc = proto_desc->m_Components[i];
            dmResource::PreloadHint(hint_info, component_desc.m_Component);
        }

        *preload_data = proto_desc;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResPrototypeCreate(dmResource::HFactory factory,
                                                void* context,
                                                const void* buffer, uint32_t buffer_size,
                                                void* preload_data,
                                                dmResource::SResourceDescriptor* resource,
                                                const char* filename)
    {
        HRegister regist = (HRegister)context;
        dmGameObjectDDF::PrototypeDesc* proto_desc = (dmGameObjectDDF::PrototypeDesc*) preload_data;;

        Prototype* proto = new Prototype();
        proto->m_Components.SetCapacity(proto_desc->m_Components.m_Count);

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
                for (uint32_t j = 0; j < proto->m_Components.Size(); ++j)
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
                DestroyPrototype(proto, factory);
                dmDDF::FreeMessage(proto_desc);
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
                                                                              component_desc.m_Rotation);
                c.m_PropertySet.m_GetPropertyCallback = GetPropertyCallbackDDF;
                bool r = CreatePropertySetUserData(&component_desc.m_PropertyDecls, &c.m_PropertySet.m_UserData);
                proto->m_Components.Push(c);
                if (!r)
                {
                    DestroyPrototype(proto, factory);
                    dmDDF::FreeMessage(proto_desc);
                    return dmResource::RESULT_FORMAT_ERROR;
                }
            }
        }

        resource->m_Resource = (void*) proto;

        dmDDF::FreeMessage(proto_desc);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResPrototypeDestroy(dmResource::HFactory factory,
                                                 void* context,
                                                 dmResource::SResourceDescriptor* resource)
    {
        Prototype* proto = (Prototype*) resource->m_Resource;
        DestroyPrototype(proto, factory);
        return dmResource::RESULT_OK;
    }
}
