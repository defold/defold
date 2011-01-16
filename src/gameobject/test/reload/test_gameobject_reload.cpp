#include <gtest/gtest.h>

#include <vectormath/cpp/vectormath_aos.h>

#include <dlib/hash.h>

#include <resource/resource.h>

#include "../gameobject.h"

#include "gameobject/test/reload/test_gameobject_reload_ddf.h"

using namespace Vectormath::Aos;

class ReloadTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        dmGameObject::Initialize();

        m_NewResource = 0x0;

        m_UpdateContext.m_DT = 1.0f / 60.0f;

        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT;
        m_Factory = dmResource::NewFactory(&params, "build/default/src/gameobject/test/reload");
        m_Register = dmGameObject::NewRegister(0, 0);
        dmGameObject::RegisterResourceTypes(m_Factory, m_Register);
        dmGameObject::RegisterComponentTypes(m_Factory, m_Register);
        m_Collection = dmGameObject::NewCollection(m_Factory, m_Register, 1024);

        dmResource::FactoryResult e = dmResource::RegisterType(m_Factory, "rt", this, ResReloadTargetCreate, ResReloadTargetDestroy, ResReloadTargetRecreate);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, e);

        uint32_t resource_type;
        e = dmResource::GetTypeFromExtension(m_Factory, "rt", &resource_type);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, e);
        dmGameObject::ComponentType rt_type;
        rt_type.m_Name = "rt";
        rt_type.m_ResourceType = resource_type;
        rt_type.m_Context = this;
        rt_type.m_CreateFunction = CompReloadTargetCreate;
        rt_type.m_DestroyFunction = CompReloadTargetDestroy;
        rt_type.m_OnReloadFunction = CompReloadTargetOnReload;
        rt_type.m_InstanceHasUserData = true;
        dmGameObject::Result result = dmGameObject::RegisterComponentType(m_Register, rt_type);
        ASSERT_EQ(dmGameObject::RESULT_OK, result);
    }

    virtual void TearDown()
    {
        dmGameObject::DeleteCollection(m_Collection);
        dmResource::DeleteFactory(m_Factory);
        dmGameObject::DeleteRegister(m_Register);
        dmGameObject::Finalize();
    }

    static dmResource::CreateResult ResReloadTargetCreate(dmResource::HFactory factory, void* context, const void* buffer, uint32_t buffer_size, dmResource::SResourceDescriptor* resource, const char* filename);
    static dmResource::CreateResult ResReloadTargetDestroy(dmResource::HFactory factory, void* context, dmResource::SResourceDescriptor* resource);
    static dmResource::CreateResult ResReloadTargetRecreate(dmResource::HFactory factory,
                                              void* context,
                                              const void* buffer, uint32_t buffer_size,
                                              dmResource::SResourceDescriptor* resource,
                                              const char* filename);

    static dmGameObject::CreateResult CompReloadTargetCreate(dmGameObject::HCollection collection,
                                                             dmGameObject::HInstance instance,
                                                             void* resource,
                                                             void* world,
                                                             void* context,
                                                             uintptr_t* user_data);
    static dmGameObject::CreateResult CompReloadTargetDestroy(dmGameObject::HCollection collection,
                                                              dmGameObject::HInstance instance,
                                                              void* world,
                                                              void* context,
                                                              uintptr_t* user_data);
    static void CompReloadTargetOnReload(dmGameObject::HInstance instance,
                                            void* resource,
                                            void* world,
                                            void* context,
                                            uintptr_t* user_data);

public:

    void* m_NewResource;

    dmGameObject::UpdateContext m_UpdateContext;
    dmGameObject::HRegister m_Register;
    dmGameObject::HCollection m_Collection;
    dmResource::HFactory m_Factory;
};

dmResource::CreateResult ReloadTest::ResReloadTargetCreate(dmResource::HFactory factory, void* context, const void* buffer, uint32_t buffer_size, dmResource::SResourceDescriptor* resource, const char* filename)
{
    TestGameObjectDDF::ReloadTarget* obj;
    dmDDF::Result e = dmDDF::LoadMessage<TestGameObjectDDF::ReloadTarget>(buffer, buffer_size, &obj);
    if (e == dmDDF::RESULT_OK)
    {
        resource->m_Resource = (void*) obj;
        return dmResource::CREATE_RESULT_OK;
    }
    else
    {
        return dmResource::CREATE_RESULT_UNKNOWN;
    }
}

dmResource::CreateResult ReloadTest::ResReloadTargetDestroy(dmResource::HFactory factory, void* context, dmResource::SResourceDescriptor* resource)
{
    dmDDF::FreeMessage((void*) resource->m_Resource);
    return dmResource::CREATE_RESULT_OK;
}

dmResource::CreateResult ReloadTest::ResReloadTargetRecreate(dmResource::HFactory factory,
                                          void* context,
                                          const void* buffer, uint32_t buffer_size,
                                          dmResource::SResourceDescriptor* resource,
                                          const char* filename)
{
    dmDDF::FreeMessage((void*) resource->m_Resource);
    TestGameObjectDDF::ReloadTarget* obj;
    dmDDF::Result e = dmDDF::LoadMessage<TestGameObjectDDF::ReloadTarget>(buffer, buffer_size, &obj);
    if (e == dmDDF::RESULT_OK)
    {
        resource->m_Resource = (void*) obj;
        return dmResource::CREATE_RESULT_OK;
    }
    else
    {
        return dmResource::CREATE_RESULT_UNKNOWN;
    }
}

dmGameObject::CreateResult ReloadTest::CompReloadTargetCreate(dmGameObject::HCollection collection,
                                                         dmGameObject::HInstance instance,
                                                         void* resource,
                                                         void* world,
                                                         void* context,
                                                         uintptr_t* user_data)
{
    return dmGameObject::CREATE_RESULT_OK;
}

dmGameObject::CreateResult ReloadTest::CompReloadTargetDestroy(dmGameObject::HCollection collection,
                                                          dmGameObject::HInstance instance,
                                                          void* world,
                                                          void* context,
                                                          uintptr_t* user_data)
{
    return dmGameObject::CREATE_RESULT_OK;
}

void ReloadTest::CompReloadTargetOnReload(dmGameObject::HInstance instance,
                                        void* resource,
                                        void* world,
                                        void* context,
                                        uintptr_t* user_data)
{
    ReloadTest* self = (ReloadTest*) context;
    self->m_NewResource = resource;
}

TEST_F(ReloadTest, TestComponentReload)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "component_reload.goc");
    ASSERT_NE((void*) 0, (void*) go);

    bool r = dmGameObject::Update(&m_Collection, &m_UpdateContext, 1);

    ASSERT_FALSE(r);

    ASSERT_EQ((void*)0, m_NewResource);

    dmResource::ReloadResult rr = dmResource::ReloadResource(m_Factory, "reload_target.rt", 0);
    ASSERT_EQ(dmResource::RELOAD_RESULT_OK, rr);

    ASSERT_NE((void*)0, m_NewResource);
    TestGameObjectDDF::ReloadTarget* rt = (TestGameObjectDDF::ReloadTarget*)m_NewResource;
    ASSERT_EQ(123, rt->m_Value);

    dmResource::ReloadResource(m_Factory, "component_reload.scriptc", 0);

    r = dmGameObject::Update(&m_Collection, &m_UpdateContext, 1);

    ASSERT_TRUE(r);

    dmGameObject::Delete(m_Collection, go);
}

TEST_F(ReloadTest, TestComponentReloadScriptFail)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "component_reload_fail.goc");
    ASSERT_NE((void*) 0, (void*) go);

    dmResource::ReloadResult rr = dmResource::ReloadResource(m_Factory, "component_reload_fail.scriptc", 0);
    ASSERT_EQ(dmResource::RELOAD_RESULT_OK, rr);

    dmGameObject::Delete(m_Collection, go);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
