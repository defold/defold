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

#include "gameobject_private.h"
#include <resource/resource.h>
#include <dlib/mutex.h>
#include <dlib/profile.h>

namespace dmGameObject
{

// ********************************************************************************************

static bool IterateCollectionGetNext(SceneNodeIterator* it)
{
    assert(it->m_Parent.m_Type == SCENE_NODE_TYPE_COLLECTION);
    HCollection hcollection = it->m_Parent.m_Collection;
    Collection* collection = GetCollectionFromHandle(hcollection);
    if (!collection)
        return false;

    const dmArray<uint32_t>& root_level = collection->m_LevelIndices[0];

    // If the index is still valid
    uint64_t index = it->m_NextChild.m_Node;
    bool valid = index < root_level.Size();

    if (valid) {
        it->m_Node = it->m_NextChild;
        it->m_Node.m_Collection = hcollection;
        it->m_Node.m_Instance = GetGameObjectHandle(collection->m_Instances[root_level[index]]);
        it->m_NextChild.m_Node++;
    } else {
        // We're done iterating this collection
        dmMutex::Unlock(collection->m_Mutex);
    }

    return valid;
}

static void IterateCollectionChildren(SceneNodeIterator* it, SceneNode* node)
{
    assert(node->m_Type == SCENE_NODE_TYPE_COLLECTION);

    it->m_Parent = *node;
    it->m_NextChild.m_Type = SCENE_NODE_TYPE_GAMEOBJECT;
    it->m_NextChild.m_Node = 0; // root level array indices

    it->m_FnIterateNext = IterateCollectionGetNext;

    Collection* collection = GetCollectionFromHandle(node->m_Collection);
    if (collection)
        dmMutex::Lock(collection->m_Mutex);
}

// ********************************************************************************************


static bool IterateGameObjectGetNext(SceneNodeIterator* it)
{
    assert(it->m_Parent.m_Type == SCENE_NODE_TYPE_GAMEOBJECT);

    Collection* collection = GetCollectionFromHandle(it->m_Parent.m_Collection);
    Instance* parent = GetGameObjectFromHandle(collection, it->m_Parent.m_Instance);
    if (!parent)
        return false;

    if (it->m_IteratorPhase == 0 && it->m_NextGameObject != INVALID_GAME_OBJECT) {
        Instance* instance = GetGameObjectFromHandle(collection, it->m_NextGameObject);
        if (!instance || instance->m_Parent != parent->m_Index)
            return false;

        it->m_Node = it->m_NextChild;
        it->m_Node.m_Collection = it->m_Parent.m_Collection;
        it->m_Node.m_Instance = it->m_NextGameObject;
        it->m_NextGameObject = instance->m_SiblingIndex == INVALID_INSTANCE_INDEX
                ? INVALID_GAME_OBJECT
                : GetGameObjectHandle(collection->m_Instances[instance->m_SiblingIndex]);

        if (it->m_NextGameObject == INVALID_GAME_OBJECT) {
            it->m_IteratorPhase = 1;
        }
        return true;
    }

    it->m_IteratorPhase = 1;
    if (it->m_NextComponent < parent->m_Prototype->m_ComponentCount) {
        uint32_t index = it->m_NextComponent;
        Instance* instance = parent;
        Prototype* prototype = parent->m_Prototype;

        // Find the actual component instance data
        // if none exist at this index, fast forward to the next item in the list
        uint32_t next_component_instance_data = 0;
        uintptr_t* component_instance_data = 0;

        for (uint32_t k = 0; k < prototype->m_ComponentCount; ++k)
        {
            ComponentType* component_type = prototype->m_Components[k].m_Type;

            if (component_type->m_InstanceHasUserData)
            {
                uintptr_t* current_component_instance_data = &instance->m_ComponentInstanceUserData[next_component_instance_data++];

                // We need to iterate from k=0 since the prototype potentially contains multiple instances of the same component type
                if (k >= index)
                {
                    index = k;
                    component_instance_data = current_component_instance_data;
                    break;
                }
            }
        }

        if (component_instance_data)
        {
            Prototype::Component* component = &prototype->m_Components[index];
            void* component_world = collection->m_ComponentWorlds[component->m_TypeIndex];

            // the the actual instance data
            it->m_Node.m_Node = (uint64_t)*component_instance_data;
            it->m_Node.m_Type = SCENE_NODE_TYPE_COMPONENT;
            // Needed to iterate over the properties
            it->m_Node.m_ComponentWorld = component_world;
            it->m_Node.m_Component = *component_instance_data;
            it->m_Node.m_ComponentType = component->m_Type;
            it->m_Node.m_ComponentPrototype = (void*)&prototype->m_Components[index];
            it->m_Node.m_Collection = it->m_Parent.m_Collection;
            it->m_Node.m_Instance = it->m_Parent.m_Instance;

            it->m_NextComponent = index + 1;
            return true;
        } else
        {
            it->m_NextComponent = prototype->m_ComponentCount;
        }
    }

    return false;
}

static void IterateGameObjectChildren(SceneNodeIterator* it, SceneNode* node)
{
    assert(node->m_Type == SCENE_NODE_TYPE_GAMEOBJECT);

    it->m_Parent = *node;
    //it->m_NextChild = *node;
    it->m_NextChild.m_Type = SCENE_NODE_TYPE_GAMEOBJECT;
    Collection* collection = GetCollectionFromHandle(node->m_Collection);
    Instance* instance = GetGameObjectFromHandle(collection, node->m_Instance);
    it->m_NextGameObject = instance && instance->m_FirstChildIndex != INVALID_INSTANCE_INDEX
            ? GetGameObjectHandle(collection->m_Instances[instance->m_FirstChildIndex])
            : INVALID_GAME_OBJECT;
    it->m_NextComponent = 0;
    it->m_IteratorPhase = 0;
    it->m_FnIterateNext = IterateGameObjectGetNext;
}

// ********************************************************************************************
// Implementations for component types that want to be part of the scene graph, that doesn't have an hierarchy
// E.g. for iterating over the properties

static bool IterateComponentNullGetNext(struct SceneNodeIterator* it)
{
    return false;
}
static void IterateComponentNullChildren(struct SceneNodeIterator* it, struct SceneNode* node)
{
    it->m_Parent = *node;
    it->m_NextChild.m_Type = SCENE_NODE_TYPE_SUBCOMPONENT; // doesn't really matter
    it->m_NextChild.m_Node = 0;
    it->m_FnIterateNext = IterateComponentNullGetNext;
}

static bool ResolveComponentNode(SceneNode* node, ComponentType** out_component_type, Prototype::Component** out_component_prototype)
{
    Collection* collection = GetCollectionFromHandle(node->m_Collection);
    Instance* instance = GetGameObjectFromHandle(collection, node->m_Instance);
    if (!instance)
        return false;

    Prototype* prototype = instance->m_Prototype;
    uint32_t component_instance_data_index = 0;
    for (uint32_t i = 0; i < prototype->m_ComponentCount; ++i)
    {
        Prototype::Component* component = &prototype->m_Components[i];
        ComponentType* component_type = component->m_Type;
        uintptr_t component_instance_data = 0;
        if (component_type->m_InstanceHasUserData)
        {
            component_instance_data = instance->m_ComponentInstanceUserData[component_instance_data_index++];
        }

        if (node->m_ComponentPrototype != component)
            continue;
        if (node->m_ComponentType != component_type || node->m_Component != component_instance_data)
            return false;
        if (node->m_ComponentWorld != collection->m_ComponentWorlds[component->m_TypeIndex])
            return false;

        if (out_component_type)
            *out_component_type = component_type;
        if (out_component_prototype)
            *out_component_prototype = component;
        return true;
    }
    return false;
}

// ********************************************************************************************

static void IterateComponentChildren(SceneNodeIterator* it, SceneNode* node)
{
    assert(node->m_Type == SCENE_NODE_TYPE_COMPONENT || node->m_Type == SCENE_NODE_TYPE_SUBCOMPONENT);
    ComponentType* component_type = 0;
    if (!ResolveComponentNode(node, &component_type, 0))
    {
        IterateComponentNullChildren(it, node);
        return;
    }

    if (component_type->m_IterChildren)
    {
        component_type->m_IterChildren(it, node);
    }
    else
    {
        IterateComponentNullChildren(it, node);
    }
}


// ********************************************************************************************

bool TraverseGetRoot(HContext regist, SceneNode* node)
{
    DM_MUTEX_SCOPED_LOCK(regist->m_Mutex);
    if (regist->m_Collections.Empty())
        return false;

    Collection* collection = regist->m_Collections[0];
    node->m_Node = (uint64_t)collection->m_HCollection;
    node->m_Type = SCENE_NODE_TYPE_COLLECTION;
    node->m_Collection = collection->m_HCollection;
    return true;
}

SceneNodeIterator TraverseIterateChildren(SceneNode* node)
{
    DM_PROFILE("TraverseIterateChildren");

    FIteratorChildren fn = 0;
    switch(node->m_Type)
    {
    case SCENE_NODE_TYPE_COLLECTION:    fn = IterateCollectionChildren; break;
    case SCENE_NODE_TYPE_GAMEOBJECT:    fn = IterateGameObjectChildren; break;
    case SCENE_NODE_TYPE_COMPONENT:
    case SCENE_NODE_TYPE_SUBCOMPONENT:  fn = IterateComponentChildren; break;
    default: break;
    }

    SceneNodeIterator it = {};
    if (fn) {
        fn(&it, node);
    }

    return  it;
}

bool TraverseIterateNext(SceneNodeIterator* it)
{
    DM_PROFILE("TraverseIterateNext");
    if (!it || !it->m_FnIterateNext)
        return false;
    if ((it->m_Parent.m_Type == SCENE_NODE_TYPE_COMPONENT || it->m_Parent.m_Type == SCENE_NODE_TYPE_SUBCOMPONENT)
        && !ResolveComponentNode(&it->m_Parent, 0, 0))
    {
        return false;
    }
    return it->m_FnIterateNext(it);
}

// ********************************************************************************************

static dmhash_t g_SceneNodePropertyName_id = 0;
static dmhash_t g_SceneNodePropertyName_type = 0;
static dmhash_t g_SceneNodePropertyName_resource = 0;
static dmhash_t g_SceneNodePropertyName_position = 0;
static dmhash_t g_SceneNodePropertyName_rotation = 0;
static dmhash_t g_SceneNodePropertyName_scale = 0;
static dmhash_t g_SceneNodePropertyName_world_position = 0;
static dmhash_t g_SceneNodePropertyName_world_rotation = 0;
static dmhash_t g_SceneNodePropertyName_world_scale = 0;

// In order to do reverse hashes on these, we need to hash them after the engine has started
static void InitSceneNodePropertyNames()
{
    g_SceneNodePropertyName_id             = dmHashString64("id");
    g_SceneNodePropertyName_type           = dmHashString64("type");
    g_SceneNodePropertyName_resource       = dmHashString64("resource");
    g_SceneNodePropertyName_position       = dmHashString64("position");
    g_SceneNodePropertyName_rotation       = dmHashString64("rotation");
    g_SceneNodePropertyName_scale          = dmHashString64("scale");
    g_SceneNodePropertyName_world_position = dmHashString64("world_position");
    g_SceneNodePropertyName_world_rotation = dmHashString64("world_rotation");
    g_SceneNodePropertyName_world_scale    = dmHashString64("world_scale");
}

// ********************************************************************************************

static bool IterateCollectionPropertiesGetNext(SceneNodePropertyIterator* pit)
{
    assert(pit->m_Node->m_Type == SCENE_NODE_TYPE_COLLECTION);
    assert(pit->m_Node->m_Collection != 0);

    const dmhash_t names[] = {
        g_SceneNodePropertyName_id,
        g_SceneNodePropertyName_type,
        g_SceneNodePropertyName_resource,
    };

    uint64_t index = pit->m_Next++;
    if (index >= sizeof(names)/sizeof(names[0]))
        return false;

    Collection* collection = GetCollectionFromHandle(pit->m_Node->m_Collection);
    if (!collection)
        return false;

    pit->m_Property.m_NameHash = names[index];

    if (pit->m_Property.m_NameHash == g_SceneNodePropertyName_id)
    {
        pit->m_Property.m_Type = SCENE_NODE_PROPERTY_TYPE_HASH;
        pit->m_Property.m_Value.m_Hash = collection->m_NameHash;
    }
    else if (pit->m_Property.m_NameHash == g_SceneNodePropertyName_type)
    {
        pit->m_Property.m_Type = SCENE_NODE_PROPERTY_TYPE_HASH;
        pit->m_Property.m_Value.m_Hash = dmHashString64("collectionc");
    }
    else if (pit->m_Property.m_NameHash == g_SceneNodePropertyName_resource)
    {
        pit->m_Property.m_Type = SCENE_NODE_PROPERTY_TYPE_HASH;
        pit->m_Property.m_Value.m_Hash = 0;
        HCollectionResource resource = collection->m_CollectionResource;
        if (resource)
            dmResource::GetPath(collection->m_Factory, resource, &pit->m_Property.m_Value.m_Hash);
    }

    return true;
}

static void IterateCollectionProperties(SceneNodePropertyIterator* pit, SceneNode* node)
{
    assert(node->m_Type == SCENE_NODE_TYPE_COLLECTION);
    assert(node->m_Collection != 0);
    pit->m_Node = node;
    pit->m_Next = 0;
    pit->m_FnIterateNext = IterateCollectionPropertiesGetNext;
}


// ********************************************************************************************

static bool IterateGameObjectPropertiesGetNext(SceneNodePropertyIterator* pit)
{
    assert(pit->m_Node->m_Type == SCENE_NODE_TYPE_GAMEOBJECT);
    assert(pit->m_Node->m_Instance != 0);

    const dmhash_t property_names[] = {
        g_SceneNodePropertyName_id,
        g_SceneNodePropertyName_type,
        g_SceneNodePropertyName_resource,
    };
    uint32_t num_properties = DM_ARRAY_SIZE(property_names);

    const dmhash_t transform_property_names[] = {
        g_SceneNodePropertyName_position,
        g_SceneNodePropertyName_rotation,
        g_SceneNodePropertyName_scale,
        g_SceneNodePropertyName_world_position,
        g_SceneNodePropertyName_world_rotation,
        g_SceneNodePropertyName_world_scale
    };
    uint32_t num_transform_properties = DM_ARRAY_SIZE(transform_property_names);

    uint64_t index = pit->m_Next++;


    HCollection hcollection = pit->m_Node->m_Collection;
    Collection* collection = GetCollectionFromHandle(hcollection);
    HGameObject hinstance = pit->m_Node->m_Instance;
    Instance* instance = GetGameObjectFromHandle(collection, hinstance);
    if (!instance)
        return false;
    if (index < num_properties)
    {
        pit->m_Property.m_NameHash = property_names[index];

        if (property_names[index] == g_SceneNodePropertyName_id)
        {
            pit->m_Property.m_Type = SCENE_NODE_PROPERTY_TYPE_HASH;
            pit->m_Property.m_Value.m_Hash = instance->m_Identifier;
        }
        else if (property_names[index] == g_SceneNodePropertyName_type)
        {
            pit->m_Property.m_Type = SCENE_NODE_PROPERTY_TYPE_HASH;
            pit->m_Property.m_Value.m_Hash = dmHashString64("goc");
        }
        else if (property_names[index] == g_SceneNodePropertyName_resource)
        {
            pit->m_Property.m_Type = SCENE_NODE_PROPERTY_TYPE_HASH;

            dmResource::GetPath(collection->m_Factory, instance->m_Prototype, &pit->m_Property.m_Value.m_Hash);
        }
        return true;
    }

    index -= num_properties;

    if (index < num_transform_properties)
    {

        Vector4 value;
        SceneNodePropertyType type = SCENE_NODE_PROPERTY_TYPE_VECTOR3;
        switch(index)
        {
            case 0: value = Vector4(GetPosition(instance)); break;
            case 1: value = Vector4(GetRotation(instance)); type = SCENE_NODE_PROPERTY_TYPE_QUAT; break;
            case 2: value = Vector4(GetScale(instance)); break;
            case 3: value = Vector4(GetWorldPosition(collection, instance)); break;
            case 4: value = Vector4(GetWorldRotation(collection, instance)); type = SCENE_NODE_PROPERTY_TYPE_QUAT; break;
            case 5: value = Vector4(GetWorldScale(collection, instance)); break;
        }

        pit->m_Property.m_NameHash = transform_property_names[index];
        pit->m_Property.m_Type = type;
        pit->m_Property.m_Value.m_V4[0] = value.getX();
        pit->m_Property.m_Value.m_V4[1] = value.getY();
        pit->m_Property.m_Value.m_V4[2] = value.getZ();
        pit->m_Property.m_Value.m_V4[3] = value.getW();
        return true;
    }

    index -= num_transform_properties;

    return false;
}

static void IterateGameObjectProperties(SceneNodePropertyIterator* pit, SceneNode* node)
{
    assert(node->m_Type == SCENE_NODE_TYPE_GAMEOBJECT);
    assert(node->m_Instance != 0);
    pit->m_Node = node;
    pit->m_Next = 0;
    pit->m_FnIterateNext = IterateGameObjectPropertiesGetNext;
}

// ********************************************************************************************

static bool IterateComponentPropertiesGetNext(SceneNodePropertyIterator* pit)
{
    assert(pit->m_Node->m_Type == SCENE_NODE_TYPE_COMPONENT);
    assert(pit->m_Node->m_Instance != 0);

    Prototype::Component* component_prototype = 0;
    ComponentType* component_type = 0;
    if (!ResolveComponentNode(pit->m_Node, &component_type, &component_prototype))
        return false;

    const dmhash_t names[] = {
        g_SceneNodePropertyName_id,
        g_SceneNodePropertyName_type,
        g_SceneNodePropertyName_resource
    };
    const size_t names_size = sizeof(names)/sizeof(names[0]);

    uint64_t index = pit->m_Next++;

    // Special case to allow for the internal properties of the component to be shown here
    // E.g. the properties of a scriptc
    if (index >= names_size)
    {
        if (component_type->m_IterProperties)
        {
            component_type->m_IterProperties(pit, pit->m_Node);
            return pit->m_FnIterateNext(pit);
        }
        return false;
    }

    pit->m_Property.m_NameHash = names[index];

    if (pit->m_Property.m_NameHash == g_SceneNodePropertyName_id)
    {
        pit->m_Property.m_Type = SCENE_NODE_PROPERTY_TYPE_HASH;
        pit->m_Property.m_Value.m_Hash = component_prototype->m_Id;
    }
    else if (pit->m_Property.m_NameHash == g_SceneNodePropertyName_type)
    {
        pit->m_Property.m_Type = SCENE_NODE_PROPERTY_TYPE_HASH;
        pit->m_Property.m_Value.m_Hash = component_prototype->m_Type->m_NameHash;
    }
    else if (pit->m_Property.m_NameHash == g_SceneNodePropertyName_resource)
    {
        pit->m_Property.m_Type = SCENE_NODE_PROPERTY_TYPE_HASH;
        pit->m_Property.m_Value.m_Hash = component_prototype->m_ResourceId;
    }
    return true;
}

static void IterateComponentProperties(SceneNodePropertyIterator* pit, SceneNode* node)
{
    assert(node->m_Type == SCENE_NODE_TYPE_COMPONENT);
    assert(node->m_ComponentType != 0);
    pit->m_Node = node;
    pit->m_Next = 0;
    pit->m_FnIterateNext = IterateComponentPropertiesGetNext;
}

// ********************************************************************************************
// Implementations for component types that want to be part of the scene graph, that doesn't have an hierarchy
// E.g. for iterating over the properties

static void IteratePropertiesNullProperties(struct SceneNodePropertyIterator* pit, struct SceneNode* node)
{
    pit->m_Node = 0;
}

// ********************************************************************************************

static void IterateSubComponentProperties(SceneNodePropertyIterator* pit, SceneNode* node)
{
    assert(node->m_Type == SCENE_NODE_TYPE_COMPONENT || node->m_Type == SCENE_NODE_TYPE_SUBCOMPONENT);
    ComponentType* component_type = 0;
    if (!ResolveComponentNode(node, &component_type, 0))
    {
        IteratePropertiesNullProperties(pit, node);
        return;
    }

    if (component_type->m_IterProperties)
        component_type->m_IterProperties(pit, node);
    else
        IteratePropertiesNullProperties(pit, node);
}


// ********************************************************************************************

SceneNodePropertyIterator TraverseIterateProperties(SceneNode* node)
{
    DM_PROFILE("TraverseIterateProperties");

    static bool first = true;
    if (first) {
        first = false;
        InitSceneNodePropertyNames();
    }

    FIteratorProperties fn = 0;
    switch(node->m_Type)
    {
    case SCENE_NODE_TYPE_COLLECTION:    fn = IterateCollectionProperties; break;
    case SCENE_NODE_TYPE_GAMEOBJECT:    fn = IterateGameObjectProperties; break;
    case SCENE_NODE_TYPE_COMPONENT:     fn = IterateComponentProperties; break;
    case SCENE_NODE_TYPE_SUBCOMPONENT:  fn = IterateSubComponentProperties; break;
    default: break;
    }

    SceneNodePropertyIterator pit = {};
    if (fn) {
        fn(&pit, node);
    } else {
        pit.m_Node = 0;
    }

    return pit;
}

bool TraverseIteratePropertiesNext(SceneNodePropertyIterator* pit)
{
    DM_PROFILE("TraverseIterateNext");
    if (!pit || !pit->m_Node || !pit->m_FnIterateNext)
        return false;
    if ((pit->m_Node->m_Type == SCENE_NODE_TYPE_COMPONENT || pit->m_Node->m_Type == SCENE_NODE_TYPE_SUBCOMPONENT)
        && !ResolveComponentNode(pit->m_Node, 0, 0))
    {
        return false;
    }
    return pit->m_FnIterateNext(pit);
}

}
