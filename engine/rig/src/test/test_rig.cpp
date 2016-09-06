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
        dmRig::NewContextParams params = {0};
        params.m_Context = &m_Context;
        params.m_MaxRigInstanceCount = 2;
        if (dmRig::RESULT_OK != dmRig::NewContext(params)) {
            dmLogError("Could not create rig context!");
        }
    }

    virtual void TearDown() {
        dmRig::DeleteContext(m_Context);
    }
};

static void CreateDummyMeshEntry(dmRigDDF::MeshEntry& mesh_entry, dmhash_t id, Vector4 color) {
    mesh_entry.m_Id = id;
    mesh_entry.m_Meshes.m_Data = new dmRigDDF::Mesh[1];
    mesh_entry.m_Meshes.m_Count = 1;

    uint32_t vert_count = 3;

    // set vertice position so they match bone positions
    dmRigDDF::Mesh& mesh = mesh_entry.m_Meshes.m_Data[0];
    mesh.m_Positions.m_Data = new float[vert_count*3];
    mesh.m_Positions.m_Count = vert_count*3;
    mesh.m_Positions.m_Data[0] = 0.0f;
    mesh.m_Positions.m_Data[1] = 0.0f;
    mesh.m_Positions.m_Data[2] = 0.0f;
    mesh.m_Positions.m_Data[3] = 1.0f;
    mesh.m_Positions.m_Data[4] = 0.0f;
    mesh.m_Positions.m_Data[5] = 0.0f;
    mesh.m_Positions.m_Data[6] = 2.0f;
    mesh.m_Positions.m_Data[7] = 0.0f;
    mesh.m_Positions.m_Data[8] = 0.0f;

    // data for each vertex (tex coords and normals not used)
    mesh.m_Texcoord0.m_Data       = new float[vert_count*2];
    mesh.m_Texcoord0.m_Count      = vert_count*2;
    mesh.m_Normals.m_Data         = new float[vert_count*3];
    mesh.m_Normals.m_Count        = vert_count*3;

    mesh.m_Color.m_Data           = new float[vert_count*4];
    mesh.m_Color.m_Count          = vert_count*4;
    mesh.m_Color[0]               = color.getX();
    mesh.m_Color[1]               = color.getY();
    mesh.m_Color[2]               = color.getZ();
    mesh.m_Color[3]               = color.getW();
    mesh.m_Color[4]               = color.getX();
    mesh.m_Color[5]               = color.getY();
    mesh.m_Color[6]               = color.getZ();
    mesh.m_Color[7]               = color.getW();
    mesh.m_Color[8]               = color.getX();
    mesh.m_Color[9]               = color.getY();
    mesh.m_Color[10]              = color.getZ();
    mesh.m_Color[11]              = color.getW();
    mesh.m_Indices.m_Data         = new uint32_t[vert_count];
    mesh.m_Indices.m_Count        = vert_count;
    mesh.m_Indices.m_Data[0]      = 0;
    mesh.m_Indices.m_Data[1]      = 1;
    mesh.m_Indices.m_Data[2]      = 2;
    mesh.m_BoneIndices.m_Data     = new uint32_t[vert_count*4];
    mesh.m_BoneIndices.m_Count    = vert_count*4;
    mesh.m_BoneIndices.m_Data[0]  = 0;
    mesh.m_BoneIndices.m_Data[1]  = 1;
    mesh.m_BoneIndices.m_Data[2]  = 0;
    mesh.m_BoneIndices.m_Data[3]  = 0;
    mesh.m_BoneIndices.m_Data[4]  = 0;
    mesh.m_BoneIndices.m_Data[5]  = 1;
    mesh.m_BoneIndices.m_Data[6]  = 0;
    mesh.m_BoneIndices.m_Data[7]  = 0;
    mesh.m_BoneIndices.m_Data[8]  = 0;
    mesh.m_BoneIndices.m_Data[9]  = 1;
    mesh.m_BoneIndices.m_Data[10] = 0;
    mesh.m_BoneIndices.m_Data[11] = 0;

    mesh.m_Weights.m_Data         = new float[vert_count*4];
    mesh.m_Weights.m_Count        = vert_count*4;
    mesh.m_Weights.m_Data[0]      = 1.0f;
    mesh.m_Weights.m_Data[1]      = 0.0f;
    mesh.m_Weights.m_Data[2]      = 0.0f;
    mesh.m_Weights.m_Data[3]      = 0.0f;
    mesh.m_Weights.m_Data[4]      = 0.0f;
    mesh.m_Weights.m_Data[5]      = 1.0f;
    mesh.m_Weights.m_Data[6]      = 0.0f;
    mesh.m_Weights.m_Data[7]      = 0.0f;
    mesh.m_Weights.m_Data[8]      = 0.0f;
    mesh.m_Weights.m_Data[9]      = 1.0f;
    mesh.m_Weights.m_Data[10]     = 0.0f;
    mesh.m_Weights.m_Data[11]     = 0.0f;

    mesh.m_Visible = true;
    mesh.m_DrawOrder = 0;
}

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

        /*

            Bones:
            A:
            (0)---->(1)---->
             |
         B:  |
             v
            (2)
             |
             |
             v
            (3)
             |
             |
             v

         A: 0: Pos; (0,0), rotation: 0
            1: Pos; (1,0), rotation: 0

         B: 0: Pos; (0,0), rotation: 0
            2: Pos; (0,1), rotation: 0
            3: Pos; (0,2), rotation: 0

        ------------------------------------

            Animation (id: "valid") for Bone A:

            I:
            (0)---->(1)---->

            II:
            (0)---->(1)
                     |
                     |
                     v

            III:
            (0)
             |
             |
             v
            (1)
             |
             |
             v


        ------------------------------------

            Animation (id: "ik_anim") for IK on Bone B.

        */

        uint32_t bone_count = 5;
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
        bone0.m_Length       = 0.0f;

        // Bone 1
        dmRigDDF::Bone& bone1 = m_Skeleton->m_Bones.m_Data[1];
        bone1.m_Parent       = 0;
        bone1.m_Id           = 1;
        bone1.m_Position     = Vectormath::Aos::Point3(1.0f, 0.0f, 0.0f);
        bone1.m_Rotation     = Vectormath::Aos::Quat::identity();
        bone1.m_Scale        = Vectormath::Aos::Vector3(1.0f, 1.0f, 1.0f);
        bone1.m_InheritScale = false;
        bone1.m_Length       = 1.0f;

        // Bone 2
        dmRigDDF::Bone& bone2 = m_Skeleton->m_Bones.m_Data[2];
        bone2.m_Parent       = 0;
        bone2.m_Id           = 2;
        bone2.m_Position     = Vectormath::Aos::Point3(0.0f, 1.0f, 0.0f);
        bone2.m_Rotation     = Vectormath::Aos::Quat::identity();
        bone2.m_Scale        = Vectormath::Aos::Vector3(1.0f, 1.0f, 1.0f);
        bone2.m_InheritScale = false;
        bone2.m_Length       = 1.0f;

        // Bone 3
        dmRigDDF::Bone& bone3 = m_Skeleton->m_Bones.m_Data[3];
        bone3.m_Parent       = 2;
        bone3.m_Id           = 3;
        bone3.m_Position     = Vectormath::Aos::Point3(0.0f, 1.0f, 0.0f);
        bone3.m_Rotation     = Vectormath::Aos::Quat::identity();
        bone3.m_Scale        = Vectormath::Aos::Vector3(1.0f, 1.0f, 1.0f);
        bone3.m_InheritScale = false;
        bone3.m_Length       = 1.0f;

        // Bone 4
        dmRigDDF::Bone& bone4 = m_Skeleton->m_Bones.m_Data[4];
        bone4.m_Parent       = 3;
        bone4.m_Id           = 4;
        bone4.m_Position     = Vectormath::Aos::Point3(0.0f, 1.0f, 0.0f);
        bone4.m_Rotation     = Vectormath::Aos::Quat::identity();
        bone4.m_Scale        = Vectormath::Aos::Vector3(1.0f, 1.0f, 1.0f);
        bone4.m_InheritScale = false;
        bone4.m_Length       = 1.0f;

        m_BindPose.SetCapacity(bone_count);
        m_BindPose.SetSize(bone_count);

        // IK
        m_Skeleton->m_Iks.m_Data = new dmRigDDF::IK[1];
        m_Skeleton->m_Iks.m_Count = 1;
        dmRigDDF::IK& ik_target = m_Skeleton->m_Iks.m_Data[0];
        ik_target.m_Id       = dmHashString64("test_ik");
        ik_target.m_Parent   = 3;
        ik_target.m_Child    = 2;
        ik_target.m_Target   = 4;
        ik_target.m_Positive = true;
        ik_target.m_Mix      = 1.0f;

        // Calculate bind pose
        // (code from res_rig_scene.h)
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
        uint32_t animation_count = 2;
        m_AnimationSet->m_Animations.m_Data = new dmRigDDF::RigAnimation[animation_count];
        m_AnimationSet->m_Animations.m_Count = animation_count;
        dmRigDDF::RigAnimation& anim0 = m_AnimationSet->m_Animations.m_Data[0];
        dmRigDDF::RigAnimation& anim1 = m_AnimationSet->m_Animations.m_Data[1];
        anim0.m_Id = dmHashString64("valid");
        anim0.m_Duration            = 3.0f;
        anim0.m_SampleRate          = 1.0f;
        anim0.m_EventTracks.m_Count = 0;
        anim0.m_MeshTracks.m_Count  = 0;
        anim0.m_IkTracks.m_Count    = 0;
        anim1.m_Id = dmHashString64("ik_anim");
        anim1.m_Duration            = 3.0f;
        anim1.m_SampleRate          = 1.0f;
        anim1.m_Tracks.m_Count      = 0;
        anim1.m_EventTracks.m_Count = 0;
        anim1.m_MeshTracks.m_Count  = 0;

        uint32_t bone_track_count = 2;
        anim0.m_Tracks.m_Data = new dmRigDDF::AnimationTrack[bone_track_count];
        anim0.m_Tracks.m_Count = bone_track_count;
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

        // IK animation
        anim1.m_IkTracks.m_Data = new dmRigDDF::IKAnimationTrack[1];
        anim1.m_IkTracks.m_Count = 1;
        dmRigDDF::IKAnimationTrack& ik_track = anim1.m_IkTracks.m_Data[0];
        ik_track.m_IkIndex = 0;
        ik_track.m_Mix.m_Data = new float[samples];
        ik_track.m_Mix.m_Count = samples;
        ik_track.m_Mix.m_Data[0] = 1.0f;
        ik_track.m_Mix.m_Data[1] = 1.0f;
        ik_track.m_Mix.m_Data[2] = 1.0f;
        ik_track.m_Mix.m_Data[3] = 1.0f;
        ik_track.m_Positive.m_Data = new bool[samples];
        ik_track.m_Positive.m_Count = samples;
        ik_track.m_Positive.m_Data[0] = 1.0f;
        ik_track.m_Positive.m_Data[1] = 1.0f;
        ik_track.m_Positive.m_Data[2] = 1.0f;
        ik_track.m_Positive.m_Data[3] = 1.0f;

        // Meshes / skins
        m_MeshSet->m_MeshEntries.m_Data = new dmRigDDF::MeshEntry[2];
        m_MeshSet->m_MeshEntries.m_Count = 2;

        CreateDummyMeshEntry(m_MeshSet->m_MeshEntries.m_Data[0], dmHashString64("test"), Vector4(0.0f));
        CreateDummyMeshEntry(m_MeshSet->m_MeshEntries.m_Data[1], dmHashString64("secondary_skin"), Vector4(1.0f));

    }

    void TearDownSimpleSpine() {

        delete [] m_AnimationSet->m_Animations.m_Data[1].m_IkTracks.m_Data[0].m_Positive.m_Data;
        delete [] m_AnimationSet->m_Animations.m_Data[1].m_IkTracks.m_Data[0].m_Mix.m_Data;
        delete [] m_AnimationSet->m_Animations.m_Data[1].m_IkTracks.m_Data;
        delete [] m_AnimationSet->m_Animations.m_Data[0].m_Tracks.m_Data[1].m_Rotations.m_Data;
        delete [] m_AnimationSet->m_Animations.m_Data[0].m_Tracks.m_Data[0].m_Rotations.m_Data;
        delete [] m_AnimationSet->m_Animations.m_Data[0].m_Tracks.m_Data;
        delete [] m_AnimationSet->m_Animations.m_Data;
        delete [] m_Skeleton->m_Bones.m_Data;
        delete [] m_Skeleton->m_Iks.m_Data;

        for (int i = 0; i < 2; ++i)
        {
            delete [] m_MeshSet->m_MeshEntries.m_Data[i].m_Meshes.m_Data[0].m_Normals.m_Data;
            delete [] m_MeshSet->m_MeshEntries.m_Data[i].m_Meshes.m_Data[0].m_BoneIndices.m_Data;
            delete [] m_MeshSet->m_MeshEntries.m_Data[i].m_Meshes.m_Data[0].m_Weights.m_Data;
            delete [] m_MeshSet->m_MeshEntries.m_Data[i].m_Meshes.m_Data[0].m_Indices.m_Data;
            delete [] m_MeshSet->m_MeshEntries.m_Data[i].m_Meshes.m_Data[0].m_Color.m_Data;
            delete [] m_MeshSet->m_MeshEntries.m_Data[i].m_Meshes.m_Data[0].m_Texcoord0.m_Data;
            delete [] m_MeshSet->m_MeshEntries.m_Data[i].m_Meshes.m_Data[0].m_Positions.m_Data;
            delete [] m_MeshSet->m_MeshEntries.m_Data[i].m_Meshes.m_Data;
        }
        delete [] m_MeshSet->m_MeshEntries.m_Data;
    }

protected:
    virtual void SetUp() {
        RigContextTest::SetUp();

        m_Instance = 0x0;
        dmRig::InstanceCreateParams create_params = {0};
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

        create_params.m_MeshId           = dmHashString64((const char*)"test");
        create_params.m_DefaultAnimation = dmHashString64((const char*)"");

        if (dmRig::RESULT_OK != dmRig::InstanceCreate(create_params)) {
            dmLogError("Could not create rig instance!");
        }
    }

    virtual void TearDown() {
        dmRig::InstanceDestroyParams destroy_params = {0};
        destroy_params.m_Context = m_Context;
        destroy_params.m_Instance = m_Instance;
        if (dmRig::RESULT_OK != dmRig::InstanceDestroy(destroy_params)) {
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
    dmRig::InstanceCreateParams create_params = {0};
    create_params.m_Context = m_Context;
    create_params.m_Instance = &instance;

    // Dummy data
    dmArray<dmRig::RigBone> bind_pose;
    create_params.m_BindPose     = &bind_pose;
    create_params.m_Skeleton     = new dmRigDDF::Skeleton();
    create_params.m_MeshSet      = new dmRigDDF::MeshSet();
    create_params.m_AnimationSet = new dmRigDDF::AnimationSet();

    create_params.m_MeshId           = dmHashString64((const char*)"dummy");
    create_params.m_DefaultAnimation = dmHashString64((const char*)"");

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::InstanceCreate(create_params));
    ASSERT_NE((dmRig::HRigInstance)0x0, instance);

    delete create_params.m_Skeleton;
    delete create_params.m_MeshSet;
    delete create_params.m_AnimationSet;

    dmRig::InstanceDestroyParams destroy_params = {0};
    destroy_params.m_Context = m_Context;
    destroy_params.m_Instance = instance;
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::InstanceDestroy(destroy_params));
}

TEST_F(RigContextTest, InvalidInstanceCreation)
{
    dmRig::HRigInstance instance0 = 0x0;
    dmRig::HRigInstance instance1 = 0x0;
    dmRig::HRigInstance instance2 = 0x0;
    dmArray<dmRig::RigBone> bind_pose;

    dmRig::InstanceCreateParams create_params = {0};
    create_params.m_Context = m_Context;
    create_params.m_BindPose     = &bind_pose;
    create_params.m_Skeleton     = new dmRigDDF::Skeleton();
    create_params.m_MeshSet      = new dmRigDDF::MeshSet();
    create_params.m_AnimationSet = new dmRigDDF::AnimationSet();
    create_params.m_MeshId             = dmHashString64((const char*)"dummy");
    create_params.m_DefaultAnimation = dmHashString64((const char*)"");

    create_params.m_Instance = &instance0;
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::InstanceCreate(create_params));
    ASSERT_NE((dmRig::HRigInstance)0x0, instance0);

    create_params.m_Instance = &instance1;
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::InstanceCreate(create_params));
    ASSERT_NE((dmRig::HRigInstance)0x0, instance1);

    create_params.m_Instance = &instance2;
    ASSERT_EQ(dmRig::RESULT_ERROR, dmRig::InstanceCreate(create_params));
    ASSERT_EQ((dmRig::HRigInstance)0x0, instance2);

    delete create_params.m_Skeleton;
    delete create_params.m_MeshSet;
    delete create_params.m_AnimationSet;

    dmRig::InstanceDestroyParams destroy_params = {0};
    destroy_params.m_Context = m_Context;
    destroy_params.m_Instance = instance0;
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::InstanceDestroy(destroy_params));

    destroy_params.m_Instance = instance1;
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::InstanceDestroy(destroy_params));

    destroy_params.m_Instance = instance2;
    ASSERT_EQ(dmRig::RESULT_ERROR, dmRig::InstanceDestroy(destroy_params));
}

TEST_F(RigContextTest, UpdateEmptyContext)
{
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0/60.0));
}

TEST_F(RigInstanceTest, PlayInvalidAnimation)
{
    dmhash_t invalid_anim_id = dmHashString64("invalid");
    dmhash_t empty_id = dmHashString64("");

    // Default animation does not exist
    ASSERT_EQ(dmRig::RESULT_ANIM_NOT_FOUND, dmRig::PlayAnimation(m_Instance, invalid_anim_id, dmGameObject::PLAYBACK_LOOP_FORWARD, 0.0f));
    ASSERT_NE(invalid_anim_id, dmRig::GetAnimation(m_Instance));
    ASSERT_EQ(empty_id, dmRig::GetAnimation(m_Instance));

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0/60.0));
}

TEST_F(RigInstanceTest, PlayValidAnimation)
{
    dmhash_t valid_anim_id = dmHashString64("valid");

    // Default animation does not exist
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, valid_anim_id, dmGameObject::PLAYBACK_LOOP_FORWARD, 0.0f));
    ASSERT_EQ(valid_anim_id, dmRig::GetAnimation(m_Instance));

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0/60.0));
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
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0/60.0));

    dmArray<dmTransform::Transform>& pose = *dmRig::GetPose(m_Instance);

    // should be same as bind pose
    ASSERT_VEC3(Vector3(0.0f), pose[0].GetTranslation());
    ASSERT_VEC3(Vector3(1.0f, 0.0f, 0.0f), pose[1].GetTranslation());
    ASSERT_VEC4(Quat::identity(), pose[0].GetRotation());
    ASSERT_VEC4(Quat::identity(), pose[1].GetRotation());

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0/60.0));

    ASSERT_VEC3(Vector3(0.0f), pose[0].GetTranslation());
    ASSERT_VEC3(Vector3(1.0f, 0.0f, 0.0f), pose[1].GetTranslation());
    ASSERT_VEC4(Quat::identity(), pose[0].GetRotation());
    ASSERT_VEC4(Quat::identity(), pose[1].GetRotation());
}

TEST_F(RigInstanceTest, PoseAnim)
{
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmGameObject::PLAYBACK_LOOP_FORWARD, 0.0f));

    dmArray<dmTransform::Transform>& pose = *dmRig::GetPose(m_Instance);

    // sample 0
    ASSERT_VEC3(Vector3(0.0f), pose[0].GetTranslation());
    ASSERT_VEC3(Vector3(1.0f, 0.0f, 0.0f), pose[1].GetTranslation());
    ASSERT_VEC4(Quat::identity(), pose[0].GetRotation());
    ASSERT_VEC4(Quat::identity(), pose[1].GetRotation());

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));

    // sample 1
    ASSERT_VEC3(Vector3(0.0f), pose[0].GetTranslation());
    ASSERT_VEC3(Vector3(1.0f, 0.0f, 0.0f), pose[1].GetTranslation());
    ASSERT_VEC4(Quat::identity(), pose[0].GetRotation());
    ASSERT_VEC4(Quat::rotationZ((float)M_PI / 2.0f), pose[1].GetRotation());

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));

    // sample 2
    ASSERT_VEC3(Vector3(0.0f), pose[0].GetTranslation());
    ASSERT_VEC3(Vector3(0.0f, 1.0f, 0.0f), pose[1].GetTranslation());
    ASSERT_VEC4(Quat::rotationZ((float)M_PI / 2.0f), pose[0].GetRotation());
    ASSERT_VEC4(Quat::rotationZ((float)M_PI / 2.0f), pose[1].GetRotation());


    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));

    // sample 0 (looped)
    ASSERT_VEC3(Vector3(0.0f), pose[0].GetTranslation());
    ASSERT_VEC3(Vector3(1.0f, 0.0f, 0.0f), pose[1].GetTranslation());
    ASSERT_VEC4(Quat::identity(), pose[0].GetRotation());
    ASSERT_VEC4(Quat::identity(), pose[1].GetRotation());
}

TEST_F(RigInstanceTest, PoseAnimCancel)
{
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmGameObject::PLAYBACK_LOOP_FORWARD, 0.0f));

    dmArray<dmTransform::Transform>& pose = *dmRig::GetPose(m_Instance);

    // sample 0
    ASSERT_VEC3(Vector3(0.0f), pose[0].GetTranslation());
    ASSERT_VEC3(Vector3(1.0f, 0.0f, 0.0f), pose[1].GetTranslation());
    ASSERT_VEC4(Quat::identity(), pose[0].GetRotation());
    ASSERT_VEC4(Quat::identity(), pose[1].GetRotation());

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::CancelAnimation(m_Instance));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));

    // sample 1
    ASSERT_VEC3(Vector3(0.0f), pose[0].GetTranslation());
    ASSERT_VEC3(Vector3(1.0f, 0.0f, 0.0f), pose[1].GetTranslation());
    ASSERT_VEC4(Quat::identity(), pose[0].GetRotation());
    ASSERT_VEC4(Quat::identity(), pose[1].GetRotation());

}

TEST_F(RigInstanceTest, GetVertexCount)
{
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(3, dmRig::GetVertexCount(m_Instance));
}

TEST_F(RigInstanceTest, GenerateVertexData)
{
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmGameObject::PLAYBACK_LOOP_FORWARD, 0.0f));

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
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(data + 3, dmRig::GenerateVertexData(m_Context, m_Instance, params));
    ASSERT_VEC3(Vector3(0.0f), Vector3(data[0].x, data[0].y, data[0].z));            // v0
    ASSERT_VEC3(Vector3(1.0f, 0.0f, 0.0), Vector3(data[1].x, data[1].y, data[1].z)); // v1
    ASSERT_VEC3(Vector3(1.0f, 1.0f, 0.0), Vector3(data[2].x, data[2].y, data[2].z)); // v2

    // sample 2
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(data + 3, dmRig::GenerateVertexData(m_Context, m_Instance, params));
    ASSERT_VEC3(Vector3(0.0f), Vector3(data[0].x, data[0].y, data[0].z));            // v0
    ASSERT_VEC3(Vector3(0.0f, 1.0f, 0.0), Vector3(data[1].x, data[1].y, data[1].z)); // v1
    ASSERT_VEC3(Vector3(0.0f, 2.0f, 0.0), Vector3(data[2].x, data[2].y, data[2].z)); // v2

    delete [] data;
}

TEST_F(RigInstanceTest, SetMeshInvalid)
{
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmGameObject::PLAYBACK_LOOP_FORWARD, 0.0f));

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

    dmhash_t new_mesh = dmHashString64("not_a_valid_skin");
    ASSERT_EQ(dmRig::RESULT_ERROR, dmRig::SetMesh(m_Instance, new_mesh));
    ASSERT_EQ(dmHashString64("test"), dmRig::GetMesh(m_Instance));

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(data + 3, dmRig::GenerateVertexData(m_Context, m_Instance, params));
    ASSERT_EQ(0, data[0].r);
    ASSERT_EQ(0, data[0].g);
    ASSERT_EQ(0, data[0].b);
    ASSERT_EQ(0, data[0].a);

    delete [] data;
}


TEST_F(RigInstanceTest, SetMeshValid)
{
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmGameObject::PLAYBACK_LOOP_FORWARD, 0.0f));

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

    dmhash_t new_mesh = dmHashString64("secondary_skin");
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::SetMesh(m_Instance, new_mesh));
    ASSERT_EQ(new_mesh, dmRig::GetMesh(m_Instance));

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
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
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON);

    // no anim + set cursor
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON);
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::SetCursor(m_Instance, 100.0f, false));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON);

}

TEST_F(RigInstanceTest, CursorGet)
{
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmGameObject::PLAYBACK_LOOP_FORWARD, 0.0f));

    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON);
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON);
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_NEAR(1.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON);
    ASSERT_NEAR(1.0f / 3.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON);

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_NEAR(2.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON);
    ASSERT_NEAR(2.0f / 3.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON);

    // "half a sample"
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 0.5f));
    ASSERT_NEAR(2.5f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON);
    ASSERT_NEAR(2.5f / 3.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON);

    // animation restarted/looped
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 0.5f));
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON);
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON);

}

TEST_F(RigInstanceTest, CursorSet)
{
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmGameObject::PLAYBACK_LOOP_FORWARD, 0.0f));

    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON);
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON);
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_NEAR(1.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON);
    ASSERT_NEAR(1.0f / 3.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON);

    ASSERT_NEAR(dmRig::RESULT_OK, dmRig::SetCursor(m_Instance, 0.0f, false), RIG_EPSILON);
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON);
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON);

    ASSERT_NEAR(dmRig::RESULT_OK, dmRig::SetCursor(m_Instance, 0.5f, false), RIG_EPSILON);
    ASSERT_NEAR(0.5f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON);
    ASSERT_NEAR(0.5f / 3.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON);

    ASSERT_NEAR(dmRig::RESULT_OK, dmRig::SetCursor(m_Instance, 0.0f, true), RIG_EPSILON);
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON);
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON);

    ASSERT_NEAR(dmRig::RESULT_OK, dmRig::SetCursor(m_Instance, 0.5f, true), RIG_EPSILON);
    ASSERT_NEAR(3.0f * 0.5f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON);
    ASSERT_NEAR(0.5f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON);

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_NEAR(2.5f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON);
    ASSERT_NEAR(2.5f / 3.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON);
}

TEST_F(RigInstanceTest, CursorSetOutside)
{
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmGameObject::PLAYBACK_LOOP_FORWARD, 0.0f));

    ASSERT_NEAR(dmRig::RESULT_OK, dmRig::SetCursor(m_Instance, 4.0f, false), RIG_EPSILON);
    ASSERT_NEAR(1.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON);

    ASSERT_NEAR(dmRig::RESULT_OK, dmRig::SetCursor(m_Instance, -4.0f, false), RIG_EPSILON);
    ASSERT_NEAR(2.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON);

    ASSERT_NEAR(dmRig::RESULT_OK, dmRig::SetCursor(m_Instance, 4.0f / 3.0f, true), RIG_EPSILON);
    ASSERT_NEAR(1.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON);

    ASSERT_NEAR(dmRig::RESULT_OK, dmRig::SetCursor(m_Instance, -4.0f / 3.0f, true), RIG_EPSILON);
    ASSERT_NEAR(2.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON);
}

static Vector3 IKTargetPositionCallback(void* user_data, void*)
{
    return *(Vector3*)user_data;
}

TEST_F(RigInstanceTest, InvalidIKTarget)
{
    // Getting invalid ik constraint
    ASSERT_EQ((dmRig::IKTarget*)0x0, dmRig::GetIKTarget(m_Instance, dmHashString64("invalid_ik_name")));
}

static Vector3 UpdateIKPositionCallback(dmRig::IKTarget* ik_target)
{
    return (Vector3)ik_target->m_Position;
}

TEST_F(RigInstanceTest, IKTarget)
{
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("ik_anim"), dmGameObject::PLAYBACK_LOOP_FORWARD, 0.0f));

    dmRig::IKTarget* target = dmRig::GetIKTarget(m_Instance, dmHashString64("test_ik"));
    ASSERT_NE((dmRig::IKTarget*)0x0, target);
    target->m_Callback = UpdateIKPositionCallback;
    target->m_Mix = 1.0f;
    target->m_Position = Vector3(0.0f, 100.0f, 0.0f);

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));

    dmArray<dmTransform::Transform>& pose = *dmRig::GetPose(m_Instance);

    ASSERT_VEC3(Vector3(0.0f, 0.0f, 0.0f), pose[0].GetTranslation());
    ASSERT_VEC3(Vector3(0.0f, 1.0f, 0.0f), pose[2].GetTranslation());
    ASSERT_VEC3(Vector3(-1.0f, 1.0f, 0.0f), pose[3].GetTranslation());
    ASSERT_VEC3(Vector3(-2.0f, 1.0f, 0.0f), pose[4].GetTranslation());
    ASSERT_VEC4(Quat::identity(), pose[0].GetRotation());
    ASSERT_VEC4(Quat::rotationZ((float)M_PI / 2.0f), pose[3].GetRotation());
    ASSERT_VEC4(Quat::rotationZ((float)M_PI / 2.0f), pose[4].GetRotation());

    target->m_Position.setX(100.0f);
    target->m_Position.setY(1.0f);

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 0.0f));

    ASSERT_VEC3(Vector3(0.0f, 0.0f, 0.0f), pose[0].GetTranslation());
    ASSERT_VEC3(Vector3(0.0f, 1.0f, 0.0f), pose[2].GetTranslation());
    ASSERT_VEC3(Vector3(-1.0f, 1.0f, 0.0f), pose[3].GetTranslation());
    ASSERT_VEC3(Vector3(-1.0f, 2.0f, 0.0f), pose[4].GetTranslation());
    ASSERT_VEC4(Quat::identity(), pose[0].GetRotation());
    ASSERT_VEC4(Quat::identity(), pose[3].GetRotation());
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
