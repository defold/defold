#include <new>
#include <stdio.h>
#include <dlib/log.h>
#include <dlib/hashtable.h>
#include <dlib/event.h>
#include <dlib/hash.h>
#include <dlib/array.h>
#include <dlib/index_pool.h>
#include <dlib/profile.h>
#include <ddf/ddf.h>
#include "gameobject.h"
#include "../proto/gameobject_ddf.h"
#include "gameobject_script.h"
#include "gameobject_common.h"

namespace dmGameObject
{
    struct ComponentType
    {
        ComponentType()
        {
            memset(this, 0, sizeof(*this));
        }

        uint32_t         m_ResourceType;
        const char*      m_Name;
        void*            m_Context;
        ComponentCreate  m_CreateFunction;
        ComponentDestroy m_DestroyFunction;
        ComponentsUpdate m_ComponentsUpdate;
        ComponentOnEvent m_ComponentOnEvent;
        uint32_t         m_ComponentInstanceHasUserdata : 1;
    };

    // Internal representation of a transform
    struct Transform
    {
        Point3 m_Translation;
        Quat   m_Rotation;
    };

    const uint32_t MAX_COMPONENT_TYPES = 128;
    // Max hierarchical depth
    // 4 is interpreted as up to four levels of child nodes including root-nodes
    // Must be greater than zero
    const uint32_t MAX_HIERARCHICAL_DEPTH = 4;
    struct Collection
    {
        Collection(dmResource::HFactory factory, uint32_t max_instances)
        {
            m_Factory = factory;
            m_ComponentTypeCount = 0;
            m_MaxInstances = max_instances;
            m_Instances.SetCapacity(max_instances);
            m_Instances.SetSize(max_instances);
            m_InstanceIndices.SetCapacity(max_instances);
            m_LevelIndices.SetCapacity(max_instances * MAX_HIERARCHICAL_DEPTH);
            m_LevelIndices.SetSize(max_instances * MAX_HIERARCHICAL_DEPTH);
            m_WorldTransforms.SetCapacity(max_instances);
            m_WorldTransforms.SetSize(max_instances);
            m_InstancesToDelete.SetCapacity(max_instances);
            m_IDToInstance.SetCapacity(max_instances/3, max_instances);
            m_InUpdate = 0;

            for (uint32_t i = 0; i < m_LevelIndices.Size(); ++i)
            {
                m_LevelIndices[i] = INVALID_INSTANCE_INDEX;
            }

            memset(&m_Instances[0], 0, sizeof(Instance*) * max_instances);
            memset(&m_WorldTransforms[0], 0xcc, sizeof(Transform) * max_instances);
            memset(&m_LevelInstanceCount[0], 0, sizeof(m_LevelInstanceCount));
        }
        dmResource::HFactory     m_Factory;

        uint32_t                 m_ComponentTypeCount;
        ComponentType            m_ComponentTypes[MAX_COMPONENT_TYPES];

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

        // Set to 1 if in update-loop
        uint32_t                 m_InUpdate : 1;
    };

    const uint32_t UNNAMED_IDENTIFIER = dmHashBuffer32("__unnamed__", strlen("__unnamed__"));

    dmHashTable64<const dmDDF::Descriptor*>* g_Descriptors = 0;
    uint32_t g_Socket = 0;
    uint32_t g_ReplySocket = 0;
    uint32_t g_EventID = 0;

    void Initialize()
    {
        assert(g_Descriptors == 0);

        g_Descriptors = new dmHashTable64<const dmDDF::Descriptor*>();
        g_Descriptors->SetCapacity(17, 128);

        g_EventID = dmHashString32(DMGAMEOBJECT_EVENT_NAME);
        dmEvent::Register(g_EventID, SCRIPT_EVENT_MAX);

        g_Socket = dmHashString32(DMGAMEOBJECT_EVENT_SOCKET_NAME);
        dmEvent::CreateSocket(g_Socket, SCRIPT_EVENT_SOCKET_BUFFER_SIZE);

        g_ReplySocket = dmHashString32(DMGAMEOBJECT_REPLY_EVENT_SOCKET_NAME);
        dmEvent::CreateSocket(g_ReplySocket, SCRIPT_EVENT_SOCKET_BUFFER_SIZE);

        InitializeScript();
    }

    void Finalize()
    {
        FinalizeScript();
        dmEvent::Unregister(g_EventID);
        dmEvent::DestroySocket(g_Socket);
        dmEvent::DestroySocket(g_ReplySocket);
        delete g_Descriptors;

        g_EventID = 0xffffffff;
        g_Socket = 0xffffffff;
        g_ReplySocket = 0xffffffff;
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
            g_Descriptors->Put(dmHashBuffer64(descriptor->m_Name, strlen(descriptor->m_Name)), descriptor);
            return RESULT_OK;
        }
    }

    HCollection  NewCollection(dmResource::HFactory factory, uint32_t max_instances)
    {
        if (max_instances > INVALID_INSTANCE_INDEX)
        {
            dmLogError("max_instances must be less or equal to %d", INVALID_INSTANCE_INDEX);
            return 0;
        }
        return new Collection(factory, max_instances);
    }

    void DeleteCollection(HCollection collection)
    {
        DeleteAll(collection);
        delete collection;
    }

    static ComponentType* FindComponentType(Collection* collection, uint32_t resource_type)
    {
        for (uint32_t i = 0; i < collection->m_ComponentTypeCount; ++i)
        {
            ComponentType* ct = &collection->m_ComponentTypes[i];
            if (ct->m_ResourceType == resource_type)
            {
                return ct;
            }
        }
        return 0;
    }

    Result RegisterComponentType(HCollection collection,
                                 const char* name,
                                 uint32_t resource_type,
                                 void* context,
                                 ComponentCreate create_function,
                                 ComponentDestroy destroy_function,
                                 ComponentsUpdate components_update,
                                 ComponentOnEvent component_on_event,
                                 bool component_instance_has_user_data)
    {
        if (collection->m_ComponentTypeCount == MAX_COMPONENT_TYPES)
            return RESULT_OUT_OF_RESOURCES;

        if (FindComponentType(collection, resource_type) != 0)
            return RESULT_ALREADY_REGISTERED;

        ComponentType component_type;
        component_type.m_ResourceType = resource_type;
        component_type.m_Name = name;
        component_type.m_Context = context;
        component_type.m_CreateFunction = create_function;
        component_type.m_DestroyFunction = destroy_function;
        component_type.m_ComponentsUpdate = components_update;
        component_type.m_ComponentOnEvent = component_on_event;
        component_type.m_ComponentInstanceHasUserdata = (uint32_t) component_instance_has_user_data;

        collection->m_ComponentTypes[collection->m_ComponentTypeCount++] = component_type;

        return RESULT_OK;
    }

    dmResource::CreateResult PrototypeCreate(dmResource::HFactory factory,
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
            const char* component_name = proto_desc->m_Components[i].m_Name;
            void* component;
            dmResource::FactoryResult fact_e = dmResource::Get(factory, component_name, (void**) &component);

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
                proto->m_Components.Push(Prototype::Component(component, resource_type, component_name));
            }
        }

        HScript script;
        dmResource::FactoryResult fact_e = dmResource::Get(factory, proto_desc->m_Script, (void**) &script);
        if (fact_e != dmResource::FACTORY_RESULT_OK)
        {
            dmDDF::FreeMessage(proto_desc);
            return dmResource::CREATE_RESULT_UNKNOWN;
        }

        proto->m_Name = strdup(proto_desc->m_Name);
        proto->m_Script = script;

        resource->m_Resource = (void*) proto;

        dmDDF::FreeMessage(proto_desc);
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult PrototypeDestroy(dmResource::HFactory factory,
                                              void* context,
                                              dmResource::SResourceDescriptor* resource)
    {
        Prototype* proto = (Prototype*) resource->m_Resource;
        for (uint32_t i = 0; i < proto->m_Components.Size(); ++i)
        {
            dmResource::Release(factory, proto->m_Components[i].m_Resource);
        }

        free((void*) proto->m_Name);
        dmResource::Release(factory, proto->m_Script);
        delete proto;
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ScriptCreate(dmResource::HFactory factory,
                                          void* context,
                                          const void* buffer, uint32_t buffer_size,
                                          dmResource::SResourceDescriptor* resource,
                                          const char* filename)
    {
        HScript script = NewScript(buffer);
        if (script)
        {
            resource->m_Resource = (void*) script;
            return dmResource::CREATE_RESULT_OK;
        }
        else
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
    }

    dmResource::CreateResult ScriptDestroy(dmResource::HFactory factory,
                                           void* context,
                                           dmResource::SResourceDescriptor* resource)
    {

        DeleteScript((HScript) resource->m_Resource);
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ScriptRecreate(dmResource::HFactory factory,
                                            void* context,
                                            const void* buffer, uint32_t buffer_size,
                                            dmResource::SResourceDescriptor* resource,
                                            const char* filename)
    {
        HScript script = (HScript) resource->m_Resource;
        if (ReloadScript(script, buffer))
        {
            return dmResource::CREATE_RESULT_OK;
        }
        else
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
    }

    dmResource::FactoryResult RegisterResourceTypes(dmResource::HFactory factory)
    {
        dmResource::FactoryResult ret;
        ret = dmResource::RegisterType(factory, "go", 0, &PrototypeCreate, &PrototypeDestroy, 0);
        if (ret != dmResource::FACTORY_RESULT_OK)
            return ret;

        ret = dmResource::RegisterType(factory, "scriptc", 0, &ScriptCreate, &ScriptDestroy, &ScriptRecreate);
        if (ret != dmResource::FACTORY_RESULT_OK)
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
        assert(collection->m_LevelInstanceCount[instance->m_Depth] < collection->m_MaxInstances);
    }

    HInstance New(HCollection collection, const char* prototype_name, const UpdateContext* update_context)
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
            ComponentType* component_type = FindComponentType(collection, component->m_ResourceType);
            if (!component_type)
            {
                dmLogError("Internal error. Component type #%d for '%s' not found.", i, prototype_name);
                assert(false);
            }
            if (component_type->m_ComponentInstanceHasUserdata)
                component_instance_userdata_count++;
        }

        uint32_t component_userdata_size = sizeof(((Instance*)0)->m_ComponentInstanceUserData[0]);
        // NOTE: Allocate actual Instance with *all* component instance user-data accounted
        void* instance_memory = ::operator new (sizeof(Instance) + component_instance_userdata_count * component_userdata_size);
        Instance* instance = new(instance_memory) Instance(proto);
        instance->m_ComponentInstanceUserDataCount = component_instance_userdata_count;

        uint32_t components_created = 0;
        uint32_t next_component_instance_data = 0;
        bool ok = true;
        for (uint32_t i = 0; i < proto->m_Components.Size(); ++i)
        {
            Prototype::Component* component = &proto->m_Components[i];
            ComponentType* component_type = FindComponentType(collection, component->m_ResourceType);
            assert(component_type);

            uintptr_t* component_instance_data = 0;
            if (component_type->m_ComponentInstanceHasUserdata)
            {
                component_instance_data = &instance->m_ComponentInstanceUserData[next_component_instance_data++];
                *component_instance_data = 0;
            }
            assert(next_component_instance_data <= instance->m_ComponentInstanceUserDataCount);

            CreateResult create_result =  component_type->m_CreateFunction(collection, instance, component->m_Resource, component_type->m_Context, component_instance_data);
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
                ComponentType* component_type = FindComponentType(collection, component->m_ResourceType);
                assert(component_type);

                uintptr_t* component_instance_data = 0;
                if (component_type->m_ComponentInstanceHasUserdata)
                {
                    component_instance_data = &instance->m_ComponentInstanceUserData[next_component_instance_data++];
                }
                assert(next_component_instance_data <= instance->m_ComponentInstanceUserDataCount);

                component_type->m_DestroyFunction(collection, instance, component_type->m_Context, component_instance_data);
            }

            // We can not call Delete here. Delete call DestroyFunction for every component
            dmResource::Release(factory, instance->m_Prototype);
            operator delete (instance_memory);
            return 0;
        }

        instance->m_Collection = collection;
        uint16_t instance_index = collection->m_InstanceIndices.Pop();
        instance->m_Index = instance_index;
        assert(collection->m_Instances[instance_index] == 0);
        collection->m_Instances[instance_index] = instance;

        InsertInstanceInLevelIndex(collection, instance);
        bool init_ok = RunScript(collection, proto->m_Script, "Init", instance->m_ScriptInstance, update_context);

        if (init_ok)
        {
            return instance;
        }
        else
        {
            Delete(collection, instance);
            return 0;
        }
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

    void Delete(HCollection collection, HInstance instance)
    {
        assert(collection->m_Instances[instance->m_Index] == instance);
        assert(instance->m_Collection == collection);

        if (collection->m_InUpdate)
        {
            // NOTE: Do not add for delete twice.
            if (instance->m_ToBeDeleted)
                return;

            instance->m_ToBeDeleted = 1;
            collection->m_InstancesToDelete.Push(instance->m_Index);
            return;
        }

        dmResource::HFactory factory = collection->m_Factory;
        uint32_t next_component_instance_data = 0;
        Prototype* prototype = instance->m_Prototype;
        for (uint32_t i = 0; i < prototype->m_Components.Size(); ++i)
        {
            Prototype::Component* component = &prototype->m_Components[i];
            ComponentType* component_type = FindComponentType(collection, component->m_ResourceType);
            assert(component_type);

            uintptr_t* component_instance_data = 0;
            if (component_type->m_ComponentInstanceHasUserdata)
            {
                component_instance_data = &instance->m_ComponentInstanceUserData[next_component_instance_data++];
            }
            assert(next_component_instance_data <= instance->m_ComponentInstanceUserDataCount);

            component_type->m_DestroyFunction(collection, instance, component_type->m_Context, component_instance_data);
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
        // This will perform tons of unnecessary work to resolve and reorder
        // the hierarchies and other things but will serve as a nice test case
        for (uint32_t i = 0; i < collection->m_Instances.Size(); ++i)
        {
            Instance* instance = collection->m_Instances[i];
            if (instance)
            {
                Delete(collection, instance);
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

    HInstance GetInstanceFromIdentifier(HCollection collection, uint32_t identifier)
    {
        Instance** instance = collection->m_IDToInstance.Get(identifier);
        if (instance)
            return *instance;
        else
            return 0;
    }

    Result PostNamedEvent(HInstance instance, const char* component_name, const char* event, const dmDDF::Descriptor* ddf_desc, char* ddf_data)
    {
        uint32_t event_hash = dmHashString32(event);
        uint32_t component_index = 0xffffffff;

        // Send to component or script?
        if (component_name != 0 && *component_name != '\0')
        {
            uint32_t component_name_hash = dmHashString32(component_name);
            Prototype* p = instance->m_Prototype;
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

        char buf[SCRIPT_EVENT_MAX];
        ScriptEventData* e = (ScriptEventData*)buf;
        e->m_Component = component_index & 0xff;
        e->m_EventHash = event_hash;
        e->m_Instance = instance;
        e->m_DDFDescriptor = ddf_desc;
        if (ddf_desc != 0x0)
        {
        	assert(ddf_data != 0x0);
        	uint32_t max_data_size = SCRIPT_EVENT_MAX - sizeof(ScriptEventData);
        	// TODO: This assert does not cover the case when e.g. strings are located after the ddf-message.
        	assert(ddf_desc->m_Size < max_data_size);
        	// TODO: We need to copy the whole mem-block since we don't know how much data is located after the ddf-message. How to solve? Size as parameter?
        	memcpy(buf + sizeof(ScriptEventData), ddf_data, max_data_size);
        }

        dmEvent::Post(g_ReplySocket, g_EventID, buf);

        return RESULT_OK;
    }

    bool Update(HCollection collection, HInstance instance, const UpdateContext* update_context)
    {
        Prototype* proto = instance->m_Prototype;
        bool ret = RunScript(collection, proto->m_Script, "Update", instance->m_ScriptInstance, update_context);
        if (!ret)
        {
        	dmLogError("The script for prototype %s failed to run.", proto->m_Name);
        }
        return ret;
    }

    struct DispatchEventsContext
    {
        HCollection m_Collection;
        const UpdateContext* m_UpdateContext;
        bool m_Success;
    };

    void DispatchEventsFunction(dmEvent::Event *event_object, void* user_ptr)
    {
        DispatchEventsContext* context = (DispatchEventsContext*) user_ptr;

        dmGameObject::ScriptEventData* script_event_data = (dmGameObject::ScriptEventData*) event_object->m_Data;
        assert(script_event_data->m_Instance);
        bool exists = false;
        for (uint32_t i = 0; i < context->m_Collection->m_Instances.Size(); ++i)
        {
        	if (script_event_data->m_Instance == context->m_Collection->m_Instances[i])
        	{
        		exists = true;
        		break;
        	}
        }

        if (exists)
        {
			if (script_event_data->m_Component == 0xff)
			{
				bool ret = DispatchScriptEventsFunction(event_object, context->m_UpdateContext);
				if (!ret)
					context->m_Success = false;
			}
			else
			{
				Instance* instance = script_event_data->m_Instance;
				Prototype* prototype = instance->m_Prototype;
				uint32_t resource_type = prototype->m_Components[script_event_data->m_Component].m_ResourceType;
				ComponentType* component_type = FindComponentType(context->m_Collection, resource_type);
				assert(component_type);

				if (component_type->m_ComponentOnEvent)
				{

					// TODO: Not optimal way to find index of component instance data
					uint32_t next_component_instance_data = 0;
					for (uint32_t i = 0; i < script_event_data->m_Component; ++i)
					{
						ComponentType* ct = FindComponentType(context->m_Collection, prototype->m_Components[i].m_ResourceType);
						assert(component_type);
						if (ct->m_ComponentInstanceHasUserdata)
						{
							next_component_instance_data++;
						}
					}

					uintptr_t* component_instance_data = 0;
					if (component_type->m_ComponentInstanceHasUserdata)
					{
						component_instance_data = &instance->m_ComponentInstanceUserData[next_component_instance_data];
					}
					component_type->m_ComponentOnEvent(context->m_Collection, instance, script_event_data, component_type->m_Context, component_instance_data);
				}
				else
				{
					// TODO User-friendly error message here...
					dmLogWarning("Component type is missing OnEvent function");
				}
			}
        }
    }

    bool DispatchEvents(HCollection collection, const UpdateContext* update_context)
    {
        DispatchEventsContext ctx;
        ctx.m_Collection = collection;
        ctx.m_UpdateContext = update_context;
        ctx.m_Success = true;
        (void) dmEvent::Dispatch(g_ReplySocket, &DispatchEventsFunction, (void*) &ctx);

        return ctx.m_Success;
    }

    void UpdateTransforms(HCollection collection, const UpdateContext* update_context)
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

                trans->m_Rotation =  instance->m_Rotation * parent_trans->m_Rotation;
                trans->m_Translation = (instance->m_Rotation * Quat(Vector3(parent_trans->m_Translation), 0.0f) * conj(instance->m_Rotation)).getXYZ()
                                      + instance->m_Position;
            }
        }
    }

    bool Update(HCollection collection, const UpdateContext* update_context)
    {
        DM_PROFILE(GameObject, "Update");
        collection->m_InUpdate = 1;
        collection->m_InstancesToDelete.SetSize(0);

        bool ret = DispatchEvents(collection, update_context);

        // Update scripts
        uint32_t n_objects = collection->m_Instances.Size();
        for (uint32_t i = 0; i < n_objects; ++i)
        {
            Instance* instance = collection->m_Instances[i];
            if (instance)
            {
                assert(collection->m_Instances[instance->m_Index] == instance);
                bool update_ret = Update(collection, instance, update_context);
                if (!update_ret)
                    ret = false;
            }
        }

        UpdateTransforms(collection, update_context);

        uint32_t component_types = collection->m_ComponentTypeCount;
        for (uint32_t i = 0; i < component_types; ++i)
        {
            ComponentType* component_type = &collection->m_ComponentTypes[i];
            DM_PROFILE(GameObject, component_type->m_Name);
            if (component_type->m_ComponentsUpdate)
            {
                component_type->m_ComponentsUpdate(collection, update_context, component_type->m_Context);
            }
        }

        collection->m_InUpdate = 0;

        if (collection->m_InstancesToDelete.Size() > 0)
        {
            uint32_t n_to_delete = collection->m_InstancesToDelete.Size();
            for (uint32_t i = 0; i < n_to_delete; ++i)
            {
                uint16_t index = collection->m_InstancesToDelete[i];
                Instance* instance = collection->m_Instances[index];

                assert(collection->m_Instances[instance->m_Index] == instance);
                assert(instance->m_ToBeDeleted);
                Delete(collection, instance);
            }
        }

        return ret;
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
        // TODO: Cache this calculation in UpdateTransforms?
        HCollection collection = instance->m_Collection;
        if (instance->m_Parent == INVALID_INSTANCE_INDEX)
        {
            return instance->m_Position;
        }
        else
        {
            Transform* t = &collection->m_WorldTransforms[instance->m_Parent];
            Point3 position = (t->m_Rotation * Quat(Vector3(instance->m_Position), 0.0f) * conj(t->m_Rotation)).getXYZ() + t->m_Translation;
            return position;
        }
    }

    Quat GetWorldRotation(HInstance instance)
    {
        HCollection collection = instance->m_Collection;
        Quat rotation = collection->m_WorldTransforms[instance->m_Index].m_Rotation;
        return rotation;
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
