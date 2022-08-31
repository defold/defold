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

#ifndef DMSDK_RIG_H
#define DMSDK_RIG_H

#include <stdint.h>

#include <dmsdk/dlib/array.h>
#include <dmsdk/dlib/align.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/transform.h>
#include <dmsdk/dlib/vmath.h>

#include <ddf/ddf.h>
#include <rig/rig_ddf.h>

namespace dmRig
{
    static const uint32_t INVALID_BONE_INDEX = 0xFFFFFFFF;

    typedef struct RigContext*  HRigContext;
    typedef struct RigInstance* HRigInstance;

    enum Result
    {
        RESULT_OK             = 0,
        RESULT_ERROR          = 1,
        RESULT_ERROR_BUFFER_FULL = 2,
        RESULT_ANIM_NOT_FOUND = 3,
        RESULT_UPDATED_POSE   = 4
    };

    enum RigPlayback
    {
        PLAYBACK_NONE          = 0,
        PLAYBACK_ONCE_FORWARD  = 1,
        PLAYBACK_ONCE_BACKWARD = 2,
        PLAYBACK_ONCE_PINGPONG = 3,
        PLAYBACK_LOOP_FORWARD  = 4,
        PLAYBACK_LOOP_BACKWARD = 5,
        PLAYBACK_LOOP_PINGPONG = 6,
        PLAYBACK_COUNT = 7,
    };

    typedef struct IKTarget IKTarget;
    typedef dmVMath::Vector3 (*RigIKTargetCallback)(IKTarget*);

    // IK targets can either use a static position or a callback (that is
    // called during the context update). A pointer to the IKTarget struct
    // is passed to the callback as the only argument. If the IK target
    // becomes invalid (for example the GO is removed in the collection,
    // or a GUI node in the GUI scene) it is up the callback to reset the
    // struct fields.
    struct IKTarget {
        float               m_Mix;
        /// Static IK target position
        dmVMath::Vector3    m_Position;
        /// Callback to dynamically set the IK target position. If NULL then the target isn't active.
        RigIKTargetCallback m_Callback;
        void*               m_UserPtr;
        dmhash_t            m_UserHash;
    };

    enum RigEventType
    {
        RIG_EVENT_TYPE_COMPLETED = 0,
        RIG_EVENT_TYPE_KEYFRAME  = 1
    };

    struct RigCompletedEventData
    {
        uint64_t  m_AnimationId;
        uint32_t  m_Playback;
    };

    struct RigKeyframeEventData
    {
        uint64_t  m_EventId;
        uint64_t  m_AnimationId;
        float     m_T;
        float     m_BlendWeight;
        int32_t   m_Integer;
        float     m_Float;
        uint64_t  m_String;
    };

    struct RigModelVertex
    {
        float pos[3];
        float normal[3];
        float tangent[3];
        float color[4];
        float uv0[2];
        float uv1[2];
    };

    // Can we not use the skeleton directly?
    struct RigBone
    {
        /// Inv model space transform
        dmVMath::Matrix4 m_ModelToLocal;

        /// Index of parent bone
        uint32_t m_ParentIndex;
        /// Length of the bone
        float m_Length;
    };

    struct BonePose
    {
        dmTransform::Transform  m_Local; // Local space transform
        dmTransform::Transform  m_World; // Model space transform (local to model space)
        dmVMath::Matrix4        m_Final; // m_World * InvBindPose
        uint32_t m_ParentIndex;          // Index of parent bone
        float m_Length;
    };

    struct NewContextParams {
        uint32_t     m_MaxRigInstanceCount;
    };

    typedef void (*RigEventCallback)(RigEventType, void*, void* userdata1, void* userdata2);
    typedef void (*RigPoseCallback)(void*, void*);

    struct InstanceCreateParams
    {
        dmhash_t                      m_ModelId;
        dmhash_t                      m_DefaultAnimation;

        const dmArray<struct RigBone>* m_BindPose;
        const dmRigDDF::Skeleton*     m_Skeleton;
        const dmRigDDF::MeshSet*      m_MeshSet;
        const dmRigDDF::AnimationSet* m_AnimationSet;

        RigPoseCallback               m_PoseCallback;
        void*                         m_PoseCBUserData1;
        void*                         m_PoseCBUserData2;
        RigEventCallback              m_EventCallback;
        void*                         m_EventCBUserData1;
        void*                         m_EventCBUserData2;

        bool                          m_ForceAnimatePose;
    };

    Result NewContext(const NewContextParams& params, HRigContext* context);
    void DeleteContext(HRigContext context);
    Result Update(HRigContext context, float dt);

    Result InstanceCreate(HRigContext context, const InstanceCreateParams& params, HRigInstance* instance);
    Result InstanceDestroy(HRigContext context, HRigInstance instance);

    Result PlayAnimation(HRigInstance instance, dmhash_t animation_id, RigPlayback playback, float blend_duration, float offset, float playback_rate);
    Result CancelAnimation(HRigInstance instance);
    dmhash_t GetAnimation(HRigInstance instance);

    // Returns the new position in the array
    RigModelVertex* GenerateVertexData(HRigContext context, dmRig::HRigInstance instance, dmRigDDF::Mesh* mesh, const dmVMath::Matrix4& world_matrix, RigModelVertex* vertex_data_out);
    uint32_t GetVertexCount(HRigInstance instance);

    Result SetModel(HRigInstance instance, dmhash_t model_id);
    dmhash_t GetModel(HRigInstance instance);

    float GetCursor(HRigInstance instance, bool normalized);
    Result SetCursor(HRigInstance instance, float cursor, bool normalized);
    float GetPlaybackRate(HRigInstance instance);
    Result SetPlaybackRate(HRigInstance instance, float playback_rate);
    dmArray<BonePose>* GetPose(HRigInstance instance);
    IKTarget* GetIKTarget(HRigInstance instance, dmhash_t constraint_id);
    bool ResetIKTarget(HRigInstance instance, dmhash_t constraint_id);
    void SetEnabled(HRigInstance instance, bool enabled);
    bool GetEnabled(HRigInstance instance);
    bool IsValid(HRigInstance instance);
    uint32_t GetBoneCount(HRigInstance instance);
    uint32_t GetMaxBoneCount(HRigInstance instance);
    void SetEventCallback(HRigInstance instance, RigEventCallback event_callback, void* user_data1, void* user_data2);

    // Util function used to fill a bind pose array from skeleton data
    // used in rig tests and loading rig resources.
    void CopyBindPose(dmRigDDF::Skeleton& skeleton, dmArray<RigBone>& bind_pose);
}

#endif // DMSDK_RIG_H
