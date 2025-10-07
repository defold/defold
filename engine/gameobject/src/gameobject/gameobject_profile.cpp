// Copyright 2020-2025 The Defold Foundation
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
    HCollection hcollection = (HCollection)it->m_Parent.m_Node;
    Collection* collection = hcollection->m_Collection;

    const dmArray<uint16_t>& root_level = collection->m_LevelIndices[0];

    // If the index is still valid
    uint64_t index = it->m_NextChild.m_Node;
    bool valid = index < root_level.Size();

    if (valid) {
        it->m_Node = it->m_NextChild;
        it->m_Node.m_Instance = collection->m_Instances[root_level[index]];
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

    HCollection collection = (HCollection)node->m_Node;
    dmMutex::Lock(collection->m_Collection->m_Mutex);
}

// ********************************************************************************************


static bool IterateGameObjectGetNext(SceneNodeIterator* it)
{
    assert(it->m_Parent.m_Type == SCENE_NODE_TYPE_GAMEOBJECT);

    // We're using the same unsigned integer to hold two types of ranges (not at the same time though)
    // The first range is the valid ranges for game objects, which is less than INVALID_INSTANCE_INDEX
    // The second range is at a safe range above that (component_count_offset)
    const uint32_t invalid_index = 0xFFFFFFFF;
    const uint32_t component_count_offset = 0xFFFF;
    DM_STATIC_ASSERT(component_count_offset >= INVALID_INSTANCE_INDEX, _ranges_must_not_overlap);

    uint32_t index = (uint32_t)it->m_NextChild.m_Node;
    if (index == INVALID_INSTANCE_INDEX)
        index = component_count_offset;

    bool is_gameobject = index < INVALID_INSTANCE_INDEX;
    bool is_component = index >= component_count_offset && index != invalid_index && !is_gameobject;

    if (is_gameobject) {
        HInstance parent = it->m_Parent.m_Instance;
        HInstance instance = parent->m_Collection->m_Instances[index];

        it->m_Node = it->m_NextChild;
        it->m_Node.m_Instance = instance;
        it->m_NextChild.m_Node = instance->m_SiblingIndex;

        if (it->m_NextChild.m_Node == INVALID_INSTANCE_INDEX) {
            it->m_NextChild.m_Node = component_count_offset; // start iterating over the components
            it->m_NextChild.m_Type = SCENE_NODE_TYPE_SUBCOMPONENT;
        }

    } else if(is_component) {
        index -= component_count_offset; // make it zero based again, for iterating the array

        HInstance instance = it->m_Parent.m_Instance; // the owner of the component instances, i.e. the parent in this traversal
        Prototype* prototype = instance->m_Prototype;

        // Find the actual component instance data
        // if none exist at this index, fast forward to the next item in the list
        uint32_t next_component_instance_data = 0;
        uintptr_t* component_instance_data = 0;

        for (uint32_t k = 0; k < prototype->m_ComponentCount; ++k)
        {
            ComponentType* component_type = prototype->m_Components[k].m_Type;

            if (component_type->m_InstanceHasUserData)
            {
                component_instance_data = &instance->m_ComponentInstanceUserData[next_component_instance_data++];

                // We need to iterate from k=0 since the prototype potentially contains multiple instances of the same component type
                if (k >= index)
                {
                    index = k;
                    break;
                }
            }
        }

        if (component_instance_data)
        {
            Prototype::Component* component = &prototype->m_Components[index];
            void* component_world = instance->m_Collection->m_ComponentWorlds[component->m_TypeIndex];

            // the the actual instance data
            it->m_Node.m_Node = (uint64_t)*component_instance_data;
            it->m_Node.m_Type = SCENE_NODE_TYPE_COMPONENT;
            // Needed to iterate over the properties
            it->m_Node.m_ComponentWorld = component_world;
            it->m_Node.m_Component = *component_instance_data;
            it->m_Node.m_ComponentType = component->m_Type;
            it->m_Node.m_ComponentPrototype = (void*)&prototype->m_Components[index];
            it->m_Node.m_Instance = instance;

            if ((index + 1) >= prototype->m_ComponentCount)
                it->m_NextChild.m_Node = invalid_index;
            else
                it->m_NextChild.m_Node = index + component_count_offset + 1;
        } else
        {
            is_component = false;
        }

    } // We're done iterating this instance, let's iterate the components

    return is_gameobject || is_component;
}

static void IterateGameObjectChildren(SceneNodeIterator* it, SceneNode* node)
{
    assert(node->m_Type == SCENE_NODE_TYPE_GAMEOBJECT);

    it->m_Parent = *node;
    //it->m_NextChild = *node;
    it->m_NextChild.m_Type = SCENE_NODE_TYPE_GAMEOBJECT;
    it->m_NextChild.m_Node = (uint64_t)it->m_Parent.m_Instance->m_FirstChildIndex;
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

// ********************************************************************************************

static void IterateComponentChildren(SceneNodeIterator* it, SceneNode* node)
{
    assert(node->m_Type == SCENE_NODE_TYPE_COMPONENT || node->m_Type == SCENE_NODE_TYPE_SUBCOMPONENT);
    ComponentType* component_type = node->m_ComponentType;

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

bool TraverseGetRoot(HRegister regist, SceneNode* node)
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

    SceneNodeIterator it;
    if (fn) {
        fn(&it, node);
    }

    return  it;
}

bool TraverseIterateNext(SceneNodeIterator* it)
{
    DM_PROFILE("TraverseIterateNext");
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

    Collection* collection = pit->m_Node->m_Collection->m_Collection;

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
        dmResource::GetPath(collection->m_Factory, collection->m_HCollection, &pit->m_Property.m_Value.m_Hash);
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


    HInstance instance = pit->m_Node->m_Instance;
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

            Collection* collection = instance->m_Collection;
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
            case 0: value = Vector4(dmGameObject::GetPosition(instance)); break;
            case 1: value = Vector4(dmGameObject::GetRotation(instance)); type = SCENE_NODE_PROPERTY_TYPE_QUAT; break;
            case 2: value = Vector4(dmGameObject::GetScale(instance)); break;
            case 3: value = Vector4(dmGameObject::GetWorldPosition(instance)); break;
            case 4: value = Vector4(dmGameObject::GetWorldRotation(instance)); type = SCENE_NODE_PROPERTY_TYPE_QUAT; break;
            case 5: value = Vector4(dmGameObject::GetWorldScale(instance)); break;
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
    assert(pit->m_Node->m_ComponentType != 0);
    assert(pit->m_Node->m_Instance != 0);

    Prototype::Component* component_prototype = (Prototype::Component*)pit->m_Node->m_ComponentPrototype;
    ComponentType* component_type = pit->m_Node->m_ComponentType;

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
    assert(node->m_ComponentType != 0);

    if (node->m_ComponentType->m_IterProperties)
        node->m_ComponentType->m_IterProperties(pit, node);
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

    SceneNodePropertyIterator pit;
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
    if (pit->m_Node == 0)
        return false;
    return pit->m_FnIterateNext(pit);
}

}
