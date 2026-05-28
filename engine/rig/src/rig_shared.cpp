// Copyright 2020-2026 The Defold Foundation
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

#include "rig.h"

#include <dmsdk/ddf/ddf.h>
#include <dmsdk/dlib/array.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/hashtable.h>
#include <dmsdk/dlib/math.h>
#include <dmsdk/dlib/transform.h>
#include <dmsdk/dlib/vmath.h>
#include <graphics/graphics.h>

#include <string.h>

struct RigPreview
{
    dmRig::HRigContext          m_Context;
    dmRig::HRigInstance         m_Instance;
    dmRigDDF::Skeleton*         m_Skeleton;
    dmRigDDF::MeshSet*          m_MeshSet;
    dmRigDDF::AnimationSet*     m_AnimationSet;
    dmArray<dmRig::RigBone>     m_BindPose;
    dmHashTable64<uint32_t>     m_BoneIndices;
};

extern "C" DM_DLLEXPORT void Rig_DestroyPreview(RigPreview* preview);

namespace
{
    static void CreateBoneIndexMap(dmRigDDF::Skeleton* skeleton, dmHashTable64<uint32_t>* indices)
    {
        uint32_t size = skeleton->m_Bones.m_Count;
        if (indices->Capacity() < size)
            indices->SetCapacity(dmMath::Max((size * 2) / 3, 1U), size);

        for (uint32_t i = 0; i < size; ++i)
        {
            dmRigDDF::Bone* bone = &skeleton->m_Bones[i];
            indices->Put(bone->m_Id, i);
        }
    }

    template <typename T>
    static bool LoadMessage(const void* buffer, uint32_t buffer_size, const dmDDF::Descriptor* descriptor, T** message)
    {
        *message = 0;
        if (buffer == 0 || buffer_size == 0)
            return true;

        return dmDDF::LoadMessage(buffer, buffer_size, descriptor, (void**)message) == dmDDF::RESULT_OK;
    }

    static dmRigDDF::Model* GetModel(RigPreview* preview, uint32_t model_index)
    {
        if (preview == 0 || preview->m_MeshSet == 0 || model_index >= preview->m_MeshSet->m_Models.m_Count)
            return 0;
        return &preview->m_MeshSet->m_Models[model_index];
    }

    static dmRigDDF::Mesh* GetMesh(RigPreview* preview, uint32_t model_index, uint32_t mesh_index)
    {
        dmRigDDF::Model* model = GetModel(preview, model_index);
        if (model == 0 || mesh_index >= model->m_Meshes.m_Count)
            return 0;
        return &model->m_Meshes[mesh_index];
    }

    static void StoreMatrix(const dmVMath::Matrix4& matrix, float* out16)
    {
        for (uint32_t col = 0; col < 4; ++col)
        {
            dmVMath::Vector4 v = matrix.getCol(col);
            out16[col * 4 + 0] = v.getX();
            out16[col * 4 + 1] = v.getY();
            out16[col * 4 + 2] = v.getZ();
            out16[col * 4 + 3] = v.getW();
        }
    }
}

extern "C" DM_DLLEXPORT RigPreview* Rig_CreatePreview(const void* skeleton_buffer, uint32_t skeleton_buffer_size,
                                                      const void* mesh_set_buffer, uint32_t mesh_set_buffer_size,
                                                      const void* animation_set_buffer, uint32_t animation_set_buffer_size)
{
    RigPreview* preview = new RigPreview();
    memset(preview, 0, sizeof(RigPreview));

    if (!LoadMessage(skeleton_buffer, skeleton_buffer_size, &dmRigDDF_Skeleton_DESCRIPTOR, &preview->m_Skeleton) ||
        !LoadMessage(mesh_set_buffer, mesh_set_buffer_size, &dmRigDDF_MeshSet_DESCRIPTOR, &preview->m_MeshSet) ||
        !LoadMessage(animation_set_buffer, animation_set_buffer_size, &dmRigDDF_AnimationSet_DESCRIPTOR, &preview->m_AnimationSet) ||
        preview->m_MeshSet == 0 || preview->m_MeshSet->m_Models.m_Count == 0)
    {
        Rig_DestroyPreview(preview);
        return 0;
    }

    dmRig::NewContextParams context_params;
    memset(&context_params, 0, sizeof(context_params));
    context_params.m_MaxRigInstanceCount = 1;

    if (dmRig::NewContext(context_params, &preview->m_Context) != dmRig::RESULT_OK)
    {
        Rig_DestroyPreview(preview);
        return 0;
    }

    if (preview->m_Skeleton)
    {
        dmRig::CopyBindPose(*preview->m_Skeleton, preview->m_BindPose);
        CreateBoneIndexMap(preview->m_Skeleton, &preview->m_BoneIndices);
    }

    dmRig::InstanceCreateParams create_params;
    memset(&create_params, 0, sizeof(create_params));
    create_params.m_ModelId          = preview->m_MeshSet->m_Models[0].m_Id;
    create_params.m_DefaultAnimation = 0;
    create_params.m_BindPose         = preview->m_Skeleton ? &preview->m_BindPose : 0;
    create_params.m_BoneIndices      = preview->m_Skeleton ? &preview->m_BoneIndices : 0;
    create_params.m_Skeleton         = preview->m_Skeleton;
    create_params.m_MeshSet          = preview->m_MeshSet;
    create_params.m_AnimationSet     = preview->m_AnimationSet;
    create_params.m_ForceAnimatePose = 1;

    if (dmRig::InstanceCreate(preview->m_Context, create_params, &preview->m_Instance) != dmRig::RESULT_OK)
    {
        Rig_DestroyPreview(preview);
        return 0;
    }

    return preview;
}

extern "C" DM_DLLEXPORT void Rig_DestroyPreview(RigPreview* preview)
{
    if (preview == 0)
        return;

    if (preview->m_Context && preview->m_Instance)
        dmRig::InstanceDestroy(preview->m_Context, preview->m_Instance);
    if (preview->m_Context)
        dmRig::DeleteContext(preview->m_Context);
    if (preview->m_Skeleton)
        dmDDF::FreeMessage(preview->m_Skeleton);
    if (preview->m_MeshSet)
        dmDDF::FreeMessage(preview->m_MeshSet);
    if (preview->m_AnimationSet)
        dmDDF::FreeMessage(preview->m_AnimationSet);

    delete preview;
}

extern "C" DM_DLLEXPORT uint64_t Rig_Hash(const char* value)
{
    return dmHashString64(value);
}

extern "C" DM_DLLEXPORT int Rig_PlayAnimation(RigPreview* preview, uint64_t animation_id)
{
    if (preview == 0 || preview->m_Instance == 0)
        return dmRig::RESULT_ERROR;
    dmRig::SetCursor(preview->m_Instance, 0.0f, true);
    return dmRig::PlayAnimation(preview->m_Instance, animation_id, dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, 0.0f, 1.0f);
}

extern "C" DM_DLLEXPORT int Rig_CancelAnimation(RigPreview* preview)
{
    if (preview == 0 || preview->m_Instance == 0)
        return dmRig::RESULT_ERROR;
    return dmRig::CancelAnimation(preview->m_Instance);
}

extern "C" DM_DLLEXPORT int Rig_Update(RigPreview* preview, float dt)
{
    if (preview == 0 || preview->m_Context == 0)
        return dmRig::RESULT_ERROR;
    return dmRig::Update(preview->m_Context, dt);
}

extern "C" DM_DLLEXPORT void Rig_ResetPoseMatrixCache(RigPreview* preview)
{
    if (preview && preview->m_Context)
        dmRig::ResetPoseMatrixCache(preview->m_Context);
}

extern "C" DM_DLLEXPORT uint32_t Rig_AcquirePoseMatrixCacheEntry(RigPreview* preview)
{
    if (preview == 0 || preview->m_Context == 0 || preview->m_Instance == 0 || preview->m_Skeleton == 0)
        return dmRig::INVALID_POSE_MATRIX_CACHE_ENTRY;
    return dmRig::AcquirePoseMatrixCacheEntry(preview->m_Context, preview->m_Instance);
}

extern "C" DM_DLLEXPORT uint32_t Rig_GetPoseMatrixCacheDataOffset(RigPreview* preview)
{
    if (preview == 0 || preview->m_Context == 0 || preview->m_Instance == 0)
        return dmRig::INVALID_POSE_MATRIX_CACHE_ENTRY;
    return dmRig::GetPoseMatrixCacheDataOffset(preview->m_Context, preview->m_Instance);
}

extern "C" DM_DLLEXPORT bool Rig_HasPoseMatrixCacheAnimatedPose(RigPreview* preview)
{
    return preview && preview->m_Instance && dmRig::HasPoseMatrixCacheAnimatedPose(preview->m_Instance);
}

extern "C" DM_DLLEXPORT uint32_t Rig_GetBoneCount(RigPreview* preview)
{
    if (preview == 0 || preview->m_Instance == 0)
        return 0;
    return dmRig::GetBoneCount(preview->m_Instance);
}

extern "C" DM_DLLEXPORT uint32_t Rig_WritePoseMatrixCache(RigPreview* preview, float* out_rgba, uint32_t max_vec4_count)
{
    if (preview == 0 || preview->m_Context == 0 || out_rgba == 0)
        return 0;

    const dmVMath::Matrix4* pose_matrix_read_ptr = 0;
    uint32_t pose_matrix_count = 0;
    dmRig::GetPoseMatrixCacheData(preview->m_Context, &pose_matrix_read_ptr, &pose_matrix_count);

    uint32_t write_matrix_count = dmMath::Min(pose_matrix_count, max_vec4_count / 3);
    for (uint32_t i = 0; i < write_matrix_count; ++i)
    {
        const dmVMath::Matrix4& matrix = pose_matrix_read_ptr[i];
        const dmVMath::Vector4 col0 = matrix.getCol(0);
        const dmVMath::Vector4 col1 = matrix.getCol(1);
        const dmVMath::Vector4 col2 = matrix.getCol(2);
        const dmVMath::Vector4 col3 = matrix.getCol(3);
        float* write_ptr = out_rgba + i * 12;
        write_ptr[0] = col0.getX();
        write_ptr[1] = col0.getY();
        write_ptr[2] = col0.getZ();
        write_ptr[3] = col3.getX();
        write_ptr[4] = col1.getX();
        write_ptr[5] = col1.getY();
        write_ptr[6] = col1.getZ();
        write_ptr[7] = col3.getY();
        write_ptr[8] = col2.getX();
        write_ptr[9] = col2.getY();
        write_ptr[10] = col2.getZ();
        write_ptr[11] = col3.getZ();
    }

    return write_matrix_count * 3;
}

extern "C" DM_DLLEXPORT uint32_t Rig_GetDeindexedVertexCount(RigPreview* preview, uint32_t model_index, uint32_t mesh_index)
{
    dmRigDDF::Mesh* mesh = GetMesh(preview, model_index, mesh_index);
    if (mesh == 0)
        return 0;
    return mesh->m_IndicesFormat == dmRigDDF::INDEXBUFFER_FORMAT_32 ? mesh->m_Indices.m_Count / 4 : mesh->m_Indices.m_Count / 2;
}

extern "C" DM_DLLEXPORT uint32_t Rig_GenerateVertexData(RigPreview* preview, uint32_t model_index, uint32_t mesh_index,
                                                        const dmVMath::Matrix4& world_matrix, const dmVMath::Matrix4& normal_matrix,
                                                        const dmGraphics::VertexAttributeInfos& attribute_infos,
                                                        void* vertex_buffer, uint32_t vertex_buffer_size, uint32_t* out_vertex_buffer_size)
{
    if (out_vertex_buffer_size)
        *out_vertex_buffer_size = 0;

    dmRigDDF::Mesh* mesh = GetMesh(preview, model_index, mesh_index);
    if (preview == 0 || preview->m_Context == 0 || preview->m_Instance == 0 || mesh == 0 || vertex_buffer == 0)
        return dmRig::RESULT_ERROR;

    uint32_t vertex_count = Rig_GetDeindexedVertexCount(preview, model_index, mesh_index);
    uint32_t required_size = vertex_count * attribute_infos.m_VertexStride;
    if (vertex_buffer_size < required_size)
        return dmRig::RESULT_ERROR_BUFFER_FULL;

    uint8_t* begin = (uint8_t*)vertex_buffer;
    uint8_t* end = dmRig::GenerateVertexDataFromAttributes(preview->m_Context, preview->m_Instance, mesh, world_matrix, normal_matrix, &attribute_infos, attribute_infos.m_VertexStride, begin);
    if (out_vertex_buffer_size)
        *out_vertex_buffer_size = (uint32_t)(end - begin);
    return dmRig::RESULT_OK;
}

extern "C" DM_DLLEXPORT bool Rig_GetModelMatrix(RigPreview* preview, uint32_t model_index, bool use_bone_transform, float* out16)
{
    dmRigDDF::Model* model = GetModel(preview, model_index);
    if (model == 0 || out16 == 0)
        return false;

    dmVMath::Matrix4 model_matrix = dmTransform::ToMatrix4(model->m_Local);
    if (use_bone_transform && model->m_BoneId != 0 && preview->m_BoneIndices.Capacity() != 0)
    {
        uint32_t* bone_index = preview->m_BoneIndices.Get(model->m_BoneId);
        dmArray<dmRig::BonePose>* pose = dmRig::GetPose(preview->m_Instance);
        if (bone_index && *bone_index < pose->Size())
            model_matrix = dmTransform::ToMatrix4((*pose)[*bone_index].m_World) * model_matrix;
    }

    StoreMatrix(model_matrix, out16);
    return true;
}
