#include "gameobject.h"
#include "gameobject_ddf.h"
#include "gameobject_script.h"
#include "gameobject_common.h"

#include <Python.h>

namespace GameObject
{
    void Initialize()
    {
        InitializeScript();
    }

    void Finalize()
    {
        FinalizeScript();
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

        HScript* script;
        Resource::FactoryError fact_e = Resource::Get(factory, proto_desc->m_Script, (void**) &script);
        if (fact_e != Resource::FACTORY_ERROR_OK)
        {
            DDFFreeMessage(proto_desc);
            return Resource::CREATE_ERROR_UNKNOWN;
        }

        Prototype* proto = new Prototype();
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

        ret = Resource::RegisterType(factory, "pyscript", 0, &ScriptCreate, &ScriptDestroy);
        if (ret != Resource::FACTORY_ERROR_OK)
            return ret;

        return ret;
    }

    HInstance New(Resource::HFactory factory, const char* prototype_name)
    {
        Prototype* proto;
        Resource::FactoryError error = Resource::Get(factory, prototype_name, (void**)&proto);
        if (error != Resource::FACTORY_ERROR_OK)
        {
            return 0;
        }
        Instance* instance = new Instance(proto);

        PyObject* lst = PyTuple_New(1);
        Py_INCREF(instance->m_Self); //NOTE: PyTuple_SetItem steals a reference
        PyTuple_SetItem(lst, 0, instance->m_Self);
        bool init_ok = RunScript(proto->m_Script, "Init", instance->m_Self, lst);
        Py_DECREF(lst);

        if (init_ok)
        {
            return instance;
        }
        else
        {
            Delete(factory, instance);
            return 0;
        }
    }

    void Delete(Resource::HFactory factory, HInstance instance)
    {
        Resource::Release(factory, instance->m_Prototype);
        delete instance;
    }

    bool Update(HInstance instance)
    {
        Prototype* proto = instance->m_Prototype;
        PyObject* lst = PyTuple_New(1);
        Py_INCREF(instance->m_Self); //NOTE: PyTuple_SetItem steals a reference
        PyTuple_SetItem(lst, 0, instance->m_Self);
        bool ret = RunScript(proto->m_Script, "Update", instance->m_Self, lst);
        Py_DECREF(lst);
        return ret;
    }
}
