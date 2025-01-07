// Copyright 2020-2024 The Defold Foundation
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
#include "gameobject_props_lua.h"
#include "gameobject_props_ddf.h"
#include "gameobject_props.h"

#include "gameobject/gameobject_ddf.h"

#include <dmsdk/dlib/vmath.h>
#include <dmsdk/resource/resource.h>

#include <dmsdk/dlib/vmath.h>
#include <dmsdk/resource/resource.hpp>

DM_PROPERTY_GROUP(rmtp_GameObject, "Gameobjects");

DM_PROPERTY_U32(rmtp_GOInstances, 0, FrameReset, "# alive go instances / frame", &rmtp_GameObject);
DM_PROPERTY_U32(rmtp_GODeleted, 0, FrameReset, "# deleted instances / frame", &rmtp_GameObject);

namespace dmGameObject
{
    const char* COLLECTION_MAX_INSTANCES_KEY = "collection.max_instances";
    const char* COLLECTION_MAX_INPUT_STACK_ENTRIES_KEY = "collection.max_input_stack_entries";
    const dmhash_t UNNAMED_IDENTIFIER = dmHashBuffer64("__unnamed__", strlen("__unnamed__"));
    const dmhash_t GAME_OBJECT_EXT = dmHashString64("goc");
    const char* ID_SEPARATOR = "/";
    const uint32_t MAX_DISPATCH_ITERATION_COUNT = 10;

    static Prototype EMPTY_PROTOTYPE;

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

    PROP_VECTOR3(POSITION, position);
    PROP_QUAT(ROTATION, rotation);
    PROP_VECTOR3(EULER, euler);
    PROP_VECTOR3(SCALE, scale);

    static void ResourceReloadedCallback(const ResourceReloadedParams* params);
    static void DoDeleteInstance(Collection* collection, HInstance instance);
    static bool InitInstance(Collection* collection, HInstance instance);
    static bool FinalInstance(Collection* collection, HInstance instance);

    static Collection* AllocCollection(const char* name, HRegister regist, uint32_t max_instances, dmGameObjectDDF::CollectionDesc* collection_desc);
    static void DeallocCollection(Collection* collection);
    static bool InitCollection(Collection* collection);
    static bool FinalCollection(Collection* collection);

    Prototype::~Prototype()
    {
        free(m_Components);
    }

    InputAction::InputAction()
    {
        memset(this, 0, sizeof(InputAction));
    }

    PropertyOptions::PropertyOptions()
    : m_Index(0)
    , m_HasKey(0) {}

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

    PropertyVar::PropertyVar(Matrix4 v)
    {
        m_Type   = PROPERTY_TYPE_MATRIX4;

        Vector4& c0 = v[0];
        float& v0   = c0[0];
        memcpy(m_M4, &v0, sizeof(m_M4));
    }

    Register::Register()
    {
        m_ComponentTypeCount = 0;
        m_DefaultCollectionCapacity = DEFAULT_MAX_COLLECTION_CAPACITY;
        m_DefaultInputStackCapacity = DEFAULT_MAX_INPUT_STACK_CAPACITY;
        m_Mutex = dmMutex::New();
    }

    Register::~Register()
    {
        dmMutex::Delete(m_Mutex);
    }

    ComponentType::ComponentType()
    {
        memset(this, 0, sizeof(*this));
    }

    void Initialize(HRegister regist, dmScript::HContext context)
    {
        InitializeScript(regist, context);
    }

    HRegister NewRegister()
    {
        return new Register();
    }

    Collection::Collection(dmResource::HFactory factory, HRegister regist, uint32_t max_instances, uint32_t max_input_stack_entries)
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
        m_InputFocusStack.SetCapacity(max_input_stack_entries);
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
        m_Initialized = 0;
        m_FixedAccumTime = 0.0f;
        m_FirstUpdate = 1;

        m_InstancesToDeleteHead = INVALID_INSTANCE_INDEX;
        m_InstancesToDeleteTail = INVALID_INSTANCE_INDEX;

        m_InstancesToAddHead = INVALID_INSTANCE_INDEX;
        m_InstancesToAddTail = INVALID_INSTANCE_INDEX;

        memset(&m_Instances[0], 0, sizeof(Instance*) * max_instances);
        memset(&m_WorldTransforms[0], 0xcc, sizeof(dmTransform::Transform) * max_instances);
        memset(&m_LevelIndices[0], 0, sizeof(m_LevelIndices));
    }

    Result SetCollectionDefaultCapacity(HRegister regist, uint32_t capacity)
    {
        assert(regist != 0x0);
        if(capacity >= INVALID_INSTANCE_INDEX - 1 || capacity == 0)
            return RESULT_INVALID_OPERATION;
        regist->m_DefaultCollectionCapacity = capacity;
        return RESULT_OK;
    }

    uint32_t GetCollectionDefaultCapacity(HRegister regist)
    {
        assert(regist != 0x0);
        return regist->m_DefaultCollectionCapacity;
    }

    void SetInputStackDefaultCapacity(HRegister regist, uint32_t capacity)
    {
        assert(regist != 0x0);
        regist->m_DefaultInputStackCapacity = capacity;
    }

    static uint32_t GetInputStackDefaultCapacity(HRegister regist)
    {
        assert(regist != 0x0);
        return regist->m_DefaultInputStackCapacity;
    }

    void AddDynamicResourceHash(HCollection hcollection, dmhash_t resource_hash)
    {
        Collection* collection = hcollection->m_Collection;
        dmMutex::Lock(collection->m_Mutex);
        if (collection->m_DynamicResources.Remaining() == 0)
        {
            collection->m_DynamicResources.OffsetCapacity(1);
        }
        collection->m_DynamicResources.Push(resource_hash);
        dmMutex::Unlock(collection->m_Mutex);
    }

    void RemoveDynamicResourceHash(HCollection hcollection, dmhash_t resource_hash)
    {
        Collection* collection = hcollection->m_Collection;
        dmMutex::Lock(collection->m_Mutex);
        for (int i = 0; i < collection->m_DynamicResources.Size(); ++i)
        {
            if (collection->m_DynamicResources[i] == resource_hash)
            {
                collection->m_DynamicResources.EraseSwap(i);
            }
        }
        dmMutex::Unlock(collection->m_Mutex);
    }

    static void ReleaseDynamicResources(Collection* collection)
    {
        dmMutex::Lock(collection->m_Mutex);
        for (int i = 0; i < collection->m_DynamicResources.Size(); ++i)
        {
            HResourceDescriptor rd = dmResource::FindByHash(collection->m_Factory, collection->m_DynamicResources[i]);
            assert(rd);
            void* resource = dmResource::GetResource(rd);
            dmResource::Release(collection->m_Factory, resource);
        }
        collection->m_DynamicResources.SetSize(0);
        collection->m_DynamicResources.SetCapacity(0);
        dmMutex::Unlock(collection->m_Mutex);
    }

    void DeleteCollections(HRegister regist)
    {
        uint32_t collection_count = regist->m_Collections.Size();
        for (uint32_t i = 0; i < collection_count; ++i)
        {
            // TODO Note indexing of m_Collections is always 0 because DeleteCollection modifies the array.
            // Should be fixed by DEF-54
            Collection* collection = regist->m_Collections[0];
            FinalCollection(collection);
            DeleteCollection(collection);
        }
        regist->m_Collections.SetSize(0);
    }

    HCollection GetCollectionByHash(HRegister regist, dmhash_t socket_name)
    {
        uint32_t collection_count = regist->m_Collections.Size();
        for (uint32_t i = 0; i < collection_count; ++i)
        {
            Collection* collection = regist->m_Collections[i];
            if (collection->m_NameHash == socket_name)
                return collection->m_HCollection;
        }
        return 0;
    }

    void DeleteRegister(HRegister regist)
    {
        DeleteCollections(regist);
        delete regist;
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

    Collection* AllocCollection(const char* name, HRegister regist, uint32_t max_instances, dmGameObjectDDF::CollectionDesc* collection_desc)
    {
        uint32_t instances_in_collection = GetMaxComponentInstances(GAME_OBJECT_EXT, collection_desc);
        if (instances_in_collection == 0)
        {
            instances_in_collection = max_instances;
        }
        else
        {
            instances_in_collection = dmMath::Min(max_instances, instances_in_collection);
        }
        Collection* collection = new Collection(0, 0, instances_in_collection, GetInputStackDefaultCapacity(regist));
        collection->m_Mutex = dmMutex::New();

        for (uint32_t i = 0; i < regist->m_ComponentTypeCount; ++i)
        {
            if (regist->m_ComponentTypes[i].m_NewWorldFunction)
            {
                ComponentNewWorldParams params;
                params.m_Context = regist->m_ComponentTypes[i].m_Context;
                params.m_ComponentIndex = i;
                params.m_MaxComponentInstances = GetMaxComponentInstances(regist->m_ComponentTypes[i].m_NameHash, collection_desc);
                params.m_MaxInstances = max_instances;
                params.m_World = &collection->m_ComponentWorlds[i];
                regist->m_ComponentTypes[i].m_NewWorldFunction(params);
            }
        }

        return collection;
    }

    void DeallocCollection(Collection* collection)
    {
        DM_PROFILE("DeallocCollection");

        HRegister regist = collection->m_Register;
        for (uint32_t i = 0; i < regist->m_ComponentTypeCount; ++i)
        {
            DM_PROFILE_DYN(regist->m_ComponentTypes[i].m_Name, 0);

            ComponentDeleteWorldParams params;
            params.m_Context = regist->m_ComponentTypes[i].m_Context;
            params.m_World = collection->m_ComponentWorlds[i];
            if (regist->m_ComponentTypes[i].m_DeleteWorldFunction)
                regist->m_ComponentTypes[i].m_DeleteWorldFunction(params);
        }
        dmMutex::Delete(collection->m_Mutex);
        delete collection;
    }

    Result AttachCollection(Collection* collection, const char* name, dmResource::HFactory factory, HRegister regist, HCollection hcollection)
    {
        collection->m_HCollection = hcollection;
        collection->m_Register = regist;
        hcollection->m_Collection = collection;
        collection->m_Factory = factory;

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

        dmResource::RegisterResourceReloadedCallback(factory, ResourceReloadedCallback, collection);

        DM_MUTEX_SCOPED_LOCK(regist->m_Mutex);
        if (regist->m_Collections.Full())
        {
            regist->m_Collections.OffsetCapacity(4);
        }
        regist->m_Collections.Push(collection);
        return RESULT_OK;
    }

    void DetachCollection(Collection* collection)
    {
        HRegister regist = collection->m_Register;

        dmMutex::Lock(regist->m_Mutex);
        for (uint32_t i = 0; i < regist->m_Collections.Size(); ++i)
        {
            // TODO This design is not really thought through, since it modifies the m_Collections
            // member in a hidden context. Reported in DEF-54
            if (regist->m_Collections[i] == collection)
            {
                // Resize m_Collections manually here instead of using EraseSwap.
                // This is to keep m_Collections in their spawn order, otherwise we run the risk
                // of deleting proxy collections before their parent/spawning which in turn is
                // problematic since the "parent" are going to delete the proxy collections they spawned.
                for (uint32_t j = i; j < regist->m_Collections.Size() - 1; ++j)
                {
                    regist->m_Collections[j] = regist->m_Collections[j+1];
                }
                regist->m_Collections.SetSize(regist->m_Collections.Size() - 1);
                break;
            }
        }

        dmMutex::Unlock(regist->m_Mutex);

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

        collection->m_HCollection->m_Collection = 0;
        collection->m_HCollection = 0;
    }

    HCollection NewCollection(const char* name, dmResource::HFactory factory, HRegister regist, uint32_t max_instances, HCollectionDesc collection_desc)
    {
        if (max_instances > INVALID_INSTANCE_INDEX)
        {
            dmLogError("max_instances must be less or equal to %d", INVALID_INSTANCE_INDEX - 1);
            return 0;
        }

        Collection* collection = AllocCollection(name, regist, max_instances, (dmGameObjectDDF::CollectionDesc*)collection_desc);
        if (!collection)
        {
            return 0;
        }

        collection->m_NameHash = dmHashString64(name); // Same as the socket name

        HCollection hcollection = (HCollection)new CollectionHandle;
        Result result = AttachCollection(collection, name, factory, regist, hcollection);
        if (result != RESULT_OK)
        {
            DeallocCollection(collection);
            delete hcollection;
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

        HCollection hcollection = collection->m_HCollection;
        DetachCollection(collection);
        DeallocCollection(collection);
        delete hcollection;
    }

    // Really should be renamed "DelayDelete"
    void DeleteCollection(HCollection hcollection)
    {
        hcollection->m_Collection->m_ToBeDeleted = 1;
    }

    uint32_t GetComponentTypeIndex(HCollection hcollection, dmhash_t type_hash)
    {
        Register* regist = GetRegister(hcollection);
        for (uint32_t i = 0; i < regist->m_ComponentTypeCount; ++i)
        {
            ComponentType* ct = &regist->m_ComponentTypes[i];
            if (ct->m_NameHash == type_hash)
            {
                return i;
            }
        }
        return 0xFFFFFFFF;
    }

    HComponentWorld GetWorld(HCollection hcollection, uint32_t component_type_index)
    {
        Register* regist = GetRegister(hcollection);
        if (component_type_index < regist->m_ComponentTypeCount)
        {
            return hcollection->m_Collection->m_ComponentWorlds[component_type_index];
        }
        else
        {
            return 0x0;
        }
    }

    void* GetContext(HCollection hcollection, uint32_t component_type_index)
    {
        Register* regist = GetRegister(hcollection);
        if (component_type_index < regist->m_ComponentTypeCount)
        {
            ComponentType* ct = &regist->m_ComponentTypes[component_type_index];
            return ct->m_Context;
        }
        else
        {
            return 0;
        }
    }

    ComponentType* FindComponentType(Register* regist, HResourceType resource_type, uint32_t* index)
    {
        for (uint32_t i = 0; i < regist->m_ComponentTypeCount; ++i)
        {
            ComponentType* ct = &regist->m_ComponentTypes[i];
            if (ct->m_ResourceType == resource_type)
            {
                if (index != 0x0)
                    *index = i;
                return ct;
            }
        }
        return 0;
    }

    uint32_t GetNumComponentTypes(HRegister regist)
    {
        return regist->m_ComponentTypeCount;
    }

    ComponentType* GetComponentType(HRegister regist, uint32_t index)
    {
        return &regist->m_ComponentTypes[index];
    }

    struct ComponentTypeSortPred
    {
        HRegister m_Register;
        ComponentTypeSortPred(HRegister regist) : m_Register(regist) {}

        bool operator ()(const uint16_t& a, const uint16_t& b) const
        {
            return m_Register->m_ComponentTypes[a].m_UpdateOrderPrio < m_Register->m_ComponentTypes[b].m_UpdateOrderPrio;
        }
    };

    Result RegisterComponentType(HRegister regist, const ComponentType& type)
    {
        if (regist->m_ComponentTypeCount == MAX_COMPONENT_TYPES)
            return RESULT_OUT_OF_RESOURCES;

        if (FindComponentType(regist, type.m_ResourceType, 0x0) != 0)
            return RESULT_ALREADY_REGISTERED;

        if (type.m_UpdateFunction != 0x0 && type.m_AddToUpdateFunction == 0x0) {
            dmLogWarning("Registering an Update function for '%s' requires the registration of an AddToUpdate function.", type.m_Name);
            return RESULT_INVALID_OPERATION;
        }

        regist->m_ComponentTypes[regist->m_ComponentTypeCount] = type;
        regist->m_ComponentTypes[regist->m_ComponentTypeCount].m_NameHash = dmHashString64(type.m_Name);
        regist->m_ComponentTypesOrder[regist->m_ComponentTypeCount] = regist->m_ComponentTypeCount;
        regist->m_ComponentTypeCount++;
        return RESULT_OK;
    }

    Result SetUpdateOrderPrio(HRegister regist, HResourceType resource_type, uint16_t prio)
    {
        bool found = false;
        for (uint32_t i = 0; i < regist->m_ComponentTypeCount; ++i)
        {
            if (regist->m_ComponentTypes[i].m_ResourceType == resource_type)
            {
                regist->m_ComponentTypes[i].m_UpdateOrderPrio = prio;
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

    void SortComponentTypes(HRegister regist)
    {
        std::sort(regist->m_ComponentTypesOrder, regist->m_ComponentTypesOrder + regist->m_ComponentTypeCount, ComponentTypeSortPred(regist));
    }


    static void EraseSwapLevelIndex(Collection* collection, HInstance instance)
    {
        /*
         * Remove instance from m_LevelIndices using an erase-swap operation
         */

        dmArray<uint16_t>& level = collection->m_LevelIndices[instance->m_Depth];
        assert(level.Size() > 0);
        assert(instance->m_LevelIndex < level.Size());

        uint16_t level_index = instance->m_LevelIndex;
        uint16_t swap_in_index = level.EraseSwap(level_index);
        HInstance swap_in_instance = collection->m_Instances[swap_in_index];
        assert(swap_in_instance->m_Index == swap_in_index);
        swap_in_instance->m_LevelIndex = level_index;
    }

    /*
     * Heuristic for expanding the level indices arrays:
     * * Increase capacity by 50%, but:
     * ** 10 elements as min
     * ** Up to max_instances as max
     */
    static void ExpandLevel(dmArray<uint16_t>& level, uint32_t max_instances)
    {
        const uint32_t min_offset = 10;
        const uint32_t max_offset = max_instances - level.Capacity();
        int32_t offset = dmMath::Min(max_offset, dmMath::Max(min_offset, level.Size() / 2));
        level.OffsetCapacity(offset);
    }

    static void InsertInstanceInLevelIndex(Collection* collection, HInstance instance)
    {
        /*
         * Insert instance in m_LevelIndices at level set in instance->m_Depth
         */
        dmArray<uint16_t>& level = collection->m_LevelIndices[instance->m_Depth];
        if (level.Full())
            ExpandLevel(level, collection->m_MaxInstances);
        assert(!level.Full());

        uint16_t level_index = (uint16_t)level.Size();
        level.SetSize(level_index + 1);
        level[level_index] = instance->m_Index;
        instance->m_LevelIndex = level_index;
    }

    static HInstance AllocInstance(Prototype* proto, const char* prototype_name) {
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

    static void DeallocInstance(HInstance instance) {
        instance->~Instance();
        void* instance_memory = (void*) instance;

        // This is required for failing test
        // TODO: #ifdef on something...?
        // Clear all memory excluding ComponentInstanceUserData
        memset(instance_memory, 0xcc, sizeof(Instance));
        operator delete (instance_memory);
    }

    HInstance NewInstance(Collection* collection, Prototype* proto, const char* prototype_name) {
        if (collection->m_InstanceIndices.Remaining() == 0)
        {
            dmLogError("The game object instance could not be created since the buffer is full (%d). Increase the capacity with collection.max_instances", collection->m_InstanceIndices.Capacity());
            return 0;
        }
        HInstance instance = AllocInstance(proto, prototype_name);
        instance->m_Collection = collection;
        instance->m_ScaleAlongZ = collection->m_ScaleAlongZ;
        uint16_t instance_index = collection->m_InstanceIndices.Pop();
        instance->m_Index = instance_index;
        assert(collection->m_Instances[instance_index] == 0);
        collection->m_Instances[instance_index] = instance;

        InsertInstanceInLevelIndex(collection, instance);

        return instance;
    }

    HInstance NewInstance(HCollection hcollection, Prototype* proto, const char* prototype_name){
        return NewInstance(hcollection->m_Collection, proto, prototype_name);
    }

    void UndoNewInstance(Collection* collection, HInstance instance) {
        if (instance->m_Prototype != &EMPTY_PROTOTYPE) {
            dmResource::Release(collection->m_Factory, instance->m_Prototype);
        }
        EraseSwapLevelIndex(collection, instance);

        if (instance->m_Parent != INVALID_INSTANCE_INDEX)
        {
            Unlink(collection, instance);
        }

        uint16_t instance_index = instance->m_Index;
        operator delete ((void*)instance);
        collection->m_Instances[instance_index] = 0x0;
        collection->m_InstanceIndices.Push(instance_index);
        assert(collection->m_IDToInstance.Size() <= collection->m_InstanceIndices.Size());
    }

    void UndoNewInstance(HCollection hcollection, HInstance instance) {
        UndoNewInstance(hcollection->m_Collection, instance);
    }

    bool CreateComponents(Collection* collection, HInstance instance) {
        DM_PROFILE("CreateComponents");

        Prototype* proto = instance->m_Prototype;
        uint32_t components_created = 0;
        uint32_t next_component_instance_data = 0;
        bool ok = true;
        if (proto->m_ComponentCount > 0xFFFF ) {
            dmLogWarning("Too many components in game object: %u (max is 65536)", proto->m_ComponentCount);
            return false;
        }
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
            params.m_Instance = instance;
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
                ok = false;
                break;
            }
        }

        if (!ok)
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
                params.m_Instance = instance;
                params.m_World = collection->m_ComponentWorlds[component->m_TypeIndex];
                params.m_Context = component_type->m_Context;
                params.m_UserData = component_instance_data;
                component_type->m_DestroyFunction(params);
            }
        }

        return ok;
    }

    bool CreateComponents(HCollection hcollection, HInstance instance) {
        return CreateComponents(hcollection->m_Collection, instance);
    }

    static void DestroyComponents(Collection* collection, HInstance instance) {
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
            params.m_Instance = instance;
            params.m_World = collection->m_ComponentWorlds[component->m_TypeIndex];
            params.m_Context = component_type->m_Context;
            params.m_UserData = component_instance_data;
            component_type->m_DestroyFunction(params);
        }
    }

    void* GetResource(HInstance instance)
    {
        return instance->m_Prototype == &EMPTY_PROTOTYPE ? 0 : instance->m_Prototype;
    }

    HInstance New(HCollection hcollection, const char* prototype_name) {
        Collection* collection = hcollection->m_Collection;
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
        HInstance instance = NewInstance(hcollection, proto, prototype_name);
        if (instance != 0) {
            bool result = CreateComponents(hcollection, instance);
            if (!result) {
                // We can not call Delete here. Delete call DestroyFunction for every component
                ReleaseIdentifier(collection, instance);
                UndoNewInstance(collection, instance);
                instance = 0;
            }
        } else if (proto != &EMPTY_PROTOTYPE) {
            dmResource::Release(factory, proto);
        }
        return instance;
    }

    dmhash_t ConstructInstanceId(uint32_t index)
    {
        char buffer[16] = { 0 };
        int length = dmSnPrintf(buffer, sizeof(buffer), "%sinstance%d", ID_SEPARATOR, index);
        return dmHashBuffer64(buffer, (uint32_t)length);
    }

    uint32_t AcquireInstanceIndex(HCollection hcollection)
    {
        Collection* collection = hcollection->m_Collection;
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
        ReleaseInstanceIndex(index, hcollection->m_Collection);
    }

    void AssignInstanceIndex(uint32_t index, HInstance instance)
    {
        if (instance != 0x0)
        {
            instance->m_IdentifierIndex = index;
        }
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

    Result SetIdentifier(Collection* collection, HInstance instance, dmhash_t id)
    {
        if (collection->m_IDToInstance.Get(id))
            return RESULT_IDENTIFIER_IN_USE;

        if (instance->m_Identifier != UNNAMED_IDENTIFIER)
            return RESULT_IDENTIFIER_ALREADY_SET;

        instance->m_Identifier = id;
        collection->m_IDToInstance.Put(id, instance);

        assert(collection->m_IDToInstance.Size() <= collection->m_InstanceIndices.Size());
        return RESULT_OK;
    }

    Result SetIdentifier(HCollection hcollection, HInstance instance, dmhash_t id)
    {
        return SetIdentifier(hcollection->m_Collection, instance, id);
    }

    void ReleaseIdentifier(Collection* collection, HInstance instance)
    {
        if (instance->m_Identifier != UNNAMED_IDENTIFIER) {
            collection->m_IDToInstance.Erase(instance->m_Identifier);
            instance->m_Identifier = UNNAMED_IDENTIFIER;
        }
    }

    // Schedule instance to be added to update
    static void AddToUpdate(Collection* collection, HInstance instance)
    {
        // NOTE: Do not add to update twice.
        assert(instance->m_ToBeAdded == 0);
        if (instance->m_ToBeDeleted) {
            return;
        }
        instance->m_ToBeAdded = 1;
        uint16_t index = instance->m_Index;
        uint16_t tail = collection->m_InstancesToAddTail;
        if (tail != INVALID_INSTANCE_INDEX) {
            HInstance tail_instance = collection->m_Instances[tail];
            tail_instance->m_NextToAdd = index;
        } else {
            collection->m_InstancesToAddHead = index;
        }
        collection->m_InstancesToAddTail = index;
    }

    // Actually add instance to update
    static bool DoAddToUpdate(Collection* collection, HInstance instance) {
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
                        params.m_Instance = instance;
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
        uint16_t index = collection->m_InstancesToAddHead;
        bool result = true;
        while (index != INVALID_INSTANCE_INDEX) {
            HInstance instance = collection->m_Instances[index];
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

    static bool SetScriptPropertiesFromBuffer(HInstance instance, const char *prototype_name, HPropertyContainer property_container)
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
                params.m_Instance = instance;
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
    static HInstance SpawnInternal(Collection* collection, Prototype *proto, const char *prototype_name, dmhash_t id, HPropertyContainer property_container, const Point3& position, const Quat& rotation, const Vector3& scale)
    {
        if (collection->m_ToBeDeleted) {
            dmLogWarning("Spawning is not allowed when the collection is being deleted.");
            return 0;
        }

        HInstance instance = dmGameObject::NewInstance(collection, proto, prototype_name);
        if (instance == 0) {
            return 0;
        }

        dmResource::IncRef(collection->m_Factory, proto);

        SetPosition(instance, position);
        SetRotation(instance, rotation);
        SetScale(instance, scale);
        collection->m_WorldTransforms[instance->m_Index] = dmTransform::ToMatrix4(instance->m_Transform);

        dmHashInit64(&instance->m_CollectionPathHashState, true);
        dmHashUpdateBuffer64(&instance->m_CollectionPathHashState, ID_SEPARATOR, strlen(ID_SEPARATOR));

        Result result = SetIdentifier(collection, instance, id);
        if (result == RESULT_IDENTIFIER_IN_USE)
        {
            dmLogError("The identifier '%s' is already in use.", dmHashReverseSafe64(id));
            UndoNewInstance(collection, instance);
            return 0;
        }

        bool success = CreateComponents(collection, instance);
        if (!success) {
            ReleaseIdentifier(collection, instance);
            UndoNewInstance(collection, instance);
            return 0;
        }

        success = SetScriptPropertiesFromBuffer(instance, prototype_name, property_container);

        if (success && !InitInstance(collection, instance))
        {
            dmLogError("Could not initialize when spawning %s.", prototype_name);
            success = false;
        }

        if (success) {
            AddToUpdate(collection, instance);
        } else {
            Delete(collection, instance, false);
            return 0;
        }

        return instance;
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

    static void ReparentChildNodes(Collection* collection, HInstance instance)
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


    // Returns if successful or not
    static bool CollectionSpawnFromDescInternal(Collection* collection, dmGameObjectDDF::CollectionDesc* collection_desc, InstancePropertyBuffers *property_buffers, InstanceIdMap *id_mapping, dmTransform::Transform const &transform)
    {
        // Path prefix for collection objects
        char root_path[32];
        HashState64 prefixHashState;
        dmHashInit64(&prefixHashState, true);
        GenerateUniqueCollectionInstanceId(collection, root_path, sizeof(root_path));
        dmHashUpdateBuffer64(&prefixHashState, root_path, strlen(root_path));

        // table for output ids
        id_mapping->SetCapacity(32, collection_desc->m_Instances.m_Count);

        dmArray<HInstance> new_instances;
        new_instances.SetCapacity(collection_desc->m_Instances.m_Count);

        bool success = true;

        for (uint32_t i = 0; i < collection_desc->m_Instances.m_Count; ++i)
        {
            const dmGameObjectDDF::InstanceDesc& instance_desc = collection_desc->m_Instances[i];
            Prototype* proto = 0x0;
            dmResource::HFactory factory = collection->m_Factory;
            dmGameObject::HInstance instance = 0x0;

            if (instance_desc.m_Prototype)
            {
                dmResource::Result error = dmResource::Get(factory, instance_desc.m_Prototype, (void**)&proto);
                if (error == dmResource::RESULT_OK) {
                    instance = dmGameObject::NewInstance(collection, proto, instance_desc.m_Prototype);
                    if (instance == 0) {
                        dmResource::Release(factory, proto);
                        success = false;
                        break;
                    }
                }
            }

            if (!instance)
                continue;

            instance->m_ScaleAlongZ = collection_desc->m_ScaleAlongZ;
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
                success = false;
            } else {
                dmHashUpdateBuffer64(&instance->m_CollectionPathHashState, instance_desc.m_Id, path_end - instance_desc.m_Id + 1);
            }

            // Construct the full new path id and store in the id mapping table (mapping from prefixless
            // to with the root_path added)
            HashState64 new_id_hs;
            dmHashClone64(&new_id_hs, &prefixHashState, true);
            dmHashUpdateBuffer64(&new_id_hs, instance_desc.m_Id, strlen(instance_desc.m_Id));
            dmhash_t new_id = dmHashFinal64(&new_id_hs);
            dmhash_t id = dmHashBuffer64(instance_desc.m_Id, strlen(instance_desc.m_Id));
            id_mapping->Put(id, new_id);
            new_instances.Push(instance);

            if (dmGameObject::SetIdentifier(collection, instance, new_id) != dmGameObject::RESULT_OK)
            {
                dmLogError("Unable to set identifier for %s%s. Name clash?", root_path, instance_desc.m_Id);
                success = false;
            }
        }
        dmHashRelease64(&prefixHashState);

        if (success)
        {
            // Setup hierarchy
            for (uint32_t i = 0; i < collection_desc->m_Instances.m_Count; ++i)
            {
                const dmGameObjectDDF::InstanceDesc& instance_desc = collection_desc->m_Instances[i];

                dmhash_t *parent_id = id_mapping->Get(dmHashString64(instance_desc.m_Id));
                assert(parent_id);

                dmGameObject::HInstance parent = dmGameObject::GetInstanceFromIdentifier(collection, *parent_id);
                assert(parent);

                for (uint32_t j = 0; j < instance_desc.m_Children.m_Count; ++j)
                {
                    dmhash_t child_id = dmGameObject::GetAbsoluteIdentifier(parent, instance_desc.m_Children[j], strlen(instance_desc.m_Children[j]));

                    // It is not always the case that 'parent' has had the path prefix prepended to its id, so it is necessary
                    // to see if a remapping exists.
                    dmhash_t *new_id = id_mapping->Get(child_id);
                    if (new_id)
                    {
                        child_id = *new_id;
                    }

                    dmGameObject::HInstance child = dmGameObject::GetInstanceFromIdentifier(collection, child_id);
                    if (child)
                    {
                        dmGameObject::Result r = dmGameObject::SetParent(child, parent);
                        if (r != dmGameObject::RESULT_OK)
                        {
                            dmLogError("Unable to set %s as parent to %s (%d)", instance_desc.m_Id, instance_desc.m_Children[j], r);
                            success = false;
                        }
                    }
                    else
                    {
                        dmLogError("Child not found: %s", instance_desc.m_Children[j]);
                        success = false;
                    }
                }
            }
        }

        // Exit point 1: Before components are created.
        if (!success)
        {
            for (uint32_t i=0;i!=new_instances.Size();i++)
            {
                ReleaseIdentifier(collection, new_instances[i]);
                UndoNewInstance(collection, new_instances[i]);
            }
            id_mapping->Clear();
            return false;
        }

        // Update the transform for all parent-less objects
        for (uint32_t i=0;i!=new_instances.Size();i++)
        {
            if (!GetParent(new_instances[i]))
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
        dmArray<HInstance> created;
        created.SetCapacity(collection_desc->m_Instances.m_Count);

        for (uint32_t i = 0; i < collection_desc->m_Instances.m_Count; ++i)
        {
            const dmGameObjectDDF::InstanceDesc& instance_desc = collection_desc->m_Instances[i];

            dmhash_t *instance_id = id_mapping->Get(dmHashString64(instance_desc.m_Id));
            assert(instance_id);

            dmGameObject::HInstance instance = dmGameObject::GetInstanceFromIdentifier(collection, *instance_id);
            bool result = dmGameObject::CreateComponents(collection, instance);
            if (result) {
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
                            success = false;
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
                                    success = false;
                                }
                                break;
                            }
                        }

                        HPropertyContainer lua_properties = 0x0;
                        HPropertyContainer* instance_properties = property_buffers->Get(dmHashString64(instance_desc.m_Id));
                        if (instance_properties != 0x0)
                        {
                            if (strcmp(type->m_Name, "scriptc") == 0)
                            {
                                // TODO: Investigate if it's enough to have one property set, (to save time/memory)
                                // and only register the Free function once (letting the first instance "own" it)
                                lua_properties = PropertyContainerCopy(*instance_properties);
                            }
                        }

                        if (!success)
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
                                success = false;
                                break;
                            }
                        }
                        else
                        {
                            properties = ddf_properties ? ddf_properties : lua_properties;
                        }

                        ComponentSetPropertiesParams params;
                        params.m_Instance = instance;

                        if (properties != 0x0)
                        {
                            params.m_PropertySet.m_GetPropertyCallback = PropertyContainerGetPropertyCallback;
                            params.m_PropertySet.m_FreeUserDataCallback = PropertyContainerDestroyCallback;
                            params.m_PropertySet.m_UserData = (uintptr_t)properties;
                        }

                        uintptr_t* component_instance_data = &instance->m_ComponentInstanceUserData[component_instance_data_index];
                        params.m_UserData = component_instance_data;

                        PropertyResult result = type->m_SetPropertiesFunction(params);
                        if (result != PROPERTY_RESULT_OK)
                        {
                            DM_HASH_REVERSE_MEM(hash_ctx, 256);
                            dmLogError("Could not load properties for component '%s' when spawning '%s' in collection '%s'.", dmHashReverseSafe64Alloc(&hash_ctx, component.m_Id), instance_desc.m_Id, collection_desc->m_Name);
                            PropertyContainerDestroy(properties);
                            success = false;
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
                success = false;
            }
        }

        if (success)
        {
            for (uint32_t i=0;i!=created.Size();i++)
            {
                if (!InitInstance(collection, created[i]))
                {
                    success = false;
                    break;
                }
            }
        }

        if (!success)
        {
            // Fail cleanup
            for (uint32_t i=0;i!=created.Size();i++)
                dmGameObject::Delete(collection, created[i], false);
            id_mapping->Clear();
            return false;
        }

        for (uint32_t i=0;i!=created.Size();i++)
        {
            AddToUpdate(collection, created[i]);
        }

        return true;
    }

    bool SpawnFromCollection(HCollection hcollection, HCollectionDesc collection_desc, InstancePropertyBuffers *property_buffers,
                             const Point3& position, const Quat& rotation, const Vector3& scale,
                             InstanceIdMap *instances)
    {
        dmTransform::Transform transform;
        transform.SetTranslation(Vector3(position));
        transform.SetRotation(rotation);
        transform.SetScale(scale);

        bool success = CollectionSpawnFromDescInternal(hcollection->m_Collection, (dmGameObjectDDF::CollectionDesc*)collection_desc, property_buffers, instances, transform);

        return success;
    }

    HInstance Spawn(HCollection hcollection, HPrototype proto, const char* prototype_name, dmhash_t id, HPropertyContainer property_container, const Point3& position, const Quat& rotation, const Vector3& scale)
    {
        if (proto == 0x0) {
            dmLogError("No prototype to spawn from.");
            return 0x0;
        }

        HInstance instance = SpawnInternal(hcollection->m_Collection, proto, prototype_name, id, property_container, position, rotation, scale);

        if (instance == 0) {
            dmLogError("Could not spawn an instance of prototype %s.", prototype_name);
        }

        return instance;
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

    static bool InitComponents(Collection* collection, HInstance instance)
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
                params.m_Instance = instance;
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

    static bool InitInstance(Collection* collection, HInstance instance)
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
                if (instance->m_ScaleAlongZ)
                {
                    *trans = (*parent_trans) * dmTransform::ToMatrix4(instance->m_Transform);
                }
                else
                {
                    *trans = dmTransform::MulNoScaleZ(*parent_trans, dmTransform::ToMatrix4(instance->m_Transform));
                }
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
        // Update scripts
        uint32_t count = collection->m_InstanceIndices.Size();
        for (uint32_t i = 0; i < count; ++i) {
            Instance* instance = collection->m_Instances[i];
            if (!InitInstance(collection, instance)) {
                result = false;
            }
        }
        for (uint32_t i = 0; i < count; ++i) {
            Instance* instance = collection->m_Instances[i];
            if (!DoAddToUpdate(collection, instance)) {
                result = false;
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
        return InitCollection(hcollection->m_Collection);
    }

    static bool FinalComponents(Collection* collection, HInstance instance)
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
                params.m_Instance = instance;
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

    static bool FinalInstance(Collection* collection, HInstance instance)
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
        return FinalCollection(hcollection->m_Collection);
    }

    void Delete(Collection* collection, HInstance instance, bool recursive)
    {
        assert(collection->m_Instances[instance->m_Index] == instance);
        assert(instance->m_Collection == collection);

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

        uint16_t index = instance->m_Index;
        uint16_t tail = collection->m_InstancesToDeleteTail;
        if (tail != INVALID_INSTANCE_INDEX) {
            HInstance tail_instance = collection->m_Instances[tail];
            tail_instance->m_NextToDelete = index;
        } else {
            collection->m_InstancesToDeleteHead = index;
        }
        collection->m_InstancesToDeleteTail = index;
    }

    void Delete(HCollection hcollection, HInstance instance, bool recursive)
    {
        Delete(hcollection->m_Collection, instance, recursive);
    }

    static void RemoveFromAddToUpdate(Collection* collection, HInstance instance)
    {
        uint16_t index = instance->m_Index;
        assert(collection->m_InstancesToAddTail == index || instance->m_NextToAdd != INVALID_INSTANCE_INDEX);
        uint16_t* prev_index_ptr = &collection->m_InstancesToAddHead;
        uint16_t prev_index = *prev_index_ptr;
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

    static void DoDeleteInstance(Collection* collection, HInstance instance)
    {
        DM_PROFILE("DoDeleteInstance");
        HCollection hcollection = collection->m_HCollection;
        CancelAnimations(hcollection, instance);
        if (instance->m_ToBeAdded) {
            RemoveFromAddToUpdate(collection, instance);
        }
        dmResource::HFactory factory = collection->m_Factory;
        Prototype* prototype = instance->m_Prototype;
        DestroyComponents(collection, instance);

        dmHashRelease64(&instance->m_CollectionPathHashState);
        if(instance->m_Generated)
        {
            dmHashReverseErase64(instance->m_Identifier);
        }

        if (instance->m_IdentifierIndex < collection->m_MaxInstances)
        {
            // The identifier (hash) for this gameobject comes from the pool!
            ReleaseInstanceIndex(instance->m_IdentifierIndex, hcollection);
        }
        ReleaseIdentifier(collection, instance);

        assert(collection->m_LevelIndices[instance->m_Depth].Size() > 0);
        assert(instance->m_LevelIndex < collection->m_LevelIndices[instance->m_Depth].Size());

        ReparentChildNodes(collection, instance);

        // Unlink "me" from parent
        Unlink(collection, instance);
        EraseSwapLevelIndex(collection, instance);
        MoveAllUp(collection, instance);

        if (prototype != &EMPTY_PROTOTYPE)
            dmResource::Release(factory, prototype);
        collection->m_InstanceIndices.Push(instance->m_Index);
        collection->m_Instances[instance->m_Index] = 0;

        // Erase from input stack
        bool found_instance = false;
        for (uint32_t i = 0; i < collection->m_InputFocusStack.Size(); ++i)
        {
            if (collection->m_InputFocusStack[i] == instance)
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
        Collection* collection = hcollection->m_Collection;
        for (uint32_t i = 0; i < collection->m_Instances.Size(); ++i)
        {
            Instance* instance = collection->m_Instances[i];
            if (instance)
            {
                Delete(hcollection, instance, false);
            }
        }
    }

    Result SetIdentifier(Collection* collection, HInstance instance, const char* identifier)
    {
        dmhash_t id = dmHashBuffer64(identifier, strlen(identifier));
        return SetIdentifier(collection, instance, id);
    }

    Result SetIdentifier(HCollection hcollection, HInstance instance, const char* identifier)
    {
        return SetIdentifier(hcollection->m_Collection, instance, identifier);
    }

    dmhash_t GetIdentifier(HInstance instance)
    {
        return instance->m_Identifier;
    }

    dmhash_t GetAbsoluteIdentifier(HInstance instance, const char* id, uint32_t id_size)
    {
        // check for global id (/foo/bar)
        if (*id == *ID_SEPARATOR)
        {
            return dmHashBuffer64(id, id_size);
        }
        else
        {
            // Make a copy of the state.
            HashState64 tmp_state;
            dmHashClone64(&tmp_state, &instance->m_CollectionPathHashState, false);
            dmHashUpdateBuffer64(&tmp_state, id, id_size);
            return dmHashFinal64(&tmp_state);
        }
    }

    HInstance GetInstanceFromIdentifier(Collection* collection, dmhash_t identifier)
    {
        Instance** instance = collection->m_IDToInstance.Get(identifier);
        if (instance)
            return *instance;
        else
            return 0;
    }

    HInstance GetInstanceFromIdentifier(HCollection hcollection, dmhash_t identifier)
    {
        return GetInstanceFromIdentifier(hcollection->m_Collection, identifier);
    }

    Result GetComponentIndex(HInstance instance, dmhash_t component_id, uint16_t* component_index)
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

    Result GetComponentId(HInstance instance, uint16_t component_index, dmhash_t* component_id)
    {
        assert(instance != 0x0);
        if (component_index < instance->m_Prototype->m_ComponentCount)
        {
            *component_id = instance->m_Prototype->m_Components[component_index].m_Id;
            return RESULT_OK;
        }
        return RESULT_COMPONENT_NOT_FOUND;
    }

    Result GetComponent(HInstance instance, dmhash_t component_id, uint32_t* component_type, HComponent* out_component, HComponentWorld* out_world)
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
                    world = GetWorld(GetCollection(instance), component->m_TypeIndex);
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

    bool ScaleAlongZ(HInstance instance)
    {
        return instance->m_ScaleAlongZ != 0;
    }

    bool ScaleAlongZ(HCollection hcollection)
    {
        return hcollection->m_Collection->m_ScaleAlongZ != 0;
    }

    void SetBone(HInstance instance, bool bone)
    {
        instance->m_Bone = bone;
    }

    bool IsBone(HInstance instance)
    {
        return instance->m_Bone;
    }

    static uint32_t DoSetBoneTransforms(HCollection hcollection, dmTransform::Transform* component_transform, uint16_t first_index, dmTransform::Transform* transforms, uint32_t transform_count)
    {
        if (transform_count == 0)
            return 0;
        uint16_t current_index = first_index;
        uint32_t count = 0;
        Collection* collection = hcollection->m_Collection;
        while (current_index != INVALID_INSTANCE_INDEX)
        {
            HInstance instance = collection->m_Instances[current_index];
            if (instance->m_Bone)
            {
                instance->m_Transform = transforms[count++];
                if (component_transform && count == 1) {
                    instance->m_Transform = dmTransform::Mul(*component_transform, instance->m_Transform);
                }
                if (count < transform_count)
                {
                    count += DoSetBoneTransforms(hcollection, 0x0, instance->m_FirstChildIndex, &transforms[count], transform_count - count);
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

    uint32_t SetBoneTransforms(HInstance instance, dmTransform::Transform& component_transform, dmTransform::Transform* transforms, uint32_t transform_count)
    {
        return DoSetBoneTransforms(instance->m_Collection->m_HCollection, &component_transform, instance->m_Index, transforms, transform_count);
    }

    static void DeleteBones(Collection* collection, uint16_t first_index) {
        uint16_t current_index = first_index;
        while (current_index != INVALID_INSTANCE_INDEX) {
            HInstance instance = collection->m_Instances[current_index];
            if (instance->m_Bone && instance->m_ToBeDeleted == 0) {
                DeleteBones(collection, instance->m_FirstChildIndex);
                // Delete children first, to avoid any unnecessary re-parenting
                Delete(collection, instance, false);
            }
            current_index = instance->m_SiblingIndex;
        }
    }

    void DeleteBones(HInstance parent) {
        return DeleteBones(parent->m_Collection, parent->m_FirstChildIndex);
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
                dmGameObject::HInstance parent = 0;
                if (sp->m_ParentId != 0)
                {
                    parent = dmGameObject::GetInstanceFromIdentifier(context->m_Collection, sp->m_ParentId);
                    if (parent == 0)
                        dmLogWarning("Could not find parent instance with id '%s'.", dmHashReverseSafe64(sp->m_ParentId));

                }
                Matrix4 parent_t = Matrix4::identity();

                if (parent)
                {
                    parent_t = collection->m_WorldTransforms[parent->m_Index];
                }

                if (sp->m_KeepWorldTransform == 0)
                {
                    Matrix4& world = collection->m_WorldTransforms[instance->m_Index];
                    if (instance->m_ScaleAlongZ)
                    {
                        world = parent_t * dmTransform::ToMatrix4(instance->m_Transform);
                    }
                    else
                    {
                        world = dmTransform::MulNoScaleZ(parent_t, dmTransform::ToMatrix4(instance->m_Transform));
                    }
                }
                else
                {
                    if (instance->m_ScaleAlongZ)
                    {
                        instance->m_Transform = dmTransform::ToTransform(inverse(parent_t) * collection->m_WorldTransforms[instance->m_Index]);
                    }
                    else
                    {
                        Matrix4 tmp = dmTransform::MulNoScaleZ(inverse(parent_t), collection->m_WorldTransforms[instance->m_Index]);
                        instance->m_Transform = dmTransform::ToTransform(tmp);
                    }
                }

                dmGameObject::Result result = dmGameObject::SetParent(instance, parent);

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
                    params.m_Instance = instance;
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
                        params.m_Instance = instance;
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
                    collection->m_DirtyTransforms = true;
                    iterate = true;
                }
            }
            ++iteration_count;
        }

        return ctx.m_Success;
    }

    bool DispatchMessages(HCollection hcollection, dmMessage::HSocket* sockets, uint32_t socket_count)
    {
        return DispatchMessages(hcollection->m_Collection, sockets, socket_count);
    }

    static void UpdateEulerToRotation(HInstance instance);

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
        dmArray<uint16_t>& root_level = collection->m_LevelIndices[0];
        uint32_t root_count = root_level.Size();
        for (uint32_t i = 0; i < root_count; ++i)
        {
            uint16_t index = root_level[i];
            Instance* instance = collection->m_Instances[index];
            CheckEuler(instance);
            collection->m_WorldTransforms[index] = dmTransform::ToMatrix4(instance->m_Transform);
            uint16_t parent_index = instance->m_Parent;
            assert(parent_index == INVALID_INSTANCE_INDEX);
        }


        if (collection->m_ScaleAlongZ) {
            for (uint32_t level_i = 1; level_i < MAX_HIERARCHICAL_DEPTH; ++level_i)
            {
                dmArray<uint16_t>& level = collection->m_LevelIndices[level_i];
                uint32_t instance_count = level.Size();
                for (uint32_t i = 0; i < instance_count; ++i)
                {
                    uint16_t index = level[i];
                    Instance* instance = collection->m_Instances[index];
                    CheckEuler(instance);
                    Matrix4* trans = &collection->m_WorldTransforms[index];

                    uint16_t parent_index = instance->m_Parent;
                    assert(parent_index != INVALID_INSTANCE_INDEX);

                    Matrix4* parent_trans = &collection->m_WorldTransforms[parent_index];
                    Matrix4 own = dmTransform::ToMatrix4(instance->m_Transform);
                    *trans = *parent_trans * own;
                }
            }
        } else {
            for (uint32_t level_i = 1; level_i < MAX_HIERARCHICAL_DEPTH; ++level_i)
            {
                dmArray<uint16_t>& level = collection->m_LevelIndices[level_i];
                uint32_t instance_count = level.Size();
                for (uint32_t i = 0; i < instance_count; ++i)
                {
                    uint16_t index = level[i];
                    Instance* instance = collection->m_Instances[index];
                    CheckEuler(instance);
                    Matrix4* trans = &collection->m_WorldTransforms[index];

                    uint16_t parent_index = instance->m_Parent;
                    assert(parent_index != INVALID_INSTANCE_INDEX);

                    Matrix4* parent_trans = &collection->m_WorldTransforms[parent_index];
                    Matrix4 own = dmTransform::ToMatrix4(instance->m_Transform);
                    *trans = dmTransform::MulNoScaleZ(*parent_trans, own);
                }
            }
        }

        collection->m_DirtyTransforms = false;
    }

    void UpdateTransforms(HCollection hcollection)
    {
        UpdateTransforms(hcollection->m_Collection);
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

        uint32_t component_types = collection->m_Register->m_ComponentTypeCount;
        for (uint32_t i = 0; i < component_types; ++i)
        {
            uint16_t update_index = collection->m_Register->m_ComponentTypesOrder[i];
            ComponentType* component_type = &collection->m_Register->m_ComponentTypes[update_index];

            // Avoid to call UpdateTransforms for each/all component types.
            if (component_type->m_ReadsTransforms && collection->m_DirtyTransforms) {
                UpdateTransforms(collection);
            }

            if (component_type->m_UpdateFunction)
            {
                DM_PROFILE_DYN(component_type->m_Name, 0);
                ComponentsUpdateParams params;
                params.m_Collection = collection->m_HCollection;
                params.m_UpdateContext = &dynamic_update_context;
                params.m_World = collection->m_ComponentWorlds[update_index];
                params.m_Context = component_type->m_Context;

                ComponentsUpdateResult update_result;
                update_result.m_TransformsUpdated = false;
                UpdateResult res = component_type->m_UpdateFunction(params, update_result);
                if (res != UPDATE_RESULT_OK)
                    ret = false;

                // Mark the collections transforms as dirty if this component has updated
                // them in its update function.
                collection->m_DirtyTransforms |= update_result.m_TransformsUpdated;
            }

            if (!DispatchMessages(collection, &collection->m_ComponentSocket, 1))
            {
                ret = false;
            }
        }

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
            uint32_t num_fixed_steps = (uint32_t)(time / fixed_dt);
            // Store the remainder for the next frame
            collection->m_FixedAccumTime = time - (num_fixed_steps * fixed_dt);

            if (num_fixed_steps != 0)
            {
                UpdateContext fixed_update_context;
                fixed_update_context = dynamic_update_context;
                fixed_update_context.m_DT = fixed_dt;

                for (uint32_t step = 0; step < num_fixed_steps; ++step)
                {
                    for (uint32_t i = 0; i < component_types; ++i)
                    {
                        uint16_t update_index = collection->m_Register->m_ComponentTypesOrder[i];
                        ComponentType* component_type = &collection->m_Register->m_ComponentTypes[update_index];

                        // Avoid to call UpdateTransforms for each/all component types.
                        if (component_type->m_ReadsTransforms && collection->m_DirtyTransforms) {
                            UpdateTransforms(collection);
                        }

                        if (component_type->m_FixedUpdateFunction)
                        {
                            DM_PROFILE_DYN(component_type->m_Name, 0);
                            ComponentsUpdateParams params;
                            params.m_Collection = collection->m_HCollection;
                            params.m_UpdateContext = &fixed_update_context;
                            params.m_World = collection->m_ComponentWorlds[update_index];
                            params.m_Context = component_type->m_Context;

                            ComponentsUpdateResult update_result;
                            update_result.m_TransformsUpdated = false;
                            UpdateResult res = component_type->m_FixedUpdateFunction(params, update_result);
                            if (res != UPDATE_RESULT_OK)
                                ret = false;

                            // Mark the collections transforms as dirty if this component has updated
                            // them in its update function.
                            collection->m_DirtyTransforms |= update_result.m_TransformsUpdated;
                        }

                        if (!DispatchMessages(collection, &collection->m_ComponentSocket, 1))
                        {
                            ret = false;
                        }
                    }
                }

            }
        }

        collection->m_InUpdate = 0;
        if (collection->m_DirtyTransforms) {
            UpdateTransforms(collection);
        }

        return ret;
    }

    bool Update(HCollection hcollection, const UpdateContext* update_context)
    {
        return Update(hcollection->m_Collection, update_context);
    }

    bool Render(HCollection hcollection)
    {
        DM_PROFILE("Render");

        Collection* collection = hcollection->m_Collection;
        assert(collection != 0x0);

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
        HRegister reg = collection->m_Register;
        assert(reg);

        bool result = true;

        uint32_t component_types = reg->m_ComponentTypeCount;
        for (uint32_t i = 0; i < component_types; ++i)
        {
            uint16_t update_index = reg->m_ComponentTypesOrder[i];
            ComponentType* component_type = &reg->m_ComponentTypes[update_index];

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
                uint16_t head = collection->m_InstancesToDeleteHead;
                collection->m_InstancesToDeleteHead = INVALID_INSTANCE_INDEX;
                collection->m_InstancesToDeleteTail = INVALID_INSTANCE_INDEX;

                uint16_t index = head;
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
        return PostUpdate(hcollection->m_Collection);
    }

    bool PostUpdate(HRegister reg)
    {
        DM_PROFILE("PostUpdateRegister");

        assert(reg != 0x0);

        bool result = true;

        uint32_t collection_count = reg->m_Collections.Size();
        uint32_t i = 0;
        while (i < collection_count)
        {
            Collection* collection = reg->m_Collections[i];
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
                    HInstance instance = collection->m_InputFocusStack[stack_size - 1 - k];
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
                            params.m_Instance = instance;
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
        return DispatchInput(hcollection->m_Collection, input_actions, input_action_count);
    }

    void AcquireInputFocus(Collection* collection, HInstance instance)
    {
        bool found = false;
        for (uint32_t i = 0; i < collection->m_InputFocusStack.Size(); ++i)
        {
            if (collection->m_InputFocusStack[i] == instance)
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
            collection->m_InputFocusStack.Push(instance);
        }
        else
        {
            dmLogWarning("Input focus could not be acquired since the buffer is full (%d).", collection->m_InputFocusStack.Size());
        }
    }

    void ReleaseInputFocus(Collection* collection, HInstance instance)
    {
        bool found = false;
        for (uint32_t i = 0; i < collection->m_InputFocusStack.Size(); ++i)
        {
            if (collection->m_InputFocusStack[i] == instance)
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

    void AcquireInputFocus(HCollection hcollection, HInstance instance)
    {
        AcquireInputFocus(hcollection->m_Collection, instance);
    }

    void ReleaseInputFocus(HCollection hcollection, HInstance instance)
    {
        ReleaseInputFocus(hcollection->m_Collection, instance);
    }

    HCollection GetCollection(HInstance instance)
    {
        return instance->m_Collection->m_HCollection;
    }

    dmResource::HFactory GetFactory(HCollection hcollection)
    {
        if (hcollection && hcollection->m_Collection)
            return hcollection->m_Collection->m_Factory;
        else
            return 0x0;
    }

    dmResource::HFactory GetFactory(HInstance instance)
    {
        return instance->m_Collection->m_Factory;
    }

    HRegister GetRegister(HCollection hcollection)
    {
        if (hcollection && hcollection->m_Collection)
            return hcollection->m_Collection->m_Register;
        else
            return 0x0;
    }

    dmMessage::HSocket GetMessageSocket(HCollection hcollection)
    {
        if (hcollection && hcollection->m_Collection)
            return hcollection->m_Collection->m_ComponentSocket;
        else
            return 0;
    }

    dmMessage::HSocket GetFrameMessageSocket(HCollection hcollection)
    {
        if (hcollection && hcollection->m_Collection)
            return hcollection->m_Collection->m_FrameSocket;
        else
            return 0;
    }

    void SetPosition(HInstance instance, Point3 position)
    {
        instance->m_Transform.SetTranslation(Vector3(position));
    }

    Point3 GetPosition(HInstance instance)
    {
        return Point3(instance->m_Transform.GetTranslation());
    }

    void SetRotation(HInstance instance, Quat rotation)
    {
        instance->m_Transform.SetRotation(rotation);
    }

    Quat GetRotation(HInstance instance)
    {
        return instance->m_Transform.GetRotation();
    }

    void SetScale(HInstance instance, float scale)
    {
        instance->m_Transform.SetUniformScale(scale);
    }

    void SetScale(HInstance instance, Vector3 scale)
    {
        instance->m_Transform.SetScale(scale);
    }

    float GetUniformScale(HInstance instance)
    {
        return instance->m_Transform.GetUniformScale();
    }

    Vector3 GetScale(HInstance instance)
    {
        return instance->m_Transform.GetScale();
    }

    Point3 GetWorldPosition(HInstance instance)
    {
        Vector4 translation = instance->m_Collection->m_WorldTransforms[instance->m_Index].getCol(3);
        return Point3(translation.getX(), translation.getY(), translation.getZ());
    }

    Quat GetWorldRotation(HInstance instance)
    {
        Matrix4 world_transform = instance->m_Collection->m_WorldTransforms[instance->m_Index];
        dmTransform::ResetScale(&world_transform);
        return Quat(world_transform.getUpper3x3());
    }

    float GetWorldUniformScale(HInstance instance)
    {
        Vector3 scale = GetWorldScale(instance);
        return dmMath::Max<float>(scale.getX(), dmMath::Max<float>(scale.getY(), scale.getZ()));
    }

    Vector3 GetWorldScale(HInstance instance)
    {
        return dmTransform::ExtractScale(instance->m_Collection->m_WorldTransforms[instance->m_Index]);
    }

    /*
        dmTransform::ToMatrix a dmTransform::Transform from the world transform.
        When this is not possible, nonsense will be returned.
    */
    dmTransform::Transform GetWorldTransform(HInstance instance)
    {
        Matrix4 mtx = instance->m_Collection->m_WorldTransforms[instance->m_Index];
        return dmTransform::ToTransform(mtx);
    }

    const Matrix4 & GetWorldMatrix(HInstance instance)
    {
        return instance->m_Collection->m_WorldTransforms[instance->m_Index];
    }

    Result SetParent(HInstance child, HInstance parent)
    {
        if (parent == 0 && child->m_Parent == INVALID_INSTANCE_INDEX)
            return RESULT_OK;

        if (parent != 0 && parent->m_Depth >= MAX_HIERARCHICAL_DEPTH-1)
        {
            dmLogError("Unable to set parent to child. Parent at maximum depth %d", MAX_HIERARCHICAL_DEPTH-1);
            return RESULT_MAXIMUM_HIEARCHICAL_DEPTH;
        }

        Collection* collection = child->m_Collection;

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
            assert(child->m_Collection == parent->m_Collection);
            assert(collection->m_LevelIndices[child->m_Depth+1].Size() < collection->m_MaxInstances);
        }
        else
        {
            assert(collection->m_LevelIndices[0].Size() < collection->m_MaxInstances);
        }

        if (child->m_Parent != INVALID_INSTANCE_INDEX)
        {
            Unlink(collection, child);
        }

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

        return RESULT_OK;
    }

    HInstance GetParent(HInstance instance)
    {
        if (instance->m_Parent == INVALID_INSTANCE_INDEX)
        {
            return 0;
        }
        else
        {
            return instance->m_Collection->m_Instances[instance->m_Parent];
        }
    }

    uint32_t GetDepth(HInstance instance)
    {
        return instance->m_Depth;
    }

    uint32_t GetChildCount(HInstance instance)
    {
        Collection* collection = instance->m_Collection;
        uint32_t count = 0;
        uint32_t index = instance->m_FirstChildIndex;
        while (index != INVALID_INSTANCE_INDEX)
        {
            ++count;
            index = collection->m_Instances[index]->m_SiblingIndex;
        }

        return count;
    }

    bool IsChildOf(HInstance child, HInstance parent)
    {
        Collection* collection = parent->m_Collection;
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

    static void UpdateRotationToEuler(HInstance instance)
    {
        Quat q = instance->m_Transform.GetRotation();
        instance->m_EulerRotation = dmVMath::QuatToEuler(q.getX(), q.getY(), q.getZ(), q.getW());
        instance->m_PrevEulerRotation = instance->m_EulerRotation;
    }

    static void UpdateEulerToRotation(HInstance instance)
    {
        instance->m_PrevEulerRotation = instance->m_EulerRotation;
        instance->m_Transform.SetRotation(dmVMath::EulerToQuat(instance->m_EulerRotation));
    }

    PropertyResult GetProperty(HInstance instance, dmhash_t component_id, dmhash_t property_id, PropertyOptions options, PropertyDesc& out_value)
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
                    p.m_World = instance->m_Collection->m_ComponentWorlds[component.m_TypeIndex];
                    p.m_Instance = instance;
                    p.m_PropertyId = property_id;
                    p.m_Options = options;
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

    PropertyResult SetProperty(HInstance instance, dmhash_t component_id, dmhash_t property_id, PropertyOptions options, const PropertyVar& value)
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
                return PROPERTY_RESULT_OK;
            }
            else if (property_id == PROP_POSITION_X)
            {
                if (value.m_Type != PROPERTY_TYPE_NUMBER)
                    return PROPERTY_RESULT_TYPE_MISMATCH;
                position[0] = (float)value.m_Number;
                return PROPERTY_RESULT_OK;
            }
            else if (property_id == PROP_POSITION_Y)
            {
                if (value.m_Type != PROPERTY_TYPE_NUMBER)
                    return PROPERTY_RESULT_TYPE_MISMATCH;
                position[1] = (float)value.m_Number;
                return PROPERTY_RESULT_OK;
            }
            else if (property_id == PROP_POSITION_Z)
            {
                if (value.m_Type != PROPERTY_TYPE_NUMBER)
                    return PROPERTY_RESULT_TYPE_MISMATCH;
                position[2] = (float)value.m_Number;
                return PROPERTY_RESULT_OK;
            }
            else if (property_id == PROP_SCALE)
            {
                if (value.m_Type == PROPERTY_TYPE_NUMBER)
                {
                    scale[0] = (float)value.m_Number;
                    scale[1] = scale[0];
                    scale[2] = scale[0];
                    return PROPERTY_RESULT_OK;
                }
                else if (value.m_Type == PROPERTY_TYPE_VECTOR3)
                {
                    scale[0] = value.m_V4[0];
                    scale[1] = value.m_V4[1];
                    scale[2] = value.m_V4[2];
                    return PROPERTY_RESULT_OK;
                }
                return PROPERTY_RESULT_TYPE_MISMATCH;
            }
            else if (property_id == PROP_SCALE_X)
            {
                if (value.m_Type != PROPERTY_TYPE_NUMBER)
                    return PROPERTY_RESULT_TYPE_MISMATCH;
                scale[0] = (float)value.m_Number;
                return PROPERTY_RESULT_OK;
            }
            else if (property_id == PROP_SCALE_Y)
            {
                if (value.m_Type != PROPERTY_TYPE_NUMBER)
                    return PROPERTY_RESULT_TYPE_MISMATCH;
                scale[1] = (float)value.m_Number;
                return PROPERTY_RESULT_OK;
            }
            else if (property_id == PROP_SCALE_Z)
            {
                if (value.m_Type != PROPERTY_TYPE_NUMBER)
                    return PROPERTY_RESULT_TYPE_MISMATCH;
                scale[2] = (float)value.m_Number;
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
                return PROPERTY_RESULT_OK;
            }
            else if (property_id == PROP_ROTATION_X)
            {
                if (value.m_Type != PROPERTY_TYPE_NUMBER)
                    return PROPERTY_RESULT_TYPE_MISMATCH;
                rotation[0] = (float)value.m_Number;
                return PROPERTY_RESULT_OK;
            }
            else if (property_id == PROP_ROTATION_Y)
            {
                if (value.m_Type != PROPERTY_TYPE_NUMBER)
                    return PROPERTY_RESULT_TYPE_MISMATCH;
                rotation[1] = (float)value.m_Number;
                return PROPERTY_RESULT_OK;
            }
            else if (property_id == PROP_ROTATION_Z)
            {
                if (value.m_Type != PROPERTY_TYPE_NUMBER)
                    return PROPERTY_RESULT_TYPE_MISMATCH;
                rotation[2] = (float)value.m_Number;
                return PROPERTY_RESULT_OK;
            }
            else if (property_id == PROP_ROTATION_W)
            {
                if (value.m_Type != PROPERTY_TYPE_NUMBER)
                    return PROPERTY_RESULT_TYPE_MISMATCH;
                rotation[3] = (float)value.m_Number;
                return PROPERTY_RESULT_OK;
            }
            else if (property_id == PROP_EULER)
            {
                if (value.m_Type != PROPERTY_TYPE_VECTOR3)
                    return PROPERTY_RESULT_TYPE_MISMATCH;
                instance->m_EulerRotation = Vector3(value.m_V4[0], value.m_V4[1], value.m_V4[2]);
                UpdateEulerToRotation(instance);
                return PROPERTY_RESULT_OK;
            }
            else if (property_id == PROP_EULER_X)
            {
                if (value.m_Type != PROPERTY_TYPE_NUMBER)
                    return PROPERTY_RESULT_TYPE_MISMATCH;
                instance->m_EulerRotation.setX((float)value.m_Number);
                UpdateEulerToRotation(instance);
                return PROPERTY_RESULT_OK;
            }
            else if (property_id == PROP_EULER_Y)
            {
                if (value.m_Type != PROPERTY_TYPE_NUMBER)
                    return PROPERTY_RESULT_TYPE_MISMATCH;
                instance->m_EulerRotation.setY((float)value.m_Number);
                UpdateEulerToRotation(instance);
                return PROPERTY_RESULT_OK;
            }
            else if (property_id == PROP_EULER_Z)
            {
                if (value.m_Type != PROPERTY_TYPE_NUMBER)
                    return PROPERTY_RESULT_TYPE_MISMATCH;
                instance->m_EulerRotation.setZ((float)value.m_Number);
                UpdateEulerToRotation(instance);
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
                    p.m_World = instance->m_Collection->m_ComponentWorlds[component.m_TypeIndex];
                    p.m_Instance = instance;
                    p.m_PropertyId = property_id;
                    p.m_UserData = user_data;
                    p.m_Value = value;
                    p.m_Options = options;
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

    // Recreate the instance at the given index with a new prototype.
    // Specifically:
    //  - recreate components and call init/final functions
    //  - patch data structures for identification and input stack
    //  - copy the rest of the fields
    // The old instance is destroyed.
    static void RecreateInstance(Collection* collection, uint16_t index, Prototype* old_proto, Prototype* new_proto, const char* new_proto_name) {
        HInstance instance = collection->m_Instances[index];
        // We don't support recreating instances that are 'transitioning'
        assert(instance->m_ToBeAdded == 0);
        assert(instance->m_ToBeDeleted == 0);
        HInstance new_instance = AllocInstance(new_proto, new_proto_name);
        if (!new_instance) {
            return;
        }
        new_instance->m_Collection = instance->m_Collection;
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
        new_instance->m_ScaleAlongZ = instance->m_ScaleAlongZ;
        // id-related
        new_instance->m_Identifier = instance->m_Identifier;
        new_instance->m_IdentifierIndex = instance->m_IdentifierIndex;
        dmHashClone64(&new_instance->m_CollectionPathHashState, &instance->m_CollectionPathHashState, true);
        new_instance->m_Generated = instance->m_Generated;
        HCollection hcollection = collection->m_HCollection;
        bool res = CreateComponents(hcollection, new_instance);
        if (!res) {
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
        if (instance->m_Initialized) {
            FinalComponents(collection, instance);
        }
        DestroyComponents(collection, instance);
        dmHashRelease64(&instance->m_CollectionPathHashState);
        collection->m_Instances[index] = new_instance;
        collection->m_IDToInstance.Put(new_instance->m_Identifier, new_instance);

        dmArray<Instance*>& stack = collection->m_InputFocusStack;
        uint32_t n_stack = stack.Size();
        for (uint32_t i = 0; i < n_stack; ++i) {
            if (stack[i] == instance) {
                stack[i] = new_instance;
                // instances are only permitted one slot in the stack
                break;
            }
        }
        DeallocInstance(instance);
        DoAddToUpdate(collection, new_instance);
    }

    static void ResourceReloadedCallback(const ResourceReloadedParams* params)
    {
        Collection* collection = (Collection*) params->m_UserData;
        for (uint32_t level_i = 0; level_i < MAX_HIERARCHICAL_DEPTH; ++level_i)
        {
            dmArray<uint16_t>& level = collection->m_LevelIndices[level_i];
            uint32_t instance_count = level.Size();
            for (uint32_t i = 0; i < instance_count; ++i)
            {
                uint16_t index = level[i];
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
                                on_reload_params.m_Instance = instance;
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
        Collection* collection = hcollection->m_Collection;
        uint32_t count = 0;
        uint16_t index = collection->m_InstancesToAddHead;
        while (index != INVALID_INSTANCE_INDEX) {
            index = collection->m_Instances[index]->m_NextToAdd;
            ++count;
        }
        return count;
    }

    uint32_t GetRemoveFromUpdateCount(HCollection hcollection)
    {
        Collection* collection = hcollection->m_Collection;
        uint32_t count = 0;
        uint16_t index = collection->m_InstancesToDeleteHead;
        while (index != INVALID_INSTANCE_INDEX) {
            index = collection->m_Instances[index]->m_NextToDelete;
            ++count;
        }
        return count;
    }
}
