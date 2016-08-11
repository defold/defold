#include <gtest/gtest.h>
#include <dlib/log.h>

#include "../rig.h"

#define RIG_EPSILON 0.0001f

class RigContextTest : public ::testing::Test
{
public:
    dmRig::HRigContext m_Context;

protected:
    virtual void SetUp() {
        dmRig::NewContextParams params;
        params.m_Context = &m_Context;
        params.m_MaxRigInstanceCount = 2;
        if (dmRig::CREATE_RESULT_OK != dmRig::NewContext(params)) {
            dmLogError("Could not create rig context!");
        }
    }

    virtual void TearDown() {
        if (dmRig::CREATE_RESULT_OK != dmRig::DeleteContext(m_Context)) {
            dmLogError("Could not delete rig context!");
        }
    }
};

class RigInstanceTest : public RigContextTest
{
public:
    dmRig::HRigInstance     m_Instance;
    dmArray<dmRig::RigBone> m_BindPose;
    dmRigDDF::Skeleton*     m_Skeleton;
    dmRigDDF::MeshSet*      m_MeshSet;
    dmRigDDF::AnimationSet* m_AnimationSet;

private:
    void SetUpSimpleSpine() {
        uint32_t bone_count = 2;
        m_Skeleton->m_Bones.m_Data = new dmRigDDF::Bone[bone_count];
        m_Skeleton->m_Bones.m_Count = bone_count;

        // Bone 0
        dmRigDDF::Bone& bone0 = m_Skeleton->m_Bones.m_Data[0];
        bone0.m_Parent       = 0xffff;
        bone0.m_Id           = 0;
        bone0.m_Position     = Vectormath::Aos::Point3(0.0f, 0.0f, 0.0f);
        bone0.m_Rotation     = Vectormath::Aos::Quat::identity();
        bone0.m_Scale        = Vectormath::Aos::Vector3(1.0f, 1.0f, 1.0f);
        bone0.m_InheritScale = false;
        bone0.m_Length       = 1.0f;

        // Bone 1
        dmRigDDF::Bone& bone1 = m_Skeleton->m_Bones.m_Data[1];
        bone1.m_Parent       = 0;
        bone1.m_Id           = 1;
        bone1.m_Position     = Vectormath::Aos::Point3(1.0f, 0.0f, 0.0f);
        bone1.m_Rotation     = Vectormath::Aos::Quat::identity();
        bone1.m_Scale        = Vectormath::Aos::Vector3(1.0f, 1.0f, 1.0f);
        bone1.m_InheritScale = false;
        bone1.m_Length       = 1.0f;

        m_BindPose.SetCapacity(bone_count);
        m_BindPose.SetSize(bone_count);
        // TMP: code from res_rig_scene.h
        for (uint32_t i = 0; i < bone_count; ++i)
        {
            dmRig::RigBone* bind_bone = &m_BindPose[i];
            dmRigDDF::Bone* bone = &m_Skeleton->m_Bones[i];
            bind_bone->m_LocalToParent = dmTransform::Transform(Vector3(bone->m_Position), bone->m_Rotation, bone->m_Scale);
            if (i > 0)
            {
                bind_bone->m_LocalToModel = dmTransform::Mul(m_BindPose[bone->m_Parent].m_LocalToModel, bind_bone->m_LocalToParent);
                if (!bone->m_InheritScale)
                {
                    bind_bone->m_LocalToModel.SetScale(bind_bone->m_LocalToParent.GetScale());
                }
            }
            else
            {
                bind_bone->m_LocalToModel = bind_bone->m_LocalToParent;
            }
            bind_bone->m_ModelToLocal = dmTransform::Inv(bind_bone->m_LocalToModel);
            bind_bone->m_ParentIndex = bone->m_Parent;
            bind_bone->m_Length = bone->m_Length;
        }

        // Bone animations
        uint32_t animation_count = 1;
        m_AnimationSet->m_Animations.m_Data = new dmRigDDF::RigAnimation[animation_count];
        m_AnimationSet->m_Animations.m_Count = animation_count;
        dmRigDDF::RigAnimation& anim0 = m_AnimationSet->m_Animations.m_Data[0];
        anim0.m_Id = dmHashString64("valid");
        anim0.m_Duration            = 3.0f;
        anim0.m_SampleRate          = 1.0f;
        anim0.m_EventTracks.m_Count = 0;
        anim0.m_MeshTracks.m_Count  = 0;
        anim0.m_IkTracks.m_Count    = 0;

        uint32_t track_count = 2;
        anim0.m_Tracks.m_Data = new dmRigDDF::AnimationTrack[track_count];
        anim0.m_Tracks.m_Count = track_count;
        dmRigDDF::AnimationTrack& anim_track0 = anim0.m_Tracks.m_Data[0];
        dmRigDDF::AnimationTrack& anim_track1 = anim0.m_Tracks.m_Data[1];

        anim_track0.m_BoneIndex         = 0;
        anim_track0.m_Positions.m_Count = 0;
        anim_track0.m_Scale.m_Count     = 0;

        anim_track1.m_BoneIndex         = 1;
        anim_track1.m_Positions.m_Count = 0;
        anim_track1.m_Scale.m_Count     = 0;

        uint32_t samples = 4;
        anim_track0.m_Rotations.m_Data = new float[samples*4];
        anim_track0.m_Rotations.m_Count = samples*4;
        ((Quat*)anim_track0.m_Rotations.m_Data)[0] = Quat::identity();
        ((Quat*)anim_track0.m_Rotations.m_Data)[1] = Quat::identity();
        ((Quat*)anim_track0.m_Rotations.m_Data)[2] = Quat::rotationZ((float)M_PI / 2.0f);
        ((Quat*)anim_track0.m_Rotations.m_Data)[3] = Quat::rotationZ((float)M_PI / 2.0f);

        anim_track1.m_Rotations.m_Data = new float[samples*4];
        anim_track1.m_Rotations.m_Count = samples*4;
        ((Quat*)anim_track1.m_Rotations.m_Data)[0] = Quat::identity();
        ((Quat*)anim_track1.m_Rotations.m_Data)[1] = Quat::rotationZ((float)M_PI / 2.0f);
        ((Quat*)anim_track1.m_Rotations.m_Data)[2] = Quat::identity();
        ((Quat*)anim_track1.m_Rotations.m_Data)[3] = Quat::identity();

        /*

            Keyframe I:
            (0)---->(1)---->(x)

            Keyframe II:
            (0)---->(1)
                     |
                     |
                     v
                    (x)

            Keyframe III:
            (0)
             |
             |
             v
            (1)---->(x)

        */
    }

    void TearDownSimpleSpine() {
        delete [] m_AnimationSet->m_Animations.m_Data[0].m_Tracks.m_Data[1].m_Rotations.m_Data;
        delete [] m_AnimationSet->m_Animations.m_Data[0].m_Tracks.m_Data[0].m_Rotations.m_Data;
        delete [] m_AnimationSet->m_Animations.m_Data[0].m_Tracks.m_Data;
        delete [] m_AnimationSet->m_Animations.m_Data;
        delete [] m_Skeleton->m_Bones.m_Data;
    }

protected:
    virtual void SetUp() {
        RigContextTest::SetUp();

        m_Instance = 0x0;
        dmRig::InstanceCreateParams create_params;
        create_params.m_Context = m_Context;
        create_params.m_Instance = &m_Instance;

        m_Skeleton     = new dmRigDDF::Skeleton();
        m_MeshSet      = new dmRigDDF::MeshSet();
        m_AnimationSet = new dmRigDDF::AnimationSet();
        SetUpSimpleSpine();

        // Data
        create_params.m_BindPose     = &m_BindPose;
        create_params.m_Skeleton     = m_Skeleton;
        create_params.m_MeshSet      = m_MeshSet;
        create_params.m_AnimationSet = m_AnimationSet;

        create_params.m_SkinId           = (const char*)"dummy";
        create_params.m_DefaultAnimation = (const char*)"";

        if (dmRig::CREATE_RESULT_OK != dmRig::InstanceCreate(create_params)) {
            dmLogError("Could not create rig instance!");
        }
    }

    virtual void TearDown() {
        dmRig::InstanceDestroyParams destroy_params;
        destroy_params.m_Context = m_Context;
        destroy_params.m_Instance = m_Instance;
        if (dmRig::CREATE_RESULT_OK != dmRig::InstanceDestroy(destroy_params)) {
            dmLogError("Could not delete rig instance!");
        }

        TearDownSimpleSpine();
        delete m_Skeleton;
        delete m_MeshSet;
        delete m_AnimationSet;

        RigContextTest::TearDown();
    }
};

TEST_F(RigContextTest, InstanceCreation)
{
    dmRig::HRigInstance instance = 0x0;
    dmRig::InstanceCreateParams create_params;
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

    dmRig::InstanceDestroyParams destroy_params;
    destroy_params.m_Context = m_Context;
    destroy_params.m_Instance = instance;
    ASSERT_EQ(dmRig::CREATE_RESULT_OK, dmRig::InstanceDestroy(destroy_params));
}

TEST_F(RigContextTest, UpdateEmptyContext)
{
    dmRig::UpdateResult res = dmRig::Update(m_Context, 1.0/60.0);
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, res);
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
}

TEST_F(RigInstanceTest, PlayValidAnimation)
{
    dmhash_t valid_anim_id = dmHashString64("valid");

    // Default animation does not exist
    ASSERT_EQ(dmRig::PLAY_RESULT_OK, dmRig::PlayAnimation(m_Instance, valid_anim_id, dmGameObject::PLAYBACK_LOOP_FORWARD, 0.0f));
    ASSERT_EQ(valid_anim_id, dmRig::GetAnimation(m_Instance));

    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::Update(m_Context, 1.0/60.0));
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

    const dmArray<dmTransform::Transform>& pose = dmRig::GetPose(m_Instance);

    // should be same as bind pose
    ASSERT_VEC3(Vector3(0.0f), pose[0].GetTranslation());
    ASSERT_VEC3(Vector3(1.0f, 0.0f, 0.0f), pose[1].GetTranslation());
    ASSERT_VEC4(Quat::identity(), pose[0].GetRotation());
    ASSERT_VEC4(Quat::identity(), pose[1].GetRotation());

    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::Update(m_Context, 1.0/60.0));

    ASSERT_VEC3(Vector3(0.0f), pose[0].GetTranslation());
    ASSERT_VEC3(Vector3(1.0f, 0.0f, 0.0f), pose[1].GetTranslation());
    ASSERT_VEC4(Quat::identity(), pose[0].GetRotation());
    ASSERT_VEC4(Quat::identity(), pose[1].GetRotation());
}

TEST_F(RigInstanceTest, PoseAnim)
{
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::PLAY_RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmGameObject::PLAYBACK_LOOP_FORWARD, 0.0f));

    const dmArray<dmTransform::Transform>& pose = dmRig::GetPose(m_Instance);

    // sample 0
    ASSERT_VEC3(Vector3(0.0f), pose[0].GetTranslation());
    ASSERT_VEC3(Vector3(1.0f, 0.0f, 0.0f), pose[1].GetTranslation());
    ASSERT_VEC4(Quat::identity(), pose[0].GetRotation());
    ASSERT_VEC4(Quat::identity(), pose[1].GetRotation());

    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::Update(m_Context, 1.0f));

    // sample 1
    ASSERT_VEC3(Vector3(0.0f), pose[0].GetTranslation());
    ASSERT_VEC3(Vector3(1.0f, 0.0f, 0.0f), pose[1].GetTranslation());
    ASSERT_VEC4(Quat::identity(), pose[0].GetRotation());
    ASSERT_VEC4(Quat::rotationZ((float)M_PI / 2.0f), pose[1].GetRotation());

    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::Update(m_Context, 1.0f));

    // sample 2
    ASSERT_VEC3(Vector3(0.0f), pose[0].GetTranslation());
    ASSERT_VEC3(Vector3(0.0f, 1.0f, 0.0f), pose[1].GetTranslation());
    ASSERT_VEC4(Quat::rotationZ((float)M_PI / 2.0f), pose[0].GetRotation());
    ASSERT_VEC4(Quat::rotationZ((float)M_PI / 2.0f), pose[1].GetRotation());


    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::Update(m_Context, 1.0f));

    // sample 0 (looped)
    ASSERT_VEC3(Vector3(0.0f), pose[0].GetTranslation());
    ASSERT_VEC3(Vector3(1.0f, 0.0f, 0.0f), pose[1].GetTranslation());
    ASSERT_VEC4(Quat::identity(), pose[0].GetRotation());
    ASSERT_VEC4(Quat::identity(), pose[1].GetRotation());
}

TEST_F(RigInstanceTest, PoseAnimCancel)
{
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::PLAY_RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmGameObject::PLAYBACK_LOOP_FORWARD, 0.0f));

    const dmArray<dmTransform::Transform>& pose = dmRig::GetPose(m_Instance);

    // sample 0
    ASSERT_VEC3(Vector3(0.0f), pose[0].GetTranslation());
    ASSERT_VEC3(Vector3(1.0f, 0.0f, 0.0f), pose[1].GetTranslation());
    ASSERT_VEC4(Quat::identity(), pose[0].GetRotation());
    ASSERT_VEC4(Quat::identity(), pose[1].GetRotation());

    ASSERT_EQ(dmRig::PLAY_RESULT_OK, dmRig::CancelAnimation(m_Instance));
    ASSERT_EQ(dmRig::UPDATE_RESULT_OK, dmRig::Update(m_Context, 1.0f));

    // sample 1
    ASSERT_VEC3(Vector3(0.0f), pose[0].GetTranslation());
    ASSERT_VEC3(Vector3(1.0f, 0.0f, 0.0f), pose[1].GetTranslation());
    ASSERT_VEC4(Quat::identity(), pose[0].GetRotation());
    ASSERT_VEC4(Quat::identity(), pose[1].GetRotation());

}

#undef ASSERT_VEC3
#undef ASSERT_VEC4

/*
    Left to test:
        GenerateVertexData(...)
        GetSkin(...)
        SetSkin(...)
        GetAnimation(...)
        SetAnimation(...)
        GetPose(...)
        GetCursor(...)
        SetCursor(...)
        SetIKTarget(...)
        SetIKTargetPosition(...)
*/

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
