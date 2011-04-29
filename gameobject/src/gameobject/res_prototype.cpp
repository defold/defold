#include "res_prototype.h"

#include <dlib/log.h>

#include "gameobject_private.h"

#include "../proto/gameobject_ddf.h"

namespace dmGameObject
{
    dmResource::CreateResult ResPrototypeCreate(dmResource::HFactory factory,
                                                void* context,
                                                const void* buffer, uint32_t buffer_size,
                                                dmResource::SResourceDescriptor* resource,
                                                const char* filename)
    {
        HRegister regist = (HRegister)context;
        dmGameObjectDDF::PrototypeDesc* proto_desc;

        dmDDF::Result e = dmDDF::LoadMessage(buffer, buffer_size, &dmGameObjectDDF_PrototypeDesc_DESCRIPTOR, (void**)(&proto_desc));
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }

        Prototype* proto = new Prototype();
        proto->m_Components.SetCapacity(proto_desc->m_Components.m_Count);

        for (uint32_t i = 0; i < proto_desc->m_Components.m_Count; ++i)
        {
            const char* component_resource = proto_desc->m_Components[i].m_Component;
            void* component;
            dmResource::FactoryResult fact_e = dmResource::Get(factory, component_resource, (void**) &component);

            bool id_used = false;
            dmhash_t id = 0;
            if (fact_e == dmResource::FACTORY_RESULT_OK)
            {
                id = dmHashString64(proto_desc->m_Components[i].m_Id);
                for (uint32_t j = 0; j < proto->m_Components.Size(); ++j)
                {
                    if (proto->m_Components[j].m_Id == id)
                    {
                        dmLogError("The id '%s' has already been used in the prototype %s.", proto_desc->m_Components[i].m_Id, filename);
                        id_used = true;
                    }
                }
            }

            if (id_used || fact_e != dmResource::FACTORY_RESULT_OK)
            {
                // Error, release created
                if (id_used)
                    dmResource::Release(factory, component);
                for (uint32_t j = 0; j < proto->m_Components.Size(); ++j)
                {
                    dmResource::Release(factory, proto->m_Components[j].m_Resource);
                }
                delete proto;
                dmDDF::FreeMessage(proto_desc);
                return dmResource::CREATE_RESULT_UNKNOWN;
            }
            else
            {
                uint32_t resource_type;
                fact_e = dmResource::GetType(factory, component, &resource_type);
                assert(fact_e == dmResource::FACTORY_RESULT_OK);
                uint32_t type_index;
                ComponentType* type = FindComponentType(regist, resource_type, &type_index);
                assert(type != 0x0);
                dmResource::SResourceDescriptor descriptor;
                fact_e = dmResource::GetDescriptor(factory, component_resource, &descriptor);
                assert(fact_e == dmResource::FACTORY_RESULT_OK);

                proto->m_Components.Push(Prototype::Component(component, resource_type, id, descriptor.m_NameHash, type, type_index));
            }
        }

        resource->m_Resource = (void*) proto;

        dmDDF::FreeMessage(proto_desc);
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResPrototypeDestroy(dmResource::HFactory factory,
                                                 void* context,
                                                 dmResource::SResourceDescriptor* resource)
    {
        Prototype* proto = (Prototype*) resource->m_Resource;
        for (uint32_t i = 0; i < proto->m_Components.Size(); ++i)
        {
            dmResource::Release(factory, proto->m_Components[i].m_Resource);
        }

        delete proto;
        return dmResource::CREATE_RESULT_OK;
    }
}
