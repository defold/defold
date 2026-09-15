// Copyright 2020-2026 The Defold Foundation
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

#include <new>
#include <algorithm>
#include <stdio.h>
#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/hashtable.h>
#include <dlib/message.h>
#include <dlib/hash.h>
#include <dlib/array.h>
#include <dlib/index_pool.h>
#include <dlib/profile.h>
#include <dlib/math.h>
#include <dlib/vmath.h>
#include <dlib/mutex.h>
#include <ddf/ddf.h>
#include "gameobject.h"
#include "gameobject_script.h"
#include "gameobject_private.h"
#include "gameobject_props.h"
#include "gameobject_props_lua.h"
#include "gameobject_props_ddf.h"

#include "gameobject/gameobject_ddf.h"

#include <dmsdk/dlib/vmath.h>
#include <dmsdk/resource/resource.h>

#include <dmsdk/dlib/vmath.h>
#include <dmsdk/resource/resource.hpp>

DM_PROPERTY_GROUP(rmtp_GameObject, "Gameobjects", 0);

DM_PROPERTY_U32(rmtp_GOInstances, 0, PROFILE_PROPERTY_FRAME_RESET, "# alive go instances / frame", &rmtp_GameObject);
DM_PROPERTY_U32(rmtp_GODeleted, 0, PROFILE_PROPERTY_FRAME_RESET, "# deleted instances / frame", &rmtp_GameObject);

namespace dmGameObject
{
    DM_STATIC_ASSERT(sizeof(InputAction) == 464, Invalid_Struct_Size); // to avoid it accidentally growing

    const char* COLLECTION_MAX_INSTANCES_KEY = "collection.max_instances";
    const char* COLLECTION_MAX_INPUT_STACK_ENTRIES_KEY = "collection.max_input_stack_entries";
    const dmhash_t UNNAMED_IDENTIFIER = dmHashBuffer64("__unnamed__", strlen("__unnamed__"));
#define ID_SEPARATOR_CHAR "/"
    const char* ID_SEPARATOR = ID_SEPARATOR_CHAR;
    const uint32_t MAX_DISPATCH_ITERATION_COUNT = 10;

    static Prototype EMPTY_PROTOTYPE;

    const uint16_t INVALID_COLLECTION_INDEX = 0xffff;
    const uint32_t MAX_COLLECTION_COUNT = 1U << 12;
    const uint32_t MAX_INSTANCE_COUNT = 1U << 20;

    const uint32_t INSTANCE_INDEX_BITS = 20;
    const uint32_t COLLECTION_INDEX_BITS = 12;
    const uint32_t COLLECTION_INDEX_SHIFT = INSTANCE_INDEX_BITS;
    const uint32_t INSTANCE_GENERATION_SHIFT = INSTANCE_INDEX_BITS + COLLECTION_INDEX_BITS;
    const uint64_t INSTANCE_INDEX_MASK = (1ULL << INSTANCE_INDEX_BITS) - 1;
    const uint64_t COLLECTION_INDEX_MASK = (1ULL << COLLECTION_INDEX_BITS) - 1;

    struct CollectionRegistrySlot
    {
        Collection* m_Collection;
        Collection* m_ReplacementCollection;
        uint32_t    m_InstanceGeneration;
        uint16_t    m_Generation;
        uint16_t    m_ReplacementGeneration;
        uint16_t    m_NextGeneration;
        uint16_t    m_NextFree;
    };

    static dmArray<CollectionRegistrySlot> g_CollectionRegistry;
    static uint16_t                        g_FirstFreeCollection = INVALID_COLLECTION_INDEX;
    static dmMutex::HMutex                 g_CollectionRegistryMutex = 0;

    static uint16_t GetCollectionIndex(HCollection collection)
    {
        return (uint16_t)(collection & 0xffff);
    }

    static uint16_t GetCollectionGeneration(HCollection collection)
    {
        return (uint16_t)(collection >> 16);
    }

    static HCollection MakeCollectionHandle(uint16_t index, uint16_t generation)
    {
        return ((uint32_t)generation << 16) | index;
    }

    static HInstance MakeInstanceHandle(uint32_t collection_index, uint32_t instance_index, uint32_t generation)
    {
        return ((uint64_t)generation << INSTANCE_GENERATION_SHIFT) |
               ((uint64_t)collection_index << COLLECTION_INDEX_SHIFT) |
               instance_index;
    }

    static uint32_t GetInstanceIndex(HInstance hinstance)
    {
        return (uint32_t)(hinstance & INSTANCE_INDEX_MASK);
    }

    static uint16_t GetInstanceCollectionIndex(HInstance hinstance)
    {
        return (uint16_t)((hinstance >> COLLECTION_INDEX_SHIFT) & COLLECTION_INDEX_MASK);
    }

    uint32_t GetInstanceGeneration(HInstance hinstance)
    {
        return (uint32_t)(hinstance >> INSTANCE_GENERATION_SHIFT);
    }

    uint32_t GetGeneration(HInstance hinstance)
    {
        return GetInstanceGeneration(hinstance);
    }

    static uint32_t AllocateInstanceGeneration(Collection* collection)
    {
        uint16_t collection_index = GetCollectionIndex(collection->m_HCollection);
        assert(collection_index < g_CollectionRegistry.Size());
        CollectionRegistrySlot& slot = g_CollectionRegistry[collection_index];
        assert(slot.m_Collection == collection || slot.m_ReplacementCollection == collection);
        slot.m_InstanceGeneration = NextGameObjectGeneration(slot.m_InstanceGeneration);
        return slot.m_InstanceGeneration;
    }

    static void GrowCollectionRegistry()
    {
        uint32_t old_size = g_CollectionRegistry.Size();
        uint32_t new_size = dmMath::Min(MAX_COLLECTION_COUNT, dmMath::Max(16U, old_size * 2));
        if (new_size == old_size)
            return;

        g_CollectionRegistry.SetCapacity(new_size);
        g_CollectionRegistry.SetSize(new_size);
        for (uint32_t i = new_size; i-- > old_size;)
        {
            CollectionRegistrySlot& slot = g_CollectionRegistry[i];
            slot.m_Collection = 0;
            slot.m_ReplacementCollection = 0;
            slot.m_InstanceGeneration = 0;
            slot.m_Generation = 0;
            slot.m_ReplacementGeneration = 0;
            slot.m_NextGeneration = 1;
            slot.m_NextFree = g_FirstFreeCollection;
            g_FirstFreeCollection = (uint16_t)i;
        }
    }

    static HCollection RegisterCollection(Collection* collection, HCollection replaced_collection)
    {
        DM_MUTEX_SCOPED_LOCK(g_CollectionRegistryMutex);

        if (replaced_collection != INVALID_COLLECTION)
        {
            uint16_t index = GetCollectionIndex(replaced_collection);
            if (index >= g_CollectionRegistry.Size())
                return INVALID_COLLECTION;

            CollectionRegistrySlot& slot = g_CollectionRegistry[index];
            if (!slot.m_Collection || slot.m_Generation != GetCollectionGeneration(replaced_collection) || slot.m_ReplacementCollection)
                return INVALID_COLLECTION;

            slot.m_ReplacementCollection = collection;
            slot.m_ReplacementGeneration = slot.m_NextGeneration;
            if (slot.m_ReplacementGeneration == slot.m_Generation)
                slot.m_ReplacementGeneration = NextCollectionGeneration(slot.m_ReplacementGeneration);
            slot.m_NextGeneration = NextCollectionGeneration(slot.m_ReplacementGeneration);
            return MakeCollectionHandle(index, slot.m_ReplacementGeneration);
        }

        if (g_FirstFreeCollection == INVALID_COLLECTION_INDEX)
            GrowCollectionRegistry();
        if (g_FirstFreeCollection == INVALID_COLLECTION_INDEX)
            return INVALID_COLLECTION;

        uint16_t index = g_FirstFreeCollection;
        CollectionRegistrySlot& slot = g_CollectionRegistry[index];
        g_FirstFreeCollection = slot.m_NextFree;
        slot.m_NextFree = INVALID_COLLECTION_INDEX;
        slot.m_Collection = collection;
        slot.m_Generation = slot.m_NextGeneration;
        slot.m_NextGeneration = NextCollectionGeneration(slot.m_NextGeneration);
        return MakeCollectionHandle(index, slot.m_Generation);
    }

    static void UnregisterCollection(HCollection collection)
    {
        if (collection == INVALID_COLLECTION)
            return;

        DM_MUTEX_SCOPED_LOCK(g_CollectionRegistryMutex);
        uint16_t index = GetCollectionIndex(collection);
        if (index >= g_CollectionRegistry.Size())
            return;

        CollectionRegistrySlot& slot = g_CollectionRegistry[index];
        uint16_t generation = GetCollectionGeneration(collection);
        if (slot.m_Collection && slot.m_Generation == generation)
        {
            slot.m_Collection = slot.m_ReplacementCollection;
            slot.m_Generation = slot.m_ReplacementGeneration;
            slot.m_ReplacementCollection = 0;
            slot.m_ReplacementGeneration = 0;
            if (!slot.m_Collection)
            {
                slot.m_NextFree = g_FirstFreeCollection;
                g_FirstFreeCollection = index;
            }
        }
        else if (slot.m_ReplacementCollection && slot.m_ReplacementGeneration == generation)
        {
            slot.m_ReplacementCollection = 0;
            slot.m_ReplacementGeneration = 0;
        }
    }

    Collection* GetCollectionFromHandle(HCollection collection)
    {
        if (collection == INVALID_COLLECTION)
            return 0;

        uint16_t index = GetCollectionIndex(collection);
        if (index >= g_CollectionRegistry.Size())
            return 0;

        CollectionRegistrySlot& slot = g_CollectionRegistry[index];
        uint16_t generation = GetCollectionGeneration(collection);
        if (slot.m_Generation == generation)
            return slot.m_Collection;
        if (slot.m_ReplacementGeneration == generation)
            return slot.m_ReplacementCollection;
        return 0;
    }

    HInstance GetInstanceHandle(Collection* collection, const Instance* instance)
    {
        if (!collection || !instance || instance->m_Index == INVALID_INSTANCE_INDEX || instance->m_Generation == 0)
            return INVALID_GAME_OBJECT;

        uint16_t collection_index = GetCollectionIndex(collection->m_HCollection);
        if (collection_index >= g_CollectionRegistry.Size() || instance->m_Index >= MAX_INSTANCE_COUNT)
            return INVALID_GAME_OBJECT;

        CollectionRegistrySlot& slot = g_CollectionRegistry[collection_index];
        if (slot.m_Collection != collection && slot.m_ReplacementCollection != collection)
            return INVALID_GAME_OBJECT;
        return MakeInstanceHandle(collection_index, instance->m_Index, instance->m_Generation);
    }

    Instance* GetInstanceFromHandle(HInstance hinstance, Collection** out_collection)
    {
        if (out_collection)
            *out_collection = 0;

        uint32_t generation = GetInstanceGeneration(hinstance);
        if (generation == 0)
            return 0;

        uint16_t collection_index = GetInstanceCollectionIndex(hinstance);
        if (collection_index >= g_CollectionRegistry.Size())
            return 0;

        uint32_t instance_index = GetInstanceIndex(hinstance);
        CollectionRegistrySlot& slot = g_CollectionRegistry[collection_index];
        Collection* collection = slot.m_Collection;
        if (collection && instance_index < collection->m_Instances.Size())
        {
            Instance* instance = collection->m_Instances[instance_index];
            if (instance && instance->m_Generation == generation)
            {
                if (out_collection)
                    *out_collection = collection;
                return instance;
            }
        }

        collection = slot.m_ReplacementCollection;
        if (collection && instance_index < collection->m_Instances.Size())
        {
            Instance* instance = collection->m_Instances[instance_index];
            if (instance && instance->m_Generation == generation)
            {
                if (out_collection)
                    *out_collection = collection;
                return instance;
            }
        }
        return 0;
    }

    Instance* GetInstanceFromHandle(Collection* collection, HInstance hinstance)
    {
        uint32_t generation = GetInstanceGeneration(hinstance);
        if (!collection || generation == 0 || GetInstanceCollectionIndex(hinstance) != GetCollectionIndex(collection->m_HCollection))
            return 0;

        uint32_t instance_index = GetInstanceIndex(hinstance);
        if (instance_index >= collection->m_Instances.Size())
            return 0;

        Instance* instance = collection->m_Instances[instance_index];
        return instance && instance->m_Generation == generation ? instance : 0;
    }

    static void Unlink(Collection* collection, Instance* instance);

#define PROP_FLOAT(var_name, prop_name)\
    const dmhash_t PROP_##var_name = dmHashString64(#prop_name);\

#define PROP_VECTOR3(var_name, prop_name)\
    const dmhash_t PROP_##var_name = dmHashString64(#prop_name);\
    const dmhash_t PROP_##var_name##_X = dmHashString64(#prop_name ".x");\
    const dmhash_t PROP_##var_name##_Y = dmHashString64(#prop_name ".y");\
    const dmhash_t PROP_##var_name##_Z = dmHashString64(#prop_name ".z");

#define PROP_QUAT(var_name, prop_name)\
    const dmhash_t PROP_##var_name = dmHashString64(#prop_name);\
    const dmhash_t PROP_##var_name##_X = dmHashString64(#prop_name ".x");\
    const dmhash_t PROP_##var_name##_Y = dmHashString64(#prop_name ".y");\
    const dmhash_t PROP_##var_name##_Z = dmHashString64(#prop_name ".z");\
    const dmhash_t PROP_##var_name##_W = dmHashString64(#prop_name ".w");

    static const dmhash_t PROP_SCALE_XY = dmHashString64("scale.xy");

    PROP_VECTOR3(POSITION, position);
    PROP_QUAT(ROTATION, rotation);
    PROP_VECTOR3(EULER, euler);
    PROP_VECTOR3(SCALE, scale);

    bool IsGameObjectTransformProperty(dmhash_t property_id)
    {
        return property_id == PROP_POSITION ||
               property_id == PROP_POSITION_X ||
               property_id == PROP_POSITION_Y ||
               property_id == PROP_POSITION_Z ||
               property_id == PROP_ROTATION ||
               property_id == PROP_ROTATION_X ||
               property_id == PROP_ROTATION_Y ||
               property_id == PROP_ROTATION_Z ||
               property_id == PROP_ROTATION_W ||
               property_id == PROP_EULER ||
               property_id == PROP_EULER_X ||
               property_id == PROP_EULER_Y ||
               property_id == PROP_EULER_Z ||
               property_id == PROP_SCALE ||
               property_id == PROP_SCALE_XY ||
               property_id == PROP_SCALE_X ||
               property_id == PROP_SCALE_Y ||
               property_id == PROP_SCALE_Z;
    }

    static void ResourceReloadedCallback(const ResourceReloadedParams* params);
    static void DoDeleteInstance(Collection* collection, Instance* instance);
    static bool InitInstance(Collection* collection, Instance* instance);
    static bool FinalInstance(Collection* collection, Instance* instance);

    static Collection* AllocCollection(dmResource::HFactory factory, HContext gocontext, uint32_t max_instances);
    static void CreateComponentWorlds(Collection* collection, uint32_t max_instances, dmGameObjectDDF::CollectionDesc* collection_desc);
    static void DeallocCollection(Collection* collection);
    static bool InitCollection(Collection* collection);
    static bool FinalCollection(Collection* collection);

    static dmScript::HContext g_ScriptContext;

    Prototype::~Prototype()
    {
        free(m_Components);
    }

    InputAction::InputAction()
    {
        memset(this, 0, sizeof(InputAction));
    }

    PropertyOptions::PropertyOptions()
    {
        memset(this, 0, sizeof(*this));
    }

    PropertyVar::PropertyVar()
    {
        DM_STATIC_ASSERT(sizeof(PropertyVar::m_URL) == sizeof(dmMessage::URL), Invalid_Struct_Alias_Size);

        m_Type = PROPERTY_TYPE_NUMBER;
        memset(this, 0, sizeof(*this));
    }

    PropertyVar::PropertyVar(float v)
    {
        m_Type = PROPERTY_TYPE_NUMBER;
        m_Number = v;
    }

    PropertyVar::PropertyVar(double v)
    {
        m_Type = PROPERTY_TYPE_NUMBER;
        m_Number = v;
    }

    PropertyVar::PropertyVar(dmhash_t v)
    {
        m_Type = PROPERTY_TYPE_HASH;
        m_Hash = v;
    }

    PropertyVar::PropertyVar(const dmMessage::URL& v)
    {
        m_Type = PROPERTY_TYPE_URL;
        dmMessage::URL* u = (dmMessage::URL*) m_URL;
        *u = v;
    }

    PropertyVar::PropertyVar(dmVMath::Vector3 v)
    {
        m_Type = PROPERTY_TYPE_VECTOR3;
        m_V4[0] = v.getX();
        m_V4[1] = v.getY();
        m_V4[2] = v.getZ();
    }

    PropertyVar::PropertyVar(dmVMath::Vector4 v)
    {
        m_Type = PROPERTY_TYPE_VECTOR4;
        m_V4[0] = v.getX();
        m_V4[1] = v.getY();
        m_V4[2] = v.getZ();
        m_V4[3] = v.getW();
    }

    PropertyVar::PropertyVar(dmVMath::Quat v)
    {
        m_Type = PROPERTY_TYPE_QUAT;
        m_V4[0] = v.getX();
        m_V4[1] = v.getY();
        m_V4[2] = v.getZ();
        m_V4[3] = v.getW();
    }

    PropertyVar::PropertyVar(bool v)
    {
        m_Type = PROPERTY_TYPE_BOOLEAN;
        m_Bool = v;
    }
    PropertyVar::PropertyVar(const char* v)
    {
        m_Type = PROPERTY_TYPE_TEXT;
        m_Text = v;
    }

    PropertyVar::PropertyVar(Matrix4 v)
    {
        m_Type   = PROPERTY_TYPE_MATRIX4;

        Vector4& c0 = v[0];
        float& v0   = c0[0];
        memcpy(m_M4, &v0, sizeof(m_M4));
    }

    Context::Context()
    {
        m_ComponentTypeCount = 0;
        m_DefaultCollectionCapacity = DEFAULT_MAX_COLLECTION_CAPACITY;
        m_DefaultInputStackCapacity = DEFAULT_MAX_INPUT_STACK_CAPACITY;
        m_ContextRegistry = 0;
        m_Mutex = dmMutex::New();
    }

    Context::~Context()
    {
        dmMutex::Delete(m_Mutex);
    }

    ComponentType::ComponentType()
    {
        memset(this, 0, sizeof(*this));
    }

    void Initialize(HContext gocontext, dmScript::HContext context)
    {
        InitializeScript(context);
        g_ScriptContext = context;
    }

    HContext NewContext()
    {
        if (!g_CollectionRegistryMutex)
            g_CollectionRegistryMutex = dmMutex::New();
        return new Context();
    }

    Collection::Collection(dmResource::HFactory factory, HContext gocontext, uint32_t max_instances, uint32_t max_input_stack_entries)
    {
        m_Factory = factory;
        m_Register = gocontext;
        m_CollectionResource = 0;
        m_Instances.SetCapacity(max_instances);
        m_Instances.SetSize(max_instances);
        m_InstanceIndices.SetCapacity(max_instances);
        m_WorldTransforms.SetCapacity(max_instances);
        m_WorldTransforms.SetSize(max_instances);
        m_IDToInstance.SetCapacity(max_instances);
        m_InputFocusStack.SetCapacity(max_input_stack_entries);
        m_NameHash = 0;
        m_ComponentSocket = 0;
        m_FrameSocket = 0;

        m_GenCollectionInstanceCounter = 0;
        m_InstanceIdPool.SetCapacity(max_instances);
        m_InUpdate = 0;
        m_ToBeDeleted = 0;
        m_DirtyTransforms = 1;
        m_Initialized = 0;
        m_FixedAccumTime = 0.0f;
        m_FirstUpdate = 1;

        m_InstancesToDeleteHead = INVALID_INSTANCE_INDEX;
        m_InstancesToDeleteTail = INVALID_INSTANCE_INDEX;

        m_InstancesToAddHead = INVALID_INSTANCE_INDEX;
        m_InstancesToAddTail = INVALID_INSTANCE_INDEX;

        memset(&m_Instances[0], 0, sizeof(m_Instances[0]) * max_instances);
        memset(&m_LevelIndices[0], 0, sizeof(m_LevelIndices));
    }

    Result SetCollectionDefaultCapacity(HContext gocontext, uint32_t capacity)
    {
        assert(gocontext != 0x0);
        if (capacity > MAX_INSTANCE_COUNT || capacity == 0)
            return RESULT_INVALID_OPERATION;
        gocontext->m_DefaultCollectionCapacity = capacity;
        return RESULT_OK;
    }

    uint32_t GetCollectionDefaultCapacity(HContext gocontext)
    {
        assert(gocontext != 0x0);
        return gocontext->m_DefaultCollectionCapacity;
    }

    void SetContextRegistry(HContext gocontext, HContextRegistry context_registry)
    {
        gocontext->m_ContextRegistry = context_registry;
    }

    HContextRegistry GetContextRegistry(HContext gocontext)
    {
        return gocontext->m_ContextRegistry;
    }

    void SetInputStackDefaultCapacity(HContext gocontext, uint32_t capacity)
    {
        assert(gocontext != 0x0);
        gocontext->m_DefaultInputStackCapacity = capacity;
    }

    static uint32_t GetInputStackDefaultCapacity(HContext gocontext)
    {
        assert(gocontext != 0x0);
        return gocontext->m_DefaultInputStackCapacity;
    }

    void AddDynamicResourceHash(HCollection hcollection, dmhash_t resource_hash)
    {
        Collection* collection = GetCollectionFromHandle(hcollection);
        if (!collection)
            return;
        DM_MUTEX_SCOPED_LOCK(collection->m_Mutex);
        // The creating collection tracks each dynamic resource once so it can release
        // it when the collection is deleted. Avoid recording the same resource twice,
        // since that would release it twice.
        for (uint32_t i = 0; i < collection->m_DynamicResources.Size(); ++i)
        {
            if (collection->m_DynamicResources[i] == resource_hash)
            {
                return;
            }
        }
        if (collection->m_DynamicResources.Remaining() == 0)
        {
            collection->m_DynamicResources.OffsetCapacity(1);
        }
        collection->m_DynamicResources.Push(resource_hash);
    }

    void RemoveDynamicResourceHash(HCollection hcollection, dmhash_t resource_hash)
    {
        Collection* owner = GetCollectionFromHandle(hcollection);
        if (!owner)
            return;
        Context* gocontext = owner->m_Register;
        DM_MUTEX_SCOPED_LOCK(gocontext->m_Mutex);
        // Search every collection in the register to remove the actual owner's entry.
        // We need to do this to avoid the following scenario (#13002):
        //
        // 1. Collection A creates the resource and records its hash.
        // 2. Collection B calls resource.release().
        // 3. Searching only B finds nothing, so A's entry is still in the register.
        // 4. When A unloads, its stale entry resolves to a deleted descriptor and asserts.
        for (uint32_t collection_index = 0; collection_index < gocontext->m_Collections.Size(); ++collection_index)
        {
            Collection* collection = gocontext->m_Collections[collection_index];
            DM_MUTEX_SCOPED_LOCK(collection->m_Mutex);
            for (uint32_t resource_index = 0; resource_index < collection->m_DynamicResources.Size(); ++resource_index)
            {
                if (collection->m_DynamicResources[resource_index] == resource_hash)
                {
                    collection->m_DynamicResources.EraseSwap(resource_index);
                    break;
                }
            }
        }
    }

    static void ReleaseDynamicResources(Collection* collection)
    {
        DM_MUTEX_SCOPED_LOCK(collection->m_Mutex);
        for (int i = 0; i < collection->m_DynamicResources.Size(); ++i)
        {
            HResourceDescriptor rd = dmResource::FindByHash(collection->m_Factory, collection->m_DynamicResources[i]);
            if (!rd)
            {
                dmLogError("Unable to find '%s' when releasing dynamic resources", dmHashReverseSafe64(collection->m_DynamicResources[i]));
                continue;
            }
            void* resource = dmResource::GetResource(rd);
            dmResource::Release(collection->m_Factory, resource);
        }
        collection->m_DynamicResources.SetSize(0);
        collection->m_DynamicResources.SetCapacity(0);
    }

    void DeleteCollections(HContext gocontext)
    {
        DM_MUTEX_SCOPED_LOCK(gocontext->m_Mutex);
        uint32_t collection_count = gocontext->m_Collections.Size();
        for (uint32_t i = 0; i < collection_count; ++i)
        {
            // TODO Note indexing of m_Collections is always 0 because DeleteCollection modifies the array.
            // Should be fixed by DEF-54
            Collection* collection = gocontext->m_Collections[0];
            FinalCollection(collection);
            DeleteCollection(collection);
        }
        gocontext->m_Collections.SetSize(0);
    }

    HCollection GetCollectionByHash(HContext gocontext, dmhash_t socket_name)
    {
        DM_MUTEX_SCOPED_LOCK(gocontext->m_Mutex);
        uint32_t collection_count = gocontext->m_Collections.Size();
        for (uint32_t i = 0; i < collection_count; ++i)
        {
            Collection* collection = gocontext->m_Collections[i];
            if (collection->m_NameHash == socket_name)
                return collection->m_HCollection;
        }
        return 0;
    }

    void DeleteContext(HContext gocontext)
    {
        DeleteCollections(gocontext);
        delete gocontext;
    }

    uint32_t GetMaxComponentInstances(uint64_t name_hash, dmGameObjectDDF::CollectionDesc* collection_desc)
    {
        if (!collection_desc || collection_desc->m_ComponentTypes.m_Count == 0)
        {
            return 0xFFFFFFFF;
        }
        for (uint32_t i = 0; i < collection_desc->m_ComponentTypes.m_Count; ++i)
        {
            const dmGameObjectDDF::ComponenTypeDesc& type_desc = collection_desc->m_ComponentTypes[i];
            if (name_hash == type_desc.m_NameHash)
            {
                return type_desc.m_MaxCount;
            }
        }
        return 0;
    }

    Collection* AllocCollection(dmResource::HFactory factory, HContext gocontext, uint32_t max_instances)
    {
        Collection* collection = new Collection(factory, gocontext, max_instances, GetInputStackDefaultCapacity(gocontext));
        collection->m_Mutex = dmMutex::New();

        return collection;
    }

    static void CreateComponentWorlds(Collection* collection, uint32_t max_instances, dmGameObjectDDF::CollectionDesc* collection_desc)
    {
        HContext gocontext = collection->m_Register;
        for (uint32_t i = 0; i < gocontext->m_ComponentTypeCount; ++i)
        {
            if (gocontext->m_ComponentTypes[i].m_NewWorldFunction)
            {
                ComponentNewWorldParams params;
                params.m_Context = gocontext->m_ComponentTypes[i].m_Context;
                params.m_ComponentIndex = i;
                params.m_MaxComponentInstances = GetMaxComponentInstances(gocontext->m_ComponentTypes[i].m_NameHash, collection_desc);
                params.m_MaxInstances = max_instances;
                params.m_World = &collection->m_ComponentWorlds[i];
                gocontext->m_ComponentTypes[i].m_NewWorldFunction(params);
            }
        }
    }

    static void DeallocCollectionStorage(Collection* collection)
    {
        HContext gocontext = collection->m_Register;
        for (uint32_t i = 0; i < gocontext->m_ComponentTypeCount; ++i)
        {
            DM_PROFILE_DYN(gocontext->m_ComponentTypes[i].m_Name, 0);

            ComponentDeleteWorldParams params;
            params.m_Context = gocontext->m_ComponentTypes[i].m_Context;
            params.m_World = collection->m_ComponentWorlds[i];
            if (gocontext->m_ComponentTypes[i].m_DeleteWorldFunction)
                gocontext->m_ComponentTypes[i].m_DeleteWorldFunction(params);
        }
        dmMutex::Delete(collection->m_Mutex);
    }

    void DeallocCollection(Collection* collection)
    {
        DM_PROFILE("DeallocCollection");

        DeallocCollectionStorage(collection);
        delete collection;
    }

    Result AttachCollection(Collection* collection, const char* name)
    {
        HContext gocontext = collection->m_Register;

        // if there exists a collection with the same name and that collection
        // is to be deleted we immediately delete it so that we can attach the
        // the new one
        HCollection hexisting = GetCollectionByHash(gocontext, dmHashString64(name));
        Collection* existing_collection = GetCollectionFromHandle(hexisting);
        if (existing_collection && existing_collection->m_ToBeDeleted)
        {
            DeleteCollection(existing_collection);
        }

        char name_frame[128];
        dmStrlCpy(name_frame, name, sizeof(name_frame));
        dmStrlCat(name_frame, "_frame", sizeof(name_frame));
        const char* socket_names[] = {name, name_frame};
        dmMessage::HSocket* sockets[] = {&collection->m_ComponentSocket, &collection->m_FrameSocket};
        for (int i = 0; i < 2; ++i)
        {
            dmMessage::Result result = dmMessage::NewSocket(socket_names[i], sockets[i]);
            if (result != dmMessage::RESULT_OK)
            {
                for (int j = 0; j < i; ++j)
                {
                    dmMessage::DeleteSocket(*sockets[j]);
                    *sockets[j] = 0;
                }
                if (result == dmMessage::RESULT_SOCKET_EXISTS)
                {
                    dmLogError("The collection '%s' could not be created since there is already a socket with the same name.", socket_names[i]);
                }
                else if (result == dmMessage::RESULT_INVALID_SOCKET_NAME)
                {
                    dmLogError("The collection '%s' could not be created since the name is invalid for sockets.", socket_names[i]);
                }
                return RESULT_UNKNOWN_ERROR;
            }
        }

        dmResource::RegisterResourceReloadedCallback(collection->m_Factory, ResourceReloadedCallback, collection);

        DM_MUTEX_SCOPED_LOCK(gocontext->m_Mutex);
        if (gocontext->m_Collections.Full())
        {
            gocontext->m_Collections.OffsetCapacity(4);
        }
        gocontext->m_Collections.Push(collection);
        return RESULT_OK;
    }

    void DetachCollection(Collection* collection, bool unregister_handle)
    {
        HContext gocontext = collection->m_Register;

        dmMutex::Lock(gocontext->m_Mutex);
        for (uint32_t i = 0; i < gocontext->m_Collections.Size(); ++i)
        {
            // TODO This design is not really thought through, since it modifies the m_Collections
            // member in a hidden context. Reported in DEF-54
            if (gocontext->m_Collections[i] == collection)
            {
                // Resize m_Collections manually here instead of using EraseSwap.
                // This is to keep m_Collections in their spawn order, otherwise we run the risk
                // of deleting proxy collections before their parent/spawning which in turn is
                // problematic since the "parent" are going to delete the proxy collections they spawned.
                for (uint32_t j = i; j < gocontext->m_Collections.Size() - 1; ++j)
                {
                    gocontext->m_Collections[j] = gocontext->m_Collections[j+1];
                }
                gocontext->m_Collections.SetSize(gocontext->m_Collections.Size() - 1);
                break;
            }
        }

        dmMutex::Unlock(gocontext->m_Mutex);

        dmResource::UnregisterResourceReloadedCallback(collection->m_Factory, ResourceReloadedCallback, collection);

        if (collection->m_ComponentSocket)
        {
            dmMessage::Consume(collection->m_ComponentSocket);
            dmMessage::DeleteSocket(collection->m_ComponentSocket);
            collection->m_ComponentSocket = 0;
        }
        if (collection->m_FrameSocket)
        {
            dmMessage::Consume(collection->m_FrameSocket);
            dmMessage::DeleteSocket(collection->m_FrameSocket);
            collection->m_FrameSocket = 0;
        }

        if (unregister_handle)
        {
            UnregisterCollection(collection->m_HCollection);
            collection->m_HCollection = 0;
        }
    }

    static void DetachCollection(Collection* collection)
    {
        DetachCollection(collection, true);
    }

    HCollection NewCollection(const char* name, dmResource::HFactory factory, HContext gocontext, uint32_t max_instances, HCollectionDesc collection_desc, HCollection replaced_hcollection)
    {
        if (max_instances == 0 || max_instances > MAX_INSTANCE_COUNT)
        {
            dmLogError("max_instances must be between 1 and %u", MAX_INSTANCE_COUNT);
            return 0;
        }

        Collection* collection = AllocCollection(factory, gocontext, max_instances);
        if (!collection)
        {
            return 0;
        }

        collection->m_NameHash = dmHashString64(name); // Same as the socket name

        HCollection hcollection = RegisterCollection(collection, replaced_hcollection);
        if (hcollection == INVALID_COLLECTION)
        {
            dmMutex::Delete(collection->m_Mutex);
            delete collection;
            return INVALID_COLLECTION;
        }
        collection->m_HCollection = hcollection;

        CreateComponentWorlds(collection, max_instances, (dmGameObjectDDF::CollectionDesc*)collection_desc);

        Result result = AttachCollection(collection, name);
        if (result != RESULT_OK)
        {
            DeallocCollectionStorage(collection);
            UnregisterCollection(hcollection);
            delete collection;
            return 0;
        }
        return hcollection;
    }

    static void DoDeleteAll(Collection* collection)
    {
        // This will perform tons of unnecessary work to resolve and reorder
        // the hierarchies and other things but will serve as a nice test case
        for (uint32_t i = 0; i < collection->m_Instances.Size(); ++i)
        {
            Instance* instance = collection->m_Instances[i];
            if (instance)
            {
                DoDeleteInstance(collection, instance);
            }
        }
    }

    void DeleteCollection(Collection* collection)
    {
        DM_PROFILE("DeleteCollection");

        // We mark the collection as beeing deleted here to avoid component
        // triggered recursive deletes to add gameobjects to the delayed delete list.
        //
        // For example, deleting a Spine component would mark bone gameobjects
        // to be deleted next frame. However, since DoDeleteAll just deletes all
        // instances directly, the entries in the "delayed delete list" might already
        // have been deleted, making it impossible to add the spine bones to this list.
        collection->m_ToBeDeleted = 1;

        FinalCollection(collection);
        DoDeleteAll(collection);
        ReleaseDynamicResources(collection);

        DetachCollection(collection);
        DeallocCollection(collection);
    }

    // Really should be renamed "DelayDelete"
    void DeleteCollection(HCollection hcollection)
    {
        Collection* collection = GetCollectionFromHandle(hcollection);
        if (collection)
            collection->m_ToBeDeleted = 1;
    }

    uint32_t GetComponentTypeIndex(HCollection hcollection, dmhash_t type_hash)
    {
        Collection* collection = GetCollectionFromHandle(hcollection);
        if (!collection)
            return 0xFFFFFFFF;
        Context* gocontext = collection->m_Register;
        for (uint32_t i = 0; i < gocontext->m_ComponentTypeCount; ++i)
        {
            ComponentType* ct = &gocontext->m_ComponentTypes[i];
            if (ct->m_NameHash == type_hash)
            {
                return i;
            }
        }
        return 0xFFFFFFFF;
    }

    HComponentWorld GetWorld(HCollection hcollection, uint32_t component_type_index)
    {
        Collection* collection = GetCollectionFromHandle(hcollection);
        if (!collection)
            return 0;
        Context* gocontext = collection->m_Register;
        if (component_type_index < gocontext->m_ComponentTypeCount)
        {
            return collection->m_ComponentWorlds[component_type_index];
        }
        else
        {
            return 0x0;
        }
    }

    void* GetContext(HCollection hcollection, uint32_t component_type_index)
    {
        Collection* collection = GetCollectionFromHandle(hcollection);
        if (!collection)
            return 0;
        Context* gocontext = collection->m_Register;
        if (component_type_index < gocontext->m_ComponentTypeCount)
        {
            ComponentType* ct = &gocontext->m_ComponentTypes[component_type_index];
            return ct->m_Context;
        }
        else
        {
            return 0;
        }
    }

    ComponentType* FindComponentType(Context* gocontext, HResourceType resource_type, uint32_t* index)
    {
        for (uint32_t i = 0; i < gocontext->m_ComponentTypeCount; ++i)
        {
            ComponentType* ct = &gocontext->m_ComponentTypes[i];
            if (ct->m_ResourceType == resource_type)
            {
                if (index != 0x0)
                    *index = i;
                return ct;
            }
        }
        return 0;
    }

    uint32_t GetNumComponentTypes(HContext gocontext)
    {
        return gocontext->m_ComponentTypeCount;
    }

    ComponentType* GetComponentType(HContext gocontext, uint32_t index)
    {
        return &gocontext->m_ComponentTypes[index];
    }

    struct ComponentTypeSortPred
    {
        HContext m_Register;
        ComponentTypeSortPred(HContext gocontext) : m_Register(gocontext) {}

        bool operator ()(const uint16_t& a, const uint16_t& b) const
        {
            return m_Register->m_ComponentTypes[a].m_UpdateOrderPrio < m_Register->m_ComponentTypes[b].m_UpdateOrderPrio;
        }
    };

    Result RegisterComponentType(HContext gocontext, const ComponentType& type)
    {
        if (gocontext->m_ComponentTypeCount == MAX_COMPONENT_TYPES)
            return RESULT_OUT_OF_RESOURCES;

        if (FindComponentType(gocontext, type.m_ResourceType, 0x0) != 0)
            return RESULT_ALREADY_REGISTERED;

        if ((type.m_UpdateFunction != 0x0 
            || type.m_FixedUpdateFunction != 0x0
            || type.m_LateUpdateFunction != 0x0) && type.m_AddToUpdateFunction == 0x0) {
            dmLogWarning("Registering an Update/FixedUpdate/LateUpdate function for '%s' requires the registration of an AddToUpdate function.", type.m_Name);
            return RESULT_INVALID_OPERATION;
        }

        gocontext->m_ComponentTypes[gocontext->m_ComponentTypeCount] = type;
        gocontext->m_ComponentTypes[gocontext->m_ComponentTypeCount].m_NameHash = dmHashString64(type.m_Name);
        gocontext->m_ComponentTypesOrder[gocontext->m_ComponentTypeCount] = gocontext->m_ComponentTypeCount;
        gocontext->m_ComponentTypeCount++;
        return RESULT_OK;
    }

    Result SetUpdateOrderPrio(HContext gocontext, HResourceType resource_type, uint16_t prio)
    {
        bool found = false;
        for (uint32_t i = 0; i < gocontext->m_ComponentTypeCount; ++i)
        {
            if (gocontext->m_ComponentTypes[i].m_ResourceType == resource_type)
            {
                gocontext->m_ComponentTypes[i].m_UpdateOrderPrio = prio;
                found = true;
                break;
            }
        }
        if (!found)
        {
            return RESULT_RESOURCE_TYPE_NOT_FOUND;
        }

        return RESULT_OK;
    }

    void SortComponentTypes(HContext gocontext)
    {
        std::sort(gocontext->m_ComponentTypesOrder, gocontext->m_ComponentTypesOrder + gocontext->m_ComponentTypeCount, ComponentTypeSortPred(gocontext));
    }


    static void EraseSwapLevelIndex(Collection* collection, Instance* instance)
    {
        /*
         * Remove instance from m_LevelIndices using an erase-swap operation
         */

        dmArray<uint32_t>& level = collection->m_LevelIndices[instance->m_Depth];
        assert(level.Size() > 0);
        assert(instance->m_LevelIndex < level.Size());

        uint32_t level_index = instance->m_LevelIndex;
        uint32_t swap_in_index = level.EraseSwap(level_index);
        Instance* swap_in_instance = collection->m_Instances[swap_in_index];
        assert(swap_in_instance->m_Index == swap_in_index);
        swap_in_instance->m_LevelIndex = level_index;
    }

    /*
     * Heuristic for expanding the level indices arrays:
     * * Increase capacity by 50%, but:
     * ** 10 elements as min
     * ** Up to max_instances as max
     */
    static void ExpandLevel(dmArray<uint32_t>& level, uint32_t max_instances)
    {
        const uint32_t min_offset = 10;
        const uint32_t max_offset = max_instances - level.Capacity();
        int32_t offset = dmMath::Min(max_offset, dmMath::Max(min_offset, level.Size() / 2));
        level.OffsetCapacity(offset);
    }

    static void InsertInstanceInLevelIndex(Collection* collection, Instance* instance)
    {
        /*
         * Insert instance in m_LevelIndices at level set in instance->m_Depth
         */
        dmArray<uint32_t>& level = collection->m_LevelIndices[instance->m_Depth];
        if (level.Full())
            ExpandLevel(level, collection->m_Instances.Size());
        assert(!level.Full());

        uint32_t level_index = level.Size();
        level.SetSize(level_index + 1);
        level[level_index] = instance->m_Index;
        instance->m_LevelIndex = level_index;
    }

    static Instance* AllocInstance(Prototype* proto, const char* prototype_name) {
        // Count number of component userdata fields required
        uint32_t component_instance_userdata_count = 0;
        for (uint32_t i = 0; i < proto->m_ComponentCount; ++i)
        {
            Prototype::Component* component = &proto->m_Components[i];
            ComponentType* component_type = component->m_Type;
            if (!component_type)
            {
                dmLogError("Internal error. Component type #%d for '%s' not found.", i, prototype_name);
                assert(false);
            }
            if (component_type->m_InstanceHasUserData)
                component_instance_userdata_count++;
        }

        uint32_t component_userdata_size = sizeof(((Instance*)0)->m_ComponentInstanceUserData[0]);
        // NOTE: Allocate actual Instance with *all* component instance user-data accounted
        void* instance_memory = ::operator new (sizeof(Instance) + component_instance_userdata_count * component_userdata_size);
        Instance* instance = new(instance_memory) Instance(proto);
        instance->m_ComponentInstanceUserDataCount = component_instance_userdata_count;
        return instance;
    }

    static void DeallocInstance(Instance* instance) {
        instance->~Instance();
        void* instance_memory = (void*) instance;

        // This is required for failing test
        // TODO: #ifdef on something...?
        // Clear all memory excluding ComponentInstanceUserData
        memset(instance_memory, 0xcc, sizeof(Instance));
        operator delete (instance_memory);
    }

    Instance* NewInstance(Collection* collection, Prototype* proto, const char* prototype_name) {
        if (collection->m_InstanceIndices.Remaining() == 0)
        {
            dmLogError("The game object instance could not be created since the buffer is full (%d). Increase the capacity with collection.max_instances", collection->m_InstanceIndices.Capacity());
            return 0;
        }
        Instance* instance = AllocInstance(proto, prototype_name);
        uint32_t instance_index = collection->m_InstanceIndices.Pop();
        instance->m_Index = instance_index;
        instance->m_Generation = AllocateInstanceGeneration(collection);
        assert(collection->m_Instances[instance_index] == 0);
        collection->m_Instances[instance_index] = instance;

        InsertInstanceInLevelIndex(collection, instance);

        return instance;
    }

    void UndoNewInstance(Collection* collection, Instance* instance) {
        if (instance->m_Prototype != &EMPTY_PROTOTYPE) {
            dmResource::Release(collection->m_Factory, instance->m_Prototype);
        }
        EraseSwapLevelIndex(collection, instance);

        if (instance->m_Parent != INVALID_INSTANCE_INDEX)
        {
            Unlink(collection, instance);
        }

        uint32_t instance_index = instance->m_Index;
        operator delete ((void*)instance);
        collection->m_Instances[instance_index] = 0x0;
        collection->m_InstanceIndices.Push(instance_index);
        assert(collection->m_IDToInstance.Size() <= collection->m_InstanceIndices.Size());
    }

    CreateResult CreateComponents(Collection* collection, Instance* instance) {
        DM_PROFILE("CreateComponents");

        Prototype* proto = instance->m_Prototype;
        uint32_t components_created = 0;
        uint32_t next_component_instance_data = 0;
        if (proto->m_ComponentCount > 0xFFFF ) {
            dmLogWarning("Too many components in game object: %u (max is 65536)", proto->m_ComponentCount);
            return CREATE_RESULT_TOO_MANY_COMPONENTS;
        }
        CreateResult r = CREATE_RESULT_OK;
        for (uint32_t i = 0; i < proto->m_ComponentCount; ++i)
        {
            Prototype::Component* component = &proto->m_Components[i];
            ComponentType* component_type = component->m_Type;
            assert(component_type);

            DM_PROFILE_DYN(component_type->m_Name, 0);

            uintptr_t* component_instance_data = 0;
            if (component_type->m_InstanceHasUserData)
            {
                component_instance_data = &instance->m_ComponentInstanceUserData[next_component_instance_data++];
                *component_instance_data = 0;
            }
            assert(next_component_instance_data <= instance->m_ComponentInstanceUserDataCount);

            ComponentCreateParams params;
            params.m_Instance = GetInstanceHandle(collection, instance);
            params.m_Position = component->m_Position;
            params.m_Rotation = component->m_Rotation;
            params.m_Scale = component->m_Scale;
            params.m_ComponentIndex = i;
            params.m_Resource = component->m_Resource;
            params.m_World = collection->m_ComponentWorlds[component->m_TypeIndex];
            params.m_Context = component_type->m_Context;
            params.m_UserData = component_instance_data;
            params.m_PropertySet = component->m_PropertySet;
            CreateResult create_result =  component_type->m_CreateFunction(params);
            if (create_result == CREATE_RESULT_OK)
            {
                components_created++;
            }
            else
            {
                r = create_result;
                break;
            }
        }

        if (CREATE_RESULT_OK != r)
        {
            uint32_t next_component_instance_data = 0;
            for (uint32_t i = 0; i < components_created; ++i)
            {
                Prototype::Component* component = &proto->m_Components[i];
                ComponentType* component_type = component->m_Type;
                assert(component_type);
                uintptr_t* component_instance_data = 0;
                if (component_type->m_InstanceHasUserData)
                {
                    component_instance_data = &instance->m_ComponentInstanceUserData[next_component_instance_data++];
                }
                assert(next_component_instance_data <= instance->m_ComponentInstanceUserDataCount);

                ComponentDestroyParams params;
                params.m_Collection = collection->m_HCollection;
                params.m_Instance = GetInstanceHandle(collection, instance);
                params.m_World = collection->m_ComponentWorlds[component->m_TypeIndex];
                params.m_Context = component_type->m_Context;
                params.m_UserData = component_instance_data;
                component_type->m_DestroyFunction(params);
            }
        }

        return r;
    }

    static void DestroyComponents(Collection* collection, Instance* instance) {
        DM_PROFILE("DestroyComponents");

        HPrototype prototype = instance->m_Prototype;
        uint32_t next_component_instance_data = 0;
        for (uint32_t i = 0; i < prototype->m_ComponentCount; ++i)
        {
            Prototype::Component* component = &prototype->m_Components[i];
            ComponentType* component_type = component->m_Type;

            DM_PROFILE_DYN(component_type->m_Name, 0);

            uintptr_t* component_instance_data = 0;
            if (component_type->m_InstanceHasUserData)
            {
                component_instance_data = &instance->m_ComponentInstanceUserData[next_component_instance_data++];
            }
            assert(next_component_instance_data <= instance->m_ComponentInstanceUserDataCount);

            ComponentDestroyParams params;
            params.m_Collection = collection->m_HCollection;
            params.m_Instance = GetInstanceHandle(collection, instance);
            params.m_World = collection->m_ComponentWorlds[component->m_TypeIndex];
            params.m_Context = component_type->m_Context;
            params.m_UserData = component_instance_data;
            component_type->m_DestroyFunction(params);
        }
    }

    void* GetResource(Instance* instance)
    {
        return instance->m_Prototype == &EMPTY_PROTOTYPE ? 0 : instance->m_Prototype;
    }

    HInstance New(HCollection hcollection, const char* prototype_name) {
        Collection* collection = GetCollectionFromHandle(hcollection);
        if (!collection)
            return INVALID_GAME_OBJECT;
        Prototype* proto;
        dmResource::HFactory factory = collection->m_Factory;
        if (prototype_name != 0x0)
        {
            dmResource::Result error = dmResource::Get(factory, prototype_name, (void**)&proto);
            if (error != dmResource::RESULT_OK)
            {
                return 0;
            }
        }
        else
        {
            proto = &EMPTY_PROTOTYPE;
        }
        Instance* instance = NewInstance(collection, proto, prototype_name);
        if (instance != 0) {
            CreateResult result = CreateComponents(collection, instance);
            if (result != CREATE_RESULT_OK) {
                // We can not call Delete here. Delete call DestroyFunction for every component
                ReleaseIdentifier(collection, instance);
                UndoNewInstance(collection, instance);
                instance = 0;
            }
        } else if (proto != &EMPTY_PROTOTYPE) {
            dmResource::Release(factory, proto);
        }
        return GetInstanceHandle(collection, instance);
    }

    dmhash_t CreateInstanceId()
    {
        static uint32_t index = 0;
        //20 bytes: '/'' + 'instance' + uint32 + null terminator = 1 + 8 + 10 + 1
        char buffer[32] = { 0 };
        int length = dmSnPrintf(buffer, sizeof(buffer), ID_SEPARATOR_CHAR "instance%d", index);
        index += 1;
        return dmHashBuffer64(buffer, (uint32_t)length);
    }

    uint32_t AcquireInstanceIndex(HCollection hcollection)
    {
        Collection* collection = GetCollectionFromHandle(hcollection);
        if (!collection)
            return INVALID_INSTANCE_POOL_INDEX;

        dmMutex::Lock(collection->m_Mutex);
        uint32_t index = INVALID_INSTANCE_POOL_INDEX;
        if (collection->m_InstanceIdPool.Remaining() > 0)
        {
            index = collection->m_InstanceIdPool.Pop();
        }
        dmMutex::Unlock(collection->m_Mutex);

        return index;
    }

    void ReleaseInstanceIndex(uint32_t index, Collection* collection)
    {
        dmMutex::Lock(collection->m_Mutex);
        collection->m_InstanceIdPool.Push(index);
        dmMutex::Unlock(collection->m_Mutex);
    }

    void ReleaseInstanceIndex(uint32_t index, HCollection hcollection)
    {
        Collection* collection = GetCollectionFromHandle(hcollection);
        if (collection)
            ReleaseInstanceIndex(index, collection);
    }

    void AssignInstanceIndex(uint32_t index, Instance* instance)
    {
        if (instance != 0x0)
        {
            instance->m_IdentifierIndex = index;
        }
    }

    void AssignInstanceIndex(uint32_t index, HInstance hinstance)
    {
        Instance* instance = GetInstanceFromHandle(hinstance);
        if (instance)
            instance->m_IdentifierIndex = index;
    }

    static void GenerateUniqueCollectionInstanceId(Collection* collection, char* buf, uint32_t bufsize)
    {
        // global path
        const char* id_format = "%scollection%d";
        uint32_t index = 0;
        dmMutex::Lock(collection->m_Mutex);
        index = collection->m_GenCollectionInstanceCounter++;
        dmMutex::Unlock(collection->m_Mutex);
        dmSnPrintf(buf, bufsize, id_format, ID_SEPARATOR, index);
    }

    Result SetIdentifier(Collection* collection, Instance* instance, dmhash_t id)
    {
        if (collection->m_IDToInstance.Get(id))
            return RESULT_IDENTIFIER_IN_USE;

        if (instance->m_Identifier != UNNAMED_IDENTIFIER)
            return RESULT_IDENTIFIER_ALREADY_SET;

        HInstance hinstance = GetInstanceHandle(collection, instance);
        assert(hinstance != INVALID_GAME_OBJECT);
        instance->m_Identifier = id;
        collection->m_IDToInstance.Put(id, hinstance);

        assert(collection->m_IDToInstance.Size() <= collection->m_InstanceIndices.Size());
        return RESULT_OK;
    }

    Result SetIdentifier(HCollection hcollection, HInstance hinstance, dmhash_t id)
    {
        Collection* collection = GetCollectionFromHandle(hcollection);
        Instance* instance = GetInstanceFromHandle(collection, hinstance);
        return instance ? SetIdentifier(collection, instance, id) : RESULT_INVALID_INSTANCE;
    }

    void ReleaseIdentifier(Collection* collection, Instance* instance)
    {
        if (instance->m_Identifier != UNNAMED_IDENTIFIER) {
            collection->m_IDToInstance.Erase(instance->m_Identifier);
            instance->m_Identifier = UNNAMED_IDENTIFIER;
        }
    }

    // Schedule instance to be added to update
    static void AddToUpdate(Collection* collection, Instance* instance)
    {
        // NOTE: Do not add to update twice.
        assert(instance->m_ToBeAdded == 0);
        if (instance->m_ToBeDeleted) {
            return;
        }
        instance->m_ToBeAdded = 1;
        uint32_t index = instance->m_Index;
        uint32_t tail = collection->m_InstancesToAddTail;
        if (tail != INVALID_INSTANCE_INDEX) {
            Instance* tail_instance = collection->m_Instances[tail];
            tail_instance->m_NextToAdd = index;
        } else {
            collection->m_InstancesToAddHead = index;
        }
        collection->m_InstancesToAddTail = index;
    }

    // Actually add instance to update
    static bool DoAddToUpdate(Collection* collection, Instance* instance) {
        bool add_to_update_result = true;
        if (instance)
        {
            instance->m_ToBeAdded = 0;
            if (instance->m_ToBeDeleted == 0) {
                assert(collection->m_Instances[instance->m_Index] == instance);

                uint32_t next_component_instance_data = 0;
                Prototype* prototype = instance->m_Prototype;
                for (uint32_t i = 0; i < prototype->m_ComponentCount; ++i)
                {
                    Prototype::Component* component = &prototype->m_Components[i];
                    ComponentType* component_type = component->m_Type;

                    uintptr_t* component_instance_data = 0;
                    if (component_type->m_InstanceHasUserData)
                    {
                        component_instance_data = &instance->m_ComponentInstanceUserData[next_component_instance_data++];
                    }
                    assert(next_component_instance_data <= instance->m_ComponentInstanceUserDataCount);

                    if (component_type->m_AddToUpdateFunction)
                    {
                        ComponentAddToUpdateParams params;
                        params.m_Collection = collection->m_HCollection;
                        params.m_Instance = GetInstanceHandle(collection, instance);
                        params.m_World = collection->m_ComponentWorlds[component->m_TypeIndex];
                        params.m_Context = component_type->m_Context;
                        params.m_UserData = component_instance_data;
                        CreateResult result = component_type->m_AddToUpdateFunction(params);
                        if (result != CREATE_RESULT_OK)
                        {
                            add_to_update_result = false;
                        }
                    }
                }
            }
        }

        return add_to_update_result;
    }

    // Actually add all scheduled instances to the update
    static bool DoAddToUpdate(Collection* collection) {
        if (collection->m_InUpdate) {
            dmLogError("Instances can not be added to update during the update.");
            return false;
        }
        uint32_t index = collection->m_InstancesToAddHead;
        bool result = true;
        while (index != INVALID_INSTANCE_INDEX) {
            Instance* instance = collection->m_Instances[index];
            if (!DoAddToUpdate(collection, instance)) {
                result = false;
            }
            index = instance->m_NextToAdd;
            instance->m_NextToAdd = INVALID_INSTANCE_INDEX;
        }
        collection->m_InstancesToAddHead = INVALID_INSTANCE_INDEX;
        collection->m_InstancesToAddTail = INVALID_INSTANCE_INDEX;
        return result;
    }

    static bool SetScriptPropertiesFromBuffer(Collection* collection, Instance* instance, const char *prototype_name, HPropertyContainer property_container)
    {
        uint32_t next_component_instance_data = 0;
        Prototype::Component* components = instance->m_Prototype->m_Components;
        uint32_t count = instance->m_Prototype->m_ComponentCount;
        for (uint32_t i = 0; i < count; ++i) {
            Prototype::Component& component = components[i];
            ComponentType* component_type = component.m_Type;
            uintptr_t* component_instance_data = 0;
            if (component_type->m_InstanceHasUserData)
            {
                component_instance_data = &instance->m_ComponentInstanceUserData[next_component_instance_data++];
            }

            if (strcmp(component.m_Type->m_Name, "scriptc") == 0 && component.m_Type->m_SetPropertiesFunction != 0x0)
            {
                ComponentSetPropertiesParams params;
                params.m_Instance = GetInstanceHandle(collection, instance);
                params.m_UserData = component_instance_data;

                if (property_container)
                    params.m_PropertySet.m_UserData = (uintptr_t)dmGameObject::PropertyContainerCopy(property_container);
                else
                    params.m_PropertySet.m_UserData = 0;

                params.m_PropertySet.m_GetPropertyCallback = PropertyContainerGetPropertyCallback;
                params.m_PropertySet.m_FreeUserDataCallback = PropertyContainerDestroyCallback;
                PropertyResult result = component.m_Type->m_SetPropertiesFunction(params);
                if (result != PROPERTY_RESULT_OK)
                {
                    dmLogError("Could not load properties when spawning '%s'.", prototype_name);
                    return false;
                }
            }
        }
        return true;
    }

    // Supplied 'proto' will be released after this function is done.
    static Result SpawnInternal(Collection* collection, HPrototype proto, const char *prototype_name, dmhash_t id, HPropertyContainer property_container, const Point3& position, const Quat& rotation, const Vector3& scale, Instance** out_instance)
    {
        if (collection->m_ToBeDeleted) {
            dmLogWarning("Spawning is not allowed in the given collection because it is being deleted.");
            return RESULT_INVALID_OPERATION;
        }

        Instance* instance = dmGameObject::NewInstance(collection, proto, prototype_name);
        if (instance == 0) {
            return RESULT_OUT_OF_RESOURCES;
        }

        dmResource::IncRef(collection->m_Factory, proto);

        SetPosition(collection, instance, position);
        SetRotation(collection, instance, rotation);
        SetScale(collection, instance, scale);
        collection->m_WorldTransforms[instance->m_Index] = dmTransform::ToMatrix4(instance->m_Transform);

        dmHashInit64(&instance->m_CollectionPathHashState, true);
        dmHashUpdateBuffer64(&instance->m_CollectionPathHashState, ID_SEPARATOR, strlen(ID_SEPARATOR));

        Result result = SetIdentifier(collection, instance, id);
        if (result != RESULT_OK) {
            if (result == RESULT_IDENTIFIER_IN_USE)
            {
                dmLogError("The identifier '%s' is already in use.", dmHashReverseSafe64(id));
                UndoNewInstance(collection, instance);
                return result;
            }
            else
            {
                // the other non-ok result for SetIdentifier can only be RESULT_IDENTIFIER_ALREADY_SET, which means we can still continue here
                result = RESULT_OK;
            }
        }

        bool success = CreateComponents(collection, instance) == CREATE_RESULT_OK;
        if (!success) {
            ReleaseIdentifier(collection, instance);
            UndoNewInstance(collection, instance);
            return RESULT_UNABLE_TO_CREATE_COMPONENTS;
        }

        success = SetScriptPropertiesFromBuffer(collection, instance, prototype_name, property_container);
        
        if (!success) {
            result = RESULT_INVALID_PROPERTIES;
        }
        else if (!InitInstance(collection, instance))
        {
            dmLogError("Could not initialize when spawning %s.", prototype_name);
            success = false;
            result = RESULT_UNABLE_TO_INIT_INSTANCE;
        }

        if (success) {
            AddToUpdate(collection, instance);
            instance->m_Generated = 1;
            *out_instance = instance;
        }
        else
        {
            Delete(collection, instance, false);
        }

        return result;
    }

    Result Spawn(HCollection hcollection, HPrototype proto, const char *prototype_name, dmhash_t id, HPropertyContainer property_container, const Point3& position, const Quat& rotation, const Vector3& scale, HInstance* out_instance)
    {
        if (!out_instance)
            return RESULT_INVALID_OPERATION;

        *out_instance = INVALID_GAME_OBJECT;
        Collection* collection = GetCollectionFromHandle(hcollection);
        if (!collection)
            return RESULT_INVALID_INSTANCE;

        Instance* instance = 0;
        Result result = SpawnInternal(collection, proto, prototype_name, id, property_container, position, rotation, scale, &instance);
        *out_instance = GetInstanceHandle(collection, instance);
        return result;
    }

    static void Unlink(Collection* collection, Instance* instance)
    {
        // Unlink "me" from parent
        if (instance->m_Parent != INVALID_INSTANCE_INDEX)
        {
            assert(instance->m_Depth > 0);
            Instance* parent = collection->m_Instances[instance->m_Parent];
            uint32_t index = parent->m_FirstChildIndex;
            Instance* prev_child = 0;
            while (index != INVALID_INSTANCE_INDEX)
            {
                Instance* child = collection->m_Instances[index];
                if (child == instance)
                {
                    if (prev_child)
                        prev_child->m_SiblingIndex = child->m_SiblingIndex;
                    else
                        parent->m_FirstChildIndex = child->m_SiblingIndex;

                    break;
                }

                prev_child = child;
                index = collection->m_Instances[index]->m_SiblingIndex;
            }
            instance->m_SiblingIndex = INVALID_INSTANCE_INDEX;
            instance->m_Parent = INVALID_INSTANCE_INDEX;
        }
    }

    static void MoveUp(Collection* collection, Instance* instance)
    {
        /*
         * Move instance up in hierarchy
         */

        assert(instance->m_Depth > 0);
        EraseSwapLevelIndex(collection, instance);
        instance->m_Depth--;
        InsertInstanceInLevelIndex(collection, instance);
    }

    static void MoveAllUp(Collection* collection, Instance* instance)
    {
        /*
         * Move all children up in hierarchy
         */

        uint32_t index = instance->m_FirstChildIndex;
        while (index != INVALID_INSTANCE_INDEX)
        {
            Instance* child = collection->m_Instances[index];
            // NOTE: This assertion is only valid if we processes the tree depth first
            // The order of MoveAllUp and MoveUp below is imperative
            // NOTE: This assert is not possible when moving more than a single step. TODO: ?
            //assert(child->m_Depth == instance->m_Depth + 1);
            MoveAllUp(collection, child);
            MoveUp(collection, child);
            index = collection->m_Instances[index]->m_SiblingIndex;
        }
    }

    static void ReparentChildNodes(Collection* collection, Instance* instance)
    {
        // Reparent child nodes
        uint32_t index = instance->m_FirstChildIndex;
        while (index != INVALID_INSTANCE_INDEX)
        {
            Instance* child = collection->m_Instances[index];
            assert(child->m_Parent == instance->m_Index);
            child->m_Parent = instance->m_Parent;
            index = collection->m_Instances[index]->m_SiblingIndex;
        }

        // Add child nodes to parent
        if (instance->m_Parent != INVALID_INSTANCE_INDEX)
        {
            Instance* parent = collection->m_Instances[instance->m_Parent];
            uint32_t index = parent->m_FirstChildIndex;
            Instance* child = 0;
            while (index != INVALID_INSTANCE_INDEX)
            {
                child = collection->m_Instances[index];
                index = collection->m_Instances[index]->m_SiblingIndex;
            }

            // Child is last child if present
            if (child)
            {
                assert(child->m_SiblingIndex == INVALID_INSTANCE_INDEX);
                child->m_SiblingIndex = instance->m_FirstChildIndex;
            }
            else
            {
                assert(parent->m_FirstChildIndex == INVALID_INSTANCE_INDEX);
                parent->m_FirstChildIndex = instance->m_FirstChildIndex;
            }
        }
    }

    static Result CollectionSpawnFromDescInternal(Collection* collection, dmGameObjectDDF::CollectionDesc* collection_desc, 
        const char* id_prefix, InstancePropertyContainers *property_containers, InstanceIdMap *id_mapping, dmTransform::Transform const &transform)
    {
        // Path prefix for collection objects
        char root_path[32];
        HashState64 prefixHashState;
        dmHashInit64(&prefixHashState, true);
        if (!id_prefix)
        {
            GenerateUniqueCollectionInstanceId(collection, root_path, sizeof(root_path));
            dmHashUpdateBuffer64(&prefixHashState, root_path, strlen(root_path));
        }
        else
        {
            if (id_prefix[0] != *ID_SEPARATOR)
            {
                dmLogError("The id_prefix must start with the path specifier '%s', in this case: \"%s%s\"", ID_SEPARATOR, ID_SEPARATOR, id_prefix);
                return RESULT_IDENTIFIER_INVALID;
            }
            dmHashUpdateBuffer64(&prefixHashState, id_prefix, strlen(id_prefix));
        }

        // table for output ids
        uint32_t instance_count = collection_desc->m_Instances.m_Count;
        id_mapping->SetCapacity(dmMath::Max(1U, instance_count / 3), instance_count);

        dmArray<Instance*> new_instances;
        new_instances.SetCapacity(collection_desc->m_Instances.m_Count);

        Result result = RESULT_OK;

        for (uint32_t i = 0; i < collection_desc->m_Instances.m_Count; ++i)
        {
            const dmGameObjectDDF::InstanceDesc& instance_desc = collection_desc->m_Instances[i];
            Prototype* proto = 0x0;
            dmResource::HFactory factory = collection->m_Factory;
            dmGameObject::Instance* instance = 0x0;

            if (instance_desc.m_Prototype)
            {
                dmResource::Result error = dmResource::Get(factory, instance_desc.m_Prototype, (void**)&proto);
                if (error == dmResource::RESULT_OK) {
                    instance = dmGameObject::NewInstance(collection, proto, instance_desc.m_Prototype);
                    if (instance == 0) {
                        dmResource::Release(factory, proto);
                        result = RESULT_OUT_OF_RESOURCES;
                        break;
                    }
                }
            }

            if (!instance)
                continue;

            instance->m_Generated = 1;

            // support legacy pipeline which outputs 0 for Scale3 and scale in Scale
            Vector3 scale = instance_desc.m_Scale3;
            if (scale.getX() == 0 && scale.getY() == 0 && scale.getZ() == 0)
                    scale = Vector3(instance_desc.m_Scale, instance_desc.m_Scale, instance_desc.m_Scale);

            instance->m_Transform = dmTransform::Transform(Vector3(instance_desc.m_Position), instance_desc.m_Rotation, scale);
            dmHashClone64(&instance->m_CollectionPathHashState, &prefixHashState, true);

            const char* path_end = strrchr(instance_desc.m_Id, *ID_SEPARATOR);
            if (path_end == 0x0) {
                dmLogError("The id of %s has an incorrect format, missing path specifier.", instance_desc.m_Id);
                result = RESULT_IDENTIFIER_INVALID;
            } else {
                dmHashUpdateBuffer64(&instance->m_CollectionPathHashState, instance_desc.m_Id, path_end - instance_desc.m_Id + 1);
            }

            // Construct the full new path id and store in the id mapping table (mapping from prefixless
            // to with the root_path or id_prefix added)
            HashState64 new_id_hs;
            dmHashClone64(&new_id_hs, &prefixHashState, true);
            dmHashUpdateBuffer64(&new_id_hs, instance_desc.m_Id, strlen(instance_desc.m_Id));
            dmhash_t new_id = dmHashFinal64(&new_id_hs);
            dmhash_t id = dmHashBuffer64(instance_desc.m_Id, strlen(instance_desc.m_Id));
            id_mapping->Put(id, new_id);
            new_instances.Push(instance);

            Result r = dmGameObject::SetIdentifier(collection, instance, new_id);
            if (r != dmGameObject::RESULT_OK)
            {
                result = r;
                if (r == RESULT_IDENTIFIER_IN_USE)
                {
                    dmLogError("Unable to set identifier %s%s. Identifier already in use.", id_prefix ? id_prefix : root_path, instance_desc.m_Id);
                }
                else
                {
                    dmLogError("Unable to set identifier %s%s. %s", id_prefix ? id_prefix : root_path, instance_desc.m_Id, dmHashReverseSafe64(new_id));
                }
            }
        }
        dmHashRelease64(&prefixHashState);

        if (result == RESULT_OK)
        {
            // Setup hierarchy
            for (uint32_t i = 0; i < collection_desc->m_Instances.m_Count; ++i)
            {
                const dmGameObjectDDF::InstanceDesc& instance_desc = collection_desc->m_Instances[i];

                dmhash_t *parent_id = id_mapping->Get(dmHashString64(instance_desc.m_Id));
                assert(parent_id);

                dmGameObject::Instance* parent = dmGameObject::GetInstanceFromIdentifier(collection, *parent_id);
                assert(parent);

                for (uint32_t j = 0; j < instance_desc.m_Children.m_Count; ++j)
                {
                    dmhash_t child_id = GetAbsoluteIdentifier(parent, instance_desc.m_Children[j]);

                    // It is not always the case that 'parent' has had the path prefix prepended to its id, so it is necessary
                    // to see if a remapping exists.
                    dmhash_t *new_id = id_mapping->Get(child_id);
                    if (new_id)
                    {
                        child_id = *new_id;
                    }

                    dmGameObject::Instance* child = dmGameObject::GetInstanceFromIdentifier(collection, child_id);
                    if (child)
                    {
                        dmGameObject::Result r = dmGameObject::SetParent(collection, child, parent);
                        if (r != dmGameObject::RESULT_OK)
                        {
                            dmLogError("Unable to set %s as parent to %s (%d)", instance_desc.m_Id, instance_desc.m_Children[j], r);
                            result = r;
                        }
                    }
                    else
                    {
                        dmLogError("Child not found: %s", instance_desc.m_Children[j]);
                        result = RESULT_CHILD_NOT_FOUND;
                    }
                }
            }
        }

        // Exit point 1: Before components are created.
        if (result != RESULT_OK)
        {
            for (uint32_t i=0;i!=new_instances.Size();i++)
            {
                ReleaseIdentifier(collection, new_instances[i]);
                UndoNewInstance(collection, new_instances[i]);
            }
            id_mapping->Clear();
            return result;
        }

        // Update the transform for all parent-less objects
        for (uint32_t i=0;i!=new_instances.Size();i++)
        {
            if (!GetParent(collection, new_instances[i]))
            {
                new_instances[i]->m_Transform = dmTransform::Mul(transform, new_instances[i]->m_Transform);
            }

            // world transforms need to be up to date in time for the script init calls
            collection->m_WorldTransforms[new_instances[i]->m_Index] = dmTransform::ToMatrix4(new_instances[i]->m_Transform);
        }

        // Create components and set properties
        //
        // First set properties from the collection definition
        // Then look if there are any properties in the supplied property_buffers for the instance

        // After this point, instances are either removed (through undo) on error, or added
        // to the 'created' array from which they can be deleted on error.
        dmArray<Instance*> created;
        created.SetCapacity(collection_desc->m_Instances.m_Count);

        for (uint32_t i = 0; i < collection_desc->m_Instances.m_Count; ++i)
        {
            const dmGameObjectDDF::InstanceDesc& instance_desc = collection_desc->m_Instances[i];

            dmhash_t *instance_id = id_mapping->Get(dmHashString64(instance_desc.m_Id));
            assert(instance_id);

            dmGameObject::Instance* instance = dmGameObject::GetInstanceFromIdentifier(collection, *instance_id);
            bool success = dmGameObject::CreateComponents(collection, instance) == CREATE_RESULT_OK;
            if (success) {
                created.Push(instance);
                // Set properties
                uint32_t component_instance_data_index = 0;
                Prototype::Component* components = instance->m_Prototype->m_Components;
                uint32_t comp_count = instance->m_Prototype->m_ComponentCount;
                for (uint32_t comp_i = 0; comp_i < comp_count; ++comp_i)
                {
                    Prototype::Component& component = components[comp_i];
                    ComponentType* type = component.m_Type;
                    if (type->m_SetPropertiesFunction != 0x0)
                    {
                        if (!type->m_InstanceHasUserData)
                        {
                            DM_HASH_REVERSE_MEM(hash_ctx, 256);
                            dmLogError("Unable to set properties for the component '%s' in game object '%s' in collection '%s' since it has no ability to store them.", dmHashReverseSafe64Alloc(&hash_ctx, component.m_Id), instance_desc.m_Id, collection_desc->m_Name);
                            result = RESULT_INVALID_PROPERTIES;
                            break;
                        }

                        HPropertyContainer ddf_properties = 0x0;
                        uint32_t comp_prop_count = instance_desc.m_ComponentProperties.m_Count;
                        for (uint32_t prop_i = 0; prop_i < comp_prop_count; ++prop_i)
                        {
                            const dmGameObjectDDF::ComponentPropertyDesc& comp_prop = instance_desc.m_ComponentProperties[prop_i];
                            if (dmHashString64(comp_prop.m_Id) == component.m_Id)
                            {
                                ddf_properties = PropertyContainerCreateFromDDF(&comp_prop.m_PropertyDecls);
                                if (ddf_properties == 0x0)
                                {
                                    DM_HASH_REVERSE_MEM(hash_ctx, 256);
                                    dmLogError("Could not read properties parameters for the component '%s' in game object '%s' in collection '%s'.", dmHashReverseSafe64Alloc(&hash_ctx, component.m_Id), instance_desc.m_Id, collection_desc->m_Name);
                                    result = RESULT_INVALID_PROPERTIES;
                                }
                                break;
                            }
                        }

                        HPropertyContainer lua_properties = 0x0;
                        HPropertyContainer* instance_properties = property_containers->Get(dmHashString64(instance_desc.m_Id));
                        if (instance_properties != 0x0)
                        {
                            if (strcmp(type->m_Name, "scriptc") == 0)
                            {
                                // TODO: Investigate if it's enough to have one property set, (to save time/memory)
                                // and only register the Free function once (letting the first instance "own" it)
                                lua_properties = PropertyContainerCopy(*instance_properties);
                            }
                        }

                        if (result != RESULT_OK)
                        {
                            PropertyContainerDestroy(lua_properties);
                            PropertyContainerDestroy(ddf_properties);
                            break;
                        }

                        HPropertyContainer properties = 0x0;
                        if (ddf_properties != 0x0 && lua_properties !=0x0)
                        {
                            properties = PropertyContainerMerge(ddf_properties, lua_properties);
                            PropertyContainerDestroy(lua_properties);
                            PropertyContainerDestroy(ddf_properties);
                            if (properties == 0x0)
                            {
                                DM_HASH_REVERSE_MEM(hash_ctx, 256);
                                dmLogError("Could not merge properties parameters for the component '%s' in game object '%s' in collection '%s'", dmHashReverseSafe64Alloc(&hash_ctx, component.m_Id), instance_desc.m_Id, collection_desc->m_Name);
                                result = RESULT_INVALID_PROPERTIES;
                                break;
                            }
                        }
                        else
                        {
                            properties = ddf_properties ? ddf_properties : lua_properties;
                        }

                        ComponentSetPropertiesParams params;
                        params.m_Instance = GetInstanceHandle(collection, instance);

                        if (properties != 0x0)
                        {
                            params.m_PropertySet.m_GetPropertyCallback = PropertyContainerGetPropertyCallback;
                            params.m_PropertySet.m_FreeUserDataCallback = PropertyContainerDestroyCallback;
                            params.m_PropertySet.m_UserData = (uintptr_t)properties;
                        }

                        uintptr_t* component_instance_data = &instance->m_ComponentInstanceUserData[component_instance_data_index];
                        params.m_UserData = component_instance_data;

                        PropertyResult r = type->m_SetPropertiesFunction(params);
                        if (r != PROPERTY_RESULT_OK)
                        {
                            DM_HASH_REVERSE_MEM(hash_ctx, 256);
                            dmLogError("Could not load properties for component '%s' when spawning '%s' in collection '%s'.", dmHashReverseSafe64Alloc(&hash_ctx, component.m_Id), instance_desc.m_Id, collection_desc->m_Name);
                            PropertyContainerDestroy(properties);
                            result = RESULT_INVALID_PROPERTIES;
                            break;
                        }
                    }
                    if (component.m_Type->m_InstanceHasUserData)
                        ++component_instance_data_index;
                }
            } else {
                ReparentChildNodes(collection, instance);
                Unlink(collection, instance);
                MoveAllUp(collection, instance);

                ReleaseIdentifier(collection, instance);
                UndoNewInstance(collection, instance);
                result = RESULT_UNABLE_TO_CREATE_COMPONENTS;
            }
        }

        if (result == RESULT_OK)
        {
            for (uint32_t i=0;i!=created.Size();i++)
            {
                if (!InitInstance(collection, created[i]))
                {
                    DM_HASH_REVERSE_MEM(hash_ctx, 256);
                    dmLogError("Could not initialize instance '%s' in collection '%s'.", dmHashReverseSafe64Alloc(&hash_ctx, created[i]->m_Identifier), collection_desc->m_Name);
                    result = RESULT_UNABLE_TO_INIT_INSTANCE;
                    break;
                }
            }
        }

        if (result != RESULT_OK)
        {
            // Fail cleanup
            for (uint32_t i=0;i!=created.Size();i++)
                dmGameObject::Delete(collection, created[i], false);
            id_mapping->Clear();
            return result;
        }

        for (uint32_t i=0;i!=created.Size();i++)
        {
            AddToUpdate(collection, created[i]);
        }

        return result;
    }

    Result SpawnFromCollection(HCollection hcollection, HCollectionDesc collection_desc, const char* id_prefix, 
        InstancePropertyContainers *property_containers,
        const Point3& position, const Quat& rotation, const Vector3& scale,
        InstanceIdMap *out_instances)
    {
        Collection* collection = GetCollectionFromHandle(hcollection);
        if (!collection)
            return RESULT_INVALID_INSTANCE;
        dmTransform::Transform transform;
        transform.SetTranslation(Vector3(position));
        transform.SetRotation(rotation);
        transform.SetScale(scale);

        return CollectionSpawnFromDescInternal(collection, (dmGameObjectDDF::CollectionDesc*)collection_desc, id_prefix, property_containers, out_instances, transform);
    }

    HInstance Spawn(HCollection hcollection, HPrototype proto, const char* prototype_name, dmhash_t id, HPropertyContainer property_container, const Point3& position, const Quat& rotation, const Vector3& scale)
    {
        Collection* collection = GetCollectionFromHandle(hcollection);
        if (!collection)
            return INVALID_GAME_OBJECT;
        if (proto == 0x0) {
            dmLogError("No prototype to spawn from.");
            return INVALID_GAME_OBJECT;
        }

        Instance* instance;
        Result result = SpawnInternal(collection, proto, prototype_name, id, property_container, position, rotation, scale, &instance);
        if (result != RESULT_OK) {
            dmLogError("Could not spawn an instance of prototype %s.", prototype_name);
            return INVALID_GAME_OBJECT;
        }

        return GetInstanceHandle(collection, instance);
    }

    static void MoveDown(Collection* collection, Instance* instance)
    {
        /*
         * Move instance down in hierarchy
         */

        assert(instance->m_Depth < MAX_HIERARCHICAL_DEPTH - 1);
        EraseSwapLevelIndex(collection, instance);
        instance->m_Depth++;
        InsertInstanceInLevelIndex(collection, instance);
    }

    static void MoveAllDown(Collection* collection, Instance* instance)
    {
        /*
         * Move all children down in hierarchy
         */

        uint32_t index = instance->m_FirstChildIndex;
        while (index != INVALID_INSTANCE_INDEX)
        {
            Instance* child = collection->m_Instances[index];
            // NOTE: This assertion is only valid if we processes the tree depth first
            // The order of MoveAllUp and MoveUp below is imperative
            // NOTE: This assert is not possible when moving more than a single step. TODO: ?
            //assert(child->m_Depth == instance->m_Depth + 1);
            MoveAllDown(collection, child);
            MoveDown(collection, child);
            index = collection->m_Instances[index]->m_SiblingIndex;
        }
    }

    static bool InitComponents(Collection* collection, Instance* instance)
    {
        uint32_t next_component_instance_data = 0;
        Prototype* prototype = instance->m_Prototype;
        bool init_result = true;
        for (uint32_t i = 0; i < prototype->m_ComponentCount; ++i)
        {
            Prototype::Component* component = &prototype->m_Components[i];
            ComponentType* component_type = component->m_Type;

            uintptr_t* component_instance_data = 0;
            if (component_type->m_InstanceHasUserData)
            {
                component_instance_data = &instance->m_ComponentInstanceUserData[next_component_instance_data++];
            }
            assert(next_component_instance_data <= instance->m_ComponentInstanceUserDataCount);

            if (component_type->m_InitFunction)
            {
                ComponentInitParams params;
                params.m_Collection = collection->m_HCollection;
                params.m_Instance = GetInstanceHandle(collection, instance);
                params.m_World = collection->m_ComponentWorlds[component->m_TypeIndex];
                params.m_Context = component_type->m_Context;
                params.m_UserData = component_instance_data;
                CreateResult result = component_type->m_InitFunction(params);
                if (result != CREATE_RESULT_OK)
                {
                    init_result = false;
                }
            }
        }
        return init_result;
    }

    static bool InitInstance(Collection* collection, Instance* instance)
    {
        if (instance)
        {
            if (instance->m_Initialized)
            {
                dmLogWarning("Instance '%s' is initialized twice, this may lead to undefined behaviour.", dmHashReverseSafe64(instance->m_Identifier));
            }
            else
            {
                instance->m_Initialized = 1;
            }

            assert(collection->m_Instances[instance->m_Index] == instance);

            // Update world transforms since some components might need them in their init-callback
            Matrix4* trans = &collection->m_WorldTransforms[instance->m_Index];
            if (instance->m_Parent == INVALID_INSTANCE_INDEX)
            {
                *trans = dmTransform::ToMatrix4(instance->m_Transform);
            }
            else
            {
                const Matrix4* parent_trans = &collection->m_WorldTransforms[instance->m_Parent];
                *trans = (*parent_trans) * dmTransform::ToMatrix4(instance->m_Transform);
            }
            return InitComponents(collection, instance);
        }

        return true;
    }

    static bool DispatchMessages(Collection* collection, dmMessage::HSocket* sockets, uint32_t socket_count);


    bool IsCollectionInitialized(Collection* collection)
    {
        return collection->m_Initialized;
    }

    static bool InitCollection(Collection* collection)
    {
        DM_PROFILE("Init");
        assert(collection->m_InUpdate == 0 && "Initializing instances during Update(.) is not permitted");

        // Update transform cache
        UpdateTransforms(collection);

        bool result = true;
        // Mark the live instances before callbacks so instances spawned from an
        // init callback are not initialized as part of this pass.
        uint32_t last_instance_index = 0;
        bool has_instances = false;
        for (uint32_t level = 0; level < MAX_HIERARCHICAL_DEPTH; ++level)
        {
            const dmArray<uint32_t>& indices = collection->m_LevelIndices[level];
            for (uint32_t i = 0; i < indices.Size(); ++i)
            {
                uint32_t instance_index = indices[i];
                collection->m_Instances[instance_index]->m_InitSnapshot = 1;
                last_instance_index = dmMath::Max(last_instance_index, instance_index);
                has_instances = true;
            }
        }

        uint32_t instance_count = has_instances ? last_instance_index + 1 : 0;
        for (uint32_t i = 0; i < instance_count; ++i)
        {
            Instance* instance = collection->m_Instances[i];
            if (instance && instance->m_InitSnapshot && !InitInstance(collection, instance))
            {
                result = false;
            }
        }
        for (uint32_t i = 0; i < instance_count; ++i)
        {
            Instance* instance = collection->m_Instances[i];
            if (instance && instance->m_InitSnapshot)
            {
                instance->m_InitSnapshot = 0;
                if (!DoAddToUpdate(collection, instance))
                {
                    result = false;
                }
            }
        }
        dmMessage::HSocket sockets[] = {collection->m_ComponentSocket, collection->m_FrameSocket};
        if (!DispatchMessages(collection, sockets, 2))
            result = false;

        collection->m_Initialized = 1;
        return result;
    }

    bool Init(HCollection hcollection)
    {
        Collection* collection = GetCollectionFromHandle(hcollection);
        return collection ? InitCollection(collection) : false;
    }

    static bool FinalComponents(Collection* collection, Instance* instance)
    {
        uint32_t next_component_instance_data = 0;
        Prototype* prototype = instance->m_Prototype;
        for (uint32_t i = 0; i < prototype->m_ComponentCount; ++i)
        {
            Prototype::Component* component = &prototype->m_Components[i];
            ComponentType* component_type = component->m_Type;
            assert(component_type);

            uintptr_t* component_instance_data = 0;
            if (component_type->m_InstanceHasUserData)
            {
                component_instance_data = &instance->m_ComponentInstanceUserData[next_component_instance_data++];
            }
            assert(next_component_instance_data <= instance->m_ComponentInstanceUserDataCount);

            if (component_type->m_FinalFunction)
            {
                ComponentFinalParams params;
                params.m_Collection = collection->m_HCollection;
                params.m_Instance = GetInstanceHandle(collection, instance);
                params.m_World = collection->m_ComponentWorlds[component->m_TypeIndex];
                params.m_Context = component_type->m_Context;
                params.m_UserData = component_instance_data;
                CreateResult result = component_type->m_FinalFunction(params);
                if (result != CREATE_RESULT_OK)
                {
                    return false;
                }
            }
        }
        return true;
    }

    static bool FinalInstance(Collection* collection, Instance* instance)
    {
        if (instance)
        {
            if (instance->m_Initialized)
                instance->m_Initialized = 0;
            else
                dmLogWarning("%s", "Instance is finalized without being initialized, this may lead to undefined behaviour.");

            assert(collection->m_Instances[instance->m_Index] == instance);
            return FinalComponents(collection, instance);
        }

        return true;
    }

    static bool FinalCollection(Collection* collection)
    {
        DM_PROFILE("Final");
        assert(collection->m_InUpdate == 0 && "Finalizing instances during Update(.) is not permitted");

        bool result = true;
        uint32_t n_objects = collection->m_Instances.Size();
        for (uint32_t i = 0; i < n_objects; ++i)
        {
            Instance* instance = collection->m_Instances[i];
            if (instance != 0x0 && instance->m_Initialized && ! FinalInstance(collection, instance))
            {
                result = false;
            }
        }

        collection->m_Initialized = 0;
        return result;
    }

    bool Final(HCollection hcollection)
    {
        Collection* collection = GetCollectionFromHandle(hcollection);
        return collection ? FinalCollection(collection) : false;
    }

    void Delete(Collection* collection, Instance* instance, bool recursive)
    {
        assert(collection->m_Instances[instance->m_Index] == instance);

        // NOTE: Do not add for delete twice.
        if (instance->m_ToBeDeleted)
            return;

        // NOTE: No point in deleting when the collection is being deleted
        // Actually dangerous, since we can be anywhere in the sequential game object destruction
        if (collection->m_ToBeDeleted)
            return;

        // If recursive, Delete child hierarchy recursively, child to parent order (leaf first).
        if(recursive)
        {
            uint32_t childIndex = instance->m_FirstChildIndex;
            while (childIndex != INVALID_INSTANCE_INDEX)
            {
                Instance* child = collection->m_Instances[childIndex];
                assert(child->m_Parent == instance->m_Index);
                childIndex = child->m_SiblingIndex;
                Delete(collection, child, true);
            }
        }

        // Delete instance
        instance->m_ToBeDeleted = 1;

        uint32_t index = instance->m_Index;
        uint32_t tail = collection->m_InstancesToDeleteTail;
        if (tail != INVALID_INSTANCE_INDEX) {
            Instance* tail_instance = collection->m_Instances[tail];
            tail_instance->m_NextToDelete = index;
        } else {
            collection->m_InstancesToDeleteHead = index;
        }
        collection->m_InstancesToDeleteTail = index;
    }

    void Delete(HCollection hcollection, HInstance hinstance, bool recursive)
    {
        Collection* collection = GetCollectionFromHandle(hcollection);
        Instance* instance = GetInstanceFromHandle(collection, hinstance);
        if (instance)
            Delete(collection, instance, recursive);
    }

    static void RemoveFromAddToUpdate(Collection* collection, Instance* instance)
    {
        uint32_t index = instance->m_Index;
        assert(collection->m_InstancesToAddTail == index || instance->m_NextToAdd != INVALID_INSTANCE_INDEX);
        uint32_t* prev_index_ptr = &collection->m_InstancesToAddHead;
        uint32_t prev_index = *prev_index_ptr;
        while (prev_index != index) {
            prev_index_ptr = &collection->m_Instances[prev_index]->m_NextToAdd;
            if (collection->m_InstancesToAddTail == *prev_index_ptr) {
                collection->m_InstancesToAddTail = prev_index;
            }
            prev_index = *prev_index_ptr;
        }
        *prev_index_ptr = instance->m_NextToAdd;
        if (prev_index_ptr == &collection->m_InstancesToAddHead && *prev_index_ptr == INVALID_INSTANCE_INDEX) { // If we unlinked the last item
            collection->m_InstancesToAddTail = INVALID_INSTANCE_INDEX;
        }
        instance->m_NextToAdd = INVALID_INSTANCE_INDEX;
        instance->m_ToBeAdded = 0;
    }

    static void DoDeleteInstance(Collection* collection, Instance* instance)
    {
        DM_PROFILE("DoDeleteInstance");
        CancelAnimations(collection, GetInstanceHandle(collection, instance));
        if (instance->m_ToBeAdded) {
            RemoveFromAddToUpdate(collection, instance);
        }
        dmResource::HFactory factory = collection->m_Factory;
        Prototype* prototype = instance->m_Prototype;
        DestroyComponents(collection, instance);

        dmHashRelease64(&instance->m_CollectionPathHashState);
        if(instance->m_Generated)
        {
            dmScript::ReleaseHash(dmScript::GetLuaState(g_ScriptContext), instance->m_Identifier);
        }

        if (instance->m_IdentifierIndex < collection->m_InstanceIdPool.Capacity())
        {
            // The identifier (hash) for this gameobject comes from the pool!
            ReleaseInstanceIndex(instance->m_IdentifierIndex, collection);
        }
        ReleaseIdentifier(collection, instance);

        assert(collection->m_LevelIndices[instance->m_Depth].Size() > 0);
        assert(instance->m_LevelIndex < collection->m_LevelIndices[instance->m_Depth].Size());

        ReparentChildNodes(collection, instance);

        // Unlink "me" from parent
        Unlink(collection, instance);
        EraseSwapLevelIndex(collection, instance);
        MoveAllUp(collection, instance);

        if (instance->m_FirstChildIndex != INVALID_INSTANCE_INDEX)
        {
            collection->m_DirtyTransforms = 1;
        }

        if (prototype != &EMPTY_PROTOTYPE)
            dmResource::Release(factory, prototype);
        collection->m_InstanceIndices.Push(instance->m_Index);
        collection->m_Instances[instance->m_Index] = 0;

        // Erase from input stack
        HInstance hinstance = GetInstanceHandle(collection, instance);
        bool found_instance = false;
        for (uint32_t i = 0; i < collection->m_InputFocusStack.Size(); ++i)
        {
            if (collection->m_InputFocusStack[i] == hinstance)
            {
                found_instance = true;
            }
            if (found_instance)
            {
                if (i < collection->m_InputFocusStack.Size() - 1)
                {
                    collection->m_InputFocusStack[i] = collection->m_InputFocusStack[i+1];
                }
            }
        }
        if (found_instance)
        {
            collection->m_InputFocusStack.Pop();
        }

        DeallocInstance(instance);

        assert(collection->m_IDToInstance.Size() <= collection->m_InstanceIndices.Size());
    }

    void DeleteAll(HCollection hcollection)
    {
        Collection* collection = GetCollectionFromHandle(hcollection);
        if (!collection)
            return;
        for (uint32_t i = 0; i < collection->m_Instances.Size(); ++i)
        {
            Instance* instance = collection->m_Instances[i];
            if (instance)
            {
                Delete(collection, instance, false);
            }
        }
    }

    Result SetIdentifier(Collection* collection, Instance* instance, const char* identifier)
    {
        dmhash_t id = dmHashBuffer64(identifier, strlen(identifier));
        return SetIdentifier(collection, instance, id);
    }

    Result SetIdentifier(HCollection hcollection, HInstance hinstance, const char* identifier)
    {
        Collection* collection = GetCollectionFromHandle(hcollection);
        Instance* instance = GetInstanceFromHandle(collection, hinstance);
        return instance ? SetIdentifier(collection, instance, identifier) : RESULT_INVALID_INSTANCE;
    }

    dmhash_t GetIdentifier(HInstance hinstance)
    {
        Instance* instance = GetInstanceFromHandle(hinstance);
        return instance ? instance->m_Identifier : 0;
    }

    dmhash_t GetAbsoluteIdentifier(Instance* instance, const char* identifier)
    {
        // check for global id (/foo/bar)
        if (*identifier == *ID_SEPARATOR)
        {
            return dmHashBuffer64(identifier, strlen(identifier));
        }
        else
        {
            // Make a copy of the state.
            HashState64 tmp_state;
            dmHashClone64(&tmp_state, &instance->m_CollectionPathHashState, false);
            dmHashUpdateBuffer64(&tmp_state, identifier, strlen(identifier));
            return dmHashFinal64(&tmp_state);
        }
    }

    dmhash_t GetAbsoluteIdentifier(HInstance hinstance, const char* identifier)
    {
        Instance* instance = GetInstanceFromHandle(hinstance);
        return instance && identifier ? GetAbsoluteIdentifier(instance, identifier) : 0;
    }

    static HInstance GetInstanceHandleFromIdentifier(Collection* collection, dmhash_t identifier)
    {
        if (!collection)
            return INVALID_GAME_OBJECT;

        HInstance* hinstance = collection->m_IDToInstance.Get(identifier);
        return hinstance ? *hinstance : INVALID_GAME_OBJECT;
    }

    Instance* GetInstanceFromIdentifier(Collection* collection, dmhash_t identifier)
    {
        return GetInstanceFromHandle(collection, GetInstanceHandleFromIdentifier(collection, identifier));
    }

    HInstance GetInstanceFromIdentifier(HCollection hcollection, dmhash_t identifier)
    {
        return GetInstanceHandleFromIdentifier(GetCollectionFromHandle(hcollection), identifier);
    }

    bool IsValid(HInstance hinstance)
    {
        return GetInstanceFromHandle(hinstance) != 0;
    }

    Result GetComponentIndex(Instance* instance, dmhash_t component_id, uint16_t* component_index)
    {
        assert(instance != 0x0);
        for (uint32_t i = 0; i < instance->m_Prototype->m_ComponentCount; ++i)
        {
            Prototype::Component* component = &instance->m_Prototype->m_Components[i];
            if (component->m_Id == component_id)
            {
                *component_index = (uint16_t)i;
                return RESULT_OK;
            }
        }
        return RESULT_COMPONENT_NOT_FOUND;
    }

    Result GetComponentIndex(HInstance hinstance, dmhash_t component_id, uint16_t* component_index)
    {
        Instance* instance = GetInstanceFromHandle(hinstance);
        if (instance)
            return GetComponentIndex(instance, component_id, component_index);
        if (component_index)
            *component_index = 0;
        return RESULT_INVALID_INSTANCE;
    }

    Result GetComponentId(Instance* instance, uint16_t component_index, dmhash_t* component_id)
    {
        assert(instance != 0x0);
        if (component_index < instance->m_Prototype->m_ComponentCount)
        {
            *component_id = instance->m_Prototype->m_Components[component_index].m_Id;
            return RESULT_OK;
        }
        return RESULT_COMPONENT_NOT_FOUND;
    }

    Result GetComponentId(HInstance hinstance, uint16_t component_index, dmhash_t* component_id)
    {
        Instance* instance = GetInstanceFromHandle(hinstance);
        if (instance)
            return GetComponentId(instance, component_index, component_id);
        if (component_id)
            *component_id = 0;
        return RESULT_INVALID_INSTANCE;
    }

    Result GetComponent(Collection* collection, Instance* instance, dmhash_t component_id, uint32_t* component_type, HComponent* out_component, HComponentWorld* out_world)
    {
        // TODO: We should probably not store user-data sparse.
        // A lot of loops just to find user-data such as the code below
        assert(instance != 0x0);
        const Prototype::Component* components = instance->m_Prototype->m_Components;
        uint32_t n = instance->m_Prototype->m_ComponentCount;
        uint32_t component_instance_data = 0;
        for (uint32_t i = 0; i < n; ++i)
        {
            const Prototype::Component* component = &components[i];
            const ComponentType* type = component->m_Type;
            if (component->m_Id == component_id)
            {
                *component_type = component->m_TypeIndex;

                dmGameObject::HComponentInternal user_data = 0;
                if (type->m_InstanceHasUserData)
                {
                    user_data = instance->m_ComponentInstanceUserData[component_instance_data];
                }

                dmGameObject::HComponentWorld world = 0;
                if (type->m_GetFunction || (out_world != 0))
                {
                    world = collection->m_ComponentWorlds[component->m_TypeIndex];
                }

                if (type->m_GetFunction)
                {
                    ComponentGetParams params = {world, user_data};
                    *out_component = (dmGameObject::HComponent)type->m_GetFunction(params);
                }
                else
                {
                    *out_component = (dmGameObject::HComponent)user_data;
                }

                if (out_world != 0)
                {
                    *out_world = world;
                }
                return RESULT_OK;
            }

            if (type->m_InstanceHasUserData)
            {
                component_instance_data++;
            }
        }

        return RESULT_COMPONENT_NOT_FOUND;
    }

    Result GetComponent(HInstance hinstance, dmhash_t component_id, uint32_t* component_type, HComponent* out_component, HComponentWorld* out_world)
    {
        Collection* collection = 0;
        Instance* instance = GetInstanceFromHandle(hinstance, &collection);
        if (instance)
            return GetComponent(collection, instance, component_id, component_type, out_component, out_world);
        if (component_type)
            *component_type = 0;
        if (out_component)
            *out_component = 0;
        if (out_world)
            *out_world = 0;
        return RESULT_INVALID_INSTANCE;
    }

    void SetBone(Instance* instance, bool bone)
    {
        instance->m_Bone = bone;
        instance->m_Generated = 1;
    }

    bool IsBone(Instance* instance)
    {
        return instance->m_Bone;
    }

    void SetBone(HInstance hinstance, bool bone)
    {
        Instance* instance = GetInstanceFromHandle(hinstance);
        if (instance)
            SetBone(instance, bone);
    }

    bool IsBone(HInstance hinstance)
    {
        Instance* instance = GetInstanceFromHandle(hinstance);
        return instance ? IsBone(instance) : false;
    }

    static uint32_t DoSetBoneTransforms(Collection* collection, dmTransform::Transform* component_transform, uint32_t first_index, dmTransform::Transform* transforms, uint32_t transform_count)
    {
        if (transform_count == 0)
            return 0;
        uint32_t current_index = first_index;
        uint32_t count = 0;
        while (current_index != INVALID_INSTANCE_INDEX)
        {
            Instance* instance = collection->m_Instances[current_index];
            if (instance->m_Bone)
            {
                instance->m_Transform = transforms[count++];
                if (component_transform && count == 1)
                {
                    instance->m_Transform = dmTransform::Mul(*component_transform, instance->m_Transform);
                }
                if (count < transform_count)
                {
                    count += DoSetBoneTransforms(collection, 0x0, instance->m_FirstChildIndex, &transforms[count], transform_count - count);
                }
                if (transform_count == count)
                {
                    return count;
                }
            }
            current_index = instance->m_SiblingIndex;
        }
        return count;
    }

    uint32_t SetBoneTransforms(HInstance hinstance, dmTransform::Transform& component_transform, dmTransform::Transform* transforms, uint32_t transform_count)
    {
        Collection* collection = 0;
        Instance* instance = GetInstanceFromHandle(hinstance, &collection);
        if (!instance)
            return 0;
        uint32_t count = DoSetBoneTransforms(collection, &component_transform, instance->m_Index, transforms, transform_count);
        collection->m_DirtyTransforms |= count > 0 ? 1 : 0;
        return count;
    }

    static void DeleteBones(Collection* collection, uint32_t first_index) {
        uint32_t current_index = first_index;
        while (current_index != INVALID_INSTANCE_INDEX) {
            Instance* instance = collection->m_Instances[current_index];
            if (instance->m_Bone && instance->m_ToBeDeleted == 0) {
                DeleteBones(collection, instance->m_FirstChildIndex);
                // Delete children first, to avoid any unnecessary re-parenting
                Delete(collection, instance, false);
            }
            current_index = instance->m_SiblingIndex;
        }
    }

    void DeleteBones(HInstance hparent) {
        Collection* collection = 0;
        Instance* parent = GetInstanceFromHandle(hparent, &collection);
        if (parent)
            DeleteBones(collection, parent->m_FirstChildIndex);
    }

    struct DispatchMessagesContext
    {
        Collection* m_Collection;
        bool m_Success;
    };

    void DispatchMessagesFunction(dmMessage::Message* message, void* user_ptr)
    {
        DispatchMessagesContext* context = (DispatchMessagesContext*) user_ptr;
        Collection* collection = context->m_Collection;

        Instance* instance = GetInstanceFromIdentifier(collection, message->m_Receiver.m_Path);
        if (instance == 0x0)
        {
            DM_HASH_REVERSE_MEM(hash_ctx, 512);
            const dmMessage::URL* sender = &message->m_Sender;
            const char* socket_name = dmMessage::GetSocketName(sender->m_Socket);
            const char* path_name = dmHashReverseSafe64Alloc(&hash_ctx, sender->m_Path);
            const char* fragment_name = dmHashReverseSafe64Alloc(&hash_ctx, sender->m_Fragment);

            dmLogError("Instance '%s' could not be found when dispatching message '%s' sent from %s:%s#%s",
                        dmHashReverseSafe64Alloc(&hash_ctx, message->m_Receiver.m_Path),
                        dmHashReverseSafe64Alloc(&hash_ctx, message->m_Id),
                        socket_name, path_name, fragment_name);

            context->m_Success = false;
            return;
        }
        if (message->m_Descriptor != 0)
        {
            dmDDF::Descriptor* descriptor = (dmDDF::Descriptor*)message->m_Descriptor;
            if (descriptor == dmGameObjectDDF::AcquireInputFocus::m_DDFDescriptor)
            {
                dmGameObject::AcquireInputFocus(collection, instance);
                return;
            }
            else if (descriptor == dmGameObjectDDF::ReleaseInputFocus::m_DDFDescriptor)
            {
                dmGameObject::ReleaseInputFocus(collection, instance);
                return;
            }
            else if (descriptor == dmGameObjectDDF::SetParent::m_DDFDescriptor)
            {
                dmGameObjectDDF::SetParent* sp = (dmGameObjectDDF::SetParent*)message->m_Data;
                dmGameObject::Instance* parent = 0;
                if (sp->m_ParentId != 0)
                {
                    parent = dmGameObject::GetInstanceFromIdentifier(context->m_Collection, sp->m_ParentId);
                    if (parent == 0)
                        dmLogWarning("Could not find parent instance with id '%s'.", dmHashReverseSafe64(sp->m_ParentId));

                }
                uint32_t old_parent = instance->m_Parent;

                dmGameObject::Result result = dmGameObject::SetParent(collection, instance, parent);

                if (result == dmGameObject::RESULT_OK && old_parent != instance->m_Parent)
                {
                    Matrix4 parent_t = Matrix4::identity();
                    if (parent)
                    {
                        parent_t = collection->m_WorldTransforms[parent->m_Index];
                    }

                    if (sp->m_KeepWorldTransform == 0)
                    {
                        collection->m_WorldTransforms[instance->m_Index] = parent_t * dmTransform::ToMatrix4(instance->m_Transform);
                    }
                    else
                    {
                        instance->m_Transform = dmTransform::ToTransform(inverse(parent_t) * collection->m_WorldTransforms[instance->m_Index]);
                    }
                }

                if (result != dmGameObject::RESULT_OK)
                    dmLogWarning("Error when setting parent of '%s' to '%s', error: %i.",
                                 dmHashReverseSafe64(instance->m_Identifier),
                                 dmHashReverseSafe64(sp->m_ParentId),
                                 result);
                return;
            }
        }
        Prototype* prototype = instance->m_Prototype;

        if (message->m_Receiver.m_Fragment != 0)
        {
            uint16_t component_index;
            Result result = GetComponentIndex(instance, message->m_Receiver.m_Fragment, &component_index);
            if (result != RESULT_OK)
            {
                DM_HASH_REVERSE_MEM(hash_ctx, 512);
                const dmMessage::URL* sender = &message->m_Sender;
                const char* socket_name = dmMessage::GetSocketName(sender->m_Socket);
                const char* path_name = dmHashReverseSafe64Alloc(&hash_ctx, sender->m_Path);
                const char* fragment_name = dmHashReverseSafe64Alloc(&hash_ctx, sender->m_Fragment);

                dmLogError("Component '%s#%s' could not be found when dispatching message '%s' sent from %s:%s#%s",
                            dmHashReverseSafe64Alloc(&hash_ctx, message->m_Receiver.m_Path),
                            dmHashReverseSafe64Alloc(&hash_ctx, message->m_Receiver.m_Fragment),
                            dmHashReverseSafe64Alloc(&hash_ctx, message->m_Id),
                            socket_name, path_name, fragment_name);
                context->m_Success = false;
                return;
            }
            Prototype::Component* component = &prototype->m_Components[component_index];
            ComponentType* component_type = component->m_Type;
            assert(component_type);

            if (component_type->m_OnMessageFunction)
            {
                // TODO: Not optimal way to find index of component instance data
                uint32_t next_component_instance_data = 0;
                for (uint32_t i = 0; i < component_index; ++i)
                {
                    ComponentType* ct = prototype->m_Components[i].m_Type;
                    assert(component_type);
                    if (ct->m_InstanceHasUserData)
                    {
                        next_component_instance_data++;
                    }
                }

                uintptr_t* component_instance_data = 0;
                if (component_type->m_InstanceHasUserData)
                {
                    component_instance_data = &instance->m_ComponentInstanceUserData[next_component_instance_data];
                }
                {
                    DM_PROFILE("OnMessageFunction");
                    ComponentOnMessageParams params;
                    params.m_Instance = GetInstanceHandle(collection, instance);
                    params.m_World = collection->m_ComponentWorlds[component->m_TypeIndex];
                    params.m_Context = component_type->m_Context;
                    params.m_UserData = component_instance_data;
                    params.m_Message = message;
                    UpdateResult res = component_type->m_OnMessageFunction(params);
                    if (res != UPDATE_RESULT_OK)
                        context->m_Success = false;
                }
            }
            else
            {
                // TODO User-friendly error message here...
                dmLogWarning("Component type is missing OnMessage function");
            }
        }
        else // broadcast
        {
            uint32_t next_component_instance_data = 0;
            for (uint32_t i = 0; i < prototype->m_ComponentCount; ++i)
            {
                Prototype::Component* component = &prototype->m_Components[i];
                ComponentType* component_type = component->m_Type;
                assert(component_type);

                if (component_type->m_OnMessageFunction)
                {
                    uintptr_t* component_instance_data = 0;
                    if (component_type->m_InstanceHasUserData)
                    {
                        component_instance_data = &instance->m_ComponentInstanceUserData[next_component_instance_data++];
                    }
                    {
                        DM_PROFILE("OnMessageFunction");
                        ComponentOnMessageParams params;
                        params.m_Instance = GetInstanceHandle(collection, instance);
                        params.m_World = collection->m_ComponentWorlds[component->m_TypeIndex];
                        params.m_Context = component_type->m_Context;
                        params.m_UserData = component_instance_data;
                        params.m_Message = message;
                        UpdateResult res = component_type->m_OnMessageFunction(params);
                        if (res != UPDATE_RESULT_OK)
                            context->m_Success = false;
                    }
                }
                else
                {
                    if (component_type->m_InstanceHasUserData)
                    {
                        ++next_component_instance_data;
                    }
                }
            }
        }
    }

    static bool DispatchMessages(Collection* collection, dmMessage::HSocket* sockets, uint32_t socket_count)
    {
        DM_PROFILE("DispatchMessages");

        DispatchMessagesContext ctx;
        ctx.m_Collection = collection;
        ctx.m_Success = true;
        bool iterate = true;
        uint32_t iteration_count = 0;
        while (iterate && iteration_count < MAX_DISPATCH_ITERATION_COUNT)
        {
            iterate = false;
            for (uint32_t i = 0; i < socket_count; ++i)
            {
                if (!dmMessage::HasMessages(sockets[i]))
                {
                    continue; // no need to try to update or send anything
                }
                // Make sure the transforms are updated if we are about to dispatch messages
                if (collection->m_DirtyTransforms)
                {
                    UpdateTransforms(collection);
                }
                uint32_t message_count = dmMessage::Dispatch(sockets[i], &DispatchMessagesFunction, (void*) &ctx);
                if (message_count)
                {
                    iterate = true;
                }
            }
            ++iteration_count;
        }

        return ctx.m_Success;
    }

    bool DispatchMessages(HCollection hcollection, dmMessage::HSocket* sockets, uint32_t socket_count)
    {
        Collection* collection = GetCollectionFromHandle(hcollection);
        return collection ? DispatchMessages(collection, sockets, socket_count) : false;
    }

    static void UpdateEulerToRotation(Instance* instance);

    static inline bool Vec3Equals(const uint32_t* a, const uint32_t* b)
    {
        return a[0] == b[0] && a[1] == b[1] && a[2] == b[2];
    }

    static bool HasEulerChanged(Instance* instance)
    {
        Vector3& euler = instance->m_EulerRotation;
        Vector3& prev_euler = instance->m_PrevEulerRotation;
        return !Vec3Equals((uint32_t*)(&euler), (uint32_t*)(&prev_euler));
    }

    static void CheckEuler(Instance* instance)
    {
        if (HasEulerChanged(instance))
        {
            UpdateEulerToRotation(instance);
        }
    }

    void UpdateTransforms(Collection* collection)
    {
        DM_PROFILE("UpdateTransforms");

        // Calculate world transforms
        // First root-level instances
        dmArray<uint32_t>& root_level = collection->m_LevelIndices[0];
        uint32_t root_count = root_level.Size();
        for (uint32_t i = 0; i < root_count; ++i)
        {
            uint32_t index = root_level[i];
            Instance* instance = collection->m_Instances[index];
            CheckEuler(instance);
            collection->m_WorldTransforms[index] = dmTransform::ToMatrix4(instance->m_Transform);
            uint32_t parent_index = instance->m_Parent;
            assert(parent_index == INVALID_INSTANCE_INDEX);
        }

        for (uint32_t level_i = 1; level_i < MAX_HIERARCHICAL_DEPTH; ++level_i)
        {
            dmArray<uint32_t>& level = collection->m_LevelIndices[level_i];
            uint32_t instance_count = level.Size();
            for (uint32_t i = 0; i < instance_count; ++i)
            {
                uint32_t index = level[i];
                Instance* instance = collection->m_Instances[index];
                CheckEuler(instance);
                Matrix4* trans = &collection->m_WorldTransforms[index];

                uint32_t parent_index = instance->m_Parent;
                assert(parent_index != INVALID_INSTANCE_INDEX);

                Matrix4* parent_trans = &collection->m_WorldTransforms[parent_index];
                Matrix4 own = dmTransform::ToMatrix4(instance->m_Transform);
                *trans = *parent_trans * own;
            }
        }

        collection->m_DirtyTransforms = false;
    }

    void UpdateTransforms(HCollection hcollection)
    {
        Collection* collection = GetCollectionFromHandle(hcollection);
        if (collection)
            UpdateTransforms(collection);
    }

    void UpdateTransformsForInstance(Collection* collection, Instance* instance)
    {
        DM_PROFILE("UpdateTransformsForInstance");

        // Build ancestor chain from root to the target instance
        Instance* chain[MAX_HIERARCHICAL_DEPTH];
        uint32_t  count = 0;

        Instance* n = instance;
        while (n && count < MAX_HIERARCHICAL_DEPTH)
        {
            chain[count++] = n;
            if (n->m_Parent == INVALID_INSTANCE_INDEX)
                break;
            n = collection->m_Instances[n->m_Parent];
        }

        // Reverse iterate: parent first, then child, ... , target
        for (int32_t i = (int32_t)count - 1; i >= 0; --i)
        {
            Instance* cur = chain[i];
            CheckEuler(cur);

            Matrix4 own = dmTransform::ToMatrix4(cur->m_Transform);
            if (cur->m_Parent == INVALID_INSTANCE_INDEX)
            {
                collection->m_WorldTransforms[cur->m_Index] = own;
            }
            else
            {
                Matrix4& parent_world = collection->m_WorldTransforms[cur->m_Parent];
                collection->m_WorldTransforms[cur->m_Index] = parent_world * own;
            }
        }
        // Note: Do not modify collection->m_DirtyTransforms here; other branches remain stale.
    }

    enum UpdateFunctionType
    {
        UPDATE_FUNCTION_TYPE_FIXED_UPDATE,
        UPDATE_FUNCTION_TYPE_UPDATE,
        UPDATE_FUNCTION_TYPE_LATE_UPDATE
    };

    static bool UpdateComponentFunction(Collection* collection, uint32_t component_type_count, UpdateFunctionType function_type, ComponentsUpdateParams& update_params)
    {
        bool ret = true;
        for (uint32_t i = 0; i < component_type_count; ++i)
        {
            uint16_t update_index = collection->m_Register->m_ComponentTypesOrder[i];
            ComponentType* component_type = &collection->m_Register->m_ComponentTypes[update_index];

            // Avoid to call UpdateTransforms for each/all component types.
            if (component_type->m_ReadsTransforms && collection->m_DirtyTransforms)
            {
                UpdateTransforms(collection);
            }

            // TODO: Could be stored in an array in component type, and just use the function type as index
            ComponentsUpdate func = 0;
            switch(function_type)
            {
                case UPDATE_FUNCTION_TYPE_UPDATE:
                {
                    func = component_type->m_UpdateFunction;
                    break;
                }
                case UPDATE_FUNCTION_TYPE_FIXED_UPDATE:
                {
                    func = component_type->m_FixedUpdateFunction;
                    break;
                }
                case UPDATE_FUNCTION_TYPE_LATE_UPDATE:
                {
                    func = component_type->m_LateUpdateFunction;
                    break;
                }
            }

            if (func)
            {
                DM_PROFILE_DYN(component_type->m_Name, 0);
                update_params.m_World = collection->m_ComponentWorlds[update_index];
                update_params.m_Context = component_type->m_Context;

                ComponentsUpdateResult update_result;
                update_result.m_TransformsUpdated = false;
                UpdateResult res = func(update_params, update_result);
                if (res != UPDATE_RESULT_OK)
                    ret = false;

                // Mark the collections transforms as dirty if this component has updated
                // them in its update function.
                if (update_result.m_TransformsUpdated)
                {
                    collection->m_DirtyTransforms = 1;
                }
            }

            if (!DispatchMessages(collection, &collection->m_ComponentSocket, 1))
            {
                ret = false;
            }
        }

        return ret;
    }

    static bool Update(Collection* collection, const UpdateContext* update_context)
    {
        DM_PROFILE("Update");
        DM_PROPERTY_ADD_U32(rmtp_GOInstances, collection->m_InstanceIndices.Size());

        assert(collection != 0x0);

        // Add to update
        DoAddToUpdate(collection);

        collection->m_InUpdate = 1;

        bool ret = true;


        UpdateContext dynamic_update_context;
        dynamic_update_context = *update_context;

        // pass it as unscaled time
        if (update_context->m_TimeScale > 0.001f )
        {
            dynamic_update_context.m_AccumFrameTime = collection->m_FixedAccumTime / update_context->m_TimeScale;
        }
        else
        {
            dynamic_update_context.m_AccumFrameTime = collection->m_FixedAccumTime;
        }

        uint32_t num_fixed_steps = 0;
        UpdateContext fixed_update_context;
        if (update_context->m_FixedUpdateFrequency != 0 && update_context->m_TimeScale > 0.001f)
        {
            if (collection->m_FirstUpdate)
            {
                collection->m_FirstUpdate = 0;
                collection->m_FixedAccumTime = update_context->m_AccumFrameTime * update_context->m_TimeScale;
            }

            const float time = collection->m_FixedAccumTime + update_context->m_DT; // Add the scaled time
            const float fixed_frequency = update_context->m_FixedUpdateFrequency;
            // If the proxy is slowed down, we want e.g. the physics to be slowed down as well
            const float fixed_dt = (1.0f / (float)fixed_frequency) * update_context->m_TimeScale;
            num_fixed_steps = (uint32_t)(time / fixed_dt);
            // Store the remainder for the next frame
            collection->m_FixedAccumTime = time - (num_fixed_steps * fixed_dt);

            if (num_fixed_steps != 0)
            {
                fixed_update_context = dynamic_update_context;
                fixed_update_context.m_DT = fixed_dt;
            }
        }

        /* The overall function order is as follows:
        *
        * - For each function "UpdateFn" in ["Update", "LateUpdateFn]:
        *   - For each component type:
        *       - Call type->UpdateFn()
        *       - Flush messages (if necessary)
        *       - Update transforms (if necessary)
        *
        * Note that FixedUpdate is only called if the project has the setting enabled.
        * In that case, the loop looks like above, but with one extra function in the list:
        *
        * - For each function "UpdateFn" in ["FixedUpdate", "Update", "LateUpdateFn]:
        *   - same as above: for each component type...
        *
        * When using fixed physics update, we call into the fixed update functions for each component type.
        * Currently, only the Script and CollisionObject components support this function.
        *
        * To summarize, the default update loop looks like:
        *
        *     script update(), animation, ..., physics update, ...
        *
        * With fixed update enabled, the update loop looks like:
        *
        *     [script fixed_update(), physics fixed update], animation, ...,
        */

        ComponentsUpdateParams update_params;
        update_params.m_Collection = collection->m_HCollection;
        update_params.m_UpdateContext = &dynamic_update_context;

        ComponentsUpdateParams fixed_update_params;
        fixed_update_params.m_Collection = collection->m_HCollection;
        fixed_update_params.m_UpdateContext = &fixed_update_context;

        uint32_t component_type_count = collection->m_Register->m_ComponentTypeCount;

        // See gamesys.cpp for list of priorities for each component type.
        // These priorities ensure the update order between components.
        // I.e. collectionproxy, script, animation, collision ...

        // 1. for each fixed step, call component's fixed update
        //      - Lua fixed_update() (comp_script.cpp)
        //      - CompCollisionObjectFixedUpdate() (comp_collision_object.cpp)
        // Keep running subsequent update phases after a component error. The engine
        // still renders the collection, and late update prepares component render data.
        for (uint32_t step = 0; step < num_fixed_steps; ++step)
        {
            if (!UpdateComponentFunction(collection, component_type_count, UPDATE_FUNCTION_TYPE_FIXED_UPDATE, fixed_update_params))
                ret = false;
        }

        // 2. call component's regular update
        if (!UpdateComponentFunction(collection, component_type_count, UPDATE_FUNCTION_TYPE_UPDATE, update_params))
            ret = false;

        // 3. call component's late update
        if (!UpdateComponentFunction(collection, component_type_count, UPDATE_FUNCTION_TYPE_LATE_UPDATE, update_params))
            ret = false;

        collection->m_InUpdate = 0;
        if (collection->m_DirtyTransforms)
        {
            UpdateTransforms(collection);
        }

        return ret;
    }

    bool Update(HCollection hcollection, const UpdateContext* update_context)
    {
        Collection* collection = GetCollectionFromHandle(hcollection);
        return collection ? Update(collection, update_context) : false;
    }

    bool Render(HCollection hcollection)
    {
        DM_PROFILE("Render");

        Collection* collection = GetCollectionFromHandle(hcollection);
        if (!collection)
            return false;

        bool ret = true;
        uint32_t component_types = collection->m_Register->m_ComponentTypeCount;
        for (uint32_t i = 0; i < component_types; ++i)
        {
            uint16_t update_index = collection->m_Register->m_ComponentTypesOrder[i];
            ComponentType* component_type = &collection->m_Register->m_ComponentTypes[update_index];
            if (component_type->m_RenderFunction)
            {
                DM_PROFILE_DYN(component_type->m_Name, 0);
                ComponentsRenderParams params;
                params.m_Collection = hcollection;
                params.m_World = collection->m_ComponentWorlds[update_index];
                params.m_Context = component_type->m_Context;
                UpdateResult res = component_type->m_RenderFunction(params);
                if (res != UPDATE_RESULT_OK)
                    ret = false;
            }
        }
        return ret;
    }

    static bool DispatchAllSockets(Collection* collection) {
        bool result = true;
        dmMessage::HSocket sockets[] =
        {
                // Some components might have sent messages in their final()
                collection->m_ComponentSocket,
                // Frame dispatch, handle e.g. spawning
                collection->m_FrameSocket
        };
        if (!DispatchMessages(collection, sockets, 2))
            result = false;
        return result;
    }

    static bool PostUpdate(Collection* collection)
    {
        DM_PROFILE("PostUpdate");
        assert(collection != 0x0);
        HContext gocontext = collection->m_Register;
        assert(gocontext);

        bool result = true;

        uint32_t component_types = gocontext->m_ComponentTypeCount;
        for (uint32_t i = 0; i < component_types; ++i)
        {
            uint16_t update_index = gocontext->m_ComponentTypesOrder[i];
            ComponentType* component_type = &gocontext->m_ComponentTypes[update_index];

            if (component_type->m_PostUpdateFunction)
            {
                DM_PROFILE_DYN(component_type->m_Name, 0);
                ComponentsPostUpdateParams params;
                params.m_Collection = collection->m_HCollection;
                params.m_World = collection->m_ComponentWorlds[update_index];
                params.m_Context = component_type->m_Context;
                UpdateResult res = component_type->m_PostUpdateFunction(params);
                if (res != UPDATE_RESULT_OK && result)
                    result = false;
            }
        }

        uint32_t instances_deleted = 0;

        if (collection->m_InstancesToDeleteHead != INVALID_INSTANCE_INDEX) {
            // Arbitrary max pass count to only guard for unexpected cycles and infinite hangs, see clause after while
            uint32_t max_pass_count = 10;
            uint32_t pass_count = 0;
            while (collection->m_InstancesToDeleteHead != INVALID_INSTANCE_INDEX && pass_count < max_pass_count) {
                ++pass_count;
                // Save the list and clear the head and tail
                uint32_t head = collection->m_InstancesToDeleteHead;
                collection->m_InstancesToDeleteHead = INVALID_INSTANCE_INDEX;
                collection->m_InstancesToDeleteTail = INVALID_INSTANCE_INDEX;

                uint32_t index = head;
                while (index != INVALID_INSTANCE_INDEX) {
                    Instance* instance = collection->m_Instances[index];

                    assert(collection->m_Instances[instance->m_Index] == instance);
                    assert(instance->m_ToBeDeleted);
                    if (instance->m_Initialized) {
                        if (!FinalInstance(collection, instance) && result) {
                            result = false;
                        }
                    }
                    index = instance->m_NextToDelete;
                }

                if (!DispatchAllSockets(collection)) {
                    result = false;
                }

                // Reset to iterate for actual deletion
                index = head;
                while (index != INVALID_INSTANCE_INDEX) {
                    Instance* instance = collection->m_Instances[index];

                    assert(collection->m_Instances[instance->m_Index] == instance);
                    assert(instance->m_ToBeDeleted);
                    index = instance->m_NextToDelete;
                    DoDeleteInstance(collection, instance);
                    ++instances_deleted;
                }
            }
            if (pass_count == max_pass_count) {
                dmLogWarning("Creation/deletion cycles encountered, postponing to next frame to avoid infinite hang.");
            }
        } else {
            // Dispatch messages even if there are no deletion happening
            if (!DispatchAllSockets(collection)) {
                result = false;
            }
        }

        DM_PROPERTY_ADD_U32(rmtp_GODeleted, instances_deleted);

        return result;
    }

    bool PostUpdate(HCollection hcollection)
    {
        Collection* collection = GetCollectionFromHandle(hcollection);
        return collection ? PostUpdate(collection) : false;
    }

    bool PostUpdate(HContext gocontext)
    {
        DM_PROFILE("PostUpdateRegister");

        assert(gocontext != 0x0);

        bool result = true;

        DM_MUTEX_SCOPED_LOCK(gocontext->m_Mutex);
        uint32_t collection_count = gocontext->m_Collections.Size();
        uint32_t i = 0;
        while (i < collection_count)
        {
            Collection* collection = gocontext->m_Collections[i];
            if (collection->m_ToBeDeleted)
            {
                DeleteCollection(collection);
                --collection_count;
            }
            else
            {
                ++i;
            }
        }

        return result;
    }

    UpdateResult DispatchInput(Collection* collection, InputAction* input_actions, uint32_t input_action_count)
    {
        DM_PROFILE("DispatchInput");

        // iterate stacks from top to bottom
        for (uint32_t i = 0; i < input_action_count; ++i)
        {
            InputAction& input_action = input_actions[i];
            if (input_action.m_ActionId != 0 || input_action.m_PositionSet || input_action.m_AccelerationSet)
            {
                uint32_t stack_size = collection->m_InputFocusStack.Size();
                for (uint32_t k = 0; k < stack_size; ++k)
                {
                    HInstance hinstance = collection->m_InputFocusStack[stack_size - 1 - k];
                    Instance* instance = GetInstanceFromHandle(collection, hinstance);
                    if (!instance)
                        continue;
                    Prototype* prototype = instance->m_Prototype;
                    uint32_t components_size = prototype->m_ComponentCount;

                    InputResult res = INPUT_RESULT_IGNORED;
                    uint32_t next_component_instance_data = 0;
                    for (uint32_t l = 0; l < components_size; ++l)
                    {
                        ComponentType* component_type = prototype->m_Components[l].m_Type;
                        assert(component_type);
                        if (component_type->m_OnInputFunction)
                        {
                            uintptr_t* component_instance_data = 0;
                            if (component_type->m_InstanceHasUserData)
                            {
                                component_instance_data = &instance->m_ComponentInstanceUserData[next_component_instance_data];
                            }
                            ComponentOnInputParams params;
                            params.m_Instance = GetInstanceHandle(collection, instance);
                            params.m_InputAction = &input_action;
                            params.m_Context = component_type->m_Context;
                            params.m_UserData = component_instance_data;
                            InputResult comp_res = component_type->m_OnInputFunction(params);
                            if (comp_res == INPUT_RESULT_CONSUMED)
                                res = comp_res;
                            else if (comp_res == INPUT_RESULT_UNKNOWN_ERROR)
                                return UPDATE_RESULT_UNKNOWN_ERROR;
                        }
                        if (component_type->m_InstanceHasUserData)
                        {
                            next_component_instance_data++;
                        }
                    }
                    if (res == INPUT_RESULT_CONSUMED)
                    {
                        memset(&input_action, 0, sizeof(InputAction));
                        input_action.m_Consumed = 1;
                        break;
                    }
                }
            }
        }
        return UPDATE_RESULT_OK;
    }

    UpdateResult DispatchInput(HCollection hcollection, InputAction* input_actions, uint32_t input_action_count)
    {
        Collection* collection = GetCollectionFromHandle(hcollection);
        return collection ? DispatchInput(collection, input_actions, input_action_count) : UPDATE_RESULT_UNKNOWN_ERROR;
    }

    void AcquireInputFocus(Collection* collection, Instance* instance)
    {
        HInstance hinstance = GetInstanceHandle(collection, instance);
        bool found = false;
        for (uint32_t i = 0; i < collection->m_InputFocusStack.Size(); ++i)
        {
            if (collection->m_InputFocusStack[i] == hinstance)
            {
                found = true;
            }
            if (found && i < collection->m_InputFocusStack.Size() - 1)
            {
                collection->m_InputFocusStack[i] = collection->m_InputFocusStack[i + 1];
            }
        }
        if (found)
        {
            collection->m_InputFocusStack.Pop();
        }
        if (!collection->m_InputFocusStack.Full())
        {
            collection->m_InputFocusStack.Push(hinstance);
        }
        else
        {
            dmLogWarning("Input focus could not be acquired since the buffer is full (%d).", collection->m_InputFocusStack.Size());
        }
    }

    void ReleaseInputFocus(Collection* collection, Instance* instance)
    {
        HInstance hinstance = GetInstanceHandle(collection, instance);
        bool found = false;
        for (uint32_t i = 0; i < collection->m_InputFocusStack.Size(); ++i)
        {
            if (collection->m_InputFocusStack[i] == hinstance)
            {
                found = true;
            }
            if (found && i < collection->m_InputFocusStack.Size() - 1)
            {
                collection->m_InputFocusStack[i] = collection->m_InputFocusStack[i + 1];
            }
        }
        if (found)
        {
            collection->m_InputFocusStack.Pop();
        }
    }

    void AcquireInputFocus(HCollection hcollection, HInstance hinstance)
    {
        Collection* collection = GetCollectionFromHandle(hcollection);
        Instance* instance = GetInstanceFromHandle(collection, hinstance);
        if (instance)
            AcquireInputFocus(collection, instance);
    }

    void ReleaseInputFocus(HCollection hcollection, HInstance hinstance)
    {
        Collection* collection = GetCollectionFromHandle(hcollection);
        Instance* instance = GetInstanceFromHandle(collection, hinstance);
        if (instance)
            ReleaseInputFocus(collection, instance);
    }

    dmResource::HFactory GetFactory(HCollection hcollection)
    {
        Collection* collection = GetCollectionFromHandle(hcollection);
        return collection ? collection->m_Factory : 0;
    }

    HCollection GetCollection(HInstance hinstance)
    {
        Collection* collection = 0;
        return GetInstanceFromHandle(hinstance, &collection) ? collection->m_HCollection : INVALID_COLLECTION;
    }

    dmResource::HFactory GetFactory(HInstance hinstance)
    {
        Collection* collection = 0;
        return GetInstanceFromHandle(hinstance, &collection) ? collection->m_Factory : 0;
    }

    HContext GetGameObjectContext(HCollection hcollection)
    {
        Collection* collection = GetCollectionFromHandle(hcollection);
        return collection ? collection->m_Register : 0;
    }

    dmMessage::HSocket GetMessageSocket(HCollection hcollection)
    {
        Collection* collection = GetCollectionFromHandle(hcollection);
        return collection ? collection->m_ComponentSocket : 0;
    }

    dmMessage::HSocket GetFrameMessageSocket(HCollection hcollection)
    {
        Collection* collection = GetCollectionFromHandle(hcollection);
        return collection ? collection->m_FrameSocket : 0;
    }

    void SetPosition(Collection* collection, Instance* instance, Point3 position)
    {
        instance->m_Transform.SetTranslation(Vector3(position));
        collection->m_DirtyTransforms = 1;
    }

    Point3 GetPosition(Instance* instance)
    {
        return Point3(instance->m_Transform.GetTranslation());
    }

    void SetRotation(Collection* collection, Instance* instance, Quat rotation)
    {
        instance->m_Transform.SetRotation(rotation);
        collection->m_DirtyTransforms = 1;
    }

    Quat GetRotation(Instance* instance)
    {
        return instance->m_Transform.GetRotation();
    }

    void SetScale(Collection* collection, Instance* instance, float scale)
    {
        instance->m_Transform.SetUniformScale(scale);
        collection->m_DirtyTransforms = 1;
    }

    void SetScale(Collection* collection, Instance* instance, Vector3 scale)
    {
        instance->m_Transform.SetScale(scale);
        collection->m_DirtyTransforms = 1;
    }

    void SetScaleXY(Collection* collection, Instance* instance, float scale_x, float scale_y)
    {
        instance->m_Transform.SetScaleXY(scale_x, scale_y);
        collection->m_DirtyTransforms = 1;
    }

    float GetUniformScale(Instance* instance)
    {
        return instance->m_Transform.GetUniformScale();
    }

    Vector3 GetScale(Instance* instance)
    {
        return instance->m_Transform.GetScale();
    }

    Point3 GetWorldPosition(Collection* collection, Instance* instance)
    {
        Vector4 translation = collection->m_WorldTransforms[instance->m_Index].getCol(3);
        return Point3(translation.getX(), translation.getY(), translation.getZ());
    }

    Quat GetWorldRotation(Collection* collection, Instance* instance)
    {
        Matrix4 world_transform = collection->m_WorldTransforms[instance->m_Index];
        dmTransform::ResetScale(&world_transform);
        return Quat(world_transform.getUpper3x3());
    }

    float GetWorldUniformScale(Collection* collection, Instance* instance)
    {
        Vector3 scale = GetWorldScale(collection, instance);
        return dmMath::Max<float>(scale.getX(), dmMath::Max<float>(scale.getY(), scale.getZ()));
    }

    Vector3 GetWorldScale(Collection* collection, Instance* instance)
    {
        return dmTransform::ExtractScale(collection->m_WorldTransforms[instance->m_Index]);
    }

    /*
        dmTransform::ToMatrix a dmTransform::Transform from the world transform.
        When this is not possible, nonsense will be returned.
    */
    dmTransform::Transform GetWorldTransform(Collection* collection, Instance* instance)
    {
        Matrix4 mtx = collection->m_WorldTransforms[instance->m_Index];
        return dmTransform::ToTransform(mtx);
    }

    const Matrix4 & GetWorldMatrix(Collection* collection, Instance* instance)
    {
        return collection->m_WorldTransforms[instance->m_Index];
    }

    Result SetParent(Collection* collection, Instance* child, Instance* parent)
    {
        if (parent == 0 && child->m_Parent == INVALID_INSTANCE_INDEX)
            return RESULT_OK;

        if (parent != 0 && parent->m_Depth >= MAX_HIERARCHICAL_DEPTH-1)
        {
            dmLogError("Unable to set parent to child. Parent at maximum depth %d", MAX_HIERARCHICAL_DEPTH-1);
            return RESULT_MAXIMUM_HIEARCHICAL_DEPTH;
        }

        if (parent != 0)
        {
            uint32_t index = parent->m_Index;
            while (index != INVALID_INSTANCE_INDEX)
            {
                Instance* i = collection->m_Instances[index];

                if (i == child)
                {
                    dmLogError("Unable to set parent to child. Child is present in tree above parent. Unsupported");
                    return RESULT_INVALID_OPERATION;

                }
                index = i->m_Parent;
            }
            assert(collection->m_LevelIndices[child->m_Depth+1].Size() < collection->m_Instances.Size());
        }
        else
        {
            assert(collection->m_LevelIndices[0].Size() < collection->m_Instances.Size());
        }

        if (child->m_Parent != INVALID_INSTANCE_INDEX)
        {
            Unlink(collection, child);
        }

        // Root instances may carry a stale sibling link from a deleted parent.
        child->m_SiblingIndex = INVALID_INSTANCE_INDEX;

        EraseSwapLevelIndex(collection, child);

        // Add child to parent
        if (parent != 0)
        {
            if (parent->m_FirstChildIndex == INVALID_INSTANCE_INDEX)
            {
                parent->m_FirstChildIndex = child->m_Index;
            }
            else
            {
                Instance* first_child = collection->m_Instances[parent->m_FirstChildIndex];
                assert(parent->m_Depth == first_child->m_Depth - 1);

                child->m_SiblingIndex = first_child->m_Index;
                parent->m_FirstChildIndex = child->m_Index;
            }
        }

        int original_child_depth = child->m_Depth;
        if (parent != 0)
        {
            child->m_Parent = parent->m_Index;
            child->m_Depth = parent->m_Depth + 1;
        }
        else
        {
            child->m_Parent = INVALID_INSTANCE_INDEX;
            child->m_Depth = 0;
        }
        InsertInstanceInLevelIndex(collection, child);

        int32_t n_steps =  (int32_t) original_child_depth - (int32_t) child->m_Depth;
        if (n_steps < 0)
        {
            for (int i = 0; i < -n_steps; ++i)
            {
                MoveAllDown(collection, child);
            }
        }
        else
        {
            for (int i = 0; i < n_steps; ++i)
            {
                MoveAllUp(collection, child);
            }
        }

        collection->m_DirtyTransforms = 1;
        return RESULT_OK;
    }

    Instance* GetParent(Collection* collection, Instance* instance)
    {
        if (instance->m_Parent == INVALID_INSTANCE_INDEX)
        {
            return 0;
        }
        else
        {
            return collection->m_Instances[instance->m_Parent];
        }
    }

    uint32_t GetDepth(Instance* instance)
    {
        return instance->m_Depth;
    }

    uint32_t GetChildCount(Collection* collection, Instance* instance)
    {
        uint32_t count = 0;
        uint32_t index = instance->m_FirstChildIndex;
        while (index != INVALID_INSTANCE_INDEX)
        {
            ++count;
            index = collection->m_Instances[index]->m_SiblingIndex;
        }

        return count;
    }

    bool IsChildOf(Collection* collection, Instance* child, Instance* parent)
    {
        uint32_t index = parent->m_FirstChildIndex;
        while (index != INVALID_INSTANCE_INDEX)
        {
            Instance*i = collection->m_Instances[index];
            if (i == child)
                return true;
            index = collection->m_Instances[index]->m_SiblingIndex;
        }

        return false;
    }

    void SetPosition(HInstance hinstance, Point3 position)
    {
        Collection* collection = 0;
        Instance* instance = GetInstanceFromHandle(hinstance, &collection);
        if (instance)
            SetPosition(collection, instance, position);
    }

    Point3 GetPosition(HInstance hinstance)
    {
        Instance* instance = GetInstanceFromHandle(hinstance);
        return instance ? GetPosition(instance) : Point3(0.0f, 0.0f, 0.0f);
    }

    void SetRotation(HInstance hinstance, Quat rotation)
    {
        Collection* collection = 0;
        Instance* instance = GetInstanceFromHandle(hinstance, &collection);
        if (instance)
            SetRotation(collection, instance, rotation);
    }

    Quat GetRotation(HInstance hinstance)
    {
        Instance* instance = GetInstanceFromHandle(hinstance);
        return instance ? GetRotation(instance) : Quat::identity();
    }

    void SetScale(HInstance hinstance, float scale)
    {
        Collection* collection = 0;
        Instance* instance = GetInstanceFromHandle(hinstance, &collection);
        if (instance)
            SetScale(collection, instance, scale);
    }

    void SetScale(HInstance hinstance, Vector3 scale)
    {
        Collection* collection = 0;
        Instance* instance = GetInstanceFromHandle(hinstance, &collection);
        if (instance)
            SetScale(collection, instance, scale);
    }

    void SetScaleXY(HInstance hinstance, float scale_x, float scale_y)
    {
        Collection* collection = 0;
        Instance* instance = GetInstanceFromHandle(hinstance, &collection);
        if (instance)
            SetScaleXY(collection, instance, scale_x, scale_y);
    }

    float GetUniformScale(HInstance hinstance)
    {
        Instance* instance = GetInstanceFromHandle(hinstance);
        return instance ? GetUniformScale(instance) : 1.0f;
    }

    Vector3 GetScale(HInstance hinstance)
    {
        Instance* instance = GetInstanceFromHandle(hinstance);
        return instance ? GetScale(instance) : Vector3(1.0f, 1.0f, 1.0f);
    }

    Point3 GetWorldPosition(HInstance hinstance)
    {
        Collection* collection = 0;
        Instance* instance = GetInstanceFromHandle(hinstance, &collection);
        return instance ? GetWorldPosition(collection, instance) : Point3(0.0f, 0.0f, 0.0f);
    }

    Quat GetWorldRotation(HInstance hinstance)
    {
        Collection* collection = 0;
        Instance* instance = GetInstanceFromHandle(hinstance, &collection);
        return instance ? GetWorldRotation(collection, instance) : Quat::identity();
    }

    float GetWorldUniformScale(HInstance hinstance)
    {
        Collection* collection = 0;
        Instance* instance = GetInstanceFromHandle(hinstance, &collection);
        return instance ? GetWorldUniformScale(collection, instance) : 1.0f;
    }

    Vector3 GetWorldScale(HInstance hinstance)
    {
        Collection* collection = 0;
        Instance* instance = GetInstanceFromHandle(hinstance, &collection);
        return instance ? GetWorldScale(collection, instance) : Vector3(1.0f, 1.0f, 1.0f);
    }

    dmTransform::Transform GetWorldTransform(HInstance hinstance)
    {
        Collection* collection = 0;
        Instance* instance = GetInstanceFromHandle(hinstance, &collection);
        dmTransform::Transform transform;
        transform.SetIdentity();
        return instance ? GetWorldTransform(collection, instance) : transform;
    }

    const Matrix4& GetWorldMatrix(HInstance hinstance)
    {
        static const Matrix4 identity = Matrix4::identity();
        Collection* collection = 0;
        Instance* instance = GetInstanceFromHandle(hinstance, &collection);
        return instance ? GetWorldMatrix(collection, instance) : identity;
    }

    Result SetParent(HInstance hchild, HInstance hparent)
    {
        Collection* collection = 0;
        Instance* child = GetInstanceFromHandle(hchild, &collection);
        Instance* parent = hparent == INVALID_GAME_OBJECT ? 0 : GetInstanceFromHandle(collection, hparent);
        if (!child || (hparent != INVALID_GAME_OBJECT && !parent))
            return RESULT_INVALID_INSTANCE;
        return SetParent(collection, child, parent);
    }

    HInstance GetParent(HInstance hinstance)
    {
        Collection* collection = 0;
        Instance* instance = GetInstanceFromHandle(hinstance, &collection);
        return GetInstanceHandle(collection, instance ? GetParent(collection, instance) : 0);
    }

    uint32_t GetDepth(HInstance hinstance)
    {
        Instance* instance = GetInstanceFromHandle(hinstance);
        return instance ? GetDepth(instance) : 0;
    }

    uint32_t GetChildCount(HInstance hinstance)
    {
        Collection* collection = 0;
        Instance* instance = GetInstanceFromHandle(hinstance, &collection);
        return instance ? GetChildCount(collection, instance) : 0;
    }

    bool IsChildOf(HInstance hchild, HInstance hparent)
    {
        Collection* collection = 0;
        Instance* child = GetInstanceFromHandle(hchild, &collection);
        Instance* parent = GetInstanceFromHandle(collection, hparent);
        return child && parent && IsChildOf(collection, child, parent);
    }

    static void UpdateRotationToEuler(Instance* instance)
    {
        Quat q = instance->m_Transform.GetRotation();
        instance->m_EulerRotation = dmVMath::QuatToEuler(q.getX(), q.getY(), q.getZ(), q.getW());
        instance->m_PrevEulerRotation = instance->m_EulerRotation;
    }

    static void UpdateEulerToRotation(Instance* instance)
    {
        instance->m_PrevEulerRotation = instance->m_EulerRotation;
        instance->m_Transform.SetRotation(dmVMath::EulerToQuat(instance->m_EulerRotation));
    }

    PropertyResult GetProperty(Collection* collection, Instance* instance, dmhash_t component_id, dmhash_t property_id, PropertyOptions options, PropertyDesc& out_value)
    {
        if (instance == 0)
            return PROPERTY_RESULT_INVALID_INSTANCE;

        out_value.m_ValueType = dmGameObject::PROP_VALUE_ARRAY;
        out_value.m_ArrayLength = 0;

        if (component_id == 0)
        {
            out_value.m_ValuePtr = 0x0;

            // Scale used to be a uniform scalar, but is now a non-uniform 3-component scale
            if (property_id == PROP_SCALE)
            {
                float* scale = instance->m_Transform.GetScalePtr();
                out_value.m_ValuePtr = scale;
                out_value.m_ElementIds[0] = PROP_SCALE_X;
                out_value.m_ElementIds[1] = PROP_SCALE_Y;
                out_value.m_ElementIds[2] = PROP_SCALE_Z;
                out_value.m_Variant = PropertyVar(instance->m_Transform.GetScale());
            }
            else if (property_id == PROP_SCALE_XY)
            {
                float* scale = instance->m_Transform.GetScalePtr();
                out_value.m_ValuePtr = scale;
                out_value.m_ElementIds[0] = PROP_SCALE_X;
                out_value.m_ElementIds[1] = PROP_SCALE_Y;
                out_value.m_ElementIds[2] = 0;
                Vector3 vec = instance->m_Transform.GetScale();
                vec.setZ(1.0f);
                out_value.m_Variant = PropertyVar(vec);
            }
            else if (property_id == PROP_SCALE_X)
            {
                float* scale = instance->m_Transform.GetScalePtr();
                out_value.m_ValuePtr = scale;
                out_value.m_Variant = PropertyVar(*out_value.m_ValuePtr);
            }
            else if (property_id == PROP_SCALE_Y)
            {
                float* scale = instance->m_Transform.GetScalePtr();
                out_value.m_ValuePtr = scale + 1;
                out_value.m_Variant = PropertyVar(*out_value.m_ValuePtr);
            }
            else if (property_id == PROP_SCALE_Z)
            {
                float* scale = instance->m_Transform.GetScalePtr();
                out_value.m_ValuePtr = scale + 2;
                out_value.m_Variant = PropertyVar(*out_value.m_ValuePtr);
            }
            else if (property_id == PROP_POSITION)
            {
                float* position = instance->m_Transform.GetPositionPtr();
                out_value.m_ValuePtr = position;
                out_value.m_ElementIds[0] = PROP_POSITION_X;
                out_value.m_ElementIds[1] = PROP_POSITION_Y;
                out_value.m_ElementIds[2] = PROP_POSITION_Z;
                out_value.m_Variant = PropertyVar(instance->m_Transform.GetTranslation());
            }
            else if (property_id == PROP_POSITION_X)
            {
                float* position = instance->m_Transform.GetPositionPtr();
                out_value.m_ValuePtr = position;
                out_value.m_Variant = PropertyVar(*out_value.m_ValuePtr);
            }
            else if (property_id == PROP_POSITION_Y)
            {
                float* position = instance->m_Transform.GetPositionPtr();
                out_value.m_ValuePtr = position + 1;
                out_value.m_Variant = PropertyVar(*out_value.m_ValuePtr);
            }
            else if (property_id == PROP_POSITION_Z)
            {
                float* position = instance->m_Transform.GetPositionPtr();
                out_value.m_ValuePtr = position + 2;
                out_value.m_Variant = PropertyVar(*out_value.m_ValuePtr);
            }
            else if (property_id == PROP_ROTATION)
            {
                if (HasEulerChanged(instance))
                {
                    UpdateEulerToRotation(instance);
                }
                float* rotation = instance->m_Transform.GetRotationPtr();
                out_value.m_ValuePtr = rotation;
                out_value.m_ElementIds[0] = PROP_ROTATION_X;
                out_value.m_ElementIds[1] = PROP_ROTATION_Y;
                out_value.m_ElementIds[2] = PROP_ROTATION_Z;
                out_value.m_ElementIds[3] = PROP_ROTATION_W;
                out_value.m_Variant = PropertyVar(instance->m_Transform.GetRotation());
            }
            else if (property_id == PROP_ROTATION_X)
            {
                if (HasEulerChanged(instance))
                {
                    UpdateEulerToRotation(instance);
                }
                float* rotation = instance->m_Transform.GetRotationPtr();
                out_value.m_ValuePtr = rotation;
                out_value.m_Variant = PropertyVar(*out_value.m_ValuePtr);
            }
            else if (property_id == PROP_ROTATION_Y)
            {
                if (HasEulerChanged(instance))
                {
                    UpdateEulerToRotation(instance);
                }
                float* rotation = instance->m_Transform.GetRotationPtr();
                out_value.m_ValuePtr = rotation + 1;
                out_value.m_Variant = PropertyVar(*out_value.m_ValuePtr);
            }
            else if (property_id == PROP_ROTATION_Z)
            {
                if (HasEulerChanged(instance))
                {
                    UpdateEulerToRotation(instance);
                }
                float* rotation = instance->m_Transform.GetRotationPtr();
                out_value.m_ValuePtr = rotation + 2;
                out_value.m_Variant = PropertyVar(*out_value.m_ValuePtr);
            }
            else if (property_id == PROP_ROTATION_W)
            {
                if (HasEulerChanged(instance))
                {
                    UpdateEulerToRotation(instance);
                }
                float* rotation = instance->m_Transform.GetRotationPtr();
                out_value.m_ValuePtr = rotation + 3;
                out_value.m_Variant = PropertyVar(*out_value.m_ValuePtr);
            }
            else if (property_id == PROP_EULER)
            {
                if (!HasEulerChanged(instance))
                {
                    UpdateRotationToEuler(instance);
                }
                out_value.m_ValuePtr = (float*)&instance->m_EulerRotation;
                out_value.m_ElementIds[0] = PROP_EULER_X;
                out_value.m_ElementIds[1] = PROP_EULER_Y;
                out_value.m_ElementIds[2] = PROP_EULER_Z;
                out_value.m_Variant = PropertyVar(instance->m_EulerRotation);
            }
            else if (property_id == PROP_EULER_X)
            {
                if (!HasEulerChanged(instance))
                {
                    UpdateRotationToEuler(instance);
                }
               out_value.m_ValuePtr = ((float*)&instance->m_EulerRotation);
                out_value.m_Variant = PropertyVar(*out_value.m_ValuePtr);
            }
            else if (property_id == PROP_EULER_Y)
            {
                if (!HasEulerChanged(instance))
                {
                    UpdateRotationToEuler(instance);
                }
                out_value.m_ValuePtr = ((float*)&instance->m_EulerRotation) + 1;
                out_value.m_Variant = PropertyVar(*out_value.m_ValuePtr);
            }
            else if (property_id == PROP_EULER_Z)
            {
                if (!HasEulerChanged(instance))
                {
                    UpdateRotationToEuler(instance);
                }
                out_value.m_ValuePtr = ((float*)&instance->m_EulerRotation) + 2;
                out_value.m_Variant = PropertyVar(*out_value.m_ValuePtr);
            }
            if (out_value.m_ValuePtr != 0x0)
            {
                return PROPERTY_RESULT_OK;
            }
            else
            {
                return PROPERTY_RESULT_NOT_FOUND;
            }
        }
        else
        {
            uint16_t component_index;
            if (RESULT_OK == GetComponentIndex(instance, component_id, &component_index))
            {
                Prototype::Component* components = instance->m_Prototype->m_Components;
                Prototype::Component& component = components[component_index];
                ComponentType* type = component.m_Type;
                if (type->m_GetPropertyFunction)
                {
                    uintptr_t* user_data = 0;
                    if (type->m_InstanceHasUserData)
                    {
                        uint32_t next_component_instance_data = 0;
                        for (uint32_t i = 0; i < component_index; ++i)
                        {
                            if (components[i].m_Type->m_InstanceHasUserData)
                                ++next_component_instance_data;
                        }
                        user_data = &instance->m_ComponentInstanceUserData[next_component_instance_data];
                    }
                    ComponentGetPropertyParams p;
                    p.m_Context = type->m_Context;
                    p.m_World = collection->m_ComponentWorlds[component.m_TypeIndex];
                    p.m_Instance = GetInstanceHandle(collection, instance);
                    p.m_PropertyId = property_id;
                    p.m_Options = &options;
                    p.m_UserData = user_data;
                    PropertyDesc prop_desc;
                    PropertyResult result = type->m_GetPropertyFunction(p, prop_desc);
                    if (result == PROPERTY_RESULT_OK)
                    {
                        out_value = prop_desc;
                    }
                    return result;
                }
                else
                {
                    return PROPERTY_RESULT_NOT_FOUND;
                }
            }
            else
            {
                return PROPERTY_RESULT_COMP_NOT_FOUND;
            }
        }
    }

    PropertyResult GetPropertyAsHash(Collection* collection, Instance* instance, dmhash_t component_id, dmhash_t property_id, dmhash_t* out_value)
    {
        PropertyOptions options;
        PropertyDesc out_prop;
        PropertyResult result = GetProperty(collection, instance, component_id, property_id, options, out_prop);
        if (result == PROPERTY_RESULT_OK)
        {
            if (PROPERTY_TYPE_HASH == out_prop.m_Variant.m_Type)
            {
                *out_value = out_prop.m_Variant.m_Hash;
            }
            else
            {
                result = PROPERTY_RESULT_TYPE_MISMATCH;
            }
        }
        return result;
    }

    PropertyResult GetPropertyAsFloat(Collection* collection, Instance* instance, dmhash_t component_id, dmhash_t property_id, float* out_value)
    {
        PropertyOptions options;
        PropertyDesc out_prop;
        PropertyResult result = GetProperty(collection, instance, component_id, property_id, options, out_prop);
        if (result == PROPERTY_RESULT_OK)
        {
            if (PROPERTY_TYPE_NUMBER == out_prop.m_Variant.m_Type)
            {
                *out_value = out_prop.m_Variant.m_Number;
            }
            else
            {
                result = PROPERTY_RESULT_TYPE_MISMATCH;
            }
        }
        return result;
    }

    PropertyResult GetPropertyAsVector3(Collection* collection, Instance* instance, dmhash_t component_id, dmhash_t property_id, dmVMath::Vector3* out_value)
    {
        PropertyOptions options;
        PropertyDesc out_prop;
        PropertyResult result = GetProperty(collection, instance, component_id, property_id, options, out_prop);
        if (result == PROPERTY_RESULT_OK)
        {
            if (PROPERTY_TYPE_VECTOR3 == out_prop.m_Variant.m_Type)
            {
                out_value->setX(out_prop.m_Variant.m_V4[0]);
                out_value->setY(out_prop.m_Variant.m_V4[1]);
                out_value->setZ(out_prop.m_Variant.m_V4[2]);
            }
            else
            {
                result = PROPERTY_RESULT_TYPE_MISMATCH;
            }
        }
        return result;
    }

    PropertyResult GetPropertyAsVector4(Collection* collection, Instance* instance, dmhash_t component_id, dmhash_t property_id, dmVMath::Vector4* out_value)
    {
        PropertyOptions options;
        PropertyDesc out_prop;
        PropertyResult result = GetProperty(collection, instance, component_id, property_id, options, out_prop);
        if (result == PROPERTY_RESULT_OK)
        {
            if (PROPERTY_TYPE_VECTOR4 == out_prop.m_Variant.m_Type)
            {
                out_value->setX(out_prop.m_Variant.m_V4[0]);
                out_value->setY(out_prop.m_Variant.m_V4[1]);
                out_value->setZ(out_prop.m_Variant.m_V4[2]);
                out_value->setW(out_prop.m_Variant.m_V4[3]);
            }
            else
            {
                result = PROPERTY_RESULT_TYPE_MISMATCH;
            }
        }
        return result;
    }

    PropertyResult GetPropertyAsQuat(Collection* collection, Instance* instance, dmhash_t component_id, dmhash_t property_id, dmVMath::Quat* out_value)
    {
        PropertyOptions options;
        PropertyDesc out_prop;
        PropertyResult result = GetProperty(collection, instance, component_id, property_id, options, out_prop);
        if (result == PROPERTY_RESULT_OK)
        {
            if (PROPERTY_TYPE_QUAT == out_prop.m_Variant.m_Type)
            {
                out_value->setX(out_prop.m_Variant.m_V4[0]);
                out_value->setY(out_prop.m_Variant.m_V4[1]);
                out_value->setZ(out_prop.m_Variant.m_V4[2]);
                out_value->setW(out_prop.m_Variant.m_V4[3]);
            }
            else
            {
                result = PROPERTY_RESULT_TYPE_MISMATCH;
            }
        }
        return result;
    }

    PropertyResult GetPropertyAsBool(Collection* collection, Instance* instance, dmhash_t component_id, dmhash_t property_id, bool* out_value)
    {
        PropertyOptions options;
        PropertyDesc out_prop;
        PropertyResult result = GetProperty(collection, instance, component_id, property_id, options, out_prop);
        if (result == PROPERTY_RESULT_OK)
        {
            if (PROPERTY_TYPE_BOOLEAN == out_prop.m_Variant.m_Type)
            {
                *out_value = out_prop.m_Variant.m_Bool;
            }
            else
            {
                result = PROPERTY_RESULT_TYPE_MISMATCH;
            }
        }
        return result;
    }

    PropertyResult GetPropertyAsURL(Collection* collection, Instance* instance, dmhash_t component_id, dmhash_t property_id, dmMessage::URL* out_value)
    {
        PropertyOptions options;
        PropertyDesc out_prop;
        PropertyResult result = GetProperty(collection, instance, component_id, property_id, options, out_prop);
        if (result == PROPERTY_RESULT_OK)
        {
            if (PROPERTY_TYPE_URL == out_prop.m_Variant.m_Type)
            {
                dmMessage::URL* url = (dmMessage::URL*) out_prop.m_Variant.m_URL;
                out_value->m_Socket = url->m_Socket;
                out_value->_reserved = url->_reserved;
                out_value->m_Path = url->m_Path;
                out_value->m_Fragment = url->m_Fragment;
            }
            else
            {
                result = PROPERTY_RESULT_TYPE_MISMATCH;
            }
        }
        return result;
    }

    PropertyResult GetPropertyAsText(Collection* collection, Instance* instance, dmhash_t component_id, dmhash_t property_id, const char** out_value)
    {
        PropertyOptions options;
        PropertyDesc out_prop;
        PropertyResult result = GetProperty(collection, instance, component_id, property_id, options, out_prop);
        if (result == PROPERTY_RESULT_OK)
        {
            if (PROPERTY_TYPE_TEXT == out_prop.m_Variant.m_Type)
            {
                *out_value = out_prop.m_Variant.m_Text;
            }
            else
            {
                result = PROPERTY_RESULT_TYPE_MISMATCH;
            }
        }
        return result;
    }

    PropertyResult GetPropertyAsMatrix4(Collection* collection, Instance* instance, dmhash_t component_id, dmhash_t property_id, dmVMath::Matrix4* out_value)
    {
        PropertyOptions options;
        PropertyDesc out_prop;
        PropertyResult result = GetProperty(collection, instance, component_id, property_id, options, out_prop);
        if (result == PROPERTY_RESULT_OK)
        {
            if (PROPERTY_TYPE_MATRIX4 == out_prop.m_Variant.m_Type)
            {
                out_value->setCol0(dmVMath::Vector4(out_prop.m_Variant.m_M4[0],  out_prop.m_Variant.m_M4[1],  out_prop.m_Variant.m_M4[2],  out_prop.m_Variant.m_M4[3]));
                out_value->setCol1(dmVMath::Vector4(out_prop.m_Variant.m_M4[4],  out_prop.m_Variant.m_M4[5],  out_prop.m_Variant.m_M4[6],  out_prop.m_Variant.m_M4[7]));
                out_value->setCol2(dmVMath::Vector4(out_prop.m_Variant.m_M4[8],  out_prop.m_Variant.m_M4[9],  out_prop.m_Variant.m_M4[10], out_prop.m_Variant.m_M4[11]));
                out_value->setCol3(dmVMath::Vector4(out_prop.m_Variant.m_M4[12], out_prop.m_Variant.m_M4[13], out_prop.m_Variant.m_M4[14], out_prop.m_Variant.m_M4[15]));
            }
            else
            {
                result = PROPERTY_RESULT_TYPE_MISMATCH;
            }
        }
        return result;
    }

    PropertyResult SetProperty(Collection* collection, Instance* instance, dmhash_t component_id, dmhash_t property_id, PropertyOptions options, const PropertyVar& value)
    {
        if (instance == 0)
            return PROPERTY_RESULT_INVALID_INSTANCE;
        if (component_id == 0)
        {
            float* position = instance->m_Transform.GetPositionPtr();
            float* rotation = instance->m_Transform.GetRotationPtr();
            float* scale = instance->m_Transform.GetScalePtr();
            if (property_id == PROP_POSITION)
            {
                if (value.m_Type != PROPERTY_TYPE_VECTOR3)
                    return PROPERTY_RESULT_TYPE_MISMATCH;
                position[0] = value.m_V4[0];
                position[1] = value.m_V4[1];
                position[2] = value.m_V4[2];
                collection->m_DirtyTransforms = 1;
                return PROPERTY_RESULT_OK;
            }
            else if (property_id == PROP_POSITION_X)
            {
                if (value.m_Type != PROPERTY_TYPE_NUMBER)
                    return PROPERTY_RESULT_TYPE_MISMATCH;
                position[0] = (float)value.m_Number;
                collection->m_DirtyTransforms = 1;
                return PROPERTY_RESULT_OK;
            }
            else if (property_id == PROP_POSITION_Y)
            {
                if (value.m_Type != PROPERTY_TYPE_NUMBER)
                    return PROPERTY_RESULT_TYPE_MISMATCH;
                position[1] = (float)value.m_Number;
                collection->m_DirtyTransforms = 1;
                return PROPERTY_RESULT_OK;
            }
            else if (property_id == PROP_POSITION_Z)
            {
                if (value.m_Type != PROPERTY_TYPE_NUMBER)
                    return PROPERTY_RESULT_TYPE_MISMATCH;
                position[2] = (float)value.m_Number;
                collection->m_DirtyTransforms = 1;
                return PROPERTY_RESULT_OK;
            }
            else if (property_id == PROP_SCALE)
            {
                if (value.m_Type == PROPERTY_TYPE_NUMBER)
                {
                    scale[0] = (float)value.m_Number;
                    scale[1] = scale[0];
                    scale[2] = scale[0];
                    collection->m_DirtyTransforms = 1;
                    return PROPERTY_RESULT_OK;
                }
                else if (value.m_Type == PROPERTY_TYPE_VECTOR3)
                {
                    scale[0] = value.m_V4[0];
                    scale[1] = value.m_V4[1];
                    scale[2] = value.m_V4[2];
                    collection->m_DirtyTransforms = 1;
                    return PROPERTY_RESULT_OK;
                }
                return PROPERTY_RESULT_TYPE_MISMATCH;
            }
            else if (property_id == PROP_SCALE_XY)
            {
                if (value.m_Type == PROPERTY_TYPE_NUMBER)
                {
                    scale[0] = (float)value.m_Number;
                    scale[1] = scale[0];
                    collection->m_DirtyTransforms = 1;
                    return PROPERTY_RESULT_OK;
                }
                else if (value.m_Type == PROPERTY_TYPE_VECTOR3)
                {
                    scale[0] = value.m_V4[0];
                    scale[1] = value.m_V4[1];
                    collection->m_DirtyTransforms = 1;
                    return PROPERTY_RESULT_OK;
                }
                return PROPERTY_RESULT_TYPE_MISMATCH;
            }
            else if (property_id == PROP_SCALE_X)
            {
                if (value.m_Type != PROPERTY_TYPE_NUMBER)
                    return PROPERTY_RESULT_TYPE_MISMATCH;
                scale[0] = (float)value.m_Number;
                collection->m_DirtyTransforms = 1;
                return PROPERTY_RESULT_OK;
            }
            else if (property_id == PROP_SCALE_Y)
            {
                if (value.m_Type != PROPERTY_TYPE_NUMBER)
                    return PROPERTY_RESULT_TYPE_MISMATCH;
                scale[1] = (float)value.m_Number;
                collection->m_DirtyTransforms = 1;
                return PROPERTY_RESULT_OK;
            }
            else if (property_id == PROP_SCALE_Z)
            {
                if (value.m_Type != PROPERTY_TYPE_NUMBER)
                    return PROPERTY_RESULT_TYPE_MISMATCH;
                scale[2] = (float)value.m_Number;
                collection->m_DirtyTransforms = 1;
                return PROPERTY_RESULT_OK;
            }
            else if (property_id == PROP_ROTATION)
            {
                if (value.m_Type != PROPERTY_TYPE_QUAT)
                    return PROPERTY_RESULT_TYPE_MISMATCH;
                rotation[0] = value.m_V4[0];
                rotation[1] = value.m_V4[1];
                rotation[2] = value.m_V4[2];
                rotation[3] = value.m_V4[3];
                collection->m_DirtyTransforms = 1;
                return PROPERTY_RESULT_OK;
            }
            else if (property_id == PROP_ROTATION_X)
            {
                if (value.m_Type != PROPERTY_TYPE_NUMBER)
                    return PROPERTY_RESULT_TYPE_MISMATCH;
                rotation[0] = (float)value.m_Number;
                collection->m_DirtyTransforms = 1;
                return PROPERTY_RESULT_OK;
            }
            else if (property_id == PROP_ROTATION_Y)
            {
                if (value.m_Type != PROPERTY_TYPE_NUMBER)
                    return PROPERTY_RESULT_TYPE_MISMATCH;
                rotation[1] = (float)value.m_Number;
                collection->m_DirtyTransforms = 1;
                return PROPERTY_RESULT_OK;
            }
            else if (property_id == PROP_ROTATION_Z)
            {
                if (value.m_Type != PROPERTY_TYPE_NUMBER)
                    return PROPERTY_RESULT_TYPE_MISMATCH;
                rotation[2] = (float)value.m_Number;
                collection->m_DirtyTransforms = 1;
                return PROPERTY_RESULT_OK;
            }
            else if (property_id == PROP_ROTATION_W)
            {
                if (value.m_Type != PROPERTY_TYPE_NUMBER)
                    return PROPERTY_RESULT_TYPE_MISMATCH;
                rotation[3] = (float)value.m_Number;
                collection->m_DirtyTransforms = 1;
                return PROPERTY_RESULT_OK;
            }
            else if (property_id == PROP_EULER)
            {
                if (value.m_Type != PROPERTY_TYPE_VECTOR3)
                    return PROPERTY_RESULT_TYPE_MISMATCH;
                instance->m_EulerRotation = Vector3(value.m_V4[0], value.m_V4[1], value.m_V4[2]);
                UpdateEulerToRotation(instance);
                collection->m_DirtyTransforms = 1;
                return PROPERTY_RESULT_OK;
            }
            else if (property_id == PROP_EULER_X)
            {
                if (value.m_Type != PROPERTY_TYPE_NUMBER)
                    return PROPERTY_RESULT_TYPE_MISMATCH;
                instance->m_EulerRotation.setX((float)value.m_Number);
                UpdateEulerToRotation(instance);
                collection->m_DirtyTransforms = 1;
                return PROPERTY_RESULT_OK;
            }
            else if (property_id == PROP_EULER_Y)
            {
                if (value.m_Type != PROPERTY_TYPE_NUMBER)
                    return PROPERTY_RESULT_TYPE_MISMATCH;
                instance->m_EulerRotation.setY((float)value.m_Number);
                UpdateEulerToRotation(instance);
                collection->m_DirtyTransforms = 1;
                return PROPERTY_RESULT_OK;
            }
            else if (property_id == PROP_EULER_Z)
            {
                if (value.m_Type != PROPERTY_TYPE_NUMBER)
                    return PROPERTY_RESULT_TYPE_MISMATCH;
                instance->m_EulerRotation.setZ((float)value.m_Number);
                UpdateEulerToRotation(instance);
                collection->m_DirtyTransforms = 1;
                return PROPERTY_RESULT_OK;
            }
            else
            {
                return PROPERTY_RESULT_NOT_FOUND;
            }
        }
        else
        {
            uint16_t component_index;
            if (RESULT_OK == GetComponentIndex(instance, component_id, &component_index))
            {
                Prototype::Component* components = instance->m_Prototype->m_Components;
                Prototype::Component& component = components[component_index];
                ComponentType* type = component.m_Type;
                if (type->m_SetPropertyFunction)
                {
                    uintptr_t* user_data = 0;
                    if (type->m_InstanceHasUserData)
                    {
                        uint32_t next_component_instance_data = 0;
                        for (uint32_t i = 0; i < component_index; ++i)
                        {
                            if (components[i].m_Type->m_InstanceHasUserData)
                                ++next_component_instance_data;
                        }
                        user_data = &instance->m_ComponentInstanceUserData[next_component_instance_data];
                    }
                    ComponentSetPropertyParams p;
                    p.m_Context = type->m_Context;
                    p.m_World = collection->m_ComponentWorlds[component.m_TypeIndex];
                    p.m_Instance = GetInstanceHandle(collection, instance);
                    p.m_PropertyId = property_id;
                    p.m_UserData = user_data;
                    p.m_Value = value;
                    p.m_Options = &options;
                    return type->m_SetPropertyFunction(p);
                }
                else
                {
                    return PROPERTY_RESULT_NOT_FOUND;
                }
            }
            else
            {
                return PROPERTY_RESULT_COMP_NOT_FOUND;
            }
        }
        return PROPERTY_RESULT_OK;
    }

    static inline PropertyOption* NextPropertyOption(PropertyOptions* options)
    {
        if (options->m_OptionsCount >= MAX_PROPERTY_OPTIONS_COUNT)
            return 0;
        return &options->m_Options[options->m_OptionsCount++];
    }

    bool AddPropertyOptionsKey(PropertyOptions* options, dmhash_t key)
    {
        PropertyOption* option = NextPropertyOption(options);
        if (!option)
            return false;
        option->m_Key = key;
        option->m_HasKey = 1;
        return true;
    }

    bool AddPropertyOptionsIndex(PropertyOptions* options, int32_t index)
    {
        PropertyOption* option = NextPropertyOption(options);
        if (!option)
            return false;
        option->m_Index = index;
        option->m_HasKey = 0;
        return true;
    }

    bool AddPropertyOption(PropertyOptions* options, PropertyOption option)
    {
        PropertyOption* opt = NextPropertyOption(options);
        if (!opt)
            return false;
        *opt = option;
        return true;
    }

    bool SetPropertyOptionsByIndex(PropertyOptions* options, uint32_t index, int32_t value)
    {
        if (index >= options->m_OptionsCount)
            return false;
        PropertyOption* option = &options->m_Options[index];
        option->m_Index = value;
        option->m_HasKey = 0;
        return true;
    }

    uint32_t GetPropertyOptionsCount(HPropertyOptions options)
    {
        if (!options)
            return 0;
        return options->m_OptionsCount;
    }

    PropertyResult GetPropertyOptionsIndex(HPropertyOptions options, uint32_t index, int32_t* result)
    {
        if (!options || index >= options->m_OptionsCount)
            return PROPERTY_RESULT_INVALID_INDEX;
        if (options->m_Options[index].m_HasKey)
            return PROPERTY_RESULT_TYPE_MISMATCH;
        *result = options->m_Options[index].m_Index;
        return PROPERTY_RESULT_OK;
    }

    PropertyResult GetPropertyOptionsKey(HPropertyOptions options, uint32_t index, dmhash_t* result)
    {
        if (!options || index >= options->m_OptionsCount)
            return PROPERTY_RESULT_INVALID_INDEX;
        if (!options->m_Options[index].m_HasKey)
            return PROPERTY_RESULT_TYPE_MISMATCH;
        *result = options->m_Options[index].m_Key;
        return PROPERTY_RESULT_OK;
    }

    PropertyResult SetPropertyFromHash(Collection* collection, Instance* instance, dmhash_t component_id, dmhash_t property_id, dmhash_t value)
    {
        PropertyOptions options;
        PropertyVar prop_value(value);
        PropertyResult r = SetProperty(collection, instance, component_id, property_id, options, prop_value);
        return r;
    }

    PropertyResult SetPropertyFromFloat(Collection* collection, Instance* instance, dmhash_t component_id, dmhash_t property_id, float value)
    {
        PropertyOptions options;
        PropertyVar prop_value(value);
        PropertyResult r = SetProperty(collection, instance, component_id, property_id, options, prop_value);
        return r;
    }

    PropertyResult SetPropertyFromVector3(Collection* collection, Instance* instance, dmhash_t component_id, dmhash_t property_id, dmVMath::Vector3 value)
    {
        PropertyOptions options;
        PropertyVar prop_value(value);
        PropertyResult r = SetProperty(collection, instance, component_id, property_id, options, prop_value);
        return r;
    }

    PropertyResult SetPropertyFromVector4(Collection* collection, Instance* instance, dmhash_t component_id, dmhash_t property_id, dmVMath::Vector4 value)
    {
        PropertyOptions options;
        PropertyVar prop_value(value);
        PropertyResult r = SetProperty(collection, instance, component_id, property_id, options, prop_value);
        return r;
    }

    PropertyResult SetPropertyFromQuat(Collection* collection, Instance* instance, dmhash_t component_id, dmhash_t property_id, dmVMath::Quat value)
    {
        PropertyOptions options;
        PropertyVar prop_value(value);
        PropertyResult r = SetProperty(collection, instance, component_id, property_id, options, prop_value);
        return r;
    }

    PropertyResult SetPropertyFromBool(Collection* collection, Instance* instance, dmhash_t component_id, dmhash_t property_id, bool value)
    {
        PropertyOptions options;
        PropertyVar prop_value(value);
        PropertyResult r = SetProperty(collection, instance, component_id, property_id, options, prop_value);
        return r;
    }

    PropertyResult SetPropertyFromURL(Collection* collection, Instance* instance, dmhash_t component_id, dmhash_t property_id, dmMessage::URL value)
    {
        PropertyOptions options;
        PropertyVar prop_value(value);
        PropertyResult r = SetProperty(collection, instance, component_id, property_id, options, prop_value);
        return r;
    }

    PropertyResult SetPropertyFromText(Collection* collection, Instance* instance, dmhash_t component_id, dmhash_t property_id, const char* value)
    {
        PropertyOptions options;
        PropertyVar prop_value(value);
        PropertyResult r = SetProperty(collection, instance, component_id, property_id, options, prop_value);
        return r;
    }

    PropertyResult SetPropertyFromMatrix4(Collection* collection, Instance* instance, dmhash_t component_id, dmhash_t property_id, const dmVMath::Matrix4& value)
    {
        PropertyOptions options;
        PropertyVar prop_value(value);
        PropertyResult r = SetProperty(collection, instance, component_id, property_id, options, prop_value);
        return r;
    }

    PropertyResult GetProperty(HInstance hinstance, dmhash_t component_id, dmhash_t property_id, PropertyOptions options, PropertyDesc& out_value)
    {
        Collection* collection = 0;
        Instance* instance = GetInstanceFromHandle(hinstance, &collection);
        return instance ? GetProperty(collection, instance, component_id, property_id, options, out_value) : PROPERTY_RESULT_INVALID_INSTANCE;
    }

    PropertyResult SetProperty(HInstance hinstance, dmhash_t component_id, dmhash_t property_id, PropertyOptions options, const PropertyVar& value)
    {
        Collection* collection = 0;
        Instance* instance = GetInstanceFromHandle(hinstance, &collection);
        return instance ? SetProperty(collection, instance, component_id, property_id, options, value) : PROPERTY_RESULT_INVALID_INSTANCE;
    }

#define DM_GAMEOBJECT_PROPERTY_GETTER_WRAPPER(NAME, VALUE_TYPE) \
    PropertyResult NAME(HInstance hinstance, dmhash_t component_id, dmhash_t property_id, VALUE_TYPE out_value) \
    { \
        Collection* collection = 0; \
        Instance* instance = GetInstanceFromHandle(hinstance, &collection); \
        return instance ? NAME(collection, instance, component_id, property_id, out_value) : PROPERTY_RESULT_INVALID_INSTANCE; \
    }

    DM_GAMEOBJECT_PROPERTY_GETTER_WRAPPER(GetPropertyAsHash, dmhash_t*)
    DM_GAMEOBJECT_PROPERTY_GETTER_WRAPPER(GetPropertyAsFloat, float*)
    DM_GAMEOBJECT_PROPERTY_GETTER_WRAPPER(GetPropertyAsVector3, dmVMath::Vector3*)
    DM_GAMEOBJECT_PROPERTY_GETTER_WRAPPER(GetPropertyAsVector4, dmVMath::Vector4*)
    DM_GAMEOBJECT_PROPERTY_GETTER_WRAPPER(GetPropertyAsQuat, dmVMath::Quat*)
    DM_GAMEOBJECT_PROPERTY_GETTER_WRAPPER(GetPropertyAsBool, bool*)
    DM_GAMEOBJECT_PROPERTY_GETTER_WRAPPER(GetPropertyAsURL, dmMessage::URL*)
    DM_GAMEOBJECT_PROPERTY_GETTER_WRAPPER(GetPropertyAsText, const char**)
    DM_GAMEOBJECT_PROPERTY_GETTER_WRAPPER(GetPropertyAsMatrix4, dmVMath::Matrix4*)

#undef DM_GAMEOBJECT_PROPERTY_GETTER_WRAPPER

#define DM_GAMEOBJECT_PROPERTY_SETTER_WRAPPER(NAME, VALUE_TYPE) \
    PropertyResult NAME(HInstance hinstance, dmhash_t component_id, dmhash_t property_id, VALUE_TYPE value) \
    { \
        Collection* collection = 0; \
        Instance* instance = GetInstanceFromHandle(hinstance, &collection); \
        return instance ? NAME(collection, instance, component_id, property_id, value) : PROPERTY_RESULT_INVALID_INSTANCE; \
    }

    DM_GAMEOBJECT_PROPERTY_SETTER_WRAPPER(SetPropertyFromHash, dmhash_t)
    DM_GAMEOBJECT_PROPERTY_SETTER_WRAPPER(SetPropertyFromFloat, float)
    DM_GAMEOBJECT_PROPERTY_SETTER_WRAPPER(SetPropertyFromVector3, dmVMath::Vector3)
    DM_GAMEOBJECT_PROPERTY_SETTER_WRAPPER(SetPropertyFromVector4, dmVMath::Vector4)
    DM_GAMEOBJECT_PROPERTY_SETTER_WRAPPER(SetPropertyFromQuat, dmVMath::Quat)
    DM_GAMEOBJECT_PROPERTY_SETTER_WRAPPER(SetPropertyFromBool, bool)
    DM_GAMEOBJECT_PROPERTY_SETTER_WRAPPER(SetPropertyFromURL, dmMessage::URL)
    DM_GAMEOBJECT_PROPERTY_SETTER_WRAPPER(SetPropertyFromText, const char*)
    DM_GAMEOBJECT_PROPERTY_SETTER_WRAPPER(SetPropertyFromMatrix4, const dmVMath::Matrix4&)

#undef DM_GAMEOBJECT_PROPERTY_SETTER_WRAPPER

    // Recreate the instance at the given index with a new prototype.
    // Specifically:
    //  - recreate components and call init/final functions
    //  - patch data structures for identification and input stack
    //  - copy the rest of the fields
    // The old instance is destroyed.
    static void RecreateInstance(Collection* collection, uint32_t index, Prototype* old_proto, Prototype* new_proto, const char* new_proto_name) {
        Instance* instance = collection->m_Instances[index];
        // We don't support recreating instances that are 'transitioning'
        assert(instance->m_ToBeAdded == 0);
        assert(instance->m_ToBeDeleted == 0);
        Instance* new_instance = AllocInstance(new_proto, new_proto_name);
        if (!new_instance) {
            return;
        }
        // hierarchy-related
        new_instance->m_Index = instance->m_Index;
        new_instance->m_LevelIndex = instance->m_LevelIndex;
        new_instance->m_Depth = instance->m_Depth;
        new_instance->m_Bone = instance->m_Bone;
        new_instance->m_Parent = instance->m_Parent;
        new_instance->m_FirstChildIndex = instance->m_FirstChildIndex;
        new_instance->m_SiblingIndex = instance->m_SiblingIndex;
        // transform-related
        new_instance->m_Transform = instance->m_Transform;
        new_instance->m_EulerRotation = instance->m_EulerRotation;
        new_instance->m_PrevEulerRotation = instance->m_PrevEulerRotation;
        // id-related
        new_instance->m_Identifier = instance->m_Identifier;
        new_instance->m_IdentifierIndex = instance->m_IdentifierIndex;
        new_instance->m_Generation = instance->m_Generation;
        dmHashClone64(&new_instance->m_CollectionPathHashState, &instance->m_CollectionPathHashState, true);
        new_instance->m_Generated = instance->m_Generated;

        // Component callbacks receive a numeric handle and may resolve it back
        // through the collection. Point the slot at the instance whose callback
        // is currently running, while keeping the stable handle unchanged.
        collection->m_Instances[index] = new_instance;
        CreateResult res = CreateComponents(collection, new_instance);
        if (res != CREATE_RESULT_OK) {
            collection->m_Instances[index] = instance;
            dmHashRelease64(&new_instance->m_CollectionPathHashState);
            DeallocInstance(new_instance);
            return;
        }
        if (instance->m_Initialized) {
            InitComponents(collection, new_instance);
            new_instance->m_Initialized = 1;
        }
        // Because of how hot-reloading reloads resources in-place, the old instance would already point to the 'new' resource, so re-point it to the old
        instance->m_Prototype = old_proto;
        collection->m_Instances[index] = instance;
        if (instance->m_Initialized) {
            FinalComponents(collection, instance);
        }
        DestroyComponents(collection, instance);
        dmHashRelease64(&instance->m_CollectionPathHashState);
        collection->m_Instances[index] = new_instance;
        collection->m_IDToInstance.Put(new_instance->m_Identifier, GetInstanceHandle(collection, new_instance));

        DeallocInstance(instance);
        DoAddToUpdate(collection, new_instance);
    }

    static void ResourceReloadedCallback(const ResourceReloadedParams* params)
    {
        Collection* collection = (Collection*) params->m_UserData;
        for (uint32_t level_i = 0; level_i < MAX_HIERARCHICAL_DEPTH; ++level_i)
        {
            dmArray<uint32_t>& level = collection->m_LevelIndices[level_i];
            uint32_t instance_count = level.Size();
            for (uint32_t i = 0; i < instance_count; ++i)
            {
                uint32_t index = level[i];
                Instance* instance = collection->m_Instances[index];
                Prototype* prototype = (Prototype*)ResourceDescriptorGetResource(params->m_Resource);
                if (instance->m_Prototype == prototype) {
                    Prototype* prev_prototype = (Prototype*)ResourceDescriptorGetPrevResource(params->m_Resource);
                    RecreateInstance(collection, index, prev_prototype, prototype, params->m_Filename);
                } else {
                    uint32_t next_component_instance_data = 0;
                    for (uint32_t j = 0; j < instance->m_Prototype->m_ComponentCount; ++j)
                    {
                        Prototype::Component& component = instance->m_Prototype->m_Components[j];
                        ComponentType* type = component.m_Type;
                        if (component.m_ResourceId == ResourceDescriptorGetNameHash(params->m_Resource))
                        {
                            if (type->m_OnReloadFunction)
                            {
                                uintptr_t* user_data = 0;
                                if (type->m_InstanceHasUserData)
                                {
                                    user_data = &instance->m_ComponentInstanceUserData[next_component_instance_data];
                                }
                                ComponentOnReloadParams on_reload_params;
                                on_reload_params.m_Instance = GetInstanceHandle(collection, instance);
                                on_reload_params.m_Resource = prototype;
                                on_reload_params.m_World = collection->m_ComponentWorlds[component.m_TypeIndex];
                                on_reload_params.m_Context = type->m_Context;
                                on_reload_params.m_UserData = user_data;
                                type->m_OnReloadFunction(on_reload_params);
                            }
                        }
                        if (type->m_InstanceHasUserData)
                        {
                            next_component_instance_data++;
                        }
                    }
                }
            }
        }
    }

    // Unit test functions
    uint32_t GetAddToUpdateCount(HCollection hcollection)
    {
        Collection* collection = GetCollectionFromHandle(hcollection);
        if (!collection)
            return 0;
        uint32_t count = 0;
        uint32_t index = collection->m_InstancesToAddHead;
        while (index != INVALID_INSTANCE_INDEX) {
            index = collection->m_Instances[index]->m_NextToAdd;
            ++count;
        }
        return count;
    }

    uint32_t GetRemoveFromUpdateCount(HCollection hcollection)
    {
        Collection* collection = GetCollectionFromHandle(hcollection);
        if (!collection)
            return 0;
        uint32_t count = 0;
        uint32_t index = collection->m_InstancesToDeleteHead;
        while (index != INVALID_INSTANCE_INDEX) {
            index = collection->m_Instances[index]->m_NextToDelete;
            ++count;
        }
        return count;
    }
}
