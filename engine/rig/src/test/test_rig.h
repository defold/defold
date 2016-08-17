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
        if (dmRig::CREATE_RESULT_OK != dmRig::NewContext(params)) {
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

        create_params.m_SkinId           = (const char*)"test";
        create_params.m_DefaultAnimation = (const char*)"";

        if (dmRig::CREATE_RESULT_OK != dmRig::InstanceCreate(create_params)) {
            dmLogError("Could not create rig instance!");
        }
    }

    virtual void TearDown() {
        dmRig::InstanceDestroyParams destroy_params = {0};
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
