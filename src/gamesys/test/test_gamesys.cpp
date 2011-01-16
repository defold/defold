#include "test_gamesys.h"

#include <stdio.h>

#include <dlib/dstrings.h>

#include <gameobject/gameobject_ddf.h>

const char* ROOT = "build/default/src/gamesys/test";

bool CopyResource(const char* src, const char* dst)
{
    char src_path[128];
    DM_SNPRINTF(src_path, sizeof(src_path), "%s/%s", ROOT, src);
    FILE* src_f = fopen(src_path, "rb");
    if (src_f == 0x0)
        return false;
    char dst_path[128];
    DM_SNPRINTF(dst_path, sizeof(dst_path), "%s/%s", ROOT, dst);
    FILE* dst_f = fopen(dst_path, "wb");
    if (dst_f == 0x0)
    {
        fclose(src_f);
        return false;
    }
    char buffer[1024];
    int c = fread(buffer, 1, sizeof(buffer), src_f);
    while (c > 0)
    {
        fwrite(buffer, 1, c, dst_f);
        c = fread(buffer, 1, sizeof(buffer), src_f);
    }

    fclose(src_f);
    fclose(dst_f);

    return true;
}

bool UnlinkResource(const char* name)
{
    char path[128];
    DM_SNPRINTF(path, sizeof(path), "%s/%s", ROOT, name);
    return unlink(path) == 0;
}

TEST_P(ResourceTest, Test)
{
    const char* resource_name = GetParam();
    void* resource;
    ASSERT_EQ(dmResource::FACTORY_RESULT_OK, dmResource::Get(m_Factory, resource_name, &resource));
    ASSERT_NE((void*)0, resource);

    ASSERT_EQ(dmResource::RELOAD_RESULT_OK, dmResource::ReloadResource(m_Factory, resource_name, 0));

    dmResource::Release(m_Factory, resource);
}

TEST_P(ResourceFailTest, Test)
{
    const ResourceFailParams& p = GetParam();
    const char* tmp_name = "tmp";

    void* resource;
    ASSERT_NE(dmResource::FACTORY_RESULT_OK, dmResource::Get(m_Factory, p.m_InvalidResource, &resource));

    bool exists = CopyResource(p.m_InvalidResource, tmp_name);
    ASSERT_TRUE(CopyResource(p.m_ValidResource, p.m_InvalidResource));
    ASSERT_EQ(dmResource::FACTORY_RESULT_OK, dmResource::Get(m_Factory, p.m_InvalidResource, &resource));

    if (exists)
        ASSERT_TRUE(CopyResource(tmp_name, p.m_InvalidResource));
    else
        ASSERT_TRUE(UnlinkResource(p.m_InvalidResource));
    ASSERT_NE(dmResource::RELOAD_RESULT_OK, dmResource::ReloadResource(m_Factory, p.m_InvalidResource, 0));

    dmResource::Release(m_Factory, resource);

    UnlinkResource(tmp_name);
}

TEST_P(ComponentTest, Test)
{
    const char* go_name = GetParam();
    dmGameObjectDDF::PrototypeDesc* go_ddf;
    char path[128];
    DM_SNPRINTF(path, sizeof(path), "%s/%s", ROOT, go_name);
    ASSERT_EQ(dmDDF::RESULT_OK, dmDDF::LoadMessageFromFile(path, dmGameObjectDDF::PrototypeDesc::m_DDFDescriptor, (void**)&go_ddf));
    ASSERT_LT(0u, go_ddf->m_Components.m_Count);
    const char* component_name = go_ddf->m_Components[0].m_Resource;

    dmGameObject::HInstance go = dmGameObject::New(m_Collection, go_name);
    ASSERT_NE((void*)0, go);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    ASSERT_TRUE(dmGameObject::Update(&m_Collection, &m_UpdateContext, 1));
    ASSERT_TRUE(dmGameObject::PostUpdate(&m_Collection, 1));

    ASSERT_EQ(dmResource::RELOAD_RESULT_OK, dmResource::ReloadResource(m_Factory, component_name, 0));

    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    ASSERT_TRUE(dmGameObject::Update(&m_Collection, &m_UpdateContext, 1));
    ASSERT_TRUE(dmGameObject::PostUpdate(&m_Collection, 1));

    dmDDF::FreeMessage(go_ddf);
}

TEST_P(ComponentTest, TestReloadFail)
{
    const char* go_name = GetParam();
    dmGameObjectDDF::PrototypeDesc* go_ddf;
    char path[128];
    DM_SNPRINTF(path, sizeof(path), "%s/%s", ROOT, go_name);
    ASSERT_EQ(dmDDF::RESULT_OK, dmDDF::LoadMessageFromFile(path, dmGameObjectDDF::PrototypeDesc::m_DDFDescriptor, (void**)&go_ddf));
    ASSERT_LT(0u, go_ddf->m_Components.m_Count);
    const char* component_name = go_ddf->m_Components[0].m_Resource;
    const char* temp_name = "tmp";

    dmGameObject::HInstance go = dmGameObject::New(m_Collection, go_name);
    ASSERT_NE((void*)0, go);

    ASSERT_TRUE(CopyResource(component_name, temp_name));
    ASSERT_TRUE(UnlinkResource(component_name));

    ASSERT_NE(dmResource::RELOAD_RESULT_OK, dmResource::ReloadResource(m_Factory, component_name, 0));

    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    ASSERT_TRUE(dmGameObject::Update(&m_Collection, &m_UpdateContext, 1));
    ASSERT_TRUE(dmGameObject::PostUpdate(&m_Collection, 1));

    ASSERT_TRUE(CopyResource(temp_name, component_name));

    dmDDF::FreeMessage(go_ddf);
}

TEST_P(ComponentFailTest, Test)
{
    const char* go_name = GetParam();

    dmGameObject::HInstance go = dmGameObject::New(m_Collection, go_name);
    ASSERT_EQ((void*)0, go);
}

/* Camera */

const char* valid_camera_resources[] = {"camera/valid.camerac"};
INSTANTIATE_TEST_CASE_P(Camera, ResourceTest, ::testing::ValuesIn(valid_camera_resources));

ResourceFailParams invalid_camera_resources[] =
{
    {"camera/valid.camerac", "camera/missing.camerac"},
};
INSTANTIATE_TEST_CASE_P(Camera, ResourceFailTest, ::testing::ValuesIn(invalid_camera_resources));

const char* valid_camera_gos[] = {"camera/valid_camera.goc"};
INSTANTIATE_TEST_CASE_P(Camera, ComponentTest, ::testing::ValuesIn(valid_camera_gos));

const char* invalid_camera_gos[] = {"camera/invalid_camera.goc"};
INSTANTIATE_TEST_CASE_P(Camera, ComponentFailTest, ::testing::ValuesIn(invalid_camera_gos));

/* Collision Object */

const char* valid_collision_object_resources[] = {"collision_object/valid.collisionobjectc"};
INSTANTIATE_TEST_CASE_P(CollisionObject, ResourceTest, ::testing::ValuesIn(valid_collision_object_resources));

ResourceFailParams invalid_collision_object_resources[] =
{
    {"collision_object/valid.collisionobjectc", "collision_object/missing.collisionobjectc"},
    {"collision_object/valid.collisionobjectc", "collision_object/missing.collisionobjectc"},
};
INSTANTIATE_TEST_CASE_P(CollisionObject, ResourceFailTest, ::testing::ValuesIn(invalid_collision_object_resources));

const char* valid_collision_object_gos[] = {"collision_object/valid_collision_object.goc"};
INSTANTIATE_TEST_CASE_P(CollisionObject, ComponentTest, ::testing::ValuesIn(valid_collision_object_gos));

const char* invalid_collision_object_gos[] =
{
    "collision_object/invalid_mass.goc",
    "collision_object/invalid_shape.goc"
};
INSTANTIATE_TEST_CASE_P(CollisionObject, ComponentFailTest, ::testing::ValuesIn(invalid_collision_object_gos));

/* Convex Shape */

const char* valid_cs_resources[] =
{
    "convex_shape/box.convexshapec",
    "convex_shape/capsule.convexshapec",
    "convex_shape/hull.convexshapec",
    "convex_shape/sphere.convexshapec",
};
INSTANTIATE_TEST_CASE_P(ConvexShape, ResourceTest, ::testing::ValuesIn(valid_cs_resources));

ResourceFailParams invalid_cs_resources[] =
{
    {"convex_shape/box.convexshapec", "convex_shape/invalid_box.convexshapec"},
    {"convex_shape/capsule.convexshapec", "convex_shape/invalid_capsule.convexshapec"},
    {"convex_shape/hull.convexshapec", "convex_shape/invalid_hull.convexshapec"},
    {"convex_shape/sphere.convexshapec", "convex_shape/invalid_sphere.convexshapec"},
};
INSTANTIATE_TEST_CASE_P(ConvexShape, ResourceFailTest, ::testing::ValuesIn(invalid_cs_resources));

/* Fragment Program */

const char* valid_fp_resources[] = {"fragment_program/valid.fpc"};
INSTANTIATE_TEST_CASE_P(FragmentProgram, ResourceTest, ::testing::ValuesIn(valid_fp_resources));

ResourceFailParams invalid_fp_resources[] =
{
    {"fragment_program/valid.fpc", "fragment_program/missing.fpc"},
};
INSTANTIATE_TEST_CASE_P(FragmentProgram, ResourceFailTest, ::testing::ValuesIn(invalid_fp_resources));

/* Texture */

const char* valid_texture_resources[] = {"texture/valid.texturec"};
INSTANTIATE_TEST_CASE_P(Texture, ResourceTest, ::testing::ValuesIn(valid_texture_resources));

ResourceFailParams invalid_texture_resources[] =
{
    {"texture/valid.texturec", "texture/missing.texturec"},
};
INSTANTIATE_TEST_CASE_P(Texture, ResourceFailTest, ::testing::ValuesIn(invalid_texture_resources));

/* Vertex Program */

const char* valid_vp_resources[] = {"vertex_program/valid.vpc"};
INSTANTIATE_TEST_CASE_P(VertexProgram, ResourceTest, ::testing::ValuesIn(valid_vp_resources));

ResourceFailParams invalid_vp_resources[] =
{
    {"vertex_program/valid.vpc", "vertex_program/missing.vpc"},
};
INSTANTIATE_TEST_CASE_P(VertexProgram, ResourceFailTest, ::testing::ValuesIn(invalid_vp_resources));

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
