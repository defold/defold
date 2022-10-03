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

#include "rig.h"
#include "rig_private.h"

#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/vmath.h>
#include <dlib/profile.h>
#include <dmsdk/dlib/object_pool.h>
#include <graphics/graphics.h>


#include <stdio.h>

namespace dmRig
{
    using namespace dmVMath;

    static const dmhash_t NULL_ANIMATION = dmHashString64("");
    static const float CURSOR_EPSILON = 0.0001f;

    static void DoAnimate(HRigContext context, RigInstance* instance, float dt);
    static bool DoPostUpdate(RigInstance* instance);

    struct RigContext
    {
        dmObjectPool<HRigInstance>      m_Instances;
        // Temporary scratch buffers used for store pose as transform and matrices
        // (avoids modifying the real pose transform data during rendering).
        dmArray<dmVMath::Matrix4>       m_ScratchPoseMatrixBuffer;
        // Temporary scratch buffers used when transforming the vertex buffer,
        // used to creating primitives from indices.
        dmArray<dmVMath::Vector3>       m_ScratchPositionBuffer;
        dmArray<dmVMath::Vector3>       m_ScratchNormalBuffer;
        dmArray<dmVMath::Vector3>       m_ScratchTangentBuffer;
    };


    Result NewContext(const NewContextParams& params, HRigContext* out)
    {
        RigContext* context = new RigContext;
        if (!context) {
            return dmRig::RESULT_ERROR;
        }

        context->m_Instances.SetCapacity(params.m_MaxRigInstanceCount);
        context->m_ScratchPoseMatrixBuffer.SetCapacity(0);
        *out = context;
        return dmRig::RESULT_OK;
    }

    void DeleteContext(HRigContext context)
    {
        delete context;
    }

    static const dmRigDDF::RigAnimation* FindAnimation(const dmRigDDF::AnimationSet* anim_set, dmhash_t animation_id)
    {
        if(anim_set == 0x0)
            return 0x0;
        uint32_t anim_count = anim_set->m_Animations.m_Count;
        for (uint32_t i = 0; i < anim_count; ++i)
        {
            const dmRigDDF::RigAnimation* anim = &anim_set->m_Animations[i];
            if (anim->m_Id == animation_id)
            {
                return anim;
            }
        }
        return 0x0;
    }

    static RigPlayer* GetPlayer(HRigInstance instance)
    {
        return &instance->m_Players[instance->m_CurrentPlayer];
    }

    static RigPlayer* GetSecondaryPlayer(HRigInstance instance)
    {
        return &instance->m_Players[(instance->m_CurrentPlayer+1) % 2];
    }

    static RigPlayer* SwitchPlayer(HRigInstance instance)
    {
        instance->m_CurrentPlayer = (instance->m_CurrentPlayer + 1) % 2;
        return &instance->m_Players[instance->m_CurrentPlayer];
    }

    Result PlayAnimation(HRigInstance instance, dmhash_t animation_id, dmRig::RigPlayback playback, float blend_duration, float offset, float playback_rate)
    {
        const dmRigDDF::RigAnimation* anim = FindAnimation(instance->m_AnimationSet, animation_id);
        if (anim == 0x0)
        {
            RigPlayer* player = GetPlayer(instance);
            player->m_Playing = 0;
            return dmRig::RESULT_ANIM_NOT_FOUND;
        }

        if (blend_duration > 0.0f)
        {
            instance->m_BlendTimer = 0.0f;
            instance->m_BlendDuration = blend_duration;
            instance->m_Blending = 1;
        }
        else
        {
            RigPlayer* player = GetPlayer(instance);
            player->m_Playing = 0;
        }

        RigPlayer* player = SwitchPlayer(instance);
        player->m_AnimationId = animation_id;
        player->m_Animation = anim;
        player->m_Playing = 1;
        player->m_Playback = playback;

        if (player->m_Playback == dmRig::PLAYBACK_ONCE_BACKWARD || player->m_Playback == dmRig::PLAYBACK_LOOP_BACKWARD) {
            player->m_Backwards = 1;
            offset = 1.0f - dmMath::Clamp(offset, 0.0f, 1.0f);
        } else {
            player->m_Backwards = 0;
        }

        SetCursor(instance, offset, true);
        SetPlaybackRate(instance, playback_rate);
        return dmRig::RESULT_OK;
    }

    Result CancelAnimation(HRigInstance instance)
    {
        RigPlayer* player = GetPlayer(instance);
        player->m_Playing = 0;

        return dmRig::RESULT_OK;
    }

    dmhash_t GetAnimation(HRigInstance instance)
    {
        RigPlayer* player = GetPlayer(instance);
        return player->m_AnimationId;
    }

    dmhash_t GetModel(HRigInstance instance)
    {
        return instance->m_ModelId;
    }

    Result SetModel(HRigInstance instance, dmhash_t model_id)
    {
        for (uint32_t i = 0; i < instance->m_MeshSet->m_Models.m_Count; ++i)
        {
            const dmRigDDF::Model* model = &instance->m_MeshSet->m_Models[i];
            if (model->m_Id == model_id)
            {
                instance->m_Model = model;
                instance->m_ModelId = model_id;
                instance->m_DoRender = 1;
                return dmRig::RESULT_OK;
            }
        }
        instance->m_Model = 0;
        instance->m_ModelId = 0;
        instance->m_DoRender = 0;
        return dmRig::RESULT_ERROR;
    }

    static void UpdateBlend(RigInstance* instance, float dt)
    {
        if (instance->m_Blending)
        {
            instance->m_BlendTimer += dt;
            if (instance->m_BlendTimer >= instance->m_BlendDuration)
            {
                instance->m_Blending = 0;
                RigPlayer* secondary = GetSecondaryPlayer(instance);
                secondary->m_Playing = 0;
            }
        }
    }

    static float GetCursorDuration(RigPlayer* player, const dmRigDDF::RigAnimation* animation)
    {
        if (!animation)
        {
            return 0.0f;
        }

        float duration = animation->m_Duration;
        if (player->m_Playback == dmRig::PLAYBACK_ONCE_PINGPONG)
        {
            duration *= 2.0f;
        }
        return duration;
    }

    static void PostEventsInterval(HRigInstance instance, const dmRigDDF::RigAnimation* animation, float start_cursor, float end_cursor, float duration, bool backwards, float blend_weight)
    {
        const uint32_t track_count = animation->m_EventTracks.m_Count;
        for (uint32_t ti = 0; ti < track_count; ++ti)
        {
            const dmRigDDF::EventTrack* track = &animation->m_EventTracks[ti];
            const uint32_t key_count = track->m_Keys.m_Count;
            for (uint32_t ki = 0; ki < key_count; ++ki)
            {
                const dmRigDDF::EventKey* key = &track->m_Keys[ki];
                float cursor = key->m_T;
                if (backwards)
                    cursor = duration - cursor;
                if (start_cursor <= cursor && cursor < end_cursor)
                {
                    RigKeyframeEventData event_data;
                    event_data.m_EventId = track->m_EventId;
                    event_data.m_AnimationId = animation->m_Id;
                    event_data.m_BlendWeight = blend_weight;
                    event_data.m_T = key->m_T;
                    event_data.m_Integer = key->m_Integer;
                    event_data.m_Float = key->m_Float;
                    event_data.m_String = key->m_String;

                    instance->m_EventCallback(RIG_EVENT_TYPE_KEYFRAME, (void*)&event_data, instance->m_EventCBUserData1, instance->m_EventCBUserData2);
                }
            }
        }
    }

    static void PostEvents(HRigInstance instance, RigPlayer* player, const dmRigDDF::RigAnimation* animation, float dt, float prev_cursor, float duration, bool completed, float blend_weight)
    {
        float cursor = player->m_Cursor;
        // Since the intervals are defined as t0 <= t < t1, make sure we include the end of the animation, i.e. when t1 == duration
        if (completed)
            cursor += dt;
        // If the start cursor is greater than the end cursor, we have looped and handle that as two distinct intervals: [0,end_cursor) and [start_cursor,duration)
        // Note that for looping ping pong, one event can be triggered twice during the same frame by appearing in both intervals
        if (prev_cursor > cursor)
        {
            bool prev_backwards = player->m_Backwards;
            // Handle the flipping nature of ping pong
            if (player->m_Playback == dmRig::PLAYBACK_LOOP_PINGPONG)
            {
                prev_backwards = !player->m_Backwards;
            }
            PostEventsInterval(instance, animation, prev_cursor, duration, duration, prev_backwards, blend_weight);
            PostEventsInterval(instance, animation, 0.0f, cursor, duration, player->m_Backwards, blend_weight);
        }
        else
        {
            // Special handling when we reach the way back of once ping pong playback
            float half_duration = duration * 0.5f;
            if (player->m_Playback == dmRig::PLAYBACK_ONCE_PINGPONG && cursor > half_duration)
            {
                // If the previous cursor was still in the forward direction, treat it as two distinct intervals: [start_cursor,half_duration) and [half_duration, end_cursor)
                if (prev_cursor < half_duration)
                {
                    PostEventsInterval(instance, animation, prev_cursor, half_duration, duration, false, blend_weight);
                    PostEventsInterval(instance, animation, half_duration, cursor, duration, true, blend_weight);
                }
                else
                {
                    PostEventsInterval(instance, animation, prev_cursor, cursor, duration, true, blend_weight);
                }
            }
            else
            {
                PostEventsInterval(instance, animation, prev_cursor, cursor, duration, player->m_Backwards, blend_weight);
            }
        }
    }

    static void UpdatePlayer(RigInstance* instance, RigPlayer* player, float dt, float blend_weight)
    {
        const dmRigDDF::RigAnimation* animation = player->m_Animation;
        if (animation == 0x0 || !player->m_Playing)
        {
            return;
        }

        // Advance cursor
        float prev_cursor = player->m_Cursor;
        if (player->m_Playback != dmRig::PLAYBACK_NONE)
        {
            player->m_Cursor += dt * player->m_PlaybackRate;
        }
        float duration = GetCursorDuration(player, animation);
        if (duration == 0.0f)
        {
            player->m_Cursor = 0.0f;
        }

        // Adjust cursor
        bool completed = false;
        switch (player->m_Playback)
        {
        case dmRig::PLAYBACK_ONCE_FORWARD:
        case dmRig::PLAYBACK_ONCE_BACKWARD:
        case dmRig::PLAYBACK_ONCE_PINGPONG:
            if (player->m_Cursor >= duration)
            {
                player->m_Cursor = duration;
                completed = true;
            }
            break;
        case dmRig::PLAYBACK_LOOP_FORWARD:
        case dmRig::PLAYBACK_LOOP_BACKWARD:
            while (player->m_Cursor >= duration && duration > 0.0f)
            {
                player->m_Cursor -= duration;
            }
            break;
        case dmRig::PLAYBACK_LOOP_PINGPONG:
            while (player->m_Cursor >= duration && duration > 0.0f)
            {
                player->m_Cursor -= duration;
                player->m_Backwards = ~player->m_Backwards;
            }
            break;
        default:
            break;
        }

        if (prev_cursor != player->m_Cursor && instance->m_EventCallback)
        {
            PostEvents(instance, player, animation, dt, prev_cursor, duration, completed, blend_weight);
        }

        if (completed)
        {
            player->m_Playing = 0;
            // Only report completeness for the primary player
            if (player == GetPlayer(instance) && instance->m_EventCallback)
            {
                RigCompletedEventData event_data;
                event_data.m_AnimationId = player->m_AnimationId;
                event_data.m_Playback = player->m_Playback;

                instance->m_EventCallback(RIG_EVENT_TYPE_COMPLETED, (void*)&event_data, instance->m_EventCBUserData1, instance->m_EventCBUserData2);
            }
        }

    }

    static Vector3 SampleVec3(uint32_t sample, float frac, float* data)
    {
        uint32_t i0 = sample*3;
        uint32_t i1 = i0+3;
        return lerp(frac, Vector3(data[i0+0], data[i0+1], data[i0+2]), Vector3(data[i1+0], data[i1+1], data[i1+2]));
    }

    static Quat SampleQuat(uint32_t sample, float frac, float* data)
    {
        uint32_t i = sample*4;
        return slerp(frac, Quat(data[i+0], data[i+1], data[i+2], data[i+3]), Quat(data[i+0+4], data[i+1+4], data[i+2+4], data[i+3+4]));
    }

    static float CursorToTime(float cursor, float duration, bool backwards, bool once_pingpong)
    {
        float t = cursor;
        if (backwards)
            t = duration - t;
        if (once_pingpong && t > duration * 0.5f)
        {
            t = duration - t;
        }
        return t;
    }

    static void ApplyAnimation(RigPlayer* player, dmArray<BonePose>& pose, dmArray<IKAnimation>& ik_animation, float blend_weight)
    {
        const dmRigDDF::RigAnimation* animation = player->m_Animation;
        if (!animation)
            return;
        float duration = GetCursorDuration(player, animation);
        float t = CursorToTime(player->m_Cursor, duration, player->m_Backwards, player->m_Playback == dmRig::PLAYBACK_ONCE_PINGPONG);

        float fraction = t * animation->m_SampleRate;
        uint32_t sample = (uint32_t)fraction;
        fraction -= sample;
        // Sample animation tracks
        uint32_t track_count = animation->m_Tracks.m_Count;
        for (uint32_t ti = 0; ti < track_count; ++ti)
        {
            const dmRigDDF::AnimationTrack* track = &animation->m_Tracks[ti];
            uint32_t bone_index = track->m_BoneIndex;
            if (bone_index >= pose.Size()) {
                continue;
            }

            dmTransform::Transform& transform = pose[bone_index].m_Local;

            if (track->m_Positions.m_Count > 0)
            {
                Vector3 v;
                if (track->m_Positions.m_Count == 3)
                    v = Vector3(track->m_Positions.m_Data[0], track->m_Positions.m_Data[1], track->m_Positions.m_Data[2]);
                else
                    v = SampleVec3(sample, fraction, track->m_Positions.m_Data);

                transform.SetTranslation(lerp(blend_weight, transform.GetTranslation(), v));
            }
            if (track->m_Rotations.m_Count > 0)
            {
                Quat q;
                if (track->m_Rotations.m_Count == 4)
                    q = Quat(track->m_Rotations.m_Data[0], track->m_Rotations.m_Data[1], track->m_Rotations.m_Data[2], track->m_Rotations.m_Data[3]);
                else
                    q = SampleQuat(sample, fraction, track->m_Rotations.m_Data);

                transform.SetRotation(slerp(blend_weight, transform.GetRotation(), q));
            }
            if (track->m_Scale.m_Count > 0)
            {
                Vector3 s;
                if (track->m_Scale.m_Count == 3)
                    s = Vector3(track->m_Scale.m_Data[0], track->m_Scale.m_Data[1], track->m_Scale.m_Data[2]);
                else
                    s = SampleVec3(sample, fraction, track->m_Scale.m_Data);

                transform.SetScale(lerp(blend_weight, transform.GetScale(), s));
            }
        }
    }

    static void Animate(HRigContext context, float dt)
    {
        DM_PROFILE("RigAnimate");

        const dmArray<RigInstance*>& instances = context->m_Instances.m_Objects;
        uint32_t n = instances.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            RigInstance* instance = instances[i];
            DoAnimate(context, instance, dt);
        }
    }

    static void ResetPose(const dmRigDDF::Skeleton* skeleton, dmArray<BonePose>& pose)
    {
        uint32_t bone_count = pose.Size();
        for (uint32_t bi = 0; bi < bone_count; ++bi)
        {
            const dmRigDDF::Bone* bone = &skeleton->m_Bones[bi];
            pose[bi].m_Local = bone->m_Local;
            pose[bi].m_World.SetIdentity();
        }
    }

    static void UpdatePoseTransforms(dmArray<BonePose>& pose)
    {
        uint32_t bone_count = pose.Size();
        for (uint32_t bi = 0; bi < bone_count; ++bi)
        {
            BonePose& bp = pose[bi];

            if (bp.m_ParentIndex != INVALID_BONE_INDEX)
                bp.m_World = dmTransform::Mul(pose[bp.m_ParentIndex].m_World, bp.m_Local);
            else
                bp.m_World = bp.m_Local;
        }
    }

    static void PoseToMatrix(const dmArray<BonePose>& pose, dmArray<Matrix4>& out_matrices)
    {
        uint32_t bone_count = pose.Size();
        for (uint32_t bi = 0; bi < bone_count; ++bi)
        {
            out_matrices[bi] = dmTransform::ToMatrix4(pose[bi].m_World);
        }
    }

    static void DoAnimate(HRigContext context, RigInstance* instance, float dt)
    {
        // NOTE we previously checked for (!instance->m_Enabled || !instance->m_AddedToUpdate) here also
        RigPlayer* player = GetPlayer(instance);

        if (!player->m_Playing || !instance->m_Enabled || !player->m_Animation)
            return;

        const dmRigDDF::Skeleton* skeleton = instance->m_Skeleton;

        dmArray<BonePose>& pose = instance->m_Pose;
        ResetPose(skeleton, pose);

        // Reset IK animation
        dmArray<IKAnimation>& ik_animation = instance->m_IKAnimation;
        uint32_t ik_animation_count = ik_animation.Size();
        for (uint32_t ii = 0; ii < ik_animation_count; ++ii)
        {
            const dmRigDDF::IK* ik = &skeleton->m_Iks[ii];
            ik_animation[ii].m_Mix = ik->m_Mix;
            ik_animation[ii].m_Positive = ik->m_Positive;
        }

        UpdateBlend(instance, dt);

        if (instance->m_Blending)
        {
            float fade_rate = instance->m_BlendTimer / instance->m_BlendDuration;
            // How much to blend the pose, 1 first time to overwrite the bind pose, either fade_rate or 1 - fade_rate second depending on which one is the current player
            float alpha = 1.0f;
            for (uint32_t pi = 0; pi < 2; ++pi)
            {
                RigPlayer* p = &instance->m_Players[pi];
                // How much relative blending between the two players
                float blend_weight = fade_rate;
                if (player != p) {
                    blend_weight = 1.0f - fade_rate;
                }

                UpdatePlayer(instance, p, dt, blend_weight);
                ApplyAnimation(p, pose, ik_animation, alpha);
                if (player == p)
                {
                    alpha = 1.0f - fade_rate;
                }
                else
                {
                    alpha = fade_rate;
                }
            }
        }
        else
        {
            UpdatePlayer(instance, player, dt, 1.0f);
            ApplyAnimation(player, pose, ik_animation, 1.0f);
        }

        // Normalize quaternions while we blend
        if (instance->m_Blending)
        {
            uint32_t bone_count = pose.Size();
            for (uint32_t bi = 0; bi < bone_count; ++bi)
            {
                BonePose& bp = pose[bi];
                {
                    Quat rotation = bp.m_Local.GetRotation();
                    if (dot(rotation, rotation) > 0.001f)
                        rotation = normalize(rotation);
                    bp.m_Local.SetRotation(rotation);
                }
            }
        }

        UpdatePoseTransforms(pose);
    }

    static Result PostUpdate(HRigContext context)
    {
        const dmArray<RigInstance*>& instances = context->m_Instances.m_Objects;
        uint32_t count = instances.Size();
        bool updated_pose = false;
        for (uint32_t i = 0; i < count; ++i)
        {
            RigInstance* instance = instances[i];
            if (DoPostUpdate(instance)) {
                updated_pose = true;
            }
        }

        return updated_pose ? dmRig::RESULT_UPDATED_POSE : dmRig::RESULT_OK;
    }

    static bool DoPostUpdate(RigInstance* instance)
    {
            // If pose is empty, there are no bones to update
            dmArray<BonePose>& pose = instance->m_Pose;
            if (pose.Empty())
                return false;

            // Notify any listener that the pose has been recalculated
            if (instance->m_PoseCallback) {
                instance->m_PoseCallback(instance->m_PoseCBUserData1, instance->m_PoseCBUserData2);
                return true;
            }

        return false;
    }

    Result Update(HRigContext context, float dt)
    {
        DM_PROFILE("RigUpdate");

        Animate(context, dt);

        return PostUpdate(context);
    }

    static dmRig::Result CreatePose(HRigContext context, HRigInstance instance)
    {
        if(!instance->m_Skeleton)
            return dmRig::RESULT_OK;

        const dmRigDDF::Skeleton* skeleton = instance->m_Skeleton;
        uint32_t bone_count = skeleton->m_Bones.m_Count;
        instance->m_Pose.SetCapacity(bone_count);
        instance->m_Pose.SetSize(bone_count);
        for (uint32_t i = 0; i < bone_count; ++i)
        {
            const dmRigDDF::Bone* bone = &skeleton->m_Bones[i];
            instance->m_Pose[i].m_Length = bone->m_Length;
            instance->m_Pose[i].m_ParentIndex = bone->m_Parent;
            // If we won't play an animation, then use the skeleton transforms
            instance->m_Pose[i].m_Local = bone->m_Local;
            instance->m_Pose[i].m_World = bone->m_World;
        }

        instance->m_IKTargets.SetCapacity(skeleton->m_Iks.m_Count);
        instance->m_IKTargets.SetSize(skeleton->m_Iks.m_Count);
        memset(instance->m_IKTargets.Begin(), 0x0, instance->m_IKTargets.Size()*sizeof(IKTarget));

        instance->m_IKAnimation.SetCapacity(skeleton->m_Iks.m_Count);
        instance->m_IKAnimation.SetSize(skeleton->m_Iks.m_Count);

        return dmRig::RESULT_OK;
    }

    dmArray<BonePose>* GetPose(HRigInstance instance)
    {
        return &instance->m_Pose;
    }


    float GetCursor(HRigInstance instance, bool normalized)
    {
        RigPlayer* player = GetPlayer(instance);

        if (!player || !player->m_Animation)
        {
            return 0.0f;
        }

        float duration = player->m_Animation->m_Duration;
        if (duration == 0.0f)
        {
            return 0.0f;
        }

        float t = player->m_Cursor;
        if (player->m_Playback == dmRig::PLAYBACK_ONCE_PINGPONG && t > duration)
        {
            // In once-pingpong the cursor will be greater than duration during the "pong" part, compensate for that
            t = (2.f * duration) - t;
        }

        if (player->m_Backwards)
        {
            t = duration - t;
        }

        if (normalized)
        {
            t = t / duration;
        }
        return t;
    }

    Result SetCursor(HRigInstance instance, float cursor, bool normalized)
    {
        float t = cursor;
        RigPlayer* player = GetPlayer(instance);

        if (!player)
        {
            return dmRig::RESULT_ERROR;
        }

        if (!player->m_Animation)
        {
            return RESULT_OK;
        }

        float duration = player->m_Animation->m_Duration;
        if (normalized)
        {
            t = t * duration;
        }

        if (player->m_Playback == dmRig::PLAYBACK_LOOP_PINGPONG && player->m_Backwards)
        {
            // NEVER set cursor on the "looped" part of a pingpong animation
            player->m_Backwards = 0;
        }

        if (fabs(t) > duration)
        {
            t = fmod(t, duration);
            if (fabs(t) < CURSOR_EPSILON)
            {
                t = duration;
            }
        }

        if (t < 0.0f)
        {
            t = duration - fmod(fabs(t), duration);
        }

        if (player->m_Backwards)
        {
            t = duration - t;
        }

        player->m_Cursor = t;

        return dmRig::RESULT_OK;
    }

    float GetPlaybackRate(HRigInstance instance)
    {
        RigPlayer* player = GetPlayer(instance);

        if (!player || !player->m_Animation)
        {
            return 1.0f;
        }

        return player->m_PlaybackRate;
    }

    Result SetPlaybackRate(HRigInstance instance, float playback_rate)
    {
        RigPlayer* player = GetPlayer(instance);

        if (!player)
        {
            return dmRig::RESULT_ERROR;
        }

        player->m_PlaybackRate = dmMath::Max(playback_rate, 0.0f);

        return dmRig::RESULT_OK;
    }

    uint32_t GetVertexCount(HRigInstance instance)
    {
        if (!instance->m_Model || !instance->m_DoRender) {
            return 0;
        }

        uint32_t vertex_count = 0;
        for (uint32_t i = 0; i < instance->m_Model->m_Meshes.m_Count; ++i)
        {
            const dmRigDDF::Mesh& mesh = instance->m_Model->m_Meshes[i];

            vertex_count += mesh.m_Positions.m_Count/3;
        }

        return vertex_count;
    }

    static void GenerateNormalData(const dmRigDDF::Mesh* mesh, const Matrix4& normal_matrix, const dmArray<Matrix4>& pose_matrices, float* normals_buffer, float* tangents_buffer)
    {
        const float* normals_in = mesh->m_Normals.m_Data;
        bool has_tangents = mesh->m_Tangents.m_Count > 0;
        const float* tangents_in = has_tangents ? mesh->m_Tangents.m_Data : 0;
        const uint32_t vertex_count = mesh->m_Positions.m_Count / 3;
        Vector4 normal;
        Vector4 tangent;

        // Non skinned data
        if (!mesh->m_BoneIndices.m_Count || pose_matrices.Size() == 0)
        {
            for (uint32_t i = 0; i < vertex_count; ++i)
            {
                Vector3 normal_in(normals_in[i*3+0], normals_in[i*3+1], normals_in[i*3+2]);
                normal = normal_matrix * normal_in;
                if (lengthSqr(normal) > 0.0f) {
                    normalize(normal);
                }

                *normals_buffer++ = normal[0];
                *normals_buffer++ = normal[1];
                *normals_buffer++ = normal[2];

                if (has_tangents)
                {
                    Vector3 tangent_in(tangents_in[i*3+0], tangents_in[i*3+1], tangents_in[i*3+2]);
                    tangent = normal_matrix * tangent_in;
                    if (lengthSqr(tangent) > 0.0f) {
                        normalize(tangent);
                    }
                    *tangents_buffer++ = tangent[0];
                    *tangents_buffer++ = tangent[1];
                    *tangents_buffer++ = tangent[2];
                }
            }
            return;
        }

        // Skinned data
        const uint32_t* indices = mesh->m_BoneIndices.m_Data;
        const float* weights = mesh->m_Weights.m_Data;
        for (uint32_t i = 0; i < vertex_count; ++i)
        {
            const Vector3 normal_in(normals_in[i*3+0], normals_in[i*3+1], normals_in[i*3+2]);
            Vector4 normal_out(0.0f, 0.0f, 0.0f, 0.0f);

            const Vector3 tangent_in = has_tangents ? Vector3(tangents_in[i*3+0], tangents_in[i*3+1], tangents_in[i*3+2]) : Vector3(0,0,0);
            Vector4 tangent_out(0.0f, 0.0f, 0.0f, 0.0f);

            const uint32_t bi_offset = i * 4;
            const uint32_t* bone_indices = &indices[bi_offset];
            const float* bone_weights = &weights[bi_offset];

            if (bone_weights[0])
            {
                normal_out += (pose_matrices[bone_indices[0]] * normal_in) * bone_weights[0];
                tangent_out += (pose_matrices[bone_indices[0]] * tangent_in) * bone_weights[0];
                if (bone_weights[1])
                {
                    normal_out += (pose_matrices[bone_indices[1]] * normal_in) * bone_weights[1];
                    tangent_out += (pose_matrices[bone_indices[1]] * tangent_in) * bone_weights[1];
                    if (bone_weights[2])
                    {
                        normal_out += (pose_matrices[bone_indices[2]] * normal_in) * bone_weights[2];
                        tangent_out += (pose_matrices[bone_indices[2]] * tangent_in) * bone_weights[2];
                        if (bone_weights[3])
                        {
                            normal_out += (pose_matrices[bone_indices[3]] * normal_in) * bone_weights[3];
                            tangent_out += (pose_matrices[bone_indices[3]] * tangent_in) * bone_weights[3];
                        }
                    }
                }
            }

            normal = normal_matrix * normal_out.getXYZ();
            if (lengthSqr(normal) > 0.0f) {
                normalize(normal);
            }
            *normals_buffer++ = normal[0];
            *normals_buffer++ = normal[1];
            *normals_buffer++ = normal[2];

            if (has_tangents)
            {
                tangent = normal_matrix * normal_out;
                if (lengthSqr(tangent) > 0.0f) {
                    normalize(tangent);
                }
                *tangents_buffer++ = tangent[0];
                *tangents_buffer++ = tangent[1];
                *tangents_buffer++ = tangent[2];
            }
        }
    }

    static float* GeneratePositionData(const dmRigDDF::Mesh* mesh, const Matrix4& model_matrix, const dmArray<Matrix4>& pose_matrices, float* out_buffer)
    {
        const float* positions = mesh->m_Positions.m_Data;
        const uint32_t vertex_count = mesh->m_Positions.m_Count / 3;
        Point3 in_p;
        Vector4 v;

        if(!mesh->m_BoneIndices.m_Count || pose_matrices.Size() == 0)
        {
            for (uint32_t i = 0; i < vertex_count; ++i)
            {
                in_p[0] = *positions++;
                in_p[1] = *positions++;
                in_p[2] = *positions++;
                v = model_matrix * in_p;
                *out_buffer++ = v[0];
                *out_buffer++ = v[1];
                *out_buffer++ = v[2];
            }
            return out_buffer;
        }

        const uint32_t* indices = mesh->m_BoneIndices.m_Data;
        const float* weights = mesh->m_Weights.m_Data;
        for (uint32_t i = 0; i < vertex_count; ++i)
        {
            Vector4 in_v;
            in_v.setX(*positions++);
            in_v.setY(*positions++);
            in_v.setZ(*positions++);
            in_v.setW(1.0f);

            Vector4 out_p(0.0f, 0.0f, 0.0f, 0.0f);
            const uint32_t bi_offset = i * 4;
            const uint32_t* bone_indices = &indices[bi_offset];
            const float* bone_weights = &weights[bi_offset];

            if(bone_weights[0])
            {
                out_p += pose_matrices[bone_indices[0]] * in_v * bone_weights[0];
                if(bone_weights[1])
                {
                    out_p += pose_matrices[bone_indices[1]] * in_v * bone_weights[1];
                    if(bone_weights[2])
                    {
                        out_p += pose_matrices[bone_indices[2]] * in_v * bone_weights[2];
                        if(bone_weights[3])
                        {
                            out_p += pose_matrices[bone_indices[3]] * in_v * bone_weights[3];
                        }
                    }
                }
            }

            v = model_matrix * Point3(out_p.getX(), out_p.getY(), out_p.getZ());
            *out_buffer++ = v[0];
            *out_buffer++ = v[1];
            *out_buffer++ = v[2];
        }
        return out_buffer;
    }

    static RigModelVertex* WriteVertexData(const dmRigDDF::Mesh* mesh, const float* positions, const float* normals, const float* tangents, RigModelVertex* out_write_ptr)
    {
        uint32_t vertex_count = mesh->m_Positions.m_Count / 3;

        const float* uv0 = mesh->m_Texcoord0.m_Count ? mesh->m_Texcoord0.m_Data : 0;
        const float* uv1 = mesh->m_Texcoord1.m_Count ? mesh->m_Texcoord1.m_Data : 0;
        const float* colors = mesh->m_Colors.m_Count ? mesh->m_Colors.m_Data : 0;

        if (mesh->m_Indices.m_Count == 0)
        {
            for (uint32_t i = 0; i < vertex_count; ++i)
            {
                for (int c = 0; c < 3; ++c)
                {
                    out_write_ptr->pos[c] = *positions++;
                    out_write_ptr->normal[c] = *normals++;
                    out_write_ptr->tangent[c] = *tangents++;
                }

                for (int c = 0; c < 4; ++c)
                {
                    out_write_ptr->color[c] = colors ? *colors++ : 1.0f;
                }

                for (int c = 0; c < 2; ++c)
                {
                    out_write_ptr->uv0[c] = uv0 ? *uv0++ : 0.0f;
                    out_write_ptr->uv1[c] = uv1 ? *uv1++ : 0.0f;
                }

                out_write_ptr++;
            }
        }
        else { // Use the index buffer to write out all the vertices
            uint32_t* indices32 = 0;
            uint16_t* indices16 = 0;
            uint32_t num_indices;
            if (mesh->m_IndicesFormat == dmRigDDF::INDEXBUFFER_FORMAT_32)
            {
                indices32 = (uint32_t*)mesh->m_Indices.m_Data;
                num_indices = mesh->m_Indices.m_Count / 4;
            }
            else
            {
                indices16 = (uint16_t*)mesh->m_Indices.m_Data;
                num_indices = mesh->m_Indices.m_Count / 2;
            }

            for (uint32_t i = 0; i < num_indices; ++i)
            {
                uint32_t idx = indices32?indices32[i]:indices16[i];

                for (int c = 0; c < 3; ++c)
                {
                    out_write_ptr->pos[c] = positions[idx*3+c];
                    out_write_ptr->normal[c] = normals[idx*3+c];
                    out_write_ptr->tangent[c] = tangents[idx*3+c];
                }

                for (int c = 0; c < 4; ++c)
                {
                    out_write_ptr->color[c] = colors ? colors[idx*4+c] : 1.0f;
                }

                for (int c = 0; c < 2; ++c)
                {
                    out_write_ptr->uv0[c] = uv0 ? uv0[idx*2+c] : 0.0f;
                    out_write_ptr->uv1[c] = uv1 ? uv1[idx*2+c] : 0.0f;
                }

                out_write_ptr++;
            }
        }

        return out_write_ptr;
    }

    static void EnsureSize(dmArray<Vector3>& array, uint32_t size)
    {
        if (array.Capacity() < size) {
            array.OffsetCapacity(size - array.Capacity());
        }
        array.SetSize(size);
    }

    RigModelVertex* GenerateVertexData(dmRig::HRigContext context, dmRig::HRigInstance instance, dmRigDDF::Mesh* mesh, const Matrix4& world_matrix, RigModelVertex* vertex_data_out)
    {
        // TODO: Separate out the instance part.
        // to do that we need to pass in the updated pose matrices
        const dmRigDDF::Model* model = instance->m_Model;

        if (!model || !mesh || !instance->m_DoRender) {
            return vertex_data_out;
        }

        dmArray<Matrix4>& pose_matrices      = context->m_ScratchPoseMatrixBuffer;
        dmArray<Vector3>& positions          = context->m_ScratchPositionBuffer;
        dmArray<Vector3>& normals            = context->m_ScratchNormalBuffer;
        dmArray<Vector3>& tangents           = context->m_ScratchTangentBuffer;

        // If the rig has bones, update the pose to be local-to-model
        uint32_t bone_count = GetBoneCount(instance);
        if (bone_count)
        {
            // Make sure pose scratch buffers have enough space
            if (pose_matrices.Capacity() < bone_count) {
                uint32_t size_offset = bone_count - pose_matrices.Capacity();
                pose_matrices.OffsetCapacity(size_offset);
            }
            pose_matrices.SetSize(bone_count);

            PoseToMatrix(instance->m_Pose, pose_matrices);

            // Premultiply pose matrices with the bind pose inverse so they
            // can be directly be used to transform each vertex.
            const dmArray<RigBone>& bind_pose = *instance->m_BindPose;
            for (uint32_t bi = 0; bi < pose_matrices.Size(); ++bi)
            {
                Matrix4& pose_matrix = pose_matrices[bi];
                pose_matrix = pose_matrix * bind_pose[bi].m_ModelToLocal;
            }
        } else {
            pose_matrices.SetSize(0);
        }

        Matrix4 normal_matrix = Vectormath::Aos::inverse(world_matrix);
        normal_matrix = Vectormath::Aos::transpose(normal_matrix);

        // TODO: Currently, we only have support for a single material so we bake all meshes into one
        uint32_t vertex_count = mesh->m_Positions.m_Count / 3;

        // Bump scratch buffers capacity to handle current vertex count
        EnsureSize(positions, vertex_count);
        EnsureSize(normals, vertex_count);
        EnsureSize(tangents, vertex_count);

        float* positions_buffer = (float*)positions.Begin();
        float* normals_buffer = (float*)normals.Begin();
        float* tangents_buffer = (float*)tangents.Begin();

        // Transform the mesh data into world space
        dmRig::GeneratePositionData(mesh, world_matrix, pose_matrices, positions_buffer);
        if (mesh->m_Normals.m_Count) {
            dmRig::GenerateNormalData(mesh, normal_matrix, pose_matrices, normals_buffer, tangents_buffer);
        }

        return WriteVertexData(mesh, positions_buffer, normals_buffer, tangents_buffer, vertex_data_out);
    }

    static uint32_t FindIKIndex(HRigInstance instance, dmhash_t ik_constraint_id)
    {
        const dmRigDDF::Skeleton* skeleton = instance->m_Skeleton;
        uint32_t ik_count = skeleton->m_Iks.m_Count;
        uint32_t ik_index = ~0u;
        for (uint32_t i = 0; i < ik_count; ++i)
        {
            if (skeleton->m_Iks[i].m_Id == ik_constraint_id)
            {
                ik_index = i;
                break;
            }
        }
        return ik_index;
    }

    void SetEnabled(HRigInstance instance, bool enabled)
    {
        instance->m_Enabled = enabled;
    }

    bool GetEnabled(HRigInstance instance)
    {
        return instance->m_Enabled;
    }

    bool IsValid(HRigInstance instance)
    {
        return instance->m_Model != 0;
    }

    uint32_t GetBoneCount(HRigInstance instance)
    {
        if (instance == 0x0 || instance->m_Skeleton == 0x0) {
            return 0;
        }

        return instance->m_Skeleton->m_Bones.m_Count;
    }

    uint32_t GetMaxBoneCount(HRigInstance instance)
    {
        if (instance == 0x0) {
            return 0;
        }
        return instance->m_MaxBoneCount;
    }

    void SetEventCallback(HRigInstance instance, RigEventCallback event_callback, void* user_data1, void* user_data2)
    {
        if (!instance) {
            return;
        }

        instance->m_EventCallback = event_callback;
        instance->m_EventCBUserData1 = user_data1;
        instance->m_EventCBUserData2 = user_data2;
    }

    IKTarget* GetIKTarget(HRigInstance instance, dmhash_t constraint_id)
    {
        if (!instance) {
            return 0x0;
        }
        uint32_t ik_index = FindIKIndex(instance, constraint_id);
        if (ik_index == ~0u) {
            dmLogError("Could not find IK constraint (%llu)", (unsigned long long)constraint_id);
            return 0x0;
        }

        return &instance->m_IKTargets[ik_index];
    }

    bool ResetIKTarget(HRigInstance instance, dmhash_t constraint_id)
    {
        if (!instance) {
            return false;
        }

        uint32_t ik_index = FindIKIndex(instance, constraint_id);
        if (ik_index == ~0u) {
            dmLogError("Could not find IK constraint (%llu)", (unsigned long long)constraint_id);
            return false;
        }

        // Clear target fields, see DoAnimate function of the fields usage.
        // If callback is NULL it is considered not active, clear rest of fields
        // to avoid confusion.
        IKTarget* target = &instance->m_IKTargets[ik_index];
        target->m_Callback = 0x0;
        target->m_Mix = 0.0f;
        target->m_UserPtr = 0x0;
        target->m_UserHash = 0x0;

        return true;
    }

    static void DestroyInstance(HRigContext context, uint32_t index)
    {
        RigInstance* instance = context->m_Instances.Get(index);
        // If we're going to use memset, then we should explicitly clear pose and instance arrays.
        instance->m_Pose.SetCapacity(0);
        instance->m_IKTargets.SetCapacity(0);
        delete instance;
        context->m_Instances.Free(index, true);
    }

    Result InstanceCreate(HRigContext context, const InstanceCreateParams& params, HRigInstance* out_instance)
    {
        if (context->m_Instances.Full())
        {
            dmLogError("Rig instance could not be created since the buffer is full (%d).", context->m_Instances.Capacity());
            return dmRig::RESULT_ERROR_BUFFER_FULL;
        }

        RigInstance* instance = new RigInstance;

        uint32_t index = context->m_Instances.Alloc();
        memset(instance, 0, sizeof(RigInstance));
        instance->m_Index = index;
        context->m_Instances.Set(index, instance);
        instance->m_ModelId = params.m_ModelId;

        instance->m_PoseCallback     = params.m_PoseCallback;
        instance->m_PoseCBUserData1  = params.m_PoseCBUserData1;
        instance->m_PoseCBUserData2  = params.m_PoseCBUserData2;
        instance->m_EventCallback    = params.m_EventCallback;
        instance->m_EventCBUserData1 = params.m_EventCBUserData1;
        instance->m_EventCBUserData2 = params.m_EventCBUserData2;

        instance->m_BindPose           = params.m_BindPose;
        instance->m_Skeleton           = params.m_Skeleton;
        instance->m_MeshSet            = params.m_MeshSet;
        instance->m_AnimationSet       = params.m_AnimationSet;

        instance->m_Enabled = 1;

        SetModel(instance, instance->m_ModelId);

        instance->m_MaxBoneCount = dmMath::Max(instance->m_MeshSet->m_MaxBoneCount, instance->m_Skeleton == 0x0 ? 0 : instance->m_Skeleton->m_Bones.m_Count);
        Result result = CreatePose(context, instance);
        if (result != dmRig::RESULT_OK) {
            DestroyInstance(context, index);
            return result;
        }

        if (params.m_DefaultAnimation != NULL_ANIMATION)
        {
            // Loop forward should be the most common for idle anims etc.
            (void)PlayAnimation(instance, params.m_DefaultAnimation, dmRig::PLAYBACK_LOOP_FORWARD, 0.0f, 0.0f, 1.0f);
        }

        // m_ForceAnimatePose should be set if the animation step needs to run once (with dt 0)
        // to setup the pose to the current cursor.
        // Useful if pose needs to be calculated before draw but dmRig::Update will not be called
        // before that happens, for example cloning a GUI spine node happens in script update,
        // which comes after the regular dmRig::Update.
        if (params.m_ForceAnimatePose) {
            DoAnimate(context, instance, 0.0f);
        }

        *out_instance = instance;

        return dmRig::RESULT_OK;
    }

    Result InstanceDestroy(HRigContext context, HRigInstance instance)
    {
        if (!context || !instance) {
            return dmRig::RESULT_ERROR;
        }

        DestroyInstance(context, instance->m_Index);
        return dmRig::RESULT_OK;
    }

    void CopyBindPose(dmRigDDF::Skeleton& skeleton, dmArray<RigBone>& bind_pose)
    {
        uint32_t bone_count = skeleton.m_Bones.m_Count;
        bind_pose.SetCapacity(bone_count);
        bind_pose.SetSize(bone_count);
        for (uint32_t i = 0; i < bone_count; ++i)
        {
            dmRig::RigBone* bind_bone = &bind_pose[i];
            dmRigDDF::Bone* bone = &skeleton.m_Bones[i];
            bind_bone->m_ModelToLocal = dmTransform::ToMatrix4(bone->m_InverseBindPose);

            bind_bone->m_ParentIndex = bone->m_Parent;
            bind_bone->m_Length = bone->m_Length;
        }
    }
}
