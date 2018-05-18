#include <gtest/gtest.h>
#include <dlib/log.h>

#include <../rig.h>

#define RIG_EPSILON_FLOAT 0.0001f
#define RIG_EPSILON_BYTE (1.0f / 255.0f)

// Helper function to clean up / delete RigAnimation data
static void DeleteRigAnimation(dmRigDDF::RigAnimation& anim)
{
    for (uint32_t t = 0; t < anim.m_Tracks.m_Count; ++t) {
        dmRigDDF::AnimationTrack& anim_track = anim.m_Tracks.m_Data[t];

        if (anim_track.m_Positions.m_Count) {
            delete [] anim_track.m_Positions.m_Data;
        }
        if (anim_track.m_Rotations.m_Count) {
            delete [] anim_track.m_Rotations.m_Data;
        }
        if (anim_track.m_Scale.m_Count) {
            delete [] anim_track.m_Scale.m_Data;
        }
    }

    for (uint32_t t = 0; t < anim.m_IkTracks.m_Count; ++t) {
        dmRigDDF::IKAnimationTrack& anim_iktrack = anim.m_IkTracks.m_Data[t];

        if (anim_iktrack.m_Mix.m_Count) {
            delete [] anim_iktrack.m_Mix.m_Data;
        }
        if (anim_iktrack.m_Positive.m_Count) {
            delete [] anim_iktrack.m_Positive.m_Data;
        }
    }

    for (uint32_t t = 0; t < anim.m_MeshTracks.m_Count; ++t) {
        dmRigDDF::MeshAnimationTrack& anim_meshtrack = anim.m_MeshTracks.m_Data[t];
        if (anim_meshtrack.m_OrderOffset.m_Count) {
            delete [] anim_meshtrack.m_OrderOffset.m_Data;
        }
        if (anim_meshtrack.m_MeshAttachment.m_Count) {
            delete [] anim_meshtrack.m_MeshAttachment.m_Data;
        }
        if (anim_meshtrack.m_SlotColors.m_Count) {
            delete [] anim_meshtrack.m_SlotColors.m_Data;
        }
    }

    if (anim.m_Tracks.m_Count) {
        delete [] anim.m_Tracks.m_Data;
    }
    if (anim.m_IkTracks.m_Count) {
        delete [] anim.m_IkTracks.m_Data;
    }
    if (anim.m_MeshTracks.m_Count) {
        delete [] anim.m_MeshTracks.m_Data;
    }
}

// Helper function to clean up / delete MeshSet, Skeleton and AnimationSet data
static void DeleteRigData(dmRigDDF::MeshSet* mesh_set, dmRigDDF::Skeleton* skeleton, dmRigDDF::AnimationSet* animation_set)
{
    if (animation_set != 0x0)
    {
        for (int anim_idx = 0; anim_idx < animation_set->m_Animations.m_Count; ++anim_idx)
        {
            dmRigDDF::RigAnimation& anim = animation_set->m_Animations.m_Data[anim_idx];
            DeleteRigAnimation(anim);
        }
        delete [] animation_set->m_Animations.m_Data;
        delete animation_set;
    }

    if (skeleton != 0x0)
    {
        delete [] skeleton->m_Bones.m_Data;
        delete [] skeleton->m_Iks.m_Data;
        delete skeleton;
    }

    if (mesh_set != 0x0)
    {
        // Delete mesh attachments and their data
        uint32_t mesh_count = mesh_set->m_MeshAttachments.m_Count;
        for (int i = 0; i < mesh_count; ++i)
        {
            dmRigDDF::Mesh& mesh = mesh_set->m_MeshAttachments.m_Data[i];
            if (mesh.m_NormalsIndices.m_Count > 0)   { delete [] mesh.m_NormalsIndices.m_Data; }
            if (mesh.m_Normals.m_Count > 0)          { delete [] mesh.m_Normals.m_Data; }
            if (mesh.m_BoneIndices.m_Count > 0)      { delete [] mesh.m_BoneIndices.m_Data; }
            if (mesh.m_Weights.m_Count > 0)          { delete [] mesh.m_Weights.m_Data; }
            if (mesh.m_Indices.m_Count > 0)          { delete [] mesh.m_Indices.m_Data; }
            if (mesh.m_MeshColor.m_Count > 0)        { delete [] mesh.m_MeshColor.m_Data; }
            if (mesh.m_Texcoord0Indices.m_Count > 0) { delete [] mesh.m_Texcoord0Indices.m_Data; }
            if (mesh.m_Texcoord0.m_Count > 0)        { delete [] mesh.m_Texcoord0.m_Data; }
            if (mesh.m_Positions.m_Count > 0)        { delete [] mesh.m_Positions.m_Data; }
        }
        delete [] mesh_set->m_MeshAttachments.m_Data;

        // Delete mesh entries and their slot data
        uint32_t mesh_entry_count = mesh_set->m_MeshEntries.m_Count;
        for (int i = 0; i < mesh_entry_count; ++i)
        {
            dmRigDDF::MeshEntry& mesh_entry = mesh_set->m_MeshEntries.m_Data[i];
            uint32_t mesh_slot_count = mesh_entry.m_MeshSlots.m_Count;
            for (int j = 0; j < mesh_slot_count; j++) {
                dmRigDDF::MeshSlot& mesh_slot = mesh_entry.m_MeshSlots.m_Data[j];
                if (mesh_slot.m_MeshAttachments.m_Count > 0) { delete [] mesh_slot.m_MeshAttachments.m_Data; }
                if (mesh_slot.m_SlotColor.m_Count > 0) { delete [] mesh_slot.m_SlotColor.m_Data; }
            }
            delete [] mesh_set->m_MeshEntries.m_Data[i].m_MeshSlots.m_Data;

        }
        delete [] mesh_set->m_MeshEntries.m_Data;
        delete [] mesh_set->m_BoneList.m_Data;
        delete mesh_set;
    }
}

static void CreateDrawOrderMeshes(dmRigDDF::MeshSet* mesh_set, int mesh_entry_index, dmhash_t id)
{
    dmRigDDF::MeshEntry& mesh_entry = mesh_set->m_MeshEntries[mesh_entry_index];
    mesh_entry.m_Id = id;

    for (uint32_t i = 0; i < 5; ++i)
    {
        // set vertice position so they match bone positions
        int mesh_attachment_index = mesh_set->m_MeshAttachments.m_Count++;
        dmRigDDF::Mesh& mesh = mesh_set->m_MeshAttachments.m_Data[mesh_attachment_index];
        mesh.m_Positions.m_Data = new float[3];
        mesh.m_Positions.m_Count = 3;
        mesh.m_Positions.m_Data[0]  = (float)i;
        mesh.m_Positions.m_Data[1]  = (float)i;
        mesh.m_Positions.m_Data[2]  = (float)i;

        // data for each vertex (tex coords not used)
        mesh.m_Texcoord0.m_Data       = new float[2];
        mesh.m_Texcoord0.m_Count      = 2;
        mesh.m_Texcoord0.m_Data[0]    = 0.0f;
        mesh.m_Texcoord0.m_Data[1]    = 0.0f;
        mesh.m_Texcoord0Indices.m_Count = 0;

        mesh.m_MeshColor.m_Data           = new float[4];
        mesh.m_MeshColor.m_Count          = 4;
        mesh.m_MeshColor.m_Data[0]        = 0.0f;
        mesh.m_MeshColor.m_Data[1]        = 0.0f;
        mesh.m_MeshColor.m_Data[2]        = 0.0f;
        mesh.m_MeshColor.m_Data[3]        = 0.0f;

        mesh.m_Indices.m_Data         = new uint32_t[1];
        mesh.m_Indices.m_Count        = 1;
        mesh.m_Indices.m_Data[0]      = 0;

        mesh.m_Normals.m_Count        = 0;
        mesh.m_NormalsIndices.m_Count = 0;
        mesh.m_BoneIndices.m_Count    = 0;
        mesh.m_Weights.m_Count        = 0;

        // The generated mesh index is set in the following order:
        // Fromat: (X, Y) -> (slot index, attachment index)
        // [ (0,0), (0,1), (1,0), (1,1), (2,0) ]
        //
        // We are generating 5 meshes, this means all slots will have 2 mesh attachments available,
        // except the last slot (2), which will only have one attachment.
        dmRigDDF::MeshSlot& mesh_slot = mesh_entry.m_MeshSlots[i / 2];
        mesh_slot.m_MeshAttachments[i % 2] = mesh_attachment_index;
        mesh_slot.m_ActiveIndex = 0;
    }
}

static void CreateTestSkin(dmRigDDF::MeshSet* mesh_set, int mesh_entry_index, dmhash_t id, Vector4 color, Vector4 slot_color = Vector4(1.0f))
{
    dmRigDDF::MeshEntry& mesh_entry = mesh_set->m_MeshEntries.m_Data[mesh_entry_index];
    int mesh_attachment_index = mesh_set->m_MeshAttachments.m_Count++;
    dmRigDDF::Mesh& mesh = mesh_set->m_MeshAttachments.m_Data[mesh_attachment_index];

    mesh_entry.m_MeshSlots[0].m_MeshAttachments[0] = mesh_attachment_index;
    mesh_entry.m_MeshSlots[0].m_Id = 0;
    mesh_entry.m_MeshSlots[0].m_ActiveIndex = 0;
    mesh_entry.m_MeshSlots[0].m_SlotColor.m_Data = new float[4];
    mesh_entry.m_MeshSlots[0].m_SlotColor.m_Data[0] = slot_color.getX();
    mesh_entry.m_MeshSlots[0].m_SlotColor.m_Data[1] = slot_color.getY();
    mesh_entry.m_MeshSlots[0].m_SlotColor.m_Data[2] = slot_color.getZ();
    mesh_entry.m_MeshSlots[0].m_SlotColor.m_Data[3] = slot_color.getW();
    mesh_entry.m_MeshSlots[0].m_SlotColor.m_Count = 4;

    mesh_entry.m_Id = id;

    uint32_t vert_count = 4;
    // set vertice position so they match bone positions
    mesh.m_Positions.m_Data = new float[vert_count*3];
    mesh.m_Positions.m_Count = vert_count*3;
    mesh.m_Positions.m_Data[0]  = 0.0f;
    mesh.m_Positions.m_Data[1]  = 0.0f;
    mesh.m_Positions.m_Data[2]  = 0.0f;
    mesh.m_Positions.m_Data[3]  = 1.0f;
    mesh.m_Positions.m_Data[4]  = 0.0f;
    mesh.m_Positions.m_Data[5]  = 0.0f;
    mesh.m_Positions.m_Data[6]  = 2.0f;
    mesh.m_Positions.m_Data[7]  = 0.0f;
    mesh.m_Positions.m_Data[8]  = 0.0f;

    // Vert positioned at bone 2 origo
    mesh.m_Positions.m_Data[9]  = 1.0f;
    mesh.m_Positions.m_Data[10] = 2.0f;
    mesh.m_Positions.m_Data[11] = 0.0f;

    // data for each vertex
    mesh.m_Texcoord0.m_Data       = new float[vert_count*2];
    mesh.m_Texcoord0.m_Count      = vert_count*2;
    mesh.m_Texcoord0[0]           = -1.0;
    mesh.m_Texcoord0[1]           = 2.0;

    mesh.m_Texcoord0Indices.m_Data  = new uint32_t[vert_count];
    mesh.m_Texcoord0Indices.m_Count = vert_count;
    mesh.m_Texcoord0Indices[0]      = 0;
    mesh.m_Texcoord0Indices[1]      = 0;
    mesh.m_Texcoord0Indices[2]      = 0;
    mesh.m_Texcoord0Indices[3]      = 0;

    mesh.m_Normals.m_Data         = new float[vert_count*3];
    mesh.m_Normals.m_Count        = vert_count*3;
    mesh.m_Normals[0]             = 0.0;
    mesh.m_Normals[1]             = 1.0;
    mesh.m_Normals[2]             = 0.0;
    mesh.m_Normals[3]             = 0.0;
    mesh.m_Normals[4]             = 1.0;
    mesh.m_Normals[5]             = 0.0;
    mesh.m_Normals[6]             = 0.0;
    mesh.m_Normals[7]             = 1.0;
    mesh.m_Normals[8]             = 0.0;
    mesh.m_Normals[9]             = 0.0;
    mesh.m_Normals[10]            = 1.0;
    mesh.m_Normals[11]            = 0.0;

    mesh.m_NormalsIndices.m_Data    = new uint32_t[vert_count];
    mesh.m_NormalsIndices.m_Count   = vert_count;
    mesh.m_NormalsIndices.m_Data[0] = 0;
    mesh.m_NormalsIndices.m_Data[1] = 1;
    mesh.m_NormalsIndices.m_Data[2] = 2;
    mesh.m_NormalsIndices.m_Data[3] = 3;

    mesh.m_MeshColor.m_Data       = new float[vert_count*4];
    mesh.m_MeshColor.m_Count      = vert_count*4;
    mesh.m_MeshColor[0]           = color.getX();
    mesh.m_MeshColor[1]           = color.getY();
    mesh.m_MeshColor[2]           = color.getZ();
    mesh.m_MeshColor[3]           = color.getW();
    mesh.m_MeshColor[4]           = color.getX();
    mesh.m_MeshColor[5]           = color.getY();
    mesh.m_MeshColor[6]           = color.getZ();
    mesh.m_MeshColor[7]           = color.getW();
    mesh.m_MeshColor[8]           = color.getX();
    mesh.m_MeshColor[9]           = color.getY();
    mesh.m_MeshColor[10]          = color.getZ();
    mesh.m_MeshColor[11]          = color.getW();
    mesh.m_MeshColor[12]          = color.getX();
    mesh.m_MeshColor[13]          = color.getY();
    mesh.m_MeshColor[14]          = color.getZ();
    mesh.m_MeshColor[15]          = color.getW();
    mesh.m_Indices.m_Data         = new uint32_t[vert_count];
    mesh.m_Indices.m_Count        = vert_count;
    mesh.m_Indices.m_Data[0]      = 0;
    mesh.m_Indices.m_Data[1]      = 1;
    mesh.m_Indices.m_Data[2]      = 2;
    mesh.m_Indices.m_Data[3]      = 3;
    mesh.m_BoneIndices.m_Data     = new uint32_t[vert_count*4];
    mesh.m_BoneIndices.m_Count    = vert_count*4;

    // Bone indices are in reverse order here to test bone list in meshset.
    int bone_count = 6;
    mesh.m_BoneIndices.m_Data[0]  = bone_count-1;
    mesh.m_BoneIndices.m_Data[1]  = bone_count-2;
    mesh.m_BoneIndices.m_Data[2]  = bone_count-1;
    mesh.m_BoneIndices.m_Data[3]  = bone_count-1;

    mesh.m_BoneIndices.m_Data[4]  = bone_count-2;
    mesh.m_BoneIndices.m_Data[5]  = bone_count-1;
    mesh.m_BoneIndices.m_Data[6]  = bone_count-1;
    mesh.m_BoneIndices.m_Data[7]  = bone_count-1;

    mesh.m_BoneIndices.m_Data[8]  = bone_count-2;
    mesh.m_BoneIndices.m_Data[9]  = bone_count-1;
    mesh.m_BoneIndices.m_Data[10] = bone_count-1;
    mesh.m_BoneIndices.m_Data[11] = bone_count-1;

    mesh.m_BoneIndices.m_Data[12] = bone_count-3;
    mesh.m_BoneIndices.m_Data[13] = bone_count-1;
    mesh.m_BoneIndices.m_Data[14] = bone_count-1;
    mesh.m_BoneIndices.m_Data[15] = bone_count-1;

    mesh.m_Weights.m_Data         = new float[vert_count*4];
    mesh.m_Weights.m_Count        = vert_count*4;
    mesh.m_Weights.m_Data[0]      = 1.0f;
    mesh.m_Weights.m_Data[1]      = 0.0f;
    mesh.m_Weights.m_Data[2]      = 0.0f;
    mesh.m_Weights.m_Data[3]      = 0.0f;

    mesh.m_Weights.m_Data[4]      = 1.0f;
    mesh.m_Weights.m_Data[5]      = 0.0f;
    mesh.m_Weights.m_Data[6]      = 0.0f;
    mesh.m_Weights.m_Data[7]      = 0.0f;

    mesh.m_Weights.m_Data[8]      = 1.0f;
    mesh.m_Weights.m_Data[9]      = 0.0f;
    mesh.m_Weights.m_Data[10]     = 0.0f;
    mesh.m_Weights.m_Data[11]     = 0.0f;

    mesh.m_Weights.m_Data[12]     = 1.0f;
    mesh.m_Weights.m_Data[13]     = 0.0f;
    mesh.m_Weights.m_Data[14]     = 0.0f;
    mesh.m_Weights.m_Data[15]     = 0.0f;
}

void SetUpSimpleRig(dmArray<dmRig::RigBone>& bind_pose, dmRigDDF::Skeleton* skeleton, dmRigDDF::MeshSet* mesh_set, dmRigDDF::AnimationSet* animation_set, dmArray<uint32_t>& pose_idx_to_influence, dmArray<uint32_t>& track_idx_to_pose)
{

    /*

        Note:
            - Skeleton has a depth first bone hirarchy, as expected by the engine.
            - Bone indices in the influence/weight and animations are specified in
              reverse order, together with reversed bone lists to compensate for this.
              See rig_ddf.proto for detailed information on skeleton, meshset and animationset
              decoupling and usage of bone lists.

        ------------------------------------

        Bones:
            A:
            (0)---->(1,2)---->
             |        |
         B:  |        |
             v        |
            (3)       |
             |        |
             |        |
             v        v
            (4)
             |
             |
             v
            (5)

         A: 0: Pos; (0,0), rotation; 0
            1: Pos; (1,0), rotation; 0
            2: Pos; (1,0), rotation; 90, scale; (2, 1)

         B: 0: Pos; (0,0), rotation; 0
            3: Pos; (0,1), rotation; 0
            4: Pos; (0,2), rotation; 0

        ------------------------------------

            Animation 0 (id: "valid") for Bone A:

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

            Animation 1 (id: "scaling") for Bone A:

            I:
            (0)---->(1)---->

            II:
            (0) (scale 2x)
             |
             |
             |
             |
             v
             (1)---->


        ------------------------------------

            Animation 2 (id: "ik") for IK on Bone B.

        ------------------------------------

            Animation 3 (id: "invalid_bones") for IK on Bone B.

            Tries to animate a bone that does not exist.

        ------------------------------------

            Animation 4 (id: "rot_blend1")

                Has a "static" rotation of bone 1 -89 degrees on Z.

            Animation 5 (id: "rot_blend2")

                Has a "static" rotation of bone 1 +89 degrees on Z.

            Used to test bone rotation slerp.

        ------------------------------------

            Animation 6 (id: "trans_rot")

                Rotation and translation on bone 0.

            A:
                       ^
                       |
                       |
              x - - - (0)

            0: Pos; (10,0), rotation: 90 degrees around Z

        ------------------------------------

            Animation 7 (id: "mesh_colors")


        ------------------------------------

            Animation 8 (id: "draw_order_anim")

                Animates the draw order of "draw_order_skin".

                t0: Slot 0 (mesh 0) has a positive offset of 2
                t1: Slot 3 (mesh 4) has a negative offset of 2
                t2: Slot 2 (mesh 2) has a negative offset of 1


        ------------------------------------

            Animation 9 (id: "skin_and_slot_colors_anim")

                Animates the slot colors.


        ------------------------------------

            Animation 10 (id: "slot_attachments")

                Animate the slot attachment for slot 0, to attachment 1.
        */

        uint32_t bone_count = 6;
        skeleton->m_Bones.m_Data = new dmRigDDF::Bone[bone_count];
        skeleton->m_Bones.m_Count = bone_count;
        skeleton->m_LocalBoneScaling = true;

        // Bone 0
        dmRigDDF::Bone& bone0 = skeleton->m_Bones.m_Data[0];
        bone0.m_Parent       = 0xffff;
        bone0.m_Id           = 0;
        bone0.m_Position     = Point3(0.0f, 0.0f, 0.0f);
        bone0.m_Rotation     = Quat::identity();
        bone0.m_Scale        = Vector3(1.0f, 1.0f, 1.0f);
        bone0.m_InheritScale = true;
        bone0.m_Length       = 0.0f;

        // Bone 1
        dmRigDDF::Bone& bone1 = skeleton->m_Bones.m_Data[1];
        bone1.m_Parent       = 0;
        bone1.m_Id           = 1;
        bone1.m_Position     = Point3(1.0f, 0.0f, 0.0f);
        bone1.m_Rotation     = Quat::identity();
        bone1.m_Scale        = Vector3(1.0f, 1.0f, 1.0f);
        bone1.m_InheritScale = true;
        bone1.m_Length       = 1.0f;

        // Bone 2
        dmRigDDF::Bone& bone2 = skeleton->m_Bones.m_Data[2];
        bone2.m_Parent       = 0;
        bone2.m_Id           = 2;
        bone2.m_Position     = Point3(1.0f, 0.0f, 0.0f);
        bone2.m_Rotation     = Quat::rotationZ((float)M_PI / 2.0f);
        bone2.m_Scale        = Vector3(2.0f, 1.0f, 1.0f);
        bone2.m_InheritScale = true;
        bone2.m_Length       = 1.0f;

        dmRigDDF::Bone& bone3 = skeleton->m_Bones.m_Data[3];
        bone3.m_Parent       = 0;
        bone3.m_Id           = 3;
        bone3.m_Position     = Point3(0.0f, 1.0f, 0.0f);
        bone3.m_Rotation     = Quat::identity();
        bone3.m_Scale        = Vector3(1.0f, 1.0f, 1.0f);
        bone3.m_InheritScale = true;
        bone3.m_Length       = 1.0f;

        // Bone 4
        dmRigDDF::Bone& bone4 = skeleton->m_Bones.m_Data[4];
        bone4.m_Parent       = 3;
        bone4.m_Id           = 4;
        bone4.m_Position     = Point3(0.0f, 1.0f, 0.0f);
        bone4.m_Rotation     = Quat::identity();
        bone4.m_Scale        = Vector3(1.0f, 1.0f, 1.0f);
        bone4.m_InheritScale = true;
        bone4.m_Length       = 1.0f;

        // Bone 5
        dmRigDDF::Bone& bone5 = skeleton->m_Bones.m_Data[5];
        bone5.m_Parent       = 4;
        bone5.m_Id           = 5;
        bone5.m_Position     = Point3(0.0f, 1.0f, 0.0f);
        bone5.m_Rotation     = Quat::identity();
        bone5.m_Scale        = Vector3(1.0f, 1.0f, 1.0f);
        bone5.m_InheritScale = true;
        bone5.m_Length       = 1.0f;

        bind_pose.SetCapacity(bone_count);
        bind_pose.SetSize(bone_count);

        // IK
        skeleton->m_Iks.m_Data = new dmRigDDF::IK[1];
        skeleton->m_Iks.m_Count = 1;
        dmRigDDF::IK& ik_target = skeleton->m_Iks.m_Data[0];
        ik_target.m_Id       = dmHashString64("test_ik");
        ik_target.m_Parent   = 4;
        ik_target.m_Child    = 3;
        ik_target.m_Target   = 5;
        ik_target.m_Positive = true;
        ik_target.m_Mix      = 1.0f;

        // Calculate bind pose
        dmRig::CreateBindPose(*skeleton, bind_pose);

        // Bone animations
        uint32_t animation_count = 11;
        animation_set->m_Animations.m_Data = new dmRigDDF::RigAnimation[animation_count];
        animation_set->m_Animations.m_Count = animation_count;
        dmRigDDF::RigAnimation& anim0 = animation_set->m_Animations.m_Data[0];
        dmRigDDF::RigAnimation& anim1 = animation_set->m_Animations.m_Data[1];
        dmRigDDF::RigAnimation& anim2 = animation_set->m_Animations.m_Data[2];
        dmRigDDF::RigAnimation& anim3 = animation_set->m_Animations.m_Data[3];
        dmRigDDF::RigAnimation& anim4 = animation_set->m_Animations.m_Data[4];
        dmRigDDF::RigAnimation& anim5 = animation_set->m_Animations.m_Data[5];
        dmRigDDF::RigAnimation& anim6 = animation_set->m_Animations.m_Data[6];
        dmRigDDF::RigAnimation& anim7 = animation_set->m_Animations.m_Data[7];
        dmRigDDF::RigAnimation& anim8 = animation_set->m_Animations.m_Data[8];
        dmRigDDF::RigAnimation& anim9 = animation_set->m_Animations.m_Data[9];
        dmRigDDF::RigAnimation& anim10 = animation_set->m_Animations.m_Data[10];
        anim0.m_Id = dmHashString64("valid");
        anim0.m_Duration            = 3.0f;
        anim0.m_SampleRate          = 1.0f;
        anim0.m_EventTracks.m_Count = 0;
        anim0.m_MeshTracks.m_Count  = 0;
        anim0.m_IkTracks.m_Count    = 0;
        anim1.m_Id = dmHashString64("ik");
        anim1.m_Duration            = 3.0f;
        anim1.m_SampleRate          = 1.0f;
        anim1.m_Tracks.m_Count      = 0;
        anim1.m_EventTracks.m_Count = 0;
        anim1.m_MeshTracks.m_Count  = 0;
        anim2.m_Id = dmHashString64("scaling");
        anim2.m_Duration            = 2.0f;
        anim2.m_SampleRate          = 1.0f;
        anim2.m_EventTracks.m_Count = 0;
        anim2.m_MeshTracks.m_Count  = 0;
        anim2.m_IkTracks.m_Count    = 0;
        anim3.m_Id = dmHashString64("invalid_bones");
        anim3.m_Duration            = 3.0f;
        anim3.m_SampleRate          = 1.0f;
        anim3.m_EventTracks.m_Count = 0;
        anim3.m_MeshTracks.m_Count  = 0;
        anim3.m_IkTracks.m_Count    = 0;
        anim4.m_Id = dmHashString64("rot_blend1");
        anim4.m_Duration            = 1.0f;
        anim4.m_SampleRate          = 1.0f;
        anim4.m_EventTracks.m_Count = 0;
        anim4.m_MeshTracks.m_Count  = 0;
        anim4.m_IkTracks.m_Count    = 0;
        anim5.m_Id = dmHashString64("rot_blend2");
        anim5.m_Duration            = 1.0f;
        anim5.m_SampleRate          = 1.0f;
        anim5.m_EventTracks.m_Count = 0;
        anim5.m_MeshTracks.m_Count  = 0;
        anim5.m_IkTracks.m_Count    = 0;
        anim6.m_Id = dmHashString64("trans_rot");
        anim6.m_Duration            = 1.0f;
        anim6.m_SampleRate          = 1.0f;
        anim6.m_EventTracks.m_Count = 0;
        anim6.m_MeshTracks.m_Count  = 0;
        anim6.m_IkTracks.m_Count    = 0;
        anim7.m_Id = dmHashString64("mesh_colors");
        anim7.m_Duration            = 3.0f;
        anim7.m_SampleRate          = 1.0f;
        anim7.m_EventTracks.m_Count = 0;
        anim7.m_Tracks.m_Count      = 0;
        anim7.m_IkTracks.m_Count    = 0;
        anim8.m_Id = dmHashString64("draw_order_anim");
        anim8.m_Duration            = 3.0f;
        anim8.m_SampleRate          = 1.0f;
        anim8.m_EventTracks.m_Count = 0;
        anim8.m_Tracks.m_Count      = 0;
        anim8.m_IkTracks.m_Count    = 0;
        anim9.m_Id = dmHashString64("skin_and_slot_colors_anim");
        anim9.m_Duration            = 3.0f;
        anim9.m_SampleRate          = 1.0f;
        anim9.m_EventTracks.m_Count = 0;
        anim9.m_Tracks.m_Count      = 0;
        anim9.m_IkTracks.m_Count    = 0;
        anim10.m_Id = dmHashString64("slot_attachments");
        anim10.m_Duration            = 0.0f;
        anim10.m_SampleRate          = 1.0f;
        anim10.m_EventTracks.m_Count = 0;
        anim10.m_Tracks.m_Count      = 0;
        anim10.m_IkTracks.m_Count    = 0;

        // Animation 0: "valid"
        {
            uint32_t track_count = 2;
            anim0.m_Tracks.m_Data = new dmRigDDF::AnimationTrack[track_count];
            anim0.m_Tracks.m_Count = track_count;
            dmRigDDF::AnimationTrack& anim_track0 = anim0.m_Tracks.m_Data[0];
            dmRigDDF::AnimationTrack& anim_track1 = anim0.m_Tracks.m_Data[1];

            anim_track0.m_BoneIndex         = 5;
            anim_track0.m_Positions.m_Count = 0;
            anim_track0.m_Scale.m_Count     = 0;

            anim_track1.m_BoneIndex         = 4;
            anim_track1.m_Positions.m_Count = 0;
            anim_track1.m_Scale.m_Count     = 0;

            uint32_t samples = 5;
            anim_track0.m_Rotations.m_Data = new float[samples*4];
            anim_track0.m_Rotations.m_Count = samples*4;
            ((Quat*)anim_track0.m_Rotations.m_Data)[0] = Quat::identity();
            ((Quat*)anim_track0.m_Rotations.m_Data)[1] = Quat::identity();
            ((Quat*)anim_track0.m_Rotations.m_Data)[2] = Quat::rotationZ((float)M_PI / 2.0f);
            ((Quat*)anim_track0.m_Rotations.m_Data)[3] = Quat::rotationZ((float)M_PI / 2.0f);
            ((Quat*)anim_track0.m_Rotations.m_Data)[4] = Quat::rotationZ((float)M_PI / 2.0f);

            anim_track1.m_Rotations.m_Data = new float[samples*4];
            anim_track1.m_Rotations.m_Count = samples*4;
            ((Quat*)anim_track1.m_Rotations.m_Data)[0] = Quat::identity();
            ((Quat*)anim_track1.m_Rotations.m_Data)[1] = Quat::rotationZ((float)M_PI / 2.0f);
            ((Quat*)anim_track1.m_Rotations.m_Data)[2] = Quat::identity();
            ((Quat*)anim_track1.m_Rotations.m_Data)[3] = Quat::identity();
            ((Quat*)anim_track1.m_Rotations.m_Data)[4] = Quat::identity();
        }

        // Animation 1: "ik"
        {
            uint32_t samples = 4;
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
        }

        // Animation 2: "scaling"
        {
            uint32_t track_count = 3; // 2x rotation, 1x scale
            anim2.m_Tracks.m_Data = new dmRigDDF::AnimationTrack[track_count];
            anim2.m_Tracks.m_Count = track_count;
            dmRigDDF::AnimationTrack& anim_track_b0_rot   = anim2.m_Tracks.m_Data[0];
            dmRigDDF::AnimationTrack& anim_track_b0_scale = anim2.m_Tracks.m_Data[1];
            dmRigDDF::AnimationTrack& anim_track_b1_rot   = anim2.m_Tracks.m_Data[2];

            anim_track_b0_rot.m_BoneIndex         = 5;
            anim_track_b0_rot.m_Positions.m_Count = 0;
            anim_track_b0_rot.m_Scale.m_Count     = 0;

            anim_track_b0_scale.m_BoneIndex         = 5;
            anim_track_b0_scale.m_Rotations.m_Count = 0;
            anim_track_b0_scale.m_Positions.m_Count = 0;

            anim_track_b1_rot.m_BoneIndex         = 4;
            anim_track_b1_rot.m_Positions.m_Count = 0;
            anim_track_b1_rot.m_Scale.m_Count     = 0;

            uint32_t samples = 3;
            anim_track_b0_rot.m_Rotations.m_Data = new float[samples*4];
            anim_track_b0_rot.m_Rotations.m_Count = samples*4;
            ((Quat*)anim_track_b0_rot.m_Rotations.m_Data)[0] = Quat::identity();
            ((Quat*)anim_track_b0_rot.m_Rotations.m_Data)[1] = Quat::rotationZ((float)M_PI / 2.0f);
            ((Quat*)anim_track_b0_rot.m_Rotations.m_Data)[2] = Quat::rotationZ((float)M_PI / 2.0f);

            anim_track_b0_scale.m_Scale.m_Data = new float[samples*3];
            anim_track_b0_scale.m_Scale.m_Count = samples*3;
            anim_track_b0_scale.m_Scale.m_Data[0] = 1.0f;
            anim_track_b0_scale.m_Scale.m_Data[1] = 1.0f;
            anim_track_b0_scale.m_Scale.m_Data[2] = 1.0f;
            anim_track_b0_scale.m_Scale.m_Data[3] = 2.0f;
            anim_track_b0_scale.m_Scale.m_Data[4] = 1.0f;
            anim_track_b0_scale.m_Scale.m_Data[5] = 1.0f;
            anim_track_b0_scale.m_Scale.m_Data[6] = 2.0f;
            anim_track_b0_scale.m_Scale.m_Data[7] = 1.0f;
            anim_track_b0_scale.m_Scale.m_Data[8] = 1.0f;

            anim_track_b1_rot.m_Rotations.m_Data = new float[samples*4];
            anim_track_b1_rot.m_Rotations.m_Count = samples*4;
            ((Quat*)anim_track_b1_rot.m_Rotations.m_Data)[0] = Quat::identity();
            ((Quat*)anim_track_b1_rot.m_Rotations.m_Data)[1] = Quat::rotationZ(-(float)M_PI / 2.0f);
            ((Quat*)anim_track_b1_rot.m_Rotations.m_Data)[2] = Quat::rotationZ(-(float)M_PI / 2.0f);
        }

        // Animation 3: "invalid_bones"
        {
            uint32_t track_count = 1;
            anim3.m_Tracks.m_Data = new dmRigDDF::AnimationTrack[track_count];
            anim3.m_Tracks.m_Count = track_count;
            dmRigDDF::AnimationTrack& anim_track0 = anim3.m_Tracks.m_Data[0];

            anim_track0.m_BoneIndex         = 6;
            anim_track0.m_Positions.m_Count = 0;
            anim_track0.m_Scale.m_Count     = 0;

            uint32_t samples = 4;
            anim_track0.m_Rotations.m_Data = new float[samples*4];
            anim_track0.m_Rotations.m_Count = samples*4;
            ((Quat*)anim_track0.m_Rotations.m_Data)[0] = Quat::identity();
            ((Quat*)anim_track0.m_Rotations.m_Data)[1] = Quat::identity();
            ((Quat*)anim_track0.m_Rotations.m_Data)[2] = Quat::rotationZ((float)M_PI / 2.0f);
            ((Quat*)anim_track0.m_Rotations.m_Data)[3] = Quat::rotationZ((float)M_PI / 2.0f);
        }

        // Animation 4: "rot_blend1"
        {
            uint32_t track_count = 1;
            anim4.m_Tracks.m_Data = new dmRigDDF::AnimationTrack[track_count];
            anim4.m_Tracks.m_Count = track_count;
            dmRigDDF::AnimationTrack& anim_track0 = anim4.m_Tracks.m_Data[0];

            anim_track0.m_BoneIndex         = 5;
            anim_track0.m_Positions.m_Count = 0;
            anim_track0.m_Scale.m_Count     = 0;

            uint32_t samples = 2;
            anim_track0.m_Rotations.m_Data = new float[samples*4];
            anim_track0.m_Rotations.m_Count = samples*4;
            float rot_angle = ((float)M_PI / 180.0f) * (-89.0f);
            ((Quat*)anim_track0.m_Rotations.m_Data)[0] = Quat::rotationZ(rot_angle);
            ((Quat*)anim_track0.m_Rotations.m_Data)[1] = Quat::rotationZ(rot_angle);
        }

        // Animation 5: "rot_blend2"
        {
            uint32_t track_count = 1;
            anim5.m_Tracks.m_Data = new dmRigDDF::AnimationTrack[track_count];
            anim5.m_Tracks.m_Count = track_count;
            dmRigDDF::AnimationTrack& anim_track0 = anim5.m_Tracks.m_Data[0];

            anim_track0.m_BoneIndex         = 5;
            anim_track0.m_Positions.m_Count = 0;
            anim_track0.m_Scale.m_Count     = 0;

            uint32_t samples = 2;
            anim_track0.m_Rotations.m_Data = new float[samples*4];
            anim_track0.m_Rotations.m_Count = samples*4;
            float rot_angle = ((float)M_PI / 180.0f) * (89.0f);
            ((Quat*)anim_track0.m_Rotations.m_Data)[0] = Quat::rotationZ(rot_angle);
            ((Quat*)anim_track0.m_Rotations.m_Data)[1] = Quat::rotationZ(rot_angle);
        }

        // Animation 6: "trans_rot"
        {
            uint32_t track_count = 2;
            uint32_t samples = 2;

            anim6.m_Tracks.m_Data = new dmRigDDF::AnimationTrack[track_count];
            anim6.m_Tracks.m_Count = track_count;
            dmRigDDF::AnimationTrack& anim_track0 = anim6.m_Tracks.m_Data[0];
            dmRigDDF::AnimationTrack& anim_track1 = anim6.m_Tracks.m_Data[1];

            anim_track0.m_BoneIndex         = 5;
            anim_track0.m_Positions.m_Count = 0;
            anim_track0.m_Scale.m_Count     = 0;

            anim_track0.m_Rotations.m_Data = new float[samples*4];
            anim_track0.m_Rotations.m_Count = samples*4;
            ((Quat*)anim_track0.m_Rotations.m_Data)[0] = Quat::rotationZ((float)M_PI / 2.0f);
            ((Quat*)anim_track0.m_Rotations.m_Data)[1] = Quat::rotationZ((float)M_PI / 2.0f);

            anim_track1.m_BoneIndex         = 5;
            anim_track1.m_Rotations.m_Count = 0;
            anim_track1.m_Scale.m_Count     = 0;

            anim_track1.m_Positions.m_Data = new float[samples*3];
            anim_track1.m_Positions.m_Count = samples*3;
            anim_track1.m_Positions.m_Data[0] = 10.0f;
            anim_track1.m_Positions.m_Data[1] = 0.0f;
            anim_track1.m_Positions.m_Data[2] = 0.0f;
            anim_track1.m_Positions.m_Data[3] = 10.0f;
            anim_track1.m_Positions.m_Data[4] = 0.0f;
            anim_track1.m_Positions.m_Data[5] = 0.0f;
        }

        // Animation 7: "mesh_colors"
        {
            uint32_t track_count = 1;
            anim7.m_MeshTracks.m_Data = new dmRigDDF::MeshAnimationTrack[track_count];
            anim7.m_MeshTracks.m_Count = track_count;
            dmRigDDF::MeshAnimationTrack& anim_track0 = anim7.m_MeshTracks.m_Data[0];

            anim_track0.m_MeshSlot            = 0;
            anim_track0.m_OrderOffset.m_Count = 0;
            anim_track0.m_MeshAttachment.m_Count = 0;

            uint32_t samples = 4;
            anim_track0.m_SlotColors.m_Data = new float[samples*4];
            anim_track0.m_SlotColors.m_Count = samples*4;
            anim_track0.m_SlotColors.m_Data[0] = 1.0f;
            anim_track0.m_SlotColors.m_Data[1] = 0.5f;
            anim_track0.m_SlotColors.m_Data[2] = 0.0f;
            anim_track0.m_SlotColors.m_Data[3] = 1.0f;
            anim_track0.m_SlotColors.m_Data[4] = 0.0f;
            anim_track0.m_SlotColors.m_Data[5] = 0.5f;
            anim_track0.m_SlotColors.m_Data[6] = 1.0f;
            anim_track0.m_SlotColors.m_Data[7] = 0.5f;
            anim_track0.m_SlotColors.m_Data[8] = 0.0f;
            anim_track0.m_SlotColors.m_Data[9] = 0.5f;
            anim_track0.m_SlotColors.m_Data[10] = 1.0f;
            anim_track0.m_SlotColors.m_Data[11] = 0.5f;
            anim_track0.m_SlotColors.m_Data[12] = 0.0f;
            anim_track0.m_SlotColors.m_Data[13] = 0.5f;
            anim_track0.m_SlotColors.m_Data[14] = 1.0f;
            anim_track0.m_SlotColors.m_Data[15] = 0.5f;
        }

        // Animation 8: "draw_order_anim"
        {
            uint32_t track_count = 3;
            uint32_t samples = 4;

            anim8.m_MeshTracks.m_Data = new dmRigDDF::MeshAnimationTrack[track_count];
            anim8.m_MeshTracks.m_Count = track_count;

            // Three draw order tracks, one for each slot.
            // k0 - slot 0 is shifted to slot 2 (delta +2)
            // k1 - slot 2 is shifted to slot 0 (delta -2)
            // k2 - slot 1 is shifted to slot 0 (delta -1)
            // k3 - no draw order change, ie all marked as "unchanged"
            static const int SIG_UNCHANGED = 0x10cced;
            const int32_t track_entries[] = {
                //         k0,            k1,            k2,            k3
                            2, SIG_UNCHANGED, SIG_UNCHANGED, SIG_UNCHANGED, // slot 0
                SIG_UNCHANGED, SIG_UNCHANGED,            -1, SIG_UNCHANGED, // slot 1
                SIG_UNCHANGED,            -2, SIG_UNCHANGED, SIG_UNCHANGED, // slot 2
            };

            // Loop over all tracks and pick appropriet offset entries from list above.
            for (int i = 0; i < track_count; ++i)
            {
                dmRigDDF::MeshAnimationTrack& anim_track = anim8.m_MeshTracks.m_Data[i];
                anim_track.m_MeshSlot               = i;
                anim_track.m_SlotColors.m_Count     = 0;
                anim_track.m_MeshAttachment.m_Count = 0;

                anim_track.m_OrderOffset.m_Data = new int32_t[samples];
                anim_track.m_OrderOffset.m_Count = samples;
                for (int j = 0; j < samples; ++j)
                {
                    anim_track.m_OrderOffset.m_Data[j] = track_entries[i*samples+j];
                }
            }
        }

        // Animation 9: "skin_and_slot_colors_anim"
        {
            uint32_t track_count = 1;
            anim9.m_MeshTracks.m_Data = new dmRigDDF::MeshAnimationTrack[track_count];
            anim9.m_MeshTracks.m_Count = track_count;
            dmRigDDF::MeshAnimationTrack& anim_track0 = anim9.m_MeshTracks.m_Data[0];

            anim_track0.m_MeshSlot            = 0;
            anim_track0.m_OrderOffset.m_Count = 0;
            anim_track0.m_MeshAttachment.m_Count = 0;

            uint32_t samples = 4;
            anim_track0.m_SlotColors.m_Data = new float[samples*4];
            anim_track0.m_SlotColors.m_Count = samples*4;
            anim_track0.m_SlotColors.m_Data[0] = 1.0f;
            anim_track0.m_SlotColors.m_Data[1] = 0.5f;
            anim_track0.m_SlotColors.m_Data[2] = 0.0f;
            anim_track0.m_SlotColors.m_Data[3] = 1.0f;
            anim_track0.m_SlotColors.m_Data[4] = 0.0f;
            anim_track0.m_SlotColors.m_Data[5] = 0.5f;
            anim_track0.m_SlotColors.m_Data[6] = 1.0f;
            anim_track0.m_SlotColors.m_Data[7] = 0.5f;
            anim_track0.m_SlotColors.m_Data[8] = 0.0f;
            anim_track0.m_SlotColors.m_Data[9] = 0.5f;
            anim_track0.m_SlotColors.m_Data[10] = 1.0f;
            anim_track0.m_SlotColors.m_Data[11] = 0.5f;
            anim_track0.m_SlotColors.m_Data[12] = 0.0f;
            anim_track0.m_SlotColors.m_Data[13] = 0.5f;
            anim_track0.m_SlotColors.m_Data[14] = 1.0f;
            anim_track0.m_SlotColors.m_Data[15] = 0.5f;
        }

        // Animation 10: "slot_attachments"
        {
            uint32_t samples = 2;

            anim10.m_MeshTracks.m_Data = new dmRigDDF::MeshAnimationTrack[1];
            anim10.m_MeshTracks.m_Count = 1;

            // One track for attachment animation on slot 0
            // k0 - slot 0 gets attachment 1 (instead of 0)
            // k1 - same as k0
            dmRigDDF::MeshAnimationTrack& anim_track = anim10.m_MeshTracks.m_Data[0];
            anim_track.m_MeshSlot = 0;
            anim_track.m_SlotColors.m_Count = 0;
            anim_track.m_OrderOffset.m_Count = 0;
            anim_track.m_MeshAttachment.m_Data = new int32_t[samples];
            anim_track.m_MeshAttachment.m_Count = samples;
            anim_track.m_MeshAttachment.m_Data[0] = 1;
            anim_track.m_MeshAttachment.m_Data[1] = 1;

            // // Loop over all tracks and pick appropriet offset entries from list above.
            // for (int i = 0; i < track_count; ++i)
            // {
            //     dmRigDDF::MeshAnimationTrack& anim_track = anim10.m_MeshTracks.m_Data[i];
            //     anim_track.m_MeshSlot               = i;
            //     anim_track.m_SlotColors.m_Count     = 0;
            //     anim_track.m_MeshAttachment.m_Count = 0;

            //     anim_track.m_OrderOffset.m_Data = new int32_t[samples];
            //     anim_track.m_OrderOffset.m_Count = samples;
            //     for (int j = 0; j < samples; ++j)
            //     {
            //         anim_track.m_OrderOffset.m_Data[j] = track_entries[i*samples+j];
            //     }
            // }
        }

        // Meshes / skins
        mesh_set->m_SlotCount = 3;
        mesh_set->m_MeshEntries.m_Data = new dmRigDDF::MeshEntry[4];
        mesh_set->m_MeshEntries.m_Count = 4;

        // Every slot will get two attachment points.
        // Make all slot attachment point to -1, ie no meshes attached/visible
        for (int i = 0; i < mesh_set->m_MeshEntries.m_Count; i++) {
            mesh_set->m_MeshEntries.m_Data[i].m_MeshSlots.m_Data = new dmRigDDF::MeshSlot[mesh_set->m_SlotCount];
            mesh_set->m_MeshEntries.m_Data[i].m_MeshSlots.m_Count = mesh_set->m_SlotCount;

            for (int j = 0; j < mesh_set->m_SlotCount; j++) {
                mesh_set->m_MeshEntries.m_Data[i].m_MeshSlots.m_Data[j].m_Id = j;
                mesh_set->m_MeshEntries.m_Data[i].m_MeshSlots.m_Data[j].m_MeshAttachments.m_Data = new uint32_t[2];
                mesh_set->m_MeshEntries.m_Data[i].m_MeshSlots.m_Data[j].m_MeshAttachments.m_Count = 2;
                mesh_set->m_MeshEntries.m_Data[i].m_MeshSlots.m_Data[j].m_MeshAttachments.m_Data[0] = -1;
                mesh_set->m_MeshEntries.m_Data[i].m_MeshSlots.m_Data[j].m_MeshAttachments.m_Data[1] = -1;
                mesh_set->m_MeshEntries.m_Data[i].m_MeshSlots.m_Data[j].m_ActiveIndex = -1;
                mesh_set->m_MeshEntries.m_Data[i].m_MeshSlots.m_Data[j].m_SlotColor.m_Count = 0;
            }
        }
        mesh_set->m_MaxBoneCount = bone_count + 1;

        // Allocate mesh attachments, one for each regular skin, and 5 extra for "draw_order_skin".
        int available_mesh_count = 3 + 5;
        mesh_set->m_MeshAttachments.m_Data = new dmRigDDF::Mesh[available_mesh_count];
        mesh_set->m_MeshAttachments.m_Count = 0;

        // Create mesh data for skins
        CreateTestSkin(mesh_set, 0, dmHashString64("test"), Vector4(0.0f));
        CreateTestSkin(mesh_set, 1, dmHashString64("secondary_skin"), Vector4(1.0f));
        CreateTestSkin(mesh_set, 2, dmHashString64("skin_color"), Vector4(0.5f, 0.4f, 0.3f, 0.2f), Vector4(1.0f));
        CreateDrawOrderMeshes(mesh_set, 3, dmHashString64("draw_order_skin"));

        // For sanity, check that all the expected meshes were added.
        assert(mesh_set->m_MeshAttachments.m_Count == available_mesh_count);

        // We create bone lists for both the meshset and animationset,
        // that is in "inverted" order of the skeleton hirarchy.
        mesh_set->m_BoneList.m_Data = new uint64_t[bone_count];
        mesh_set->m_BoneList.m_Count = bone_count;
        animation_set->m_BoneList.m_Data = mesh_set->m_BoneList.m_Data;
        animation_set->m_BoneList.m_Count = bone_count;
        for (int i = 0; i < bone_count; ++i)
        {
            mesh_set->m_BoneList.m_Data[i] = bone_count-i-1;
        }

        dmRig::FillBoneListArrays(*mesh_set, *animation_set, *skeleton, track_idx_to_pose, pose_idx_to_influence);
}

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

template<typename T>
class RigContextCursorTest : public ::testing::TestWithParam<T>
{
public:
    dmRig::HRigContext m_Context;

    virtual void SetUp() {
        dmRig::NewContextParams params = {0};
        params.m_Context = &m_Context;
        params.m_MaxRigInstanceCount = 2;
        if (dmRig::RESULT_OK != dmRig::NewContext(params)) {
            dmLogError("Could not create rig context");
        }
    }

    virtual void TearDown() {
        dmRig::DeleteContext(m_Context);
    }
};

template<typename T>
class RigInstanceCursorTest : public RigContextCursorTest<T>
{
public:
    dmRig::HRigInstance     m_Instance;
    dmArray<dmRig::RigBone> m_BindPose;
    dmRigDDF::Skeleton*     m_Skeleton;
    dmRigDDF::MeshSet*      m_MeshSet;
    dmRigDDF::AnimationSet* m_AnimationSet;

    dmArray<uint32_t>       m_PoseIdxToInfluence;
    dmArray<uint32_t>       m_TrackIdxToPose;

protected:
    virtual void SetUp() {
        RigContextCursorTest<T>::SetUp();

        m_Instance = 0x0;
        dmRig::InstanceCreateParams create_params = {0};
        create_params.m_Context = RigContextCursorTest<T>::m_Context;
        create_params.m_Instance = &m_Instance;

        m_Skeleton     = new dmRigDDF::Skeleton();
        m_MeshSet      = new dmRigDDF::MeshSet();
        m_AnimationSet = new dmRigDDF::AnimationSet();
        SetUpSimpleRig(m_BindPose, m_Skeleton, m_MeshSet, m_AnimationSet, m_PoseIdxToInfluence, m_TrackIdxToPose);

        // Data
        create_params.m_BindPose     = &m_BindPose;
        create_params.m_Skeleton     = m_Skeleton;
        create_params.m_MeshSet      = m_MeshSet;
        create_params.m_AnimationSet = m_AnimationSet;
        create_params.m_TrackIdxToPose     = &m_TrackIdxToPose;
        create_params.m_PoseIdxToInfluence = &m_PoseIdxToInfluence;

        create_params.m_MeshId           = dmHashString64((const char*)"test");
        create_params.m_DefaultAnimation = dmHashString64((const char*)"");

        if (dmRig::RESULT_OK != dmRig::InstanceCreate(create_params)) {
            dmLogError("Could not create rig instance!");
        }
    }

    virtual void TearDown() {

        dmRig::InstanceDestroyParams destroy_params = {0};
        destroy_params.m_Context = RigContextCursorTest<T>::m_Context;
        destroy_params.m_Instance = m_Instance;
        if (dmRig::RESULT_OK != dmRig::InstanceDestroy(destroy_params)) {
            dmLogError("Could not delete rig instance!");
        }

        DeleteRigData(m_MeshSet, m_Skeleton, m_AnimationSet);

        RigContextCursorTest<T>::TearDown();
    }
};

class RigInstanceCursorBackwardTest : public RigInstanceCursorTest<dmRig::RigPlayback>
{
public:
    virtual ~RigInstanceCursorBackwardTest() {}
};

class RigInstanceCursorForwardTest : public RigInstanceCursorTest<dmRig::RigPlayback>
{
public:
    virtual ~RigInstanceCursorForwardTest() {}
};

class RigInstanceCursorPingpongTest : public RigInstanceCursorTest<dmRig::RigPlayback>
{
public:
    virtual ~RigInstanceCursorPingpongTest() {}
};

TEST_P(RigInstanceCursorPingpongTest, CursorSet)
{
    dmRig::RigPlayback playback_mode = GetParam();

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), playback_mode, 0.0f, 0.0f, 1.0f));

    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));

    ASSERT_NEAR(1.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(1.0f / 3.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);

    // Setting cursor > 0.5f (normalized) should still put the cursor in the "ping" part of the animation
    ASSERT_NEAR(dmRig::RESULT_OK, dmRig::SetCursor(m_Instance, 2.0f, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(2.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(2.0f / 3.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_NEAR(3.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(1.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);
}

TEST_P(RigInstanceCursorBackwardTest, CursorSet)
{
    dmRig::RigPlayback playback_mode = GetParam();

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), playback_mode, 0.0f, 0.0f, 1.0f));

    // Starts playing from "end"
    ASSERT_NEAR(3.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(1.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);

    // Steps in direction from "end" towards "start"
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_NEAR(2.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(2.0f / 3.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);

    // Cursor set at 0.0 should place cursor at 0.0
    ASSERT_NEAR(dmRig::RESULT_OK, dmRig::SetCursor(m_Instance, 0.0f, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);

    // Cursor placed at end should place cursor at end
    ASSERT_NEAR(dmRig::RESULT_OK, dmRig::SetCursor(m_Instance, 3.0f, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(3.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(1.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);

    // Place cursor at arbitrary positions
    ASSERT_NEAR(dmRig::RESULT_OK, dmRig::SetCursor(m_Instance, 0.5f, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(0.5f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(0.5f / 3.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);

    ASSERT_NEAR(dmRig::RESULT_OK, dmRig::SetCursor(m_Instance, 0.25f, true), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(0.25f * 3.f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(0.25f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);

    // Over/under placed cursor should wrap correctly
    ASSERT_NEAR(dmRig::RESULT_OK, dmRig::SetCursor(m_Instance, 3.5f, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(0.5f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(0.5f / 3.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);

    ASSERT_NEAR(dmRig::RESULT_OK, dmRig::SetCursor(m_Instance, -3.5f, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(2.5f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(2.5f / 3.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);

    ASSERT_NEAR(dmRig::RESULT_OK, dmRig::SetCursor(m_Instance, 1.0f, true), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(3.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(1.0, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);

    ASSERT_NEAR(dmRig::RESULT_OK, dmRig::SetCursor(m_Instance, -12.0f, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(3.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(1.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);

    ASSERT_NEAR(dmRig::RESULT_OK, dmRig::SetCursor(m_Instance, 12.0f, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(3.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(1.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);

    ASSERT_NEAR(dmRig::RESULT_OK, dmRig::SetCursor(m_Instance, -10.0f, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(2.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(2.0f / 3.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);

    ASSERT_NEAR(dmRig::RESULT_OK, dmRig::SetCursor(m_Instance, 10.0f, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(1.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(1.0f / 3.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);
}

TEST_P(RigInstanceCursorForwardTest, CursorSet)
{
    dmRig::RigPlayback playback_mode = GetParam();

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), playback_mode, 0.0f, 0.0f, 1.0f));

    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));

    ASSERT_NEAR(1.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(1.0f / 3.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);

    ASSERT_NEAR(dmRig::RESULT_OK, dmRig::SetCursor(m_Instance, 0.0f, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);

    ASSERT_NEAR(dmRig::RESULT_OK, dmRig::SetCursor(m_Instance, 0.5f, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(0.5f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(0.5f / 3.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);

    ASSERT_NEAR(dmRig::RESULT_OK, dmRig::SetCursor(m_Instance, 0.0f, true), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);

    ASSERT_NEAR(dmRig::RESULT_OK, dmRig::SetCursor(m_Instance, 1.0f, true), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(3.0f * 1.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(1.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);

    ASSERT_NEAR(dmRig::RESULT_OK, dmRig::SetCursor(m_Instance, 0.5f, true), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(3.0f * 0.5f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(0.5f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));

    ASSERT_NEAR(2.5f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(2.5f / 3.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);

    ASSERT_NEAR(dmRig::RESULT_OK, dmRig::SetCursor(m_Instance, 12.0f, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(3.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(1.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);

    ASSERT_NEAR(dmRig::RESULT_OK, dmRig::SetCursor(m_Instance, -12.0f, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(3.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(1.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);

    ASSERT_NEAR(dmRig::RESULT_OK, dmRig::SetCursor(m_Instance, -1.0f, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(2.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(2.0f / 3.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);

    ASSERT_NEAR(dmRig::RESULT_OK, dmRig::SetCursor(m_Instance, -1.2f, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(1.8f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(1.8f / 3.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);
}

dmRig::RigPlayback playback_modes_pingpong[] = {
    dmRig::PLAYBACK_ONCE_PINGPONG,  dmRig::PLAYBACK_LOOP_PINGPONG
};

dmRig::RigPlayback playback_modes_forward[] = {
    dmRig::PLAYBACK_ONCE_FORWARD, dmRig::PLAYBACK_LOOP_FORWARD
};

dmRig::RigPlayback playback_modes_backward[] = {
    dmRig::PLAYBACK_ONCE_BACKWARD,  dmRig::PLAYBACK_LOOP_BACKWARD
};

dmRig::RigPlayback playback_modes_all[] = {
    dmRig::PLAYBACK_ONCE_PINGPONG,  dmRig::PLAYBACK_LOOP_PINGPONG,  dmRig::PLAYBACK_ONCE_FORWARD,
    dmRig::PLAYBACK_LOOP_FORWARD,   dmRig::PLAYBACK_ONCE_BACKWARD,  dmRig::PLAYBACK_LOOP_BACKWARD
};

INSTANTIATE_TEST_CASE_P(Rig, RigInstanceCursorForwardTest, ::testing::ValuesIn(playback_modes_forward));
INSTANTIATE_TEST_CASE_P(Rig, RigInstanceCursorBackwardTest, ::testing::ValuesIn(playback_modes_backward));
INSTANTIATE_TEST_CASE_P(Rig, RigInstanceCursorPingpongTest, ::testing::ValuesIn(playback_modes_pingpong));

class RigInstanceTest : public RigContextTest
{
public:
    dmRig::HRigInstance     m_Instance;
    dmArray<dmRig::RigBone> m_BindPose;
    dmRigDDF::Skeleton*     m_Skeleton;
    dmRigDDF::MeshSet*      m_MeshSet;
    dmRigDDF::AnimationSet* m_AnimationSet;

    dmArray<uint32_t>       m_PoseIdxToInfluence;
    dmArray<uint32_t>       m_TrackIdxToPose;

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
        SetUpSimpleRig(m_BindPose, m_Skeleton, m_MeshSet, m_AnimationSet, m_PoseIdxToInfluence, m_TrackIdxToPose);

        // Data
        create_params.m_BindPose     = &m_BindPose;
        create_params.m_Skeleton     = m_Skeleton;
        create_params.m_MeshSet      = m_MeshSet;
        create_params.m_AnimationSet = m_AnimationSet;
        create_params.m_TrackIdxToPose     = &m_TrackIdxToPose;
        create_params.m_PoseIdxToInfluence = &m_PoseIdxToInfluence;

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

        DeleteRigData(m_MeshSet, m_Skeleton, m_AnimationSet);

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
    ASSERT_EQ(dmRig::RESULT_ERROR_BUFFER_FULL, dmRig::InstanceCreate(create_params));
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
    ASSERT_EQ(dmRig::RESULT_ANIM_NOT_FOUND, dmRig::PlayAnimation(m_Instance, invalid_anim_id, dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, 0.0f, 1.0f));
    ASSERT_NE(invalid_anim_id, dmRig::GetAnimation(m_Instance));
    ASSERT_EQ(empty_id, dmRig::GetAnimation(m_Instance));

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0/60.0));
}

TEST_F(RigInstanceTest, PlayValidAnimation)
{
    dmhash_t valid_anim_id = dmHashString64("valid");

    // Default animation does not exist
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, valid_anim_id, dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, 0.0f, 1.0f));
    ASSERT_EQ(valid_anim_id, dmRig::GetAnimation(m_Instance));

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0/60.0));
}

TEST_F(RigInstanceTest, MaxBoneCount)
{
    // Call GenerateVertedData to setup m_ScratchInfluenceMatrixBuffer
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0/60.0));
    dmRig::RigModelVertex data[4];
    dmRig::RigModelVertex* data_end = data + 4;
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, Matrix4::identity(), Matrix4::identity(), Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_MODEL, (void*)data));

    // m_ScratchInfluenceMatrixBuffer should be able to contain the instance max bone count, which is the max of the used skeleton and meshset
    // MaxBoneCount is set to BoneCount + 1 for testing.
    ASSERT_EQ(m_Context->m_ScratchInfluenceMatrixBuffer.Size(), dmRig::GetMaxBoneCount(m_Instance));
    ASSERT_EQ(m_Context->m_ScratchInfluenceMatrixBuffer.Size(), dmRig::GetBoneCount(m_Instance) + 1);

    // Setting the m_ScratchInfluenceMatrixBuffer to zero ensures it have to be resized to max bone count
    m_Context->m_ScratchInfluenceMatrixBuffer.SetCapacity(0);
    // If this isn't done correctly, it'll assert out of bounds
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0/60.0));
}


#define ASSERT_VEC3(exp, act)\
    ASSERT_NEAR(exp.getX(), act.getX(), RIG_EPSILON_FLOAT);\
    ASSERT_NEAR(exp.getY(), act.getY(), RIG_EPSILON_FLOAT);\
    ASSERT_NEAR(exp.getZ(), act.getZ(), RIG_EPSILON_FLOAT);\

#define ASSERT_VEC4(exp, act)\
    ASSERT_NEAR(exp.getX(), act.getX(), RIG_EPSILON_FLOAT);\
    ASSERT_NEAR(exp.getY(), act.getY(), RIG_EPSILON_FLOAT);\
    ASSERT_NEAR(exp.getZ(), act.getZ(), RIG_EPSILON_FLOAT);\
    ASSERT_NEAR(exp.getW(), act.getW(), RIG_EPSILON_FLOAT);\

#define ASSERT_VEC4_NEAR(exp, act, eps)\
    ASSERT_NEAR(exp.getX(), act.getX(), eps);\
    ASSERT_NEAR(exp.getY(), act.getY(), eps);\
    ASSERT_NEAR(exp.getZ(), act.getZ(), eps);\
    ASSERT_NEAR(exp.getW(), act.getW(), eps);\

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
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, 0.0f, 1.0f));

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
    ASSERT_VEC3(Vector3(1.0f, 0.0f, 0.0f), pose[1].GetTranslation());
    ASSERT_VEC4(Quat::rotationZ((float)M_PI / 2.0f), pose[0].GetRotation());
    ASSERT_VEC4(Quat::identity(), pose[1].GetRotation());


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
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, 0.0f, 1.0f));

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

// Test that blend between two rotation animations that their midway
// value is normalized and between the two rotations.
TEST_F(RigInstanceTest, BlendRotation)
{
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("rot_blend1"), dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, 0.0f, 1.0f));

    dmArray<dmTransform::Transform>& pose = *dmRig::GetPose(m_Instance);

    const Quat rot_1 = Quat::rotationZ((float)M_PI / 180.0f * (-89.0f));
    const Quat rot_2 = Quat::rotationZ((float)M_PI / 180.0f * (89.0f));
    const Quat rot_midway = Quat::identity();

    // Verify that the rotation is "static" -89 degrees.
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_VEC4(rot_1, pose[0].GetRotation());
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_VEC4(rot_1, pose[0].GetRotation());

    // Start second animation with 1 second blend
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("rot_blend2"), dmRig::PLAYBACK_LOOP_FORWARD, 1.0f, 0.0f, 1.0f));

    // Verify that the rotation is midway between -89 and 90 degrees on Z (ie identity).
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 0.5f));
    ASSERT_VEC4(rot_midway, pose[0].GetRotation());

    // Blend is done, should be "static" at 89 degrees.
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_VEC4(rot_2, pose[0].GetRotation());
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_VEC4(rot_2, pose[0].GetRotation());
}

TEST_F(RigInstanceTest, GetVertexCount)
{
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(4, dmRig::GetVertexCount(m_Instance));
}

#define ASSERT_VERT_POS(exp, act)\
    ASSERT_VEC3(exp, Vector3(act.x, act.y, act.z));

#define ASSERT_VERT_NORM(exp, act)\
    ASSERT_VEC3(exp, Vector3(act.nx, act.ny, act.nz));

#define ASSERT_VERT_UV(exp_u, exp_v, act_u, act_v)\
    ASSERT_NEAR(exp_u, act_u, RIG_EPSILON_FLOAT);\
    ASSERT_NEAR(exp_v, act_v, RIG_EPSILON_FLOAT);\

#define ASSERT_VERT_COLOR(exp, act)\
    {\
        uint32_t r = (act & 255);\
        uint32_t g = ((act >> 8) & 255);\
        uint32_t b = ((act >> 16) & 255);\
        uint32_t a = ((act >> 24) & 255);\
        float rf = r/255.0f;\
        float gf = g/255.0f;\
        float bf = b/255.0f;\
        float af = a/255.0f;\
        ASSERT_VEC4_NEAR(exp, Vector4(rf, gf, bf, af), RIG_EPSILON_BYTE);\
    }

// DEF-2343 - Test that a bone with both scaling and rotation in its bind pose
//            does correctly apply to vertices bound to it.
TEST_F(RigInstanceTest, ScaleRotBindPose)
{
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    dmRig::RigSpineModelVertex data[4];
    dmRig::RigSpineModelVertex* data_end = data + 4;

    // no animation, just need to check bind pose
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, Matrix4::identity(), Matrix4::identity(), Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_SPINE, (void*)data));
    ASSERT_VERT_POS(Vector3(1.0f, 2.0f, 0.0f), data[3]); // v3
}

TEST_F(RigInstanceTest, GenerateVertexData)
{
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, 0.0f, 1.0f));
    dmRig::RigSpineModelVertex data[4];
    dmRig::RigSpineModelVertex* data_end = data + 4;

    // sample 0
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, Matrix4::identity(), Matrix4::identity(), Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_SPINE, (void*)data));
    ASSERT_VERT_POS(Vector3(0.0f),            data[0]); // v0
    ASSERT_VERT_POS(Vector3(1.0f, 0.0f, 0.0), data[1]); // v1
    ASSERT_VERT_POS(Vector3(2.0f, 0.0f, 0.0), data[2]); // v2

    // sample 1
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, Matrix4::identity(), Matrix4::identity(), Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_SPINE, (void*)data));
    ASSERT_VERT_POS(Vector3(0.0f),            data[0]); // v0
    ASSERT_VERT_POS(Vector3(1.0f, 0.0f, 0.0), data[1]); // v1
    ASSERT_VERT_POS(Vector3(1.0f, 1.0f, 0.0), data[2]); // v2

    // sample 2
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, Matrix4::identity(), Matrix4::identity(), Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_SPINE, (void*)data));
    ASSERT_VERT_POS(Vector3(0.0f),            data[0]); // v0
    ASSERT_VERT_POS(Vector3(0.0f, 1.0f, 0.0), data[1]); // v1
    ASSERT_VERT_POS(Vector3(0.0f, 2.0f, 0.0), data[2]); // v2
}

TEST_F(RigInstanceTest, GenerateNormalData)
{
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, 0.0f, 1.0f));
    dmRig::RigModelVertex data[4];
    dmRig::RigModelVertex* data_end = data + 4;

    Vector3 n_up(0.0f, 1.0f, 0.0f);
    Vector3 n_neg_right(-1.0f, 0.0f, 0.0f);

    // sample 0
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, Matrix4::identity(), Matrix4::identity(), Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_MODEL, (void*)data));
    ASSERT_VERT_NORM(n_up, data[0]); // v0
    ASSERT_VERT_NORM(n_up, data[1]); // v1
    ASSERT_VERT_NORM(n_up, data[2]); // v2

    // sample 1
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, Matrix4::identity(), Matrix4::identity(), Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_MODEL, (void*)data));
    ASSERT_VERT_NORM(n_up,        data[0]); // v0
    ASSERT_VERT_NORM(n_neg_right, data[1]); // v1
    ASSERT_VERT_NORM(n_neg_right, data[2]); // v2

    // sample 2
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, Matrix4::identity(), Matrix4::identity(), Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_MODEL, (void*)data));
    ASSERT_VERT_NORM(n_neg_right, data[0]); // v0
    ASSERT_VERT_NORM(n_neg_right, data[1]); // v1
    ASSERT_VERT_NORM(n_neg_right, data[2]); // v2
}

TEST_F(RigInstanceTest, SetMesh)
{
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::SetMesh(m_Instance, dmHashString64("test")));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, 0.0f, 1.0f));

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 0.0f));
    ASSERT_EQ(4, GetVertexCount(m_Instance));

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::SetMesh(m_Instance, dmHashString64("draw_order_skin")));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 0.0f));
    ASSERT_EQ(3, GetVertexCount(m_Instance));
}

TEST_F(RigInstanceTest, SetMeshSlot)
{
    dmRig::RigSpineModelVertex data[4];

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::SetMesh(m_Instance, dmHashString64("test")));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, 0.0f, 1.0f));

    // initial skin has 4 vertices on slot 0
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 0.0f));
    ASSERT_EQ(4, GetVertexCount(m_Instance));

    // Set specific slots from "draw_order_skin", each slot only has one vertex, with position corresponding the the mesh index.

    // Set slot 0
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::SetMeshSlot(m_Instance, dmHashString64("draw_order_skin"), 0));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 0.0f));
    ASSERT_EQ(1, GetVertexCount(m_Instance));
    ASSERT_EQ(data + 1, dmRig::GenerateVertexData(m_Context, m_Instance, Matrix4::identity(), Matrix4::identity(), Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_SPINE, (void*)data));
    ASSERT_VERT_POS(Vector3(0.0f), data[0]);

    // Set slot 1
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::SetMeshSlot(m_Instance, dmHashString64("draw_order_skin"), 1));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 0.0f));
    ASSERT_EQ(2, GetVertexCount(m_Instance));
    ASSERT_EQ(data + 2, dmRig::GenerateVertexData(m_Context, m_Instance, Matrix4::identity(), Matrix4::identity(), Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_SPINE, (void*)data));
    ASSERT_VERT_POS(Vector3(0.0f), data[0]);
    ASSERT_VERT_POS(Vector3(2.0f), data[1]); // Mesh index = slot * 2

    // Set slot 2
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::SetMeshSlot(m_Instance, dmHashString64("draw_order_skin"), 2));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 0.0f));
    ASSERT_EQ(3, GetVertexCount(m_Instance));
    ASSERT_EQ(data + 3, dmRig::GenerateVertexData(m_Context, m_Instance, Matrix4::identity(), Matrix4::identity(), Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_SPINE, (void*)data));
    ASSERT_VERT_POS(Vector3(0.0f), data[0]);
    ASSERT_VERT_POS(Vector3(2.0f), data[1]);
    ASSERT_VERT_POS(Vector3(4.0f), data[2]);
}

TEST_F(RigInstanceTest, SkinColor)
{
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::SetMesh(m_Instance, dmHashString64("skin_color")));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, 0.0f, 1.0f));
    dmRig::RigSpineModelVertex data[4];
    dmRig::RigSpineModelVertex* data_end = data + 4;

    
    // Trigger update which will recalculate mesh properties
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 0.0f));

    // sample 0
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, Matrix4::identity(), Matrix4::identity(), Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_SPINE, (void*)data));
    // Skin color is (0.5, 0.4, 0.3, 0.2)
    ASSERT_VERT_COLOR(Vector4(0.5f, 0.4f, 0.3f, 0.2f), data[0].rgba);
}

TEST_F(RigInstanceTest, SkinColorAndSlotColor)
{
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::SetMesh(m_Instance, dmHashString64("skin_color")));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("skin_and_slot_colors_anim"), dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, 0.0f, 1.0f));
    dmRig::RigSpineModelVertex data[4];
    dmRig::RigSpineModelVertex* data_end = data + 4;

    
    // Trigger update which will recalculate mesh properties
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 0.0f));

    /*anim_track0.m_Colors.m_Data[0] = 1.0f;
            anim_track0.m_Colors.m_Data[1] = 0.5f;
            anim_track0.m_Colors.m_Data[2] = 0.0f;
            anim_track0.m_Colors.m_Data[3] = 1.0f;*/

    // sample 0
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, Matrix4::identity(), Matrix4::identity(), Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_SPINE, (void*)data));
    // Slot color is (1.0, 0.5, 0.0, 1.0)
    // Skin color is (0.5, 0.4, 0.3, 0.2)
    ASSERT_VERT_COLOR(Vector4(0.5f, 0.2f, 0.0f, 0.2f), data[0].rgba);
}

TEST_F(RigInstanceTest, GenerateColorData)
{
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::SetMesh(m_Instance, dmHashString64("secondary_skin")));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("mesh_colors"), dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, 0.0f, 1.0f));
    dmRig::RigSpineModelVertex data[4];
    dmRig::RigSpineModelVertex* data_end = data + 4;

    // Trigger update which will recalculate mesh properties
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 0.0f));

    // sample 0
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, Matrix4::identity(), Matrix4::identity(), Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_SPINE, (void*)data));
    ASSERT_VERT_COLOR(Vector4(1.0f, 0.5f, 0.0f, 1.0f), data[0].rgba);

    // sample 1
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, Matrix4::identity(), Matrix4::identity(), Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_SPINE, (void*)data));
    ASSERT_VERT_COLOR(Vector4(0.0f, 0.5f, 1.0f, 0.5f), data[0].rgba);

    // sample 2, color has been changed for the model
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, Matrix4::identity(), Matrix4::identity(), Vector4(0.5), dmRig::RIG_VERTEX_FORMAT_SPINE, (void*)data));
    ASSERT_VERT_COLOR(Vector4(0.0f, 0.25f, 0.5f, 0.25f), data[0].rgba);
}

TEST_F(RigInstanceTest, GenerateTexcoordData)
{
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::SetMesh(m_Instance, dmHashString64("secondary_skin")));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("mesh_colors"), dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, 0.0f, 1.0f));
    dmRig::RigModelVertex data[4];
    dmRig::RigModelVertex* data_end = data + 4;

    // Trigger update which will recalculate mesh properties
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 0.0f));

    // sample 0
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, Matrix4::identity(), Matrix4::identity(), Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_MODEL, (void*)data));
    ASSERT_VERT_UV(-1.0f, 2.0f, data[0].u, data[0].v);
}

// Test to verify that two rigs does not interfeer with each others pose information,
// by comparing their generated vertex data. This verifies fix DEF-2838.
TEST_F(RigInstanceTest, MultipleRigInfluences)
{
    // We need to setup a separate rig instance than m_Instance, but without any animation set.
    dmRig::HRigInstance second_instance = 0x0;

    dmRigDDF::Skeleton* skeleton     = new dmRigDDF::Skeleton();
    dmRigDDF::MeshSet* mesh_set      = new dmRigDDF::MeshSet();
    dmRigDDF::AnimationSet* animation_set = new dmRigDDF::AnimationSet();

    dmArray<dmRig::RigBone> bind_pose;
    dmArray<uint32_t>       pose_to_influence;
    dmArray<uint32_t>       track_idx_to_pose;
    SetUpSimpleRig(bind_pose, skeleton, mesh_set, animation_set, pose_to_influence, track_idx_to_pose);

    // Second rig instance data
    dmRig::InstanceCreateParams create_params = {0};
    create_params.m_Context          = m_Context;
    create_params.m_Instance         = &second_instance;
    create_params.m_BindPose         = &bind_pose;
    create_params.m_Skeleton         = skeleton;
    create_params.m_MeshSet          = mesh_set;
    create_params.m_TrackIdxToPose   = &track_idx_to_pose;
    create_params.m_MeshId           = dmHashString64((const char*)"test");
    create_params.m_DefaultAnimation = 0x0;

    // We deliberately set the animation set to NULL and the size of pose-to-influence array to 0.
    // This mimics as if there was no animation resource set for a model component.
    pose_to_influence.SetSize(0);
    create_params.m_AnimationSet       = 0x0;
    create_params.m_PoseIdxToInfluence = &pose_to_influence;

    if (dmRig::RESULT_OK != dmRig::InstanceCreate(create_params)) {
        dmLogError("Could not create rig instance!");
    }

    // Play animation on first rig instance.
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, 0.0f, 1.0f));

    dmRig::RigModelVertex data[4];
    dmRig::RigModelVertex* data_end = data + 4;

    // Comparison values
    Vector3 n_up(0.0f, 1.0f, 0.0f);
    Vector3 n_neg_right(-1.0f, 0.0f, 0.0f);

    // sample 0 - Both rigs are in their bind pose.
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, Matrix4::identity(), Matrix4::identity(), Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_MODEL, (void*)data));
    ASSERT_VERT_NORM(n_up, data[0]);
    ASSERT_VERT_NORM(n_up, data[1]);
    ASSERT_VERT_NORM(n_up, data[2]);
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, second_instance, Matrix4::identity(), Matrix4::identity(), Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_MODEL, (void*)data));
    ASSERT_VERT_NORM(n_up, data[0]);
    ASSERT_VERT_NORM(n_up, data[1]);
    ASSERT_VERT_NORM(n_up, data[2]);

    // sample 1 - First rig instance should be animating, while the second one should still be in its bind pose.
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, Matrix4::identity(), Matrix4::identity(), Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_MODEL, (void*)data));
    ASSERT_VERT_NORM(n_up,        data[0]); // v0
    ASSERT_VERT_NORM(n_neg_right, data[1]); // v1
    ASSERT_VERT_NORM(n_neg_right, data[2]); // v2
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, second_instance, Matrix4::identity(), Matrix4::identity(), Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_MODEL, (void*)data));
    ASSERT_VERT_NORM(n_up, data[0]); // v0
    ASSERT_VERT_NORM(n_up, data[1]); // v1
    ASSERT_VERT_NORM(n_up, data[2]); // v2

    // Cleanup after second rig instance
    dmRig::InstanceDestroyParams destroy_params = {0};
    destroy_params.m_Context = m_Context;
    destroy_params.m_Instance = second_instance;
    if (dmRig::RESULT_OK != dmRig::InstanceDestroy(destroy_params)) {
        dmLogError("Could not delete second rig instance!");
    }
    DeleteRigData(mesh_set, skeleton, animation_set);
}

TEST_F(RigInstanceTest, AnimatedDrawOrder)
{
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::SetMesh(m_Instance, dmHashString64("draw_order_skin")));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, 0.0f, 1.0f));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 0.0f));

    dmRig::RigSpineModelVertex data[1*3];
    dmRig::RigSpineModelVertex* data_end = data + 1*3;

    // Check bind "pose" of draw order
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, Matrix4::identity(), Matrix4::identity(), Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_SPINE, (void*)data));
    ASSERT_VERT_POS(Vector3(0.0f), data[0]);
    ASSERT_VERT_POS(Vector3(2.0f), data[1]);
    ASSERT_VERT_POS(Vector3(4.0f), data[2]);

    // Play draw order animation
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("draw_order_anim"), dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, 0.0f, 1.0f));

    // sample 0, mesh 0 has offset +2
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 0.0f));
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, Matrix4::identity(), Matrix4::identity(), Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_SPINE, (void*)data));
    ASSERT_VERT_POS(Vector3(2.0f), data[0]);
    ASSERT_VERT_POS(Vector3(4.0f), data[1]);
    ASSERT_VERT_POS(Vector3(0.0f), data[2]);

    // sample 1, mesh 4 has offset -2
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, Matrix4::identity(), Matrix4::identity(), Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_SPINE, (void*)data));
    ASSERT_VERT_POS(Vector3(4.0f), data[0]);
    ASSERT_VERT_POS(Vector3(0.0f), data[1]);
    ASSERT_VERT_POS(Vector3(2.0f), data[2]);

    // sample 2, mesh 2 has offset -1
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, Matrix4::identity(), Matrix4::identity(), Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_SPINE, (void*)data));
    ASSERT_VERT_POS(Vector3(2.0f), data[0]);
    ASSERT_VERT_POS(Vector3(0.0f), data[1]);
    ASSERT_VERT_POS(Vector3(4.0f), data[2]);
}

TEST_F(RigInstanceTest, AnimatedDrawOrderBlending)
{
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::SetMesh(m_Instance, dmHashString64("draw_order_skin")));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("draw_order_anim"), dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, 0.0f, 1.0f));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 0.0f));

    dmRig::RigSpineModelVertex data[1*3];
    dmRig::RigSpineModelVertex* data_end = data + 1*3;

    // Check that order is same as first frame in draw_order_skin
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, Matrix4::identity(), Matrix4::identity(), Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_SPINE, (void*)data));
    ASSERT_VERT_POS(Vector3(2.0f), data[0]);
    ASSERT_VERT_POS(Vector3(4.0f), data[1]);
    ASSERT_VERT_POS(Vector3(0.0f), data[2]);

    // Play animation without any draw order track, but blend over one frame
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmRig::PLAYBACK_LOOP_FORWARD, 1.0f, 0.0f, 1.0f));
    // sample 0, should have same order as bind pose (has not finished blending yet)
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 0.0f));
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, Matrix4::identity(), Matrix4::identity(), Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_SPINE, (void*)data));
    ASSERT_VERT_POS(Vector3(2.0f), data[0]);
    ASSERT_VERT_POS(Vector3(4.0f), data[1]);
    ASSERT_VERT_POS(Vector3(0.0f), data[2]);

    // sample 1, blending done, back to bind draw order
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, Matrix4::identity(), Matrix4::identity(), Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_SPINE, (void*)data));
    ASSERT_VERT_POS(Vector3(0.0f), data[0]);
    ASSERT_VERT_POS(Vector3(2.0f), data[1]);
    ASSERT_VERT_POS(Vector3(4.0f), data[2]);

    // sample 2, still same as bind draw order
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, Matrix4::identity(), Matrix4::identity(), Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_SPINE, (void*)data));
    ASSERT_VERT_POS(Vector3(0.0f), data[0]);
    ASSERT_VERT_POS(Vector3(2.0f), data[1]);
    ASSERT_VERT_POS(Vector3(4.0f), data[2]);
}

TEST_F(RigInstanceTest, AnimatedAttachmentBlending)
{
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::SetMesh(m_Instance, dmHashString64("draw_order_skin")));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("slot_attachments"), dmRig::PLAYBACK_ONCE_FORWARD, 0.0f, 0.0f, 1.0f));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 4.0f));

    dmRig::RigSpineModelVertex data[1*3];
    dmRig::RigSpineModelVertex* data_end = data + 1*3;

    // Check that attachments are correct for "slot_attachments", slot 0 should have mesh 1
    // In our setup, we indicate this by vert position being the same as mesh index, ie data[0] should be 1.0.
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, Matrix4::identity(), Matrix4::identity(), Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_SPINE, (void*)data));
    ASSERT_VERT_POS(Vector3(1.0f), data[0]);
    ASSERT_VERT_POS(Vector3(2.0f), data[1]);
    ASSERT_VERT_POS(Vector3(4.0f), data[2]);

    // Play animation without any attachment track, blend over one frame
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmRig::PLAYBACK_ONCE_FORWARD, 1.0f, 0.0f, 1.0f));

    // Update dt with 0.25, this means that the blend is still in progress.
    // Slot attachments should still be based on "slot_attachments" animation.
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 0.25f));
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, Matrix4::identity(), Matrix4::identity(), Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_SPINE, (void*)data));
    ASSERT_VERT_POS(Vector3(1.0f), data[0]);
    ASSERT_VERT_POS(Vector3(2.0f), data[1]);
    ASSERT_VERT_POS(Vector3(4.0f), data[2]);

    // Update dt with 0.5, the blend is still in progress, roughly at 0.75 blend.
    // Slot attachment should now instead be taken from "valid" animation, thus slot 0 should be back to attachment 0.
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 0.5f));
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, Matrix4::identity(), Matrix4::identity(), Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_SPINE, (void*)data));
    ASSERT_VERT_POS(Vector3(0.0f), data[0]);
    ASSERT_VERT_POS(Vector3(2.0f), data[1]);
    ASSERT_VERT_POS(Vector3(4.0f), data[2]);

    // Update dt with 1.0 one last time, blending should be done and slot attachment should be same as previous update.
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, Matrix4::identity(), Matrix4::identity(), Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_SPINE, (void*)data));
    ASSERT_VERT_POS(Vector3(0.0f), data[0]);
    ASSERT_VERT_POS(Vector3(2.0f), data[1]);
    ASSERT_VERT_POS(Vector3(4.0f), data[2]);
}

// Test Spine 2.x skeleton that has scaling relative to the bone local space.
TEST_F(RigInstanceTest, LocalBoneScaling)
{
    m_Skeleton->m_LocalBoneScaling = true;

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("scaling"), dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, 0.0f, 1.0f));
    dmRig::RigSpineModelVertex data[4];
    dmRig::RigSpineModelVertex* data_end = data + 4;

    // sample 0
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, Matrix4::identity(), Matrix4::identity(), Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_SPINE, (void*)data));
    ASSERT_VERT_POS(Vector3(0.0f),            data[0]); // v0
    ASSERT_VERT_POS(Vector3(1.0f, 0.0f, 0.0), data[1]); // v1
    ASSERT_VERT_POS(Vector3(2.0f, 0.0f, 0.0), data[2]); // v2

    // sample 1
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, Matrix4::identity(), Matrix4::identity(), Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_SPINE, (void*)data));
    ASSERT_VERT_POS(Vector3(0.0f),            data[0]); // v0
    ASSERT_VERT_POS(Vector3(0.0f, 2.0f, 0.0), data[1]); // v1
    ASSERT_VERT_POS(Vector3(2.0f, 2.0f, 0.0), data[2]); // v2
}

TEST_F(RigInstanceTest, BoneScaling)
{
    m_Skeleton->m_LocalBoneScaling = false;

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("scaling"), dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, 0.0f, 1.0f));
    dmRig::RigSpineModelVertex data[4];
    dmRig::RigSpineModelVertex* data_end = data + 4;

    // sample 0
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, Matrix4::identity(), Matrix4::identity(), Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_SPINE, (void*)data));
    ASSERT_VERT_POS(Vector3(0.0f),            data[0]); // v0
    ASSERT_VERT_POS(Vector3(1.0f, 0.0f, 0.0), data[1]); // v1
    ASSERT_VERT_POS(Vector3(2.0f, 0.0f, 0.0), data[2]); // v2

    // sample 1
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, Matrix4::identity(), Matrix4::identity(), Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_SPINE, (void*)data));
    ASSERT_VERT_POS(Vector3(0.0f),            data[0]); // v0
    ASSERT_VERT_POS(Vector3(0.0f, 2.0f, 0.0), data[1]); // v1

    // This is the major difference from Spine 2.x -> Spine 3.x behaviour.
    ASSERT_VERT_POS(Vector3(1.0f, 2.0f, 0.0), data[2]); // v2
}


TEST_F(RigInstanceTest, SetMeshInvalid)
{
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, 0.0f, 1.0f));
    dmRig::RigSpineModelVertex data[4];
    dmRig::RigSpineModelVertex* data_end = data + 4;

    dmhash_t new_mesh = dmHashString64("not_a_valid_skin");
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, Matrix4::identity(), Matrix4::identity(), Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_SPINE, (void*)data));
    ASSERT_EQ(dmRig::RESULT_ERROR, dmRig::SetMesh(m_Instance, new_mesh));
    ASSERT_EQ(dmHashString64("test"), dmRig::GetMesh(m_Instance));
    ASSERT_VERT_POS(Vector3(0.0f),            data[0]); // v0
    ASSERT_VERT_POS(Vector3(1.0f, 0.0f, 0.0), data[1]); // v1
    ASSERT_VERT_POS(Vector3(2.0f, 0.0f, 0.0), data[2]); // v2

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, Matrix4::identity(), Matrix4::identity(), Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_SPINE, (void*)data));
    ASSERT_VERT_POS(Vector3(0.0f),            data[0]); // v0
    ASSERT_VERT_POS(Vector3(1.0f, 0.0f, 0.0), data[1]); // v1
    ASSERT_VERT_POS(Vector3(1.0f, 1.0f, 0.0), data[2]); // v2
}


TEST_F(RigInstanceTest, SetMeshValid)
{
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, 0.0f, 1.0f));
    dmRig::RigSpineModelVertex data[4];
    dmRig::RigSpineModelVertex* data_end = data + 4;

    dmhash_t new_mesh = dmHashString64("secondary_skin");
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, Matrix4::identity(), Matrix4::identity(), Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_SPINE, (void*)data));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::SetMesh(m_Instance, new_mesh));
    ASSERT_EQ(new_mesh, dmRig::GetMesh(m_Instance));
    ASSERT_VERT_POS(Vector3(0.0f),            data[0]); // v0
    ASSERT_VERT_POS(Vector3(1.0f, 0.0f, 0.0), data[1]); // v1
    ASSERT_VERT_POS(Vector3(2.0f, 0.0f, 0.0), data[2]); // v2

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, Matrix4::identity(), Matrix4::identity(), Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_SPINE, (void*)data));
    ASSERT_VERT_POS(Vector3(0.0f),            data[0]); // v0
    ASSERT_VERT_POS(Vector3(1.0f, 0.0f, 0.0), data[1]); // v1
    ASSERT_VERT_POS(Vector3(1.0f, 1.0f, 0.0), data[2]); // v2
}

TEST_F(RigInstanceTest, CursorNoAnim)
{

    // no anim
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);

    // no anim + set cursor
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::SetCursor(m_Instance, 100.0f, false));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);

}

TEST_F(RigInstanceTest, CursorGet)
{
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, 0.0f, 1.0f));

    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_NEAR(1.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(1.0f / 3.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_NEAR(2.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(2.0f / 3.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);

    // "half a sample"
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 0.5f));
    ASSERT_NEAR(2.5f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(2.5f / 3.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);

    // animation restarted/looped
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 0.5f));
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);

}

TEST_F(RigInstanceTest, CursorSet)
{
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, 0.0f, 1.0f));

    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_NEAR(1.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(1.0f / 3.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);

    ASSERT_NEAR(dmRig::RESULT_OK, dmRig::SetCursor(m_Instance, 0.0f, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);

    ASSERT_NEAR(dmRig::RESULT_OK, dmRig::SetCursor(m_Instance, 0.5f, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(0.5f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(0.5f / 3.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);

    ASSERT_NEAR(dmRig::RESULT_OK, dmRig::SetCursor(m_Instance, 0.0f, true), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);

    ASSERT_NEAR(dmRig::RESULT_OK, dmRig::SetCursor(m_Instance, 1.0f, true), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(3.0f * 1.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(1.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);

    ASSERT_NEAR(dmRig::RESULT_OK, dmRig::SetCursor(m_Instance, 0.5f, true), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(3.0f * 0.5f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(0.5f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_NEAR(2.5f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(2.5f / 3.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);
}

TEST_F(RigInstanceTest, CursorSetOutside)
{
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, 0.0f, 1.0f));

    ASSERT_NEAR(dmRig::RESULT_OK, dmRig::SetCursor(m_Instance, 4.0f, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(1.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);

    ASSERT_NEAR(dmRig::RESULT_OK, dmRig::SetCursor(m_Instance, -4.0f, false), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(2.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);

    ASSERT_NEAR(dmRig::RESULT_OK, dmRig::SetCursor(m_Instance, 4.0f / 3.0f, true), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(1.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);

    ASSERT_NEAR(dmRig::RESULT_OK, dmRig::SetCursor(m_Instance, -4.0f / 3.0f, true), RIG_EPSILON_FLOAT);
    ASSERT_NEAR(2.0f, dmRig::GetCursor(m_Instance, false), RIG_EPSILON_FLOAT);
}

TEST_F(RigInstanceTest, CursorOffset)
{
    float offset = 0.5f;
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, offset, 1.0f));

    ASSERT_NEAR(offset, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);
}

TEST_F(RigInstanceTest, CursorOffsetOutside)
{
    float offset = 1.3f;
    float offset_normalized = 0.3;

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, offset, 1.0f));

    ASSERT_NEAR(offset_normalized, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);

    offset = -0.3;
    offset_normalized = 0.7f;

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, offset, 1.0f));

    ASSERT_NEAR(offset_normalized, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);
}

TEST_F(RigInstanceTest, PlaybackRateNoAnim)
{
    float default_playback_rate = 1.0f;
    // no anim
    ASSERT_NEAR(default_playback_rate, dmRig::GetPlaybackRate(m_Instance), RIG_EPSILON_FLOAT);
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_NEAR(default_playback_rate, dmRig::GetPlaybackRate(m_Instance), RIG_EPSILON_FLOAT);

    // no anim + set playback rate
    float playback_rate = 2.0f;
    ASSERT_NEAR(default_playback_rate, dmRig::GetPlaybackRate(m_Instance), RIG_EPSILON_FLOAT);
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::SetPlaybackRate(m_Instance, playback_rate));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_NEAR(default_playback_rate, dmRig::GetPlaybackRate(m_Instance), RIG_EPSILON_FLOAT);
}

TEST_F(RigInstanceTest, PlaybackRateGet)
{
    float default_playback_rate = 1.0f;
    float double_playback_rate = 2.0f;
    float negative_playback_rate = -1.0f;

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, 0.0f, default_playback_rate));
    ASSERT_NEAR(default_playback_rate, dmRig::GetPlaybackRate(m_Instance), RIG_EPSILON_FLOAT);

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, 0.0f, double_playback_rate));
    ASSERT_NEAR(double_playback_rate, dmRig::GetPlaybackRate(m_Instance), RIG_EPSILON_FLOAT);

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, 0.0f, negative_playback_rate));
    ASSERT_NEAR(0.0f, dmRig::GetPlaybackRate(m_Instance), RIG_EPSILON_FLOAT);
}

TEST_F(RigInstanceTest, PlaybackRateSet)
{
    float default_playback_rate = 1.0f;
    float zero_playback_rate = 0.0f;
    float double_playback_rate = 2.0f;
    float negative_playback_rate = -1.0f;

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, 0.0f, default_playback_rate));

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::SetPlaybackRate(m_Instance, zero_playback_rate));
    ASSERT_NEAR(zero_playback_rate, dmRig::GetPlaybackRate(m_Instance), RIG_EPSILON_FLOAT);

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::SetPlaybackRate(m_Instance, double_playback_rate));
    ASSERT_NEAR(double_playback_rate, dmRig::GetPlaybackRate(m_Instance), RIG_EPSILON_FLOAT);

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::SetPlaybackRate(m_Instance, negative_playback_rate));
    ASSERT_NEAR(0.0f, dmRig::GetPlaybackRate(m_Instance), RIG_EPSILON_FLOAT);
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
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("ik"), dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, 0.0f, 1.0f));


    dmRig::IKTarget* target = dmRig::GetIKTarget(m_Instance, dmHashString64("test_ik"));
    ASSERT_NE((dmRig::IKTarget*)0x0, target);
    target->m_Callback = UpdateIKPositionCallback;
    target->m_Mix = 1.0f;
    target->m_Position = Vector3(0.0f, 100.0f, 0.0f);

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));

    dmArray<dmTransform::Transform>& pose = *dmRig::GetPose(m_Instance);

    ASSERT_VEC3(Vector3(0.0f, 0.0f, 0.0f), pose[0].GetTranslation());
    ASSERT_VEC3(Vector3(0.0f, 1.0f, 0.0f), pose[3].GetTranslation());
    ASSERT_VEC3(Vector3(0.0f, 1.0f, 0.0f), pose[4].GetTranslation());
    ASSERT_VEC3(Vector3(0.0f, 1.0f, 0.0f), pose[5].GetTranslation());
    ASSERT_VEC4(Quat::identity(), pose[0].GetRotation());
    ASSERT_VEC4(Quat::identity(), pose[4].GetRotation());
    ASSERT_VEC4(Quat::identity(), pose[5].GetRotation());

    target->m_Position.setX(100.0f);
    target->m_Position.setY(1.0f);

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 0.0f));

    ASSERT_VEC3(Vector3(0.0f, 0.0f, 0.0f), pose[0].GetTranslation());
    ASSERT_VEC3(Vector3(0.0f, 1.0f, 0.0f), pose[3].GetTranslation());
    ASSERT_VEC3(Vector3(0.0f, 1.0f, 0.0f), pose[4].GetTranslation());
    ASSERT_VEC3(Vector3(0.0f, 1.0f, 0.0f), pose[5].GetTranslation());
    ASSERT_VEC4(Quat::identity(), pose[0].GetRotation());
    ASSERT_VEC4(Quat::rotationZ(-(float)M_PI / 2.0f), pose[4].GetRotation());
    ASSERT_VEC4(Quat::identity(), pose[5].GetRotation());

    target->m_Position.setX(0.0f);
    target->m_Position.setY(-100.0f);

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 0.0f));
    ASSERT_VEC4(Quat::identity(), pose[0].GetRotation());
    ASSERT_VEC4(Quat::rotationZ(-(float)M_PI), pose[4].GetRotation());
    ASSERT_VEC4(Quat::identity(), pose[5].GetRotation());
}

TEST_F(RigInstanceTest, InvalidTrackBone)
{
    m_Skeleton->m_LocalBoneScaling = false;

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("invalid_bones"), dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, 0.0f, 1.0f));
    dmRig::RigSpineModelVertex data[4];
    dmRig::RigSpineModelVertex* data_end = data + 4;

    // sample 0
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, Matrix4::identity(), Matrix4::identity(), Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_SPINE, (void*)data));
    ASSERT_VERT_POS(Vector3(0.0f),            data[0]); // v0
    ASSERT_VERT_POS(Vector3(1.0f, 0.0f, 0.0), data[1]); // v1
    ASSERT_VERT_POS(Vector3(2.0f, 0.0f, 0.0), data[2]); // v2

    // sample 1
    // There should be no changes to the pose/vertices since
    // the animation includes a track for a bone that does not exist.
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, Matrix4::identity(), Matrix4::identity(), Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_SPINE, (void*)data));
    ASSERT_VERT_POS(Vector3(0.0f),            data[0]); // v0
    ASSERT_VERT_POS(Vector3(1.0f, 0.0f, 0.0), data[1]); // v1
    ASSERT_VERT_POS(Vector3(2.0f, 0.0f, 0.0), data[2]); // v2
}

TEST_F(RigInstanceTest, BoneTranslationRotation)
{
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("trans_rot"), dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, 0.0f, 1.0f));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    dmArray<dmTransform::Transform>& pose = *dmRig::GetPose(m_Instance);

    ASSERT_VEC3(Vector3(10.0f, 0.0f, 0.0f), pose[0].GetTranslation());
    ASSERT_VEC4(Quat::rotationZ((float)M_PI / 2.0f), pose[0].GetRotation());
}

// DEF-3121 - Starting new animation from inside a "animation completed callback" would previously
// use the wrong animation for one frame.
// In the test we register a "completion callback", play one animation forward once, then play another
// animation once the callback is triggered. We test that the root bone is on the expected position
// after each animation.
static void DEF3121_EventCallback(dmRig::RigEventType event_type, void* event_data, void* user_data1, void* user_data2)
{
    ASSERT_EQ(dmRig::RIG_EVENT_TYPE_COMPLETED, event_type);
    dmRig::HRigInstance instance = *(dmRig::HRigInstance*)user_data1;
    ASSERT_NE((dmRig::HRigInstance)0x0, instance);

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(instance, dmHashString64("trans_rot"), dmRig::PLAYBACK_ONCE_FORWARD, 0.0f, 0.0f, 1.0f));
}

TEST_F(RigContextTest, DEF_3121)
{
    dmRig::HRigInstance instance = 0x0;

    // Setup test data
    dmRigDDF::Skeleton*     skeleton      = new dmRigDDF::Skeleton();
    dmRigDDF::MeshSet*      mesh_set      = new dmRigDDF::MeshSet();
    dmRigDDF::AnimationSet* animation_set = new dmRigDDF::AnimationSet();
    dmArray<dmRig::RigBone> bind_pose;
    dmArray<uint32_t>       pose_to_influence;
    dmArray<uint32_t>       track_idx_to_pose;
    SetUpSimpleRig(bind_pose, skeleton, mesh_set, animation_set, pose_to_influence, track_idx_to_pose);

    // Create rig instance
    dmRig::InstanceCreateParams create_params = {0};
    create_params.m_Context          = m_Context;
    create_params.m_Instance         = &instance;
    create_params.m_BindPose         = &bind_pose;
    create_params.m_Skeleton         = skeleton;
    create_params.m_MeshSet          = mesh_set;
    create_params.m_TrackIdxToPose   = &track_idx_to_pose;
    create_params.m_MeshId           = dmHashString64((const char*)"test");
    create_params.m_DefaultAnimation = dmHashString64((const char*)"");
    create_params.m_EventCallback    = DEF3121_EventCallback;
    create_params.m_EventCBUserData1 = &instance;
    create_params.m_AnimationSet       = animation_set;
    create_params.m_PoseIdxToInfluence = &pose_to_influence;
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::InstanceCreate(create_params));

    // Play initial animation
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(instance, dmHashString64("valid"), dmRig::PLAYBACK_ONCE_FORWARD, 0.0f, 0.0f, 1.0f));

    // "valid" animation should start with root at origo
    dmArray<dmTransform::Transform>& pose = *dmRig::GetPose(instance);
    ASSERT_VEC3(Vector3(0.0f), pose[0].GetTranslation());

    // Update rig instance, make sure we finish the animation which should trigger
    // the complete callback, which also will trigger a new animation, "trans_rot".
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 10.0f));

    // Verify that the position of the root bone is the same as previous frame.
    ASSERT_VEC3(Vector3(0.0f), pose[0].GetTranslation());

    // One more update, this time the new animation should start playing.
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 0.0f));

    // Verify that the position of the root bone is from the new animation.
    ASSERT_VEC3(Vector3(10.0f, 0.0f, 0.0f), pose[0].GetTranslation());

    // Cleanup
    dmRig::InstanceDestroyParams destroy_params = {0};
    destroy_params.m_Context = m_Context;
    destroy_params.m_Instance = instance;
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::InstanceDestroy(destroy_params));
    DeleteRigData(mesh_set, skeleton, animation_set);
}

// Test for DEF-3054 - Playing a spine backwards 3 times does not work as expected
struct PlaybackCursorTestParams
{
    dmRig::RigPlayback m_Playback;
    float              m_Offset;
    float              m_ExpectedStart;
    float              m_ExpectedEnd;
};

class PlaybackCursorTest : public RigInstanceCursorTest<PlaybackCursorTestParams>
{
public:
    virtual ~PlaybackCursorTest() {}
};

TEST_P(PlaybackCursorTest, PlayBackwardsBug)
{
    PlaybackCursorTestParams params = GetParam();

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_NEAR(0.0f, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);

    // Run test 3 times to make sure we use all (2) internal "players" that
    // could leak data between runs.
    for (int i = 0; i < 3; ++i)
    {
        ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), params.m_Playback, 0.0f, params.m_Offset, 1.0f));
        ASSERT_NEAR(params.m_ExpectedStart, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);
        ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 3.0f));
        ASSERT_NEAR(params.m_ExpectedEnd, dmRig::GetCursor(m_Instance, true), RIG_EPSILON_FLOAT);
    }
}

PlaybackCursorTestParams playback_cursor_test_params[] = {
    {dmRig::PLAYBACK_ONCE_FORWARD, 0.0f, 0.0f, 1.0f},
    {dmRig::PLAYBACK_ONCE_FORWARD, 0.5f, 0.5f, 1.0f},
    {dmRig::PLAYBACK_ONCE_BACKWARD, 0.0f, 1.0f, 0.0f},
    {dmRig::PLAYBACK_ONCE_BACKWARD, 0.5f, 0.5f, 0.0f}
};
INSTANTIATE_TEST_CASE_P(Rig, PlaybackCursorTest, ::testing::ValuesIn(playback_cursor_test_params));

#undef ASSERT_VEC3
#undef ASSERT_VEC4
#undef ASSERT_VEC4_NEAR
#undef ASSERT_VERT_POS
#undef ASSERT_VERT_NORM
#undef ASSERT_VERT_COLOR

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
