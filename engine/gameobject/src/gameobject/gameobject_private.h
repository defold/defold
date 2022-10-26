// Copyright 2020-2022 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
// 
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
// 
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

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
#include "component.h"

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
        Prototype()
            : m_Components(0)
            , m_ComponentCount(0)
        {
        }
        ~Prototype();
        struct Component
        {
            Component(void* resource,
                      uint32_t resource_type,
                      dmhash_t id,
                      dmhash_t resource_id,
                      ComponentType* type,
                      uint32_t type_index,
                      const Point3& position,
                      const Quat& rotation,
                      const Vector3& scale) :
                m_Id(id),
                m_ResourceId(resource_id),
                m_Type(type),
                m_TypeIndex(type_index),
                m_Resource(resource),
                m_ResourceType(resource_type),
                m_Position(position),
                m_Rotation(rotation),
                m_Scale(scale),
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
            Vector3         m_Scale;
            PropertySet     m_PropertySet;
        };

        Component*     m_Components;
        uint32_t       m_ComponentCount;
        // Resources referenced through property overrides inside the prototype
        dmArray<void*> m_PropertyResources;
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
        struct Collection* m_Collection;
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
        dmMutex::HMutex             m_Mutex;

        // All collections. Protected by m_Mutex
        dmArray<Collection*>        m_Collections;
        // Default capacity of collections
        uint32_t                    m_DefaultCollectionCapacity;
        uint32_t                    m_DefaultInputStackCapacity;

        Register();
        ~Register();
    };

    // Max hierarchical depth
    // depth is interpreted as up to <depth> levels of child nodes including root-nodes
    // Must be greater than zero
    const uint32_t MAX_HIERARCHICAL_DEPTH = 128;
    struct Collection
    {
        Collection(dmResource::HFactory factory, HRegister regist, uint32_t max_instances, uint32_t max_input_stack_entries);

        // Resource factory
        dmResource::HFactory     m_Factory;

        // GameObject component register
        HRegister                m_Register;

        struct CollectionHandle* m_HCollection;

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

        // Resources referenced through property overrides inside the collection
        dmArray<void*>           m_PropertyResources;

        // Array of dynamically allocated index arrays, one for each level
        // Used for calculating transforms in scene-graph
        // Two dimensional table of indices with stride "max_instances"
        // Level 0 contains root-nodes in [0..m_LevelIndices[0].Size()-1]
        // Level 1 contains level 1 indices in [0..m_LevelIndices[1].Size()-1]
        dmArray<uint16_t>        m_LevelIndices[MAX_HIERARCHICAL_DEPTH];

        // Array of world transforms. Calculated using m_LevelIndices above
        dmArray<Matrix4>         m_WorldTransforms;

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

        dmMutex::HMutex          m_Mutex;

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

        float                    m_FixedAccumTime;  // Accumulated time between fixed updates. Scaled time.

        // Set to 1 if in update-loop
        uint32_t                 m_InUpdate : 1;
        // Used for deferred deletion
        uint32_t                 m_ToBeDeleted : 1;
        // If the game object dynamically created in this collection should have the Z component of the position affected by scale
        uint32_t                 m_ScaleAlongZ : 1;
        uint32_t                 m_DirtyTransforms : 1;
        uint32_t                 m_Initialized : 1;
        uint32_t                 m_FirstUpdate : 1;
    };

    struct CollectionHandle
    {
        Collection* m_Collection;
    };

    ComponentType* FindComponentType(Register* regist, uint32_t resource_type, uint32_t* index);

    // Used by res_collection.cpp
    HInstance NewInstance(Collection* collection, Prototype* proto, const char* prototype_name);
    HInstance GetInstanceFromIdentifier(Collection* collection, dmhash_t identifier);
    void ReleaseInstanceIndex(uint32_t index, HCollection collection);
    Result SetIdentifier(Collection* collection, HInstance instance, const char* identifier);
    void ReleaseIdentifier(Collection* collection, HInstance instance);
    void UndoNewInstance(Collection* collection, HInstance instance);
    bool CreateComponents(Collection* collection, HInstance instance);
    void Delete(Collection* collection, HInstance instance, bool recursive);
    void UpdateTransforms(Collection* collection);
    void DeleteCollection(Collection* collection);
    bool IsCollectionInitialized(Collection* collection);
    Result AttachCollection(Collection* collection, const char* name, dmResource::HFactory factory, HRegister regist, HCollection hcollection);
    void DetachCollection(Collection* collection);

    void* GetResource(HInstance instance);

    void AcquireInputFocus(Collection* collection, HInstance instance);
    void ReleaseInputFocus(Collection* collection, HInstance instance);
    UpdateResult DispatchInput(Collection* collection, InputAction* input_actions, uint32_t input_action_count);

    // Unit test functions
    uint32_t GetAddToUpdateCount(HCollection collection); // Returns the number of items scheduled to be added to update
    uint32_t GetRemoveFromUpdateCount(HCollection collection); // Returns the number of items scheduled to be removed from update
}

#endif // GAMEOBJECT_COMMON_H
