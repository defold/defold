#include <Python.h>

#include <dlib/log.h>
#include <dlib/hashtable.h>
#include "gameobject.h"
#include "gameobject_ddf.h"
#include "gameobject_script.h"
#include "gameobject_common.h"

namespace dmGameObject
{
    struct ComponentType
    {
        ComponentType()
        {
            memset(this, 0, sizeof(*this));
        }

        uint32_t         m_ResourceType;
        void*            m_Context;
        ComponentCreate  m_CreateFunction;
        ComponentDestroy m_DestroyFunction;
        ComponentsUpdate m_ComponentsUpdate;
        uint32_t         m_ComponentInstanceHasUserdata : 1;
    };

    const uint32_t MAX_COMPONENT_TYPES = 128;
    struct Collection
    {
        Collection() {m_ComponentTypeCount = 0;}
        uint32_t               m_ComponentTypeCount;
        ComponentType          m_ComponentTypes[MAX_COMPONENT_TYPES];
        std::vector<Instance*> m_Instances;
    };

    void Initialize()
    {
        InitializeScript();
    }

    void Finalize()
    {
        FinalizeScript();
    }

    HCollection  NewCollection()
    {
        return new Collection();
    }

    void DeleteCollection(HCollection collection, dmResource::HFactory factory)
    {
        for (uint32_t i = 0; collection->m_Instances.size(); ++i)
        {
            Delete(collection, factory, collection->m_Instances[i]);
        }
        delete collection;
    }

    static ComponentType* FindComponentType(Collection* collection, uint32_t resource_type)
    {
        for (uint32_t i = 0; i < collection->m_ComponentTypeCount; ++i)
        {
            ComponentType* ct = &collection->m_ComponentTypes[i];
            if (ct->m_ResourceType == resource_type)
            {
                return ct;
            }
        }
        return 0;
    }

    Result RegisterComponentType(HCollection collection,
                                 uint32_t resource_type,
                                 void* context,
                                 ComponentCreate create_function,
                                 ComponentDestroy destroy_function,
                                 ComponentsUpdate components_update,
                                 bool component_instance_has_user_data)
    {
        if (collection->m_ComponentTypeCount == MAX_COMPONENT_TYPES)
            return RESULT_OUT_OF_RESOURCES;

        if (FindComponentType(collection, resource_type) != 0)
            return RESULT_ALREADY_REGISTERED;

        ComponentType component_type;
        component_type.m_ResourceType = resource_type;
        component_type.m_Context = context;
        component_type.m_CreateFunction = create_function;
        component_type.m_DestroyFunction = destroy_function;
        component_type.m_ComponentsUpdate = components_update;
        component_type.m_ComponentInstanceHasUserdata = (uint32_t) component_instance_has_user_data;

        collection->m_ComponentTypes[collection->m_ComponentTypeCount++] = component_type;

        return RESULT_OK;
    }

    dmResource::CreateResult PrototypeCreate(dmResource::HFactory factory,
                                             void* context,
                                             const void* buffer, uint32_t buffer_size,
                                             dmResource::SResourceDescriptor* resource)
    {
        GameObjectPrototypeDesc* proto_desc;

        dmDDF::Result e = dmDDF::LoadMessage(buffer, buffer_size, &dmGameObject_GameObjectPrototypeDesc_DESCRIPTOR, (void**)(&proto_desc));
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }

        Prototype* proto = new Prototype();
        proto->m_Components.reserve(proto_desc->m_Components.m_Count);

        for (uint32_t i = 0; i < proto_desc->m_Components.m_Count; ++i)
        {
            const char* component_name = proto_desc->m_Components[i];
            void* component;
            dmResource::FactoryResult fact_e = dmResource::Get(factory, component_name, (void**) &component);

            if (fact_e != dmResource::FACTORY_RESULT_OK)
            {
                // Error, release created
                for (uint32_t j = 0; j < proto->m_Components.size(); ++j)
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
                proto->m_Components.push_back(Prototype::Component(component, resource_type));
            }
        }

        HScript script;
        dmResource::FactoryResult fact_e = dmResource::Get(factory, proto_desc->m_Script, (void**) &script);
        if (fact_e != dmResource::FACTORY_RESULT_OK)
        {
            dmDDF::FreeMessage(proto_desc);
            return dmResource::CREATE_RESULT_UNKNOWN;
        }

        proto->m_Name = strdup(proto_desc->m_Name);
        proto->m_Script = script;

        resource->m_Resource = (void*) proto;

        dmDDF::FreeMessage(proto_desc);
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult PrototypeDestroy(dmResource::HFactory factory,
                                              void* context,
                                              dmResource::SResourceDescriptor* resource)
    {
        Prototype* proto = (Prototype*) resource->m_Resource;
        for (uint32_t i = 0; i < proto->m_Components.size(); ++i)
        {
            dmResource::Release(factory, proto->m_Components[i].m_Resource);
        }

        free((void*) proto->m_Name);
        dmResource::Release(factory, proto->m_Script);
        delete proto;
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ScriptCreate(dmResource::HFactory factory,
                                          void* context,
                                          const void* buffer, uint32_t buffer_size,
                                          dmResource::SResourceDescriptor* resource)
    {
        HScript script = NewScript(buffer);
        if (script)
        {
            resource->m_Resource = (void*) script;
            return dmResource::CREATE_RESULT_OK;
        }
        else
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
    }

    dmResource::CreateResult ScriptDestroy(dmResource::HFactory factory,
                                           void* context,
                                           dmResource::SResourceDescriptor* resource)
    {

        DeleteScript((HScript) resource->m_Resource);
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::FactoryResult RegisterResourceTypes(dmResource::HFactory factory)
    {
        dmResource::FactoryResult ret;
        ret = dmResource::RegisterType(factory, "go", 0, &PrototypeCreate, &PrototypeDestroy);
        if (ret != dmResource::FACTORY_RESULT_OK)
            return ret;

        ret = dmResource::RegisterType(factory, "scriptc", 0, &ScriptCreate, &ScriptDestroy);
        if (ret != dmResource::FACTORY_RESULT_OK)
            return ret;

        return ret;
    }

    HInstance New(HCollection collection, dmResource::HFactory factory, const char* prototype_name)
    {
        Prototype* proto;
        dmResource::FactoryResult error = dmResource::Get(factory, prototype_name, (void**)&proto);
        if (error != dmResource::FACTORY_RESULT_OK)
        {
            return 0;
        }

        // Count number of component userdata fields required
        uint32_t component_instance_userdata_count = 0;
        for (uint32_t i = 0; i < proto->m_Components.size(); ++i)
        {
            Prototype::Component* component = &proto->m_Components[i];
            ComponentType* component_type = FindComponentType(collection, component->m_ResourceType);
            if (!component_type)
            {
                dmLogError("Internal error. Component type #%d for '%s' not found.", i, prototype_name);
                assert(false);
            }
            if (component_type->m_ComponentInstanceHasUserdata)
                component_instance_userdata_count++;
        }

        uint32_t component_userdata_size = sizeof(((Instance*)0)->m_ComponentInstanceUserData[0]);
        // NOTE: Allocate actual Instance with *all* component instance user-data accounted
        void* instance_memory = ::operator new (sizeof(Instance) + component_instance_userdata_count * component_userdata_size);
        Instance* instance = new(instance_memory) Instance(proto);
        instance->m_ComponentInstanceUserDataCount = component_instance_userdata_count;

        uint32_t components_created = 0;
        uint32_t next_component_instance_data = 0;
        bool ok = true;
        for (uint32_t i = 0; i < proto->m_Components.size(); ++i)
        {
            Prototype::Component* component = &proto->m_Components[i];
            ComponentType* component_type = FindComponentType(collection, component->m_ResourceType);
            assert(component_type);

            uintptr_t* component_instance_data = 0;
            if (component_type->m_ComponentInstanceHasUserdata)
            {
                component_instance_data = &instance->m_ComponentInstanceUserData[next_component_instance_data++];
                *component_instance_data = 0;
            }
            assert(next_component_instance_data <= instance->m_ComponentInstanceUserDataCount);

            CreateResult create_result =  component_type->m_CreateFunction(collection, instance, component->m_Resource, component_type->m_Context, component_instance_data);
            if (create_result == CREATE_RESULT_OK)
            {
                components_created++;
            }
            else
            {
                ok = false;
                break;
            }
        }

        if (!ok)
        {
            uint32_t next_component_instance_data = 0;
            for (uint32_t i = 0; i < components_created; ++i)
            {
                Prototype::Component* component = &proto->m_Components[i];
                ComponentType* component_type = FindComponentType(collection, component->m_ResourceType);
                assert(component_type);

                uintptr_t* component_instance_data = 0;
                if (component_type->m_ComponentInstanceHasUserdata)
                {
                    component_instance_data = &instance->m_ComponentInstanceUserData[next_component_instance_data++];
                }
                assert(next_component_instance_data <= instance->m_ComponentInstanceUserDataCount);

                component_type->m_DestroyFunction(collection, instance, component_type->m_Context, component_instance_data);
            }

            // We can not call Delete here. Delete call DestroyFunction for every component
            dmResource::Release(factory, instance->m_Prototype);
            operator delete (instance_memory);
            return 0;
        }

        PyObject* lst = PyTuple_New(1);
        Py_INCREF(instance->m_Self); //NOTE: PyTuple_SetItem steals a reference
        PyTuple_SetItem(lst, 0, instance->m_Self);
        bool init_ok = RunScript(proto->m_Script, "Init", instance->m_Self, lst);
        Py_DECREF(lst);

        if (init_ok)
        {
            collection->m_Instances.push_back(instance);
            return instance;
        }
        else
        {
            Delete(collection, factory, instance);
            return 0;
        }
    }

    void Delete(HCollection collection, dmResource::HFactory factory, HInstance instance)
    {
        uint32_t next_component_instance_data = 0;
        Prototype* prototype = instance->m_Prototype;
        for (uint32_t i = 0; i < prototype->m_Components.size(); ++i)
        {
            Prototype::Component* component = &prototype->m_Components[i];
            ComponentType* component_type = FindComponentType(collection, component->m_ResourceType);
            assert(component_type);

            uintptr_t* component_instance_data = 0;
            if (component_type->m_ComponentInstanceHasUserdata)
            {
                component_instance_data = &instance->m_ComponentInstanceUserData[next_component_instance_data++];
            }
            assert(next_component_instance_data <= instance->m_ComponentInstanceUserDataCount);

            component_type->m_DestroyFunction(collection, instance, component_type->m_Context, component_instance_data);
        }

        // TODO: O(n)...
        bool found = false;
        for (uint32_t i = 0; collection->m_Instances.size(); ++i)
        {
            if (collection->m_Instances[i] == instance)
            {
                collection->m_Instances.erase(collection->m_Instances.begin() + i);
                found = true;
                break;
            }
        }
        assert(found);

        dmResource::Release(factory, instance->m_Prototype);
        void* instance_memory = (void*) instance;
        operator delete (instance_memory);
    }

    bool Update(HCollection collection, HInstance instance)
    {
        Prototype* proto = instance->m_Prototype;
        PyObject* lst = PyTuple_New(1);
        Py_INCREF(instance->m_Self); //NOTE: PyTuple_SetItem steals a reference
        PyTuple_SetItem(lst, 0, instance->m_Self);
        bool ret = RunScript(proto->m_Script, "Update", instance->m_Self, lst);
        Py_DECREF(lst);
        return ret;
    }

    bool Update(HCollection collection, const UpdateContext* update_context)
    {
        uint32_t n_objects = collection->m_Instances.size();
        for (uint32_t i = 0; i < n_objects; ++i)
        {
            Update(collection, collection->m_Instances[i]);
        }

        uint32_t component_types = collection->m_ComponentTypeCount;
        for (uint32_t i = 0; i < component_types; ++i)
        {
            ComponentType* component_type = &collection->m_ComponentTypes[i];
            if (component_type->m_ComponentsUpdate)
            {
                component_type->m_ComponentsUpdate(collection, update_context, component_type->m_Context);
            }
        }

        return true;
    }

    void SetPosition(HCollection collection, HInstance instance, Point3 position)
    {
        instance->m_Position = position;
    }

    Point3 GetPosition(HCollection collection, HInstance instance)
    {
        return instance->m_Position;
    }

    void SetRotation(HCollection collection, HInstance instance, Quat rotation)
    {
        instance->m_Rotation = rotation;
    }

    Quat GetRotation(HCollection collection, HInstance instance)
    {
        return instance->m_Rotation;
    }
}
