#include "res_prototype.h"

#include <dlib/log.h>

#include "gameobject_private.h"
#include "gameobject_props.h"

#include "../proto/gameobject_ddf.h"

namespace dmGameObject
{
    dmResource::Result ResPrototypeCreate(dmResource::HFactory factory,
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
            return dmResource::RESULT_FORMAT_ERROR;
        }

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
                uint32_t component_count = proto->m_Components.Size();
                for (uint32_t j = 0; j < component_count; ++j)
                {
                    dmResource::Release(factory, proto->m_Components[j].m_Resource);
                    DeleteProperties(proto->m_Components[j].m_Properties);
                }
                delete proto;
                dmDDF::FreeMessage(proto_desc);
                if (id_used)
                    return dmResource::RESULT_FORMAT_ERROR;
                else
                    return fact_e;
            }
            else
            {
                uint32_t resource_type;
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
                c.m_Properties = NewProperties();
                uint32_t prop_count = component_desc.m_Properties.m_Count;
                if (prop_count > 0)
                {
                    const uint32_t buffer_size = 1024;
                    uint8_t buffer[buffer_size];
                    uint32_t actual = SerializeProperties(component_desc.m_Properties.m_Data, prop_count, buffer, buffer_size);
                    if (buffer_size < actual)
                    {
                        dmLogError("Properties could not be stored when loading %s: too many properties.", filename);
                    }
                    else
                    {
                        SetProperties(c.m_Properties, buffer, actual);
                    }
                }
                proto->m_Components.Push(c);

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
        for (uint32_t i = 0; i < proto->m_Components.Size(); ++i)
        {
            dmResource::Release(factory, proto->m_Components[i].m_Resource);
            DeleteProperties(proto->m_Components[i].m_Properties);
        }

        delete proto;
        return dmResource::RESULT_OK;
    }
}
