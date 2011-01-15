#include <dlib/dstrings.h>

#include "../test_gamesys.h"

#include "gamesys/resources/res_camera.h"
#include "gamesys/components/comp_camera.h"

class CameraTest : public GamesysTest
{
};

TEST_F(CameraTest, TestResource)
{
    const char* resource_name = "camera/test.camerac";
    dmGameSystem::CameraResource* cam_resource;
    ASSERT_EQ(dmResource::FACTORY_RESULT_OK, dmResource::Get(m_Factory, resource_name, (void**)&cam_resource));
    dmGamesysDDF::CameraDesc backup = *cam_resource->m_DDF;
    dmGamesysDDF::CameraDesc* ddf = cam_resource->m_DDF;
    ASSERT_EQ(1.0f, ddf->m_AspectRatio);
    ASSERT_EQ(2.0f, ddf->m_FOV);
    ASSERT_EQ(3.0f, ddf->m_NearZ);
    ASSERT_EQ(4.0f, ddf->m_FarZ);
    ASSERT_EQ(0u, ddf->m_AutoAspectRatio);
    ddf->m_AspectRatio = 5.0f;
    ddf->m_FOV = 6.0f;
    ddf->m_NearZ = 7.0f;
    ddf->m_FarZ = 8.0f;
    ddf->m_AutoAspectRatio = 1;
    char path[128];
    DM_SNPRINTF(path, sizeof(path), "%s/%s", ROOT, resource_name);
    ASSERT_EQ(dmDDF::RESULT_OK, dmDDF::SaveMessageToFile(ddf, dmGamesysDDF::CameraDesc::m_DDFDescriptor, path));
    ASSERT_EQ(dmResource::RELOAD_RESULT_OK, dmResource::ReloadResource(m_Factory, resource_name, 0));
    ASSERT_NE(ddf, cam_resource->m_DDF);
    ASSERT_EQ(5.0f, cam_resource->m_DDF->m_AspectRatio);
    ASSERT_EQ(6.0f, cam_resource->m_DDF->m_FOV);
    ASSERT_EQ(7.0f, cam_resource->m_DDF->m_NearZ);
    ASSERT_EQ(8.0f, cam_resource->m_DDF->m_FarZ);
    ASSERT_EQ(1u, cam_resource->m_DDF->m_AutoAspectRatio);
    dmResource::Release(m_Factory, cam_resource);
    ASSERT_EQ(dmDDF::RESULT_OK, dmDDF::SaveMessageToFile(&backup, dmGamesysDDF::CameraDesc::m_DDFDescriptor, path));
}

TEST_F(CameraTest, TestResourceFail)
{
    const char* invalid_resource_name = "camera/invalid.camerac";
    char path[128];
    DM_SNPRINTF(path, sizeof(path), "%s/%s", ROOT, invalid_resource_name);
    FILE* f = fopen(path, "wb");
    ASSERT_NE((void*)0, f);
    fprintf(f, "abc");
    fclose(f);
    dmGameSystem::CameraResource* cam_resource;
    ASSERT_NE(dmResource::FACTORY_RESULT_OK, dmResource::Get(m_Factory, invalid_resource_name, (void**)&cam_resource));

    const char* resource_name = "camera/test.camerac";
    ASSERT_EQ(dmResource::FACTORY_RESULT_OK, dmResource::Get(m_Factory, resource_name, (void**)&cam_resource));
    ASSERT_EQ(dmDDF::RESULT_OK, dmDDF::SaveMessageToFile(cam_resource->m_DDF, dmGamesysDDF::CameraDesc::m_DDFDescriptor, path));
    dmGameSystem::CameraResource* invalid_cam_resource;
    ASSERT_EQ(dmResource::FACTORY_RESULT_OK, dmResource::Get(m_Factory, invalid_resource_name, (void**)&invalid_cam_resource));

    f = fopen(path, "wb");
    ASSERT_NE((void*)0, f);
    fprintf(f, "abc");
    fclose(f);

    ASSERT_NE(dmResource::RELOAD_RESULT_OK, dmResource::ReloadResource(m_Factory, invalid_resource_name, 0));

    ASSERT_EQ(cam_resource->m_DDF->m_AspectRatio, invalid_cam_resource->m_DDF->m_AspectRatio);
    ASSERT_EQ(cam_resource->m_DDF->m_FOV, invalid_cam_resource->m_DDF->m_FOV);
    ASSERT_EQ(cam_resource->m_DDF->m_NearZ, invalid_cam_resource->m_DDF->m_NearZ);
    ASSERT_EQ(cam_resource->m_DDF->m_FarZ, invalid_cam_resource->m_DDF->m_FarZ);
    ASSERT_EQ(cam_resource->m_DDF->m_AutoAspectRatio, invalid_cam_resource->m_DDF->m_AutoAspectRatio);

    dmResource::Release(m_Factory, cam_resource);
    dmResource::Release(m_Factory, invalid_cam_resource);

    unlink(path);
}

TEST_F(CameraTest, TestComponent)
{
    const char* resource_name = "camera/test.camerac";
    const char* go_name = "camera/test_camera.goc";

    dmGameObject::HInstance go = dmGameObject::New(m_Collection, go_name);
    ASSERT_NE((void*)0, go);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    ASSERT_TRUE(dmGameObject::Update(&m_Collection, &m_UpdateContext, 1));
    ASSERT_TRUE(dmGameObject::PostUpdate(&m_Collection, 1));

    ASSERT_EQ(dmResource::RELOAD_RESULT_OK, dmResource::ReloadResource(m_Factory, resource_name, 0));

    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    ASSERT_TRUE(dmGameObject::Update(&m_Collection, &m_UpdateContext, 1));
    ASSERT_TRUE(dmGameObject::PostUpdate(&m_Collection, 1));
}

TEST_F(CameraTest, TestComponentFail)
{
    const char* resource_name = "camera/test.camerac";
    const char* go_name = "camera/test_camera.goc";

    dmGameObject::HInstance go = dmGameObject::New(m_Collection, go_name);
    ASSERT_NE((void*)0, go);

    char path[128];
    DM_SNPRINTF(path, sizeof(path), "%s/%s", ROOT, resource_name);
    dmGamesysDDF::CameraDesc* ddf;
    ASSERT_EQ(dmDDF::RESULT_OK, dmDDF::LoadMessageFromFile(path, dmGamesysDDF::CameraDesc::m_DDFDescriptor, (void**)&ddf));
    FILE* f = fopen(path, "wb");
    ASSERT_NE((void*)0, f);
    fprintf(f, "abc");
    fclose(f);

    ASSERT_NE(dmResource::RELOAD_RESULT_OK, dmResource::ReloadResource(m_Factory, resource_name, 0));

    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    ASSERT_TRUE(dmGameObject::Update(&m_Collection, &m_UpdateContext, 1));
    ASSERT_TRUE(dmGameObject::PostUpdate(&m_Collection, 1));

    dmDDF::SaveMessageToFile(ddf, dmGamesysDDF::CameraDesc::m_DDFDescriptor, path);

    dmDDF::FreeMessage(ddf);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
