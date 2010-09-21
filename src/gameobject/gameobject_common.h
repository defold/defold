#ifndef GAMEOBJECT_COMMON_H
#define GAMEOBJECT_COMMON_H

#include <stdint.h>
#include <vectormath/cpp/vectormath_aos.h>
using namespace Vectormath::Aos;
#include <dlib/array.h>
#include <dlib/mutex.h>
#include <dlib/index_pool.h>
#include <dlib/circular_array.h>
#include <dlib/stringpool.h>

namespace dmGameObject
{
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
            m_Identifier = UNNAMED_IDENTIFIER;
            m_CollectionPath = "";
            m_Depth = 0;
            m_Parent = INVALID_INSTANCE_INDEX;
            m_Index = INVALID_INSTANCE_INDEX;
            m_LevelIndex = INVALID_INSTANCE_INDEX;
            m_SiblingIndex = INVALID_INSTANCE_INDEX;
            m_FirstChildIndex = INVALID_INSTANCE_INDEX;
            m_ToBeDeleted = 0;
            m_ScriptInstancePOOOOP = 0x0;
        }

        ~Instance()
        {
        }

        // Collection this instances belongs to. Added for GetWorldPosition.
        // We should consider to remove this (memory footprint)
        HCollection     m_Collection;
        Quat            m_Rotation;
        Point3          m_Position;
        Prototype*      m_Prototype;
        uint32_t        m_Identifier;
        const char*     m_CollectionPath;

        // Hard pointer to the script instance, if any
        // TODO: This should not be needed since scripts are proper components, but are needed right now to support the script properties at GO instantiation.
        // In the long run, every component should be able to receive init-properties, which would demad a general solution.
        HScriptInstance m_ScriptInstancePOOOOP;

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

    // Internal representation of a transform
    struct Transform
    {
        Point3 m_Translation;
        Quat   m_Rotation;
    };

    // Max component types could not be larger than 255 since 0xff is used as a special case index meaning "all components" when passing named events
    const uint32_t MAX_COMPONENT_TYPES = 255;

    #define DM_GAMEOBJECT_EVENT_NAME "script_event"
    #define DM_GAMEOBJECT_EVENT_SOCKET_NAME "script"
    #define DM_GAMEOBJECT_REPLY_EVENT_SOCKET_NAME "script_reply"

    #define DM_GAMEOBJECT_CURRENT_IDENTIFIER_PATH_MAX (512)
    struct Register
    {
        uint32_t        m_ComponentTypeCount;
        ComponentType   m_ComponentTypes[MAX_COMPONENT_TYPES];
        dmMutex::Mutex  m_Mutex;
        // Current identifier path. Used during loading of collections and specifically collections in collections.
        // Contains the current path and is *protected* by m_Mutex.
        char            m_CurrentIdentifierPath[DM_GAMEOBJECT_CURRENT_IDENTIFIER_PATH_MAX];

        // All collections. Protected by m_Mutex
        dmArray<HCollection> m_Collections;

        // Pointer to current collection. Protected by m_Mutex. Related to m_CurrentIdentifierPath above.
        Collection*     m_CurrentCollection;

        Vector3         m_AccumulatedTranslation;
        Quat            m_AccumulatedRotation;

        uint32_t        m_EventId;
        uint32_t        m_EventSocketId;
        uint32_t        m_ReplyEventSocketId;

        Register();
        ~Register();
    };

    // Max hierarchical depth
    // 4 is interpreted as up to four levels of child nodes including root-nodes
    // Must be greater than zero
    const uint32_t MAX_HIERARCHICAL_DEPTH = 4;
    struct Collection
    {
        Collection(dmResource::HFactory factory, HRegister regist, uint32_t max_instances)
        {
            m_Factory = factory;
            m_Register = regist;
            m_MaxInstances = max_instances;
            m_Instances.SetCapacity(max_instances);
            m_Instances.SetSize(max_instances);
            m_InstanceIndices.SetCapacity(max_instances);
            m_LevelIndices.SetCapacity(max_instances * MAX_HIERARCHICAL_DEPTH);
            m_LevelIndices.SetSize(max_instances * MAX_HIERARCHICAL_DEPTH);
            m_WorldTransforms.SetCapacity(max_instances);
            m_WorldTransforms.SetSize(max_instances);
            m_InstancesToDelete.SetCapacity(max_instances);
            m_IDToInstance.SetCapacity(dmMath::Max(1U, max_instances/3), max_instances);
            // TODO: Un-hard-code
            m_FocusStack.SetCapacity(8);
            m_NameHash = 0;
            m_InUpdate = 0;
            m_StringPool = dmStringPool::New();

            for (uint32_t i = 0; i < m_LevelIndices.Size(); ++i)
            {
                m_LevelIndices[i] = INVALID_INSTANCE_INDEX;
            }

            memset(&m_Instances[0], 0, sizeof(Instance*) * max_instances);
            memset(&m_WorldTransforms[0], 0xcc, sizeof(Transform) * max_instances);
            memset(&m_LevelInstanceCount[0], 0, sizeof(m_LevelInstanceCount));

            for (uint32_t i = 0; i < m_Register->m_ComponentTypeCount; ++i)
            {
                if (m_Register->m_ComponentTypes[i].m_NewWorldFunction)
                    m_Register->m_ComponentTypes[i].m_NewWorldFunction(m_Register->m_ComponentTypes[i].m_Context, &m_ComponentWorlds[i]);
            }
        }
        // Resource factory
        dmResource::HFactory     m_Factory;

        // GameObject component register
        HRegister                m_Register;

        // Component type specific worlds
        void*                    m_ComponentWorlds[MAX_COMPONENT_TYPES];

        // Maximum number of instances
        uint32_t                 m_MaxInstances;

        // Array of instances. Zero values for free slots. Order must
        // always be preserved. Slots are allocated using index-pool
        // m_InstanceIndices below
        // Size if always = max_instances (at least for now)
        dmArray<Instance*>       m_Instances;

        // Index pool for mapping Instance::m_Index to m_Instances
        dmIndexPool16            m_InstanceIndices;

        // Index array of size m_Instances.Size() * MAX_HIERARCHICAL_DEPTH
        // Array of instance indices for each hierarchical level
        // Used for calculating transforms in scene-graph
        // Two dimensional table of indices with stride "max_instances"
        // Level 0 contains root-nodes in [0..m_LevelInstanceCount[0]-1]
        // Level 1 contains level 1 indices in [max_instances..max_instances+m_LevelInstanceCount[1]-1], ...
        dmArray<uint16_t>        m_LevelIndices;

        // Number of instances in each level
        uint16_t                 m_LevelInstanceCount[MAX_HIERARCHICAL_DEPTH];

        // Array of world transforms. Calculated using m_LevelIndices above
        dmArray<Transform>       m_WorldTransforms;

        // NOTE: Be *very* careful about m_InstancesToDelete
        // m_InstancesToDelete is an array of instances flagged for delete during Update(.)
        // Related code is Delete(.) and Update(.)
        dmArray<uint16_t>        m_InstancesToDelete;

        // Identifier to Instance mapping
        dmHashTable32<Instance*> m_IDToInstance;

        // Stack keeping track of which instance has the input focus
        dmCircularArray<Instance*> m_FocusStack;

        // Name-hash of the collection.
        uint32_t                   m_NameHash;

        // Pool of strings. Used primarily for instance identifiers
        dmStringPool::HPool        m_StringPool;

        // Set to 1 if in update-loop
        uint32_t                 m_InUpdate : 1;
    };
}

#endif // GAMEOBJECT_COMMON_H
