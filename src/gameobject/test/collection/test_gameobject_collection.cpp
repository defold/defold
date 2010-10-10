#include <gtest/gtest.h>

#include <stdint.h>

#include <dlib/hash.h>
#include <dlib/log.h>

#include "../gameobject.h"

class CollectionTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        dmGameObject::Initialize();

        m_UpdateContext.m_DT = 1.0f / 60.0f;

        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_EMPTY;
        m_Factory = dmResource::NewFactory(&params, "build/default/src/gameobject/test/collection");
        m_Register = dmGameObject::NewRegister(0, 0);
        dmGameObject::RegisterResourceTypes(m_Factory, m_Register);
        dmGameObject::RegisterComponentTypes(m_Factory, m_Register);
        m_Collection = dmGameObject::NewCollection(m_Factory, m_Register, 1024);
    }

    virtual void TearDown()
    {
        dmGameObject::DeleteCollection(m_Collection);
        dmResource::DeleteFactory(m_Factory);
        dmGameObject::DeleteRegister(m_Register);
        dmGameObject::Finalize();
    }

public:
    dmGameObject::UpdateContext m_UpdateContext;
    dmGameObject::HRegister m_Register;
    dmGameObject::HCollection m_Collection;
    dmResource::HFactory m_Factory;
};

TEST_F(CollectionTest, Collection)
{
    for (int i = 0; i < 10; ++i)
    {
        // NOTE: Coll is local and not m_Collection in CollectionTest
        dmGameObject::HCollection coll;
        dmResource::FactoryResult r = dmResource::Get(m_Factory, "test.collectionc", (void**) &coll);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, r);
        ASSERT_NE((void*) 0, coll);

        uint32_t go01ident = dmHashString32("go1");
        dmGameObject::HInstance go01 = dmGameObject::GetInstanceFromIdentifier(coll, go01ident);
        ASSERT_NE((void*) 0, go01);

        uint32_t go02ident = dmHashString32("go2");
        dmGameObject::HInstance go02 = dmGameObject::GetInstanceFromIdentifier(coll, go02ident);
        ASSERT_NE((void*) 0, go02);

        dmGameObject::Init(coll);
        dmGameObject::Update(&coll, 0, 1);

        ASSERT_NE(go01, go02);

        dmResource::Release(m_Factory, (void*) coll);
    }
}

TEST_F(CollectionTest, PostCollection)
{
    for (int i = 0; i < 10; ++i)
    {
        dmResource::FactoryResult r;
        dmGameObject::HCollection coll1;
        r = dmResource::Get(m_Factory, "post1.collectionc", (void**) &coll1);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, r);
        ASSERT_NE((void*) 0, coll1);

        dmGameObject::HCollection coll2;
        r = dmResource::Get(m_Factory, "post2.collectionc", (void**) &coll2);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, r);
        ASSERT_NE((void*) 0, coll2);

        bool ret;
        dmGameObject::Init(coll1);
        dmGameObject::Init(coll2);

        dmGameObject::HCollection collections[2] = {coll1, coll2};
        ret = dmGameObject::Update(collections, 0, 2);
        ASSERT_TRUE(ret);

        dmResource::Release(m_Factory, (void*) coll1);
        dmResource::Release(m_Factory, (void*) coll2);
    }
}

TEST_F(CollectionTest, CollectionFail)
{
    dmLogSetlevel(DM_LOG_SEVERITY_FATAL);
    for (int i = 0; i < 10; ++i)
    {
        // NOTE: Coll is local and not collection in CollectionTest
        dmGameObject::HCollection coll;
        dmResource::FactoryResult r = dmResource::Get(m_Factory, "failing_sub.collectionc", (void**) &coll);
        ASSERT_NE(dmResource::FACTORY_RESULT_OK, r);
    }
    dmLogSetlevel(DM_LOG_SEVERITY_WARNING);
}

TEST_F(CollectionTest, CollectionInCollection)
{
    for (int i = 0; i < 10; ++i)
    {
        // NOTE: Coll is local and not collection in CollectionTest
        dmGameObject::HCollection coll;
        dmResource::FactoryResult r = dmResource::Get(m_Factory, "root1.collectionc", (void**) &coll);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, r);
        ASSERT_NE((void*) 0, coll);

        uint32_t go01ident = dmHashString32("go1");
        dmGameObject::HInstance go01 = dmGameObject::GetInstanceFromIdentifier(coll, go01ident);
        ASSERT_NE((void*) 0, go01);
        ASSERT_NEAR(dmGameObject::GetPosition(go01).getX(), 123.0f, 0.0000f);

        uint32_t go02ident = dmHashString32("go2");
        dmGameObject::HInstance go02 = dmGameObject::GetInstanceFromIdentifier(coll, go02ident);
        ASSERT_NE((void*) 0, go02);
        ASSERT_NEAR(dmGameObject::GetPosition(go02).getX(), 456.0f, 0.0000f);

        ASSERT_NE(go01, go02);

        uint32_t go01_sub1_ident = dmHashString32("sub1.go1");
        dmGameObject::HInstance go01_sub1 = dmGameObject::GetInstanceFromIdentifier(coll, go01_sub1_ident);
        ASSERT_NE((void*) 0, go01_sub1);
        ASSERT_NEAR(dmGameObject::GetPosition(go01_sub1).getY(), 1000.0f + 10.0f, 0.0000f);

        uint32_t go02_sub1_ident = dmHashString32("sub1.go2");
        dmGameObject::HInstance go02_sub1 = dmGameObject::GetInstanceFromIdentifier(coll, go02_sub1_ident);
        ASSERT_NE((void*) 0, go02_sub1);
        ASSERT_NEAR(dmGameObject::GetPosition(go02_sub1).getY(), 1000.0f + 20.0f, 0.0000f);

        uint32_t go01_sub2_ident = dmHashString32("sub2.go1");
        dmGameObject::HInstance go01_sub2 = dmGameObject::GetInstanceFromIdentifier(coll, go01_sub2_ident);
        ASSERT_NE((void*) 0, go01_sub2);
        ASSERT_NEAR(dmGameObject::GetPosition(go01_sub2).getY(), 1000.0f + 10.0f, 0.0000f);

        uint32_t go02_sub2_ident = dmHashString32("sub2.go2");
        dmGameObject::HInstance go02_sub2 = dmGameObject::GetInstanceFromIdentifier(coll, go02_sub2_ident);
        ASSERT_NE((void*) 0, go02_sub2);
        ASSERT_NEAR(dmGameObject::GetPosition(go02_sub2).getY(), 1000.0f + 20.0f, 0.0000f);

        // Relative identifiers
        ASSERT_EQ(dmHashString32("a"), dmGameObject::GetAbsoluteIdentifier(go01, "a"));
        ASSERT_EQ(dmHashString32("a"), dmGameObject::GetAbsoluteIdentifier(go02, "a"));
        ASSERT_EQ(dmHashString32("sub1.a"), dmGameObject::GetAbsoluteIdentifier(go01_sub1, "a"));
        ASSERT_EQ(dmHashString32("sub2.a"), dmGameObject::GetAbsoluteIdentifier(go01_sub2, "a"));

        bool ret = dmGameObject::Update(&coll, 0, 1);
        ASSERT_TRUE(ret);

        dmResource::Release(m_Factory, (void*) coll);
    }
}

TEST_F(CollectionTest, CollectionInCollectionChildFail)
{
    dmLogSetlevel(DM_LOG_SEVERITY_FATAL);
    for (int i = 0; i < 10; ++i)
    {
        // NOTE: Coll is local and not collection in CollectionTest
        dmGameObject::HCollection coll;
        dmResource::FactoryResult r = dmResource::Get(m_Factory, "root2.collectionc", (void**) &coll);
        ASSERT_NE(dmResource::FACTORY_RESULT_OK, r);
    }
    dmLogSetlevel(DM_LOG_SEVERITY_WARNING);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
