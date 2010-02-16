#ifndef GAMEOBJECT_COMMON_H
#define GAMEOBJECT_COMMON_H

#include <stdint.h>
#include <vectormath/cpp/vectormath_aos.h>
using namespace Vectormath::Aos;
#include <dlib/array.h>

namespace dmGameObject
{
    // TODO: Configurable?
    const uint32_t SCRIPT_EVENT_MAX = 256;
    // TODO: Configurable?
    const uint32_t SCRIPT_EVENT_SOCKET_BUFFER_SIZE = 0x8000;

    struct Prototype
    {
        struct Component
        {
            Component(void* resource, uint32_t resource_type, const char* name) :
                m_Resource(resource),
                m_ResourceType(resource_type)
            {
                m_NameHash = dmHashBuffer32(name, strlen(name));
            }

            void*    m_Resource;
            uint32_t m_ResourceType;
            uint32_t m_NameHash;
        };

        const char*            m_Name;
        HScript                m_Script;
        dmArray<Component>     m_Components;
    };

    // NOTE: Actual size of Instance is sizeof(Instance) + sizeof(uintptr_t) * m_UserDataCount
    struct Instance
    {
        Instance(Prototype* prototype)
        {
            m_Rotation = Quat::identity();
            m_Position = Point3(0,0,0);
            m_Prototype = prototype;
            m_ScriptInstance = NewScriptInstance(prototype->m_Script, this);
            m_Identifier = UNNAMED_IDENTIFIER;
            m_ToBeDeleted = 0;
        }

        ~Instance()
        {
            DeleteScriptInstance(m_ScriptInstance);
        }

        Quat            m_Rotation;
        Point3          m_Position;
        Prototype*      m_Prototype;
        HScriptInstance m_ScriptInstance;
        uint32_t        m_Identifier;
        uint32_t        m_ToBeDeleted : 1;
        uint32_t        m_ComponentInstanceUserDataCount;
        uintptr_t       m_ComponentInstanceUserData[0];
    };
}

#endif // GAMEOBJECT_COMMON_H
