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

    // Invalid instance index. Implies that maximum number of instances is 32766 (ie 0x7fff - 1)
    const uint32_t INVALID_INSTANCE_INDEX = 0x7fff;

    // NOTE: Actual size of Instance is sizeof(Instance) + sizeof(uintptr_t) * m_UserDataCount
    struct Instance
    {
        Instance(Prototype* prototype)
        {
            m_Collection = 0;
            m_Rotation = Quat::identity();
            m_Position = Point3(0,0,0);
            m_Prototype = prototype;
            m_ScriptInstance = NewScriptInstance(prototype->m_Script, this);
            m_Identifier = UNNAMED_IDENTIFIER;
            m_Depth = 0;
            m_Parent = INVALID_INSTANCE_INDEX;
            m_Index = INVALID_INSTANCE_INDEX;
            m_LevelIndex = INVALID_INSTANCE_INDEX;
            m_SiblingIndex = INVALID_INSTANCE_INDEX;
            m_FirstChildIndex = INVALID_INSTANCE_INDEX;
            m_ToBeDeleted = 0;
        }

        ~Instance()
        {
            DeleteScriptInstance(m_ScriptInstance);
        }

        // Collection this instances belongs to. Added for GetWorldPosition.
        // We should consider to remove this (memory footprint)
        HCollection     m_Collection;
        Quat            m_Rotation;
        Point3          m_Position;
        Prototype*      m_Prototype;
        HScriptInstance m_ScriptInstance;
        uint32_t        m_Identifier;

        // Hierarchical depth
        uint16_t        m_Depth : 4;
        // Padding
        uint16_t        m_Pad : 12;

        // Index to parent
        uint16_t        m_Parent : 16;

        // Index to Collection::m_Instances
        uint16_t        m_Index : 15;
        // Used for deferred deletion
        uint16_t        m_ToBeDeleted : 1;

        // Index to Collection::m_LevelIndex. Index is relative to current level (m_Depth), eg first object in level L always has level-index 0
        // Level-index is used to reorder Collection::m_LevelIndex entries in O(1). Given an instance we need to find where the
        // instance index is located in Collection::m_LevelIndex
        uint16_t        m_LevelIndex : 15;
        uint16_t        m_Pad2 : 1;

        // Next sibling index. Index to Collection::m_Instances
        uint16_t        m_SiblingIndex : 15;
        uint16_t        m_Pad3 : 1;

        // First child index. Index to Collection::m_Instances
        uint16_t        m_FirstChildIndex : 15;
        uint16_t        m_Pad4 : 1;

        uint32_t        m_ComponentInstanceUserDataCount;
        uintptr_t       m_ComponentInstanceUserData[0];
    };
}

#endif // GAMEOBJECT_COMMON_H
