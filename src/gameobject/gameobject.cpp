#include <new>
#include <stdio.h>
#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/hashtable.h>
#include <dlib/message.h>
#include <dlib/hash.h>
#include <dlib/array.h>
#include <dlib/circular_array.h>
#include <dlib/index_pool.h>
#include <dlib/profile.h>
#include <dlib/math.h>
#include <dlib/mutex.h>
#include <ddf/ddf.h>
#include "gameobject.h"
#include "gameobject_script.h"
#include "gameobject_common.h"

#include "../proto/gameobject_ddf.h"

namespace dmGameObject
{
    uint32_t g_RegisterIndex = 0;
    const uint32_t UNNAMED_IDENTIFIER = dmHashBuffer32("__unnamed__", strlen("__unnamed__"));

    dmHashTable64<const dmDDF::Descriptor*>* g_Descriptors = 0;

    Register::Register(dmMessage::DispatchCallback dispatch_callback, void* dispatch_userdata)
    {
        m_ComponentTypeCount = 0;
        m_Mutex = dmMutex::New();
        m_CurrentIdentifierPath[0] = '\0';
        // If m_CurrentCollection != 0 => loading sub-collection
        m_CurrentCollection = 0;
        // Accumulated position for child collections
        m_AccumulatedTranslation = Vector3(0,0,0);
        m_AccumulatedRotation = Quat::identity();

        char buffer[32];

        DM_SNPRINTF(buffer, 32, "%s%d", DM_GAMEOBJECT_MESSAGE_NAME, g_RegisterIndex);
        m_MessageId = dmHashString32(buffer);

        DM_SNPRINTF(buffer, 32, "%s%d", DM_GAMEOBJECT_SOCKET_NAME, g_RegisterIndex);
        m_SocketId = dmHashString32(buffer);
        dmMessage::CreateSocket(m_SocketId, INSTANCE_MESSAGE_SOCKET_BUFFER_SIZE);

        DM_SNPRINTF(buffer, 32, "%s%d", DM_GAMEOBJECT_SPAWN_MESSAGE_NAME, g_RegisterIndex);
        m_SpawnMessageId = dmHashString32(buffer);

        DM_SNPRINTF(buffer, 32, "%s%d", DM_GAMEOBJECT_SPAWN_SOCKET_NAME, g_RegisterIndex);
        m_SpawnSocketId = dmHashString32(buffer);
        dmMessage::CreateSocket(m_SpawnSocketId, INSTANCE_MESSAGE_SOCKET_BUFFER_SIZE);

        DM_SNPRINTF(buffer, 32, "%s%d", DM_GAMEOBJECT_REPLY_SOCKET_NAME, g_RegisterIndex);
        m_ReplySocketId = dmHashString32(buffer);
        dmMessage::CreateSocket(m_ReplySocketId, INSTANCE_MESSAGE_SOCKET_BUFFER_SIZE);

        m_DispatchCallback = dispatch_callback;
        m_DispatchUserdata = dispatch_userdata;

        ++g_RegisterIndex;
    }

    Register::~Register()
    {
        dmMutex::Delete(m_Mutex);
        dmMessage::Consume(m_SocketId);
        dmMessage::DestroySocket(m_SocketId);
        dmMessage::Consume(m_SpawnSocketId);
        dmMessage::DestroySocket(m_SpawnSocketId);
        dmMessage::Consume(m_ReplySocketId);
        dmMessage::DestroySocket(m_ReplySocketId);
    }

    ComponentType::ComponentType()
    {
        memset(this, 0, sizeof(*this));
    }

    void Initialize()
    {
        assert(g_Descriptors == 0);

        g_Descriptors = new dmHashTable64<const dmDDF::Descriptor*>();
        g_Descriptors->SetCapacity(17, 128);

        InitializeScript();
    }

    void Finalize()
    {
        FinalizeScript();
        delete g_Descriptors;

        g_Descriptors = 0;
    }

    Result RegisterDDFType(const dmDDF::Descriptor* descriptor)
    {
        if (g_Descriptors->Full())
        {
            dmLogWarning("Unable to register ddf type. Out of resources.");
            return RESULT_OUT_OF_RESOURCES;
        }
        else
        {
            g_Descriptors->Put(dmHashBuffer64(descriptor->m_ScriptName, strlen(descriptor->m_ScriptName)), descriptor);
            return RESULT_OK;
        }
    }

    HRegister NewRegister(dmMessage::DispatchCallback dispatch_callback, void* dispatch_userdata)
    {
        return new Register(dispatch_callback, dispatch_userdata);
    }

    void DeleteRegister(HRegister regist)
    {
        delete regist;
    }

    HCollection NewCollection(dmResource::HFactory factory, HRegister regist, uint32_t max_instances)
    {
        if (max_instances > INVALID_INSTANCE_INDEX)
        {
            dmLogError("max_instances must be less or equal to %d", INVALID_INSTANCE_INDEX);
            return 0;
        }
        Collection* coll = new Collection(factory, regist, max_instances);

        dmMutex::Lock(regist->m_Mutex);
        if (regist->m_Collections.Full())
        {
            regist->m_Collections.OffsetCapacity(4);
        }
        regist->m_Collections.Push(coll);
        dmMutex::Unlock(regist->m_Mutex);

        return coll;
    }

    void DoDeleteAll(HCollection collection);

    void DeleteCollection(HCollection collection)
    {
        HRegister regist = collection->m_Register;
        DoDeleteAll(collection);
        for (uint32_t i = 0; i < regist->m_ComponentTypeCount; ++i)
        {
            if (regist->m_ComponentTypes[i].m_DeleteWorldFunction)
                regist->m_ComponentTypes[i].m_DeleteWorldFunction(regist->m_ComponentTypes[i].m_Context, collection->m_ComponentWorlds[i]);
        }

        dmMutex::Lock(regist->m_Mutex);
        bool found = false;
        for (uint32_t i = 0; i < regist->m_Collections.Size(); ++i)
        {
            if (regist->m_Collections[i] == collection)
            {
                regist->m_Collections.EraseSwap(i);
                found = true;
                break;
            }
        }
        assert(found);
        dmMutex::Unlock(regist->m_Mutex);

        delete collection;
    }

    static ComponentType* FindComponentType(Register* regist, uint32_t resource_type, uint32_t* index)
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

    void* FindWorld(HCollection collection, uint32_t resource_type)
    {
        uint32_t index;
        ComponentType* component_type = FindComponentType(collection->m_Register, resource_type, &index);
        if (component_type != 0x0)
            return collection->m_ComponentWorlds[index];
        return 0x0;
    }

    Result RegisterComponentType(HRegister regist, const ComponentType& type)
    {
        if (regist->m_ComponentTypeCount == MAX_COMPONENT_TYPES)
            return RESULT_OUT_OF_RESOURCES;

        if (FindComponentType(regist, type.m_ResourceType, 0x0) != 0)
            return RESULT_ALREADY_REGISTERED;

        regist->m_ComponentTypes[regist->m_ComponentTypeCount++] = type;

        return RESULT_OK;
    }

    dmResource::CreateResult ResCreatePrototype(dmResource::HFactory factory,
                                                void* context,
                                                const void* buffer, uint32_t buffer_size,
                                                dmResource::SResourceDescriptor* resource,
                                                const char* filename)
    {
        PrototypeDesc* proto_desc;

        dmDDF::Result e = dmDDF::LoadMessage(buffer, buffer_size, &dmGameObject_PrototypeDesc_DESCRIPTOR, (void**)(&proto_desc));
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }

        Prototype* proto = new Prototype();
        proto->m_Components.SetCapacity(proto_desc->m_Components.m_Count);

        for (uint32_t i = 0; i < proto_desc->m_Components.m_Count; ++i)
        {
            const char* component_resource = proto_desc->m_Components[i].m_Resource;
            void* component;
            dmResource::FactoryResult fact_e = dmResource::Get(factory, component_resource, (void**) &component);

            if (fact_e != dmResource::FACTORY_RESULT_OK)
            {
                // Error, release created
                for (uint32_t j = 0; j < proto->m_Components.Size(); ++j)
                {
                    dmResource::Release(factory, proto->m_Components[j].m_Resource);
                }
                delete proto;
                dmDDF::FreeMessage(proto_desc);
                return dmResource::CREATE_RESULT_UNKNOWN;
            }
            else
            {
                uint32_t resource_type;
                fact_e = dmResource::GetType(factory, component, &resource_type);
                assert(fact_e == dmResource::FACTORY_RESULT_OK);
                proto->m_Components.Push(Prototype::Component(component, resource_type, component_resource));
            }
        }

        resource->m_Resource = (void*) proto;

        dmDDF::FreeMessage(proto_desc);
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResDestroyPrototype(dmResource::HFactory factory,
                                                 void* context,
                                                 dmResource::SResourceDescriptor* resource)
    {
        Prototype* proto = (Prototype*) resource->m_Resource;
        for (uint32_t i = 0; i < proto->m_Components.Size(); ++i)
        {
            dmResource::Release(factory, proto->m_Components[i].m_Resource);
        }

        delete proto;
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResCreateCollection(dmResource::HFactory factory,
                                                void* context,
                                                const void* buffer, uint32_t buffer_size,
                                                dmResource::SResourceDescriptor* resource,
                                                const char* filename)
    {
        Register* regist = (Register*) context;
        char prev_identifier_path[DM_GAMEOBJECT_CURRENT_IDENTIFIER_PATH_MAX];
        char tmp_ident[DM_GAMEOBJECT_CURRENT_IDENTIFIER_PATH_MAX];

        CollectionDesc* collection_desc;
        dmDDF::Result e = dmDDF::LoadMessage<dmGameObject::CollectionDesc>(buffer, buffer_size, &collection_desc);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
        dmResource::CreateResult res = dmResource::CREATE_RESULT_OK;

        // NOTE: Be careful about control flow. See below with dmMutex::Unlock, return, etc
        dmMutex::Lock(regist->m_Mutex);

        HCollection collection;
        bool loading_root;
        if (regist->m_CurrentCollection == 0)
        {
            loading_root = true;
            // TODO: How to configure 1024. In collection?
            collection = NewCollection(factory, regist, 1024);
            collection->m_NameHash = dmHashString32(collection_desc->m_Name);
            regist->m_CurrentCollection = collection;
            regist->m_AccumulatedTranslation = Vector3(0, 0, 0);
            regist->m_AccumulatedRotation = Quat::identity();

            // NOTE: Root-collection name is not prepended to identifier
            prev_identifier_path[0] = '\0';
            regist->m_CurrentIdentifierPath[0] = '\0';
        }
        else
        {
            loading_root = false;
            collection = regist->m_CurrentCollection;
            dmStrlCpy(prev_identifier_path, regist->m_CurrentIdentifierPath, DM_GAMEOBJECT_CURRENT_IDENTIFIER_PATH_MAX);
        }

        for (uint32_t i = 0; i < collection_desc->m_Instances.m_Count; ++i)
        {
            const dmGameObject::InstanceDesc& instance_desc = collection_desc->m_Instances[i];
            dmGameObject::HInstance instance = dmGameObject::New(collection, instance_desc.m_Prototype);
            if (instance != 0x0)
            {
                Quat rot = regist->m_AccumulatedRotation * instance_desc.m_Rotation;
                Vector3 pos = rotate(regist->m_AccumulatedRotation, Vector3(instance_desc.m_Position)) + regist->m_AccumulatedTranslation;

                dmGameObject::SetPosition(instance, Point3(pos));
                dmGameObject::SetRotation(instance, rot);

                dmHashInit32(&instance->m_CollectionPathHashState);
                dmHashUpdateBuffer32(&instance->m_CollectionPathHashState, regist->m_CurrentIdentifierPath, strlen(regist->m_CurrentIdentifierPath));

                dmStrlCpy(tmp_ident, regist->m_CurrentIdentifierPath, sizeof(tmp_ident));
                dmStrlCat(tmp_ident, instance_desc.m_Id, sizeof(tmp_ident));

                if (dmGameObject::SetIdentifier(collection, instance, tmp_ident) != dmGameObject::RESULT_OK)
                {
                    dmLogError("Unable to set identifier %s for %s. Name clash?", tmp_ident, instance_desc.m_Id);
                }

                for (uint32_t j = 0; j < instance_desc.m_ScriptProperties.m_Count; ++j)
                {
                    const dmGameObject::Property& p = instance_desc.m_ScriptProperties[j];
                    switch (p.m_Type)
                    {
                        case dmGameObject::Property::STRING:
                            dmGameObject::SetScriptStringProperty(instance, p.m_Key, p.m_Value);
                        break;

                        case dmGameObject::Property::INTEGER:
                            dmGameObject::SetScriptIntProperty(instance, p.m_Key, atoi(p.m_Value));
                        break;

                        case dmGameObject::Property::FLOAT:
                            dmGameObject::SetScriptFloatProperty(instance, p.m_Key, atof(p.m_Value));
                        break;
                    }
                }
            }
            else
            {
                dmLogError("Could not instantiate game object from prototype %s.", instance_desc.m_Prototype);
                res = dmResource::CREATE_RESULT_UNKNOWN;
                goto bail;
            }
        }

        // Setup hierarchy
        for (uint32_t i = 0; i < collection_desc->m_Instances.m_Count; ++i)
        {
            const dmGameObject::InstanceDesc& instance_desc = collection_desc->m_Instances[i];

            dmStrlCpy(tmp_ident, regist->m_CurrentIdentifierPath, sizeof(tmp_ident));
            dmStrlCat(tmp_ident, instance_desc.m_Id, sizeof(tmp_ident));

            dmGameObject::HInstance parent = dmGameObject::GetInstanceFromIdentifier(collection, dmHashString32(tmp_ident));
            assert(parent);

            for (uint32_t j = 0; j < instance_desc.m_Children.m_Count; ++j)
            {
                dmGameObject::HInstance child = dmGameObject::GetInstanceFromIdentifier(collection, dmHashString32(instance_desc.m_Children[j]));
                if (child)
                {
                    dmGameObject::Result r = dmGameObject::SetParent(child, parent);
                    if (r != dmGameObject::RESULT_OK)
                    {
                        dmLogError("Unable to set %s as parent to %s (%d)", instance_desc.m_Id, instance_desc.m_Children[j], r);
                    }
                    else
                    {
                        dmGameObject::SetScriptStringProperty(child, "Parent", tmp_ident);
                    }
                }
                else
                {
                    dmLogError("Child not found: %s", instance_desc.m_Children[j]);
                }
            }
        }

        // Load sub collections
        for (uint32_t i = 0; i < collection_desc->m_CollectionInstances.m_Count; ++i)
        {
            dmGameObject::CollectionInstanceDesc& coll_instance_desc = collection_desc->m_CollectionInstances[i];

            dmStrlCpy(prev_identifier_path, regist->m_CurrentIdentifierPath, DM_GAMEOBJECT_CURRENT_IDENTIFIER_PATH_MAX);
            dmStrlCat(regist->m_CurrentIdentifierPath, coll_instance_desc.m_Id, DM_GAMEOBJECT_CURRENT_IDENTIFIER_PATH_MAX);
            dmStrlCat(regist->m_CurrentIdentifierPath, ".", DM_GAMEOBJECT_CURRENT_IDENTIFIER_PATH_MAX);

            Collection* child_coll;
            Vector3 prev_translation = regist->m_AccumulatedTranslation;
            Quat prev_rotation = regist->m_AccumulatedRotation;

            Quat rot = regist->m_AccumulatedRotation * coll_instance_desc.m_Rotation;
            Vector3 trans = (regist->m_AccumulatedRotation * Quat(Vector3(coll_instance_desc.m_Position), 0.0f) * conj(regist->m_AccumulatedRotation)).getXYZ()
                       + regist->m_AccumulatedTranslation;

            regist->m_AccumulatedRotation = rot;
            regist->m_AccumulatedTranslation = trans;

            dmResource::FactoryResult r = dmResource::Get(factory, coll_instance_desc.m_Collection, (void**) &child_coll);
            if (r != dmResource::FACTORY_RESULT_OK)
            {
                res = dmResource::CREATE_RESULT_UNKNOWN;
                goto bail;
            }
            else
            {
                assert(child_coll != collection);
                dmResource::Release(factory, (void*) child_coll);
            }

            dmStrlCpy(regist->m_CurrentIdentifierPath, prev_identifier_path, DM_GAMEOBJECT_CURRENT_IDENTIFIER_PATH_MAX);

            regist->m_AccumulatedTranslation = prev_translation;
            regist->m_AccumulatedRotation = prev_rotation;
        }

        if (loading_root)
        {
            resource->m_Resource = (void*) collection;
        }
        else
        {
            // We must create a child collection. We can't return "collection" and release.
            // The root collection is not yet created.
            resource->m_Resource = (void*) NewCollection(factory, regist, 1);
        }
bail:
        dmDDF::FreeMessage(collection_desc);
        dmStrlCpy(regist->m_CurrentIdentifierPath, prev_identifier_path, DM_GAMEOBJECT_CURRENT_IDENTIFIER_PATH_MAX);

        if (loading_root && res != dmResource::CREATE_RESULT_OK)
        {
            // Loading of root-collection is responsible for deleting
            DeleteCollection(collection);
        }

        if (loading_root)
        {
            // We must reset this to next load.
            regist->m_CurrentCollection = 0;
        }

        dmMutex::Unlock(regist->m_Mutex);
        return res;
    }

    dmResource::CreateResult ResDestroyCollection(dmResource::HFactory factory,
                                                 void* context,
                                                 dmResource::SResourceDescriptor* resource)
    {
        HCollection collection = (HCollection) resource->m_Resource;
        DeleteCollection(collection);
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::FactoryResult RegisterResourceTypes(dmResource::HFactory factory, HRegister regist)
    {
        dmResource::FactoryResult ret = dmResource::FACTORY_RESULT_OK;
        ret = dmResource::RegisterType(factory, "goc", 0, &ResCreatePrototype, &ResDestroyPrototype, 0);
        if (ret != dmResource::FACTORY_RESULT_OK)
            return ret;

        ret = dmResource::RegisterType(factory, "scriptc", 0, &ResCreateScript, &ResDestroyScript, &ResRecreateScript);
        if (ret != dmResource::FACTORY_RESULT_OK)
            return ret;

        ret = dmResource::RegisterType(factory, "collectionc", regist, &ResCreateCollection, &ResDestroyCollection, 0);
        if (ret != dmResource::FACTORY_RESULT_OK)
            return ret;

        return ret;
    }

    Result RegisterComponentTypes(dmResource::HFactory factory, HRegister regist)
    {
        ComponentType script_component;
        dmResource::GetTypeFromExtension(factory, "scriptc", &script_component.m_ResourceType);
        script_component.m_Name = "scriptc";
        script_component.m_Context = 0x0;
        script_component.m_NewWorldFunction = &ScriptNewWorld;
        script_component.m_DeleteWorldFunction = &ScriptDeleteWorld;
        script_component.m_CreateFunction = &ScriptCreateComponent;
        script_component.m_InitFunction = &ScriptInitComponent;
        script_component.m_DestroyFunction = &ScriptDestroyComponent;
        script_component.m_UpdateFunction = &ScriptUpdateComponent;
        script_component.m_OnMessageFunction = &ScriptOnMessageComponent;
        script_component.m_OnInputFunction = &ScriptOnInputComponent;
        script_component.m_InstanceHasUserData = true;
        return RegisterComponentType(regist, script_component);
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
        assert(collection->m_LevelInstanceCount[instance->m_Depth] < collection->m_MaxInstances);
    }

    HInstance New(HCollection collection, const char* prototype_name)
    {
        assert(collection->m_InUpdate == 0 && "Creating new instances during Update(.) is not permitted");
        Prototype* proto;
        dmResource::HFactory factory = collection->m_Factory;
        dmResource::FactoryResult error = dmResource::Get(factory, prototype_name, (void**)&proto);
        if (error != dmResource::FACTORY_RESULT_OK)
        {
            return 0;
        }

        if (collection->m_InstanceIndices.Remaining() == 0)
        {
            dmLogWarning("Unable to create instance. Out of resources");
            return 0;
        }

        // Count number of component userdata fields required
        uint32_t component_instance_userdata_count = 0;
        for (uint32_t i = 0; i < proto->m_Components.Size(); ++i)
        {
            Prototype::Component* component = &proto->m_Components[i];
            ComponentType* component_type = FindComponentType(collection->m_Register, component->m_ResourceType, 0x0);
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
            uint32_t component_type_index;
            ComponentType* component_type = FindComponentType(collection->m_Register, component->m_ResourceType, &component_type_index);
            assert(component_type);

            uintptr_t* component_instance_data = 0;
            if (component_type->m_InstanceHasUserData)
            {
                component_instance_data = &instance->m_ComponentInstanceUserData[next_component_instance_data++];
                *component_instance_data = 0;
            }
            assert(next_component_instance_data <= instance->m_ComponentInstanceUserDataCount);

            CreateResult create_result =  component_type->m_CreateFunction(collection, instance, component->m_Resource, collection->m_ComponentWorlds[component_type_index], component_type->m_Context, component_instance_data);
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
                uint32_t component_type_index;
                ComponentType* component_type = FindComponentType(collection->m_Register, component->m_ResourceType, &component_type_index);
                assert(component_type);

                uintptr_t* component_instance_data = 0;
                if (component_type->m_InstanceHasUserData)
                {
                    component_instance_data = &instance->m_ComponentInstanceUserData[next_component_instance_data++];
                }
                assert(next_component_instance_data <= instance->m_ComponentInstanceUserDataCount);

                component_type->m_DestroyFunction(collection, instance, collection->m_ComponentWorlds[component_type_index], component_type->m_Context, component_instance_data);
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
            assert(collection->m_Instances[instance->m_Index] == instance);

            // Update world transforms since some components might need them in their init-callback
            Transform* trans = &collection->m_WorldTransforms[instance->m_Index];
            if (instance->m_Parent == INVALID_INSTANCE_INDEX)
            {
                trans->m_Translation = instance->m_Position;
                trans->m_Rotation = instance->m_Rotation;
            }
            else
            {
                const Transform* parent_trans = &collection->m_WorldTransforms[instance->m_Parent];
                trans->m_Rotation = parent_trans->m_Rotation * instance->m_Rotation;
                trans->m_Translation = rotate(parent_trans->m_Rotation, Vector3(instance->m_Position))
                                      + parent_trans->m_Translation;
            }

            uint32_t next_component_instance_data = 0;
            Prototype* prototype = instance->m_Prototype;
            for (uint32_t i = 0; i < prototype->m_Components.Size(); ++i)
            {
                Prototype::Component* component = &prototype->m_Components[i];
                ComponentType* component_type = FindComponentType(collection->m_Register, component->m_ResourceType, 0x0);
                assert(component_type);

                uintptr_t* component_instance_data = 0;
                if (component_type->m_InstanceHasUserData)
                {
                    component_instance_data = &instance->m_ComponentInstanceUserData[next_component_instance_data++];
                }
                assert(next_component_instance_data <= instance->m_ComponentInstanceUserDataCount);

                if (component_type->m_InitFunction)
                {
                    CreateResult result = component_type->m_InitFunction(collection, instance, component_type->m_Context, component_instance_data);
                    if (result != CREATE_RESULT_OK)
                    {
                        return false;
                    }
                }
            }
        }

        return true;
    }

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
        dmResource::HFactory factory = collection->m_Factory;
        uint32_t next_component_instance_data = 0;
        Prototype* prototype = instance->m_Prototype;
        for (uint32_t i = 0; i < prototype->m_Components.Size(); ++i)
        {
            Prototype::Component* component = &prototype->m_Components[i];
            uint32_t component_type_index;
            ComponentType* component_type = FindComponentType(collection->m_Register, component->m_ResourceType, &component_type_index);
            assert(component_type);

            uintptr_t* component_instance_data = 0;
            if (component_type->m_InstanceHasUserData)
            {
                component_instance_data = &instance->m_ComponentInstanceUserData[next_component_instance_data++];
            }
            assert(next_component_instance_data <= instance->m_ComponentInstanceUserDataCount);

            component_type->m_DestroyFunction(collection, instance, collection->m_ComponentWorlds[component_type_index], component_type->m_Context, component_instance_data);
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
        uint32_t id = dmHashBuffer32(identifier, strlen(identifier));
        if (collection->m_IDToInstance.Get(id))
            return RESULT_IDENTIFIER_IN_USE;

        if (instance->m_Identifier != UNNAMED_IDENTIFIER)
            return RESULT_IDENTIFIER_ALREADY_SET;

        instance->m_Identifier = id;
        collection->m_IDToInstance.Put(id, instance);

        return RESULT_OK;
    }

    uint32_t GetIdentifier(HInstance instance)
    {
        return instance->m_Identifier;
    }

    uint32_t GetAbsoluteIdentifier(HInstance instance, const char* id)
    {
        // Make a copy of the state.
        HashState32 tmp_state = instance->m_CollectionPathHashState;
        dmHashUpdateBuffer32(&tmp_state, id, strlen(id));
        return dmHashFinal32(&tmp_state);
    }

    HInstance GetInstanceFromIdentifier(HCollection collection, uint32_t identifier)
    {
        Instance** instance = collection->m_IDToInstance.Get(identifier);
        if (instance)
            return *instance;
        else
            return 0;
    }

    Result PostMessage(HRegister reg, InstanceMessageData* instance_message_data)
    {
        instance_message_data->m_Instance = 0;
        dmMessage::Post(reg->m_SocketId, reg->m_MessageId, (void*)instance_message_data, INSTANCE_MESSAGE_MAX);

        return RESULT_OK;
    }

    Result PostNamedMessage(HRegister reg, uint32_t message_id)
    {
        char buf[INSTANCE_MESSAGE_MAX];
        InstanceMessageData* e = (InstanceMessageData*)buf;
        e->m_MessageId = message_id;
        e->m_DDFDescriptor = 0x0;

        return PostMessage(reg, e);
    }

    Result PostDDFMessage(HRegister reg, const dmDDF::Descriptor* ddf_desc, char* ddf_data)
    {
        assert(ddf_desc != 0x0);
        assert(ddf_data != 0x0);

        char buf[INSTANCE_MESSAGE_MAX];
        InstanceMessageData* e = (InstanceMessageData*)buf;
        e->m_MessageId = dmHashString32(ddf_desc->m_ScriptName);
        e->m_DDFDescriptor = ddf_desc;

        uint32_t max_data_size = INSTANCE_MESSAGE_MAX - sizeof(InstanceMessageData);
        // TODO: This assert does not cover the case when e.g. strings are located after the ddf-message.
        assert(ddf_desc->m_Size < max_data_size);
        // TODO: We need to copy the whole mem-block since we don't know how much data is located after the ddf-message. How to solve? Size as parameter?
        memcpy(buf + sizeof(InstanceMessageData), ddf_data, max_data_size);

        return PostMessage(reg, e);
    }

    Result PostMessageTo(const char* component_name, InstanceMessageData* instance_message_data)
    {
        uint32_t component_index = 0xffffffff;

        // Send to component or broadcast?
        if (component_name != 0 && *component_name != '\0')
        {
            uint32_t component_name_hash = dmHashString32(component_name);
            Prototype* p = instance_message_data->m_Instance->m_Prototype;
            for (uint32_t i = 0; i < p->m_Components.Size(); ++i)
            {
                if (p->m_Components[i].m_NameHash == component_name_hash)
                {
                    component_index = i;
                    break;
                }
            }
            if (component_index == 0xffffffff)
            {
                return RESULT_COMPONENT_NOT_FOUND;
            }
        }

        instance_message_data->m_Component = component_index & 0xff;

        dmMessage::Post(instance_message_data->m_Instance->m_Collection->m_Register->m_ReplySocketId, instance_message_data->m_Instance->m_Collection->m_Register->m_MessageId, (void*)instance_message_data, INSTANCE_MESSAGE_MAX);

        return RESULT_OK;
    }

    Result PostNamedMessageTo(HInstance instance, const char* component_name, uint32_t message_id)
    {
        char buf[INSTANCE_MESSAGE_MAX];
        InstanceMessageData* e = (InstanceMessageData*)buf;
        e->m_MessageId = message_id;
        e->m_Instance = instance;
        e->m_DDFDescriptor = 0x0;

        return PostMessageTo(component_name, e);
    }

    Result PostDDFMessageTo(HInstance instance, const char* component_name, const dmDDF::Descriptor* ddf_desc, char* ddf_data)
    {
        assert(ddf_desc != 0x0);
        assert(ddf_data != 0x0);

        char buf[INSTANCE_MESSAGE_MAX];
        InstanceMessageData* e = (InstanceMessageData*)buf;
        e->m_MessageId = dmHashString32(ddf_desc->m_ScriptName);
        e->m_Instance = instance;
        e->m_DDFDescriptor = ddf_desc;

        uint32_t max_data_size = INSTANCE_MESSAGE_MAX - sizeof(InstanceMessageData);
        // TODO: This assert does not cover the case when e.g. strings are located after the ddf-message.
        assert(ddf_desc->m_Size < max_data_size);
        // TODO: We need to copy the whole mem-block since we don't know how much data is located after the ddf-message. How to solve? Size as parameter?
        memcpy(buf + sizeof(InstanceMessageData), ddf_data, max_data_size);

        return PostMessageTo(component_name, e);
    }

    struct DispatchMessagesContext
    {
        HRegister m_Register;
        bool m_Success;
    };

    void DispatchMessagesFunction(dmMessage::Message *message_object, void* user_ptr)
    {
        DispatchMessagesContext* context = (DispatchMessagesContext*) user_ptr;

        dmGameObject::InstanceMessageData* instance_message_data = (dmGameObject::InstanceMessageData*) message_object->m_Data;
        assert(instance_message_data->m_Instance);

        Instance* instance = instance_message_data->m_Instance;
        if (instance->m_ToBeDeleted)
        {
            dmLogWarning("Message sent to game object (%X) that will be deleted, message ignored.", instance->m_Identifier);
            return;
        }
        Prototype* prototype = instance->m_Prototype;
        // Broadcast to all components
        if (instance_message_data->m_Component == 0xff)
        {
            uint32_t next_component_instance_data = 0;
            uint32_t components_size = prototype->m_Components.Size();
            for (uint32_t i = 0; i < components_size; ++i)
            {
                ComponentType* component_type = FindComponentType(context->m_Register, prototype->m_Components[i].m_ResourceType, 0x0);
                assert(component_type);
                if (component_type->m_OnMessageFunction)
                {
                    uintptr_t* component_instance_data = 0;
                    if (component_type->m_InstanceHasUserData)
                    {
                        component_instance_data = &instance->m_ComponentInstanceUserData[next_component_instance_data];
                    }
                    {
                        DM_PROFILE(GameObject, "OnMessageFunction");
                        UpdateResult res = component_type->m_OnMessageFunction(instance, instance_message_data, component_type->m_Context, component_instance_data);
                        if (res != UPDATE_RESULT_OK)
                            context->m_Success = false;
                    }
                }

                if (component_type->m_InstanceHasUserData)
                {
                    next_component_instance_data++;
                }
            }
        }
        else
        {
            uint32_t resource_type = prototype->m_Components[instance_message_data->m_Component].m_ResourceType;
            ComponentType* component_type = FindComponentType(context->m_Register, resource_type, 0x0);
            assert(component_type);

            if (component_type->m_OnMessageFunction)
            {
                // TODO: Not optimal way to find index of component instance data
                uint32_t next_component_instance_data = 0;
                for (uint32_t i = 0; i < instance_message_data->m_Component; ++i)
                {
                    ComponentType* ct = FindComponentType(context->m_Register, prototype->m_Components[i].m_ResourceType, 0x0);
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
                    UpdateResult res = component_type->m_OnMessageFunction(instance, instance_message_data, component_type->m_Context, component_instance_data);
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
    }

    bool DispatchMessages(HRegister reg)
    {
        DM_PROFILE(GameObject, "DispatchMessages");

        DispatchMessagesContext ctx;
        ctx.m_Register = reg;
        ctx.m_Success = true;
        (void) dmMessage::Dispatch(reg->m_ReplySocketId, &DispatchMessagesFunction, (void*) &ctx);

        if (reg->m_DispatchCallback)
            (void) dmMessage::Dispatch(reg->m_SocketId, reg->m_DispatchCallback, reg->m_DispatchUserdata);

        return ctx.m_Success;
    }

    void UpdateTransforms(HCollection collection)
    {
        DM_PROFILE(GameObject, "UpdateTransforms");

        // Calculate world transforms
        // First root-level instances
        for (uint32_t i = 0; i < collection->m_LevelInstanceCount[0]; ++i)
        {
            uint16_t index = collection->m_LevelIndices[i];
            Transform* trans = &collection->m_WorldTransforms[index];
            Instance* instance = collection->m_Instances[index];
            uint16_t parent_index = instance->m_Parent;
            assert(parent_index == INVALID_INSTANCE_INDEX);
            trans->m_Translation = instance->m_Position;
            trans->m_Rotation = instance->m_Rotation;
        }

        // World-transform for levels 1..MAX_HIERARCHICAL_DEPTH-1
        for (uint32_t level = 1; level < MAX_HIERARCHICAL_DEPTH; ++level)
        {
            uint32_t max_instance = collection->m_Instances.Size();
            for (uint32_t i = 0; i < collection->m_LevelInstanceCount[level]; ++i)
            {
                uint16_t index = collection->m_LevelIndices[level * max_instance + i];
                Instance* instance = collection->m_Instances[index];
                Transform* trans = &collection->m_WorldTransforms[index];

                uint16_t parent_index = instance->m_Parent;
                assert(parent_index != INVALID_INSTANCE_INDEX);

                Transform* parent_trans = &collection->m_WorldTransforms[parent_index];

                /*
                 * Quaternion + Translation transform:
                 *
                 *   x' = q * x * q^-1 + t
                 *
                 *
                 * The compound transform:
                 * The first transform is given by:
                 *
                 *    x' = q1 * x * q1^-1 + t1
                 *
                 * apply the second transform
                 *
                 *   x'' = q2 ( q1 * x * q1^-1 + t1 ) q2^-1 + t2
                 *   x'' = q2 * q1 * x * q1^-1 * q2^-1 + q2 * t1 * q2^-1 + t2
                 *
                 * by inspection the following holds:
                 *
                 * Compound rotation: q2 * q1
                 * Compound translation: q2 * t1 * q2^-1 + t2
                 *
                 */

                trans->m_Rotation = parent_trans->m_Rotation * instance->m_Rotation;
                trans->m_Translation = rotate(parent_trans->m_Rotation, Vector3(instance->m_Position))
                                      + parent_trans->m_Translation;
            }
        }
    }

    bool Update(HCollection* collections, const UpdateContext* update_contexts, uint32_t collection_count)
    {
        DM_PROFILE(GameObject, "Update");

        if (collection_count == 0)
            return true;

        assert(collections != 0x0);
        HRegister reg = 0;
        for (uint32_t i = 0; i < collection_count; ++i)
        {
            if (collections[i])
            {
                if (reg)
                    assert(reg == collections[i]->m_Register);
                else
                    reg = collections[i]->m_Register;
            }
        }
        assert(reg);

        for (uint32_t i = 0; i < collection_count; ++i)
            if (collections[i])
                collections[i]->m_InUpdate = 1;

        bool ret = true;

        uint32_t component_types = reg->m_ComponentTypeCount;
        for (uint32_t i = 0; i < component_types; ++i)
        {
            if (!DispatchMessages(reg))
                ret = false;

            ComponentType* component_type = &reg->m_ComponentTypes[i];
            DM_PROFILE(GameObject, component_type->m_Name);
            if (component_type->m_UpdateFunction)
            {
                for (uint32_t j = 0; j < collection_count; ++j)
                {
                    if (collections[j] == 0)
                        continue;
                    const UpdateContext* update_context = 0x0;
                    if (update_contexts != 0)
                        update_context = &update_contexts[j];
                    UpdateResult res = component_type->m_UpdateFunction(collections[j], update_context, collections[j]->m_ComponentWorlds[i], component_type->m_Context);
                    if (res != UPDATE_RESULT_OK)
                        ret = false;
                }
            }
            // TODO: Solve this better! Right now the worst is assumed, which is that every component updates some transforms as well as
            // demands updated child-transforms. Many redundant calculations. This could be solved by splitting the component Update-callback
            // into UpdateTrasform, then Update or similar
            for (uint32_t j = 0; j < collection_count; ++j)
                if (collections[j])
                    UpdateTransforms(collections[j]);
        }

        for (uint32_t i = 0; i < collection_count; ++i)
            if (collections[i])
                collections[i]->m_InUpdate = 0;

        return ret;
    }

    void DispatchCallback(dmMessage::Message *message, void* user_ptr)
    {
        HRegister reg = (HRegister)user_ptr;
        if (message->m_ID == reg->m_SpawnMessageId)
        {
            SpawnMessage* spawn_message = (SpawnMessage*)message->m_Data;
            dmGameObject::HInstance instance = dmGameObject::New(spawn_message->m_Collection, spawn_message->m_Prototype);
            if (instance != 0)
            {
                dmGameObject::SetPosition(instance, spawn_message->m_Position);
                dmGameObject::SetRotation(instance, spawn_message->m_Rotation);
                char id[32];
                static int spawn_count = 0;
                DM_SNPRINTF(id, 32, "spawn%d", ++spawn_count);
                dmGameObject::SetIdentifier(spawn_message->m_Collection, instance, id);
                dmGameObject::SetScriptStringProperty(instance, "Id", id);
                dmGameObject::Init(spawn_message->m_Collection, instance);
            }
        }
    }

    bool PostUpdate(HCollection* collections, uint32_t collection_count)
    {
        for (uint32_t i = 0; i < collection_count; ++i)
        {
            if (collections[i])
            {
                (void) dmMessage::Dispatch(collections[i]->m_Register->m_SpawnSocketId, DispatchCallback, collections[i]->m_Register);
                break;
            }
        }
        for (uint32_t i = 0; i < collection_count; ++i)
        {
            HCollection collection = collections[i];
            if (collection == 0x0)
                continue;
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
        }

        return true;
    }

    UpdateResult DispatchInput(HCollection* collections, uint32_t collection_count, InputAction* input_actions, uint32_t input_action_count)
    {
        DM_PROFILE(GameObject, "DispatchInput");
        // clean stacks
        for (uint32_t i = 0; i < collection_count; ++i)
        {
            HCollection collection = collections[i];
            if (collection == 0x0)
                continue;
            HInstance instance = 0x0;
            while (collection->m_FocusStack.Size() > 0 && instance == 0x0)
            {
                instance = collection->m_FocusStack.Top();
                if (instance == 0x0 || instance->m_ToBeDeleted)
                {
                    collection->m_FocusStack.Pop();
                    instance = 0x0;
                }
            }
        }
        // iterate stacks from top to bottom
        for (uint32_t i = 0; i < input_action_count; ++i)
        {
            InputAction& input_action = input_actions[i];
            for (uint32_t j = 0; j < collection_count && input_action.m_ActionId != 0; ++j)
            {
                HCollection collection = collections[j];
                if (collection == 0x0)
                    continue;
                uint32_t stack_size = collection->m_FocusStack.Size();
                for (uint32_t k = 0; k < stack_size && input_action.m_ActionId != 0; ++k)
                {
                    HInstance instance = collection->m_FocusStack[stack_size - 1 - k];
                    if (instance == 0x0 || instance->m_ToBeDeleted)
                        continue;
                    Prototype* prototype = instance->m_Prototype;
                    uint32_t components_size = prototype->m_Components.Size();

                    InputResult res = INPUT_RESULT_IGNORED;
                    uint32_t next_component_instance_data = 0;
                    for (uint32_t l = 0; l < components_size; ++l)
                    {
                        ComponentType* component_type = FindComponentType(collection->m_Register, prototype->m_Components[l].m_ResourceType, 0x0);
                        assert(component_type);
                        if (component_type->m_OnInputFunction)
                        {
                            uintptr_t* component_instance_data = 0;
                            if (component_type->m_InstanceHasUserData)
                            {
                                component_instance_data = &instance->m_ComponentInstanceUserData[next_component_instance_data];
                            }
                            InputResult comp_res = component_type->m_OnInputFunction(instance, &input_action, component_type->m_Context, component_instance_data);
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
                    }
                }
            }
        }
        return UPDATE_RESULT_OK;
    }

    void AcquireInputFocus(HCollection collection, HInstance instance)
    {
        collection->m_FocusStack.Push(instance);
    }

    void ReleaseInputFocus(HCollection collection, HInstance instance)
    {
        collection->m_FocusStack.Pop();
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

    uint32_t GetMessageSocketId(HRegister reg)
    {
        if (reg)
            return reg->m_SocketId;
        else
            return 0;
    }

    uint32_t GetReplyMessageSocketId(HRegister reg)
    {
        if (reg)
            return reg->m_ReplySocketId;
        else
            return 0;
    }

    uint32_t GetMessageId(HRegister reg)
    {
        if (reg)
            return reg->m_MessageId;
        else
            return 0;
    }

    void SetPosition(HInstance instance, Point3 position)
    {
        instance->m_Position = position;
    }

    Point3 GetPosition(HInstance instance)
    {
        return instance->m_Position;
    }

    void SetRotation(HInstance instance, Quat rotation)
    {
        instance->m_Rotation = rotation;
    }

    Quat GetRotation(HInstance instance)
    {
        return instance->m_Rotation;
    }

    Point3 GetWorldPosition(HInstance instance)
    {
        HCollection collection = instance->m_Collection;
        return collection->m_WorldTransforms[instance->m_Index].m_Translation;
    }

    Quat GetWorldRotation(HInstance instance)
    {
        HCollection collection = instance->m_Collection;
        return collection->m_WorldTransforms[instance->m_Index].m_Rotation;
    }

    Result SetParent(HInstance child, HInstance parent)
    {
        if (parent->m_Depth >= MAX_HIERARCHICAL_DEPTH-1)
        {
            dmLogError("Unable to set parent to child. Parent at maximum depth %d", MAX_HIERARCHICAL_DEPTH-1);
            return RESULT_MAXIMUM_HIEARCHICAL_DEPTH;
        }

        HCollection collection = child->m_Collection;

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

        if (child->m_Parent != INVALID_INSTANCE_INDEX)
        {
            Unlink(collection, child);
        }

        EraseSwapLevelIndex(collection, child);

        // Add child to parent
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

        int original_child_depth = child->m_Depth;
        child->m_Parent = parent->m_Index;
        child->m_Depth = parent->m_Depth + 1;
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
}
