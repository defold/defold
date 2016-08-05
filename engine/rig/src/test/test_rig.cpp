#include <gtest/gtest.h>

#include "../rig.h"

class RigTest : public ::testing::Test
{
public:
    dmRig::HRigContext m_Context;

private:
    virtual void SetUp() {
        dmRig::NewContextParams params;
        params.m_Context = &m_Context;
        params.m_MaxRigInstanceCount = 2;
        ASSERT_EQ(dmRig::CREATE_RESULT_OK, dmRig::NewContext(params));
    }

    virtual void TearDown() {
        dmRig::DeleteContextParams params;
        params.m_Context = m_Context;
        ASSERT_EQ(dmRig::CREATE_RESULT_OK, dmRig::DeleteContext(params));
    }
};

TEST_F(RigTest, InstanceCreation)
{
    dmRig::HRigInstance instance = 0x0;
    dmRig::InstanceCreateParams create_params;
    create_params.m_Context = m_Context;
    create_params.m_Instance = &instance;

    // Dummy data
    create_params.m_RigScene     = new dmRigDDF::RigScene();
    create_params.m_MeshSet      = new dmRigDDF::MeshSet();
    create_params.m_AnimationSet = new dmRigDDF::AnimationSet();

    create_params.m_SkinId           = (const char*)"dummy";
    create_params.m_DefaultAnimation = (const char*)"";

    ASSERT_EQ(dmRig::CREATE_RESULT_OK, dmRig::InstanceCreate(create_params));
    ASSERT_NE(0x0, (int)instance);

    delete create_params.m_RigScene;
    delete create_params.m_MeshSet;
    delete create_params.m_AnimationSet;

    dmRig::InstanceDestroyParams destroy_params;
    destroy_params.m_Context = m_Context;
    destroy_params.m_Instance = instance;
    ASSERT_EQ(dmRig::CREATE_RESULT_OK, dmRig::InstanceDestroy(destroy_params));
}

TEST_F(RigTest, UpdateEmptyContext)
{
    dmRig::UpdateResult res = dmRig::Update(m_Context, 1.0/60.0);
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, res);
}

TEST_F(RigTest, PlayInvalidAnimation)
{
    // uint32_t instance_id = -1;
    dmRig::HRigInstance instance = 0x0;
    dmRig::InstanceCreateParams create_params;
    create_params.m_Context = m_Context;
    // create_params.m_Index = &instance_id;
    create_params.m_Instance = &instance;

    // Dummy data
    create_params.m_RigScene     = new dmRigDDF::RigScene();
    create_params.m_MeshSet      = new dmRigDDF::MeshSet();
    create_params.m_AnimationSet = new dmRigDDF::AnimationSet();

    create_params.m_SkinId           = (const char*)"dummy";
    create_params.m_DefaultAnimation = (const char*)"default";
    dmhash_t default_anim_id = dmHashString64(create_params.m_DefaultAnimation);
    dmhash_t empty_id = dmHashString64("");

    ASSERT_EQ(dmRig::CREATE_RESULT_OK, dmRig::InstanceCreate(create_params));
    ASSERT_NE(0x0, (int)instance);
    // ASSERT_NE(-1, instance_id);

    // Default animation does not exist
    ASSERT_NE(default_anim_id, dmRig::GetAnimation(instance));
    ASSERT_EQ(empty_id, dmRig::GetAnimation(instance));

    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::Update(m_Context, 1.0/60.0));

    delete create_params.m_RigScene;
    delete create_params.m_MeshSet;
    delete create_params.m_AnimationSet;

    dmRig::InstanceDestroyParams destroy_params;
    destroy_params.m_Context = m_Context;
    destroy_params.m_Instance = instance;
    // destroy_params.m_Index = instance_id;
    ASSERT_EQ(dmRig::CREATE_RESULT_OK, dmRig::InstanceDestroy(destroy_params));
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
