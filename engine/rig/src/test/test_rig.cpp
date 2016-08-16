#include "test_rig.h"

TEST_F(RigContextTest, InstanceCreation)
{
    dmRig::HRigInstance instance = 0x0;
    dmRig::InstanceCreateParams create_params = {0};
    create_params.m_Context = m_Context;
    create_params.m_Instance = &instance;

    // Dummy data
    dmArray<dmRig::RigBone> bind_pose;
    create_params.m_BindPose     = &bind_pose;
    create_params.m_Skeleton     = new dmRigDDF::Skeleton();
    create_params.m_MeshSet      = new dmRigDDF::MeshSet();
    create_params.m_AnimationSet = new dmRigDDF::AnimationSet();

    create_params.m_SkinId           = (const char*)"dummy";
    create_params.m_DefaultAnimation = (const char*)"";

    ASSERT_EQ(dmRig::CREATE_RESULT_OK, dmRig::InstanceCreate(create_params));
    ASSERT_NE((dmRig::HRigInstance)0x0, instance);

    delete create_params.m_Skeleton;
    delete create_params.m_MeshSet;
    delete create_params.m_AnimationSet;

    dmRig::InstanceDestroyParams destroy_params = {0};
    destroy_params.m_Context = m_Context;
    destroy_params.m_Instance = instance;
    ASSERT_EQ(dmRig::CREATE_RESULT_OK, dmRig::InstanceDestroy(destroy_params));
}

TEST_F(RigContextTest, UpdateEmptyContext)
{
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::Update(m_Context, 1.0/60.0));
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::PostUpdate(m_Context));
}

TEST_F(RigInstanceTest, PlayInvalidAnimation)
{
    dmhash_t invalid_anim_id = dmHashString64("invalid");
    dmhash_t empty_id = dmHashString64("");

    // Default animation does not exist
    ASSERT_EQ(dmRig::PLAY_RESULT_ANIM_NOT_FOUND, dmRig::PlayAnimation(m_Instance, invalid_anim_id, dmGameObject::PLAYBACK_LOOP_FORWARD, 0.0f));
    ASSERT_NE(invalid_anim_id, dmRig::GetAnimation(m_Instance));
    ASSERT_EQ(empty_id, dmRig::GetAnimation(m_Instance));

    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::Update(m_Context, 1.0/60.0));
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::PostUpdate(m_Context));
}

TEST_F(RigInstanceTest, PlayValidAnimation)
{
    dmhash_t valid_anim_id = dmHashString64("valid");

    // Default animation does not exist
    ASSERT_EQ(dmRig::PLAY_RESULT_OK, dmRig::PlayAnimation(m_Instance, valid_anim_id, dmGameObject::PLAYBACK_LOOP_FORWARD, 0.0f));
    ASSERT_EQ(valid_anim_id, dmRig::GetAnimation(m_Instance));

    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::Update(m_Context, 1.0/60.0));
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::PostUpdate(m_Context));
}

#define ASSERT_VEC3(exp, act)\
    ASSERT_NEAR(exp.getX(), act.getX(), RIG_EPSILON);\
    ASSERT_NEAR(exp.getY(), act.getY(), RIG_EPSILON);\
    ASSERT_NEAR(exp.getZ(), act.getZ(), RIG_EPSILON);\

#define ASSERT_VEC4(exp, act)\
    ASSERT_NEAR(exp.getX(), act.getX(), RIG_EPSILON);\
    ASSERT_NEAR(exp.getY(), act.getY(), RIG_EPSILON);\
    ASSERT_NEAR(exp.getZ(), act.getZ(), RIG_EPSILON);\
    ASSERT_NEAR(exp.getW(), act.getW(), RIG_EPSILON);\

TEST_F(RigInstanceTest, PoseNoAnim)
{
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::Update(m_Context, 1.0/60.0));
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::PostUpdate(m_Context));

    dmArray<dmTransform::Transform>& pose = *dmRig::GetPose(m_Instance);

    // should be same as bind pose
    ASSERT_VEC3(Vector3(0.0f), pose[0].GetTranslation());
    ASSERT_VEC3(Vector3(1.0f, 0.0f, 0.0f), pose[1].GetTranslation());
    ASSERT_VEC4(Quat::identity(), pose[0].GetRotation());
    ASSERT_VEC4(Quat::identity(), pose[1].GetRotation());

    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::Update(m_Context, 1.0/60.0));
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::PostUpdate(m_Context));

    ASSERT_VEC3(Vector3(0.0f), pose[0].GetTranslation());
    ASSERT_VEC3(Vector3(1.0f, 0.0f, 0.0f), pose[1].GetTranslation());
    ASSERT_VEC4(Quat::identity(), pose[0].GetRotation());
    ASSERT_VEC4(Quat::identity(), pose[1].GetRotation());
}

TEST_F(RigInstanceTest, PoseAnim)
{
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::PostUpdate(m_Context));
    ASSERT_EQ(dmRig::PLAY_RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmGameObject::PLAYBACK_LOOP_FORWARD, 0.0f));

    dmArray<dmTransform::Transform>& pose = *dmRig::GetPose(m_Instance);

    // sample 0
    ASSERT_VEC3(Vector3(0.0f), pose[0].GetTranslation());
    ASSERT_VEC3(Vector3(1.0f, 0.0f, 0.0f), pose[1].GetTranslation());
    ASSERT_VEC4(Quat::identity(), pose[0].GetRotation());
    ASSERT_VEC4(Quat::identity(), pose[1].GetRotation());

    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::PostUpdate(m_Context));

    // sample 1
    ASSERT_VEC3(Vector3(0.0f), pose[0].GetTranslation());
    ASSERT_VEC3(Vector3(1.0f, 0.0f, 0.0f), pose[1].GetTranslation());
    ASSERT_VEC4(Quat::identity(), pose[0].GetRotation());
    ASSERT_VEC4(Quat::rotationZ((float)M_PI / 2.0f), pose[1].GetRotation());

    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::PostUpdate(m_Context));

    // sample 2
    ASSERT_VEC3(Vector3(0.0f), pose[0].GetTranslation());
    ASSERT_VEC3(Vector3(0.0f, 1.0f, 0.0f), pose[1].GetTranslation());
    ASSERT_VEC4(Quat::rotationZ((float)M_PI / 2.0f), pose[0].GetRotation());
    ASSERT_VEC4(Quat::rotationZ((float)M_PI / 2.0f), pose[1].GetRotation());


    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::PostUpdate(m_Context));

    // sample 0 (looped)
    ASSERT_VEC3(Vector3(0.0f), pose[0].GetTranslation());
    ASSERT_VEC3(Vector3(1.0f, 0.0f, 0.0f), pose[1].GetTranslation());
    ASSERT_VEC4(Quat::identity(), pose[0].GetRotation());
    ASSERT_VEC4(Quat::identity(), pose[1].GetRotation());
}

TEST_F(RigInstanceTest, PoseAnimCancel)
{
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::PostUpdate(m_Context));
    ASSERT_EQ(dmRig::PLAY_RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmGameObject::PLAYBACK_LOOP_FORWARD, 0.0f));

    dmArray<dmTransform::Transform>& pose = *dmRig::GetPose(m_Instance);

    // sample 0
    ASSERT_VEC3(Vector3(0.0f), pose[0].GetTranslation());
    ASSERT_VEC3(Vector3(1.0f, 0.0f, 0.0f), pose[1].GetTranslation());
    ASSERT_VEC4(Quat::identity(), pose[0].GetRotation());
    ASSERT_VEC4(Quat::identity(), pose[1].GetRotation());

    ASSERT_EQ(dmRig::PLAY_RESULT_OK, dmRig::CancelAnimation(m_Instance));
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::PostUpdate(m_Context));

    // sample 1
    ASSERT_VEC3(Vector3(0.0f), pose[0].GetTranslation());
    ASSERT_VEC3(Vector3(1.0f, 0.0f, 0.0f), pose[1].GetTranslation());
    ASSERT_VEC4(Quat::identity(), pose[0].GetRotation());
    ASSERT_VEC4(Quat::identity(), pose[1].GetRotation());

}

TEST_F(RigInstanceTest, GetVertexCount)
{
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::PostUpdate(m_Context));
    ASSERT_EQ(3, dmRig::GetVertexCount(m_Instance));
}

TEST_F(RigInstanceTest, GenerateVertexData)
{
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::PostUpdate(m_Context));
    ASSERT_EQ(dmRig::PLAY_RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmGameObject::PLAYBACK_LOOP_FORWARD, 0.0f));

    dmRig::RigVertexData* data = new dmRig::RigVertexData[3];
    dmRig::RigGenVertexDataParams params;
    params.m_ModelMatrix = Matrix4::identity();
    params.m_VertexData = (void**)&data;
    params.m_VertexStride = sizeof(dmRig::RigVertexData);

    // sample 0
    ASSERT_EQ(data + 3, dmRig::GenerateVertexData(m_Context, m_Instance, params));
    ASSERT_VEC3(Vector3(0.0f), Vector3(data[0].x, data[0].y, data[0].z));            // v0
    ASSERT_VEC3(Vector3(1.0f, 0.0f, 0.0), Vector3(data[1].x, data[1].y, data[1].z)); // v1
    ASSERT_VEC3(Vector3(2.0f, 0.0f, 0.0), Vector3(data[2].x, data[2].y, data[2].z)); // v2

    // sample 1
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::PostUpdate(m_Context));
    ASSERT_EQ(data + 3, dmRig::GenerateVertexData(m_Context, m_Instance, params));
    ASSERT_VEC3(Vector3(0.0f), Vector3(data[0].x, data[0].y, data[0].z));            // v0
    ASSERT_VEC3(Vector3(1.0f, 0.0f, 0.0), Vector3(data[1].x, data[1].y, data[1].z)); // v1
    ASSERT_VEC3(Vector3(1.0f, 1.0f, 0.0), Vector3(data[2].x, data[2].y, data[2].z)); // v2

    // sample 2
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::PostUpdate(m_Context));
    ASSERT_EQ(data + 3, dmRig::GenerateVertexData(m_Context, m_Instance, params));
    ASSERT_VEC3(Vector3(0.0f), Vector3(data[0].x, data[0].y, data[0].z));            // v0
    ASSERT_VEC3(Vector3(0.0f, 1.0f, 0.0), Vector3(data[1].x, data[1].y, data[1].z)); // v1
    ASSERT_VEC3(Vector3(0.0f, 2.0f, 0.0), Vector3(data[2].x, data[2].y, data[2].z)); // v2

    delete [] data;
}

TEST_F(RigInstanceTest, SetSkinInvalid)
{
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::PostUpdate(m_Context));
    ASSERT_EQ(dmRig::PLAY_RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmGameObject::PLAYBACK_LOOP_FORWARD, 0.0f));

    dmRig::RigVertexData* data = new dmRig::RigVertexData[3];
    dmRig::RigGenVertexDataParams params;
    params.m_ModelMatrix = Matrix4::identity();
    params.m_VertexData = (void**)&data;
    params.m_VertexStride = sizeof(dmRig::RigVertexData);

    ASSERT_EQ(data + 3, dmRig::GenerateVertexData(m_Context, m_Instance, params));
    ASSERT_EQ(0, data[0].r);
    ASSERT_EQ(0, data[0].g);
    ASSERT_EQ(0, data[0].b);
    ASSERT_EQ(0, data[0].a);

    dmhash_t new_skin = dmHashString64("not_a_valid_skin");
    ASSERT_EQ(dmRig::UPDATE_RESULT_FAILED, dmRig::SetSkin(m_Instance, new_skin));
    ASSERT_EQ(dmHashString64("test"), dmRig::GetSkin(m_Instance));

    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::PostUpdate(m_Context));
    ASSERT_EQ(data + 3, dmRig::GenerateVertexData(m_Context, m_Instance, params));
    ASSERT_EQ(0, data[0].r);
    ASSERT_EQ(0, data[0].g);
    ASSERT_EQ(0, data[0].b);
    ASSERT_EQ(0, data[0].a);

    delete [] data;
}


TEST_F(RigInstanceTest, SetSkinValid)
{
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::PostUpdate(m_Context));
    ASSERT_EQ(dmRig::PLAY_RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmGameObject::PLAYBACK_LOOP_FORWARD, 0.0f));

    dmRig::RigVertexData* data = new dmRig::RigVertexData[3];
    dmRig::RigGenVertexDataParams params;
    params.m_ModelMatrix = Matrix4::identity();
    params.m_VertexData = (void**)&data;
    params.m_VertexStride = sizeof(dmRig::RigVertexData);

    ASSERT_EQ(data + 3, dmRig::GenerateVertexData(m_Context, m_Instance, params));
    ASSERT_EQ(0, data[0].r);
    ASSERT_EQ(0, data[0].g);
    ASSERT_EQ(0, data[0].b);
    ASSERT_EQ(0, data[0].a);

    dmhash_t new_skin = dmHashString64("secondary_skin");
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::SetSkin(m_Instance, new_skin));
    ASSERT_EQ(new_skin, dmRig::GetSkin(m_Instance));

    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::PostUpdate(m_Context));
    ASSERT_EQ(data + 3, dmRig::GenerateVertexData(m_Context, m_Instance, params));
    ASSERT_EQ(255, data[0].r);
    ASSERT_EQ(255, data[0].g);
    ASSERT_EQ(255, data[0].b);
    ASSERT_EQ(255, data[0].a);

    delete [] data;
}

TEST_F(RigInstanceTest, CursorNoAnim)
{

    // no anim
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON);
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::PostUpdate(m_Context));
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON);

    // no anim + set cursor
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON);
    ASSERT_EQ(dmRig::PLAY_RESULT_OK, dmRig::SetCursor(m_Instance, 100.0f, false));
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::PostUpdate(m_Context));
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON);

}

TEST_F(RigInstanceTest, CursorGet)
{
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::PostUpdate(m_Context));
    ASSERT_EQ(dmRig::PLAY_RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmGameObject::PLAYBACK_LOOP_FORWARD, 0.0f));

    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON);
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON);
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::PostUpdate(m_Context));
    ASSERT_NEAR(1.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON);
    ASSERT_NEAR(1.0f / 3.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON);

    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::PostUpdate(m_Context));
    ASSERT_NEAR(2.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON);
    ASSERT_NEAR(2.0f / 3.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON);

    // "half a sample"
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::Update(m_Context, 0.5f));
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::PostUpdate(m_Context));
    ASSERT_NEAR(2.5f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON);
    ASSERT_NEAR(2.5f / 3.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON);

    // animation restarted/looped
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::Update(m_Context, 0.5f));
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::PostUpdate(m_Context));
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON);
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON);

}

TEST_F(RigInstanceTest, CursorSet)
{
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::PostUpdate(m_Context));
    ASSERT_EQ(dmRig::PLAY_RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmGameObject::PLAYBACK_LOOP_FORWARD, 0.0f));

    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON);
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON);
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::PostUpdate(m_Context));
    ASSERT_NEAR(1.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON);
    ASSERT_NEAR(1.0f / 3.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON);

    ASSERT_NEAR(dmRig::PLAY_RESULT_OK, dmRig::SetCursor(m_Instance, 0.0f, false), RIG_EPSILON);
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON);
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON);

    ASSERT_NEAR(dmRig::PLAY_RESULT_OK, dmRig::SetCursor(m_Instance, 0.5f, false), RIG_EPSILON);
    ASSERT_NEAR(0.5f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON);
    ASSERT_NEAR(0.5f / 3.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON);

    ASSERT_NEAR(dmRig::PLAY_RESULT_OK, dmRig::SetCursor(m_Instance, 0.0f, true), RIG_EPSILON);
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON);
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON);

    ASSERT_NEAR(dmRig::PLAY_RESULT_OK, dmRig::SetCursor(m_Instance, 0.5f, true), RIG_EPSILON);
    ASSERT_NEAR(3.0f * 0.5f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON);
    ASSERT_NEAR(0.5f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON);

    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::PostUpdate(m_Context));
    ASSERT_NEAR(2.5f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON);
    ASSERT_NEAR(2.5f / 3.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON);
}

TEST_F(RigInstanceTest, CursorSetOutside)
{
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::PostUpdate(m_Context));
    ASSERT_EQ(dmRig::PLAY_RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmGameObject::PLAYBACK_LOOP_FORWARD, 0.0f));

    ASSERT_NEAR(dmRig::PLAY_RESULT_OK, dmRig::SetCursor(m_Instance, 4.0f, false), RIG_EPSILON);
    ASSERT_NEAR(1.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON);

    ASSERT_NEAR(dmRig::PLAY_RESULT_OK, dmRig::SetCursor(m_Instance, -4.0f, false), RIG_EPSILON);
    ASSERT_NEAR(2.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON);

    ASSERT_NEAR(dmRig::PLAY_RESULT_OK, dmRig::SetCursor(m_Instance, 4.0f / 3.0f, true), RIG_EPSILON);
    ASSERT_NEAR(1.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON);

    ASSERT_NEAR(dmRig::PLAY_RESULT_OK, dmRig::SetCursor(m_Instance, -4.0f / 3.0f, true), RIG_EPSILON);
    ASSERT_NEAR(2.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON);
}

static Vector3 IKTargetPositionCallback(void* user_data, void*)
{
    return *(Vector3*)user_data;
}

TEST_F(RigInstanceTest, SetIKTarget)
{
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::PostUpdate(m_Context));
    ASSERT_EQ(dmRig::PLAY_RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("ik_anim"), dmGameObject::PLAYBACK_LOOP_FORWARD, 0.0f));

    Vector3 tmp_val = Vector3(0.0f, 100.0f, 0.0f);

    dmRig::RigIKTargetParams params = {0};
    params.m_RigInstance = m_Instance;
    params.m_ConstraintId = dmHashString64("test_ik");
    params.m_Mix = 1.0f;
    params.m_Callback = IKTargetPositionCallback;
    params.m_UserData1 = (void*)&tmp_val;
    params.m_UserData2 = 0x0;
    ASSERT_EQ(dmRig::IKUPDATE_RESULT_OK, dmRig::SetIKTarget(params));
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::Update(m_Context, 1.0f));

    dmArray<dmTransform::Transform>& pose = *dmRig::GetPose(m_Instance);

    ASSERT_VEC3(Vector3(0.0f, 1.0f, 0.0f), pose[3].GetTranslation());
    ASSERT_VEC3(Vector3(0.0f, 1.0f, 0.0f), pose[4].GetTranslation());
    ASSERT_VEC4(Quat::identity(), pose[0].GetRotation());
    ASSERT_VEC4(Quat::identity(), pose[3].GetRotation());
    ASSERT_VEC4(Quat::identity(), pose[4].GetRotation());

    tmp_val.setX(4.0f);
    tmp_val.setY(0.0f);

    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::Update(m_Context, 0.0f));

    ASSERT_VEC3(Vector3(0.0f, 1.0f, 0.0f), pose[3].GetTranslation());
    ASSERT_VEC3(Vector3(0.0f, 1.0f, 0.0f), pose[4].GetTranslation());
    ASSERT_VEC4(Quat::identity(), pose[0].GetRotation());
    Quat q = Quat::rotationZ(-(float)M_PI / 2.0f);
    Quat p3 = pose[3].GetRotation();
    // printf("rotZ90: %f, %f, %f, %f\n", q.getX(), q.getY(), q.getZ(), q.getW());
    // printf("pose3: %f, %f, %f, %f\n", p3.getX(), p3.getY(), p3.getZ(), p3.getW());
    // ASSERT_VEC4(q, p3);
    ASSERT_VEC4(Quat::identity(), pose[4].GetRotation());
}

#undef ASSERT_VEC3
#undef ASSERT_VEC4

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
