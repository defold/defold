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
        m_Register = dmGameObject::NewContext();
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
        dmGameObject::DeleteContext(m_Register);
    }

public:
    dmScript::HContext m_ScriptContext;
    dmGameObject::UpdateContext m_UpdateContext;
    dmGameObject::HContext m_Register;
    dmGameObject::HCollection m_Collection;
    dmResource::HFactory m_Factory;
    dmGameObject::ModuleContext m_ModuleContext;
    dmHashTable64<void*> m_Contexts;
};

TEST_F(IdTest, TestIdentifier)
{
    dmGameObject::HGameObject go1 = dmGameObject::New(m_Collection, "/go.goc");
    dmGameObject::HGameObject go2 = dmGameObject::New(m_Collection, "/go.goc");
    ASSERT_NE(dmGameObject::INVALID_GAME_OBJECT, go1);
    ASSERT_NE(dmGameObject::INVALID_GAME_OBJECT, go2);

    ASSERT_EQ(dmGameObject::UNNAMED_IDENTIFIER, dmGameObject::GetIdentifier(m_Collection, go1));
    ASSERT_EQ(dmGameObject::UNNAMED_IDENTIFIER, dmGameObject::GetIdentifier(m_Collection, go2));

    dmGameObject::Result r;
    r = dmGameObject::SetIdentifier(m_Collection, go1, "go1");
    ASSERT_EQ(dmGameObject::RESULT_OK, r);
    ASSERT_NE(dmGameObject::UNNAMED_IDENTIFIER, dmGameObject::GetIdentifier(m_Collection, go1));

    dmGameObject::HRegister legacy_register = m_Register;
    dmGameObject::HInstance legacy_instance = dmGameObject::GetInstanceFromIdentifier(m_Collection, dmHashString64("go1"));
    ASSERT_EQ(m_Register, legacy_register);
    ASSERT_EQ(go1, legacy_instance);

    r = dmGameObject::SetIdentifier(m_Collection, go1, "go1");
    ASSERT_NE(dmGameObject::RESULT_OK, r);
    ASSERT_NE(dmGameObject::UNNAMED_IDENTIFIER, dmGameObject::GetIdentifier(m_Collection, go1));

    r = dmGameObject::SetIdentifier(m_Collection, go2, "go1");
    ASSERT_EQ(dmGameObject::RESULT_IDENTIFIER_IN_USE, r);
    ASSERT_EQ(dmGameObject::UNNAMED_IDENTIFIER, dmGameObject::GetIdentifier(m_Collection, go2));

    r = dmGameObject::SetIdentifier(m_Collection, go2, "go2");
    ASSERT_EQ(dmGameObject::RESULT_OK, r);
    ASSERT_NE(dmGameObject::UNNAMED_IDENTIFIER, dmGameObject::GetIdentifier(m_Collection, go2));

    r = dmGameObject::SetIdentifier(m_Collection, go2, "go2");
    ASSERT_NE(dmGameObject::RESULT_OK, r);

    dmGameObject::Delete(m_Collection, go1, false);
    dmGameObject::Delete(m_Collection, go2, false);
}

TEST_F(IdTest, TestHierarchies)
{
    dmGameObject::HCollectionResource collection_resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/root.collectionc", (void**)&collection_resource));
    dmGameObject::HCollection collection = dmGameObject::GetCollectionFromResource(collection_resource);
    ASSERT_NE(dmGameObject::INVALID_COLLECTION, collection);
    dmhash_t id = dmHashString64("/go");
    dmhash_t sub1_id = dmHashString64("/sub/go1");
    dmhash_t sub2_id = dmHashString64("/sub/go2");
    dmGameObject::HGameObject instance = dmGameObject::GetGameObjectFromIdentifier(collection, id);
    ASSERT_NE(dmGameObject::INVALID_GAME_OBJECT, instance);
    dmGameObject::HGameObject sub1_instance = dmGameObject::GetGameObjectFromIdentifier(collection, sub1_id);
    ASSERT_NE(dmGameObject::INVALID_GAME_OBJECT, sub1_instance);
    dmGameObject::HGameObject sub2_instance = dmGameObject::GetGameObjectFromIdentifier(collection, sub2_id);
    ASSERT_NE(dmGameObject::INVALID_GAME_OBJECT, sub2_instance);
    ASSERT_EQ(sub1_id, dmGameObject::GetAbsoluteIdentifier(collection, instance, "sub/go1"));
    ASSERT_EQ(id, dmGameObject::GetAbsoluteIdentifier(collection, sub1_instance, "/go"));
    ASSERT_EQ(sub2_id, dmGameObject::GetAbsoluteIdentifier(collection, sub1_instance, "go2"));
    ASSERT_EQ(id, dmGameObject::GetAbsoluteIdentifier(collection, sub2_instance, "/go"));
    dmResource::Release(m_Factory, collection_resource);
}

// Tests that recreating a game object with a reused identifier, still gets a new generation number
TEST_F(IdTest, TestGenerationChangesOnIdentifierReuse)
{
    dmGameObject::HGameObject go1 = dmGameObject::New(m_Collection, "/go.goc");
    ASSERT_NE(dmGameObject::INVALID_GAME_OBJECT, go1);
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(m_Collection, go1, "go1"));

    dmhash_t id = dmGameObject::GetIdentifier(m_Collection, go1);
    uint32_t generation1 = dmGameObject::GetGeneration(m_Collection, go1);

    ASSERT_EQ(go1, dmGameObject::GetGameObjectFromIdentifier(m_Collection, id));

    dmGameObject::Delete(m_Collection, go1, false);
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    ASSERT_EQ(dmGameObject::INVALID_GAME_OBJECT, dmGameObject::GetGameObjectFromIdentifier(m_Collection, id));

    dmGameObject::HGameObject go2 = dmGameObject::New(m_Collection, "/go.goc");
    ASSERT_NE(dmGameObject::INVALID_GAME_OBJECT, go2);
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(m_Collection, go2, "go1"));

    uint32_t generation2 = dmGameObject::GetGeneration(m_Collection, go2);

    ASSERT_LT(generation1, generation2);
    ASSERT_EQ(go2, dmGameObject::GetGameObjectFromIdentifier(m_Collection, id));

    dmGameObject::Delete(m_Collection, go2, false);
}

TEST_F(IdTest, TestPackedHandlesAndStaleGameObject)
{
    dmGameObject::HGameObject game_object = dmGameObject::New(m_Collection, 0);
    ASSERT_NE(dmGameObject::INVALID_GAME_OBJECT, game_object);
    ASSERT_NE(0U, (uint32_t)(game_object >> 32));
    ASSERT_EQ((uint32_t)(game_object >> 32), dmGameObject::GetGeneration(m_Collection, game_object));
    ASSERT_TRUE(dmGameObject::IsValid(m_Collection, game_object));

    uint32_t index = (uint32_t)game_object;
    dmGameObject::Delete(m_Collection, game_object, false);
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    ASSERT_FALSE(dmGameObject::IsValid(m_Collection, game_object));

    dmGameObject::HGameObject replacement = dmGameObject::New(m_Collection, 0);
    ASSERT_NE(dmGameObject::INVALID_GAME_OBJECT, replacement);
    ASSERT_EQ(index, (uint32_t)replacement);
    ASSERT_NE((uint32_t)(game_object >> 32), (uint32_t)(replacement >> 32));
    ASSERT_FALSE(dmGameObject::IsValid(m_Collection, game_object));
    ASSERT_TRUE(dmGameObject::IsValid(m_Collection, replacement));
    dmGameObject::Delete(m_Collection, replacement, false);
}

TEST_F(IdTest, TestGenerationRolloverHelpers)
{
    ASSERT_EQ(1U, dmGameObject::NextCollectionGeneration(0));
    ASSERT_EQ(2U, dmGameObject::NextCollectionGeneration(1));
    ASSERT_EQ(1U, dmGameObject::NextCollectionGeneration(UINT16_MAX));
    ASSERT_EQ(1U, dmGameObject::NextGameObjectGeneration(0));
    ASSERT_EQ(2U, dmGameObject::NextGameObjectGeneration(1));
    ASSERT_EQ(1U, dmGameObject::NextGameObjectGeneration(UINT32_MAX));
}

TEST_F(IdTest, TestWrongCollectionAndStaleCollection)
{
    dmGameObject::HCollection first = dmGameObject::NewCollection("first", m_Factory, m_Register, 4, 0);
    dmGameObject::HCollection second = dmGameObject::NewCollection("second", m_Factory, m_Register, 4, 0);
    ASSERT_NE(dmGameObject::INVALID_COLLECTION, first);
    ASSERT_NE(dmGameObject::INVALID_COLLECTION, second);
    ASSERT_NE(0U, first >> 16);
    ASSERT_NE(0U, second >> 16);

    dmGameObject::HGameObject first_object = dmGameObject::New(first, 0);
    dmGameObject::HGameObject second_object = dmGameObject::New(second, 0);
    dmhash_t shared_identifier = dmHashString64("shared");
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(first, first_object, shared_identifier));
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(second, second_object, shared_identifier));
    ASSERT_TRUE(dmGameObject::IsValid(first, first_object));
    ASSERT_TRUE(dmGameObject::IsValid(second, second_object));
    ASSERT_FALSE(dmGameObject::IsValid(first, second_object));
    ASSERT_FALSE(dmGameObject::IsValid(second, first_object));
    ASSERT_EQ(first_object, dmGameObject::GetGameObjectFromIdentifier(first, shared_identifier));
    ASSERT_EQ(second_object, dmGameObject::GetGameObjectFromIdentifier(second, shared_identifier));

    uint16_t first_index = (uint16_t)first;
    uint16_t first_generation = (uint16_t)(first >> 16);
    dmGameObject::DeleteCollection(first);
    dmGameObject::PostUpdate(m_Register);
    ASSERT_FALSE(dmGameObject::IsValid(first, first_object));
    ASSERT_EQ(dmGameObject::INVALID_GAME_OBJECT, dmGameObject::GetGameObjectFromIdentifier(first, shared_identifier));

    dmGameObject::HCollection reused = dmGameObject::NewCollection("reused", m_Factory, m_Register, 4, 0);
    ASSERT_EQ(first_index, (uint16_t)reused);
    ASSERT_NE(first_generation, (uint16_t)(reused >> 16));
    ASSERT_FALSE(dmGameObject::IsValid(first, dmGameObject::New(reused, 0)));

    dmGameObject::DeleteCollection(reused);
    dmGameObject::DeleteCollection(second);
    dmGameObject::PostUpdate(m_Register);
}

TEST_F(IdTest, TestCollectionGenerationSurvivesContextDestruction)
{
    dmGameObject::HContext first_context = dmGameObject::NewContext();
    dmGameObject::HCollection first_collection = dmGameObject::NewCollection("context_lifecycle", m_Factory, first_context, 1, 0);
    ASSERT_NE(dmGameObject::INVALID_COLLECTION, first_collection);

    const uint16_t collection_index = (uint16_t)first_collection;
    const uint16_t collection_generation = (uint16_t)(first_collection >> 16);
    dmGameObject::DeleteContext(first_context);
    ASSERT_EQ((dmGameObject::HContext)0, dmGameObject::GetGameObjectContext(first_collection));

    dmGameObject::HContext second_context = dmGameObject::NewContext();
    dmGameObject::HCollection second_collection = dmGameObject::NewCollection("context_lifecycle", m_Factory, second_context, 1, 0);
    ASSERT_NE(dmGameObject::INVALID_COLLECTION, second_collection);
    ASSERT_EQ(collection_index, (uint16_t)second_collection);
    ASSERT_NE(collection_generation, (uint16_t)(second_collection >> 16));
    ASSERT_EQ(second_context, dmGameObject::GetGameObjectContext(second_collection));

    dmGameObject::DeleteContext(second_context);
}

TEST_F(IdTest, TestInvalidHandleDefaults)
{
    dmGameObject::HGameObject invalid_generation = 1;
    ASSERT_FALSE(dmGameObject::IsValid(m_Collection, invalid_generation));
    ASSERT_EQ(0U, dmGameObject::GetIdentifier(m_Collection, invalid_generation));
    ASSERT_EQ(0U, dmGameObject::GetGeneration(m_Collection, invalid_generation));
    ASSERT_EQ(dmGameObject::RESULT_INVALID_INSTANCE, dmGameObject::SetIdentifier(m_Collection, invalid_generation, "invalid"));

    dmVMath::Point3 position = dmGameObject::GetPosition(m_Collection, invalid_generation);
    ASSERT_EQ(0.0f, position.getX());
    ASSERT_EQ(0.0f, position.getY());
    ASSERT_EQ(0.0f, position.getZ());
    dmVMath::Quat rotation = dmGameObject::GetRotation(m_Collection, invalid_generation);
    ASSERT_EQ(0.0f, rotation.getX());
    ASSERT_EQ(0.0f, rotation.getY());
    ASSERT_EQ(0.0f, rotation.getZ());
    ASSERT_EQ(1.0f, rotation.getW());
    dmVMath::Vector3 scale = dmGameObject::GetScale(m_Collection, invalid_generation);
    ASSERT_EQ(1.0f, scale.getX());
    ASSERT_EQ(1.0f, scale.getY());
    ASSERT_EQ(1.0f, scale.getZ());
    ASSERT_EQ(1.0f, dmGameObject::GetUniformScale(m_Collection, invalid_generation));

    dmVMath::Point3 world_position = dmGameObject::GetWorldPosition(m_Collection, invalid_generation);
    ASSERT_EQ(0.0f, world_position.getX());
    ASSERT_EQ(0.0f, world_position.getY());
    ASSERT_EQ(0.0f, world_position.getZ());
    dmVMath::Quat world_rotation = dmGameObject::GetWorldRotation(m_Collection, invalid_generation);
    ASSERT_EQ(0.0f, world_rotation.getX());
    ASSERT_EQ(0.0f, world_rotation.getY());
    ASSERT_EQ(0.0f, world_rotation.getZ());
    ASSERT_EQ(1.0f, world_rotation.getW());
    dmVMath::Vector3 world_scale = dmGameObject::GetWorldScale(m_Collection, invalid_generation);
    ASSERT_EQ(1.0f, world_scale.getX());
    ASSERT_EQ(1.0f, world_scale.getY());
    ASSERT_EQ(1.0f, world_scale.getZ());
    ASSERT_EQ(1.0f, dmGameObject::GetWorldUniformScale(m_Collection, invalid_generation));
    const dmVMath::Matrix4& world_matrix = dmGameObject::GetWorldMatrix(m_Collection, invalid_generation);
    ASSERT_EQ(1.0f, world_matrix.getCol0().getX());
    ASSERT_EQ(1.0f, world_matrix.getCol1().getY());
    ASSERT_EQ(1.0f, world_matrix.getCol2().getZ());
    ASSERT_EQ(1.0f, world_matrix.getCol3().getW());
    dmTransform::Transform world_transform = dmGameObject::GetWorldTransform(m_Collection, invalid_generation);
    ASSERT_EQ(0.0f, world_transform.GetTranslation().getX());
    ASSERT_EQ(1.0f, world_transform.GetScale().getX());

    dmGameObject::SetPosition(m_Collection, invalid_generation, dmVMath::Point3(1.0f, 2.0f, 3.0f));
    dmGameObject::SetRotation(m_Collection, invalid_generation, dmVMath::Quat::identity());
    dmGameObject::SetScale(m_Collection, invalid_generation, 2.0f);
    dmGameObject::SetScaleXY(m_Collection, invalid_generation, 2.0f, 3.0f);
    dmGameObject::SetBone(m_Collection, invalid_generation, true);
    dmGameObject::Delete(m_Collection, invalid_generation, true);

    const dmGameObject::HCollection invalid_collection = dmGameObject::INVALID_COLLECTION;
    dmGameObject::PropertyDesc property_desc;
    dmGameObject::PropertyOptions property_options;
    uint32_t component_type = 1;
    dmGameObject::HComponent component = (dmGameObject::HComponent)1;
    dmGameObject::HComponentWorld world = (dmGameObject::HComponentWorld)1;
    dmGameObject::InputAction input_action = {};
    dmGameObject::HGameObject spawned = 1;
    uint16_t component_index = 1;
    dmhash_t component_id = 1;

    ASSERT_EQ(dmGameObject::INVALID_GAME_OBJECT, dmGameObject::New(invalid_collection, 0));
    ASSERT_EQ(dmGameObject::INVALID_GAME_OBJECT, dmGameObject::Spawn(invalid_collection, 0, 0, 0, 0, dmVMath::Point3(), dmVMath::Quat::identity(), dmVMath::Vector3(1.0f)));
    ASSERT_EQ(dmGameObject::RESULT_INVALID_INSTANCE, dmGameObject::Spawn(invalid_collection, 0, 0, 0, 0, dmVMath::Point3(), dmVMath::Quat::identity(), dmVMath::Vector3(1.0f), &spawned));
    ASSERT_EQ(dmGameObject::INVALID_GAME_OBJECT, spawned);
    ASSERT_EQ(dmGameObject::RESULT_INVALID_INSTANCE, dmGameObject::GetComponentIndex(invalid_collection, invalid_generation, 0, &component_index));
    ASSERT_EQ(0U, component_index);
    ASSERT_EQ(dmGameObject::RESULT_INVALID_INSTANCE, dmGameObject::GetComponentId(invalid_collection, invalid_generation, 0, &component_id));
    ASSERT_EQ(0U, component_id);
    ASSERT_EQ(dmGameObject::RESULT_INVALID_INSTANCE, dmGameObject::GetComponent(invalid_collection, invalid_generation, 0, &component_type, &component, &world));
    ASSERT_EQ(0U, component_type);
    ASSERT_EQ((dmGameObject::HComponent)0, component);
    ASSERT_EQ((dmGameObject::HComponentWorld)0, world);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_INVALID_INSTANCE, dmGameObject::GetProperty(invalid_collection, invalid_generation, 0, 0, property_options, property_desc));
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_INVALID_INSTANCE, dmGameObject::SetProperty(invalid_collection, invalid_generation, 0, 0, property_options, dmGameObject::PropertyVar(1.0f)));
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_INVALID_INSTANCE, dmGameObject::CancelAnimations(invalid_collection, invalid_generation, 0, 0));
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_INVALID_INSTANCE, dmGameObject::CancelAnimations(m_Collection, invalid_generation, 0, 1));
    ASSERT_EQ(dmGameObject::RESULT_INVALID_INSTANCE, dmGameObject::SetParent(invalid_collection, invalid_generation, dmGameObject::INVALID_GAME_OBJECT));
    ASSERT_EQ(dmGameObject::INVALID_GAME_OBJECT, dmGameObject::GetParent(invalid_collection, invalid_generation));
    ASSERT_EQ(0xFFFFFFFFU, dmGameObject::GetComponentTypeIndex(invalid_collection, 0));
    ASSERT_EQ((dmGameObject::HComponentWorld)0, dmGameObject::GetWorld(invalid_collection, 0));
    ASSERT_EQ((void*)0, dmGameObject::GetContext(invalid_collection, 0));
    ASSERT_EQ((dmResource::HFactory)0, dmGameObject::GetFactory(invalid_collection));
    ASSERT_EQ((dmGameObject::HContext)0, dmGameObject::GetGameObjectContext(invalid_collection));
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
    ASSERT_EQ((dmGameObject::HContext)0, dmGameObject::GetGameObjectContext(zero_generation_collection));
    ASSERT_EQ((dmGameObject::HContext)0, dmGameObject::GetGameObjectContext(sentinel_index_collection));
    ASSERT_EQ((dmResource::HFactory)0, dmGameObject::GetFactory(zero_generation_collection));
    ASSERT_EQ((dmResource::HFactory)0, dmGameObject::GetFactory(sentinel_index_collection));
    ASSERT_FALSE(dmGameObject::IsValid(zero_generation_collection, invalid_generation));
    ASSERT_FALSE(dmGameObject::IsValid(sentinel_index_collection, invalid_generation));
}

TEST_F(IdTest, TestSceneTraversalRejectsStaleHandles)
{
    dmGameObject::HGameObject parent = dmGameObject::New(m_Collection, 0);
    dmGameObject::HGameObject child = dmGameObject::New(m_Collection, 0);
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetParent(m_Collection, child, parent));

    dmGameObject::SceneNode parent_node = {};
    parent_node.m_Type = dmGameObject::SCENE_NODE_TYPE_GAMEOBJECT;
    parent_node.m_Collection = m_Collection;
    parent_node.m_Instance = parent;
    dmGameObject::SceneNodeIterator children = dmGameObject::TraverseIterateChildren(&parent_node);

    dmGameObject::Delete(m_Collection, child, false);
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    ASSERT_FALSE(dmGameObject::TraverseIterateNext(&children));

    dmGameObject::SceneNode component_node = {};
    component_node.m_Type = dmGameObject::SCENE_NODE_TYPE_COMPONENT;
    component_node.m_Collection = m_Collection;
    component_node.m_Instance = child;
    component_node.m_ComponentType = (dmGameObject::ComponentType*)1;
    component_node.m_ComponentPrototype = (void*)1;
    component_node.m_ComponentWorld = (void*)1;
    component_node.m_Component = 1;

    dmGameObject::SceneNodeIterator component_children = dmGameObject::TraverseIterateChildren(&component_node);
    ASSERT_FALSE(dmGameObject::TraverseIterateNext(&component_children));
    dmGameObject::SceneNodePropertyIterator component_properties = dmGameObject::TraverseIterateProperties(&component_node);
    ASSERT_FALSE(dmGameObject::TraverseIteratePropertiesNext(&component_properties));

    dmGameObject::HCollection other_collection = dmGameObject::NewCollection("scene_stale_other", m_Factory, m_Register, 1, 0);
    ASSERT_NE(dmGameObject::INVALID_COLLECTION, other_collection);
    dmGameObject::HGameObject other_object = dmGameObject::New(other_collection, 0);
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
    const uint32_t object_count = 65537;
    dmGameObject::HCollection collection = dmGameObject::NewCollection("large", m_Factory, m_Register, object_count, 0);
    ASSERT_NE(dmGameObject::INVALID_COLLECTION, collection);

    dmGameObject::HGameObject first = dmGameObject::INVALID_GAME_OBJECT;
    dmGameObject::HGameObject low = dmGameObject::INVALID_GAME_OBJECT;
    dmGameObject::HGameObject boundary = dmGameObject::INVALID_GAME_OBJECT;
    dmGameObject::HGameObject last = dmGameObject::INVALID_GAME_OBJECT;
    dmhash_t first_id = dmHashString64("large-first");
    for (uint32_t i = 0; i < object_count; ++i)
    {
        uint32_t identifier_index = dmGameObject::AcquireInstanceIndex(collection);
        ASSERT_EQ(i, identifier_index);
        dmGameObject::HGameObject game_object = dmGameObject::New(collection, 0);
        ASSERT_NE(dmGameObject::INVALID_GAME_OBJECT, game_object);
        dmGameObject::AssignInstanceIndex(collection, identifier_index, game_object);
        if (i == 0)
        {
            first = game_object;
            ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(collection, first, first_id));
        }
        if (i == 1)
            low = game_object;
        if (i == 65535)
            boundary = game_object;
        if (i == object_count - 1)
            last = game_object;
    }
    ASSERT_EQ(dmGameObject::INVALID_INSTANCE_POOL_INDEX, dmGameObject::AcquireInstanceIndex(collection));

    ASSERT_EQ(65535U, (uint32_t)boundary);
    ASSERT_EQ(65536U, (uint32_t)last);
    ASSERT_EQ(first, dmGameObject::GetGameObjectFromIdentifier(collection, first_id));

    // Collection initialization must scan allocated slots, not just the live
    // count. Freeing a low slot must not leave a live high-index object uninitialized.
    dmGameObject::Delete(collection, low, false);
    ASSERT_TRUE(dmGameObject::PostUpdate(collection));
    ASSERT_FALSE(dmGameObject::IsValid(collection, low));
    ASSERT_TRUE(dmGameObject::Init(collection));
    dmGameObject::Collection* collection_ptr = dmGameObject::GetCollectionFromHandle(collection);
    dmGameObject::Instance* last_instance = dmGameObject::GetGameObjectFromHandle(collection_ptr, last);
    ASSERT_NE((dmGameObject::Instance*)0, last_instance);
    ASSERT_TRUE(last_instance->m_Initialized);

    dmhash_t last_id = dmHashString64("large-last");
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(collection, last, last_id));
    ASSERT_EQ(last, dmGameObject::GetGameObjectFromIdentifier(collection, last_id));

    dmGameObject::SetPosition(collection, last, dmVMath::Point3(1.0f, 2.0f, 3.0f));
    ASSERT_EQ(3.0f, dmGameObject::GetPosition(collection, last).getZ());
    dmGameObject::SetPosition(collection, first, dmVMath::Point3(10.0f, 0.0f, 0.0f));
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetParent(collection, last, first));
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetParent(collection, boundary, first));
    ASSERT_EQ(first, dmGameObject::GetParent(collection, last));
    ASSERT_EQ(first, dmGameObject::GetParent(collection, boundary));
    ASSERT_EQ(2U, dmGameObject::GetChildCount(collection, first));

    dmGameObject::UpdateTransforms(collection);
    ASSERT_EQ(11.0f, dmGameObject::GetWorldPosition(collection, last).getX());

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
    ASSERT_EQ(1U, dmGameObject::GetChildCount(collection, first));
    ASSERT_EQ(first, dmGameObject::GetParent(collection, boundary));
    uint32_t reused_identifier_index = dmGameObject::AcquireInstanceIndex(collection);
    ASSERT_EQ(65536U, reused_identifier_index);

    dmGameObject::HPrototype prototype = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/go.goc", (void**)&prototype));
    dmGameObject::HGameObject reused = dmGameObject::INVALID_GAME_OBJECT;
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::Spawn(collection, prototype, "/go.goc",
            dmHashString64("/large-reused"), 0, dmVMath::Point3(), dmVMath::Quat::identity(),
            dmVMath::Vector3(1.0f), &reused));
    dmResource::Release(m_Factory, prototype);
    dmGameObject::AssignInstanceIndex(collection, reused_identifier_index, reused);
    ASSERT_EQ(65536U, (uint32_t)reused);
    ASSERT_NE(last, reused);
    ASSERT_EQ(1U, dmGameObject::GetAddToUpdateCount(collection));
    ASSERT_TRUE(dmGameObject::Update(collection, &m_UpdateContext));
    ASSERT_EQ(0U, dmGameObject::GetAddToUpdateCount(collection));
    ASSERT_FALSE(dmGameObject::IsValid(collection, last));
    ASSERT_TRUE(dmGameObject::IsValid(collection, boundary));
    ASSERT_TRUE(dmGameObject::IsValid(collection, reused));

    dmGameObject::DeleteCollection(collection);
    dmGameObject::PostUpdate(m_Register);
}
