#include "rig.h"

#include <dlib/log.h>
#include <dlib/profile.h>

namespace dmRig
{

    static const dmhash_t NULL_ANIMATION = dmHashString64("");
    static const uint32_t INVALID_BONE_INDEX = 0xffff;
    static const float CURSOR_EPSILON = 0.0001f;
    static const int SIGNAL_DELTA_UNCHANGED = 0x10cced; // Used to indicate if a draw order was unchanged for a certain slot

    static void DoAnimate(HRigContext context, RigInstance* instance, float dt);
    static bool DoPostUpdate(RigInstance* instance);
    static void UpdateSlotDrawOrder(dmArray<int32_t>& draw_order, dmArray<int32_t>& deltas, int changed, dmArray<int32_t>& unchanged);

    Result NewContext(const NewContextParams& params)
    {
        *params.m_Context = new RigContext();
        RigContext* context = *params.m_Context;
        if (!context) {
            return dmRig::RESULT_ERROR;
        }

        context->m_Instances.SetCapacity(params.m_MaxRigInstanceCount);
        context->m_ScratchPoseTransformBuffer.SetCapacity(0);
        context->m_ScratchPoseMatrixBuffer.SetCapacity(0);

        return dmRig::RESULT_OK;
    }

    void DeleteContext(HRigContext context)
    {
        if (context) {
            delete context;
        }
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

    static void ResetMeshSlotPose(HRigInstance instance)
    {
        instance->m_DoRender = 0;
        if (instance->m_MeshEntry != 0x0) {
            const float white[] = {1.0f, 1.0f, 1.0, 1.0f};

            int32_t slot_count = instance->m_MeshSet->m_SlotCount;
            for (int32_t slot_index = 0; slot_index < slot_count; slot_index++)
            {
                // Reset draw order list
                instance->m_DrawOrder[slot_index] = slot_index;

                // Update mesh pose
                const dmRigDDF::MeshSlot* mesh_slot = &instance->m_MeshEntry->m_MeshSlots[slot_index];
                MeshSlotPose* mesh_slot_pose = &instance->m_MeshSlotPose[slot_index];
                mesh_slot_pose->m_ActiveAttachment = mesh_slot->m_ActiveIndex;

                mesh_slot_pose->m_Color[0] = 1.0f;
                mesh_slot_pose->m_Color[1] = 1.0f;
                mesh_slot_pose->m_Color[2] = 1.0f;
                mesh_slot_pose->m_Color[3] = 1.0f;

                // Check if there is an active attachment on this slot
                if (mesh_slot_pose->m_ActiveAttachment >= 0) {

                    // Check if the attachment has a valid mesh attachment
                    int mesh_attachment_index = mesh_slot->m_MeshAttachments[mesh_slot_pose->m_ActiveAttachment];
                    if (mesh_attachment_index >= 0) {
                        const Mesh* mesh_attachment = &instance->m_MeshSet->m_MeshAttachments[mesh_attachment_index];

                        // Get color for both slot and attachment
                        const float* attachment_color = mesh_attachment->m_Color.m_Count ? mesh_attachment->m_Color.m_Data : white;
                        const float* slot_color = mesh_slot->m_SkinColor.m_Count ? mesh_slot->m_SkinColor.m_Data : white;

                        mesh_slot_pose->m_Color[0] = attachment_color[0] * slot_color[0];
                        mesh_slot_pose->m_Color[1] = attachment_color[1] * slot_color[1];
                        mesh_slot_pose->m_Color[2] = attachment_color[2] * slot_color[2];
                        mesh_slot_pose->m_Color[3] = attachment_color[3] * slot_color[3];
                    }
                }
            }
            instance->m_DoRender = 1;
        }
    }

    Result PlayAnimation(HRigInstance instance, dmhash_t animation_id, dmRig::RigPlayback playback, float blend_duration, float offset, float playback_rate)
    {
        const dmRigDDF::RigAnimation* anim = FindAnimation(instance->m_AnimationSet, animation_id);
        if (anim == 0x0)
        {
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
        player->m_Initial = 1;
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

    dmhash_t GetMesh(HRigInstance instance)
    {
        return instance->m_MeshId;
    }

    Result SetMesh(HRigInstance instance, dmhash_t mesh_id)
    {
        const dmRigDDF::MeshSet* mesh_set = instance->m_MeshSet;
        for (uint32_t i = 0; i < mesh_set->m_MeshEntries.m_Count; ++i)
        {
            if (mesh_id == mesh_set->m_MeshEntries[i].m_Id)
            {
                instance->m_MeshEntry = &mesh_set->m_MeshEntries[i];
                instance->m_MeshId = mesh_id;

                ResetMeshSlotPose(instance);
                return dmRig::RESULT_OK;
            }
        }
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

    static Vector4 SampleVec4(uint32_t sample, float frac, float* data)
    {
        uint32_t i0 = sample*4;
        uint32_t i1 = i0+4;
        return lerp(frac, Vector4(data[i0+0], data[i0+1], data[i0+2], data[i0+3]), Vector4(data[i1+0], data[i1+1], data[i1+2], data[i1+3]));
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

    static inline dmTransform::Transform GetPoseTransform(const dmArray<RigBone>& bind_pose, const dmArray<dmTransform::Transform>& pose, dmTransform::Transform transform, const uint32_t index) {
        if(bind_pose[index].m_ParentIndex == INVALID_BONE_INDEX)
            return transform;
        transform = dmTransform::Mul(pose[bind_pose[index].m_ParentIndex], transform);
        return GetPoseTransform(bind_pose, pose, transform, bind_pose[index].m_ParentIndex);
    }

    static inline float ToEulerZ(const dmTransform::Transform& t)
    {
        Vectormath::Aos::Quat q(t.GetRotation());
        return dmVMath::QuatToEuler(q.getZ(), q.getY(), q.getX(), q.getW()).getZ() * (M_PI/180.0f);
    }

    static void ApplyOneBoneIKConstraint(const dmRigDDF::IK* ik, const dmArray<RigBone>& bind_pose, dmArray<dmTransform::Transform>& pose, const Vector3 target_wp, const Vector3 parent_wp, const float mix)
    {
        if (mix == 0.0f)
            return;
        const dmTransform::Transform& parent_bt = bind_pose[ik->m_Parent].m_LocalToParent;
        dmTransform::Transform& parent_t = pose[ik->m_Parent];
        float parentRotation = ToEulerZ(parent_bt);
        // Based on code by Ryan Juckett with permission: Copyright (c) 2008-2009 Ryan Juckett, http://www.ryanjuckett.com/
        float rotationIK = atan2(target_wp.getY() - parent_wp.getY(), target_wp.getX() - parent_wp.getX());
        parentRotation = parentRotation + (rotationIK - parentRotation) * mix;
        parent_t.SetRotation( dmVMath::QuatFromAngle(2, parentRotation) );
    }

    // Based on http://www.ryanjuckett.com/programming/analytic-two-bone-ik-in-2d/
    static void ApplyTwoBoneIKConstraint(const dmRigDDF::IK* ik, const dmArray<RigBone>& bind_pose, dmArray<dmTransform::Transform>& pose, const Vector3 target_wp, const Vector3 parent_wp, const bool bend_positive, const float mix)
    {
        if (mix == 0.0f)
            return;
        const dmTransform::Transform& parent_bt = bind_pose[ik->m_Parent].m_LocalToParent;
        const dmTransform::Transform& child_bt = bind_pose[ik->m_Child].m_LocalToParent;
        dmTransform::Transform& parent_t = pose[ik->m_Parent];
        dmTransform::Transform& child_t = pose[ik->m_Child];
        float childRotation = ToEulerZ(child_bt), parentRotation = ToEulerZ(parent_bt);

        // recalc target position to local (relative parent)
        const Vector3 target(target_wp.getX() - parent_wp.getX(), target_wp.getY() - parent_wp.getY(), 0.0f);
        const Vector3 child_p = child_bt.GetTranslation();
        const float childX = child_p.getX(), childY = child_p.getY();
        const float offset = atan2(childY, childX);
        const float len1 = (float)sqrt(childX * childX + childY * childY);
        const float len2 = bind_pose[ik->m_Child].m_Length;

        // Based on code by Ryan Juckett with permission: Copyright (c) 2008-2009 Ryan Juckett, http://www.ryanjuckett.com/
        const float cosDenom = 2.0f * len1 * len2;
        if (cosDenom < 0.0001f) {
            childRotation = childRotation + ((float)atan2(target.getY(), target.getX()) - parentRotation - childRotation) * mix;
            child_t.SetRotation( dmVMath::QuatFromAngle(2, childRotation) );
            return;
        }
        float cosValue = (target.getX() * target.getX() + target.getY() * target.getY() - len1 * len1 - len2 * len2) / cosDenom;
        cosValue = dmMath::Max(-1.0f, dmMath::Min(1.0f, cosValue));
        const float childAngle = (float)acos(cosValue) * (bend_positive ? 1.0f : -1.0f);
        const float adjacent = len1 + len2 * cosValue;
        const float opposite = len2 * sin(childAngle);
        const float parentAngle = (float)atan2(target.getY() * adjacent - target.getX() * opposite, target.getX() * adjacent + target.getY() * opposite);
        parentRotation = ((parentAngle - offset) - parentRotation) * mix;
        childRotation = ((childAngle + offset) - childRotation) * mix;
        parent_t.SetRotation( dmVMath::QuatFromAngle(2, parentRotation) );
        child_t.SetRotation( dmVMath::QuatFromAngle(2, childRotation) );
    }

    static void ApplyAnimation(RigPlayer* player, dmArray<dmTransform::Transform>& pose, const dmArray<uint32_t>& track_idx_to_pose, dmArray<IKAnimation>& ik_animation, dmArray<MeshSlotPose>& mesh_slot_pose, bool update_draw_order, const dmRigDDF::MeshEntry* mesh_entry, dmArray<int32_t>& draw_order, int& slot_changed, float blend_weight)
    {
        const dmRigDDF::RigAnimation* animation = player->m_Animation;
        if (animation == 0x0)
            return;
        float duration = GetCursorDuration(player, animation);
        float t = CursorToTime(player->m_Cursor, duration, player->m_Backwards, player->m_Playback == dmRig::PLAYBACK_ONCE_PINGPONG);

        float fraction = t * animation->m_SampleRate;
        uint32_t sample = (uint32_t)fraction;
        uint32_t rounded_sample = (uint32_t)(fraction + 0.5f);
        fraction -= sample;
        // Sample animation tracks
        uint32_t track_count = animation->m_Tracks.m_Count;
        for (uint32_t ti = 0; ti < track_count; ++ti)
        {
            const dmRigDDF::AnimationTrack* track = &animation->m_Tracks[ti];
            uint32_t bone_index = track->m_BoneIndex;
            if (bone_index >= track_idx_to_pose.Size()) {
                continue;
            }
            uint32_t pose_index = track_idx_to_pose[bone_index];
            dmTransform::Transform& transform = pose[pose_index];
            if (track->m_Positions.m_Count > 0)
            {
                transform.SetTranslation(lerp(blend_weight, transform.GetTranslation(), SampleVec3(sample, fraction, track->m_Positions.m_Data)));
            }
            if (track->m_Rotations.m_Count > 0)
            {
                transform.SetRotation(slerp(blend_weight, transform.GetRotation(), SampleQuat(sample, fraction, track->m_Rotations.m_Data)));
            }
            if (track->m_Scale.m_Count > 0)
            {
                transform.SetScale(lerp(blend_weight, transform.GetScale(), SampleVec3(sample, fraction, track->m_Scale.m_Data)));
            }
        }

        track_count = animation->m_IkTracks.m_Count;
        for (uint32_t ti = 0; ti < track_count; ++ti)
        {
            const dmRigDDF::IKAnimationTrack* track = &animation->m_IkTracks[ti];
            uint32_t ik_index = track->m_IkIndex;
            IKAnimation& anim = ik_animation[ik_index];
            if (track->m_Mix.m_Count > 0)
            {
                anim.m_Mix = dmMath::LinearBezier(blend_weight, anim.m_Mix, dmMath::LinearBezier(fraction, track->m_Mix.m_Data[sample], track->m_Mix.m_Data[sample+1]));
            }
            if (track->m_Positive.m_Count > 0)
            {
                if (blend_weight >= 0.5f)
                {
                    anim.m_Positive = track->m_Positive[sample];
                }
            }
        }

        track_count = animation->m_MeshTracks.m_Count;
        for (uint32_t ti = 0; ti < track_count; ++ti) {
            const dmRigDDF::MeshAnimationTrack* track = &animation->m_MeshTracks[ti];

            if (track->m_Colors.m_Count > 0) {
                MeshSlotPose& mesh_slot = mesh_slot_pose[track->m_MeshSlot];
                Vector4 color(mesh_slot.m_Color[0], mesh_slot.m_Color[1], mesh_slot.m_Color[2], mesh_slot.m_Color[3]);
                color = lerp(blend_weight, color, SampleVec4(sample, fraction, track->m_Colors.m_Data));
                mesh_slot.m_Color[0] = color[0];
                mesh_slot.m_Color[1] = color[1];
                mesh_slot.m_Color[2] = color[2];
                mesh_slot.m_Color[3] = color[3];

                if (mesh_entry->m_MeshSlots[track->m_MeshSlot].m_SkinColor.m_Count > 0) {
                    mesh_slot.m_Color[0] *= mesh_entry->m_MeshSlots[track->m_MeshSlot].m_SkinColor.m_Data[0];
                    mesh_slot.m_Color[1] *= mesh_entry->m_MeshSlots[track->m_MeshSlot].m_SkinColor.m_Data[1];
                    mesh_slot.m_Color[2] *= mesh_entry->m_MeshSlots[track->m_MeshSlot].m_SkinColor.m_Data[2];
                    mesh_slot.m_Color[3] *= mesh_entry->m_MeshSlots[track->m_MeshSlot].m_SkinColor.m_Data[3];
                }
            }

            if (track->m_MeshAttachment.m_Count > 0) {
                if (update_draw_order) {
                    MeshSlotPose& mesh_slot = mesh_slot_pose[track->m_MeshSlot];
                    mesh_slot.m_ActiveAttachment = track->m_MeshAttachment[rounded_sample];
                }
            }

            if (track->m_OrderOffset.m_Count > 0) {
                if (update_draw_order) {
                    int32_t* order = &draw_order[track->m_MeshSlot];
                    *order = track->m_OrderOffset[rounded_sample];
                    slot_changed++;
                }
            }
        }
    }

    static void Animate(HRigContext context, float dt)
    {
        DM_PROFILE(Rig, "Animate");

        const dmArray<RigInstance*>& instances = context->m_Instances.m_Objects;
        uint32_t n = instances.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            RigInstance* instance = instances[i];
            DoAnimate(context, instance, dt);
        }
    }

    static void DoAnimate(HRigContext context, RigInstance* instance, float dt)
    {
            // NOTE we previously checked for (!instance->m_Enabled || !instance->m_AddedToUpdate) here also
            if (instance->m_Pose.Empty() || !instance->m_Enabled)
                return;

            const dmRigDDF::Skeleton* skeleton = instance->m_Skeleton;
            const dmArray<RigBone>& bind_pose = *instance->m_BindPose;
            const dmArray<uint32_t>& track_idx_to_pose = *instance->m_TrackIdxToPose;
            dmArray<dmTransform::Transform>& pose = instance->m_Pose;
            // Reset pose
            uint32_t bone_count = pose.Size();
            for (uint32_t bi = 0; bi < bone_count; ++bi)
            {
                pose[bi].SetIdentity();
            }
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

            RigPlayer* player = GetPlayer(instance);

            // If the animation has just started, we reset mesh properties (color, draw order etc)
            if (player->m_Initial) {
                ResetMeshSlotPose(instance);
                player->m_Initial = 0;
            }

            // Make sure we have enough space in the draw order deltas scratch buffer.
            int slot_count = instance->m_MeshSet->m_SlotCount;
            int slot_changed = 0;
            if (context->m_ScratchDrawOrderDeltas.Capacity() < slot_count) {
                context->m_ScratchDrawOrderDeltas.OffsetCapacity(slot_count - context->m_ScratchDrawOrderDeltas.Capacity());
            }
            context->m_ScratchDrawOrderDeltas.SetSize(slot_count);

            // Reset draw order deltas to "unchanged" constant.
            for (int i = 0; i < slot_count; i++) {
                instance->m_DrawOrder[i] = i;
                context->m_ScratchDrawOrderDeltas[i] = SIGNAL_DELTA_UNCHANGED;
            }

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
                    bool draw_order = player == p ? fade_rate >= 0.5f : fade_rate < 0.5f;
                    ApplyAnimation(p, pose, track_idx_to_pose, ik_animation, instance->m_MeshSlotPose, draw_order, instance->m_MeshEntry, context->m_ScratchDrawOrderDeltas, slot_changed, alpha);
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
                ApplyAnimation(player, pose, track_idx_to_pose, ik_animation, instance->m_MeshSlotPose, true, instance->m_MeshEntry, context->m_ScratchDrawOrderDeltas, slot_changed, 1.0f);
            }

            // Update draw order after animation
            UpdateSlotDrawOrder(instance->m_DrawOrder, context->m_ScratchDrawOrderDeltas, slot_changed, context->m_ScratchDrawOrderUnchanged);

            for (uint32_t bi = 0; bi < bone_count; ++bi)
            {
                dmTransform::Transform& t = pose[bi];
                // Normalize quaternions while we blend
                if (instance->m_Blending)
                {
                    Quat rotation = t.GetRotation();
                    if (dot(rotation, rotation) > 0.001f)
                        rotation = normalize(rotation);
                    t.SetRotation(rotation);
                }
                const dmTransform::Transform& bind_t = bind_pose[bi].m_LocalToParent;
                t.SetTranslation(bind_t.GetTranslation() + t.GetTranslation());
                t.SetRotation(bind_t.GetRotation() * t.GetRotation());
                t.SetScale(mulPerElem(bind_t.GetScale(), t.GetScale()));
            }

            if (skeleton->m_Iks.m_Count > 0) {
                DM_PROFILE(Rig, "IK");
                const uint32_t count = skeleton->m_Iks.m_Count;
                dmArray<IKTarget>& ik_targets = instance->m_IKTargets;


                for (int32_t i = 0; i < count; ++i) {
                    const dmRigDDF::IK* ik = &skeleton->m_Iks[i];

                    // transform local space hiearchy for pose
                    dmTransform::Transform parent_t = GetPoseTransform(bind_pose, pose, pose[ik->m_Parent], ik->m_Parent);
                    dmTransform::Transform parent_t_local = parent_t;
                    dmTransform::Transform target_t = GetPoseTransform(bind_pose, pose, pose[ik->m_Target], ik->m_Target);
                    const uint32_t parent_parent_index = skeleton->m_Bones[ik->m_Parent].m_Parent;
                    dmTransform::Transform parent_parent_t;
                    if(parent_parent_index != INVALID_BONE_INDEX)
                    {
                        parent_parent_t = dmTransform::Inv(GetPoseTransform(bind_pose, pose, pose[skeleton->m_Bones[ik->m_Parent].m_Parent], skeleton->m_Bones[ik->m_Parent].m_Parent));
                        parent_t = dmTransform::Mul(parent_parent_t, parent_t);
                        target_t = dmTransform::Mul(parent_parent_t, target_t);
                    }
                    Vector3 parent_position = parent_t.GetTranslation();
                    Vector3 target_position = target_t.GetTranslation();

                    if(ik_targets[i].m_Mix != 0.0f)
                    {
                        // get custom target position either from go or vector position
                        Vector3 user_target_position = target_position;
                        if(ik_targets[i].m_Callback != 0)
                        {
                            user_target_position = ik_targets[i].m_Callback(&ik_targets[i]);
                        } else {
                            // instance have been removed, disable animation
                            ik_targets[i].m_UserHash = 0;
                            ik_targets[i].m_Mix = 0.0f;
                        }

                        const float target_mix = ik_targets[i].m_Mix;

                        if (parent_parent_index != INVALID_BONE_INDEX) {
                            user_target_position = dmTransform::Apply(parent_parent_t, user_target_position);
                        }

                        // blend default target pose and target pose
                        target_position = target_mix == 1.0f ? user_target_position : dmTransform::lerp(target_mix, target_position, user_target_position);
                    }

                    if(ik->m_Child == ik->m_Parent)
                        ApplyOneBoneIKConstraint(ik, bind_pose, pose, target_position, parent_position, ik_animation[i].m_Mix);
                    else
                        ApplyTwoBoneIKConstraint(ik, bind_pose, pose, target_position, parent_position, ik_animation[i].m_Positive, ik_animation[i].m_Mix);
                }
            }
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
            dmArray<dmTransform::Transform>& pose = instance->m_Pose;
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
        DM_PROFILE(Rig, "Update");

        Animate(context, dt);

        return PostUpdate(context);
    }

    static void AllocateMeshSlotPose(const dmRig::MeshSet* mesh_set, dmArray<MeshSlotPose>& mesh_slot_pose, dmArray<int32_t>& draw_order) {
        mesh_slot_pose.SetCapacity(mesh_set->m_SlotCount);
        mesh_slot_pose.SetSize(mesh_set->m_SlotCount);
        draw_order.SetCapacity(mesh_set->m_SlotCount);
        draw_order.SetSize(mesh_set->m_SlotCount);
    }

    static const dmRigDDF::MeshEntry* FindMeshEntry(const dmRigDDF::MeshSet* mesh_set, dmhash_t mesh_id)
    {
        for (uint32_t i = 0; i < mesh_set->m_MeshEntries.m_Count; ++i)
        {
            const dmRigDDF::MeshEntry* mesh_entry = &mesh_set->m_MeshEntries[i];
            if (mesh_entry->m_Id == mesh_id)
            {
                return mesh_entry;
            }
        }
        return 0x0;
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
            instance->m_Pose[i].SetIdentity();
        }

        instance->m_IKTargets.SetCapacity(skeleton->m_Iks.m_Count);
        instance->m_IKTargets.SetSize(skeleton->m_Iks.m_Count);
        memset(instance->m_IKTargets.Begin(), 0x0, instance->m_IKTargets.Size()*sizeof(IKTarget));

        instance->m_IKAnimation.SetCapacity(skeleton->m_Iks.m_Count);
        instance->m_IKAnimation.SetSize(skeleton->m_Iks.m_Count);

        return dmRig::RESULT_OK;
    }

    dmArray<dmTransform::Transform>* GetPose(HRigInstance instance)
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

    // Slot ordering is based on the official Spine runtime
    // https://github.com/EsotericSoftware/spine-runtimes/blob/387b0afb80a775970c48099042be769e50258440/spine-c/spine-c/src/spine/SkeletonJson.c#L430
    static void UpdateSlotDrawOrder(dmArray<int32_t>& draw_order, dmArray<int32_t>& deltas, int changed, dmArray<int32_t>& unchanged)
    {
        if (changed == 0) {
            return;
        }

        int slot_count = draw_order.Size();

        // Make sure we have enough capacity to store our unchanged slots list.
        if (unchanged.Capacity() < slot_count) {
            unchanged.OffsetCapacity(slot_count - unchanged.Capacity());
        }
        unchanged.SetSize(slot_count);

        for (int i = 0; i < slot_count; i++) {
            draw_order[i] = -1;
        }

        int original_index = 0;
        int unchanged_index = 0;
        for (int slot_index = 0; slot_index < slot_count; slot_index++)
        {
            int32_t delta = deltas[slot_index];
            if (delta != SIGNAL_DELTA_UNCHANGED) {
                while (original_index != slot_index) {
                    unchanged[unchanged_index++] = original_index++;
                }

                draw_order[original_index + delta] = original_index;
                original_index++;
            }
        }

        while (original_index < slot_count) {
            unchanged[unchanged_index++] = original_index++;
        }

        for (int i = slot_count - 1; i >= 0; i--)
        {
            if (draw_order[i] == -1) {
                draw_order[i] = unchanged[--unchanged_index];
            }
        }

    }

    uint32_t GetVertexCount(HRigInstance instance)
    {
        if (!instance->m_MeshEntry || !instance->m_DoRender) {
            return 0;
        }

        uint32_t vertex_count = 0;

        int32_t slot_count = instance->m_MeshSet->m_SlotCount;
        for (int32_t slot_index = 0; slot_index < slot_count; slot_index++)
        {
            MeshSlotPose* mesh_slot_pose = &instance->m_MeshSlotPose[slot_index];

            // Get attachment index for current slot
            int active_attachment = mesh_slot_pose->m_ActiveAttachment;
            if (active_attachment >= 0) {

                // Check if there is any mesh on current attachment
                int mesh_attachment_index = instance->m_MeshEntry->m_MeshSlots[slot_index].m_MeshAttachments[active_attachment];
                if (mesh_attachment_index >= 0) {
                    const Mesh* mesh_attachment = &instance->m_MeshSet->m_MeshAttachments[mesh_attachment_index];
                    vertex_count += mesh_attachment->m_Indices.m_Count;
                }
            }
        }

        return vertex_count;
    }

    static float* GenerateNormalData(const dmRigDDF::Mesh* mesh, const Matrix4& normal_matrix, const dmArray<Matrix4>& pose_matrices, float* out_buffer)
    {
        const float* normals_in = mesh->m_Normals.m_Data;
        const uint32_t* normal_indices = mesh->m_NormalsIndices.m_Data;
        uint32_t index_count = mesh->m_Indices.m_Count;
        Vector4 v;

        if (!mesh->m_BoneIndices.m_Count || pose_matrices.Size() == 0)
        {
            for (uint32_t ii = 0; ii < index_count; ++ii)
            {
                uint32_t ni = normal_indices[ii];
                Vector3 normal_in(normals_in[ni*3+0], normals_in[ni*3+1], normals_in[ni*3+2]);
                v = normal_matrix * normal_in;
                if (lengthSqr(v) > 0.0f) {
                    normalize(v);
                }
                *out_buffer++ = v[0];
                *out_buffer++ = v[1];
                *out_buffer++ = v[2];
            }
            return out_buffer;
        }

        const uint32_t* indices = mesh->m_BoneIndices.m_Data;
        const float* weights = mesh->m_Weights.m_Data;
        const uint32_t* vertex_indices = mesh->m_Indices.m_Data;
        for (uint32_t ii = 0; ii < index_count; ++ii)
        {
            const uint32_t ni = normal_indices[ii]*3;
            const Vector3 normal_in(normals_in[ni+0], normals_in[ni+1], normals_in[ni+2]);
            Vector4 normal_out(0.0f, 0.0f, 0.0f, 0.0f);

            const uint32_t bi_offset = vertex_indices[ii] << 2;
            const uint32_t* bone_indices = &indices[bi_offset];
            const float* bone_weights = &weights[bi_offset];

            if (bone_weights[0])
            {
                normal_out += (pose_matrices[bone_indices[0]] * normal_in) * bone_weights[0];
                if (bone_weights[1])
                {
                    normal_out += (pose_matrices[bone_indices[1]] * normal_in) * bone_weights[1];
                    if (bone_weights[2])
                    {
                        normal_out += (pose_matrices[bone_indices[2]] * normal_in) * bone_weights[2];
                        if (bone_weights[3])
                        {
                            normal_out += (pose_matrices[bone_indices[3]] * normal_in) * bone_weights[3];
                        }
                    }
                }
            }

            v = normal_matrix * Vector3(normal_out.getX(), normal_out.getY(), normal_out.getZ());
            if (lengthSqr(v) > 0.0f) {
                normalize(v);
            }
            *out_buffer++ = v[0];
            *out_buffer++ = v[1];
            *out_buffer++ = v[2];
        }

        return out_buffer;
    }

    static float* GeneratePositionData(const dmRigDDF::Mesh* mesh, const Matrix4& model_matrix, const dmArray<Matrix4>& pose_matrices, float* out_buffer)
    {
        const float *positions = mesh->m_Positions.m_Data;
        const size_t vertex_count = mesh->m_Positions.m_Count / 3;
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
            const uint32_t bi_offset = i << 2;
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

    static void PoseToMatrix(const dmArray<dmTransform::Transform>& pose, dmArray<Matrix4>& out_matrices)
    {
        uint32_t bone_count = pose.Size();
        for (uint32_t bi = 0; bi < bone_count; ++bi)
        {
            out_matrices[bi] = dmTransform::ToMatrix4(pose[bi]);
        }
    }

    static void PoseToModelSpace(const dmRigDDF::Skeleton* skeleton, const dmArray<dmTransform::Transform>& pose, dmArray<dmTransform::Transform>& out_pose)
    {
        const dmRigDDF::Bone* bones = skeleton->m_Bones.m_Data;
        uint32_t bone_count = skeleton->m_Bones.m_Count;
        for (uint32_t bi = 0; bi < bone_count; ++bi)
        {
            const dmTransform::Transform& transform = pose[bi];
            dmTransform::Transform& out_transform = out_pose[bi];
            out_transform = transform;
            if (bi > 0) {
                const dmRigDDF::Bone* bone = &bones[bi];
                if (bone->m_InheritScale)
                {
                    out_transform = dmTransform::Mul(out_pose[bone->m_Parent], transform);
                }
                else
                {
                    Vector3 scale = transform.GetScale();
                    out_transform = dmTransform::Mul(out_pose[bone->m_Parent], transform);
                    out_transform.SetScale(scale);
                }
            }
        }
    }

    static void PoseToModelSpace(const dmRigDDF::Skeleton* skeleton, const dmArray<Matrix4>& pose, dmArray<Matrix4>& out_pose)
    {
        const dmRigDDF::Bone* bones = skeleton->m_Bones.m_Data;
        uint32_t bone_count = skeleton->m_Bones.m_Count;
        for (uint32_t bi = 0; bi < bone_count; ++bi)
        {
            const Matrix4& transform = pose[bi];
            Matrix4& out_transform = out_pose[bi];
            out_transform = transform;
            if (bi > 0) {
                const dmRigDDF::Bone* bone = &bones[bi];
                if (bone->m_InheritScale)
                {
                    out_transform = out_pose[bone->m_Parent] * transform;
                }
                else
                {
                    Vector3 scale = dmTransform::ExtractScale(out_pose[bone->m_Parent]);
                    out_transform.setUpper3x3(Matrix3::scale(Vector3(1.0f/scale.getX(), 1.0f/scale.getY(), 1.0f/scale.getZ())) * transform.getUpper3x3());
                    out_transform = out_pose[bone->m_Parent] * transform;
                }
            }
        }
    }

    static void PoseToInfluence(const dmArray<uint32_t>& pose_idx_to_influence, const dmArray<Matrix4>& in_pose, dmArray<Matrix4>& out_pose)
    {
        for (int i = 0; i < pose_idx_to_influence.Size(); ++i)
        {
            uint32_t j = pose_idx_to_influence[i];
            out_pose[j] = in_pose[i];
        }
    }

    // NOTE: We have two different vertex data write functions, since we expose two different vertex formats (spine and model).
    // This is a temporary fix until we have better support for custom vertex formats.
    static RigModelVertex* WriteVertexData(const dmRigDDF::Mesh* mesh, const float* positions, const float* normals, RigModelVertex* out_write_ptr)
    {
        uint32_t indices_count = mesh->m_Indices.m_Count;
        const uint32_t* indices = mesh->m_Indices.m_Data;
        const uint32_t* uv0_indices = mesh->m_Texcoord0Indices.m_Count ? mesh->m_Texcoord0Indices.m_Data : mesh->m_Indices.m_Data;
        const float* uv0 = mesh->m_Texcoord0.m_Data;

        if (mesh->m_NormalsIndices.m_Count)
        {
            for (uint32_t i = 0; i < indices_count; ++i)
            {
                uint32_t vi = indices[i];
                uint32_t e = vi * 3;
                out_write_ptr->x = positions[e];
                out_write_ptr->y = positions[++e];
                out_write_ptr->z = positions[++e];
                vi = uv0_indices[i];
                e = vi << 1;
                out_write_ptr->u = uv0[e+0];
                out_write_ptr->v = uv0[e+1];
                e = i * 3;
                out_write_ptr->nx = normals[e];
                out_write_ptr->ny = normals[++e];
                out_write_ptr->nz = normals[++e];
                out_write_ptr++;
            }
        }
        else
        {
            for (uint32_t i = 0; i < indices_count; ++i)
            {
                uint32_t vi = indices[i];
                uint32_t e = vi * 3;
                out_write_ptr->x = positions[e];
                out_write_ptr->y = positions[++e];
                out_write_ptr->z = positions[++e];
                vi = uv0_indices[i];
                e = vi << 1;
                out_write_ptr->u = uv0[e+0];
                out_write_ptr->v = uv0[e+1];
                out_write_ptr->nx = 0.0f;
                out_write_ptr->ny = 0.0f;
                out_write_ptr->nz = 1.0f;
                out_write_ptr++;
            }
        }

        return out_write_ptr;
    }

    static RigSpineModelVertex* WriteVertexData(const dmRigDDF::Mesh* mesh, const float* positions, uint32_t color, RigSpineModelVertex* out_write_ptr)
    {
        uint32_t indices_count = mesh->m_Indices.m_Count;
        const uint32_t* indices = mesh->m_Indices.m_Data;
        const uint32_t* uv0_indices = mesh->m_Texcoord0Indices.m_Count ? mesh->m_Texcoord0Indices.m_Data : mesh->m_Indices.m_Data;
        const float* uv0 = mesh->m_Texcoord0.m_Data;

        for (uint32_t i = 0; i < indices_count; ++i)
        {
            uint32_t vi = indices[i];
            uint32_t e = vi*3;
            out_write_ptr->x = positions[e+0];
            out_write_ptr->y = positions[e+1];
            out_write_ptr->z = positions[e+2];
            vi = uv0_indices[i];
            e = vi << 1;
            out_write_ptr->u = (uv0[e+0]);
            out_write_ptr->v = (uv0[e+1]);
            out_write_ptr->rgba = color;
            out_write_ptr++;
        }
        return out_write_ptr;
    }

    void* GenerateVertexData(dmRig::HRigContext context, dmRig::HRigInstance instance, const Matrix4& model_matrix, const Matrix4& normal_matrix, const Vector4 color, RigVertexFormat vertex_format, void* vertex_data_out)
    {
        const dmRigDDF::MeshEntry* mesh_entry = instance->m_MeshEntry;
        if (!instance->m_MeshEntry || !instance->m_DoRender) {
            return vertex_data_out;
        }

        // Early exit for rigs that has no mesh or only one mesh that is not visible.
        int mesh_slot_count = mesh_entry->m_MeshSlots.m_Count;
        if (mesh_slot_count == 0) {
            return vertex_data_out;

        } else if (mesh_slot_count == 1) {
            int active_attachment = instance->m_MeshSlotPose[0].m_ActiveAttachment;
            if (active_attachment == -1 || mesh_entry->m_MeshSlots[0].m_MeshAttachments[active_attachment] == -1) {
                return vertex_data_out;
            }
        }

        dmArray<Matrix4>& pose_matrices      = context->m_ScratchPoseMatrixBuffer;
        dmArray<Matrix4>& influence_matrices = context->m_ScratchInfluenceMatrixBuffer;
        dmArray<Vector3>& positions          = context->m_ScratchPositionBuffer;
        dmArray<Vector3>& normals            = context->m_ScratchNormalBuffer;

        // If the rig has bones, update the pose to be local-to-model
        uint32_t bone_count = GetBoneCount(instance);
        influence_matrices.SetSize(0);
        if (bone_count && instance->m_PoseIdxToInfluence->Size() > 0) {

            // Make sure pose scratch buffers have enough space
            if (pose_matrices.Capacity() < bone_count) {
                uint32_t size_offset = bone_count - pose_matrices.Capacity();
                pose_matrices.OffsetCapacity(size_offset);
            }
            pose_matrices.SetSize(bone_count);

            // Make sure influence scratch buffers have enough space sufficient for max bones to be indexed
            uint32_t max_bone_count = instance->m_MaxBoneCount;
            if (influence_matrices.Capacity() < max_bone_count) {
                uint32_t capacity = influence_matrices.Capacity();
                uint32_t size_offset = max_bone_count - capacity;
                influence_matrices.OffsetCapacity(size_offset);
                influence_matrices.SetSize(max_bone_count);
                for(uint32_t i = capacity; i < capacity+size_offset; ++i)
                    influence_matrices[i] = Matrix4::identity();
            }
            influence_matrices.SetSize(max_bone_count);

            const dmArray<dmTransform::Transform>& pose = instance->m_Pose;
            const dmRigDDF::Skeleton* skeleton = instance->m_Skeleton;
            if (skeleton->m_LocalBoneScaling) {

                dmArray<dmTransform::Transform>& pose_transforms = context->m_ScratchPoseTransformBuffer;
                if (pose_transforms.Capacity() < bone_count) {
                    pose_transforms.OffsetCapacity(bone_count - pose_transforms.Capacity());
                }
                pose_transforms.SetSize(bone_count);

                PoseToModelSpace(skeleton, pose, pose_transforms);
                PoseToMatrix(pose_transforms, pose_matrices);
            } else {
                PoseToMatrix(pose, pose_matrices);
                PoseToModelSpace(skeleton, pose_matrices, pose_matrices);
            }

            // Premultiply pose matrices with the bind pose inverse so they
            // can be directly be used to transform each vertex.
            const dmArray<RigBone>& bind_pose = *instance->m_BindPose;
            for (uint32_t bi = 0; bi < pose_matrices.Size(); ++bi)
            {
                Matrix4& pose_matrix = pose_matrices[bi];
                pose_matrix = pose_matrix * bind_pose[bi].m_ModelToLocal;
            }

            // Rearrange pose matrices to indices that the mesh vertices understand.
            PoseToInfluence(*instance->m_PoseIdxToInfluence, pose_matrices, influence_matrices);
        }

        // Loop that generates actual vertex data for current mesh entry.
        // We loop over the slots in the mesh entry, check which attachment point is active,
        // then locate the actual mesh that has been assigned to that attatchment point.
        // (Instead of looping over the default slot orders directly, we go through the
        // draw order array to render the slots in the correct order.)
        int32_t slot_count = instance->m_MeshSet->m_SlotCount;
        for (int32_t i = 0; i < slot_count; i++)
        {
            int32_t slot_index = instance->m_DrawOrder[i];

            const dmRigDDF::MeshSlot* mesh_slot = &instance->m_MeshEntry->m_MeshSlots[slot_index];
            MeshSlotPose* mesh_slot_pose = &instance->m_MeshSlotPose[slot_index];

            // Get active attachment in the current slot.
            int active_attachment = mesh_slot_pose->m_ActiveAttachment;
            if (active_attachment >= 0) {

                // Check if the attachment point has a mesh assigned
                int mesh_attachment_index = mesh_slot->m_MeshAttachments[active_attachment];
                if (mesh_attachment_index >= 0)
                {
                    // Lookup the mesh from the list of all the available meshes.
                    const Mesh* mesh_attachment = &instance->m_MeshSet->m_MeshAttachments[mesh_attachment_index];

                    // Bump scratch buffer capacity to handle current vertex count
                    uint32_t index_count = mesh_attachment->m_Indices.m_Count;
                    if (positions.Capacity() < index_count) {
                        positions.OffsetCapacity(index_count - positions.Capacity());
                    }
                    positions.SetSize(index_count);

                    if (vertex_format == RIG_VERTEX_FORMAT_MODEL && mesh_attachment->m_NormalsIndices.m_Count) {
                        if (normals.Capacity() < index_count) {
                            normals.OffsetCapacity(index_count - normals.Capacity());
                        }
                        normals.SetSize(index_count);
                    }

                    // Fill scratch buffers for positions, and normals if applicable, using pose matrices.
                    float* positions_buffer = (float*)positions.Begin();
                    float* normals_buffer = (float*)normals.Begin();
                    dmRig::GeneratePositionData(mesh_attachment, model_matrix, influence_matrices, positions_buffer);
                    if (vertex_format == RIG_VERTEX_FORMAT_MODEL && mesh_attachment->m_NormalsIndices.m_Count) {
                        dmRig::GenerateNormalData(mesh_attachment, normal_matrix, influence_matrices, normals_buffer);
                    }

                    // NOTE: We expose two different vertex format that GenerateVertexData can output.
                    // This is a temporary fix until we have better support for custom vertex formats.
                    if (vertex_format == RIG_VERTEX_FORMAT_MODEL) {
                        vertex_data_out = (void*)WriteVertexData(mesh_attachment, positions_buffer, normals_buffer, (RigModelVertex*)vertex_data_out);
                    } else {
                        Vector4 mesh_color = Vector4(mesh_slot_pose->m_Color[0], mesh_slot_pose->m_Color[1], mesh_slot_pose->m_Color[2], mesh_slot_pose->m_Color[3]);
                        mesh_color = mulPerElem(color, mesh_color);

                        uint32_t rgba = (((uint32_t) (mesh_color.getW() * 255.0f)) << 24) | (((uint32_t) (mesh_color.getZ() * 255.0f)) << 16) |
                                (((uint32_t) (mesh_color.getY() * 255.0f)) << 8) | ((uint32_t) (mesh_color.getX() * 255.0f));

                        vertex_data_out = (void*)WriteVertexData(mesh_attachment, positions_buffer, rgba, (RigSpineModelVertex*)vertex_data_out);
                    }
                }
            }
        }

        return vertex_data_out;
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
        return (instance->m_MeshEntry != 0x0);
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

    static void DestroyInstance(HRigContext context, uint32_t index)
    {
        RigInstance* instance = context->m_Instances.Get(index);
        // If we're going to use memset, then we should explicitly clear pose and instance arrays.
        instance->m_Pose.SetCapacity(0);
        instance->m_IKTargets.SetCapacity(0);
        instance->m_MeshSlotPose.SetCapacity(0);
        delete instance;
        context->m_Instances.Free(index, true);
    }

    Result InstanceCreate(const InstanceCreateParams& params)
    {
        RigContext* context = (RigContext*)params.m_Context;

        if (context->m_Instances.Full())
        {
            dmLogError("Rig instance could not be created since the buffer is full (%d).", context->m_Instances.Capacity());
            return dmRig::RESULT_ERROR_BUFFER_FULL;
        }

        *params.m_Instance = new RigInstance;
        RigInstance* instance = *params.m_Instance;

        uint32_t index = context->m_Instances.Alloc();
        memset(instance, 0, sizeof(RigInstance));
        instance->m_Index = index;
        context->m_Instances.Set(index, instance);
        instance->m_MeshId = params.m_MeshId;

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
        instance->m_PoseIdxToInfluence = params.m_PoseIdxToInfluence;
        instance->m_TrackIdxToPose     = params.m_TrackIdxToPose;

        instance->m_Enabled = 1;

        AllocateMeshSlotPose(params.m_MeshSet, instance->m_MeshSlotPose, instance->m_DrawOrder);
        instance->m_MeshEntry = FindMeshEntry(params.m_MeshSet, instance->m_MeshId);

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

        ResetMeshSlotPose(instance);

        return dmRig::RESULT_OK;
    }

    Result InstanceDestroy(const InstanceDestroyParams& params)
    {
        if (!params.m_Context || !params.m_Instance) {
            return dmRig::RESULT_ERROR;
        }

        DestroyInstance((RigContext*)params.m_Context, params.m_Instance->m_Index);
        return dmRig::RESULT_OK;
    }

    void CreateBindPose(dmRigDDF::Skeleton& skeleton, dmArray<RigBone>& bind_pose)
    {
        uint32_t bone_count = skeleton.m_Bones.m_Count;
        bind_pose.SetCapacity(bone_count);
        bind_pose.SetSize(bone_count);
        for (uint32_t i = 0; i < bone_count; ++i)
        {
            dmRig::RigBone* bind_bone = &bind_pose[i];
            dmRigDDF::Bone* bone = &skeleton.m_Bones[i];
            bind_bone->m_LocalToParent = dmTransform::Transform(Vector3(bone->m_Position), bone->m_Rotation, bone->m_Scale);
            if (i > 0)
            {
                bind_bone->m_LocalToModel = dmTransform::Mul(bind_pose[bone->m_Parent].m_LocalToModel, bind_bone->m_LocalToParent);
                if (!bone->m_InheritScale)
                {
                    bind_bone->m_LocalToModel.SetScale(bind_bone->m_LocalToParent.GetScale());
                }
            }
            else
            {
                bind_bone->m_LocalToModel = bind_bone->m_LocalToParent;
            }
            bind_bone->m_ModelToLocal = inverse(dmTransform::ToMatrix4(bind_bone->m_LocalToModel));
            bind_bone->m_ParentIndex = bone->m_Parent;
            bind_bone->m_Length = bone->m_Length;
        }
    }

    static const uint32_t INVALID_BONE_IDX = 0xffffffff;
    static uint32_t FindBoneInList(uint64_t* list, uint32_t count, uint64_t bone_id)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            uint64_t entry = list[i];
            if (bone_id == entry) {
                return i;
            }
        }

        return INVALID_BONE_IDX;
    }

    void FillBoneListArrays(const dmRigDDF::MeshSet& meshset, const dmRigDDF::AnimationSet& animationset, const dmRigDDF::Skeleton& skeleton, dmArray<uint32_t>& track_idx_to_pose, dmArray<uint32_t>& pose_idx_to_influence)
    {
        // Create lookup arrays
        // - track-to-pose, used to convert animation track bonde index into correct pose transform index
        // - pose-to-influence, used during vertex generation to convert pose transform index into influence index
        uint32_t bone_count = skeleton.m_Bones.m_Count;

        track_idx_to_pose.SetCapacity(bone_count);
        track_idx_to_pose.SetSize(bone_count);
        memset((void*)track_idx_to_pose.Begin(), 0x0, track_idx_to_pose.Size()*sizeof(uint32_t));
        pose_idx_to_influence.SetCapacity(bone_count);
        pose_idx_to_influence.SetSize(bone_count);

        uint32_t anim_bone_list_count = animationset.m_BoneList.m_Count;
        uint32_t mesh_bone_list_count = meshset.m_BoneList.m_Count;

        for (int bi = 0; bi < bone_count; ++bi)
        {
            uint64_t bone_id = skeleton.m_Bones[bi].m_Id;

            if (anim_bone_list_count) {
                uint32_t track_idx = FindBoneInList(animationset.m_BoneList.m_Data, animationset.m_BoneList.m_Count, bone_id);
                if (track_idx != INVALID_BONE_IDX) {
                    track_idx_to_pose[track_idx] = bi;
                }
            } else {
                track_idx_to_pose[bi] = bi;
            }

            if (mesh_bone_list_count) {
                uint32_t influence_idx = FindBoneInList(meshset.m_BoneList.m_Data, meshset.m_BoneList.m_Count, bone_id);
                if (influence_idx != INVALID_BONE_IDX) {
                    pose_idx_to_influence[bi] = influence_idx;
                } else {
                    // If there is no influence index for the current bone
                    // we still need to put the pose matrix somewhere during
                    // pose-to-influence rearrangement so just put it last.
                    pose_idx_to_influence[bi] = bone_count - 1;
                }
            } else {
                pose_idx_to_influence[bi] = bi;
            }
        }
    }

}
