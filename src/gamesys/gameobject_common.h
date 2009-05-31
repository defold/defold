#ifndef GAMEOBJECT_COMMON_H
#define GAMEOBJECT_COMMON_H

#include <vector>
#include <stdint.h>
#include <vectormath/cpp/vectormath_aos.h>
using namespace Vectormath::Aos;

namespace GameObject
{
    struct Prototype
    {
        struct Component
        {
            Component(void* resource, uint32_t resource_type) :
                m_Resource(resource),
                m_ResourceType(resource_type) {}

            void*    m_Resource;
            uint32_t m_ResourceType;
        };

        const char*   m_Name;
        HScript       m_Script;
        std::vector<Component> m_Components;
    };

    struct Instance
    {
        Instance(Prototype* prototype)
        {
            m_Position = Point3(0,0,0);
            m_Prototype = prototype;
            m_Self = PyObject_CallObject((PyObject*) &PythonInstanceType, 0);
            ((PythonInstance*) m_Self)->m_Instance = this;
        }

        ~Instance()
        {
            Py_DECREF(m_Self);
        }

        Point3      m_Position;
        Prototype*  m_Prototype;
        PyObject*   m_Self;
    };
}

#endif // GAMEOBJECT_COMMON_H
