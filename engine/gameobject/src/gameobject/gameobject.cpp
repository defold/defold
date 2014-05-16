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
#include "res_collection.h"
#include "res_prototype.h"
#include "res_script.h"
#include "res_lua.h"
#include "res_anim.h"

#include "../proto/gameobject_ddf.h"

namespace dmGameObject
{
    const dmhash_t UNNAMED_IDENTIFIER = dmHashBuffer64("__unnamed__", strlen("__unnamed__"));
    const char* ID_SEPARATOR = "/";
    const uint32_t MAX_DISPATCH_ITERATION_COUNT = 5;

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
    PROP_FLOAT(SCALE, scale);

    InputAction::InputAction()
    {
        memset(this, 0, sizeof(InputAction));
    }

    PropertyVar::PropertyVar()
    {
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

    PropertyVar::PropertyVar(Vectormath::Aos::Vector3 v)
    {
        m_Type = PROPERTY_TYPE_VECTOR3;
        m_V4[0] = v.getX();
        m_V4[1] = v.getY();
        m_V4[2] = v.getZ();
    }

    PropertyVar::PropertyVar(Vectormath::Aos::Vector4 v)
    {
        m_Type = PROPERTY_TYPE_VECTOR4;
        m_V4[0] = v.getX();
        m_V4[1] = v.getY();
        m_V4[2] = v.getZ();
        m_V4[3] = v.getW();
    }

    PropertyVar::PropertyVar(Vectormath::Aos::Quat v)
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

    Register::Register()
    {
        m_ComponentTypeCount = 0;
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

    void Initialize(dmScript::HContext context, dmResource::HFactory factory)
    {
        InitializeScript(context, factory);
    }

    void Finalize(dmScript::HContext context, dmResource::HFactory factory)
    {
        FinalizeScript(context, factory);
    }

    HRegister NewRegister()
    {
        return new Register();
    }

    void DoDeleteCollection(HCollection collection);

    void DeleteRegister(HRegister regist)
    {
        uint32_t collection_count = regist->m_Collections.Size();
        for (uint32_t i = 0; i < collection_count; ++i)
        {
            // TODO Note indexing of m_Collections is always 0 because DoDeleteCollection modifies the array.
            // Should be fixed by DEF-54
            DoDeleteCollection(regist->m_Collections[0]);
        }
        delete regist;
    }

    void ResourceReloadedCallback(void* user_data, dmResource::SResourceDescriptor* descriptor, const char* name);

    HCollection NewCollection(const char* name, dmResource::HFactory factory, HRegister regist, uint32_t max_instances)
    {
        if (max_instances > INVALID_INSTANCE_INDEX)
        {
            dmLogError("max_instances must be less or equal to %d", INVALID_INSTANCE_INDEX);
            return 0;
        }
        Collection* collection = new Collection(factory, regist, max_instances);

        for (uint32_t i = 0; i < regist->m_ComponentTypeCount; ++i)
        {
            if (regist->m_ComponentTypes[i].m_NewWorldFunction)
            {
                ComponentNewWorldParams params;
                params.m_Context = regist->m_ComponentTypes[i].m_Context;
                params.m_World = &collection->m_ComponentWorlds[i];
                regist->m_ComponentTypes[i].m_NewWorldFunction(params);
            }
        }

        collection->m_NameHash = dmHashString64(name);

        dmMutex::Lock(regist->m_Mutex);
        if (regist->m_Collections.Full())
        {
            regist->m_Collections.OffsetCapacity(4);
        }
        regist->m_Collections.Push(collection);
        dmMutex::Unlock(regist->m_Mutex);

        collection->m_Mutex = dmMutex::New();

        dmResource::RegisterResourceReloadedCallback(factory, ResourceReloadedCallback, collection);

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
                DeleteCollection(collection);
                return 0;
            }
        }

        return collection;
    }

    void DoDeleteAll(HCollection collection);

    void DoDeleteCollection(HCollection collection)
    {
        Final(collection);
        DoDeleteAll(collection);
        HRegister regist = collection->m_Register;
        for (uint32_t i = 0; i < regist->m_ComponentTypeCount; ++i)
        {
            ComponentDeleteWorldParams params;
            params.m_Context = regist->m_ComponentTypes[i].m_Context;
            params.m_World = collection->m_ComponentWorlds[i];
            if (regist->m_ComponentTypes[i].m_DeleteWorldFunction)
                regist->m_ComponentTypes[i].m_DeleteWorldFunction(params);
        }

        dmMutex::Lock(regist->m_Mutex);
        bool found = false;
        for (uint32_t i = 0; i < regist->m_Collections.Size(); ++i)
        {
            // TODO This design is not really thought through, since it modifies the m_Collections
            // member in a hidden context. Reported in DEF-54
            if (regist->m_Collections[i] == collection)
            {
                regist->m_Collections.EraseSwap(i);
                found = true;
                break;
            }
        }
        assert(found);
        dmMutex::Unlock(regist->m_Mutex);

        dmMutex::Delete(collection->m_Mutex);

        dmResource::UnregisterResourceReloadedCallback(collection->m_Factory, ResourceReloadedCallback, collection);

        if (collection->m_ComponentSocket)
        {
            dmMessage::Consume(collection->m_ComponentSocket);
            dmMessage::DeleteSocket(collection->m_ComponentSocket);
        }
        if (collection->m_FrameSocket)
        {
            dmMessage::Consume(collection->m_FrameSocket);
            dmMessage::DeleteSocket(collection->m_FrameSocket);
        }

        delete collection;
    }

    void DeleteCollection(HCollection collection)
    {
        collection->m_ToBeDeleted = 1;
    }

    void* GetWorld(HCollection collection, uint32_t component_index)
    {
        if (component_index < MAX_COMPONENT_TYPES)
        {
            return collection->m_ComponentWorlds[component_index];
        }
        else
        {
            return 0x0;
        }
    }

    ComponentType* FindComponentType(Register* regist, uint32_t resource_type, uint32_t* index)
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

        regist->m_ComponentTypes[regist->m_ComponentTypeCount] = type;
        regist->m_ComponentTypesOrder[regist->m_ComponentTypeCount] = regist->m_ComponentTypeCount;
        regist->m_ComponentTypeCount++;
        return RESULT_OK;
    }

    Result SetUpdateOrderPrio(HRegister regist, uint32_t resource_type, uint16_t prio)
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

    dmResource::Result RegisterResourceTypes(dmResource::HFactory factory, HRegister regist)
    {
        dmResource::Result ret = dmResource::RESULT_OK;
        ret = dmResource::RegisterType(factory, "goc", (void*)regist, &ResPrototypeCreate, &ResPrototypeDestroy, 0);
        if (ret != dmResource::RESULT_OK)
            return ret;

        ret = dmResource::RegisterType(factory, "scriptc", 0, &ResScriptCreate, &ResScriptDestroy, &ResScriptRecreate);
        if (ret != dmResource::RESULT_OK)
            return ret;

        ret = dmResource::RegisterType(factory, "luac", 0, &ResLuaCreate, &ResLuaDestroy, &ResLuaRecreate);
        if (ret != dmResource::RESULT_OK)
            return ret;

        ret = dmResource::RegisterType(factory, "collectionc", regist, &ResCollectionCreate, &ResCollectionDestroy, 0);
        if (ret != dmResource::RESULT_OK)
            return ret;

        ret = dmResource::RegisterType(factory, "animc", 0, &ResAnimCreate, &ResAnimDestroy, 0x0);
        if (ret != dmResource::RESULT_OK)
            return ret;

        return ret;
    }

    static void EraseSwapLevelIndex(HCollection collection, HInstance instance)
    {
        /*
         * Remove instance from m_LevelIndices using an erase-swap operation
         */

        assert(collection->m_LevelInstanceCount[instance->m_Depth] > 0);
        assert(instance->m_LevelIndex < collection->m_LevelInstanceCount[instance->m_Depth]);

        uint32_t abs_level_index = instance->m_Depth * collection->m_MaxInstances + instance->m_LevelIndex;
        uint32_t abs_level_index_last = instance->m_Depth * collection->m_MaxInstances + (collection->m_LevelInstanceCount[instance->m_Depth]-1);

        assert(collection->m_LevelIndices[abs_level_index] != INVALID_INSTANCE_INDEX);

        uint32_t swap_in_index = collection->m_LevelIndices[abs_level_index_last];
        HInstance swap_in_instance = collection->m_Instances[swap_in_index];
        assert(swap_in_instance->m_Index == swap_in_index);
        // Remove index in m_LevelIndices using an "erase-swap operation"
        collection->m_LevelIndices[abs_level_index] = collection->m_LevelIndices[abs_level_index_last];
        collection->m_LevelIndices[abs_level_index_last] = INVALID_INSTANCE_INDEX;
        collection->m_LevelInstanceCount[instance->m_Depth]--;
        swap_in_instance->m_LevelIndex = abs_level_index - instance->m_Depth * collection->m_MaxInstances;
    }

    static void InsertInstanceInLevelIndex(HCollection collection, HInstance instance)
    {
        /*
         * Insert instance in m_LevelIndices at level set in instance->m_Depth
         */

        instance->m_LevelIndex = collection->m_LevelInstanceCount[instance->m_Depth];
        assert(instance->m_LevelIndex < collection->m_MaxInstances);

        assert(collection->m_LevelInstanceCount[instance->m_Depth] < collection->m_MaxInstances);
        uint32_t abs_level_index = instance->m_Depth * collection->m_MaxInstances + collection->m_LevelInstanceCount[instance->m_Depth];
        assert(collection->m_LevelIndices[abs_level_index] == INVALID_INSTANCE_INDEX);
        collection->m_LevelIndices[abs_level_index] = instance->m_Index;

        collection->m_LevelInstanceCount[instance->m_Depth]++;
    }

    HInstance New(HCollection collection, const char* prototype_name)
    {
        assert(collection->m_InUpdate == 0 && "Creating new instances during Update(.) is not permitted");
        Prototype* proto;
        dmResource::HFactory factory = collection->m_Factory;
        dmResource::Result error = dmResource::Get(factory, prototype_name, (void**)&proto);
        if (error != dmResource::RESULT_OK)
        {
            return 0;
        }

        if (collection->m_InstanceIndices.Remaining() == 0)
        {
            dmLogError("Unable to create instance. Out of resources");
            return 0;
        }

        // Count number of component userdata fields required
        uint32_t component_instance_userdata_count = 0;
        for (uint32_t i = 0; i < proto->m_Components.Size(); ++i)
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
        instance->m_Collection = collection;
        instance->m_ScaleAlongZ = collection->m_ScaleAlongZ;
        uint16_t instance_index = collection->m_InstanceIndices.Pop();
        instance->m_Index = instance_index;
        assert(collection->m_Instances[instance_index] == 0);
        collection->m_Instances[instance_index] = instance;

        uint32_t components_created = 0;
        uint32_t next_component_instance_data = 0;
        bool ok = true;
        for (uint32_t i = 0; i < proto->m_Components.Size(); ++i)
        {
            Prototype::Component* component = &proto->m_Components[i];
            ComponentType* component_type = component->m_Type;
            assert(component_type);

            uintptr_t* component_instance_data = 0;
            if (component_type->m_InstanceHasUserData)
            {
                component_instance_data = &instance->m_ComponentInstanceUserData[next_component_instance_data++];
                *component_instance_data = 0;
            }
            assert(next_component_instance_data <= instance->m_ComponentInstanceUserDataCount);

            ComponentCreateParams params;
            params.m_Collection = collection;
            params.m_Instance = instance;
            params.m_Position = component->m_Position;
            params.m_Rotation = component->m_Rotation;
            params.m_ComponentIndex = (uint8_t)i;
            params.m_Resource = component->m_Resource;
            params.m_World = collection->m_ComponentWorlds[component->m_TypeIndex];
            params.m_Context = component_type->m_Context;
            params.m_UserData = component_instance_data;
            params.m_PropertySet = component->m_PropertySet;
            CreateResult create_result =  component_type->m_CreateFunction(params);
            if (create_result == CREATE_RESULT_OK)
            {
                collection->m_ComponentInstanceCount[component->m_TypeIndex]++;
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

                collection->m_ComponentInstanceCount[component->m_TypeIndex]--;
                ComponentDestroyParams params;
                params.m_Collection = collection;
                params.m_Instance = instance;
                params.m_World = collection->m_ComponentWorlds[component->m_TypeIndex];
                params.m_Context = component_type->m_Context;
                params.m_UserData = component_instance_data;
                component_type->m_DestroyFunction(params);
            }

            // We can not call Delete here. Delete call DestroyFunction for every component
            dmResource::Release(factory, instance->m_Prototype);
            operator delete (instance_memory);
            collection->m_Instances[instance_index] = 0x0;
            collection->m_InstanceIndices.Push(instance_index);
            return 0;
        }

        InsertInstanceInLevelIndex(collection, instance);

        return instance;
    }

    bool Init(HCollection collection, HInstance instance);

    dmhash_t GenerateUniqueInstanceId(HCollection collection)
    {
        // global path
        const char* id_format = "%sinstance%d";
        char id_s[16];
        uint32_t index = 0;
        dmMutex::Lock(collection->m_Mutex);
        index = collection->m_GenInstanceCounter++;
        dmMutex::Unlock(collection->m_Mutex);
        DM_SNPRINTF(id_s, sizeof(id_s), id_format, ID_SEPARATOR, index);
        return dmHashString64(id_s);
    }

    Result SetIdentifier(HCollection collection, HInstance instance, dmhash_t id)
    {
        if (collection->m_IDToInstance.Get(id))
            return RESULT_IDENTIFIER_IN_USE;

        if (instance->m_Identifier != UNNAMED_IDENTIFIER)
            return RESULT_IDENTIFIER_ALREADY_SET;

        instance->m_Identifier = id;
        collection->m_IDToInstance.Put(id, instance);

        return RESULT_OK;
    }

    HInstance Spawn(HCollection collection, const char* prototype_name, dmhash_t id, uint8_t* property_buffer, uint32_t property_buffer_size, const Point3& position, const Quat& rotation, float scale)
    {
        if (collection->m_InUpdate)
        {
            dmLogError("Spawning during update is not allowed, %s was never spawned.", prototype_name);
            return 0;
        }
        HInstance instance = New(collection, prototype_name);
        if (instance != 0)
        {
            SetPosition(instance, position);
            SetRotation(instance, rotation);
            SetScale(instance, scale);

            dmHashInit64(&instance->m_CollectionPathHashState, true);
            dmHashUpdateBuffer64(&instance->m_CollectionPathHashState, ID_SEPARATOR, strlen(ID_SEPARATOR));

            Result result = SetIdentifier(collection, instance, id);
            if (result == RESULT_IDENTIFIER_IN_USE)
            {
                const char* identifier = (const char*)dmHashReverse64(id, 0x0);
                if (identifier != 0x0)
                {
                    dmLogError("The identifier '%s' is already in use.", identifier);
                }
                else
                {
                    dmLogError("The identifier '%llu' is already in use.", id);
                }
                Delete(collection, instance);
                instance = 0;
            }
            else
            {
                uint32_t next_component_instance_data = 0;
                dmArray<Prototype::Component>& components = instance->m_Prototype->m_Components;
                uint32_t count = components.Size();
                for (uint32_t i = 0; i < count; ++i)
                {
                    Prototype::Component& component = components[i];
                    ComponentType* component_type = component.m_Type;
                    uintptr_t* component_instance_data = 0;
                    if (component_type->m_InstanceHasUserData)
                    {
                        component_instance_data = &instance->m_ComponentInstanceUserData[next_component_instance_data++];
                    }
                    // TODO use the component type identification system once it has been implemented (related to set_tile for tile maps)
                    if (strcmp(component.m_Type->m_Name, "scriptc") == 0 && component.m_Type->m_SetPropertiesFunction != 0x0)
                    {
                        ComponentSetPropertiesParams params;
                        params.m_Instance = instance;
                        params.m_UserData = component_instance_data;
                        PropertyResult result = CreatePropertySetUserDataLua(GetLuaState(), property_buffer, property_buffer_size, &params.m_PropertySet.m_UserData);
                        if (result == PROPERTY_RESULT_OK)
                        {
                            params.m_PropertySet.m_FreeUserDataCallback = DestroyPropertySetUserDataLua;
                            params.m_PropertySet.m_GetPropertyCallback = GetPropertyCallbackLua;
                            result = component.m_Type->m_SetPropertiesFunction(params);
                        }
                        if (result != PROPERTY_RESULT_OK)
                        {
                            dmLogError("Could not load properties when spawning '%s'.", prototype_name);
                            Delete(collection, instance);
                            return 0;
                        }
                    }
                }
                if (instance != 0 && !Init(collection, instance))
                {
                    dmLogError("Could not initialize when spawning %s.", prototype_name);
                    Delete(collection, instance);
                    return 0;
                }
            }
        }
        if (instance == 0)
        {
            dmLogError("Could not spawn an instance of prototype %s.", prototype_name);
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

    void UpdateTransforms(HCollection collection);

    bool Init(HCollection collection, HInstance instance)
    {
        if (instance)
        {
            if (instance->m_Initialized)
            {
                dmLogWarning("%s", "Instance is initialized twice, this may lead to undefined behaviour.");
            }
            else
            {
                instance->m_Initialized = 1;
            }

            assert(collection->m_Instances[instance->m_Index] == instance);

            // Update world transforms since some components might need them in their init-callback
            dmTransform::TransformS1* trans = &collection->m_WorldTransforms[instance->m_Index];
            if (instance->m_Parent == INVALID_INSTANCE_INDEX)
            {
                *trans = instance->m_Transform;
            }
            else
            {
                const dmTransform::TransformS1* parent_trans = &collection->m_WorldTransforms[instance->m_Parent];
                if (instance->m_ScaleAlongZ)
                {
                    *trans = dmTransform::Mul(*parent_trans, instance->m_Transform);
                }
                else
                {
                    *trans = dmTransform::MulNoScaleZ(*parent_trans, instance->m_Transform);
                }
            }

            uint32_t next_component_instance_data = 0;
            Prototype* prototype = instance->m_Prototype;
            for (uint32_t i = 0; i < prototype->m_Components.Size(); ++i)
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
                    params.m_Collection = collection;
                    params.m_Instance = instance;
                    params.m_World = collection->m_ComponentWorlds[component->m_TypeIndex];
                    params.m_Context = component_type->m_Context;
                    params.m_UserData = component_instance_data;
                    CreateResult result = component_type->m_InitFunction(params);
                    if (result != CREATE_RESULT_OK)
                    {
                        return false;
                    }
                }
            }
        }

        return true;
    }

    bool DispatchMessages(HCollection collection, dmMessage::HSocket* sockets, uint32_t socket_count);

    bool Init(HCollection collection)
    {
        DM_PROFILE(GameObject, "Init");

        assert(collection->m_InUpdate == 0 && "Initializing instances during Update(.) is not permitted");

        // Update trasform cache
        UpdateTransforms(collection);

        bool result = true;
        // Update scripts
        uint32_t n_objects = collection->m_Instances.Size();
        for (uint32_t i = 0; i < n_objects; ++i)
        {
            Instance* instance = collection->m_Instances[i];
            if ( ! Init(collection, instance) )
            {
                result = false;
            }
        }
        dmMessage::HSocket sockets[] = {collection->m_ComponentSocket, collection->m_FrameSocket};
        if (!DispatchMessages(collection, sockets, 2))
            result = false;

        return result;
    }

    bool Final(HCollection collection, HInstance instance)
    {
        if (instance)
        {
            if (instance->m_Initialized)
                instance->m_Initialized = 0;
            else
                dmLogWarning("%s", "Instance is finalized without being initialized, this may lead to undefined behaviour.");

            assert(collection->m_Instances[instance->m_Index] == instance);

            uint32_t next_component_instance_data = 0;
            Prototype* prototype = instance->m_Prototype;
            for (uint32_t i = 0; i < prototype->m_Components.Size(); ++i)
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
                    params.m_Collection = collection;
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
        }

        return true;
    }

    bool Final(HCollection collection)
    {
        DM_PROFILE(GameObject, "Final");

        assert(collection->m_InUpdate == 0 && "Finalizing instances during Update(.) is not permitted");

        bool result = true;
        uint32_t n_objects = collection->m_Instances.Size();
        for (uint32_t i = 0; i < n_objects; ++i)
        {
            Instance* instance = collection->m_Instances[i];
            if (instance != 0x0 && instance->m_Initialized && ! Final(collection, instance))
            {
                result = false;
            }
        }

        return result;
    }

    void Delete(HCollection collection, HInstance instance)
    {
        assert(collection->m_Instances[instance->m_Index] == instance);
        assert(instance->m_Collection == collection);

        // NOTE: Do not add for delete twice.
        if (instance->m_ToBeDeleted)
            return;

        instance->m_ToBeDeleted = 1;
        collection->m_InstancesToDelete.Push(instance->m_Index);
    }

    void DoDelete(HCollection collection, HInstance instance)
    {
        CancelAnimations(collection, instance);
        dmResource::HFactory factory = collection->m_Factory;
        uint32_t next_component_instance_data = 0;
        Prototype* prototype = instance->m_Prototype;
        for (uint32_t i = 0; i < prototype->m_Components.Size(); ++i)
        {
            Prototype::Component* component = &prototype->m_Components[i];
            ComponentType* component_type = component->m_Type;

            uintptr_t* component_instance_data = 0;
            if (component_type->m_InstanceHasUserData)
            {
                component_instance_data = &instance->m_ComponentInstanceUserData[next_component_instance_data++];
            }
            assert(next_component_instance_data <= instance->m_ComponentInstanceUserDataCount);

            collection->m_ComponentInstanceCount[component->m_TypeIndex]--;
            ComponentDestroyParams params;
            params.m_Collection = collection;
            params.m_Instance = instance;
            params.m_World = collection->m_ComponentWorlds[component->m_TypeIndex];
            params.m_Context = component_type->m_Context;
            params.m_UserData = component_instance_data;
            component_type->m_DestroyFunction(params);
        }

        if (instance->m_Identifier != UNNAMED_IDENTIFIER)
            collection->m_IDToInstance.Erase(instance->m_Identifier);

        assert(collection->m_LevelInstanceCount[instance->m_Depth] > 0);
        assert(instance->m_LevelIndex < collection->m_LevelInstanceCount[instance->m_Depth]);

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

        // Unlink "me" from parent
        Unlink(collection, instance);
        EraseSwapLevelIndex(collection, instance);
        MoveAllUp(collection, instance);

        dmResource::Release(factory, instance->m_Prototype);
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

        instance->~Instance();
        void* instance_memory = (void*) instance;

        // This is required for failing test
        // TODO: #ifdef on something...?
        // Clear all memory excluding ComponentInstanceUserData
        memset(instance_memory, 0xcc, sizeof(Instance));
        operator delete (instance_memory);
    }

    void DeleteAll(HCollection collection)
    {
        for (uint32_t i = 0; i < collection->m_Instances.Size(); ++i)
        {
            Instance* instance = collection->m_Instances[i];
            if (instance)
            {
                Delete(collection, instance);
            }
        }
    }

    void DoDeleteAll(HCollection collection)
    {
        // This will perform tons of unnecessary work to resolve and reorder
        // the hierarchies and other things but will serve as a nice test case
        for (uint32_t i = 0; i < collection->m_Instances.Size(); ++i)
        {
            Instance* instance = collection->m_Instances[i];
            if (instance)
            {
                DoDelete(collection, instance);
            }
        }
    }

    Result SetIdentifier(HCollection collection, HInstance instance, const char* identifier)
    {
        dmhash_t id = dmHashBuffer64(identifier, strlen(identifier));
        return SetIdentifier(collection, instance, id);
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
            HashState64 tmp_state = instance->m_CollectionPathHashState;
            dmHashUpdateBuffer64(&tmp_state, id, id_size);
            return dmHashFinal64(&tmp_state);
        }
    }

    HInstance GetInstanceFromIdentifier(HCollection collection, dmhash_t identifier)
    {
        Instance** instance = collection->m_IDToInstance.Get(identifier);
        if (instance)
            return *instance;
        else
            return 0;
    }

    Result GetComponentIndex(HInstance instance, dmhash_t component_id, uint8_t* component_index)
    {
        assert(instance != 0x0);
        for (uint32_t i = 0; i < instance->m_Prototype->m_Components.Size(); ++i)
        {
            Prototype::Component* component = &instance->m_Prototype->m_Components[i];
            if (component->m_Id == component_id)
            {
                *component_index = (uint8_t)i & 0xff;
                return RESULT_OK;
            }
        }
        return RESULT_COMPONENT_NOT_FOUND;
    }

    Result GetComponentId(HInstance instance, uint8_t component_index, dmhash_t* component_id)
    {
        assert(instance != 0x0);
        if (component_index < instance->m_Prototype->m_Components.Size())
        {
            *component_id = instance->m_Prototype->m_Components[component_index].m_Id;
            return RESULT_OK;
        }
        return RESULT_COMPONENT_NOT_FOUND;
    }

    bool ScaleAlongZ(HInstance instance)
    {
        return instance->m_ScaleAlongZ != 0;
    }

    struct DispatchMessagesContext
    {
        HCollection m_Collection;
        bool m_Success;
    };

    void DispatchMessagesFunction(dmMessage::Message *message, void* user_ptr)
    {
        DispatchMessagesContext* context = (DispatchMessagesContext*) user_ptr;

        Instance* instance = 0x0;
        // Start by looking for the instance in the user-data,
        // which is the case when an instance sends to itself.
        if (message->m_UserData != 0
                && message->m_Sender.m_Socket == message->m_Receiver.m_Socket
                && message->m_Sender.m_Path == message->m_Receiver.m_Path)
        {
            Instance* user_data_instance = (Instance*)message->m_UserData;
            if (message->m_Receiver.m_Path == user_data_instance->m_Identifier)
            {
                instance = user_data_instance;
            }
        }
        if (instance == 0x0)
        {
            instance = GetInstanceFromIdentifier(context->m_Collection, message->m_Receiver.m_Path);
        }
        if (instance == 0x0)
        {
            const dmMessage::URL* sender = &message->m_Sender;
            const char* socket_name = dmMessage::GetSocketName(sender->m_Socket);
            const char* path_name = (const char*) dmHashReverse64(sender->m_Path, 0);
            const char* fragment_name = (const char*) dmHashReverse64(sender->m_Fragment, 0);

            dmLogError("Instance '%s' could not be found when dispatching message '%s' sent from %s:%s#%s",
                        (const char*) dmHashReverse64(message->m_Receiver.m_Path, 0),
                        (const char*) dmHashReverse64(message->m_Id, 0),
                        socket_name, path_name, fragment_name);

            context->m_Success = false;
            return;
        }
        if (message->m_Descriptor != 0)
        {
            dmDDF::Descriptor* descriptor = (dmDDF::Descriptor*)message->m_Descriptor;
            if (descriptor == dmGameObjectDDF::AcquireInputFocus::m_DDFDescriptor)
            {
                dmGameObject::AcquireInputFocus(context->m_Collection, instance);
                return;
            }
            else if (descriptor == dmGameObjectDDF::ReleaseInputFocus::m_DDFDescriptor)
            {
                dmGameObject::ReleaseInputFocus(context->m_Collection, instance);
                return;
            }
            else if (descriptor == dmGameObjectDDF::RequestTransform::m_DDFDescriptor)
            {
                dmGameObjectDDF::TransformResponse response;
                response.m_Position = dmGameObject::GetPosition(instance);
                response.m_Rotation = dmGameObject::GetRotation(instance);
                response.m_Scale = dmGameObject::GetScale(instance);
                response.m_WorldPosition = dmGameObject::GetWorldPosition(instance);
                response.m_WorldRotation = dmGameObject::GetWorldRotation(instance);
                response.m_WorldScale = dmGameObject::GetWorldScale(instance);
                dmhash_t message_id = dmGameObjectDDF::TransformResponse::m_DDFDescriptor->m_NameHash;
                uintptr_t gotr_descriptor = (uintptr_t)dmGameObjectDDF::TransformResponse::m_DDFDescriptor;
                uint32_t data_size = sizeof(dmGameObjectDDF::TransformResponse);
                if (dmMessage::IsSocketValid(message->m_Sender.m_Socket))
                {
                    dmMessage::Result message_result = dmMessage::Post(&message->m_Receiver, &message->m_Sender, message_id, message->m_UserData, gotr_descriptor, &response, data_size);
                    if (message_result != dmMessage::RESULT_OK)
                    {
                        dmLogError("Could not send message '%s' to sender: %d.", dmGameObjectDDF::TransformResponse::m_DDFDescriptor->m_Name, message_result);
                    }
                }
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
                        dmLogWarning("Could not find parent instance with id '%s'.", (const char*) dmHashReverse64(sp->m_ParentId, 0));

                }
                dmTransform::TransformS1 parent_t;
                parent_t.SetIdentity();
                if (parent)
                {
                    parent_t = context->m_Collection->m_WorldTransforms[parent->m_Index];
                }
                if (sp->m_KeepWorldTransform == 0)
                {
                    dmTransform::TransformS1& world = context->m_Collection->m_WorldTransforms[instance->m_Index];
                    if (instance->m_ScaleAlongZ)
                    {
                        world = dmTransform::Mul(parent_t, instance->m_Transform);
                    }
                    else
                    {
                        world = dmTransform::MulNoScaleZ(parent_t, instance->m_Transform);
                    }
                }
                else
                {
                    if (instance->m_ScaleAlongZ)
                    {
                        instance->m_Transform = dmTransform::Mul(dmTransform::Inv(parent_t), context->m_Collection->m_WorldTransforms[instance->m_Index]);
                    }
                    else
                    {
                        instance->m_Transform = dmTransform::MulNoScaleZ(dmTransform::Inv(parent_t), context->m_Collection->m_WorldTransforms[instance->m_Index]);
                    }
                }
                dmGameObject::Result result = dmGameObject::SetParent(instance, parent);

                if (result != dmGameObject::RESULT_OK)
                    dmLogWarning("Error when setting parent of '%s' to '%s', error: %i.",
                                 (const char*) dmHashReverse64(instance->m_Identifier, 0),
                                 (const char*) dmHashReverse64(sp->m_ParentId, 0),
                                 result);
                return;
            }
        }
        Prototype* prototype = instance->m_Prototype;

        if (message->m_Receiver.m_Fragment != 0)
        {
            uint8_t component_index;
            Result result = GetComponentIndex(instance, message->m_Receiver.m_Fragment, &component_index);
            if (result != RESULT_OK)
            {
                const dmMessage::URL* sender = &message->m_Sender;
                const char* socket_name = dmMessage::GetSocketName(sender->m_Socket);
                const char* path_name = (const char*) dmHashReverse64(sender->m_Path, 0);
                const char* fragment_name = (const char*) dmHashReverse64(sender->m_Fragment, 0);

                dmLogError("Component '%s#%s' could not be found when dispatching message '%s' sent from %s:%s#%s",
                            (const char*) dmHashReverse64(message->m_Receiver.m_Path, 0),
                            (const char*) dmHashReverse64(message->m_Receiver.m_Fragment, 0),
                            (const char*) dmHashReverse64(message->m_Id, 0),
                            socket_name, path_name, fragment_name);
                context->m_Success = false;
                return;
            }
            Prototype::Component* component = &prototype->m_Components[component_index];
            ComponentType* component_type = component->m_Type;
            assert(component_type);
            uint32_t component_type_index = component->m_TypeIndex;

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
                    DM_PROFILE(GameObject, "OnMessageFunction");
                    ComponentOnMessageParams params;
                    params.m_Instance = instance;
                    params.m_World = context->m_Collection->m_ComponentWorlds[component_type_index];
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
            for (uint32_t i = 0; i < prototype->m_Components.Size(); ++i)
            {
                Prototype::Component* component = &prototype->m_Components[i];
                ComponentType* component_type = component->m_Type;
                assert(component_type);
                uint32_t component_type_index = component->m_TypeIndex;

                if (component_type->m_OnMessageFunction)
                {
                    uintptr_t* component_instance_data = 0;
                    if (component_type->m_InstanceHasUserData)
                    {
                        component_instance_data = &instance->m_ComponentInstanceUserData[next_component_instance_data++];
                    }
                    {
                        DM_PROFILE(GameObject, "OnMessageFunction");
                        ComponentOnMessageParams params;
                        params.m_Instance = instance;
                        params.m_World = context->m_Collection->m_ComponentWorlds[component_type_index];
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

    bool DispatchMessages(HCollection collection, dmMessage::HSocket* sockets, uint32_t socket_count)
    {
        DM_PROFILE(GameObject, "DispatchMessages");

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
                if (dmMessage::IsSocketValid(sockets[i]))
                {
                    uint32_t message_count = dmMessage::Dispatch(sockets[i], &DispatchMessagesFunction, (void*) &ctx);
                    if (message_count > 0)
                        iterate = true;
                }
            }
            ++iteration_count;
        }

        return ctx.m_Success;
    }

    static void UpdateEulerToRotation(HInstance instance);

    static void CheckEuler(Instance* instance)
    {
        Vector3& euler = instance->m_EulerRotation;
        Vector3& prev_euler = instance->m_PrevEulerRotation;
        if (lengthSqr(euler - prev_euler) != 0.0f)
        {
            UpdateEulerToRotation(instance);
            prev_euler = euler;
        }
    }

    void UpdateTransforms(HCollection collection)
    {
        DM_PROFILE(GameObject, "UpdateTransforms");

        // Calculate world transforms
        // First root-level instances
        for (uint32_t i = 0; i < collection->m_LevelInstanceCount[0]; ++i)
        {
            uint16_t index = collection->m_LevelIndices[i];
            Instance* instance = collection->m_Instances[index];
            CheckEuler(instance);
            collection->m_WorldTransforms[index] = instance->m_Transform;
            uint16_t parent_index = instance->m_Parent;
            assert(parent_index == INVALID_INSTANCE_INDEX);
        }

        // World-transform for levels 1..MAX_HIERARCHICAL_DEPTH-1
        for (uint32_t level = 1; level < MAX_HIERARCHICAL_DEPTH; ++level)
        {
            uint32_t max_instance = collection->m_Instances.Size();
            for (uint32_t i = 0; i < collection->m_LevelInstanceCount[level]; ++i)
            {
                uint16_t index = collection->m_LevelIndices[level * max_instance + i];
                Instance* instance = collection->m_Instances[index];
                CheckEuler(instance);
                dmTransform::TransformS1* trans = &collection->m_WorldTransforms[index];

                uint16_t parent_index = instance->m_Parent;
                assert(parent_index != INVALID_INSTANCE_INDEX);

                dmTransform::TransformS1* parent_trans = &collection->m_WorldTransforms[parent_index];

                if (instance->m_ScaleAlongZ)
                {
                    *trans = dmTransform::Mul(*parent_trans, instance->m_Transform);
                }
                else
                {
                    *trans = dmTransform::MulNoScaleZ(*parent_trans, instance->m_Transform);
                }
            }
        }
    }

    bool Update(HCollection collection, const UpdateContext* update_context)
    {
        DM_PROFILE(GameObject, "Update");

        assert(collection != 0x0);

        collection->m_InUpdate = 1;

        bool ret = true;

        uint32_t component_types = collection->m_Register->m_ComponentTypeCount;
        for (uint32_t i = 0; i < component_types; ++i)
        {
            uint16_t update_index = collection->m_Register->m_ComponentTypesOrder[i];
            ComponentType* component_type = &collection->m_Register->m_ComponentTypes[update_index];

            DM_COUNTER(component_type->m_Name, collection->m_ComponentInstanceCount[update_index]);

            if (component_type->m_UpdateFunction)
            {
                DM_PROFILE(GameObject, component_type->m_Name);
                ComponentsUpdateParams params;
                params.m_Collection = collection;
                params.m_UpdateContext = update_context;
                params.m_World = collection->m_ComponentWorlds[update_index];
                params.m_Context = component_type->m_Context;
                UpdateResult res = component_type->m_UpdateFunction(params);
                if (res != UPDATE_RESULT_OK)
                    ret = false;
            }
            // TODO: Solve this better! Right now the worst is assumed, which is that every component updates some transforms as well as
            // demands updated child-transforms. Many redundant calculations. This could be solved by splitting the component Update-callback
            // into UpdateTrasform, then Update or similar
            UpdateTransforms(collection);

            if (!DispatchMessages(collection, &collection->m_ComponentSocket, 1))
                ret = false;
        }

        collection->m_InUpdate = 0;

        return ret;
    }

    bool PostUpdate(HCollection collection)
    {
        DM_PROFILE(GameObject, "PostUpdate");

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
                DM_PROFILE(GameObject, component_type->m_Name);
                ComponentsPostUpdateParams params;
                params.m_Collection = collection;
                params.m_World = collection->m_ComponentWorlds[update_index];
                params.m_Context = component_type->m_Context;
                UpdateResult res = component_type->m_PostUpdateFunction(params);
                if (res != UPDATE_RESULT_OK && result)
                    result = false;
            }
        }

        if (collection->m_InstancesToDelete.Size() > 0)
        {
            uint32_t n_to_delete = collection->m_InstancesToDelete.Size();
            for (uint32_t j = 0; j < n_to_delete; ++j)
            {
                uint16_t index = collection->m_InstancesToDelete[j];
                Instance* instance = collection->m_Instances[index];

                assert(collection->m_Instances[instance->m_Index] == instance);
                assert(instance->m_ToBeDeleted);
                if (instance->m_Initialized)
                    if (!Final(collection, instance) && result)
                        result = false;

            }
        }

        dmMessage::HSocket sockets[] =
        {
                // Some components might have sent messages in their final()
                collection->m_ComponentSocket,
                // Frame dispatch, handle e.g. spawning
                collection->m_FrameSocket
        };
        if (!DispatchMessages(collection, sockets, 2))
            result = false;

        if (collection->m_InstancesToDelete.Size() > 0)
        {
            uint32_t n_to_delete = collection->m_InstancesToDelete.Size();
            for (uint32_t j = 0; j < n_to_delete; ++j)
            {
                uint16_t index = collection->m_InstancesToDelete[j];
                Instance* instance = collection->m_Instances[index];

                assert(collection->m_Instances[instance->m_Index] == instance);
                assert(instance->m_ToBeDeleted);
                DoDelete(collection, instance);
            }
            collection->m_InstancesToDelete.SetSize(0);
        }

        return result;
    }

    bool PostUpdate(HRegister reg)
    {
        DM_PROFILE(GameObject, "PostUpdateRegister");

        assert(reg != 0x0);

        bool result = true;

        uint32_t collection_count = reg->m_Collections.Size();
        uint32_t i = 0;
        while (i < collection_count)
        {
            HCollection collection = reg->m_Collections[i];
            if (collection->m_ToBeDeleted)
            {
                DoDeleteCollection(collection);
                --collection_count;
            }
            else
            {
                ++i;
            }
        }

        return result;
    }

    UpdateResult DispatchInput(HCollection collection, InputAction* input_actions, uint32_t input_action_count)
    {
        DM_PROFILE(GameObject, "DispatchInput");
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
                    uint32_t components_size = prototype->m_Components.Size();

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
                        break;
                    }
                }
            }
        }
        return UPDATE_RESULT_OK;
    }

    void AcquireInputFocus(HCollection collection, HInstance instance)
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

    void ReleaseInputFocus(HCollection collection, HInstance instance)
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

    HCollection GetCollection(HInstance instance)
    {
        return instance->m_Collection;
    }

    dmResource::HFactory GetFactory(HCollection collection)
    {
        if (collection != 0x0)
            return collection->m_Factory;
        else
            return 0x0;
    }

    HRegister GetRegister(HCollection collection)
    {
        if (collection != 0x0)
            return collection->m_Register;
        else
            return 0x0;
    }

    dmMessage::HSocket GetMessageSocket(HCollection collection)
    {
        if (collection)
            return collection->m_ComponentSocket;
        else
            return 0;
    }

    dmMessage::HSocket GetFrameMessageSocket(HCollection collection)
    {
        if (collection)
            return collection->m_FrameSocket;
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
        instance->m_Transform.SetScale(scale);
    }

    float GetScale(HInstance instance)
    {
        return instance->m_Transform.GetScale();
    }

    Point3 GetWorldPosition(HInstance instance)
    {
        HCollection collection = instance->m_Collection;
        return Point3(collection->m_WorldTransforms[instance->m_Index].GetTranslation());
    }

    Quat GetWorldRotation(HInstance instance)
    {
        HCollection collection = instance->m_Collection;
        return collection->m_WorldTransforms[instance->m_Index].GetRotation();
    }

    float GetWorldScale(HInstance instance)
    {
        HCollection collection = instance->m_Collection;
        return collection->m_WorldTransforms[instance->m_Index].GetScale();
    }

    const dmTransform::TransformS1& GetWorldTransform(HInstance instance)
    {
        HCollection collection = instance->m_Collection;
        return collection->m_WorldTransforms[instance->m_Index];
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

        HCollection collection = child->m_Collection;

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
            assert(collection->m_LevelInstanceCount[child->m_Depth+1] < collection->m_MaxInstances);
        }
        else
        {
            assert(collection->m_LevelInstanceCount[0] < collection->m_MaxInstances);
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
        Collection* c = instance->m_Collection;
        uint32_t count = 0;
        uint32_t index = instance->m_FirstChildIndex;
        while (index != INVALID_INSTANCE_INDEX)
        {
            ++count;
            index = c->m_Instances[index]->m_SiblingIndex;
        }

        return count;
    }

    bool IsChildOf(HInstance child, HInstance parent)
    {
        Collection* c = parent->m_Collection;
        uint32_t index = parent->m_FirstChildIndex;
        while (index != INVALID_INSTANCE_INDEX)
        {
            Instance*i = c->m_Instances[index];
            if (i == child)
                return true;
            index = c->m_Instances[index]->m_SiblingIndex;
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
        Quat q = instance->m_Transform.GetRotation();
        instance->m_PrevEulerRotation = instance->m_EulerRotation;
        instance->m_Transform.SetRotation(dmVMath::EulerToQuat(instance->m_EulerRotation));
    }

    PropertyResult GetProperty(HInstance instance, dmhash_t component_id, dmhash_t property_id, PropertyDesc& out_value)
    {
        if (instance == 0)
            return PROPERTY_RESULT_INVALID_INSTANCE;
        if (component_id == 0)
        {
            float* transform = (float*)&instance->m_Transform;
            out_value.m_ValuePtr = 0x0;
            if (property_id == PROP_POSITION)
            {
                out_value.m_ValuePtr = transform;
                out_value.m_ElementIds[0] = PROP_POSITION_X;
                out_value.m_ElementIds[1] = PROP_POSITION_Y;
                out_value.m_ElementIds[2] = PROP_POSITION_Z;
                out_value.m_Variant = PropertyVar(instance->m_Transform.GetTranslation());
            }
            else if (property_id == PROP_POSITION_X)
            {
                out_value.m_ValuePtr = transform;
                out_value.m_Variant = PropertyVar(*out_value.m_ValuePtr);
            }
            else if (property_id == PROP_POSITION_Y)
            {
                out_value.m_ValuePtr = transform + 1;
                out_value.m_Variant = PropertyVar(*out_value.m_ValuePtr);
            }
            else if (property_id == PROP_POSITION_Z)
            {
                out_value.m_ValuePtr = transform + 2;
                out_value.m_Variant = PropertyVar(*out_value.m_ValuePtr);
            }
            else if (property_id == PROP_SCALE)
            {
                out_value.m_ValuePtr = transform + 3;
                out_value.m_Variant = PropertyVar(*out_value.m_ValuePtr);
            }
            else if (property_id == PROP_ROTATION)
            {
                out_value.m_ValuePtr = transform + 4;
                out_value.m_ElementIds[0] = PROP_ROTATION_X;
                out_value.m_ElementIds[1] = PROP_ROTATION_Y;
                out_value.m_ElementIds[2] = PROP_ROTATION_Z;
                out_value.m_ElementIds[3] = PROP_ROTATION_W;
                out_value.m_Variant = PropertyVar(instance->m_Transform.GetRotation());
            }
            else if (property_id == PROP_ROTATION_X)
            {
                out_value.m_ValuePtr = transform + 4;
                out_value.m_Variant = PropertyVar(*out_value.m_ValuePtr);
            }
            else if (property_id == PROP_ROTATION_Y)
            {
                out_value.m_ValuePtr = transform + 5;
                out_value.m_Variant = PropertyVar(*out_value.m_ValuePtr);
            }
            else if (property_id == PROP_ROTATION_Z)
            {
                out_value.m_ValuePtr = transform + 6;
                out_value.m_Variant = PropertyVar(*out_value.m_ValuePtr);
            }
            else if (property_id == PROP_ROTATION_W)
            {
                out_value.m_ValuePtr = transform + 7;
                out_value.m_Variant = PropertyVar(*out_value.m_ValuePtr);
            }
            else if (property_id == PROP_EULER)
            {
                UpdateRotationToEuler(instance);
                out_value.m_ValuePtr = (float*)&instance->m_EulerRotation;
                out_value.m_ElementIds[0] = PROP_EULER_X;
                out_value.m_ElementIds[1] = PROP_EULER_Y;
                out_value.m_ElementIds[2] = PROP_EULER_Z;
                out_value.m_Variant = PropertyVar(instance->m_EulerRotation);
            }
            else if (property_id == PROP_EULER_X)
            {
                UpdateRotationToEuler(instance);
                out_value.m_ValuePtr = ((float*)&instance->m_EulerRotation);
                out_value.m_Variant = PropertyVar(*out_value.m_ValuePtr);
            }
            else if (property_id == PROP_EULER_Y)
            {
                UpdateRotationToEuler(instance);
                out_value.m_ValuePtr = ((float*)&instance->m_EulerRotation) + 1;
                out_value.m_Variant = PropertyVar(*out_value.m_ValuePtr);
            }
            else if (property_id == PROP_EULER_Z)
            {
                UpdateRotationToEuler(instance);
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
            uint8_t component_index;
            if (RESULT_OK == GetComponentIndex(instance, component_id, &component_index))
            {
                dmArray<Prototype::Component>& components = instance->m_Prototype->m_Components;
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
                    p.m_Instance = instance;
                    p.m_PropertyId = property_id;
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

    PropertyResult SetProperty(HInstance instance, dmhash_t component_id, dmhash_t property_id, const PropertyVar& value)
    {
        if (instance == 0)
            return PROPERTY_RESULT_INVALID_INSTANCE;
        if (component_id == 0)
        {
            float* transform = (float*)&instance->m_Transform;
            if (property_id == PROP_POSITION)
            {
                if (value.m_Type != PROPERTY_TYPE_VECTOR3)
                    return PROPERTY_RESULT_TYPE_MISMATCH;
                transform[0] = value.m_V4[0];
                transform[1] = value.m_V4[1];
                transform[2] = value.m_V4[2];
                return PROPERTY_RESULT_OK;
            }
            else if (property_id == PROP_POSITION_X)
            {
                if (value.m_Type != PROPERTY_TYPE_NUMBER)
                    return PROPERTY_RESULT_TYPE_MISMATCH;
                transform[0] = (float)value.m_Number;
                return PROPERTY_RESULT_OK;
            }
            else if (property_id == PROP_POSITION_Y)
            {
                if (value.m_Type != PROPERTY_TYPE_NUMBER)
                    return PROPERTY_RESULT_TYPE_MISMATCH;
                transform[1] = (float)value.m_Number;
                return PROPERTY_RESULT_OK;
            }
            else if (property_id == PROP_POSITION_Z)
            {
                if (value.m_Type != PROPERTY_TYPE_NUMBER)
                    return PROPERTY_RESULT_TYPE_MISMATCH;
                transform[2] = (float)value.m_Number;
                return PROPERTY_RESULT_OK;
            }
            else if (property_id == PROP_SCALE)
            {
                if (value.m_Type != PROPERTY_TYPE_NUMBER)
                    return PROPERTY_RESULT_TYPE_MISMATCH;
                transform[3] = (float)value.m_Number;
                return PROPERTY_RESULT_OK;
            }
            else if (property_id == PROP_ROTATION)
            {
                if (value.m_Type != PROPERTY_TYPE_QUAT)
                    return PROPERTY_RESULT_TYPE_MISMATCH;
                transform[4] = value.m_V4[0];
                transform[5] = value.m_V4[1];
                transform[6] = value.m_V4[2];
                transform[7] = value.m_V4[3];
                return PROPERTY_RESULT_OK;
            }
            else if (property_id == PROP_ROTATION_X)
            {
                if (value.m_Type != PROPERTY_TYPE_NUMBER)
                    return PROPERTY_RESULT_TYPE_MISMATCH;
                transform[4] = (float)value.m_Number;
                return PROPERTY_RESULT_OK;
            }
            else if (property_id == PROP_ROTATION_Y)
            {
                if (value.m_Type != PROPERTY_TYPE_NUMBER)
                    return PROPERTY_RESULT_TYPE_MISMATCH;
                transform[5] = (float)value.m_Number;
                return PROPERTY_RESULT_OK;
            }
            else if (property_id == PROP_ROTATION_Z)
            {
                if (value.m_Type != PROPERTY_TYPE_NUMBER)
                    return PROPERTY_RESULT_TYPE_MISMATCH;
                transform[6] = (float)value.m_Number;
                return PROPERTY_RESULT_OK;
            }
            else if (property_id == PROP_ROTATION_W)
            {
                if (value.m_Type != PROPERTY_TYPE_NUMBER)
                    return PROPERTY_RESULT_TYPE_MISMATCH;
                transform[7] = (float)value.m_Number;
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
            uint8_t component_index;
            if (RESULT_OK == GetComponentIndex(instance, component_id, &component_index))
            {
                dmArray<Prototype::Component>& components = instance->m_Prototype->m_Components;
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
                    p.m_Instance = instance;
                    p.m_PropertyId = property_id;
                    p.m_UserData = user_data;
                    p.m_Value = value;
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

    void ResourceReloadedCallback(void* user_data, dmResource::SResourceDescriptor* descriptor, const char* name)
    {
        Collection* collection = (Collection*)user_data;
        for (uint32_t level = 0; level < MAX_HIERARCHICAL_DEPTH; ++level)
        {
            uint32_t max_instance = collection->m_Instances.Size();
            for (uint32_t i = 0; i < collection->m_LevelInstanceCount[level]; ++i)
            {
                uint16_t index = collection->m_LevelIndices[level * max_instance + i];
                Instance* instance = collection->m_Instances[index];
                uint32_t next_component_instance_data = 0;
                for (uint32_t j = 0; j < instance->m_Prototype->m_Components.Size(); ++j)
                {
                    Prototype::Component& component = instance->m_Prototype->m_Components[j];
                    ComponentType* type = component.m_Type;
                    if (component.m_ResourceId == descriptor->m_NameHash)
                    {
                        if (type->m_OnReloadFunction)
                        {
                            uintptr_t* user_data = 0;
                            if (type->m_InstanceHasUserData)
                            {
                                user_data = &instance->m_ComponentInstanceUserData[next_component_instance_data];
                            }
                            ComponentOnReloadParams params;
                            params.m_Instance = instance;
                            params.m_Resource = descriptor->m_Resource;
                            params.m_World = collection->m_ComponentWorlds[component.m_TypeIndex];
                            params.m_Context = type->m_Context;
                            params.m_UserData = user_data;
                            type->m_OnReloadFunction(params);
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

    lua_State* GetLuaState()
    {
        return g_LuaState;
    }
}
