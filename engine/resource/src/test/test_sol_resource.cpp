#include <gtest/gtest.h>

#include <dlib/socket.h>
#include <dlib/hash.h>
#include <dlib/dstrings.h>
#include <dlib/time.h>
#include <dlib/message.h>
#include <dlib/thread.h>
#include <dlib/sol.h>

#include <ddf/ddf.h>
#include "resource_ddf.h"
#include "../resource.h"
#include "test/test_resource_ddf.h"

extern unsigned char TEST_ARC[];
extern uint32_t TEST_ARC_SIZE;

#include <dlib/sol.h>

extern "C"
{
    dmResource::Result test_foo_create(void*);
    dmResource::Result test_foo_destroy(void*);
    dmResource::Result test_foo_recreate(void*);
    dmResource::Result test_cont_create(void*);
    dmResource::Result test_cont_destroy(void*);
}

class SolResourceTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        const char* test_dir = "build/default/src/test";

        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;
        params.m_BuiltinsArchive = (const void*) TEST_ARC;
        params.m_BuiltinsArchiveSize = TEST_ARC_SIZE;

        factory = dmResource::NewFactory(&params, test_dir);
        ASSERT_NE((void*) 0, factory);

        dmResource::SolResourceFns fns;
        memset(&fns, 0x00, sizeof(fns));

        fns.m_Create = &test_foo_create;
        fns.m_Destroy = &test_foo_destroy;
        fns.m_Recreate = &test_foo_recreate;
        dmResource::RegisterTypeSol(factory, "foo", 0, fns);

        fns.m_Create = &test_cont_create;
        fns.m_Destroy = &test_cont_destroy;
        dmResource::RegisterTypeSol(factory, "cont", 0, fns);
    }

    virtual void TearDown()
    {
        dmResource::DeleteFactory(factory);
    }

    dmResource::Result PreloaderGet(dmResource::HFactory factory, const char *ref, void** resource)
    {
        dmResource::HPreloader pr = dmResource::NewPreloader(factory, ref);
        dmResource::Result r;
        for (uint32_t i=0;i<33;i++)
        {
            r = dmResource::UpdatePreloader(pr, 30*1000);
            if (r != dmResource::RESULT_PENDING)
                break;
            dmTime::Sleep(30000);
        }

        if (r == dmResource::RESULT_OK)
        {
            r = dmResource::Get(factory, ref, resource);
        }
        else
        {
            *resource = 0x0;
        }

        dmResource::DeletePreloader(pr);
        return r;
    }

    dmResource::HFactory factory;
};

struct TestResourceContainer
{

};

TEST_F(SolResourceTest, TestFooGet)
{
    void* resource = 0;
    dmResource::Result r = dmResource::Get(factory, "/test01.foo", &resource);
    ASSERT_EQ(dmResource::RESULT_OK, r);
    ASSERT_NE((void*)0, resource);
    dmResource::Release(factory, resource);
}

TEST_F(SolResourceTest, TestFooReload)
{
    void* resource = 0;
    dmResource::Result r = dmResource::Get(factory, "/test01.foo", &resource);
    ASSERT_EQ(dmResource::RESULT_OK, r);
    ASSERT_NE((void*)0, resource);

    r = dmResource::ReloadResource(factory, "/test01.foo", 0);
    ASSERT_EQ(dmResource::RESULT_OK, r);

    dmResource::Release(factory, resource);
}

TEST_F(SolResourceTest, TestGetSolAny)
{
    void* resource = 0;
    dmResource::Result r = dmResource::Get(factory, "/test01.foo", &resource);
    ASSERT_EQ(dmResource::RESULT_OK, r);
    ASSERT_NE((void*)0, resource);

    ::Any ptr;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::GetSolAnyPtr(factory, resource, &ptr));

    dmResource::Release(factory, resource);
}

TEST_F(SolResourceTest, TestContGet)
{
    void* resource = 0;
    dmResource::Result r = dmResource::Get(factory, "/test.cont", &resource);
    ASSERT_EQ(dmResource::RESULT_OK, r);
    ASSERT_NE((void*)0, resource);
    dmResource::Release(factory, resource);
}

TEST_F(SolResourceTest, SelfReferring)
{
    dmResource::Result e;
    TestResourceContainer* test_resource_cont;

    test_resource_cont = 0;
    e = dmResource::Get(factory, "/self_referring.cont", (void**) &test_resource_cont);
    ASSERT_EQ(dmResource::RESULT_RESOURCE_LOOP_ERROR, e);
    ASSERT_EQ((void*) 0, test_resource_cont);

    test_resource_cont = 0;
    e = dmResource::Get(factory, "/self_referring.cont", (void**) &test_resource_cont);
    ASSERT_EQ(dmResource::RESULT_RESOURCE_LOOP_ERROR, e);
    ASSERT_EQ((void*) 0, test_resource_cont);

    test_resource_cont = 0;
    e = PreloaderGet(factory, "/self_referring.cont", (void**) &test_resource_cont);
    ASSERT_EQ(dmResource::RESULT_RESOURCE_LOOP_ERROR, e);
    ASSERT_EQ((void*) 0, test_resource_cont);
}

TEST_F(SolResourceTest, Loop)
{
    dmResource::Result e;
    TestResourceContainer* test_resource_cont;

    test_resource_cont = 0;
    e = dmResource::Get(factory, "/root_loop.cont", (void**) &test_resource_cont);
    ASSERT_EQ(dmResource::RESULT_RESOURCE_LOOP_ERROR, e);
    ASSERT_EQ((void*) 0, test_resource_cont);

    test_resource_cont = 0;
    e = dmResource::Get(factory, "/root_loop.cont", (void**) &test_resource_cont);
    ASSERT_EQ(dmResource::RESULT_RESOURCE_LOOP_ERROR, e);
    ASSERT_EQ((void*) 0, test_resource_cont);

    e = PreloaderGet(factory, "/root_loop.cont", (void**) &test_resource_cont);
    ASSERT_EQ(dmResource::RESULT_RESOURCE_LOOP_ERROR, e);
    ASSERT_EQ((void*) 0, test_resource_cont);
}


int main(int argc, char **argv)
{
    dmSol::Initialize();
    dmSocket::Initialize();
    dmDDF::RegisterAllTypes();
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    dmSocket::Finalize();
    dmSol::FinalizeWithCheck();
    return ret;
}
