#ifndef GAMEOBJECT_COMMON_H
#define GAMEOBJECT_COMMON_H

#include <vectormath/cpp/vectormath_aos.h>
using namespace Vectormath::Aos;

namespace GameObject
{
    struct Prototype
    {
        const char*        m_Name;
        HScript            m_Script;

        #if 0
        HMesh              m_Mesh;
        HMaterial          m_Material;
        HScript            m_Script;
        #endif
    /*
        TArray<SResourceDescriptor>  m_Resources;
        // or
        TArray<HEffect>    m_Effects;
        TArray<HAnimation> m_Animations;
        // ....
        */
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
