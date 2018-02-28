#ifndef GAMEOBJECT_COMMON_H
#define GAMEOBJECT_COMMON_H

#include <dlib/hash.h>
#include <dlib/hashtable.h>
#include <dlib/index_pool.h>
#include <dlib/math.h>
#include <dlib/mutex.h>
#include <dlib/transform.h>

#include "gameobject.h"
#include "gameobject_props.h"

extern "C"
{
#include <lua/lua.h>
}

namespace dmGameObject
{
    extern const char* ID_SEPARATOR;

    // TODO: Configurable?
    const uint32_t MAX_MESSAGE_DATA_SIZE = 256;

    struct Prototype
    {
        Prototype() : m_ComponentsUserDataSize(0) {}

        struct Component
        {
            Component(void* resource,
                      uint32_t resource_type,
                      dmhash_t id,
                      dmhash_t resource_id,
                      ComponentType* type,
                      uint32_t type_index,
                      const Point3& position,
                      const Quat& rotation) :
                m_Id(id),
                m_ResourceId(resource_id),
                m_Type(type),
                m_TypeIndex(type_index),
                m_Resource(resource),
                m_ResourceType(resource_type),
                m_Position(position),
                m_Rotation(rotation),
                m_PropertySet()
            {
            }

            dmhash_t        m_Id;
            dmhash_t        m_ResourceId;
            ComponentType*  m_Type;
            uint32_t        m_TypeIndex;
            void*           m_Resource;
            uint32_t        m_ResourceType;
            Point3          m_Position;
            Quat            m_Rotation;
            PropertySet     m_PropertySet;
        };

        uint32_t m_ComponentsUserDataSize;
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
            m_Transform.SetIdentity();
            m_EulerRotation = Vector3(0.0f, 0.0f, 0.0f);
            m_PrevEulerRotation = Vector3(0.0f, 0.0f, 0.0f);
            m_Prototype = prototype;
            m_IdentifierIndex = INVALID_INSTANCE_POOL_INDEX;
            m_Identifier = UNNAMED_IDENTIFIER;
            dmHashInit64(&m_CollectionPathHashState, false);
            m_Depth = 0;
            m_Initialized = 0;
            m_ScaleAlongZ = 0;
            m_Bone = 0;
            m_Generated = 0;
            m_Parent = INVALID_INSTANCE_INDEX;
            m_Index = INVALID_INSTANCE_INDEX;
            m_LevelIndex = INVALID_INSTANCE_INDEX;
            m_SiblingIndex = INVALID_INSTANCE_INDEX;
            m_FirstChildIndex = INVALID_INSTANCE_INDEX;
            m_NextToDelete = INVALID_INSTANCE_INDEX;
            m_NextToAdd = INVALID_INSTANCE_INDEX;
            m_ToBeDeleted = 0;
            m_ToBeAdded = 0;
        }

        ~Instance()
        {
        }

        dmTransform::Transform m_Transform;

        // Shadowed rotation expressed in euler coordinates
        Vector3 m_EulerRotation;
        // Previous euler rotation, used to detect if the euler rotation has changed and should overwrite the real rotation (needed by animation)
        Vector3 m_PrevEulerRotation;
        // Collection this instances belongs to. Added for GetWorldPosition.
        // We should consider to remove this (memory footprint)
        HCollection     m_Collection;
        Prototype*      m_Prototype;

        uint32_t        m_IdentifierIndex;
        dmhash_t        m_Identifier;

        // Collection path hash-state. Used for calculating global identifiers. Contains the hash-state for the collection-path to the instance.
        // We might, in the future, for memory reasons, move this hash-state to a data-structure shared among all instances from the same collection.
        HashState64     m_CollectionPathHashState;

        // Hierarchical depth
        uint16_t        m_Depth : 8;
        // If the instance was initialized or not (Init())
        uint16_t        m_Initialized : 1;
        // If this game object should have the Z component of the position affected by scale
        uint16_t        m_ScaleAlongZ : 1;
        // If this game object is part of a skeleton
        uint16_t        m_Bone : 1;
        // If this is a generated instance, i.e. if the instance id is uniquely generated
        uint16_t        m_Generated : 1;
        // Padding
        uint16_t        m_Pad : 4;

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

#ifdef __EMSCRIPTEN__
        // TODO: FIX!! Workaround for LLVM/Clang bug when compiling with any optimization level > 0.
        //             Without this hack we get:
        //
        //               LLVM ERROR: I->getOperand(0)->getType() == i64
        //
        //             A theory was that the bug has something todo with bitfields.
        //             This dummy float breaks up the bitfield in smaller continous parts (<64bits?)...
        //             Remove when mozilla has fixed this properly...
        //             The bug is tracked as http://llvm.org/bugs/show_bug.cgi?id=19800
        float m_llvm_pad;
#endif

        // Index to next instance to delete or INVALID_INSTANCE_INDEX
        uint16_t        m_NextToDelete : 16;

        // Index to next instance to add-to-update or INVALID_INSTANCE_INDEX
        uint16_t        m_NextToAdd;

        // Next sibling index. Index to Collection::m_Instances
        uint16_t        m_SiblingIndex : 15;
        uint16_t        m_ToBeAdded : 1;

        // First child index. Index to Collection::m_Instances
        uint16_t        m_FirstChildIndex : 15;
        uint16_t        m_Pad4 : 1;

        uint32_t        m_ComponentInstanceUserDataCount;
        uintptr_t       m_ComponentInstanceUserData[0];
    };

    // Max component types could not be larger than 255 since the index is stored as a uint8_t
    const uint32_t MAX_COMPONENT_TYPES = 255;

    #define DM_GAMEOBJECT_CURRENT_IDENTIFIER_PATH_MAX (512)
    struct Register
    {
        uint32_t                    m_ComponentTypeCount;
        ComponentType               m_ComponentTypes[MAX_COMPONENT_TYPES];
        uint16_t                    m_ComponentTypesOrder[MAX_COMPONENT_TYPES];
        dmMutex::Mutex              m_Mutex;

        // All collections. Protected by m_Mutex
        dmArray<HCollection>        m_Collections;
        // Default capacity of collections
        uint32_t                    m_DefaultCollectionCapacity;

        dmHashTable64<HCollection>  m_SocketToCollection;

        Register();
        ~Register();
    };

    // Max hierarchical depth
    // depth is interpreted as up to <depth> levels of child nodes including root-nodes
    // Must be greater than zero
    const uint32_t MAX_HIERARCHICAL_DEPTH = 128;
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
            m_WorldTransforms.SetCapacity(max_instances);
            m_WorldTransforms.SetSize(max_instances);
            m_IDToInstance.SetCapacity(dmMath::Max(1U, max_instances/3), max_instances);
            // TODO: Un-hard-code
            m_InputFocusStack.SetCapacity(16);
            m_NameHash = 0;
            m_ComponentSocket = 0;
            m_FrameSocket = 0;

            // Instances that cannot use an ID from the InstanceIdPool will
            // generate indexes greater than the size of the pool.
            m_GenInstanceCounter = max_instances;
            m_GenCollectionInstanceCounter = 0;
            m_InstanceIdPool.SetCapacity(max_instances);
            m_InUpdate = 0;
            m_ToBeDeleted = 0;
            m_ScaleAlongZ = 0;
            m_DirtyTransforms = 1;

            m_InstancesToDeleteHead = INVALID_INSTANCE_INDEX;
            m_InstancesToDeleteTail = INVALID_INSTANCE_INDEX;

            m_InstancesToAddHead = INVALID_INSTANCE_INDEX;
            m_InstancesToAddTail = INVALID_INSTANCE_INDEX;

            memset(&m_Instances[0], 0, sizeof(Instance*) * max_instances);
            memset(&m_WorldTransforms[0], 0xcc, sizeof(dmTransform::Transform) * max_instances);
            memset(&m_LevelIndices[0], 0, sizeof(m_LevelIndices));
            memset(&m_ComponentInstanceCount[0], 0, sizeof(uint32_t) * MAX_COMPONENT_TYPES);
        }
        // Resource factory
        dmResource::HFactory     m_Factory;

        // GameObject component register
        HRegister                m_Register;

        // Component type specific worlds
        void*                    m_ComponentWorlds[MAX_COMPONENT_TYPES];
        // Component type specific instance counters
        uint32_t                 m_ComponentInstanceCount[MAX_COMPONENT_TYPES];

        // Maximum number of instances
        uint32_t                 m_MaxInstances;

        // Array of instances. Zero values for free slots. Order must
        // always be preserved. Slots are allocated using index-pool
        // m_InstanceIndices below
        // Size if always = max_instances (at least for now)
        dmArray<Instance*>       m_Instances;

        // Index pool for mapping Instance::m_Index to m_Instances
        dmIndexPool16            m_InstanceIndices;

        // Array of dynamically allocated index arrays, one for each level
        // Used for calculating transforms in scene-graph
        // Two dimensional table of indices with stride "max_instances"
        // Level 0 contains root-nodes in [0..m_LevelIndices[0].Size()-1]
        // Level 1 contains level 1 indices in [0..m_LevelIndices[1].Size()-1]
        dmArray<uint16_t>        m_LevelIndices[MAX_HIERARCHICAL_DEPTH];

        // Array of world transforms. Calculated using m_LevelIndices above
        dmArray<Matrix4> m_WorldTransforms;

        // Identifier to Instance mapping
        dmHashTable64<Instance*> m_IDToInstance;

        // Stack keeping track of which instance has the input focus
        dmArray<Instance*>       m_InputFocusStack;

        // Name-hash of the collection.
        dmhash_t                 m_NameHash;

        // Socket for sending to instances, dispatched between every component update
        dmMessage::HSocket       m_ComponentSocket;
        // Socket for sending to instances, dispatched once each update
        dmMessage::HSocket       m_FrameSocket;

        dmMutex::Mutex           m_Mutex;

        // Counter for generating instance ids, protected by m_Mutex
        uint32_t                 m_GenInstanceCounter;
        uint32_t                 m_GenCollectionInstanceCounter;
        dmIndexPool32            m_InstanceIdPool;

        // Head of linked list of instances scheduled for deferred deletion
        uint16_t                 m_InstancesToDeleteHead;
        // Tail of the same list, for O(1) appending
        uint16_t                 m_InstancesToDeleteTail;

        // Head of linked list of instances scheduled to be added to update
        uint16_t                 m_InstancesToAddHead;
        // Tail of the same list, for O(1) appending
        uint16_t                 m_InstancesToAddTail;

        // Set to 1 if in update-loop
        uint32_t                 m_InUpdate : 1;
        // Used for deferred deletion
        uint32_t                 m_ToBeDeleted : 1;
        // If the game object dynamically created in this collection should have the Z component of the position affected by scale
        uint32_t                 m_ScaleAlongZ : 1;
        uint32_t                 m_DirtyTransforms : 1;
    };

    ComponentType* FindComponentType(Register* regist, uint32_t resource_type, uint32_t* index);

    HInstance NewInstance(HCollection collection, Prototype* proto, const char* prototype_name);
    void ReleaseIdentifier(HCollection collection, HInstance instance);
    void UndoNewInstance(HCollection collection, HInstance instance);
    bool CreateComponents(HCollection collection, HInstance instance);
    void UpdateTransforms(HCollection collection);

    void ProfilerInitialize(const HRegister regist);
    void ProfilerFinalize(const HRegister regist);

    // Profiler snapshot iteration, for tests
    struct IterateProfilerSnapshotData
    {
        dmhash_t    m_ResourceId;
        dmhash_t    m_Tag;
        const char* m_TypeName;
        const char* m_Id;
        uint32_t    m_SnapshotIndex;
        uint32_t    m_ParentIndex;
        uint32_t    m_Flags;
    };
     // Iterate profiler snapshot data items. Set call_back function to 0 to get total items
    uint32_t IterateProfilerSnapshot(void* context, void (*call_back)(void* context, const IterateProfilerSnapshotData* data));

}

#endif // GAMEOBJECT_COMMON_H
