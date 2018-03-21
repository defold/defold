#include <gtest/gtest.h>

#include <resource/resource.h>

#include "../gameobject.h"

class IdTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        m_UpdateContext.m_DT = 1.0f / 60.0f;

        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_EMPTY;
        m_Factory = dmResource::NewFactory(&params, "build/default/src/gameobject/test/id");
        m_ScriptContext = dmScript::NewContext(0, 0, true);
        dmScript::Initialize(m_ScriptContext);
        m_Register = dmGameObject::NewRegister();
        dmGameObject::Initialize(m_Register, m_ScriptContext);
        dmGameObject::RegisterResourceTypes(m_Factory, m_Register, m_ScriptContext, &m_ModuleContext);
        dmGameObject::RegisterComponentTypes(m_Factory, m_Register, m_ScriptContext);
        m_Collection = dmGameObject::NewCollection("collection", m_Factory, m_Register, 1024);
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

public:
    dmScript::HContext m_ScriptContext;
    dmGameObject::UpdateContext m_UpdateContext;
    dmGameObject::HRegister m_Register;
    dmGameObject::HCollection m_Collection;
    dmResource::HFactory m_Factory;
    dmGameObject::ModuleContext m_ModuleContext;
};

TEST_F(IdTest, TestIdentifier)
{
    dmGameObject::HInstance go1 = dmGameObject::New(m_Collection, "/go.goc");
    dmGameObject::HInstance go2 = dmGameObject::New(m_Collection, "/go.goc");
    ASSERT_NE((void*) 0, (void*) go1);
    ASSERT_NE((void*) 0, (void*) go2);

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
    dmGameObject::HCollection collection;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/root.collectionc", (void**)&collection));
    dmhash_t id = dmHashString64("/go");
    dmhash_t sub1_id = dmHashString64("/sub/go1");
    dmhash_t sub2_id = dmHashString64("/sub/go2");
    dmGameObject::HInstance instance = dmGameObject::GetInstanceFromIdentifier(collection, id);
    ASSERT_NE((void*)0, (void*)instance);
    dmGameObject::HInstance sub1_instance = dmGameObject::GetInstanceFromIdentifier(collection, sub1_id);
    ASSERT_NE((void*)0, (void*)sub1_instance);
    dmGameObject::HInstance sub2_instance = dmGameObject::GetInstanceFromIdentifier(collection, sub2_id);
    ASSERT_NE((void*)0, (void*)sub2_instance);
    ASSERT_EQ(sub1_id, dmGameObject::GetAbsoluteIdentifier(instance, "sub/go1", strlen("sub/go1")));
    ASSERT_EQ(id, dmGameObject::GetAbsoluteIdentifier(sub1_instance, "/go", strlen("/go")));
    ASSERT_EQ(sub2_id, dmGameObject::GetAbsoluteIdentifier(sub1_instance, "go2", strlen("go2")));
    ASSERT_EQ(id, dmGameObject::GetAbsoluteIdentifier(sub2_instance, "/go", strlen("/go")));
    dmResource::Release(m_Factory, collection);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
