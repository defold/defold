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

namespace dmRig
{
    Result NewContext(const NewContextParams& params, HRigContext* out)
    {
        return dmRig::RESULT_OK;
    }

    void DeleteContext(HRigContext context)
    { }

    Result PlayAnimation(HRigInstance instance, dmhash_t animation_id, dmRig::RigPlayback playback, float blend_duration, float offset, float playback_rate)
    {
        return dmRig::RESULT_OK;
    }

    Result CancelAnimation(HRigInstance instance)
    {
        return dmRig::RESULT_OK;
    }

    dmhash_t GetAnimation(HRigInstance instance)
    {
        return 0;
    }

    dmhash_t GetModel(HRigInstance instance)
    {
        return 0;
    }

    Result SetModel(HRigInstance instance, dmhash_t model_id)
    {
        return dmRig::RESULT_OK;
    }

    Result Update(HRigContext context, float dt)
    {
        return dmRig::RESULT_OK;
    }

    dmArray<BonePose>* GetPose(HRigInstance instance)
    {
        return 0;
    }


    float GetCursor(HRigInstance instance, bool normalized)
    {
        return 0.0f;
    }

    Result SetCursor(HRigInstance instance, float cursor, bool normalized)
    {
        return dmRig::RESULT_ERROR;
    }

    float GetPlaybackRate(HRigInstance instance)
    {
        return 1.0f;
    }

    Result SetPlaybackRate(HRigInstance instance, float playback_rate)
    {
        return dmRig::RESULT_OK;
    }

    uint32_t GetVertexCount(HRigInstance instance)
    {
        return 0;
    }

    uint8_t* WriteSingleVertexDataByAttributes(uint8_t* write_ptr, uint32_t idx, const dmGraphics::VertexAttributeInfos* attribute_infos, const float* positions, const float* normals, const float* tangents, const float* uv0, const float* uv1, const float* colors)
    {
        return 0;
    }

    uint8_t* GenerateVertexDataFromAttributes(dmRig::HRigContext context, dmRig::HRigInstance instance, dmRigDDF::Mesh* mesh, const dmVMath::Matrix4& world_matrix, const dmGraphics::VertexAttributeInfos* attribute_infos, uint32_t vertex_stride, uint8_t* vertex_data_out)
    {
        return 0;
    }

    RigModelVertex* GenerateVertexData(dmRig::HRigContext context, dmRig::HRigInstance instance, dmRigDDF::Mesh* mesh, const dmVMath::Matrix4& world_matrix, RigModelVertex* vertex_data_out)
    {
        return 0;
    }

    void SetEnabled(HRigInstance instance, bool enabled)
    {
    }

    bool GetEnabled(HRigInstance instance)
    {
        return true;
    }

    bool IsValid(HRigInstance instance)
    {
        return false;
    }

    uint32_t GetBoneCount(HRigInstance instance)
    {
        return 0;
    }

    uint32_t GetMaxBoneCount(HRigInstance instance)
    {
        return 0;
    }

    void SetEventCallback(HRigInstance instance, RigEventCallback event_callback, void* user_data1, void* user_data2)
    { }

    IKTarget* GetIKTarget(HRigInstance instance, dmhash_t constraint_id)
    {
        return 0x0;
    }

    bool ResetIKTarget(HRigInstance instance, dmhash_t constraint_id)
    {
        return true;
    }

    Result InstanceCreate(HRigContext context, const InstanceCreateParams& params, HRigInstance* out_instance)
    {
        return dmRig::RESULT_OK;
    }

    Result InstanceDestroy(HRigContext context, HRigInstance instance)
    {
        return dmRig::RESULT_OK;
    }

    void CopyBindPose(dmRigDDF::Skeleton& skeleton, dmArray<RigBone>& bind_pose)
    { }
}
