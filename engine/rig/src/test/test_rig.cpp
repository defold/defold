// Copyright 2020-2022 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
// 
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
// 
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#define _USE_MATH_DEFINES // for C
#include <math.h>

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include <dlib/log.h>
#include <dlib/hash.h>
#include <dmsdk/dlib/vmath.h>
#include <dmsdk/dlib/dstrings.h>

#include <../rig.h>

using namespace dmVMath;

#define RIG_EPSILON_FLOAT 0.0001f
#define RIG_EPSILON_BYTE (1.0f / 255.0f)


template <> char* jc_test_print_value(char* buffer, size_t buffer_len, Vector3 v) {
    return buffer + dmSnPrintf(buffer, buffer_len, "Vector3(%.3f, %.3f, %.3f)", v.getX(), v.getY(), v.getZ());
}

template <> char* jc_test_print_value(char* buffer, size_t buffer_len, Vector4 v) {
    return buffer + dmSnPrintf(buffer, buffer_len, "Vector4(%.3f, %.3f, %.3f, %.3f)", v.getX(), v.getY(), v.getZ(), v.getW());
}

template <> char* jc_test_print_value(char* buffer, size_t buffer_len, Quat v) {
    return buffer + dmSnPrintf(buffer, buffer_len, "Quat(%.3f, %.3f, %.3f, %.3f)", v.getX(), v.getY(), v.getZ(), v.getW());
}

template <> int jc_test_cmp_EQ(Vector3 a, Vector3 b, const char* exprA, const char* exprB) {
    bool equal = fabsf(a.getX() - b.getX()) <= RIG_EPSILON_FLOAT &&
                 fabsf(a.getY() - b.getY()) <= RIG_EPSILON_FLOAT &&
                 fabsf(a.getZ() - b.getZ()) <= RIG_EPSILON_FLOAT;
    if (equal) return 1;
    jc_test_log_failure(a, b, exprA, exprB, "==");
    return 0;
}

template <> int jc_test_cmp_EQ(Vector4 a, Vector4 b, const char* exprA, const char* exprB) {
    bool equal = fabsf(a.getX() - b.getX()) <= RIG_EPSILON_FLOAT &&
                 fabsf(a.getY() - b.getY()) <= RIG_EPSILON_FLOAT &&
                 fabsf(a.getZ() - b.getZ()) <= RIG_EPSILON_FLOAT &&
                 fabsf(a.getW() - b.getW()) <= RIG_EPSILON_FLOAT;
    if (equal) return 1;
    jc_test_log_failure(a, b, exprA, exprB, "==");
    return 0;
}

template <> int jc_test_cmp_EQ(Quat a, Quat b, const char* exprA, const char* exprB) {
    bool equal = fabsf(a.getX() - b.getX()) <= RIG_EPSILON_FLOAT &&
                 fabsf(a.getY() - b.getY()) <= RIG_EPSILON_FLOAT &&
                 fabsf(a.getZ() - b.getZ()) <= RIG_EPSILON_FLOAT &&
                 fabsf(a.getW() - b.getW()) <= RIG_EPSILON_FLOAT;
    if (equal) return 1;
    jc_test_log_failure(a, b, exprA, exprB, "==");
    return 0;
}


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

    if (anim.m_Tracks.m_Count) {
        delete [] anim.m_Tracks.m_Data;
    }
}

// Helper function to clean up / delete MeshSet, Skeleton and AnimationSet data
static void DeleteRigData(dmRigDDF::MeshSet* mesh_set, dmRigDDF::Skeleton* skeleton, dmRigDDF::AnimationSet* animation_set)
{
    if (animation_set != 0x0)
    {
        for (uint32_t anim_idx = 0; anim_idx < animation_set->m_Animations.m_Count; ++anim_idx)
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
        delete [] mesh_set->m_BoneList.m_Data;
        delete mesh_set;
    }
}

static void CreateTestMesh(dmRigDDF::MeshSet* mesh_set, int model_index, int mesh_index, Vector4 color)
{
    dmRigDDF::Model& model = mesh_set->m_Models.m_Data[model_index];
    dmRigDDF::Mesh& mesh = model.m_Meshes.m_Data[mesh_index];

    mesh.m_Indices.m_Count = 0;
    mesh.m_Indices.m_Data = 0;

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

    // Vert positioned at bone 2 origin
    mesh.m_Positions.m_Data[9]  = 1.0f;
    mesh.m_Positions.m_Data[10] = 2.0f;
    mesh.m_Positions.m_Data[11] = 0.0f;

    // data for each vertex
    mesh.m_Texcoord0.m_Count      = vert_count*2;
    mesh.m_Texcoord0.m_Data       = new float[mesh.m_Texcoord0.m_Count];
    mesh.m_Texcoord0[0]           = -1.0;
    mesh.m_Texcoord0[1]           = 2.0;

    mesh.m_Texcoord1.m_Count      = vert_count*2;
    mesh.m_Texcoord1.m_Data       = new float[mesh.m_Texcoord1.m_Count];
    mesh.m_Texcoord1[0]           = -2.0;
    mesh.m_Texcoord1[1]           = 4.0;

    mesh.m_Normals.m_Count        = vert_count*3;
    mesh.m_Normals.m_Data         = new float[mesh.m_Normals.m_Count];
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

    mesh.m_Tangents.m_Count       = vert_count*3;
    mesh.m_Tangents.m_Data        = new float[mesh.m_Tangents.m_Count];
    mesh.m_Tangents[0]            = 0.0;
    mesh.m_Tangents[1]            = 0.0;
    mesh.m_Tangents[2]            = 1.0;
    mesh.m_Tangents[3]            = 0.0;
    mesh.m_Tangents[4]            = 0.0;
    mesh.m_Tangents[5]            = 1.0;
    mesh.m_Tangents[6]            = 0.0;
    mesh.m_Tangents[7]            = 0.0;
    mesh.m_Tangents[8]            = 1.0;
    mesh.m_Tangents[9]            = 0.0;
    mesh.m_Tangents[10]           = 0.0;
    mesh.m_Tangents[11]           = 1.0;

    mesh.m_Colors.m_Count         = vert_count*4;
    mesh.m_Colors.m_Data          = new float[mesh.m_Colors.m_Count];
    for (int i = 0; i < vert_count; ++i) {
        mesh.m_Colors.m_Data[i*4+0] = i / (float(vert_count-1));
        mesh.m_Colors.m_Data[i*4+0] = 0.5f;
        mesh.m_Colors.m_Data[i*4+0] = 1.0f;
        mesh.m_Colors.m_Data[i*4+0] = 1.0f;
    }

    mesh.m_Colors.m_Data       = new float[vert_count*4];
    mesh.m_Colors.m_Count      = vert_count*4;
    mesh.m_Colors[0]           = color.getX();
    mesh.m_Colors[1]           = color.getY();
    mesh.m_Colors[2]           = color.getZ();
    mesh.m_Colors[3]           = color.getW();
    mesh.m_Colors[4]           = color.getX();
    mesh.m_Colors[5]           = color.getY();
    mesh.m_Colors[6]           = color.getZ();
    mesh.m_Colors[7]           = color.getW();
    mesh.m_Colors[8]           = color.getX();
    mesh.m_Colors[9]           = color.getY();
    mesh.m_Colors[10]          = color.getZ();
    mesh.m_Colors[11]          = color.getW();
    mesh.m_Colors[12]          = color.getX();
    mesh.m_Colors[13]          = color.getY();
    mesh.m_Colors[14]          = color.getZ();
    mesh.m_Colors[15]          = color.getW();

    mesh.m_BoneIndices.m_Data     = new uint32_t[vert_count*4];
    mesh.m_BoneIndices.m_Count    = vert_count*4;

    mesh.m_BoneIndices.m_Data[0]  = 0;
    mesh.m_BoneIndices.m_Data[1]  = 0;
    mesh.m_BoneIndices.m_Data[2]  = 0;
    mesh.m_BoneIndices.m_Data[3]  = 0;

    mesh.m_BoneIndices.m_Data[4]  = 1;
    mesh.m_BoneIndices.m_Data[5]  = 0;
    mesh.m_BoneIndices.m_Data[6]  = 0;
    mesh.m_BoneIndices.m_Data[7]  = 0;

    mesh.m_BoneIndices.m_Data[8]  = 2;
    mesh.m_BoneIndices.m_Data[9]  = 0;
    mesh.m_BoneIndices.m_Data[10] = 0;
    mesh.m_BoneIndices.m_Data[11] = 0;

    mesh.m_BoneIndices.m_Data[12] = 3;
    mesh.m_BoneIndices.m_Data[13] = 0;
    mesh.m_BoneIndices.m_Data[14] = 0;
    mesh.m_BoneIndices.m_Data[15] = 0;

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

static void CalcWorldTransforms(dmRigDDF::Bone* bones, uint32_t bone_count)
{
    for (uint32_t i = 0; i < bone_count; ++i)
    {
        dmRigDDF::Bone& bone = bones[i];
        if (i > 0)
            bone.m_World = dmTransform::Mul(bones[bone.m_Parent].m_World, bone.m_Local);
        else
            bone.m_World = bone.m_Local;
    }
}


// static void PrintTransforms(dmRigDDF::Bone* bones, uint32_t bone_count)
// {
//     for (uint32_t i = 0; i < bone_count; ++i)
//     {
//         dmRigDDF::Bone& bone = bones[i];
//         printf("Bone: %u\n", i);
//         printf("    parent:  %u\n", bone.m_Parent);
//         printf("    local:\n");
//         printf("        t: %f, %f, %f\n", bone.m_Local.GetTranslation().getX(), bone.m_Local.GetTranslation().getY(), bone.m_Local.GetTranslation().getZ());
//         printf("        r: %f, %f, %f, %f\n", bone.m_Local.GetRotation().getX(), bone.m_Local.GetRotation().getY(), bone.m_Local.GetRotation().getZ(), bone.m_Local.GetRotation().getW());
//         printf("    world:\n");
//         printf("        t: %f, %f, %f\n", bone.m_World.GetTranslation().getX(), bone.m_World.GetTranslation().getY(), bone.m_World.GetTranslation().getZ());
//         printf("        r: %f, %f, %f, %f\n", bone.m_World.GetRotation().getX(), bone.m_World.GetRotation().getY(), bone.m_World.GetRotation().getZ(), bone.m_World.GetRotation().getW());
//     }
// }

// static void PrintPose(const dmArray<dmRig::BonePose>& pose)
// {
//     for (uint32_t i = 0; i < pose.Size(); ++i)
//     {
//         const dmRig::BonePose& bp = pose[i];

//         printf("Bone pose: %u\n", i);
//         printf("    local:\n");
//         printf("        t: %f, %f, %f\n", bp.m_Local.GetTranslation().getX(), bp.m_Local.GetTranslation().getY(), bp.m_Local.GetTranslation().getZ());
//         printf("        r: %f, %f, %f, %f\n", bp.m_Local.GetRotation().getX(), bp.m_Local.GetRotation().getY(), bp.m_Local.GetRotation().getZ(), bp.m_Local.GetRotation().getW());
//         printf("    world:\n");
//         printf("        t: %f, %f, %f\n", bp.m_World.GetTranslation().getX(), bp.m_World.GetTranslation().getY(), bp.m_World.GetTranslation().getZ());
//         printf("        r: %f, %f, %f, %f\n", bp.m_World.GetRotation().getX(), bp.m_World.GetRotation().getY(), bp.m_World.GetRotation().getZ(), bp.m_World.GetRotation().getW());
//     }
// }

// static void PrintTracks(const dmRigDDF::AnimationTrack* tracks, uint32_t track_count)
// {
//     for (uint32_t i = 0; i < track_count; ++i)
//     {
//         const dmRigDDF::AnimationTrack* track = &tracks[i];

//         printf("Anim track %u  %p\n", i, track);
//         printf("   bone index: %u\n", track->m_BoneIndex);

//         for (uint32_t j = 0; j < track->m_Positions.m_Count/3; ++j)
//         {
//                 printf("   %u  t:  %f, %f, %f\n", j, track->m_Positions.m_Data[j*3+0],
//                                                      track->m_Positions.m_Data[j*3+1],
//                                                      track->m_Positions.m_Data[j*3+2]);
//         }

//         for (uint32_t j = 0; j < track->m_Rotations.m_Count/4; ++j)
//         {
//                 printf("   %u  r:  %f, %f, %f, %f\n", j, track->m_Rotations.m_Data[j*4+0],
//                                                      track->m_Rotations.m_Data[j*4+1],
//                                                      track->m_Rotations.m_Data[j*4+2],
//                                                      track->m_Rotations.m_Data[j*4+3]);
//         }
//     }
// }

static dmRigDDF::AnimationTrack* CreateAnimationTracks(const dmRigDDF::Bone* bones, uint32_t bone_count,
                                                        uint32_t num_samples,
                                                        const dmRigDDF::AnimationTrack* tracks, uint32_t track_count)
{
    dmRigDDF::AnimationTrack* out_tracks = new dmRigDDF::AnimationTrack[bone_count];
    for (uint32_t i = 0; i < bone_count; ++i)
    {
        const dmRigDDF::Bone* bone = &bones[i];
        dmRigDDF::AnimationTrack* out_track = &out_tracks[i];

        out_track->m_BoneIndex = i;

        out_track->m_Positions.m_Count = 3;
        out_track->m_Positions.m_Data = new float[out_track->m_Positions.m_Count];

        out_track->m_Rotations.m_Count = 4;
        out_track->m_Rotations.m_Data = new float[out_track->m_Rotations.m_Count];

        out_track->m_Scale.m_Count = 3;
        out_track->m_Scale.m_Data = new float[out_track->m_Scale.m_Count];

        for (uint32_t j = 0; j < 1; ++j)
        {
            out_track->m_Positions.m_Data[j*3+0] = bone->m_Local.GetTranslation().getX();
            out_track->m_Positions.m_Data[j*3+1] = bone->m_Local.GetTranslation().getY();
            out_track->m_Positions.m_Data[j*3+2] = bone->m_Local.GetTranslation().getZ();

            out_track->m_Rotations.m_Data[j*4+0] = bone->m_Local.GetRotation().getX();
            out_track->m_Rotations.m_Data[j*4+1] = bone->m_Local.GetRotation().getY();
            out_track->m_Rotations.m_Data[j*4+2] = bone->m_Local.GetRotation().getZ();
            out_track->m_Rotations.m_Data[j*4+3] = bone->m_Local.GetRotation().getW();

            out_track->m_Scale.m_Data[j*3+0] = bone->m_Local.GetScale().getX();
            out_track->m_Scale.m_Data[j*3+1] = bone->m_Local.GetScale().getY();
            out_track->m_Scale.m_Data[j*3+2] = bone->m_Local.GetScale().getZ();
        }
    }

    // Overwrite the keys of the tracks passed in
    for (uint32_t ti = 0; ti < track_count; ++ti)
    {
        const dmRigDDF::AnimationTrack* track = &tracks[ti];
        if (track->m_BoneIndex >= bone_count)
            continue;

        dmRigDDF::AnimationTrack* out_track = &out_tracks[track->m_BoneIndex];

        if (track->m_Positions.m_Count)
        {
            delete[] out_track->m_Positions.m_Data;
            out_track->m_Positions.m_Count = track->m_Positions.m_Count;
            out_track->m_Positions.m_Data = new float[track->m_Positions.m_Count];
            memcpy(out_track->m_Positions.m_Data, track->m_Positions.m_Data, track->m_Positions.m_Count * sizeof(float));
        }

        if (track->m_Rotations.m_Count)
        {
            delete[] out_track->m_Rotations.m_Data;
            out_track->m_Rotations.m_Count = track->m_Rotations.m_Count;
            out_track->m_Rotations.m_Data = new float[track->m_Rotations.m_Count];
            memcpy(out_track->m_Rotations.m_Data, track->m_Rotations.m_Data, track->m_Rotations.m_Count * sizeof(float));
        }

        if (track->m_Scale.m_Count)
        {
            delete[] out_track->m_Scale.m_Data;
            out_track->m_Scale.m_Count = track->m_Scale.m_Count;
            out_track->m_Scale.m_Data = new float[track->m_Scale.m_Count];
            memcpy(out_track->m_Scale.m_Data, track->m_Scale.m_Data, track->m_Scale.m_Count * sizeof(float));
        }
    }

    return out_tracks;
}

static void DeleteAnimationTracks(const dmRigDDF::AnimationTrack* tracks, uint32_t track_count)
{
    for (uint32_t i = 0; i < track_count; ++i)
    {
        const dmRigDDF::AnimationTrack* track = &tracks[i];
        if(track->m_Positions.m_Count)
            delete[] track->m_Positions.m_Data;
        if(track->m_Rotations.m_Count)
            delete[] track->m_Rotations.m_Data;
        if(track->m_Scale.m_Count)
            delete[] track->m_Scale.m_Data;
    }
}

void SetUpSimpleRig(dmArray<dmRig::RigBone>& bind_pose, dmRigDDF::Skeleton* skeleton, dmRigDDF::MeshSet* mesh_set, dmRigDDF::AnimationSet* animation_set)
{

    /*

        Note:
            - Skeleton has a depth first bone hierarchy, as expected by the engine.
            - See rig_ddf.proto for detailed information on skeleton, meshset and animationset
              decoupling and usage of bone lists.

        ------------------------------------

        Bones:

            (5)
             ^
             |
             |
            (4)
             ^        ^
             |        |
             |        |
            (3)       |
             ^        |
         B:  |        |
             |        |
            (0)---->(1,2)---->

            A:

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
                     ^
                     |
                     |
            (0)---->(1)

            III:

             ^
             |
             |
            (1)
             ^
             |
             |
            (0)


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

            Animation 2 (id: "invalid_bones") for IK on Bone B.

            Tries to animate a bone that does not exist.

        ------------------------------------

            Animation 3 (id: "rot_blend1")

                Has a "static" rotation of bone 1 -89 degrees on Z.

            Animation 4 (id: "rot_blend2")

                Has a "static" rotation of bone 1 +89 degrees on Z.

            Used to test bone rotation slerp.

        ------------------------------------

            Animation 5 (id: "trans_rot")

                Rotation and translation on bone 0.

            A:
                       ^
                       |
                       |
              x - - - (0)

            0: Pos; (10,0), rotation: 90 degrees around Z

        */

        uint32_t bone_count = 6;
        skeleton->m_Bones.m_Data = new dmRigDDF::Bone[bone_count];
        skeleton->m_Bones.m_Count = bone_count;

        // Bone 0
        dmRigDDF::Bone& bone0 = skeleton->m_Bones.m_Data[0];
        bone0.m_Parent       = dmRig::INVALID_BONE_INDEX;
        bone0.m_Id           = 0;
        bone0.m_Length       = 0.0f;
        bone0.m_Local.SetIdentity();
        bone0.m_InverseBindPose.SetIdentity();

        // Bone 1
        dmRigDDF::Bone& bone1 = skeleton->m_Bones.m_Data[1];
        bone1.m_Parent       = 0;
        bone1.m_Id           = 1;
        bone1.m_Length       = 1.0f;
        bone1.m_Local.SetIdentity();
        bone1.m_Local.SetTranslation(Vector3(1.0f, 0.0f, 0.0f));
        bone1.m_InverseBindPose.SetIdentity();

        // Bone 2
        dmRigDDF::Bone& bone2 = skeleton->m_Bones.m_Data[2];
        bone2.m_Parent       = 0;
        bone2.m_Id           = 2;
        bone2.m_Length       = 1.0f;
        bone2.m_Local.SetTranslation(Vector3(1.0f, 0.0f, 0.0f));
        bone2.m_Local.SetRotation(Quat::rotationZ((float)M_PI / 2.0f));
        bone2.m_Local.SetScale(Vector3(2.0f, 1.0f, 1.0f));
        bone2.m_InverseBindPose.SetIdentity();

        // Bone 3
        dmRigDDF::Bone& bone3 = skeleton->m_Bones.m_Data[3];
        bone3.m_Parent       = 0;
        bone3.m_Id           = 3;
        bone3.m_Length       = 1.0f;
        bone3.m_Local.SetIdentity();
        bone3.m_Local.SetTranslation(Vector3(0.0f, 1.0f, 0.0f));
        bone3.m_InverseBindPose.SetIdentity();

        // Bone 4
        dmRigDDF::Bone& bone4 = skeleton->m_Bones.m_Data[4];
        bone4.m_Parent       = 3;
        bone4.m_Id           = 4;
        bone4.m_Length       = 1.0f;
        bone4.m_Local.SetIdentity();
        bone4.m_Local.SetTranslation(Vector3(0.0f, 1.0f, 0.0f));
        bone4.m_InverseBindPose.SetIdentity();

        // Bone 5
        dmRigDDF::Bone& bone5 = skeleton->m_Bones.m_Data[5];
        bone5.m_Parent       = 4;
        bone5.m_Id           = 5;
        bone5.m_Length       = 1.0f;
        bone5.m_Local.SetIdentity();
        bone5.m_Local.SetTranslation(Vector3(0.0f, 1.0f, 0.0f));
        bone5.m_InverseBindPose.SetIdentity();

        bind_pose.SetCapacity(bone_count);
        bind_pose.SetSize(bone_count);

        CalcWorldTransforms(skeleton->m_Bones.m_Data, bone_count);

//PrintTransforms(skeleton->m_Bones.m_Data, bone_count);

        // Calculate bind pose
        dmRig::CopyBindPose(*skeleton, bind_pose);

        // Bone animations
        uint32_t animation_count = 6;
        animation_set->m_Animations.m_Data = new dmRigDDF::RigAnimation[animation_count];
        animation_set->m_Animations.m_Count = animation_count;
        dmRigDDF::RigAnimation& anim0 = animation_set->m_Animations.m_Data[0];
        dmRigDDF::RigAnimation& anim1 = animation_set->m_Animations.m_Data[1];
        dmRigDDF::RigAnimation& anim2 = animation_set->m_Animations.m_Data[2];
        dmRigDDF::RigAnimation& anim3 = animation_set->m_Animations.m_Data[3];
        dmRigDDF::RigAnimation& anim4 = animation_set->m_Animations.m_Data[4];
        dmRigDDF::RigAnimation& anim5 = animation_set->m_Animations.m_Data[5];
        anim0.m_Id = dmHashString64("valid");
        anim0.m_Duration            = 3.0f;
        anim0.m_SampleRate          = 1.0f;
        anim0.m_EventTracks.m_Count = 0;
        anim1.m_Id = dmHashString64("scaling");
        anim1.m_Duration            = 2.0f;
        anim1.m_SampleRate          = 1.0f;
        anim1.m_EventTracks.m_Count = 0;
        anim2.m_Id = dmHashString64("invalid_bones");
        anim2.m_Duration            = 3.0f;
        anim2.m_SampleRate          = 1.0f;
        anim2.m_EventTracks.m_Count = 0;
        anim3.m_Id = dmHashString64("rot_blend1");
        anim3.m_Duration            = 1.0f;
        anim3.m_SampleRate          = 1.0f;
        anim3.m_EventTracks.m_Count = 0;
        anim4.m_Id = dmHashString64("rot_blend2");
        anim4.m_Duration            = 1.0f;
        anim4.m_SampleRate          = 1.0f;
        anim4.m_EventTracks.m_Count = 0;
        anim5.m_Id = dmHashString64("trans_rot");
        anim5.m_Duration            = 1.0f;
        anim5.m_SampleRate          = 1.0f;
        anim5.m_EventTracks.m_Count = 0;

        // Animation 0: "valid"
        {
            uint32_t track_count = 2;
            dmRigDDF::AnimationTrack* tracks = new dmRigDDF::AnimationTrack[track_count];
            dmRigDDF::AnimationTrack& anim_track0 = tracks[0];
            dmRigDDF::AnimationTrack& anim_track1 = tracks[1];
            memset(&anim_track0, 0, sizeof(dmRigDDF::AnimationTrack));
            memset(&anim_track1, 0, sizeof(dmRigDDF::AnimationTrack));

            anim_track0.m_BoneIndex         = 0;
            anim_track1.m_BoneIndex         = 1;

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

            dmRigDDF::AnimationTrack* merged_tracks = CreateAnimationTracks(skeleton->m_Bones.m_Data, skeleton->m_Bones.m_Count, samples,
                                                                            tracks, track_count);
            anim0.m_Tracks.m_Data = merged_tracks;
            anim0.m_Tracks.m_Count = skeleton->m_Bones.m_Count;
            DeleteAnimationTracks(tracks, track_count);
            delete[] tracks;
        }

        // Animation 1: "scaling"
        {
            uint32_t track_count = 2; // 2x rotation, 1x scale
            dmRigDDF::AnimationTrack* tracks = new dmRigDDF::AnimationTrack[track_count];
            dmRigDDF::AnimationTrack& anim_track0 = tracks[0];
            dmRigDDF::AnimationTrack& anim_track1 = tracks[1];
            memset(&anim_track0, 0, sizeof(dmRigDDF::AnimationTrack));
            memset(&anim_track1, 0, sizeof(dmRigDDF::AnimationTrack));

            anim_track0.m_BoneIndex = 5;
            anim_track1.m_BoneIndex = 4;

            uint32_t samples = 3;
            anim_track0.m_Rotations.m_Data = new float[samples*4];
            anim_track0.m_Rotations.m_Count = samples*4;
            ((Quat*)anim_track0.m_Rotations.m_Data)[0] = Quat::identity();
            ((Quat*)anim_track0.m_Rotations.m_Data)[1] = Quat::rotationZ((float)M_PI / 2.0f);
            ((Quat*)anim_track0.m_Rotations.m_Data)[2] = Quat::rotationZ((float)M_PI / 2.0f);

            anim_track0.m_Scale.m_Data = new float[samples*3];
            anim_track0.m_Scale.m_Count = samples*3;
            anim_track0.m_Scale.m_Data[0] = 1.0f;
            anim_track0.m_Scale.m_Data[1] = 1.0f;
            anim_track0.m_Scale.m_Data[2] = 1.0f;
            anim_track0.m_Scale.m_Data[3] = 2.0f;
            anim_track0.m_Scale.m_Data[4] = 1.0f;
            anim_track0.m_Scale.m_Data[5] = 1.0f;
            anim_track0.m_Scale.m_Data[6] = 2.0f;
            anim_track0.m_Scale.m_Data[7] = 1.0f;
            anim_track0.m_Scale.m_Data[8] = 1.0f;

            anim_track1.m_Rotations.m_Data = new float[samples*4];
            anim_track1.m_Rotations.m_Count = samples*4;
            ((Quat*)anim_track1.m_Rotations.m_Data)[0] = Quat::identity();
            ((Quat*)anim_track1.m_Rotations.m_Data)[1] = Quat::rotationZ(-(float)M_PI / 2.0f);
            ((Quat*)anim_track1.m_Rotations.m_Data)[2] = Quat::rotationZ(-(float)M_PI / 2.0f);

            dmRigDDF::AnimationTrack* merged_tracks = CreateAnimationTracks(skeleton->m_Bones.m_Data, skeleton->m_Bones.m_Count, samples,
                                                                            tracks, track_count);
            anim1.m_Tracks.m_Data = merged_tracks;
            anim1.m_Tracks.m_Count = skeleton->m_Bones.m_Count;
            DeleteAnimationTracks(tracks, track_count);
            delete[] tracks;
        }

        // Animation 2: "invalid_bones"
        {
            uint32_t track_count = 1;
            dmRigDDF::AnimationTrack* tracks = new dmRigDDF::AnimationTrack[track_count];
            dmRigDDF::AnimationTrack& anim_track0 = tracks[0];
            memset(&anim_track0, 0, sizeof(dmRigDDF::AnimationTrack));

            anim_track0.m_BoneIndex = 6;

            uint32_t samples = 4;
            anim_track0.m_Rotations.m_Data = new float[samples*4];
            anim_track0.m_Rotations.m_Count = samples*4;
            ((Quat*)anim_track0.m_Rotations.m_Data)[0] = Quat::identity();
            ((Quat*)anim_track0.m_Rotations.m_Data)[1] = Quat::identity();
            ((Quat*)anim_track0.m_Rotations.m_Data)[2] = Quat::rotationZ((float)M_PI / 2.0f);
            ((Quat*)anim_track0.m_Rotations.m_Data)[3] = Quat::rotationZ((float)M_PI / 2.0f);

            dmRigDDF::AnimationTrack* merged_tracks = CreateAnimationTracks(skeleton->m_Bones.m_Data, skeleton->m_Bones.m_Count, samples,
                                                                            tracks, track_count);
            anim2.m_Tracks.m_Data = merged_tracks;
            anim2.m_Tracks.m_Count = skeleton->m_Bones.m_Count;
            DeleteAnimationTracks(tracks, track_count);
            delete[] tracks;
        }

        // Animation 3: "rot_blend1"
        {
            uint32_t track_count = 1;
            dmRigDDF::AnimationTrack* tracks = new dmRigDDF::AnimationTrack[track_count];
            dmRigDDF::AnimationTrack& anim_track0 = tracks[0];
            memset(&anim_track0, 0, sizeof(dmRigDDF::AnimationTrack));

            anim_track0.m_BoneIndex = 0;

            uint32_t samples = 2;
            anim_track0.m_Rotations.m_Data = new float[samples*4];
            anim_track0.m_Rotations.m_Count = samples*4;
            float rot_angle = ((float)M_PI / 180.0f) * (-89.0f);
            ((Quat*)anim_track0.m_Rotations.m_Data)[0] = Quat::rotationZ(rot_angle);
            ((Quat*)anim_track0.m_Rotations.m_Data)[1] = Quat::rotationZ(rot_angle);

            dmRigDDF::AnimationTrack* merged_tracks = CreateAnimationTracks(skeleton->m_Bones.m_Data, skeleton->m_Bones.m_Count, samples,
                                                                            tracks, track_count);
            anim3.m_Tracks.m_Data = merged_tracks;
            anim3.m_Tracks.m_Count = skeleton->m_Bones.m_Count;
            DeleteAnimationTracks(tracks, track_count);
            delete[] tracks;
        }

        // Animation 4: "rot_blend2"
        {
            uint32_t track_count = 1;
            dmRigDDF::AnimationTrack* tracks = new dmRigDDF::AnimationTrack[track_count];
            dmRigDDF::AnimationTrack& anim_track0 = tracks[0];
            memset(&anim_track0, 0, sizeof(dmRigDDF::AnimationTrack));

            anim_track0.m_BoneIndex = 0;

            uint32_t samples = 2;
            anim_track0.m_Rotations.m_Data = new float[samples*4];
            anim_track0.m_Rotations.m_Count = samples*4;
            float rot_angle = ((float)M_PI / 180.0f) * (89.0f);
            ((Quat*)anim_track0.m_Rotations.m_Data)[0] = Quat::rotationZ(rot_angle);
            ((Quat*)anim_track0.m_Rotations.m_Data)[1] = Quat::rotationZ(rot_angle);

            dmRigDDF::AnimationTrack* merged_tracks = CreateAnimationTracks(skeleton->m_Bones.m_Data, skeleton->m_Bones.m_Count, samples,
                                                                            tracks, track_count);
            anim4.m_Tracks.m_Data = merged_tracks;
            anim4.m_Tracks.m_Count = skeleton->m_Bones.m_Count;
            DeleteAnimationTracks(tracks, track_count);
            delete[] tracks;
        }

        // Animation 5: "trans_rot"
        {
            uint32_t track_count = 1;
            uint32_t samples = 2;

            dmRigDDF::AnimationTrack* tracks = new dmRigDDF::AnimationTrack[track_count];
            dmRigDDF::AnimationTrack& anim_track0 = tracks[0];
            memset(&anim_track0, 0, sizeof(dmRigDDF::AnimationTrack));

            anim_track0.m_BoneIndex = 0;

            anim_track0.m_Rotations.m_Data = new float[samples*4];
            anim_track0.m_Rotations.m_Count = samples*4;
            ((Quat*)anim_track0.m_Rotations.m_Data)[0] = Quat::rotationZ((float)M_PI / 2.0f);
            ((Quat*)anim_track0.m_Rotations.m_Data)[1] = Quat::rotationZ((float)M_PI / 2.0f);

            anim_track0.m_Positions.m_Data = new float[samples*3];
            anim_track0.m_Positions.m_Count = samples*3;
            anim_track0.m_Positions.m_Data[0] = 10.0f;
            anim_track0.m_Positions.m_Data[1] = 0.0f;
            anim_track0.m_Positions.m_Data[2] = 0.0f;
            anim_track0.m_Positions.m_Data[3] = 10.0f;
            anim_track0.m_Positions.m_Data[4] = 0.0f;
            anim_track0.m_Positions.m_Data[5] = 0.0f;

            dmRigDDF::AnimationTrack* merged_tracks = CreateAnimationTracks(skeleton->m_Bones.m_Data, skeleton->m_Bones.m_Count, samples,
                                                                            tracks, track_count);
            anim5.m_Tracks.m_Data = merged_tracks;
            anim5.m_Tracks.m_Count = skeleton->m_Bones.m_Count;
            DeleteAnimationTracks(tracks, track_count);
            delete[] tracks;
        }

        // Meshes / skins
        mesh_set->m_Models.m_Count = 2;
        mesh_set->m_Models.m_Data = new dmRigDDF::Model[mesh_set->m_Models.m_Count];

        mesh_set->m_Models.m_Data[0].m_Meshes.m_Data = new dmRigDDF::Mesh[1];
        mesh_set->m_Models.m_Data[0].m_Meshes.m_Count = 1;
        mesh_set->m_Models.m_Data[0].m_Id = dmHashString64("test");
        mesh_set->m_Models.m_Data[0].m_Local.SetIdentity();

        mesh_set->m_Models.m_Data[1].m_Meshes.m_Data = new dmRigDDF::Mesh[2];
        mesh_set->m_Models.m_Data[1].m_Meshes.m_Count = 2;
        mesh_set->m_Models.m_Data[1].m_Id = dmHashString64("test2");
        mesh_set->m_Models.m_Data[1].m_Local.SetIdentity();

        mesh_set->m_MaxBoneCount = bone_count + 1;

        // Create mesh data for skins
        CreateTestMesh(mesh_set, 0, 0, Vector4(0.0f));
        CreateTestMesh(mesh_set, 1, 0, Vector4(0.5f, 0.4f, 0.3f, 0.2f));
        CreateTestMesh(mesh_set, 1, 1, Vector4(1.0f, 0.9f, 0.8f, 0.7f));

        // We create bone lists for both the meshset and animationset,
        // that is in "inverted" order of the skeleton hirarchy.
        mesh_set->m_BoneList.m_Data = new uint64_t[bone_count];
        mesh_set->m_BoneList.m_Count = bone_count;
        animation_set->m_BoneList.m_Data = mesh_set->m_BoneList.m_Data;
        animation_set->m_BoneList.m_Count = bone_count;
        for (uint32_t i = 0; i < bone_count; ++i)
        {
            mesh_set->m_BoneList.m_Data[i] = bone_count-i-1;
        }
}

class RigContextTest : public jc_test_base_class
{
public:
    dmRig::HRigContext m_Context;

protected:
    virtual void SetUp() {
        dmRig::NewContextParams params = {0};
        params.m_MaxRigInstanceCount = 2;
        if (dmRig::RESULT_OK != dmRig::NewContext(params, &m_Context)) {
            dmLogError("Could not create rig context!");
        }
    }

    virtual void TearDown() {
        dmRig::DeleteContext(m_Context);
    }
};

template<typename T>
class RigContextCursorTest : public jc_test_params_class<T>
{
public:
    dmRig::HRigContext m_Context;

    virtual void SetUp() {
        dmRig::NewContextParams params = {0};
        params.m_MaxRigInstanceCount = 2;
        if (dmRig::RESULT_OK != dmRig::NewContext(params, &m_Context)) {
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

protected:
    virtual void SetUp() {
        RigContextCursorTest<T>::SetUp();

        m_Instance = 0x0;

        m_Skeleton     = new dmRigDDF::Skeleton();
        m_MeshSet      = new dmRigDDF::MeshSet();
        m_AnimationSet = new dmRigDDF::AnimationSet();
        SetUpSimpleRig(m_BindPose, m_Skeleton, m_MeshSet, m_AnimationSet);

        // Data
        dmRig::InstanceCreateParams create_params = {0};
        create_params.m_BindPose     = &m_BindPose;
        create_params.m_Skeleton     = m_Skeleton;
        create_params.m_MeshSet      = m_MeshSet;
        create_params.m_AnimationSet = m_AnimationSet;

        create_params.m_ModelId          = dmHashString64((const char*)"test");
        create_params.m_DefaultAnimation = dmHashString64((const char*)"");

        if (dmRig::RESULT_OK != dmRig::InstanceCreate(RigContextCursorTest<T>::m_Context, create_params, &m_Instance)) {
            dmLogError("Could not create rig instance!");
        }
    }

    virtual void TearDown() {
        if (dmRig::RESULT_OK != dmRig::InstanceDestroy(RigContextCursorTest<T>::m_Context, m_Instance)) {
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

INSTANTIATE_TEST_CASE_P(Rig, RigInstanceCursorForwardTest, jc_test_values_in(playback_modes_forward));
INSTANTIATE_TEST_CASE_P(Rig, RigInstanceCursorBackwardTest, jc_test_values_in(playback_modes_backward));
INSTANTIATE_TEST_CASE_P(Rig, RigInstanceCursorPingpongTest, jc_test_values_in(playback_modes_pingpong));

class RigInstanceTest : public RigContextTest
{
public:
    dmRig::HRigInstance     m_Instance;
    dmArray<dmRig::RigBone> m_BindPose;
    dmRigDDF::Skeleton*     m_Skeleton;
    dmRigDDF::MeshSet*      m_MeshSet;
    dmRigDDF::Mesh*         m_FirstMesh;
    dmRigDDF::AnimationSet* m_AnimationSet;

protected:
    virtual void SetUp() {
        RigContextTest::SetUp();

        m_Instance = 0x0;

        m_Skeleton     = new dmRigDDF::Skeleton();
        m_MeshSet      = new dmRigDDF::MeshSet();
        m_AnimationSet = new dmRigDDF::AnimationSet();
        SetUpSimpleRig(m_BindPose, m_Skeleton, m_MeshSet, m_AnimationSet);

        m_FirstMesh = &m_MeshSet->m_Models.m_Data[0].m_Meshes.m_Data[0];

        // Data
        dmRig::InstanceCreateParams create_params = {0};
        create_params.m_BindPose     = &m_BindPose;
        create_params.m_Skeleton     = m_Skeleton;
        create_params.m_MeshSet      = m_MeshSet;
        create_params.m_AnimationSet = m_AnimationSet;

        create_params.m_ModelId          = dmHashString64((const char*)"test");
        create_params.m_DefaultAnimation = dmHashString64((const char*)"");

        if (dmRig::RESULT_OK != dmRig::InstanceCreate(m_Context, create_params, &m_Instance)) {
            dmLogError("Could not create rig instance!");
        }
    }

    virtual void TearDown() {
        if (dmRig::RESULT_OK != dmRig::InstanceDestroy(m_Context, m_Instance)) {
            dmLogError("Could not delete rig instance!");
        }

        DeleteRigData(m_MeshSet, m_Skeleton, m_AnimationSet);

        RigContextTest::TearDown();
    }
};

TEST_F(RigContextTest, InstanceCreation)
{
    dmRig::HRigInstance instance = 0x0;

    // Dummy data
    dmArray<dmRig::RigBone> bind_pose;
    dmRig::InstanceCreateParams create_params = {0};
    create_params.m_BindPose     = &bind_pose;
    create_params.m_Skeleton     = new dmRigDDF::Skeleton();
    create_params.m_MeshSet      = new dmRigDDF::MeshSet();
    create_params.m_AnimationSet = new dmRigDDF::AnimationSet();

    create_params.m_ModelId          = dmHashString64((const char*)"dummy");
    create_params.m_DefaultAnimation = dmHashString64((const char*)"");

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::InstanceCreate(m_Context, create_params, &instance));
    ASSERT_NE((dmRig::HRigInstance)0x0, instance);

    delete create_params.m_Skeleton;
    delete create_params.m_MeshSet;
    delete create_params.m_AnimationSet;

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::InstanceDestroy(m_Context, instance));
}

TEST_F(RigContextTest, InvalidInstanceCreation)
{
    dmRig::HRigInstance instance0 = 0x0;
    dmRig::HRigInstance instance1 = 0x0;
    dmRig::HRigInstance instance2 = 0x0;
    dmArray<dmRig::RigBone> bind_pose;

    dmRig::InstanceCreateParams create_params = {0};
    create_params.m_BindPose     = &bind_pose;
    create_params.m_Skeleton     = new dmRigDDF::Skeleton();
    create_params.m_MeshSet      = new dmRigDDF::MeshSet();
    create_params.m_AnimationSet = new dmRigDDF::AnimationSet();
    create_params.m_ModelId            = dmHashString64((const char*)"dummy");
    create_params.m_DefaultAnimation = dmHashString64((const char*)"");

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::InstanceCreate(m_Context, create_params, &instance0));
    ASSERT_NE((dmRig::HRigInstance)0x0, instance0);

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::InstanceCreate(m_Context, create_params, &instance1));
    ASSERT_NE((dmRig::HRigInstance)0x0, instance1);

    ASSERT_EQ(dmRig::RESULT_ERROR_BUFFER_FULL, dmRig::InstanceCreate(m_Context, create_params, &instance2));
    ASSERT_EQ((dmRig::HRigInstance)0x0, instance2);

    delete create_params.m_Skeleton;
    delete create_params.m_MeshSet;
    delete create_params.m_AnimationSet;

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::InstanceDestroy(m_Context, instance0));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::InstanceDestroy(m_Context, instance1));
    ASSERT_EQ(dmRig::RESULT_ERROR, dmRig::InstanceDestroy(m_Context, instance2));
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

// TEST_F(RigInstanceTest, MaxBoneCount)
// {
//     // Call GenerateVertedData to setup m_ScratchInfluenceMatrixBuffer
//     ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0/60.0));
//     dmRig::RigModelVertex data[4];
//     dmRig::RigModelVertex* data_end = data + 4;
//     ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, m_FirstMesh, Matrix4::identity(), data));

//     // m_ScratchInfluenceMatrixBuffer should be able to contain the instance max bone count, which is the max of the used skeleton and meshset
//     // MaxBoneCount is set to BoneCount + 1 for testing.
//     ASSERT_EQ(m_Context->m_ScratchInfluenceMatrixBuffer.Size(), dmRig::GetMaxBoneCount(m_Instance));
//     ASSERT_EQ(m_Context->m_ScratchInfluenceMatrixBuffer.Size(), dmRig::GetBoneCount(m_Instance) + 1);

//     // Setting the m_ScratchInfluenceMatrixBuffer to zero ensures it have to be resized to max bone count
//     m_Context->m_ScratchInfluenceMatrixBuffer.SetCapacity(0);
//     // If this isn't done correctly, it'll assert out of bounds
//     ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0/60.0));
// }


TEST_F(RigInstanceTest, PoseNoAnim)
{
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0/60.0));

    dmArray<dmRig::BonePose>& pose = *dmRig::GetPose(m_Instance);

    // should be skeleton matrices
    ASSERT_EQ(Vector3(0.0f), pose[0].m_World.GetTranslation());
    ASSERT_EQ(Vector3(1.0f, 0.0f, 0.0f), pose[1].m_World.GetTranslation());
    ASSERT_EQ(Quat::identity(), pose[0].m_World.GetRotation());
    ASSERT_EQ(Quat::identity(), pose[1].m_World.GetRotation());

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0/60.0));

    ASSERT_EQ(Vector3(0.0f), pose[0].m_World.GetTranslation());
    ASSERT_EQ(Vector3(1.0f, 0.0f, 0.0f), pose[1].m_World.GetTranslation());
    ASSERT_EQ(Quat::identity(), pose[0].m_World.GetRotation());
    ASSERT_EQ(Quat::identity(), pose[1].m_World.GetRotation());
}

TEST_F(RigInstanceTest, PoseAnim)
{
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, 0.0f, 1.0f));

    dmArray<dmRig::BonePose>& pose = *dmRig::GetPose(m_Instance);

    // sample 0 ( the initial skeleton pose ))
    ASSERT_EQ(Vector3(0.0f, 0.0f, 0.0f), pose[0].m_World.GetTranslation());
    ASSERT_EQ(Quat::identity(), pose[0].m_World.GetRotation());
    ASSERT_EQ(Vector3(1.0f, 0.0f, 0.0f), pose[1].m_World.GetTranslation());
    ASSERT_EQ(Quat::identity(), pose[1].m_World.GetRotation());
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));

    // sample 1
    ASSERT_EQ(Vector3(0.0f, 0.0f, 0.0f), pose[0].m_World.GetTranslation());
    ASSERT_EQ(Quat::identity(), pose[0].m_World.GetRotation());
    ASSERT_EQ(Vector3(1.0f, 0.0f, 0.0f), pose[1].m_World.GetTranslation());
    ASSERT_EQ(Quat::rotationZ((float)M_PI / 2.0f), pose[1].m_World.GetRotation());

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));

    // sample 2
    ASSERT_EQ(Vector3(0.0f, 0.0f, 0.0f), pose[0].m_World.GetTranslation());
    ASSERT_EQ(Quat::rotationZ((float)M_PI / 2.0f), pose[0].m_World.GetRotation());
    ASSERT_EQ(Vector3(0.0f, 1.0f, 0.0f), pose[1].m_World.GetTranslation());
    ASSERT_EQ(Quat::rotationZ((float)M_PI / 2.0f), pose[1].m_World.GetRotation());

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));

    // sample 0 (looped)
    ASSERT_EQ(Vector3(0.0f, 0.0f, 0.0f), pose[0].m_World.GetTranslation());
    ASSERT_EQ(Quat::identity(), pose[0].m_World.GetRotation());
    ASSERT_EQ(Vector3(1.0f, 0.0f, 0.0f), pose[1].m_World.GetTranslation());
    ASSERT_EQ(Quat::identity(), pose[1].m_World.GetRotation());
}

TEST_F(RigInstanceTest, PoseAnimCancel)
{
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, 0.0f, 1.0f));

    dmArray<dmRig::BonePose>& pose = *dmRig::GetPose(m_Instance);

    // sample 0
    ASSERT_EQ(Vector3(0.0f), pose[0].m_World.GetTranslation());
    ASSERT_EQ(Vector3(1.0f, 0.0f, 0.0f), pose[1].m_World.GetTranslation());
    ASSERT_EQ(Quat::identity(), pose[0].m_World.GetRotation());
    ASSERT_EQ(Quat::identity(), pose[1].m_World.GetRotation());

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::CancelAnimation(m_Instance));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));

    // sample 1
    ASSERT_EQ(Vector3(0.0f), pose[0].m_World.GetTranslation());
    ASSERT_EQ(Vector3(1.0f, 0.0f, 0.0f), pose[1].m_World.GetTranslation());
    ASSERT_EQ(Quat::identity(), pose[0].m_World.GetRotation());
    ASSERT_EQ(Quat::identity(), pose[1].m_World.GetRotation());

}

// Test that blend between two rotation animations that their midway
// value is normalized and between the two rotations.
TEST_F(RigInstanceTest, BlendRotation)
{
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("rot_blend1"), dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, 0.0f, 1.0f));

    dmArray<dmRig::BonePose>& pose = *dmRig::GetPose(m_Instance);

    const Quat rot_1 = Quat::rotationZ((float)M_PI / 180.0f * (-89.0f));
    const Quat rot_2 = Quat::rotationZ((float)M_PI / 180.0f * (89.0f));
    const Quat rot_midway = Quat::identity();

    // Verify that the rotation is "static" -89 degrees.
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(rot_1, pose[0].m_World.GetRotation());
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(rot_1, pose[0].m_World.GetRotation());

    // Start second animation with 1 second blend
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("rot_blend2"), dmRig::PLAYBACK_LOOP_FORWARD, 1.0f, 0.0f, 1.0f));

    // Verify that the rotation is midway between -89 and 90 degrees on Z (ie identity).
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 0.5f));
    ASSERT_EQ(rot_midway, pose[0].m_World.GetRotation());

    // Blend is done, should be "static" at 89 degrees.
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(rot_2, pose[0].m_World.GetRotation());
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(rot_2, pose[0].m_World.GetRotation());
}

TEST_F(RigInstanceTest, GetVertexCount)
{
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(4u, dmRig::GetVertexCount(m_Instance));
}

#define ASSERT_VERT_POS(exp, act)\
    ASSERT_EQ(exp, Vector3(act.pos[0], act.pos[1], act.pos[2]));

#define ASSERT_VERT_NORM(exp, act)\
    ASSERT_EQ(exp, Vector3(act.normal[0], act.normal[1], act.normal[2]));

#define ASSERT_VERT_UV(exp_u, exp_v, act_u, act_v)\
    ASSERT_NEAR(exp_u, act_u, RIG_EPSILON_FLOAT);\
    ASSERT_NEAR(exp_v, act_v, RIG_EPSILON_FLOAT);\


TEST_F(RigInstanceTest, GenerateNormalData)
{
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, 0.0f, 1.0f));
    dmRig::RigModelVertex data[4];
    dmRig::RigModelVertex* data_end = data + 4;

    Vector3 n_up(0.0f, 1.0f, 0.0f);
    Vector3 n_down(0.0f, -1.0f, 0.0f);
    Vector3 n_neg_left(-1.0f, 0.0f, 0.0f);

    // sample 0
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, m_FirstMesh, Matrix4::identity(), data));
    ASSERT_VERT_NORM(n_up, data[0]); // v0
    ASSERT_VERT_NORM(n_up, data[1]); // v1
    ASSERT_VERT_NORM(n_neg_left, data[2]); // v2

    // sample 1
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, m_FirstMesh, Matrix4::identity(), data));
    ASSERT_VERT_NORM(n_up,        data[0]); // v0
    ASSERT_VERT_NORM(n_neg_left, data[1]); // v1
    ASSERT_VERT_NORM(n_neg_left, data[2]); // v2

    // sample 2
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, m_FirstMesh, Matrix4::identity(), data));
    ASSERT_VERT_NORM(n_neg_left, data[0]); // v0
    ASSERT_VERT_NORM(n_neg_left, data[1]); // v1
    ASSERT_VERT_NORM(n_down, data[2]); // v2
}

TEST_F(RigInstanceTest, SetModel)
{
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::SetModel(m_Instance, dmHashString64("test")));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, 0.0f, 1.0f));

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 0.0f));
    ASSERT_EQ(4u, GetVertexCount(m_Instance));

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::SetModel(m_Instance, dmHashString64("test2")));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 0.0f));
    ASSERT_EQ(8u, GetVertexCount(m_Instance));
}

TEST_F(RigInstanceTest, GenerateTexcoordData)
{
    // ASSERT_EQ(dmRig::RESULT_OK, dmRig::SetModel(m_Instance, dmHashString64("secondary_skin")));
    // ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("mesh_colors"), dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, 0.0f, 1.0f));
    dmRig::RigModelVertex data[4];
    dmRig::RigModelVertex* data_end = data + 4;

    // Trigger update which will recalculate mesh properties
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 0.0f));

    // sample 0
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, m_FirstMesh, Matrix4::identity(), data));
    ASSERT_VERT_UV(-1.0f, 2.0f, data[0].uv0[0], data[0].uv0[1]);
}

// Test to verify that two rigs does not interfere with each others pose information,
// by comparing their generated vertex data. This verifies fix DEF-2838.
TEST_F(RigInstanceTest, MultipleRigInfluences)
{
    // We need to setup a separate rig instance than m_Instance, but without any animation set.
    dmRig::HRigInstance second_instance = 0x0;

    dmRigDDF::Skeleton* skeleton     = new dmRigDDF::Skeleton();
    dmRigDDF::MeshSet* mesh_set      = new dmRigDDF::MeshSet();
    dmRigDDF::AnimationSet* animation_set = new dmRigDDF::AnimationSet();

    dmArray<dmRig::RigBone> bind_pose;
    SetUpSimpleRig(bind_pose, skeleton, mesh_set, animation_set);

    // Second rig instance data
    dmRig::InstanceCreateParams create_params = {0};
    create_params.m_BindPose         = &bind_pose;
    create_params.m_Skeleton         = skeleton;
    create_params.m_MeshSet          = mesh_set;
    create_params.m_ModelId          = dmHashString64((const char*)"test");
    create_params.m_DefaultAnimation = 0x0;

    // We deliberately set the animation set to NULL and the size of pose-to-influence array to 0.
    // This mimics as if there was no animation resource set for a model component.
    create_params.m_AnimationSet       = 0x0;

    if (dmRig::RESULT_OK != dmRig::InstanceCreate(m_Context, create_params, &second_instance)) {
        dmLogError("Could not create rig instance!");
    }

    // Play animation on first rig instance.
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("valid"), dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, 0.0f, 1.0f));

    dmRig::RigModelVertex data[4];
    dmRig::RigModelVertex* data_end = data + 4;

    // Comparison values
    Vector3 n_up(0.0f, 1.0f, 0.0f);
    Vector3 n_down(0.0f, -1.0f, 0.0f);
    Vector3 n_neg_left(-1.0f, 0.0f, 0.0f);

    // sample 0 - Both rigs are in their bind pose.
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, m_FirstMesh, Matrix4::identity(), data));
    ASSERT_VERT_NORM(n_up, data[0]);
    ASSERT_VERT_NORM(n_up, data[1]);
    ASSERT_VERT_NORM(n_neg_left, data[2]);
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, second_instance, m_FirstMesh, Matrix4::identity(), data));
    ASSERT_VERT_NORM(n_up, data[0]);
    ASSERT_VERT_NORM(n_up, data[1]);
    ASSERT_VERT_NORM(n_neg_left, data[2]);

    // sample 1 - First rig instance should be animating, while the second one should still be in its bind pose.
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, m_Instance, m_FirstMesh, Matrix4::identity(), data));
    ASSERT_VERT_NORM(n_up,       data[0]); // v0
    ASSERT_VERT_NORM(n_neg_left, data[1]); // v1
    ASSERT_VERT_NORM(n_neg_left, data[2]); // v2
    ASSERT_EQ(data_end, dmRig::GenerateVertexData(m_Context, second_instance, m_FirstMesh, Matrix4::identity(), data));
    ASSERT_VERT_NORM(n_up,       data[0]); // v0
    ASSERT_VERT_NORM(n_up,       data[1]); // v1
    ASSERT_VERT_NORM(n_neg_left, data[2]); // v2

    // Cleanup after second rig instance
    if (dmRig::RESULT_OK != dmRig::InstanceDestroy(m_Context, second_instance)) {
        dmLogError("Could not delete second rig instance!");
    }
    DeleteRigData(mesh_set, skeleton, animation_set);
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
/*
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

    dmArray<dmRig::BonePose>& pose = *dmRig::GetPose(m_Instance);

    ASSERT_EQ(Vector3(0.0f, 0.0f, 0.0f), pose[0].m_World.GetTranslation());
    ASSERT_EQ(Vector3(0.0f, 1.0f, 0.0f), pose[3].m_World.GetTranslation());
    ASSERT_EQ(Vector3(0.0f, 1.0f, 0.0f), pose[4].m_World.GetTranslation());
    ASSERT_EQ(Vector3(0.0f, 1.0f, 0.0f), pose[5].m_World.GetTranslation());
    ASSERT_EQ(Quat::identity(), pose[0].m_World.GetRotation());
    ASSERT_EQ(Quat::identity(), pose[4].m_World.GetRotation());
    ASSERT_EQ(Quat::identity(), pose[5].m_World.GetRotation());

    target->m_Position.setX(100.0f);
    target->m_Position.setY(1.0f);

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 0.0f));

    ASSERT_EQ(Vector3(0.0f, 0.0f, 0.0f), pose[0].m_World.GetTranslation());
    ASSERT_EQ(Vector3(0.0f, 1.0f, 0.0f), pose[3].m_World.GetTranslation());
    ASSERT_EQ(Vector3(0.0f, 1.0f, 0.0f), pose[4].m_World.GetTranslation());
    ASSERT_EQ(Vector3(0.0f, 1.0f, 0.0f), pose[5].m_World.GetTranslation());
    ASSERT_EQ(Quat::identity(), pose[0].m_World.GetRotation());
    ASSERT_EQ(Quat::rotationZ(-(float)M_PI / 2.0f), pose[4].m_World.GetRotation());
    ASSERT_EQ(Quat::identity(), pose[5].m_World.GetRotation());

    target->m_Position.setX(0.0f);
    target->m_Position.setY(-100.0f);

    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 0.0f));
    ASSERT_EQ(Quat::identity(), pose[0].m_World.GetRotation());
    ASSERT_EQ(Quat::rotationZ(-(float)M_PI), pose[4].m_World.GetRotation());
    ASSERT_EQ(Quat::identity(), pose[5].m_World.GetRotation());
}
*/


TEST_F(RigInstanceTest, BoneTranslationRotation)
{
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(m_Instance, dmHashString64("trans_rot"), dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, 0.0f, 1.0f));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    dmArray<dmRig::BonePose>& pose = *dmRig::GetPose(m_Instance);

    ASSERT_EQ(Vector3(10.0f, 0.0f, 0.0f), pose[0].m_World.GetTranslation());
    ASSERT_EQ(Quat::rotationZ((float)M_PI / 2.0f), pose[0].m_World.GetRotation());
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
    SetUpSimpleRig(bind_pose, skeleton, mesh_set, animation_set);

    // Create rig instance
    dmRig::InstanceCreateParams create_params = {0};
    create_params.m_BindPose         = &bind_pose;
    create_params.m_Skeleton         = skeleton;
    create_params.m_MeshSet          = mesh_set;
    create_params.m_ModelId          = dmHashString64((const char*)"test");
    create_params.m_DefaultAnimation = dmHashString64((const char*)"");
    create_params.m_EventCallback    = DEF3121_EventCallback;
    create_params.m_EventCBUserData1 = &instance;
    create_params.m_AnimationSet       = animation_set;
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::InstanceCreate(m_Context, create_params, &instance));

    // Play initial animation
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 1.0f));
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::PlayAnimation(instance, dmHashString64("valid"), dmRig::PLAYBACK_ONCE_FORWARD, 0.0f, 0.0f, 1.0f));

    // "valid" animation should start with root at origin
    dmArray<dmRig::BonePose>& pose = *dmRig::GetPose(instance);
    ASSERT_EQ(Vector3(0.0f), pose[0].m_World.GetTranslation());

    // Update rig instance, make sure we finish the animation which should trigger
    // the complete callback, which also will trigger a new animation, "trans_rot".
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 10.0f));

    // Verify that the position of the root bone is the same as previous frame.
    ASSERT_EQ(Vector3(0.0f), pose[0].m_World.GetTranslation());

    // One more update, this time the new animation should start playing.
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::Update(m_Context, 0.0f));

    // Verify that the position of the root bone is from the new animation.
    ASSERT_EQ(Vector3(10.0f, 0.0f, 0.0f), pose[0].m_World.GetTranslation());

    // Cleanup
    ASSERT_EQ(dmRig::RESULT_OK, dmRig::InstanceDestroy(m_Context, instance));
    DeleteRigData(mesh_set, skeleton, animation_set);
}

#undef ASSERT_VERT_POS
#undef ASSERT_VERT_NORM
#undef ASSERT_VERT_UV

int main(int argc, char **argv)
{
    dmHashEnableReverseHash(true);

    jc_test_init(&argc, argv);

    int ret = jc_test_run_all();
    return ret;
}
