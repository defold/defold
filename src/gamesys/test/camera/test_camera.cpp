#include "../test_gamesys.h"
#include "gamesys/resources/res_camera.h"

#include <unistd.h>

class CameraTest : public GamesysTest
{
};

TEST_F(CameraTest, TestResource)
{
    const char* resource_name = "camera/test.camerac";
    dmGameSystem::CameraProperties* props;
    ASSERT_EQ(dmResource::FACTORY_RESULT_OK, dmResource::Get(m_Factory, resource_name, (void**)&props));
    dmCameraDDF::CameraDesc* ddf = props->m_DDF;
    ASSERT_EQ(1.0f, ddf->m_AspectRatio);
    ASSERT_EQ(2.0f, ddf->m_FOV);
    ASSERT_EQ(3.0f, ddf->m_NearZ);
    ASSERT_EQ(4.0f, ddf->m_FarZ);
    ddf->m_AspectRatio = 5.0f;
    ddf->m_AspectRatio = 6.0f;
    ddf->m_AspectRatio = 7.0f;
    ddf->m_AspectRatio = 8.0f;
    ASSERT_EQ(dmResource::RELOAD_RESULT_OK, dmResource::ReloadResource(m_Factory, resource_name, 0));
    ASSERT_NE(ddf, props->m_DDF);
    dmResource::Release(m_Factory, props);
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

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
