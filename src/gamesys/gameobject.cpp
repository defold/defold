#include <Python.h>

#include <dlib/hashtable.h>
#include "gameobject.h"
#include "gameobject_ddf.h"
#include "gameobject_script.h"
#include "gameobject_common.h"

namespace GameObject
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

    void DeleteCollection(HCollection collection, Resource::HFactory factory)
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
                                 ComponentDestroy destroy_function)
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

        collection->m_ComponentTypes[collection->m_ComponentTypeCount++] = component_type;

        return RESULT_OK;
    }

    Resource::CreateError PrototypeCreate(Resource::HFactory factory,
                                          void* context,
                                          const void* buffer, uint32_t buffer_size,
                                          Resource::SResourceDescriptor* resource)
    {
        GameObjectPrototypeDesc* proto_desc;

        DDFError e = DDFLoadMessage(buffer, buffer_size, &GameObject_GameObjectPrototypeDesc_DESCRIPTOR, (void**)(&proto_desc));
        if ( e != DDF_ERROR_OK )
        {
            return Resource::CREATE_ERROR_UNKNOWN;
        }

        Prototype* proto = new Prototype();
        proto->m_Components.reserve(proto_desc->m_Components.m_Count);

        for (uint32_t i = 0; i < proto_desc->m_Components.m_Count; ++i)
        {
            const char* component_name = proto_desc->m_Components[i];
            void* component;
            Resource::FactoryError fact_e = Resource::Get(factory, component_name, (void**) &component);

            if (fact_e != Resource::FACTORY_ERROR_OK)
            {
                // Error, release created
                for (uint32_t j = 0; j < proto->m_Components.size(); ++j)
                {
                    Resource::Release(factory, proto->m_Components[j].m_Resource);
                }
                delete proto;
                DDFFreeMessage(proto_desc);
                return Resource::CREATE_ERROR_UNKNOWN;
            }
            else
            {
                uint32_t resource_type;
                fact_e = Resource::GetType(factory, component, &resource_type);
                assert(fact_e == Resource::FACTORY_ERROR_OK);
                proto->m_Components.push_back(Prototype::Component(component, resource_type));
            }
        }

        HScript script;
        Resource::FactoryError fact_e = Resource::Get(factory, proto_desc->m_Script, (void**) &script);
        if (fact_e != Resource::FACTORY_ERROR_OK)
        {
            DDFFreeMessage(proto_desc);
            return Resource::CREATE_ERROR_UNKNOWN;
        }

        proto->m_Name = strdup(proto_desc->m_Name);
        proto->m_Script = script;

        resource->m_Resource = (void*) proto;

        DDFFreeMessage(proto_desc);
        return Resource::CREATE_ERROR_OK;
    }

    Resource::CreateError PrototypeDestroy(Resource::HFactory factory,
                                           void* context,
                                           Resource::SResourceDescriptor* resource)
    {
        Prototype* proto = (Prototype*) resource->m_Resource;
        for (uint32_t i = 0; i < proto->m_Components.size(); ++i)
        {
            Resource::Release(factory, proto->m_Components[i].m_Resource);
        }

        free((void*) proto->m_Name);
        Resource::Release(factory, proto->m_Script);
        delete proto;
        return Resource::CREATE_ERROR_OK;
    }

    Resource::CreateError ScriptCreate(Resource::HFactory factory,
                                       void* context,
                                       const void* buffer, uint32_t buffer_size,
                                       Resource::SResourceDescriptor* resource)
    {
        HScript script = NewScript(buffer);
        if (script)
        {
            resource->m_Resource = (void*) script;
            return Resource::CREATE_ERROR_OK;
        }
        else
        {
            return Resource::CREATE_ERROR_UNKNOWN;
        }
    }

    Resource::CreateError ScriptDestroy(Resource::HFactory factory,
                                        void* context,
                                        Resource::SResourceDescriptor* resource)
    {

        DeleteScript((HScript) resource->m_Resource);
        return Resource::CREATE_ERROR_OK;
    }

    Resource::FactoryError RegisterResourceTypes(Resource::HFactory factory)
    {
        Resource::FactoryError ret;
        ret = Resource::RegisterType(factory, "go", 0, &PrototypeCreate, &PrototypeDestroy);
        if (ret != Resource::FACTORY_ERROR_OK)
            return ret;

        ret = Resource::RegisterType(factory, "scriptc", 0, &ScriptCreate, &ScriptDestroy);
        if (ret != Resource::FACTORY_ERROR_OK)
            return ret;

        return ret;
    }

    HInstance New(HCollection collection, Resource::HFactory factory, const char* prototype_name)
    {
        Prototype* proto;
        Resource::FactoryError error = Resource::Get(factory, prototype_name, (void**)&proto);
        if (error != Resource::FACTORY_ERROR_OK)
        {
            return 0;
        }

        Instance* instance = new Instance(proto);
        instance->m_ID = (uint32_t) instance; // TODO: Not 64-bit friendly.

        uint32_t components_created = 0;
        bool ok = true;
        for (uint32_t i = 0; i < proto->m_Components.size(); ++i)
        {
            Prototype::Component* component = &proto->m_Components[i];
            ComponentType* component_type = FindComponentType(collection, component->m_ResourceType);
            if (component_type)
            {
                CreateResult create_result =  component_type->m_CreateFunction(collection, instance->m_ID, component->m_Resource, component_type->m_Context);
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
            else
            {
                ok = false;
                break;
            }
        }

        if (!ok)
        {
            for (uint32_t i = 0; i < components_created; ++i)
            {
                Prototype::Component* component = &proto->m_Components[i];
                ComponentType* component_type = FindComponentType(collection, component->m_ResourceType);
                assert(component_type);
                component_type->m_DestroyFunction(collection, instance->m_ID, component_type->m_Context);
            }

            // We can not call Delete here. Delete call DestroyFunction for every component
            Resource::Release(factory, instance->m_Prototype);
            delete instance;
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

    void Delete(HCollection collection, Resource::HFactory factory, HInstance instance)
    {
        Prototype* prototype = instance->m_Prototype;
        for (uint32_t i = 0; i < prototype->m_Components.size(); ++i)
        {
            Prototype::Component* component = &prototype->m_Components[i];
            ComponentType* component_type = FindComponentType(collection, component->m_ResourceType);
            assert(component_type);
            component_type->m_DestroyFunction(collection, instance->m_ID, component_type->m_Context);
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

        Resource::Release(factory, instance->m_Prototype);
        delete instance;
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

    void SetPosition(HCollection collection, HInstance instance, Point3 position)
    {
        instance->m_Position = position;
    }

    Point3 GetPosition(HCollection collection, HInstance instance)
    {
        return instance->m_Position;
    }
}
