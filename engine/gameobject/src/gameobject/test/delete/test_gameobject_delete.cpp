#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <vector>

#include <resource/resource.h>

#include "../gameobject.h"
#include "../gameobject_private.h"
#include "gameobject/test/delete/test_gameobject_delete_ddf.h"

class DeleteTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        m_UpdateContext.m_DT = 1.0f / 60.0f;

        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_EMPTY;
        m_Factory = dmResource::NewFactory(&params, "build/default/src/gameobject/test/delete");
        m_ScriptContext = dmScript::NewContext(0, 0, true);
        dmScript::Initialize(m_ScriptContext);
        m_Register = dmGameObject::NewRegister();
        dmGameObject::Initialize(m_Register, m_ScriptContext);
        dmGameObject::RegisterResourceTypes(m_Factory, m_Register, m_ScriptContext, &m_ModuleContext);
        dmGameObject::RegisterComponentTypes(m_Factory, m_Register, m_ScriptContext);
        m_Collection = dmGameObject::NewCollection("collection", m_Factory, m_Register, 1024);

        dmResource::Result e;
        e = dmResource::RegisterType(m_Factory, "deleteself", this, 0, ResDeleteSelfCreate, 0, ResDeleteSelfDestroy, 0, 0);
        ASSERT_EQ(dmResource::RESULT_OK, e);

        dmResource::ResourceType resource_type;
        e = dmResource::GetTypeFromExtension(m_Factory, "deleteself", &resource_type);
        ASSERT_EQ(dmResource::RESULT_OK, e);
        dmGameObject::ComponentType ds_type;
        ds_type.m_Name = "deleteself";
        ds_type.m_ResourceType = resource_type;
        ds_type.m_Context = this;
        ds_type.m_AddToUpdateFunction = DeleteSelfComponentAddToUpdate;
        ds_type.m_UpdateFunction = DeleteSelfComponentsUpdate;
        dmGameObject::Result result = dmGameObject::RegisterComponentType(m_Register, ds_type);
        ASSERT_EQ(dmGameObject::RESULT_OK, result);
    }

    virtual void TearDown()
    {
        dmGameObject::DeleteCollection(m_Collection);
        dmGameObject::PostUpdate(m_Register);
        dmScript::Finalize(m_ScriptContext);
        dmScript::DeleteContext(m_ScriptContext);
        dmResource::DeleteFactory(m_Factory);
        dmGameObject::DeleteRegister(m_Register);
    }

    static dmResource::Result ResDeleteSelfCreate(const dmResource::ResourceCreateParams& params);
    static dmResource::Result ResDeleteSelfDestroy(const dmResource::ResourceDestroyParams& params);

    static dmGameObject::CreateResult     DeleteSelfComponentAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params);
    static dmGameObject::UpdateResult     DeleteSelfComponentsUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result);

public:

    std::map<uint64_t, uint32_t> m_CreateCountMap;
    std::map<uint64_t, uint32_t> m_DestroyCountMap;

    // Data DeleteSelf test
    std::vector<dmGameObject::HInstance> m_SelfInstancesToDelete;
    std::vector<dmGameObject::HInstance> m_DeleteSelfInstances;
    std::vector<int> m_DeleteSelfIndices;
    std::map<int, dmGameObject::HInstance> m_DeleteSelfIndexToInstance;

    dmScript::HContext m_ScriptContext;
    dmGameObject::UpdateContext m_UpdateContext;
    dmGameObject::HRegister m_Register;
    dmGameObject::HCollection m_Collection;
    dmResource::HFactory m_Factory;
    dmGameObject::ModuleContext m_ModuleContext;
};

dmResource::Result DeleteTest::ResDeleteSelfCreate(const dmResource::ResourceCreateParams& params)
{
    DeleteTest* game_object_test = (DeleteTest*) params.m_Context;
    game_object_test->m_CreateCountMap[TestGameObjectDDF::DeleteSelfResource::m_DDFHash]++;

    TestGameObjectDDF::DeleteSelfResource* obj;
    dmDDF::Result e = dmDDF::LoadMessage<TestGameObjectDDF::DeleteSelfResource>(params.m_Buffer, params.m_BufferSize, &obj);
    if (e == dmDDF::RESULT_OK)
    {
        params.m_Resource->m_Resource = (void*) obj;
        return dmResource::RESULT_OK;
    }
    else
    {
        return dmResource::RESULT_FORMAT_ERROR;
    }
}

dmResource::Result DeleteTest::ResDeleteSelfDestroy(const dmResource::ResourceDestroyParams& params)
{
    DeleteTest* game_object_test = (DeleteTest*) params.m_Context;
    game_object_test->m_DestroyCountMap[TestGameObjectDDF::DeleteSelfResource::m_DDFHash]++;

    dmDDF::FreeMessage((void*) params.m_Resource->m_Resource);
    return dmResource::RESULT_OK;
}

dmGameObject::CreateResult DeleteTest::DeleteSelfComponentAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params)
{
    return dmGameObject::CREATE_RESULT_OK;
}

dmGameObject::UpdateResult DeleteTest::DeleteSelfComponentsUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
{
    DeleteTest* game_object_test = (DeleteTest*) params.m_Context;

    for (uint32_t i = 0; i < game_object_test->m_SelfInstancesToDelete.size(); ++i)
    {
        dmGameObject::Delete(game_object_test->m_Collection, game_object_test->m_SelfInstancesToDelete[i], false);
        // Test "double delete"
        dmGameObject::Delete(game_object_test->m_Collection, game_object_test->m_SelfInstancesToDelete[i], false);
    }

    for (uint32_t i = 0; i < game_object_test->m_DeleteSelfIndices.size(); ++i)
    {
        int index = game_object_test->m_DeleteSelfIndices[i];

        dmGameObject::HInstance go = game_object_test->m_DeleteSelfIndexToInstance[index];
        if (index != (int)dmGameObject::GetPosition(go).getX())
            return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
    }

    return dmGameObject::UPDATE_RESULT_OK;
}

TEST_F(DeleteTest, AutoDelete)
{
    for (int i = 0; i < 512; ++i)
    {
        dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/go.goc");
        ASSERT_NE((void*) 0, (void*) go);
    }
}

TEST_F(DeleteTest, DeleteSelf)
{
    /*
     * NOTE: We do not have any .deleteself resources on disk even though we register the type
     * Component instances of type 'A' is used here. We need a specific ComponentUpdate though. (DeleteSelfComponentsUpdate)
     * See New(..., goproto01.goc") below.
     */
    for (int iter = 0; iter < 4; ++iter)
    {
        m_DeleteSelfInstances.clear();
        m_DeleteSelfIndexToInstance.clear();

        for (int i = 0; i < 512; ++i)
        {
            dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/go.goc");
            dmGameObject::SetPosition(go, Vectormath::Aos::Point3((float) i,(float) i, (float) i));
            ASSERT_NE((void*) 0, (void*) go);
            m_DeleteSelfInstances.push_back(go);
            m_DeleteSelfIndexToInstance[i] = go;
            m_DeleteSelfIndices.push_back(i);
        }

        std::random_shuffle(m_DeleteSelfIndices.begin(), m_DeleteSelfIndices.end());

        while (m_DeleteSelfIndices.size() > 0)
        {
            for (int i = 0; i < 16; ++i)
            {
                int index = *(m_DeleteSelfIndices.end() - i - 1);
                m_SelfInstancesToDelete.push_back(m_DeleteSelfIndexToInstance[index]);
            }
            bool ret = dmGameObject::Update(m_Collection, &m_UpdateContext);
            ASSERT_TRUE(ret);
            ret = dmGameObject::PostUpdate(m_Collection);
            ASSERT_TRUE(ret);
            for (int i = 0; i < 16; ++i)
            {
                m_DeleteSelfIndices.pop_back();
            }

            m_SelfInstancesToDelete.clear();
        }
    }
}

TEST_F(DeleteTest, TestScriptDelete)
{
    dmGameObject::HInstance instance = dmGameObject::New(m_Collection, "/delete.goc");
    ASSERT_NE((void*)0, (void*)instance);
    ASSERT_NE(0, m_Collection->m_InstanceIndices.Size());
    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    ASSERT_EQ(0, m_Collection->m_InstanceIndices.Size());
}

TEST_F(DeleteTest, TestScriptDeleteMultiple)
{
    dmGameObject::HInstance instance = dmGameObject::New(m_Collection, "/delete_multiple.goc");
    ASSERT_NE((void*)0, (void*)instance);
    instance = dmGameObject::New(m_Collection, "/go.goc");
    dmGameObject::SetIdentifier(m_Collection, instance, "test_id_1");
    ASSERT_NE((void*)0, (void*)instance);
    instance = dmGameObject::New(m_Collection, "/go.goc");
    dmGameObject::SetIdentifier(m_Collection, instance, "test_id_2");
    ASSERT_NE((void*)0, (void*)instance);

    ASSERT_EQ(3, m_Collection->m_InstanceIndices.Size());
    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    ASSERT_EQ(1, m_Collection->m_InstanceIndices.Size());
}

TEST_F(DeleteTest, TestScriptDeleteOther)
{
    dmGameObject::HInstance instance = dmGameObject::New(m_Collection, "/delete_other.goc");
    ASSERT_NE((void*)0, (void*)instance);
    instance = dmGameObject::New(m_Collection, "/go.goc");
    dmGameObject::SetIdentifier(m_Collection, instance, "test_id");
    ASSERT_NE((void*)0, (void*)instance);

    ASSERT_NE(1, m_Collection->m_InstanceIndices.Size());
    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    ASSERT_EQ(1, m_Collection->m_InstanceIndices.Size());
}

TEST_F(DeleteTest, TestScriptDeleteRecursive)
{
    dmGameObject::HInstance parent_instance = dmGameObject::New(m_Collection, "/delete_recursive.goc");
    ASSERT_NE((void*)0, (void*)parent_instance);
    dmGameObject::HInstance instance = dmGameObject::New(m_Collection, "/go.goc");
    ASSERT_NE((void*)0, (void*)instance);
    dmGameObject::SetIdentifier(m_Collection, instance, "child_id");
    dmGameObject::SetParent(instance, parent_instance);

    ASSERT_EQ(2, m_Collection->m_InstanceIndices.Size());
    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    ASSERT_EQ(0, m_Collection->m_InstanceIndices.Size());
}

TEST_F(DeleteTest, TestScriptDeleteMultipleRecursive)
{
    dmGameObject::HInstance instance = dmGameObject::New(m_Collection, "/delete_multiple_recursive.goc");
    ASSERT_NE((void*)0, (void*)instance);

    dmGameObject::HInstance child_instance[2];
    child_instance[0] = dmGameObject::New(m_Collection, "/go.goc");
    ASSERT_NE((void*)0, (void*)child_instance[0]);
    dmGameObject::SetIdentifier(m_Collection, child_instance[0], "child_id_1");
    child_instance[1] = dmGameObject::New(m_Collection, "/go.goc");
    ASSERT_NE((void*)0, (void*)child_instance[1]);
    dmGameObject::SetIdentifier(m_Collection, child_instance[1], "child_id_2");

    instance = dmGameObject::New(m_Collection, "/go.goc");
    ASSERT_NE((void*)0, (void*)instance);
    dmGameObject::SetIdentifier(m_Collection, instance, "test_id_1");
    dmGameObject::SetParent(child_instance[0], instance);
    instance = dmGameObject::New(m_Collection, "/go.goc");
    ASSERT_NE((void*)0, (void*)instance);
    dmGameObject::SetIdentifier(m_Collection, instance, "test_id_2");
    dmGameObject::SetParent(child_instance[1], instance);

    ASSERT_EQ(5, m_Collection->m_InstanceIndices.Size());
    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    ASSERT_EQ(1, m_Collection->m_InstanceIndices.Size());
}

TEST_F(DeleteTest, TestScriptDeleteOtherRecursive)
{
    dmGameObject::HInstance instance = dmGameObject::New(m_Collection, "/delete_other_recursive.goc");
    ASSERT_NE((void*)0, (void*)instance);
    dmGameObject::HInstance parent_instance = dmGameObject::New(m_Collection, "/go.goc");
    dmGameObject::SetIdentifier(m_Collection, parent_instance, "test_id");
    ASSERT_NE((void*)0, (void*)parent_instance);

    instance = dmGameObject::New(m_Collection, "/go.goc");
    ASSERT_NE((void*)0, (void*)instance);
    dmGameObject::SetIdentifier(m_Collection, instance, "child_id");
    dmGameObject::SetParent(instance, parent_instance);

    ASSERT_EQ(3, m_Collection->m_InstanceIndices.Size());
    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    ASSERT_EQ(1, m_Collection->m_InstanceIndices.Size());
}

TEST_F(DeleteTest, TestScriptDeleteRecursiveOrder)
{
    dmHashEnableReverseHash(true);
    dmGameObject::HInstance instance = dmGameObject::New(m_Collection, "/delete_recursive_order.goc");
    ASSERT_NE((void*)0, (void*)instance);
    dmGameObject::SetIdentifier(m_Collection, instance, "root");

    dmGameObject::HInstance parent_instance = dmGameObject::New(m_Collection, "/delete_recursive_order_child.goc");
    ASSERT_NE((void*)0, (void*)parent_instance);
    dmGameObject::SetIdentifier(m_Collection, parent_instance, "parent");

    dmGameObject::HInstance child_instance_1 = dmGameObject::New(m_Collection, "/delete_recursive_order_child.goc");
    ASSERT_NE((void*)0, (void*)child_instance_1);
    dmGameObject::SetIdentifier(m_Collection, child_instance_1, "child_1");
    dmGameObject::SetParent(child_instance_1, parent_instance);
    instance = dmGameObject::New(m_Collection, "/delete_recursive_order_child.goc");
    ASSERT_NE((void*)0, (void*)instance);
    dmGameObject::SetIdentifier(m_Collection, instance, "child_1_1");
    dmGameObject::SetParent(instance, child_instance_1);

    dmGameObject::HInstance child_instance_2 = dmGameObject::New(m_Collection, "/delete_recursive_order_child.goc");
    ASSERT_NE((void*)0, (void*)child_instance_2);
    dmGameObject::SetIdentifier(m_Collection, child_instance_2, "child_2");
    dmGameObject::SetParent(child_instance_2, parent_instance);
    instance = dmGameObject::New(m_Collection, "/delete_recursive_order_child.goc");
    ASSERT_NE((void*)0, (void*)instance);
    dmGameObject::SetIdentifier(m_Collection, instance, "child_2_1");
    dmGameObject::SetParent(instance, child_instance_2);

    ASSERT_EQ(6, m_Collection->m_InstanceIndices.Size());
    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    ASSERT_EQ(0, m_Collection->m_InstanceIndices.Size());
}


TEST_F(DeleteTest, TestScriptDeleteAllDeprecated)
{
    dmGameObject::HInstance instance = dmGameObject::New(m_Collection, "/delete_all_deprecated.goc");
    ASSERT_NE((void*)0, (void*)instance);
    instance = dmGameObject::New(m_Collection, "/go.goc");
    dmGameObject::SetIdentifier(m_Collection, instance, "test_id_1");
    ASSERT_NE((void*)0, (void*)instance);
    instance = dmGameObject::New(m_Collection, "/go.goc");
    dmGameObject::SetIdentifier(m_Collection, instance, "test_id_2");
    ASSERT_NE((void*)0, (void*)instance);
    ASSERT_EQ(3, m_Collection->m_InstanceIndices.Size());

    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    ASSERT_EQ(1, m_Collection->m_InstanceIndices.Size());
}

TEST_F(DeleteTest, TestScriptDeleteBone)
{
    dmGameObject::HInstance instance = dmGameObject::New(m_Collection, "/delete_other.goc");
    ASSERT_NE((void*)0, (void*)instance);
    instance = dmGameObject::New(m_Collection, "/go.goc");
    dmGameObject::SetIdentifier(m_Collection, instance, "test_id");
    ASSERT_NE((void*)0, (void*)instance);
    dmGameObject::SetBone(instance, true);
    ASSERT_EQ(2, m_Collection->m_InstanceIndices.Size());

    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    ASSERT_FALSE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    ASSERT_EQ(2, m_Collection->m_InstanceIndices.Size());
}

TEST_F(DeleteTest, TestScriptDeleteAllBone)
{
    dmGameObject::HInstance instance = dmGameObject::New(m_Collection, "/delete_all_deprecated.goc");
    ASSERT_NE((void*)0, (void*)instance);
    instance = dmGameObject::New(m_Collection, "/go.goc");
    dmGameObject::SetIdentifier(m_Collection, instance, "test_id_1");
    ASSERT_NE((void*)0, (void*)instance);
    instance = dmGameObject::New(m_Collection, "/go.goc");
    dmGameObject::SetIdentifier(m_Collection, instance, "test_id_2");
    ASSERT_NE((void*)0, (void*)instance);
    dmGameObject::SetBone(instance, true);

    ASSERT_EQ(3, m_Collection->m_InstanceIndices.Size());
    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    ASSERT_FALSE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    ASSERT_EQ(2, m_Collection->m_InstanceIndices.Size());
}

TEST_F(DeleteTest, TestScriptDeleteNonExistent)
{
    dmGameObject::HInstance instance = dmGameObject::New(m_Collection, "/delete_non_existent.goc");
    ASSERT_NE((void*)0, (void*)instance);
    instance = dmGameObject::New(m_Collection, "/go.goc");
    dmGameObject::SetIdentifier(m_Collection, instance, "test_id");
    ASSERT_NE((void*)0, (void*)instance);

    ASSERT_NE(1, m_Collection->m_InstanceIndices.Size());
    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    ASSERT_FALSE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    ASSERT_EQ(2, m_Collection->m_InstanceIndices.Size());
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
