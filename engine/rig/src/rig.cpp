#include "rig.h"

#include <dlib/log.h>
#include <dlib/profile.h>

namespace dmRig
{

    static const dmhash_t NULL_ANIMATION = dmHashString64("");
    static const uint32_t INVALID_BONE_INDEX = 0xffff;
    static const float CURSOR_EPSILON = 0.0001f;
    static const uint32_t SIGNAL_ORDER_LOCKED = 0x10cced; // "locked" indicates that draw order offset should not be modified
    static const int SIGNAL_SLOT_UNUSED = -1; // Used to indicate if a draw order slot is unused

    /// Config key to use for tweaking the total maximum number of rig instances in a context.
    const char* RIG_MAX_INSTANCES_KEY = "rig.max_instance_count";

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

    static void UpdateMeshProperties(HRigInstance instance)
    {
        instance->m_DoRender = 0;
        if (instance->m_MeshEntry != 0x0) {
            uint32_t mesh_count = instance->m_MeshEntry->m_Meshes.m_Count;
            instance->m_MeshProperties.SetSize(mesh_count);
            for (uint32_t mesh_index = 0; mesh_index < mesh_count; ++mesh_index) {
                const dmRigDDF::Mesh* mesh = &instance->m_MeshEntry->m_Meshes[mesh_index];
                float* color = mesh->m_Color.m_Data;
                float* skin_color = mesh->m_SkinColor.m_Data;
                MeshProperties* properties = &instance->m_MeshProperties[mesh_index];
                properties->m_Color[0] = color[0] * skin_color[0];
                properties->m_Color[1] = color[1] * skin_color[1];
                properties->m_Color[2] = color[2] * skin_color[2];
                properties->m_Color[3] = color[3] * skin_color[3];
                properties->m_Order = mesh->m_DrawOrder;
                properties->m_Visible = mesh->m_Visible;
                properties->m_OrderOffset = 0;
            }
            instance->m_DoRender = 1;
        } else {
            instance->m_MeshProperties.SetSize(0);
        }

        // Make sure we recalculate the draw order next frame.
        instance->m_DrawOrderToMesh.SetCapacity(0);
        instance->m_DrawOrderToMesh.SetSize(0);
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
        player->m_AnimationId = animation_id;
        player->m_Animation = anim;
        SetCursor(instance, offset, true);
        SetPlaybackRate(instance, playback_rate);
        player->m_Playing = 1;
        player->m_Playback = playback;

        if (player->m_Playback == dmRig::PLAYBACK_ONCE_BACKWARD || player->m_Playback == dmRig::PLAYBACK_LOOP_BACKWARD)
            player->m_Backwards = 1;
        else
            player->m_Backwards = 0;

        UpdateMeshProperties(instance);

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

                UpdateMeshProperties(instance);
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
        return lerp(frac, Quat(data[i+0], data[i+1], data[i+2], data[i+3]), Quat(data[i+0+4], data[i+1+4], data[i+2+4], data[i+3+4]));
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


    static void ApplyAnimation(RigPlayer* player, dmArray<dmTransform::Transform>& pose, const dmArray<uint32_t>& track_idx_to_pose, dmArray<IKAnimation>& ik_animation, dmArray<MeshProperties>& properties, float blend_weight, dmhash_t mesh_id, bool draw_order, bool& updated_draw_order)
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
                transform.SetRotation(lerp(blend_weight, transform.GetRotation(), SampleQuat(sample, fraction, track->m_Rotations.m_Data)));
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
            if (mesh_id == track->m_MeshId) {
                MeshProperties& props = properties[track->m_MeshIndex];
                if (track->m_Colors.m_Count > 0) {
                    Vector4 color(props.m_Color[0], props.m_Color[1], props.m_Color[2], props.m_Color[3]);
                    color = lerp(blend_weight, color, SampleVec4(sample, fraction, track->m_Colors.m_Data));
                    props.m_Color[0] = color[0];
                    props.m_Color[1] = color[1];
                    props.m_Color[2] = color[2];
                    props.m_Color[3] = color[3];
                    props.m_ColorFromTrack = true;
                }
                if (track->m_Visible.m_Count > 0) {
                    if (blend_weight >= 0.5f) {
                        if (props.m_Visible != track->m_Visible[rounded_sample]) {
                            updated_draw_order = true;
                            props.m_Visible = track->m_Visible[rounded_sample];
                        }
                        props.m_VisibleFromTrack = true;
                    }
                }

                if (track->m_OrderOffset.m_Count > 0 && draw_order) {
                    if (props.m_OrderOffset != track->m_OrderOffset[rounded_sample]) {
                        updated_draw_order = true;
                        props.m_OrderOffset = track->m_OrderOffset[rounded_sample];
                    }
                    props.m_OffsetFromTrack = true;
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
            // NOTE we previously checked for (!instance->m_Enabled || !instance->m_AddedToUpdate) here also
            if (instance->m_Pose.Empty() || !instance->m_Enabled)
                continue;

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
            dmArray<MeshProperties>& properties = instance->m_MeshProperties;
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

            // Clear visible, color and offset updated flags.
            // We do this to keep track of which properties were updated during
            // during the ApplyAnimation step.
            for (uint32_t pi = 0; pi < properties.Size(); ++pi)
            {
                MeshProperties* prop = &properties[pi];
                prop->m_VisibleFromTrack = false;
                prop->m_ColorFromTrack = false;
                prop->m_OffsetFromTrack = false;
            }

            bool updated_draw_order = false;
            RigPlayer* player = GetPlayer(instance);
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
                    ApplyAnimation(p, pose, track_idx_to_pose, ik_animation, properties, alpha, instance->m_MeshId, draw_order, updated_draw_order);
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
                ApplyAnimation(player, pose, track_idx_to_pose, ik_animation, properties, 1.0f, instance->m_MeshId, true, updated_draw_order);
            }

            // Fill in properties (from bind pose) if they were not updated in the ApplyAnimation step.
            // Do this so we don't need to reset all properties to their bind pose properties each frame.
            // Doing so would mean we would not be able to keep track if the applied animation needs to
            // rebuild the draw order array.
            for (uint32_t pi = 0; pi < properties.Size(); ++pi)
            {
                MeshProperties* prop = &properties[pi];
                const dmRigDDF::Mesh* mesh = &instance->m_MeshEntry->m_Meshes[pi];
                if (!prop->m_VisibleFromTrack) {
                    if (prop->m_Visible != mesh->m_Visible) {
                        updated_draw_order = true;
                    }
                    prop->m_Visible = mesh->m_Visible;
                }

                if (prop->m_ColorFromTrack)
                {
                    float* skin_color = mesh->m_SkinColor.m_Data;
                    prop->m_Color[0] *= skin_color[0];
                    prop->m_Color[1] *= skin_color[1];
                    prop->m_Color[2] *= skin_color[2];
                    prop->m_Color[3] *= skin_color[3];
                }
                if (!prop->m_OffsetFromTrack) {
                    if (prop->m_OrderOffset != 0) {
                        updated_draw_order = true;
                    }
                    prop->m_OrderOffset = 0;
                }
            }

            // If the draw order was changed during animation, we reset the size of the m_DrawOrderToMesh
            // so that it is recalculated during render.
            if (updated_draw_order) {
                instance->m_DrawOrderToMesh.SetCapacity(0);
            }

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
    }

    static Result PostUpdate(HRigContext context)
    {
        const dmArray<RigInstance*>& instances = context->m_Instances.m_Objects;
        uint32_t count = instances.Size();
        for (uint32_t i = 0; i < count; ++i)
        {
            RigInstance* instance = instances[i];
            dmArray<dmTransform::Transform>& pose = instance->m_Pose;
            if (pose.Empty())
                continue;

            // Notify any listener that the pose has been recalculated
            if (instance->m_PoseCallback) {
                instance->m_PoseCallback(instance->m_PoseCBUserData1, instance->m_PoseCBUserData2);
            }


        }

        return dmRig::RESULT_OK;
    }

    Result Update(HRigContext context, float dt)
    {
        DM_PROFILE(Rig, "Update");

        Animate(context, dt);

        return PostUpdate(context);
    }


    static void AllocateMeshProperties(const dmRig::MeshSet* mesh_set, dmArray<MeshProperties>& mesh_properties) {
        uint32_t max_mesh_count = 0;
        uint32_t count = mesh_set->m_MeshEntries.m_Count;
        for (uint32_t i = 0; i < count; ++i) {
            uint32_t mesh_count = mesh_set->m_MeshEntries[i].m_Meshes.m_Count;
            if (mesh_count > max_mesh_count) {
                max_mesh_count = mesh_count;
            }
        }
        mesh_properties.SetCapacity(max_mesh_count);
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

	static void UpdateMeshDrawOrder(const HRigInstance instance, uint32_t mesh_count, dmArray<uint32_t>& out_order_to_mesh, dmArray<int32_t>& slots_scratch_buffer)
    {
        // Make sure output array has zero to begin with
        out_order_to_mesh.SetSize(0);

        // We resolve draw order and offsets in these two steps:
        //   I. Add entries with changed draw order (has explicit offset)
        //  II. Add untouched entries (those with no explicit offset)
        //
        // E.g.:
        // Bind draw order: [0, 1, 2]
        // Keyframe has entry 1 with offset -1, results in:
        //   (I) Entry 1 is placed first => [1, _, _]
        //   (II) Entry 0 has no offset. Entry 0 can't be placed at index 0, so look for next empty space,
        //        which is index 1:
        //        => [1, 0, _]
        //        Entry 2 also has no offset, and index 2 is empty so it can be placed directly:
        //        => [1, 0, 2]
        //
        // An exception is made if there is an explicit entry with offset "0". This means that previous order should
        // be conserved. These entries are marked as SIGNAL_ORDER_LOCKED. In that case the order should be unchanged,
        // so they will be placed in step (I) as those with explicit offset.

        // No need to reorder instances with only one, or none, meshes.
        if (mesh_count <= 1) {
            if (mesh_count == 1 && instance->m_MeshProperties[0].m_Visible) {
                out_order_to_mesh.Push(0);
            }
            return;
        }

        uint32_t slot_count = instance->m_MeshSet->m_SlotCount;
        out_order_to_mesh.SetCapacity(slot_count);

        // We use the scratch buffer to temporaraly keep track of slots and "unchanged" entries.
        // Unchanged entries will be used if there are some slot order changes, using the
        // algorithm from the official Spine C runtime.
        if (slots_scratch_buffer.Capacity() < slot_count*2) {
            slots_scratch_buffer.SetCapacity(slot_count*2);
            slots_scratch_buffer.SetSize(slot_count*2);
        }

        // Get pointers to slots and unchanged arrays from the scratch buffer.
        int32_t* slots = slots_scratch_buffer.Begin();
        int32_t* unchanged = &slots[slot_count];

        // Fill slot list with values indicating all slots are currently unused.
        for (int32_t i = 0; i < slot_count; ++i) {
            slots[i] = SIGNAL_SLOT_UNUSED;
        }

        int32_t changed_count = 0; // Keep track of how many slots have changed (needs to be reordered).
        for (int32_t i = 0; i < mesh_count; ++i)
        {
            dmRig::MeshProperties *props = &instance->m_MeshProperties[i];
            uint32_t slot_index = props->m_Order;
            int32_t offset      = props->m_OrderOffset;
            props->m_Slot       = slot_index;
            props->m_MeshId     = i;

            if (props->m_Visible) {
                // If this mesh entry is visible, we will always add/overwrite the slot
                // since there can only be one visible entry in each slot.
                slots[slot_index] = i;

                // We try to output the mesh directly to the output array
                // in case we don't find any changed slots.
                out_order_to_mesh.Push(i);
            } else if (slots[slot_index] == SIGNAL_SLOT_UNUSED) {
                slots[slot_index] = i;
            }

            // If this entry has an offset, this means it will need to be reordered.
            if (offset != 0) {
                changed_count++;

                // Apply slot order offset, but ignore entries that have a "locked" offset.
                if (props->m_OrderOffset != SIGNAL_ORDER_LOCKED) {
                    props->m_Slot = slot_index + offset;
                }
            }
        }

        // Slot reordering code should work similar to how the official spine-c implementation works:
        // https://github.com/EsotericSoftware/spine-runtimes/blob/387b0afb80a775970c48099042be769e50258440/spine-c/spine-c/src/spine/SkeletonJson.c#L430
        if (changed_count > 0) {

            out_order_to_mesh.SetSize(slot_count);
            for (int32_t i = 0; i < slot_count; ++i) {
                out_order_to_mesh[i] = SIGNAL_SLOT_UNUSED;
            }

            int32_t original_index = 0;
            int32_t unchanged_index = 0;
            for (int32_t slot_index = 0; slot_index < slot_count; ++slot_index)
            {

                if (slots[slot_index] != SIGNAL_SLOT_UNUSED) {
                    dmRig::MeshProperties* slot = &instance->m_MeshProperties[slots[slot_index]];
                    if (slot->m_OrderOffset != 0)
                    {
                        while (original_index != slot_index) {
                            unchanged[unchanged_index++] = original_index++;
                        }

                        out_order_to_mesh[slot->m_Slot] = slot->m_MeshId;
                        original_index++;
                    }
                }
            }

            // Collect remaining unchanged items.
            while (original_index < slot_count) {
                unchanged[unchanged_index++] = original_index++;
            }

            // Fill in unchanged items.
            for (int32_t i = slot_count - 1; i >= 0; i--) {
                if (out_order_to_mesh[i] == SIGNAL_SLOT_UNUSED) {
                    out_order_to_mesh[i] = slots[unchanged[--unchanged_index]];
                }
            }
        }
    }

    uint32_t GetVertexCount(HRigInstance instance)
    {
        if (!instance->m_MeshEntry || !instance->m_DoRender) {
            return 0;
        }

        uint32_t vertex_count = 0;
        uint32_t mesh_count = instance->m_MeshEntry->m_Meshes.m_Count;
        for (uint32_t mesh_index = 0; mesh_index < mesh_count; ++mesh_index) {
            if (instance->m_MeshProperties[mesh_index].m_Visible) {
                vertex_count += instance->m_MeshEntry->m_Meshes[mesh_index].m_Indices.m_Count;
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

        uint32_t mesh_count = mesh_entry->m_Meshes.m_Count;

        // Early exit for rigs that has no mesh or only one mesh that is not visible.
        if (mesh_count == 0 || (mesh_count == 1 && !instance->m_MeshProperties[0].m_Visible)) {
            return vertex_data_out;
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

        dmArray<uint32_t>& draw_order = instance->m_DrawOrderToMesh;
        if (draw_order.Capacity() < mesh_count) {
            draw_order.SetCapacity(mesh_count);
            dmRig::UpdateMeshDrawOrder(instance, mesh_count, draw_order, context->m_ScratchSlotsBuffer);
        }
        uint32_t visible_mesh_count = draw_order.Size();
        for (uint32_t draw_index = 0; draw_index < visible_mesh_count; ++draw_index) {
            uint32_t mesh_index = draw_order[draw_index];
            if (mesh_index == SIGNAL_SLOT_UNUSED) {
                continue;
            }
            const dmRig::MeshProperties* properties = &instance->m_MeshProperties[mesh_index];
            if (!properties->m_Visible) {
                continue;
            }
            const dmRigDDF::Mesh* mesh = &mesh_entry->m_Meshes[mesh_index];

            // Bump scratch buffer capacity to handle current vertex count
            uint32_t index_count = mesh->m_Indices.m_Count;
            if (positions.Capacity() < index_count) {
                positions.OffsetCapacity(index_count - positions.Capacity());
            }
            positions.SetSize(index_count);

            if (vertex_format == RIG_VERTEX_FORMAT_MODEL && mesh->m_NormalsIndices.m_Count) {
                if (normals.Capacity() < index_count) {
                    normals.OffsetCapacity(index_count - normals.Capacity());
                }
                normals.SetSize(index_count);
            }

            // Fill scratch buffers for positions, and normals if applicable, using pose matrices.
            float* positions_buffer = (float*)positions.Begin();
            float* normals_buffer = (float*)normals.Begin();
            dmRig::GeneratePositionData(mesh, model_matrix, influence_matrices, positions_buffer);
            if (vertex_format == RIG_VERTEX_FORMAT_MODEL && mesh->m_NormalsIndices.m_Count) {
                dmRig::GenerateNormalData(mesh, normal_matrix, influence_matrices, normals_buffer);
            }

            // NOTE: We expose two different vertex format that GenerateVertexData can output.
            // This is a temporary fix until we have better support for custom vertex formats.
            if (vertex_format == RIG_VERTEX_FORMAT_MODEL) {
                vertex_data_out = (void*)WriteVertexData(mesh, positions_buffer, normals_buffer, (RigModelVertex*)vertex_data_out);
            } else {
                Vector4 mesh_color = Vector4(properties->m_Color[0], properties->m_Color[1], properties->m_Color[2], properties->m_Color[3]);
                mesh_color = mulPerElem(color, mesh_color);

                uint32_t rgba = (((uint32_t) (mesh_color.getW() * 255.0f)) << 24) | (((uint32_t) (mesh_color.getZ() * 255.0f)) << 16) |
                        (((uint32_t) (mesh_color.getY() * 255.0f)) << 8) | ((uint32_t) (mesh_color.getX() * 255.0f));

                vertex_data_out = (void*)WriteVertexData(mesh, positions_buffer, rgba, (RigSpineModelVertex*)vertex_data_out);
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
        instance->m_MeshProperties.SetCapacity(0);
        delete instance;
        context->m_Instances.Free(index, true);
    }

    Result InstanceCreate(const InstanceCreateParams& params)
    {
        RigContext* context = (RigContext*)params.m_Context;

        if (context->m_Instances.Full())
        {
            dmLogError("Rig Instance could not be created since the buffer is full (%d), consider increasing %s.", context->m_Instances.Capacity(), RIG_MAX_INSTANCES_KEY);
            return dmRig::RESULT_ERROR;
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

        AllocateMeshProperties(params.m_MeshSet, instance->m_MeshProperties);
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

        UpdateMeshProperties(instance);

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

#define DM_RIG_TRAMPOLINE1(ret, name, t1) \
    ret Rig_##name(t1 a1)\
    {\
        return name(a1);\
    }\

#define DM_RIG_TRAMPOLINE2(ret, name, t1, t2) \
    ret Rig_##name(t1 a1, t2 a2)\
    {\
        return name(a1, a2);\
    }\

#define DM_RIG_TRAMPOLINE3(ret, name, t1, t2, t3) \
    ret Rig_##name(t1 a1, t2 a2, t3 a3)\
    {\
        return name(a1, a2, a3);\
    }\

    DM_RIG_TRAMPOLINE1(Result, NewContext, const NewContextParams&);
    DM_RIG_TRAMPOLINE1(void, DeleteContext, HRigContext);
    DM_RIG_TRAMPOLINE2(Result, Update, HRigContext, float);

}
