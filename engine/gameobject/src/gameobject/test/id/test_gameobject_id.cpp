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

#include <jc_test/jc_test.h>

#include <dlib/path.h>
#include <dlib/testutil.h>
#include <dmsdk/gameobject/res_collection.h>
#include <resource/resource.h>

#include "../gameobject.h"
#include "../gameobject_props.h"
#include "../component.h"
#include "../../gameobject_private.h"

class IdTest : public jc_test_base_class
{
protected:
    void SetUp() override
    {
        m_UpdateContext.m_DT = 1.0f / 60.0f;

        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_EMPTY;
        char path[DMPATH_MAX_PATH];
        m_Factory = dmResource::NewFactory(&params, dmTestUtil::MakeHostPath(path, sizeof(path), "build/src/gameobject/test/id"));
        dmScript::ContextParams script_context_params = {};
        m_ScriptContext = dmScript::NewContext(script_context_params);
        dmScript::Initialize(m_ScriptContext);
        m_Register = dmGameObject::NewRegister();
        dmGameObject::Initialize(m_Register, m_ScriptContext);

        m_Contexts.SetCapacity(7,16);
        m_Contexts.Put(dmHashString64("goc"), m_Register);
        m_Contexts.Put(dmHashString64("collectionc"), m_Register);
        m_Contexts.Put(dmHashString64("scriptc"), m_ScriptContext);
        m_Contexts.Put(dmHashString64("luac"), &m_ModuleContext);
        dmResource::RegisterTypes(m_Factory, &m_Contexts);

        dmGameObject::ComponentTypeCreateCtx component_create_ctx = {};
        component_create_ctx.m_Script = m_ScriptContext;
        component_create_ctx.m_Register = m_Register;
        component_create_ctx.m_Factory = m_Factory;
        dmGameObject::CreateRegisteredComponentTypes(&component_create_ctx);
        dmGameObject::SortComponentTypes(m_Register);

        m_Collection = dmGameObject::NewCollection("collection", m_Factory, m_Register, 1024, 0x0);
    }

    void TearDown() override
    {
        dmGameObject::DeleteCollection(m_Collection);
        dmGameObject::PostUpdate(m_Register);
        dmScript::Finalize(m_ScriptContext);
        dmScript::DeleteContext(m_ScriptContext);
        dmResource::DeleteFactory(m_Factory);
        dmGameObject::DeleteRegister(m_Register);
    }

public:
    dmScript::HContext m_ScriptContext;
    dmGameObject::UpdateContext m_UpdateContext;
    dmGameObject::HRegister m_Register;
    dmGameObject::HCollection m_Collection;
    dmResource::HFactory m_Factory;
    dmGameObject::ModuleContext m_ModuleContext;
    dmHashTable64<void*> m_Contexts;
};

TEST_F(IdTest, TestIdentifier)
{
    dmGameObject::HInstance go1 = dmGameObject::New(m_Collection, "/go.goc");
    dmGameObject::HInstance go2 = dmGameObject::New(m_Collection, "/go.goc");
    ASSERT_NE(0, go1);
    ASSERT_NE(0, go2);

    ASSERT_EQ(dmGameObject::UNNAMED_IDENTIFIER, dmGameObject::GetIdentifier(go1));
    ASSERT_EQ(dmGameObject::UNNAMED_IDENTIFIER, dmGameObject::GetIdentifier(go2));

    dmGameObject::Result r;
    r = dmGameObject::SetIdentifier(m_Collection, go1, "go1");
    ASSERT_EQ(dmGameObject::RESULT_OK, r);
    ASSERT_NE(dmGameObject::UNNAMED_IDENTIFIER, dmGameObject::GetIdentifier(go1));

    r = dmGameObject::SetIdentifier(m_Collection, go1, "go1");
    ASSERT_NE(dmGameObject::RESULT_OK, r);
    ASSERT_NE(dmGameObject::UNNAMED_IDENTIFIER, dmGameObject::GetIdentifier(go1));

    r = dmGameObject::SetIdentifier(m_Collection, go2, "go1");
    ASSERT_EQ(dmGameObject::RESULT_IDENTIFIER_IN_USE, r);
    ASSERT_EQ(dmGameObject::UNNAMED_IDENTIFIER, dmGameObject::GetIdentifier(go2));

    r = dmGameObject::SetIdentifier(m_Collection, go2, "go2");
    ASSERT_EQ(dmGameObject::RESULT_OK, r);
    ASSERT_NE(dmGameObject::UNNAMED_IDENTIFIER, dmGameObject::GetIdentifier(go2));

    r = dmGameObject::SetIdentifier(m_Collection, go2, "go2");
    ASSERT_NE(dmGameObject::RESULT_OK, r);

    dmGameObject::Delete(m_Collection, go1, false);
    dmGameObject::Delete(m_Collection, go2, false);
}

TEST_F(IdTest, TestHierarchies)
{
    dmGameObject::CollectionResource* collection_resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/root.collectionc", (void**)&collection_resource));
    dmGameObject::HCollection collection = dmGameObject::ResCollectionGetCollection(collection_resource);
    ASSERT_NE(0, collection);
    dmhash_t id = dmHashString64("/go");
    dmhash_t sub1_id = dmHashString64("/sub/go1");
    dmhash_t sub2_id = dmHashString64("/sub/go2");
    dmGameObject::HInstance instance = dmGameObject::GetInstanceFromIdentifier(collection, id);
    ASSERT_NE(0, instance);
    dmGameObject::HInstance sub1_instance = dmGameObject::GetInstanceFromIdentifier(collection, sub1_id);
    ASSERT_NE(0, sub1_instance);
    dmGameObject::HInstance sub2_instance = dmGameObject::GetInstanceFromIdentifier(collection, sub2_id);
    ASSERT_NE(0, sub2_instance);
    ASSERT_EQ(sub1_id, dmGameObject::GetAbsoluteIdentifier(instance, "sub/go1"));
    ASSERT_EQ(id, dmGameObject::GetAbsoluteIdentifier(sub1_instance, "/go"));
    ASSERT_EQ(sub2_id, dmGameObject::GetAbsoluteIdentifier(sub1_instance, "go2"));
    ASSERT_EQ(id, dmGameObject::GetAbsoluteIdentifier(sub2_instance, "/go"));
    dmResource::Release(m_Factory, collection_resource);
}

// Tests that recreating a game object with a reused identifier, still gets a new generation number
TEST_F(IdTest, TestGenerationChangesOnIdentifierReuse)
{
    dmGameObject::HInstance go1 = dmGameObject::New(m_Collection, "/go.goc");
    ASSERT_NE(0, go1);
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(m_Collection, go1, "go1"));

    dmhash_t id = dmGameObject::GetIdentifier(go1);
    uint32_t generation1 = dmGameObject::GetInstanceGeneration(go1);

    ASSERT_EQ(go1, dmGameObject::GetInstanceFromIdentifier(m_Collection, id));

    dmGameObject::Delete(m_Collection, go1, false);
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    ASSERT_EQ(0, dmGameObject::GetInstanceFromIdentifier(m_Collection, id));

    dmGameObject::HInstance go2 = dmGameObject::New(m_Collection, "/go.goc");
    ASSERT_NE(0, go2);
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(m_Collection, go2, "go1"));

    uint32_t generation2 = dmGameObject::GetInstanceGeneration(go2);

    ASSERT_LT(generation1, generation2);
    ASSERT_EQ(go2, dmGameObject::GetInstanceFromIdentifier(m_Collection, id));

    dmGameObject::Delete(m_Collection, go2, false);
}

TEST_F(IdTest, TestPackedHandlesAndStaleGameObject)
{
    const uint64_t instance_index_mask = (1ULL << 20) - 1;
    const uint64_t collection_index_mask = (1ULL << 12) - 1;
    dmGameObject::HInstance game_object = dmGameObject::New(m_Collection, 0);
    ASSERT_NE(0, game_object);
    ASSERT_EQ((uint64_t)(m_Collection & 0xffff), (game_object >> 20) & collection_index_mask);
    ASSERT_NE(0U, (uint32_t)(game_object >> 32));
    ASSERT_EQ((uint32_t)(game_object >> 32), dmGameObject::GetInstanceGeneration(game_object));
    ASSERT_TRUE(dmGameObject::IsValid(game_object));

    uint32_t index = (uint32_t)(game_object & instance_index_mask);
    dmGameObject::HInstance zero_generation = (game_object & 0xffffffffULL);
    ASSERT_EQ(0U, dmGameObject::GetInstanceGeneration(zero_generation));
    ASSERT_FALSE(dmGameObject::IsValid(zero_generation));

    dmGameObject::HInstance max_indices = ((uint64_t)dmGameObject::GetInstanceGeneration(game_object) << 32) |
                                          (collection_index_mask << 20) |
                                          instance_index_mask;
    ASSERT_FALSE(dmGameObject::IsValid(max_indices));

    dmGameObject::HInstance maximum_encoding = UINT64_MAX;
    ASSERT_EQ(UINT32_MAX, dmGameObject::GetInstanceGeneration(maximum_encoding));
    ASSERT_FALSE(dmGameObject::IsValid(maximum_encoding));
    ASSERT_EQ(0, dmGameObject::GetCollection(maximum_encoding));

    dmGameObject::Delete(m_Collection, game_object, false);
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    ASSERT_FALSE(dmGameObject::IsValid(game_object));

    dmGameObject::HInstance replacement = dmGameObject::New(m_Collection, 0);
    ASSERT_NE(0, replacement);
    ASSERT_EQ(index, (uint32_t)(replacement & instance_index_mask));
    ASSERT_NE((uint32_t)(game_object >> 32), (uint32_t)(replacement >> 32));
    ASSERT_FALSE(dmGameObject::IsValid(game_object));
    ASSERT_TRUE(dmGameObject::IsValid(replacement));

    dmGameObject::SetPosition(replacement, dmVMath::Point3(4.0f, 5.0f, 6.0f));
    dmGameObject::SetPosition(game_object, dmVMath::Point3(1.0f, 2.0f, 3.0f));
    dmGameObject::Delete(m_Collection, game_object, false);
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    ASSERT_TRUE(dmGameObject::IsValid(replacement));
    ASSERT_EQ(4.0f, dmGameObject::GetPosition(replacement).getX());
    ASSERT_EQ(5.0f, dmGameObject::GetPosition(replacement).getY());
    ASSERT_EQ(6.0f, dmGameObject::GetPosition(replacement).getZ());
    dmGameObject::Delete(m_Collection, replacement, false);
}

TEST_F(IdTest, TestGenerationRolloverHelpers)
{
    ASSERT_EQ(1U, dmGameObject::WrapIncrementU16(0));
    ASSERT_EQ(2U, dmGameObject::WrapIncrementU16(1));
    ASSERT_EQ(1U, dmGameObject::WrapIncrementU16(UINT16_MAX));
    ASSERT_EQ(1U, dmGameObject::WrapIncrementU32(0));
    ASSERT_EQ(2U, dmGameObject::WrapIncrementU32(1));
    ASSERT_EQ(1U, dmGameObject::WrapIncrementU32(UINT32_MAX));
}

TEST_F(IdTest, TestWrongCollectionAndStaleCollection)
{
    dmGameObject::HCollection first = dmGameObject::NewCollection("first", m_Factory, m_Register, 4, 0);
    dmGameObject::HCollection second = dmGameObject::NewCollection("second", m_Factory, m_Register, 4, 0);
    ASSERT_NE(0, first);
    ASSERT_NE(0, second);
    ASSERT_NE(0U, first >> 16);
    ASSERT_NE(0U, second >> 16);

    dmGameObject::HInstance first_object = dmGameObject::New(first, 0);
    dmGameObject::HInstance second_object = dmGameObject::New(second, 0);
    dmhash_t shared_identifier = dmHashString64("shared");
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(first, first_object, shared_identifier));
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(second, second_object, shared_identifier));
    ASSERT_TRUE(dmGameObject::IsValid(first_object));
    ASSERT_TRUE(dmGameObject::IsValid(second_object));
    ASSERT_EQ(first, dmGameObject::GetCollection(first_object));
    ASSERT_EQ(second, dmGameObject::GetCollection(second_object));
    ASSERT_EQ(dmGameObject::RESULT_INVALID_INSTANCE, dmGameObject::SetIdentifier(first, second_object, dmHashString64("wrong")));
    ASSERT_EQ(dmGameObject::RESULT_INVALID_INSTANCE, dmGameObject::SetIdentifier(second, first_object, dmHashString64("wrong")));
    dmGameObject::Delete(first, second_object, false);
    ASSERT_TRUE(dmGameObject::IsValid(second_object));
    ASSERT_EQ(dmGameObject::RESULT_INVALID_INSTANCE, dmGameObject::SetParent(first_object, second_object));
    ASSERT_EQ(first_object, dmGameObject::GetInstanceFromIdentifier(first, shared_identifier));
    ASSERT_EQ(second_object, dmGameObject::GetInstanceFromIdentifier(second, shared_identifier));

    uint16_t first_index = (uint16_t)first;
    uint16_t first_generation = (uint16_t)(first >> 16);
    dmGameObject::DeleteCollection(first);
    dmGameObject::PostUpdate(m_Register);
    ASSERT_FALSE(dmGameObject::IsValid(first_object));
    ASSERT_EQ(0, dmGameObject::GetCollection(first_object));
    ASSERT_EQ(0, dmGameObject::GetInstanceFromIdentifier(first, shared_identifier));

    dmGameObject::HCollection reused = dmGameObject::NewCollection("reused", m_Factory, m_Register, 4, 0);
    ASSERT_EQ(first_index, (uint16_t)reused);
    ASSERT_NE(first_generation, (uint16_t)(reused >> 16));
    dmGameObject::HInstance reused_object = dmGameObject::New(reused, 0);
    ASSERT_TRUE(dmGameObject::IsValid(reused_object));
    ASSERT_FALSE(dmGameObject::IsValid(first_object));

    dmGameObject::DeleteCollection(reused);
    dmGameObject::DeleteCollection(second);
    dmGameObject::PostUpdate(m_Register);
}

TEST_F(IdTest, TestInvalidHandleDefaults)
{
    dmGameObject::HInstance invalid_generation = 1;
    ASSERT_FALSE(dmGameObject::IsValid(invalid_generation));
    ASSERT_EQ(0U, dmGameObject::GetIdentifier(invalid_generation));
    ASSERT_EQ(0U, dmGameObject::GetInstanceGeneration(invalid_generation));
    ASSERT_EQ(0, dmGameObject::GetCollection(invalid_generation));
    ASSERT_EQ(dmGameObject::RESULT_INVALID_INSTANCE, dmGameObject::SetIdentifier(m_Collection, invalid_generation, "invalid"));

    dmVMath::Point3 position = dmGameObject::GetPosition(invalid_generation);
    ASSERT_EQ(0.0f, position.getX());
    ASSERT_EQ(0.0f, position.getY());
    ASSERT_EQ(0.0f, position.getZ());
    dmVMath::Quat rotation = dmGameObject::GetRotation(invalid_generation);
    ASSERT_EQ(0.0f, rotation.getX());
    ASSERT_EQ(0.0f, rotation.getY());
    ASSERT_EQ(0.0f, rotation.getZ());
    ASSERT_EQ(1.0f, rotation.getW());
    dmVMath::Vector3 scale = dmGameObject::GetScale(invalid_generation);
    ASSERT_EQ(1.0f, scale.getX());
    ASSERT_EQ(1.0f, scale.getY());
    ASSERT_EQ(1.0f, scale.getZ());
    ASSERT_EQ(1.0f, dmGameObject::GetUniformScale(invalid_generation));

    dmVMath::Point3 world_position = dmGameObject::GetWorldPosition(invalid_generation);
    ASSERT_EQ(0.0f, world_position.getX());
    ASSERT_EQ(0.0f, world_position.getY());
    ASSERT_EQ(0.0f, world_position.getZ());
    dmVMath::Quat world_rotation = dmGameObject::GetWorldRotation(invalid_generation);
    ASSERT_EQ(0.0f, world_rotation.getX());
    ASSERT_EQ(0.0f, world_rotation.getY());
    ASSERT_EQ(0.0f, world_rotation.getZ());
    ASSERT_EQ(1.0f, world_rotation.getW());
    dmVMath::Vector3 world_scale = dmGameObject::GetWorldScale(invalid_generation);
    ASSERT_EQ(1.0f, world_scale.getX());
    ASSERT_EQ(1.0f, world_scale.getY());
    ASSERT_EQ(1.0f, world_scale.getZ());
    ASSERT_EQ(1.0f, dmGameObject::GetWorldUniformScale(invalid_generation));
    const dmVMath::Matrix4& world_matrix = dmGameObject::GetWorldMatrix(invalid_generation);
    ASSERT_EQ(1.0f, world_matrix.getCol0().getX());
    ASSERT_EQ(1.0f, world_matrix.getCol1().getY());
    ASSERT_EQ(1.0f, world_matrix.getCol2().getZ());
    ASSERT_EQ(1.0f, world_matrix.getCol3().getW());
    dmTransform::Transform world_transform = dmGameObject::GetWorldTransform(invalid_generation);
    ASSERT_EQ(0.0f, world_transform.GetTranslation().getX());
    ASSERT_EQ(1.0f, world_transform.GetScale().getX());

    dmGameObject::SetPosition(invalid_generation, dmVMath::Point3(1.0f, 2.0f, 3.0f));
    dmGameObject::SetRotation(invalid_generation, dmVMath::Quat::identity());
    dmGameObject::SetScale(invalid_generation, 2.0f);
    dmGameObject::SetScaleXY(invalid_generation, 2.0f, 3.0f);
    dmGameObject::SetBone(invalid_generation, true);
    dmGameObject::Delete(m_Collection, invalid_generation, true);

    const dmGameObject::HCollection invalid_collection = 0;
    dmGameObject::PropertyDesc property_desc;
    dmGameObject::PropertyOptions property_options;
    uint32_t component_type = 1;
    dmGameObject::HComponent component = (dmGameObject::HComponent)1;
    dmGameObject::HComponentWorld world = (dmGameObject::HComponentWorld)1;
    dmGameObject::InputAction input_action = {};
    dmGameObject::HInstance spawned = 1;
    uint16_t component_index = 1;
    dmhash_t component_id = 1;

    ASSERT_EQ(0, dmGameObject::New(invalid_collection, 0));
    ASSERT_EQ(0, dmGameObject::Spawn(invalid_collection, 0, 0, 0, 0, dmVMath::Point3(), dmVMath::Quat::identity(), dmVMath::Vector3(1.0f)));
    ASSERT_EQ(dmGameObject::RESULT_INVALID_INSTANCE, dmGameObject::Spawn(invalid_collection, 0, 0, 0, 0, dmVMath::Point3(), dmVMath::Quat::identity(), dmVMath::Vector3(1.0f), &spawned));
    ASSERT_EQ(0, spawned);
    ASSERT_EQ(dmGameObject::RESULT_INVALID_INSTANCE, dmGameObject::GetComponentIndex(invalid_generation, 0, &component_index));
    ASSERT_EQ(0U, component_index);
    ASSERT_EQ(dmGameObject::RESULT_INVALID_INSTANCE, dmGameObject::GetComponentId(invalid_generation, 0, &component_id));
    ASSERT_EQ(0U, component_id);
    ASSERT_EQ(dmGameObject::RESULT_INVALID_INSTANCE, dmGameObject::GetComponent(invalid_generation, 0, &component_type, &component, &world));
    ASSERT_EQ(0U, component_type);
    ASSERT_EQ((dmGameObject::HComponent)0, component);
    ASSERT_EQ((dmGameObject::HComponentWorld)0, world);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_INVALID_INSTANCE, dmGameObject::GetProperty(invalid_generation, 0, 0, property_options, property_desc));
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_INVALID_INSTANCE, dmGameObject::SetProperty(invalid_generation, 0, 0, property_options, dmGameObject::PropertyVar(1.0f)));
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_INVALID_INSTANCE, dmGameObject::CancelAnimations(invalid_collection, invalid_generation, 0, 0));
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_INVALID_INSTANCE, dmGameObject::CancelAnimations(m_Collection, invalid_generation, 0, 1));
    ASSERT_EQ(dmGameObject::RESULT_INVALID_INSTANCE, dmGameObject::SetParent(invalid_generation, 0));
    ASSERT_EQ(0, dmGameObject::GetParent(invalid_generation));
    ASSERT_EQ(0xFFFFFFFFU, dmGameObject::GetComponentTypeIndex(invalid_collection, 0));
    ASSERT_EQ((dmGameObject::HComponentWorld)0, dmGameObject::GetWorld(invalid_collection, 0));
    ASSERT_EQ((void*)0, dmGameObject::GetContext(invalid_collection, 0));
    ASSERT_EQ((dmResource::HFactory)0, dmGameObject::GetFactory(invalid_collection));
    ASSERT_EQ((dmGameObject::HRegister)0, dmGameObject::GetRegister(invalid_collection));
    ASSERT_EQ((dmMessage::HSocket)0, dmGameObject::GetMessageSocket(invalid_collection));
    ASSERT_EQ((dmMessage::HSocket)0, dmGameObject::GetFrameMessageSocket(invalid_collection));
    ASSERT_FALSE(dmGameObject::Init(invalid_collection));
    ASSERT_FALSE(dmGameObject::Final(invalid_collection));
    ASSERT_FALSE(dmGameObject::Update(invalid_collection, &m_UpdateContext));
    ASSERT_FALSE(dmGameObject::Render(invalid_collection));
    ASSERT_FALSE(dmGameObject::PostUpdate(invalid_collection));
    ASSERT_EQ(dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR, dmGameObject::DispatchInput(invalid_collection, &input_action, 1));
    dmGameObject::UpdateTransforms(invalid_collection);
    dmGameObject::DeleteAll(invalid_collection);
    dmGameObject::AddDynamicResourceHash(invalid_collection, 1);
    dmGameObject::RemoveDynamicResourceHash(invalid_collection, 1);
    dmGameObject::CancelAnimations(m_Collection, invalid_generation);
    dmGameObject::CancelAnimationCallbacks(invalid_collection, 0);

    const dmGameObject::HCollection zero_generation_collection = 1;
    const dmGameObject::HCollection sentinel_index_collection = (1U << 16) | UINT16_MAX;
    ASSERT_EQ((dmGameObject::HRegister)0, dmGameObject::GetRegister(zero_generation_collection));
    ASSERT_EQ((dmGameObject::HRegister)0, dmGameObject::GetRegister(sentinel_index_collection));
    ASSERT_EQ((dmResource::HFactory)0, dmGameObject::GetFactory(zero_generation_collection));
    ASSERT_EQ((dmResource::HFactory)0, dmGameObject::GetFactory(sentinel_index_collection));
    ASSERT_FALSE(dmGameObject::IsValid(invalid_generation));
}

TEST_F(IdTest, TestSceneTraversalRejectsStaleHandles)
{
    dmGameObject::HInstance parent = dmGameObject::New(m_Collection, 0);
    dmGameObject::HInstance child = dmGameObject::New(m_Collection, 0);
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetParent(child, parent));

    dmGameObject::SceneNode parent_node = {};
    parent_node.m_Type = dmGameObject::SCENE_NODE_TYPE_GAMEOBJECT;
    parent_node.m_Collection = m_Collection;
    parent_node.m_Instance = parent;
    dmGameObject::SceneNodeIterator children = dmGameObject::TraverseIterateChildren(&parent_node);

    dmGameObject::Delete(m_Collection, child, false);
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    ASSERT_FALSE(dmGameObject::TraverseIterateNext(&children));

    dmGameObject::HInstance component_owner = dmGameObject::New(m_Collection, "/go.goc");
    ASSERT_NE(0, component_owner);

    dmGameObject::SceneNode component_owner_node = {};
    component_owner_node.m_Type = dmGameObject::SCENE_NODE_TYPE_GAMEOBJECT;
    component_owner_node.m_Collection = m_Collection;
    component_owner_node.m_Instance = component_owner;
    dmGameObject::SceneNodeIterator component_nodes = dmGameObject::TraverseIterateChildren(&component_owner_node);

    dmGameObject::SceneNode component_node = {};
    while (dmGameObject::TraverseIterateNext(&component_nodes))
    {
        if (component_nodes.m_Node.m_Type == dmGameObject::SCENE_NODE_TYPE_COMPONENT)
        {
            component_node = component_nodes.m_Node;
            break;
        }
    }
    ASSERT_EQ(dmGameObject::SCENE_NODE_TYPE_COMPONENT, component_node.m_Type);

    dmGameObject::Delete(m_Collection, component_owner, false);
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    dmGameObject::SceneNodeIterator component_children = dmGameObject::TraverseIterateChildren(&component_node);
    ASSERT_FALSE(dmGameObject::TraverseIterateNext(&component_children));
    dmGameObject::SceneNodePropertyIterator component_properties = dmGameObject::TraverseIterateProperties(&component_node);
    ASSERT_FALSE(dmGameObject::TraverseIteratePropertiesNext(&component_properties));

    dmGameObject::HCollection other_collection = dmGameObject::NewCollection("scene_stale_other", m_Factory, m_Register, 1, 0);
    ASSERT_NE(0, other_collection);
    dmGameObject::HInstance other_object = dmGameObject::New(other_collection, 0);
    component_node.m_Instance = other_object;
    component_children = dmGameObject::TraverseIterateChildren(&component_node);
    ASSERT_FALSE(dmGameObject::TraverseIterateNext(&component_children));
    component_properties = dmGameObject::TraverseIterateProperties(&component_node);
    ASSERT_FALSE(dmGameObject::TraverseIteratePropertiesNext(&component_properties));

    dmGameObject::Delete(m_Collection, parent, false);
    dmGameObject::DeleteCollection(other_collection);
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Register));
}

TEST_F(IdTest, TestGameObjectsAcrossLegacyIndexBoundary)
{
    const uint32_t object_count = 100000;
    dmGameObject::HCollection collection = dmGameObject::NewCollection("large", m_Factory, m_Register, object_count, 0);
    ASSERT_NE(0, collection);

    dmGameObject::HInstance first = 0;
    dmGameObject::HInstance low = 0;
    dmGameObject::HInstance boundary = 0;
    dmGameObject::HInstance above_boundary = 0;
    dmGameObject::HInstance last = 0;
    dmhash_t first_id = dmHashString64("large-first");
    for (uint32_t i = 0; i < object_count; ++i)
    {
        uint32_t identifier_index = dmGameObject::AcquireInstanceIndex(collection);
        ASSERT_EQ(i, identifier_index);
        dmGameObject::HInstance game_object = dmGameObject::New(collection, 0);
        ASSERT_NE(0, game_object);
        dmGameObject::AssignInstanceIndex(identifier_index, game_object);
        if (i == 0)
        {
            first = game_object;
            ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(collection, first, first_id));
        }
        if (i == 1)
            low = game_object;
        if (i == 65535)
            boundary = game_object;
        if (i == 65536)
            above_boundary = game_object;
        if (i == object_count - 1)
            last = game_object;
    }
    ASSERT_EQ(dmGameObject::INVALID_INSTANCE_POOL_INDEX, dmGameObject::AcquireInstanceIndex(collection));

    ASSERT_EQ(65535U, (uint32_t)(boundary & ((1ULL << 20) - 1)));
    ASSERT_EQ(65536U, (uint32_t)(above_boundary & ((1ULL << 20) - 1)));
    ASSERT_EQ(object_count - 1, (uint32_t)(last & ((1ULL << 20) - 1)));
    ASSERT_EQ(first, dmGameObject::GetInstanceFromIdentifier(collection, first_id));

    // Collection initialization must scan allocated slots, not just the live
    // count. Freeing a low slot must not leave a live high-index object uninitialized.
    dmGameObject::Delete(collection, low, false);
    ASSERT_TRUE(dmGameObject::PostUpdate(collection));
    ASSERT_FALSE(dmGameObject::IsValid(low));
    ASSERT_TRUE(dmGameObject::Init(collection));
    dmGameObject::Collection* collection_ptr = dmGameObject::GetCollectionFromHandle(collection);
    dmGameObject::Instance* last_instance = dmGameObject::GetInstanceFromHandle(collection_ptr, last);
    ASSERT_NE((dmGameObject::Instance*)0, last_instance);
    ASSERT_TRUE(last_instance->m_Initialized);

    dmhash_t last_id = dmHashString64("large-last");
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(collection, last, last_id));
    ASSERT_EQ(last, dmGameObject::GetInstanceFromIdentifier(collection, last_id));

    dmGameObject::SetPosition(last, dmVMath::Point3(1.0f, 2.0f, 3.0f));
    ASSERT_EQ(3.0f, dmGameObject::GetPosition(last).getZ());
    dmGameObject::SetPosition(first, dmVMath::Point3(10.0f, 0.0f, 0.0f));
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetParent(last, first));
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetParent(boundary, first));
    ASSERT_EQ(first, dmGameObject::GetParent(last));
    ASSERT_EQ(first, dmGameObject::GetParent(boundary));
    ASSERT_EQ(2U, dmGameObject::GetChildCount(first));

    dmGameObject::UpdateTransforms(collection);
    ASSERT_EQ(11.0f, dmGameObject::GetWorldPosition(last).getX());

    // Traverse through the public scene API. The collection iterator must be
    // exhausted because it owns the collection lock for its lifetime.
    dmGameObject::SceneNode collection_node = {};
    collection_node.m_Node = collection;
    collection_node.m_Type = dmGameObject::SCENE_NODE_TYPE_COLLECTION;
    collection_node.m_Collection = collection;
    dmGameObject::SceneNodeIterator roots = dmGameObject::TraverseIterateChildren(&collection_node);
    dmGameObject::SceneNode first_node = {};
    uint32_t root_count = 0;
    while (dmGameObject::TraverseIterateNext(&roots))
    {
        if (roots.m_Node.m_Instance == first)
            first_node = roots.m_Node;
        ++root_count;
    }
    ASSERT_EQ(object_count - 3, root_count);
    ASSERT_EQ(first, first_node.m_Instance);

    dmGameObject::SceneNodeIterator children = dmGameObject::TraverseIterateChildren(&first_node);
    bool found_boundary = false;
    bool found_last = false;
    uint32_t child_count = 0;
    while (dmGameObject::TraverseIterateNext(&children))
    {
        found_boundary |= children.m_Node.m_Instance == boundary;
        found_last |= children.m_Node.m_Instance == last;
        ++child_count;
    }
    ASSERT_EQ(2U, child_count);
    ASSERT_TRUE(found_boundary);
    ASSERT_TRUE(found_last);

    // Exercise the deferred-delete list with an index that cannot fit in 16 bits.
    dmGameObject::Delete(collection, last, false);
    ASSERT_TRUE(dmGameObject::PostUpdate(collection));
    ASSERT_EQ(1U, dmGameObject::GetChildCount(first));
    ASSERT_EQ(first, dmGameObject::GetParent(boundary));
    uint32_t reused_identifier_index = dmGameObject::AcquireInstanceIndex(collection);
    ASSERT_EQ(object_count - 1, reused_identifier_index);

    dmGameObject::HPrototype prototype = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/go.goc", (void**)&prototype));
    dmGameObject::HInstance reused = 0;
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::Spawn(collection, prototype, "/go.goc",
            dmHashString64("/large-reused"), 0, dmVMath::Point3(), dmVMath::Quat::identity(),
            dmVMath::Vector3(1.0f), &reused));
    dmResource::Release(m_Factory, prototype);
    dmGameObject::AssignInstanceIndex(reused_identifier_index, reused);
    ASSERT_EQ(object_count - 1, (uint32_t)(reused & ((1ULL << 20) - 1)));
    ASSERT_NE(last, reused);
    ASSERT_EQ(1U, dmGameObject::GetAddToUpdateCount(collection));
    ASSERT_TRUE(dmGameObject::Update(collection, &m_UpdateContext));
    ASSERT_EQ(0U, dmGameObject::GetAddToUpdateCount(collection));
    ASSERT_FALSE(dmGameObject::IsValid(last));
    ASSERT_TRUE(dmGameObject::IsValid(boundary));
    ASSERT_TRUE(dmGameObject::IsValid(above_boundary));
    ASSERT_TRUE(dmGameObject::IsValid(reused));

    dmGameObject::DeleteCollection(collection);
    dmGameObject::PostUpdate(m_Register);
}
