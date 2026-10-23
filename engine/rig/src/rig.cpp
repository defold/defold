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
#include "rig_private.h"

#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/vmath.h>
#include <dlib/profile.h>
#include <dmsdk/dlib/object_pool.h>
#include <graphics/graphics.h>

#include <stdio.h>
#include <string.h>

namespace dmRig
{
    using namespace dmVMath;

    static const dmhash_t NULL_ANIMATION = dmHashString64("");
    static const float CURSOR_EPSILON = 0.0001f;

    const float TEXTURE_TRANSFORM_2D_IDENTITY[9] = {
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f,
    };

    static void DoAnimate(HRigContext context, RigInstance* instance, float dt);
    static bool DoPostUpdate(RigInstance* instance);
    static void UpdatePoseTransforms(const dmRigDDF::Skeleton* skeleton, dmArray<BonePose>& pose);

    struct RigContext
    {
        dmObjectPool<HRigInstance>      m_Instances;
        PoseMatrixCache                 m_PoseMatrixCache;
        uint32_t                        m_NumUpdateInstances; // Instances with skeleton, animation or morph data.
        // Temporary scratch buffers used for store pose as transform and matrices
        // (avoids modifying the real pose transform data during rendering).
        dmArray<dmVMath::Matrix4>       m_ScratchPoseMatrixBuffer;
        dmArray<BonePose>              m_ScratchLayerPose;
        // Temporary scratch buffers used when transforming the vertex buffer,
        // used to creating primitives from indices.
        dmArray<dmVMath::Vector3>       m_ScratchPositionBufferWorld;
        dmArray<dmVMath::Vector3>       m_ScratchPositionBufferLocal;
        dmArray<dmVMath::Vector3>       m_ScratchNormalBuffer;
        dmArray<dmVMath::Vector4>       m_ScratchTangentBuffer;
    };

    static void ResetPoseMatrixCache(PoseMatrixCache* cache)
    {
        cache->m_PoseMatrices.SetSize(0);
        cache->m_CacheEntryOffsets.SetSize(0);
        cache->m_TotalPoseCount = 0;
        cache->m_MaxBoneCount = 0;
    }

    Result NewContext(const NewContextParams& params, HRigContext* out)
    {
        RigContext* context = new RigContext;
        if (!context) {
            return dmRig::RESULT_ERROR;
        }

        context->m_Instances.SetCapacity(params.m_MaxRigInstanceCount);
        context->m_NumUpdateInstances = 0;
        context->m_ScratchPoseMatrixBuffer.SetCapacity(0);

        ResetPoseMatrixCache(&context->m_PoseMatrixCache);

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

    HAnimationLayer NewAnimationLayer(const AnimationLayerParams& params)
    {
        AnimationLayer* layer = new AnimationLayer;
        layer->m_Name = params.m_Name;
        layer->m_Order = params.m_Order;
        layer->m_Weight = params.m_Weight;
        layer->m_AllBones = params.m_AllBones;
        layer->m_RefCount = 1;
        layer->m_Bones.SetCapacity(params.m_BoneCount);
        layer->m_Bones.SetSize(params.m_BoneCount);
        if (params.m_BoneCount)
            memcpy(layer->m_Bones.Begin(), params.m_Bones, sizeof(dmhash_t) * params.m_BoneCount);
        return layer;
    }

    void ReleaseAnimationLayer(HAnimationLayer layer)
    {
        if (layer && --layer->m_RefCount == 0)
            delete layer;
    }

    static RigPlayer* GetPlayer(RigPlaybackState* state)
    {
        return &state->m_Players[state->m_CurrentPlayer];
    }

    static RigPlayer* GetPlayer(HRigInstance instance)
    {
        return GetPlayer(&instance->m_BasePlayback);
    }

    static RigLayer* FindLayer(HRigInstance instance, HAnimationLayer definition)
    {
        for (uint32_t i = 0; i < instance->m_Layers.Size(); ++i)
        {
            if (instance->m_Layers[i]->m_Definition == definition)
                return instance->m_Layers[i];
        }
        return 0;
    }

    static Result SetPlayerCursor(RigPlayer* player, float cursor, bool normalized);

    static void PlayAnimation(HRigInstance instance, RigPlaybackState* state, const dmRigDDF::RigAnimation* animation, RigPlayback playback, float blend_duration, float offset, float playback_rate)
    {
        state->m_Blending = blend_duration > 0.0f;
        state->m_BlendDuration = blend_duration;
        state->m_BlendTimer = 0.0f;
        state->m_Generation = ++instance->m_PlayGeneration;
        if (!state->m_Blending)
            GetPlayer(state)->m_Playing = 0;

        state->m_CurrentPlayer = (state->m_CurrentPlayer + 1) % 2;
        RigPlayer* player = GetPlayer(state);
        player->m_AnimationId = animation->m_Id;
        player->m_Animation = animation;
        player->m_Playing = 1;
        player->m_Playback = playback;
        player->m_Backwards = playback == PLAYBACK_ONCE_BACKWARD || playback == PLAYBACK_LOOP_BACKWARD;
        if (player->m_Backwards)
            offset = 1.0f - dmMath::Clamp(offset, 0.0f, 1.0f);
        SetPlayerCursor(player, offset, true);
        player->m_PlaybackRate = dmMath::Max(0.0f, playback_rate);
        // Keep this frame's evaluated skinning pose until the next rig update.
        // Completion callbacks can start playback after the matrices have been published.
    }

    Result PlayAnimation(HRigInstance instance, dmhash_t animation_id, RigPlayback playback, float blend_duration, float offset, float playback_rate)
    {
        const dmRigDDF::RigAnimation* animation = FindAnimation(instance->m_AnimationSet, animation_id);
        if (!animation)
        {
            GetPlayer(instance)->m_Playing = 0;
            instance->m_BasePlayback.m_Blending = 0;
            instance->m_BasePlayback.m_Generation = ++instance->m_PlayGeneration;
            instance->m_HasPoseMatrixCacheAnimatedPose = 0;
            return RESULT_ANIM_NOT_FOUND;
        }
        PlayAnimation(instance, &instance->m_BasePlayback, animation, playback, blend_duration, offset, playback_rate);
        return RESULT_OK;
    }

    Result PlayAnimation(HRigInstance instance, HAnimationLayer definition, dmhash_t animation_id, RigPlayback playback, float blend_duration, float offset, float playback_rate)
    {
        if (!definition)
            return PlayAnimation(instance, animation_id, playback, blend_duration, offset, playback_rate);

        const dmRigDDF::RigAnimation* animation = FindAnimation(instance->m_AnimationSet, animation_id);
        if (!animation)
            return RESULT_ANIM_NOT_FOUND;
        if (!instance->m_Skeleton || instance->m_Pose.Empty() || !instance->m_BoneIndices)
            return RESULT_ERROR;

        RigLayer* layer = FindLayer(instance, definition);
        if (!layer)
        {
            // Validate everything before allocating or replacing any playback.
            for (uint32_t i = 0; i < instance->m_Layers.Size(); ++i)
            {
                HAnimationLayer other = instance->m_Layers[i]->m_Definition;
                if (other->m_Name == definition->m_Name || other->m_Order == definition->m_Order)
                    return RESULT_LAYER_CONFLICT;
            }
            for (uint32_t i = 0; i < definition->m_Bones.Size(); ++i)
            {
                const uint32_t* bone = instance->m_BoneIndices->Get(definition->m_Bones[i]);
                if (!bone || *bone >= instance->m_Pose.Size())
                    return RESULT_BONE_NOT_FOUND;
            }

            layer = new RigLayer;
            memset(&layer->m_Playback, 0, sizeof(layer->m_Playback));
            layer->m_Definition = definition;
            ++definition->m_RefCount;
            uint32_t bone_count = instance->m_Pose.Size();
            layer->m_Mask.SetCapacity(bone_count);
            layer->m_Mask.SetSize(bone_count);
            memset(layer->m_Mask.Begin(), definition->m_AllBones ? 1 : 0, bone_count);
            for (uint32_t i = 0; i < definition->m_Bones.Size(); ++i)
                layer->m_Mask[*instance->m_BoneIndices->Get(definition->m_Bones[i])] = 1;
            // Skeletons are stored parent before child.
            for (uint32_t i = 0; i < bone_count; ++i)
            {
                uint32_t parent = instance->m_Skeleton->m_Bones[i].m_Parent;
                if (parent != INVALID_BONE_INDEX && layer->m_Mask[parent])
                    layer->m_Mask[i] = 1;
            }
            layer->m_Pose.SetCapacity(bone_count);
            layer->m_Pose.SetSize(bone_count);
            if (instance->m_Layers.Empty())
            {
                instance->m_BasePose.SetCapacity(bone_count);
                instance->m_BasePose.SetSize(bone_count);
                memcpy(instance->m_BasePose.Begin(), instance->m_Pose.Begin(), bone_count * sizeof(BonePose));
            }
            if (instance->m_Layers.Full())
                instance->m_Layers.OffsetCapacity(4);
            uint32_t index = instance->m_Layers.Size();
            instance->m_Layers.Push(layer);
            while (index > 0 && instance->m_Layers[index - 1]->m_Definition->m_Order > definition->m_Order)
            {
                instance->m_Layers[index] = instance->m_Layers[index - 1];
                --index;
            }
            instance->m_Layers[index] = layer;
        }
        PlayAnimation(instance, &layer->m_Playback, animation, playback, blend_duration, offset, playback_rate);
        return RESULT_OK;
    }

    static void ClearLayers(HRigInstance instance)
    {
        for (uint32_t i = 0; i < instance->m_Layers.Size(); ++i)
        {
            RigLayer* layer = instance->m_Layers[i];
            ReleaseAnimationLayer(layer->m_Definition);
            delete layer;
        }
        instance->m_Layers.SetSize(0);
        instance->m_BasePose.SetSize(0);
    }

    Result CancelAnimation(HRigInstance instance)
    {
        instance->m_BasePlayback.m_Players[0].m_Playing = 0;
        instance->m_BasePlayback.m_Players[1].m_Playing = 0;
        instance->m_BasePlayback.m_Blending = 0;
        instance->m_BasePlayback.m_Generation = ++instance->m_PlayGeneration;
        // Freeze the base pose and release overlays. The next update publishes it.
        if (!instance->m_BasePose.Empty())
        {
            memcpy(instance->m_Pose.Begin(), instance->m_BasePose.Begin(), instance->m_Pose.Size() * sizeof(BonePose));
            UpdatePoseTransforms(instance->m_Skeleton, instance->m_Pose);
        }
        ClearLayers(instance);
        instance->m_HasPoseMatrixCacheAnimatedPose = 0;
        return RESULT_OK;
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
        if (model_id == 0)
        {
            instance->m_ModelId = model_id;
            instance->m_Model = instance->m_MeshSet->m_Models.m_Data;
            instance->m_NumModels = instance->m_MeshSet->m_Models.m_Count;
            instance->m_DoRender = 1;
            return dmRig::RESULT_OK;
        }

        for (uint32_t i = 0; i < instance->m_MeshSet->m_Models.m_Count; ++i)
        {
            const dmRigDDF::Model* model = &instance->m_MeshSet->m_Models[i];
            if (model->m_Id == model_id)
            {
                instance->m_Model = model;
                instance->m_NumModels = 1;
                instance->m_ModelId = model_id;
                instance->m_DoRender = 1;
                return dmRig::RESULT_OK;
            }
        }
        instance->m_Model = 0;
        instance->m_ModelId = 0;
        instance->m_DoRender = 0;
        instance->m_NumModels = 0;
        return dmRig::RESULT_ERROR;
    }

    static void UpdateBlend(RigPlaybackState* state, float dt)
    {
        if (state->m_Blending)
        {
            state->m_BlendTimer += dt;
            if (state->m_BlendTimer >= state->m_BlendDuration)
            {
                state->m_Blending = 0;
                state->m_Players[(state->m_CurrentPlayer + 1) % 2].m_Playing = 0;
            }
        }
    }

    static PendingRigEvent* NewEvent(HRigInstance instance, RigPlaybackState* state, HAnimationLayer layer, RigEventType type)
    {
        if (instance->m_PendingEvents.Full())
            instance->m_PendingEvents.OffsetCapacity(dmMath::Max(1u, instance->m_PendingEvents.Capacity()));
        instance->m_PendingEvents.SetSize(instance->m_PendingEvents.Size() + 1);
        PendingRigEvent* event = &instance->m_PendingEvents.Back();
        event->m_Type = type;
        event->m_Layer = layer;
        event->m_Generation = state->m_Generation;
        return event;
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

    static void PostEventsInterval(HRigInstance instance, RigPlaybackState* state, HAnimationLayer layer, const dmRigDDF::RigAnimation* animation, float start_cursor, float end_cursor, float duration, bool backwards, float blend_weight)
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
                    PendingRigEvent* event = NewEvent(instance, state, layer, RIG_EVENT_TYPE_KEYFRAME);
                    RigKeyframeEventData& event_data = event->m_Keyframe;
                    event_data.m_Layer = layer;
                    event_data.m_EventId = track->m_EventId;
                    event_data.m_AnimationId = animation->m_Id;
                    event_data.m_BlendWeight = blend_weight;
                    event_data.m_T = key->m_T;
                    event_data.m_Integer = key->m_Integer;
                    event_data.m_Float = key->m_Float;
                    event_data.m_String = key->m_String;
                }
            }
        }
    }

    static void PostEvents(HRigInstance instance, RigPlaybackState* state, HAnimationLayer layer, RigPlayer* player, const dmRigDDF::RigAnimation* animation, float dt, float prev_cursor, float duration, bool completed, float blend_weight)
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
            PostEventsInterval(instance, state, layer, animation, prev_cursor, duration, duration, prev_backwards, blend_weight);
            PostEventsInterval(instance, state, layer, animation, 0.0f, cursor, duration, player->m_Backwards, blend_weight);
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
                    PostEventsInterval(instance, state, layer, animation, prev_cursor, half_duration, duration, false, blend_weight);
                    PostEventsInterval(instance, state, layer, animation, half_duration, cursor, duration, true, blend_weight);
                }
                else
                {
                    PostEventsInterval(instance, state, layer, animation, prev_cursor, cursor, duration, true, blend_weight);
                }
            }
            else
            {
                PostEventsInterval(instance, state, layer, animation, prev_cursor, cursor, duration, player->m_Backwards, blend_weight);
            }
        }
    }

    static void UpdatePlayer(RigInstance* instance, RigPlaybackState* state, HAnimationLayer layer, RigPlayer* player, float dt, float blend_weight)
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
            PostEvents(instance, state, layer, player, animation, dt, prev_cursor, duration, completed, blend_weight);
        }

        if (completed)
        {
            player->m_Playing = 0;
            // Only report completeness for the primary player
            if (player == GetPlayer(state) && instance->m_EventCallback)
            {
                PendingRigEvent* event = NewEvent(instance, state, layer, RIG_EVENT_TYPE_COMPLETED);
                RigCompletedEventData& event_data = event->m_Completed;
                event_data.m_Layer = layer;
                event_data.m_AnimationId = player->m_AnimationId;
                event_data.m_Playback = player->m_Playback;
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

    static MorphWeightSlot* GetMorphWeightSlot(RigInstance* instance, uint64_t model_id)
    {
        for (uint32_t i = 0; i < instance->m_MorphSlots.Size(); ++i)
        {
            if (instance->m_MorphSlots[i].m_ModelId == model_id)
            {
                return &instance->m_MorphSlots[i];
            }
        }
        return 0x0;
    }

    static void ResetMorphWeights(RigInstance* instance)
    {
        if (instance->m_MorphSlots.Empty())
        {
            return;
        }

        for (uint32_t si = 0; si < instance->m_MorphSlots.Size(); ++si)
        {
            MorphWeightSlot& slot = instance->m_MorphSlots[si];
            float* dest = instance->m_MorphWeightsBuffer.Begin() + slot.m_BufferOffset;
            memset(dest, 0, slot.m_MorphCount * sizeof(float));
        }
    }

    static uint32_t GetModelMaxMorphTargetCount(const dmRigDDF::Model* model)
    {
        uint32_t mcount = 0;
        for (uint32_t m = 0; m < model->m_Meshes.m_Count; ++m)
        {
            const dmRigDDF::Mesh* mesh = &model->m_Meshes[m];
            mcount = dmMath::Max(mcount, mesh->m_MorphTargetCount);
        }
        return mcount;
    }

    static void InitMorphSlots(RigInstance* instance)
    {
        const dmRigDDF::MeshSet* mesh_set = instance->m_MeshSet;
        if (!mesh_set)
        {
            return;
        }

        uint32_t slot_count = 0;
        uint32_t total_floats = 0;
        uint32_t max_morph = 0;

        // Pass 1: count the number of morph slots and total number of floats
        for (uint32_t mi = 0; mi < mesh_set->m_Models.m_Count; ++mi)
        {
            const dmRigDDF::Model* model = &mesh_set->m_Models[mi];
            uint32_t mcount = GetModelMaxMorphTargetCount(model);
            if (mcount == 0)
            {
                continue;
            }
            slot_count++;
            total_floats += mcount;
            max_morph = dmMath::Max(max_morph, mcount);
        }

        if (slot_count == 0 || total_floats == 0)
        {
            return;
        }

        // Pass 2: create the morph weight slots
        instance->m_MorphSlots.SetCapacity(slot_count);
        uint32_t buffer_offset = 0;
        for (uint32_t mi = 0; mi < mesh_set->m_Models.m_Count; ++mi)
        {
            const dmRigDDF::Model* model = &mesh_set->m_Models[mi];
            uint32_t mcount = GetModelMaxMorphTargetCount(model);
            if (mcount == 0)
            {
                continue;
            }

            MorphWeightSlot slot;
            slot.m_ModelId = model->m_Id;
            slot.m_MorphCount = mcount;
            slot.m_BufferOffset = buffer_offset;
            instance->m_MorphSlots.Push(slot);
            buffer_offset += mcount;
        }

        instance->m_MorphWeightsBuffer.SetCapacity(total_floats);
        instance->m_MorphWeightsBuffer.SetSize(total_floats);
        instance->m_MorphScratch.SetCapacity(max_morph);
        instance->m_MorphScratch.SetSize(max_morph);
        ResetMorphWeights(instance);
    }

    static void SampleMorphWeightTrack(const dmRigDDF::MorphWeightTrack* track, float t, float sample_rate, float* out_weights)
    {
        uint32_t dim = track->m_MorphCount;
        uint32_t total_floats = track->m_Weights.m_Count;
        if (dim == 0 || total_floats == 0)
            return;
        uint32_t num_poses = total_floats / dim;
        if (num_poses == 0)
            return;

        float fraction = t * sample_rate;
        uint32_t sample = (uint32_t)fraction;
        fraction -= sample;
        if (sample >= num_poses - 1)
        {
            sample = num_poses - 1;
            fraction = 0.0f;
        }

        const float* data = track->m_Weights.m_Data;
        for (uint32_t d = 0; d < dim; ++d)
        {
            uint32_t i0 = sample * dim + d;
            if (sample + 1 < num_poses)
            {
                uint32_t i1 = i0 + dim;
                out_weights[d] = data[i0] + fraction * (data[i1] - data[i0]);
            }
            else
            {
                out_weights[d] = data[i0];
            }
        }
    }

    static void ApplyMorphAnimation(RigInstance* instance, RigPlayer* player, float blend_weight)
    {
        const dmRigDDF::RigAnimation* animation = player->m_Animation;
        // No m_Playing check: UpdatePlayer may clear it when a ONCE clip ends in the same DoAnimate pass;
        // morph weights must still be sampled for that final frame (same as bone tracks).
        if (!animation || instance->m_MorphSlots.Empty())
        {
            return;
        }

        uint32_t mw_count = animation->m_MorphWeightTracks.m_Count;
        const dmRigDDF::MorphWeightTrack* mw_tracks = animation->m_MorphWeightTracks.m_Data;

        float duration    = GetCursorDuration(player, animation);
        float t           = CursorToTime(player->m_Cursor, duration, player->m_Backwards, player->m_Playback == dmRig::PLAYBACK_ONCE_PINGPONG);
        float sample_rate = animation->m_SampleRate;

        for (uint32_t ti = 0; ti < mw_count; ++ti)
        {
            const dmRigDDF::MorphWeightTrack* track = &mw_tracks[ti];
            MorphWeightSlot* slot = GetMorphWeightSlot(instance, track->m_ModelId);
            if (!slot)
            {
                continue;
            }

            if (track->m_MorphCount != slot->m_MorphCount)
            {
                continue;
            }

            SampleMorphWeightTrack(track, t, sample_rate, instance->m_MorphScratch.Begin());
            float* dest = instance->m_MorphWeightsBuffer.Begin() + slot->m_BufferOffset;
            const float* sampled = instance->m_MorphScratch.Begin();
            for (uint32_t d = 0; d < slot->m_MorphCount; ++d)
            {
                dest[d] = dest[d] + blend_weight * (sampled[d] - dest[d]);
            }
        }
    }

    static void ApplyAnimation(RigInstance* instance, RigPlayer* player, dmArray<BonePose>& pose, dmArray<IKAnimation>& ik_animation, float blend_weight)
    {
        const dmRigDDF::RigAnimation* animation = player->m_Animation;
        if (!animation)
            return;
        float duration = GetCursorDuration(player, animation);
        float t = CursorToTime(player->m_Cursor, duration, player->m_Backwards, player->m_Playback == dmRig::PLAYBACK_ONCE_PINGPONG);

        float fraction = t * animation->m_SampleRate;
        uint32_t sample = (uint32_t)fraction;
        fraction -= sample;
        // Sample skeletal tracks (skip when there is no skeleton / bone map, e.g. morph-only rigs)
        const dmHashTable64<uint32_t>* bone_indices = instance->m_BoneIndices;
        uint32_t track_count = animation->m_Tracks.m_Count;
        if (bone_indices)
        {
            for (uint32_t ti = 0; ti < track_count; ++ti)
            {
                const dmRigDDF::AnimationTrack* track = &animation->m_Tracks[ti];

                const uint32_t* bone_index = bone_indices->Get(track->m_BoneId);
                if (!bone_index || *bone_index >= pose.Size()) {
                    continue;
                }
                dmTransform::Transform& transform = pose[*bone_index].m_Local;

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
    }

    static void Animate(HRigContext context, float dt)
    {
        DM_PROFILE("RigAnimate");

        // set the size of pose matrix cache once per frame - resize if needed
        // size is based on the total number of bones in rig instances which
        // have acquired a pose matrix cache entry prior to the call to
        // Animate()
        // See AcquirePoseMatrixCacheEntry
        dmArray<Matrix4>& cache_pose_matrices = context->m_PoseMatrixCache.m_PoseMatrices;
        uint32_t size = context->m_PoseMatrixCache.m_TotalPoseCount;
        cache_pose_matrices.SetCapacity(size);
        cache_pose_matrices.SetSize(size);

        // animate each rig instance
        // the final step of DoAnimate will commit the bone poses to the pose
        // matrix cache which is resized above
        const dmArray<RigInstance*>& instances = context->m_Instances.GetRawObjects();
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

    static void UpdatePoseTransforms(const dmRigDDF::Skeleton* skeleton, dmArray<BonePose>& pose)
    {
        uint32_t bone_count = pose.Size();
        for (uint32_t bi = 0; bi < bone_count; ++bi)
        {
            BonePose& bp = pose[bi];

            if (bp.m_ParentIndex != INVALID_BONE_INDEX)
            {
                bp.m_World = dmTransform::Mul(pose[bp.m_ParentIndex].m_World, bp.m_Local);
            }
            else
            {
                // Apply the skeleton bone transform when the there is no parent.
                const dmRigDDF::Bone* sk_bone = &skeleton->m_Bones[bi];
                // Separate the anscestor (everything "above" this joint hierarchy-wise) from the local transform of this joint.
                const Matrix4 world_bind = dmTransform::ToMatrix4(sk_bone->m_World);
                const Matrix4 local_bind = dmTransform::ToMatrix4(sk_bone->m_Local);
                const Matrix4 ancestor = world_bind * dmVMath::Inverse(local_bind);
                bp.m_World = dmTransform::ToTransform(ancestor * dmTransform::ToMatrix4(bp.m_Local));
            }
        }
    }

    static void PoseToMatrix(const dmArray<BonePose>& pose, Matrix4* out_matrices)
    {
        uint32_t bone_count = pose.Size();
        for (uint32_t bi = 0; bi < bone_count; ++bi)
        {
            out_matrices[bi] = dmTransform::ToMatrix4(pose[bi].m_World);
        }
    }

    static void CommitPoseMatrixToCache(HRigContext context, HRigInstance instance)
    {
        uint32_t bone_count = GetBoneCount(instance);
        if (bone_count == 0 || instance->m_PoseMatrixCacheIndex == INVALID_POSE_MATRIX_CACHE_ENTRY)
        {
            return;
        }

        PoseMatrixCache* cache = &context->m_PoseMatrixCache;
        uint32_t cache_entry_offset = cache->m_CacheEntryOffsets[instance->m_PoseMatrixCacheIndex];

        Matrix4* pose_matrix_write_ptr = cache->m_PoseMatrices.Begin() + cache_entry_offset;
        PoseToMatrix(instance->m_Pose, pose_matrix_write_ptr);

        // Premultiply pose matrices with the bind pose inverse so they
        // can be directly be used to transform each vertex.
        const dmArray<RigBone>& bind_pose = *instance->m_BindPose;
        for (uint32_t bi = 0; bi < bone_count; ++bi)
        {
            Matrix4& pose_matrix = pose_matrix_write_ptr[bi];
            pose_matrix = pose_matrix * bind_pose[bi].m_ModelToLocal;
        }

        cache->m_MaxBoneCount = dmMath::Max(cache->m_MaxBoneCount, (uint32_t) instance->m_MaxBoneCount);
    }

    static bool IsPlaying(RigPlaybackState* state, bool finish_blend)
    {
        RigPlayer* player = GetPlayer(state);
        return (player->m_Playing || (finish_blend && state->m_Blending)) && player->m_Animation;
    }

    bool IsAnimating(HRigInstance instance)
    {
        if (!instance->m_Enabled)
            return false;
        if (IsPlaying(&instance->m_BasePlayback, false))
            return true;
        for (uint32_t i = 0; i < instance->m_Layers.Size(); ++i)
            if (IsPlaying(&instance->m_Layers[i]->m_Playback, true))
                return true;
        return false;
    }

    bool HasPoseMatrixCacheAnimatedPose(HRigInstance instance)
    {
        return instance && instance->m_HasPoseMatrixCacheAnimatedPose;
    }

    static void BlendTransform(dmTransform::Transform& dst, const dmTransform::Transform& src, float weight)
    {
        dst.SetTranslation(lerp(weight, dst.GetTranslation(), src.GetTranslation()));
        dst.SetScale(lerp(weight, dst.GetScale(), src.GetScale()));
        dst.SetRotation(normalize(slerp(weight, dst.GetRotation(), src.GetRotation())));
    }

    static void EvaluatePlayback(HRigContext context, RigInstance* instance, RigPlaybackState* state, HAnimationLayer layer, dmArray<BonePose>& pose, float dt)
    {
        if (!IsPlaying(state, layer != 0))
            return;

        ResetPose(instance->m_Skeleton, pose);
        RigPlayer* player = GetPlayer(state);
        UpdateBlend(state, dt);
        bool fade_in = layer && !state->m_Players[(state->m_CurrentPlayer + 1) % 2].m_Animation;
        if (layer && state->m_Blending && !fade_in)
        {
            // Sample both clips from rest so missing channels cannot inherit the outgoing clip.
            float fade_rate = state->m_BlendTimer / state->m_BlendDuration;
            RigPlayer* outgoing = &state->m_Players[(state->m_CurrentPlayer + 1) % 2];
            UpdatePlayer(instance, state, layer, outgoing, dt, 1.0f - fade_rate);
            UpdatePlayer(instance, state, layer, player, dt, fade_rate);
            dmArray<BonePose>& scratch = context->m_ScratchLayerPose;
            scratch.EnsureSize(pose.Size());
            ResetPose(instance->m_Skeleton, scratch);
            ApplyAnimation(instance, outgoing, pose, instance->m_IKAnimation, 1.0f);
            ApplyAnimation(instance, player, scratch, instance->m_IKAnimation, 1.0f);
            for (uint32_t bi = 0; bi < pose.Size(); ++bi)
                BlendTransform(pose[bi].m_Local, scratch[bi].m_Local, fade_rate);
        }
        else if (state->m_Blending && !fade_in)
        {
            float fade_rate = state->m_BlendTimer / state->m_BlendDuration;
            float alpha = 1.0f;
            for (uint32_t pi = 0; pi < 2; ++pi)
            {
                RigPlayer* p = &state->m_Players[pi];
                float blend_weight = player == p ? fade_rate : 1.0f - fade_rate;
                UpdatePlayer(instance, state, layer, p, dt, blend_weight);
                ApplyAnimation(instance, p, pose, instance->m_IKAnimation, alpha);
                if (!layer)
                    ApplyMorphAnimation(instance, p, alpha);
                alpha = player == p ? 1.0f - fade_rate : fade_rate;
            }
        }
        else
        {
            UpdatePlayer(instance, state, layer, player, dt, 1.0f);
            ApplyAnimation(instance, player, pose, instance->m_IKAnimation, 1.0f);
            if (!layer)
                ApplyMorphAnimation(instance, player, 1.0f);
        }

        if (state->m_Blending)
        {
            for (uint32_t bi = 0; bi < pose.Size(); ++bi)
            {
                Quat rotation = pose[bi].m_Local.GetRotation();
                if (dot(rotation, rotation) > 0.001f)
                    pose[bi].m_Local.SetRotation(normalize(rotation));
            }
        }
    }

    static void DoAnimate(HRigContext context, RigInstance* instance, float dt)
    {
        if (!instance->m_MorphSlots.Empty())
            ResetMorphWeights(instance);

        if (!IsAnimating(instance))
        {
            // Held poses still need publishing to the skinning cache each frame.
            if (instance->m_Skeleton && instance->m_PoseMatrixCacheIndex != INVALID_POSE_MATRIX_CACHE_ENTRY)
            {
                UpdatePoseTransforms(instance->m_Skeleton, instance->m_Pose);
                CommitPoseMatrixToCache(context, instance);
            }
            return;
        }

        const dmRigDDF::Skeleton* skeleton = instance->m_Skeleton;
        for (uint32_t i = 0; i < instance->m_IKAnimation.Size(); ++i)
        {
            const dmRigDDF::IK* ik = &skeleton->m_Iks[i];
            instance->m_IKAnimation[i].m_Mix = ik->m_Mix;
            instance->m_IKAnimation[i].m_Positive = ik->m_Positive;
        }

        dmArray<BonePose>& pose = instance->m_Pose;
        dmArray<BonePose>& base_pose = instance->m_Layers.Empty() ? pose : instance->m_BasePose;
        EvaluatePlayback(context, instance, &instance->m_BasePlayback, 0, base_pose, dt);
        if (!instance->m_Layers.Empty())
        {
            for (uint32_t bi = 0; bi < pose.Size(); ++bi)
                pose[bi].m_Local = base_pose[bi].m_Local;
            for (uint32_t i = 0; i < instance->m_Layers.Size(); ++i)
            {
                RigLayer* layer = instance->m_Layers[i];
                RigPlaybackState* state = &layer->m_Playback;
                EvaluatePlayback(context, instance, state, layer->m_Definition, layer->m_Pose, dt);
                float weight = layer->m_Definition->m_Weight;
                if (state->m_Blending && !state->m_Players[(state->m_CurrentPlayer + 1) % 2].m_Animation)
                    weight *= state->m_BlendTimer / state->m_BlendDuration;
                for (uint32_t bi = 0; bi < pose.Size(); ++bi)
                {
                    if (!layer->m_Mask[bi] || weight == 0.0f)
                        continue;
                    BlendTransform(pose[bi].m_Local, layer->m_Pose[bi].m_Local, weight);
                }
            }
        }
        UpdatePoseTransforms(skeleton, pose);
        CommitPoseMatrixToCache(context, instance);
        instance->m_HasPoseMatrixCacheAnimatedPose = skeleton && instance->m_PoseMatrixCacheIndex != INVALID_POSE_MATRIX_CACHE_ENTRY;
    }

    static void DispatchEvents(HRigInstance instance)
    {
        for (uint32_t i = 0; i < instance->m_PendingEvents.Size(); ++i)
        {
            PendingRigEvent event = instance->m_PendingEvents[i];
            RigPlaybackState* state = &instance->m_BasePlayback;
            if (event.m_Layer)
            {
                RigLayer* layer = FindLayer(instance, event.m_Layer);
                if (!layer)
                    continue;
                state = &layer->m_Playback;
            }
            // A preceding callback may replace or cancel another completed animation.
            if (state->m_Generation != event.m_Generation || !instance->m_EventCallback)
                continue;
            void* data = event.m_Type == RIG_EVENT_TYPE_COMPLETED ? (void*)&event.m_Completed : (void*)&event.m_Keyframe;
            instance->m_EventCallback(event.m_Type, data, instance->m_EventCBUserData1, instance->m_EventCBUserData2);
        }
        instance->m_PendingEvents.SetSize(0);
    }

    static Result PostUpdate(HRigContext context)
    {
        const dmArray<RigInstance*>& instances = context->m_Instances.GetRawObjects();
        uint32_t count = instances.Size();
        bool updated_pose = false;
        for (uint32_t i = 0; i < count; ++i)
        {
            RigInstance* instance = instances[i];
            if (DoPostUpdate(instance)) {
                updated_pose = true;
            }
            DispatchEvents(instance);
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

        if (context->m_NumUpdateInstances == 0)
            return RESULT_OK;

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
        return SetPlayerCursor(GetPlayer(instance), cursor, normalized);
    }

    static Result SetPlayerCursor(RigPlayer* player, float cursor, bool normalized)
    {
        float t = cursor;

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
        for (uint32_t m = 0; m < instance->m_NumModels; ++m)
        {
            const dmRigDDF::Model* model = &instance->m_Model[m];
            for (uint32_t i = 0; i < model->m_Meshes.m_Count; ++i)
            {
                const dmRigDDF::Mesh& mesh = model->m_Meshes[i];

                vertex_count += mesh.m_Positions.m_Count/3;
            }
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
                    Vector3 tangent_in(tangents_in[i*4+0], tangents_in[i*4+1], tangents_in[i*4+2]);
                    float tangent_handedness = tangents_in[i*4+3];
                    tangent = normal_matrix * tangent_in;
                    if (lengthSqr(tangent) > 0.0f) {
                        normalize(tangent);
                    }
                    *tangents_buffer++ = tangent[0];
                    *tangents_buffer++ = tangent[1];
                    *tangents_buffer++ = tangent[2];
                    *tangents_buffer++ = tangent_handedness;
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

            const Vector3 tangent_in = has_tangents ? Vector3(tangents_in[i*4+0], tangents_in[i*4+1], tangents_in[i*4+2]) : Vector3(0,0,0);
            const float tangent_handedness = has_tangents ? tangents_in[i*4+3] : 0.0f;
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
                tangent = normal_matrix * tangent_out;
                if (lengthSqr(tangent) > 0.0f) {
                    normalize(tangent);
                }
                *tangents_buffer++ = tangent[0];
                *tangents_buffer++ = tangent[1];
                *tangents_buffer++ = tangent[2];
                *tangents_buffer++ = tangent_handedness;
            }
        }
    }

    static void GeneratePositionData(const dmRigDDF::Mesh* mesh, const Matrix4& model_matrix, const dmArray<Matrix4>& pose_matrices, float* out_buffer_world, float* out_buffer_local)
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

                if (out_buffer_world)
                {
                    v = model_matrix * in_p;
                    *out_buffer_world++ = v[0];
                    *out_buffer_world++ = v[1];
                    *out_buffer_world++ = v[2];
                }
                if (out_buffer_local)
                {
                    *out_buffer_local++ = in_p[0];
                    *out_buffer_local++ = in_p[1];
                    *out_buffer_local++ = in_p[2];
                }
            }
            return;
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

            if (out_buffer_world)
            {
                v = model_matrix * Point3(out_p.getX(), out_p.getY(), out_p.getZ());
                *out_buffer_world++ = v[0];
                *out_buffer_world++ = v[1];
                *out_buffer_world++ = v[2];
            }
            if (out_buffer_local)
            {
                *out_buffer_local++ = out_p.getX();
                *out_buffer_local++ = out_p.getY();
                *out_buffer_local++ = out_p.getZ();
            }
        }
        return;
    }

    void SetMeshWriteAttributeParams(dmGraphics::WriteAttributeParams* params,
        const dmGraphics::VertexAttributeInfos* attribute_infos,
        dmGraphics::VertexStepFunction step_function,
        const float** world_matrix,
        const float** normal_matrix,
        const float** positions_world_space,
        const float** positions_local_space,
        const float** normals,
        const float** tangents,
        const float** colors,
        const float** texture_transform_2d,
        const float** morph_target_weights,
        dmGraphics::VertexAttribute::VectorType morph_target_weights_vector_type,
        const float** uv_channels,
        uint32_t uv_channels_count)
    {
        memset(params, 0, sizeof(dmGraphics::WriteAttributeParams));
        params->m_VertexAttributeInfos = attribute_infos;
        params->m_StepFunction         = step_function;

        // Per-primitive channels
        dmGraphics::SetWriteAttributeStreamDesc(&params->m_WorldMatrix, world_matrix, dmGraphics::VertexAttribute::VECTOR_TYPE_MAT4, 1, true);
        dmGraphics::SetWriteAttributeStreamDesc(&params->m_NormalMatrix, normal_matrix, dmGraphics::VertexAttribute::VECTOR_TYPE_MAT4, 1, true);
        dmGraphics::SetWriteAttributeStreamDesc(&params->m_TextureTransform2D, texture_transform_2d, dmGraphics::VertexAttribute::VECTOR_TYPE_MAT3, 1, true);
        dmGraphics::SetWriteAttributeStreamDesc(&params->m_MorphTargetWeights, morph_target_weights, morph_target_weights_vector_type, 1, true);

        // Per-vertex channels
        dmGraphics::SetWriteAttributeStreamDesc(&params->m_PositionsWorldSpace, positions_world_space, dmGraphics::VertexAttribute::VECTOR_TYPE_VEC3, 1, false);
        dmGraphics::SetWriteAttributeStreamDesc(&params->m_PositionsLocalSpace, positions_local_space, dmGraphics::VertexAttribute::VECTOR_TYPE_VEC3, 1, false);
        dmGraphics::SetWriteAttributeStreamDesc(&params->m_Normals, normals, dmGraphics::VertexAttribute::VECTOR_TYPE_VEC3, 1, false);
        dmGraphics::SetWriteAttributeStreamDesc(&params->m_Tangents, tangents, dmGraphics::VertexAttribute::VECTOR_TYPE_VEC4, 1, false);
        dmGraphics::SetWriteAttributeStreamDesc(&params->m_Colors, colors, dmGraphics::VertexAttribute::VECTOR_TYPE_VEC4, 1, false);
        dmGraphics::SetWriteAttributeStreamDesc(&params->m_TexCoords, uv_channels, dmGraphics::VertexAttribute::VECTOR_TYPE_VEC2, uv_channels_count, false);
    }

    static uint8_t* WriteVertexDataByAttributes(const dmRigDDF::Mesh* mesh, const float* positions_world, const float* positions_local, const float* normals, const float* tangents, const dmGraphics::VertexAttributeInfos* attribute_infos, uint32_t vertex_stride, const dmVMath::Matrix4& world_matrix, const dmVMath::Matrix4& normal_matrix, uint8_t* out_write_ptr)
    {
        const float* uv0 = mesh->m_Texcoord0.m_Count ? mesh->m_Texcoord0.m_Data : 0;
        const float* uv1 = mesh->m_Texcoord1.m_Count ? mesh->m_Texcoord1.m_Data : 0;
        const float* colors = mesh->m_Colors.m_Count ? mesh->m_Colors.m_Data : 0;

        assert(mesh->m_Indices.m_Count > 0);

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

        const float* uv_channels[] = { uv0, uv1 };
        uint32_t uv_channels_count = (uv0 ? 1 : 0) + (uv1 ? 1 : 0);

        const float* world_matrix_channels[] = { (float*) &world_matrix };
        const float* normal_matrix_channels[] = { (float*) &normal_matrix };
        const float* position_world_channels[] = { positions_world };
        const float* position_local_channels[] = { positions_local };
        const float* normals_channels[] = { normals };
        const float* tangents_channels[] = { tangents };
        const float* colors_channels[] = { colors };
        const float* texture_transform_2d_channels[] = { TEXTURE_TRANSFORM_2D_IDENTITY };

        dmGraphics::WriteAttributeParams params = {};
        SetMeshWriteAttributeParams(&params,
            attribute_infos,
            dmGraphics::VERTEX_STEP_FUNCTION_VERTEX,
            world_matrix_channels,
            normal_matrix_channels,
            position_world_channels,
            position_local_channels,
            normals_channels,
            tangents_channels,
            colors_channels,
            texture_transform_2d_channels,
            0,
            dmGraphics::VertexAttribute::VECTOR_TYPE_VEC4,
            uv_channels,
            uv_channels_count);

        for (uint32_t i = 0; i < num_indices; ++i)
        {
            uint32_t idx   = indices32?indices32[i]:indices16[i];
            out_write_ptr  = dmGraphics::WriteAttributes(out_write_ptr, idx, 1, params);
        }
        return out_write_ptr;
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
                }

                for (int c = 0; c < 4; ++c)
                {
                    out_write_ptr->color[c] = colors ? *colors++ : 1.0f;
                    out_write_ptr->tangent[c] = *tangents++;
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
                }

                for (int c = 0; c < 4; ++c)
                {
                    out_write_ptr->color[c] = colors ? colors[idx*4+c] : 1.0f;
                    out_write_ptr->tangent[c] = tangents[idx*4+c];
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

    void ResetPoseMatrixCache(HRigContext context)
    {
        // Instances only acquire cache indices when entries are added.
        if (context->m_PoseMatrixCache.m_CacheEntryOffsets.Size() == 0)
            return;

        ResetPoseMatrixCache(&context->m_PoseMatrixCache);

        const dmArray<RigInstance*>& instances = context->m_Instances.GetRawObjects();
        uint32_t n = context->m_Instances.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            instances[i]->m_PoseMatrixCacheIndex = INVALID_POSE_MATRIX_CACHE_ENTRY;
            instances[i]->m_HasPoseMatrixCacheAnimatedPose = 0;
        }
    }

    void GetPoseMatrixCacheData(HRigContext context, const dmVMath::Matrix4** pose_matrices, uint32_t* pose_matrices_count)
    {
        *pose_matrices = context->m_PoseMatrixCache.m_PoseMatrices.Begin();
        *pose_matrices_count = context->m_PoseMatrixCache.m_PoseMatrices.Size();
    }

    // These should typically not be stored across multiple frames, since dispatch order might differ.
    HCachePoseMatrixEntry AcquirePoseMatrixCacheEntry(HRigContext context, HRigInstance instance)
    {
        if (instance == 0)
        {
            return INVALID_POSE_MATRIX_CACHE_ENTRY;
        }
        else if (instance->m_PoseMatrixCacheIndex != INVALID_POSE_MATRIX_CACHE_ENTRY)
        {
            return instance->m_PoseMatrixCacheIndex;
        }

        PoseMatrixCache* cache = &context->m_PoseMatrixCache;

        uint16_t next_index = cache->m_CacheEntryOffsets.Size();
        if (cache->m_CacheEntryOffsets.Full())
        {
            cache->m_CacheEntryOffsets.OffsetCapacity(32);
        }

        cache->m_CacheEntryOffsets.SetSize(next_index + 1);

        instance->m_PoseMatrixCacheIndex = next_index;

        cache->m_CacheEntryOffsets[next_index] = cache->m_TotalPoseCount;
        cache->m_TotalPoseCount += instance->m_BindPose->Size();
        return next_index;
    }

    uint32_t GetPoseMatrixCacheDataOffset(HRigContext context, HRigInstance instance)
    {
        if (instance->m_PoseMatrixCacheIndex == INVALID_POSE_MATRIX_CACHE_ENTRY)
        {
            return INVALID_POSE_MATRIX_CACHE_ENTRY;
        }
        return context->m_PoseMatrixCache.m_CacheEntryOffsets[instance->m_PoseMatrixCacheIndex];
    }

    // For tests
    PoseMatrixCache* GetPoseMatrixCache(HRigContext context)
    {
        return &context->m_PoseMatrixCache;
    }

    uint8_t* GenerateVertexDataFromAttributes(dmRig::HRigContext context, dmRig::HRigInstance instance, dmRigDDF::Mesh* mesh, const dmVMath::Matrix4& world_matrix, const dmVMath::Matrix4& normal_matrix, const dmGraphics::VertexAttributeInfos* attribute_infos, uint32_t vertex_stride, uint8_t* vertex_data_out)
    {
        const dmRigDDF::Model* model = instance->m_Model;

        if (!model || !mesh || !instance->m_DoRender)
        {
            return vertex_data_out;
        }

        dmArray<Matrix4>& pose_matrices   = context->m_ScratchPoseMatrixBuffer;
        dmArray<Vector3>& positions_world = context->m_ScratchPositionBufferWorld;
        dmArray<Vector3>& positions_local = context->m_ScratchPositionBufferLocal;
        dmArray<Vector3>& normals         = context->m_ScratchNormalBuffer;
        dmArray<Vector4>& tangents        = context->m_ScratchTangentBuffer;

        uint32_t bone_count   = GetBoneCount(instance);
        uint32_t vertex_count = mesh->m_Positions.m_Count / 3;

        dmGraphics::VertexAttributeInfoMetadata meta_datas = dmGraphics::GetVertexAttributeInfosMetaData(*attribute_infos);

        pose_matrices.SetSize(0);

        float* positions_buffer_world = 0;
        float* positions_buffer_local = 0;
        float* normals_buffer         = 0;
        float* tangents_buffer        = 0;

        if (meta_datas.m_HasAttributeWorldPosition || meta_datas.m_HasAttributeLocalPosition)
        {
            if (bone_count)
            {
                pose_matrices.EnsureSize(bone_count);
                PoseToMatrix(instance->m_Pose, pose_matrices.Begin());

                // Premultiply pose matrices with the bind pose inverse so they
                // can be directly be used to transform each vertex.
                const dmArray<RigBone>& bind_pose = *instance->m_BindPose;
                for (uint32_t bi = 0; bi < pose_matrices.Size(); ++bi)
                {
                    Matrix4& pose_matrix = pose_matrices[bi];
                    pose_matrix = pose_matrix * bind_pose[bi].m_ModelToLocal;
                }
            }

            if (meta_datas.m_HasAttributeWorldPosition)
            {
                positions_world.EnsureSize(vertex_count);
                positions_buffer_world = (float*) positions_world.Begin();
            }
            if (meta_datas.m_HasAttributeLocalPosition)
            {
                positions_local.EnsureSize(vertex_count);
                positions_buffer_local = (float*) positions_local.Begin();
            }

            dmRig::GeneratePositionData(mesh, world_matrix, pose_matrices, positions_buffer_world, positions_buffer_local);
        }
        if (meta_datas.m_HasAttributeNormal && mesh->m_Normals.m_Count)
        {
            normals.EnsureSize(vertex_count);
            tangents.EnsureSize(vertex_count);
            normals_buffer  = (float*) normals.Begin();
            tangents_buffer = (float*) tangents.Begin();

            Matrix4 normal_matrix = Vectormath::Aos::inverse(world_matrix);
            normal_matrix = Vectormath::Aos::transpose(normal_matrix);
            dmRig::GenerateNormalData(mesh, normal_matrix, pose_matrices, normals_buffer, tangents_buffer);
        }

        return WriteVertexDataByAttributes(mesh, positions_buffer_world, positions_buffer_local, normals_buffer, tangents_buffer, attribute_infos, vertex_stride, world_matrix, normal_matrix, vertex_data_out);
    }

    RigModelVertex* GenerateVertexData(dmRig::HRigContext context, dmRig::HRigInstance instance, dmRigDDF::Mesh* mesh, const Matrix4& world_matrix, RigModelVertex* vertex_data_out)
    {
        // TODO: Separate out the instance part.
        // to do that we need to pass in the updated pose matrices
        const dmRigDDF::Model* model = instance->m_Model;

        if (!model || !mesh || !instance->m_DoRender) {
            return vertex_data_out;
        }

        dmArray<Matrix4>& pose_matrices   = context->m_ScratchPoseMatrixBuffer;
        dmArray<Vector3>& positions_world = context->m_ScratchPositionBufferWorld;
        dmArray<Vector3>& normals         = context->m_ScratchNormalBuffer;
        dmArray<Vector4>& tangents        = context->m_ScratchTangentBuffer;

        // If the rig has bones, update the pose to be local-to-model
        uint32_t bone_count = GetBoneCount(instance);
        if (bone_count)
        {
            pose_matrices.EnsureSize(bone_count);
            PoseToMatrix(instance->m_Pose, pose_matrices.Begin());

            // Premultiply pose matrices with the bind pose inverse so they
            // can be directly be used to transform each vertex.
            const dmArray<RigBone>& bind_pose = *instance->m_BindPose;
            for (uint32_t bi = 0; bi < pose_matrices.Size(); ++bi)
            {
                Matrix4& pose_matrix = pose_matrices[bi];
                pose_matrix = pose_matrix * bind_pose[bi].m_ModelToLocal;
            }
        }
        else
        {
            pose_matrices.SetSize(0);
        }

        Matrix4 normal_matrix = dmVMath::Inverse(world_matrix);
        normal_matrix = dmVMath::Transpose(normal_matrix);

        // TODO: Currently, we only have support for a single material so we bake all meshes into one
        uint32_t vertex_count = mesh->m_Positions.m_Count / 3;

        // Bump scratch buffers capacity to handle current vertex count
        positions_world.EnsureSize(vertex_count);
        normals.EnsureSize(vertex_count);
        tangents.EnsureSize(vertex_count);

        float* positions_world_buffer = (float*)positions_world.Begin();
        float* normals_buffer = (float*)normals.Begin();
        float* tangents_buffer = (float*)tangents.Begin();

        // Transform the mesh data into world space
        dmRig::GeneratePositionData(mesh, world_matrix, pose_matrices, positions_world_buffer, 0);

        if (mesh->m_Normals.m_Count)
        {
            dmRig::GenerateNormalData(mesh, normal_matrix, pose_matrices, normals_buffer, tangents_buffer);
        }

        return WriteVertexData(mesh, positions_world_buffer, normals_buffer, tangents_buffer, vertex_data_out);
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

    const float* GetMorphWeights(HRigInstance instance, uint64_t model_id, uint32_t* out_count)
    {
        if (instance == 0x0)
        {
            return 0x0;
        }
        MorphWeightSlot* slot = GetMorphWeightSlot(instance, model_id);
        if (!slot)
        {
            return 0x0;
        }
        *out_count = slot->m_MorphCount;
        return instance->m_MorphWeightsBuffer.Begin() + slot->m_BufferOffset;
    }

    void SetMorphWeights(HRigInstance instance, uint64_t model_id, const float* weights, uint32_t count)
    {
        MorphWeightSlot* slot = GetMorphWeightSlot(instance, model_id);
        if (!slot)
        {
            return;
        }
        float* dest = instance->m_MorphWeightsBuffer.Begin() + slot->m_BufferOffset;
        uint32_t n = dmMath::Min(count, slot->m_MorphCount);
        memcpy(dest, weights, n * sizeof(float));
        if (n < slot->m_MorphCount)
            memset(dest + n, 0, (slot->m_MorphCount - n) * sizeof(float));
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
        context->m_NumUpdateInstances -= instance->m_RequiresUpdate;
        ClearLayers(instance);
        // If we're going to use memset, then we should explicitly clear pose and instance arrays.
        instance->m_Pose.SetCapacity(0);
        instance->m_IKTargets.SetCapacity(0);
        instance->m_MorphSlots.SetCapacity(0);
        instance->m_MorphWeightsBuffer.SetCapacity(0);
        instance->m_MorphScratch.SetCapacity(0);
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
        memset(instance, 0, sizeof(RigInstance));

        uint32_t index = context->m_Instances.Alloc();
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
        instance->m_BoneIndices        = params.m_BoneIndices;
        instance->m_Skeleton           = params.m_Skeleton;
        instance->m_MeshSet            = params.m_MeshSet;
        instance->m_AnimationSet       = params.m_AnimationSet;

        InitMorphSlots(instance);
        // Static models can have empty skeleton and animation resources.
        instance->m_RequiresUpdate = (instance->m_Skeleton && instance->m_Skeleton->m_Bones.m_Count) ||
        (instance->m_AnimationSet && instance->m_AnimationSet->m_Animations.m_Count) ||
        !instance->m_MorphSlots.Empty();
        context->m_NumUpdateInstances += instance->m_RequiresUpdate;

        instance->m_PoseMatrixCacheIndex = INVALID_POSE_MATRIX_CACHE_ENTRY;

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
