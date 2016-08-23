#include "rig.h"

#include <dlib/log.h>
#include <dlib/profile.h>

namespace dmRig
{

    static const dmhash_t NULL_ANIMATION = dmHashString64("");
    static const uint32_t INVALID_BONE_INDEX = 0xffff;

    /// Config key to use for tweaking the total maximum number of rig instances in a context.
    const char* MAX_RIG_INSTANCE_COUNT_KEY = "rig.max_instance_count";

    Result NewContext(const NewContextParams& params)
    {
        *params.m_Context = new RigContext();
        RigContext* context = *params.m_Context;
        if (!context) {
            return dmRig::RESULT_ERROR;
        }

        context->m_Instances.SetCapacity(params.m_MaxRigInstanceCount);

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

    Result PlayAnimation(HRigInstance instance, dmhash_t animation_id, dmGameObject::Playback playback, float blend_duration)
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
        player->m_Cursor = 0.0f;
        player->m_Playing = 1;
        player->m_Playback = playback;
        if (player->m_Playback == dmGameObject::PLAYBACK_ONCE_BACKWARD || player->m_Playback == dmGameObject::PLAYBACK_LOOP_BACKWARD)
            player->m_Backwards = 1;
        else
            player->m_Backwards = 0;

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

    dmhash_t GetSkin(HRigInstance instance)
    {
        return instance->m_Skin;
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
                MeshProperties* properties = &instance->m_MeshProperties[mesh_index];
                properties->m_Color[0] = color[0];
                properties->m_Color[1] = color[1];
                properties->m_Color[2] = color[2];
                properties->m_Color[3] = color[3];
                properties->m_Order = mesh->m_DrawOrder;
                properties->m_Visible = mesh->m_Visible;
            }
            instance->m_DoRender = 1;
        } else {
            instance->m_MeshProperties.SetSize(0);
        }
    }

    Result SetSkin(HRigInstance instance, dmhash_t skin)
    {
        const dmRigDDF::MeshEntry* mesh_entry = 0x0;
        const dmRigDDF::MeshSet* mesh_set = instance->m_MeshSet;
        for (uint32_t i = 0; i < mesh_set->m_MeshEntries.m_Count; ++i)
        {
            if (skin == mesh_set->m_MeshEntries[i].m_Id)
            {
                mesh_entry = &mesh_set->m_MeshEntries[i];
                break;
            }
        }
        if (mesh_entry == 0x0)
        {
            return dmRig::RESULT_FAILED;
        }
        instance->m_MeshEntry = mesh_entry;
        instance->m_Skin = skin;

        UpdateMeshProperties(instance);

        return dmRig::RESULT_OK;
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
        if (player->m_Playback == dmGameObject::PLAYBACK_ONCE_PINGPONG)
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
                    RigEventData event_data;
                    event_data.m_Type = RIG_EVENT_TYPE_SPINE;
                    event_data.m_DataSpine.m_EventId = track->m_EventId;
                    event_data.m_DataSpine.m_AnimationId = animation->m_Id;
                    event_data.m_DataSpine.m_BlendWeight = blend_weight;
                    event_data.m_DataSpine.m_T = key->m_T;
                    event_data.m_DataSpine.m_Integer = key->m_Integer;
                    event_data.m_DataSpine.m_Float = key->m_Float;
                    event_data.m_DataSpine.m_String = key->m_String;

                    instance->m_EventCallback(instance->m_EventCBUserData, event_data);
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
            if (player->m_Playback == dmGameObject::PLAYBACK_LOOP_PINGPONG)
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
            if (player->m_Playback == dmGameObject::PLAYBACK_ONCE_PINGPONG && cursor > half_duration)
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
            return;

        // Advance cursor
        float prev_cursor = player->m_Cursor;
        if (player->m_Playback != dmGameObject::PLAYBACK_NONE)
        {
            player->m_Cursor += dt;
        }
        float duration = GetCursorDuration(player, animation);
        if (duration == 0.0f) {
            player->m_Cursor = 0;
        }

        // Adjust cursor
        bool completed = false;
        switch (player->m_Playback)
        {
        case dmGameObject::PLAYBACK_ONCE_FORWARD:
        case dmGameObject::PLAYBACK_ONCE_BACKWARD:
        case dmGameObject::PLAYBACK_ONCE_PINGPONG:
            if (player->m_Cursor >= duration)
            {
                player->m_Cursor = duration;
                completed = true;
            }
            break;
        case dmGameObject::PLAYBACK_LOOP_FORWARD:
        case dmGameObject::PLAYBACK_LOOP_BACKWARD:
            while (player->m_Cursor >= duration && duration > 0.0f)
            {
                player->m_Cursor -= duration;
            }
            break;
        case dmGameObject::PLAYBACK_LOOP_PINGPONG:
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
                RigEventData event_data;
                event_data.m_Type = RIG_EVENT_TYPE_DONE;
                event_data.m_DataDone.m_AnimationId = player->m_AnimationId;
                event_data.m_DataDone.m_Playback = player->m_Playback;

                instance->m_EventCallback(instance->m_EventCBUserData, event_data);
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


    static void ApplyAnimation(RigPlayer* player, dmArray<dmTransform::Transform>& pose, dmArray<IKAnimation>& ik_animation, dmArray<MeshProperties>& properties, float blend_weight, dmhash_t skin_id, bool draw_order)
    {
        const dmRigDDF::RigAnimation* animation = player->m_Animation;
        if (animation == 0x0)
            return;
        float duration = GetCursorDuration(player, animation);
        float t = CursorToTime(player->m_Cursor, duration, player->m_Backwards, player->m_Playback == dmGameObject::PLAYBACK_ONCE_PINGPONG);

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
            dmTransform::Transform& transform = pose[bone_index];
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
            if (skin_id == track->m_SkinId) {
                MeshProperties& props = properties[track->m_MeshIndex];
                if (track->m_Colors.m_Count > 0) {
                    Vector4 color(props.m_Color[0], props.m_Color[1], props.m_Color[2], props.m_Color[3]);
                    color = lerp(blend_weight, color, SampleVec4(sample, fraction, track->m_Colors.m_Data));
                    props.m_Color[0] = color[0];
                    props.m_Color[1] = color[1];
                    props.m_Color[2] = color[2];
                    props.m_Color[3] = color[3];
                }
                if (track->m_Visible.m_Count > 0) {
                    if (blend_weight >= 0.5f) {
                        props.m_Visible = track->m_Visible[rounded_sample];
                    }
                }
                if (track->m_OrderOffset.m_Count > 0 && draw_order) {
                    props.m_Order += track->m_OrderOffset[rounded_sample];
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
                    bool draw_order = true;
                    if (player == p) {
                        draw_order = fade_rate >= 0.5f;
                    } else {
                        draw_order = fade_rate < 0.5f;
                    }
                    ApplyAnimation(p, pose, ik_animation, properties, alpha, instance->m_Skin, draw_order);
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
                ApplyAnimation(player, pose, ik_animation, properties, 1.0f, instance->m_Skin, true);
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
            const dmRigDDF::Skeleton* skeleton = instance->m_Skeleton;
            dmArray<dmTransform::Transform>& pose = instance->m_Pose;
            if (pose.Empty())
                continue;

            // Notify any listener that the pose has been recalculated
            if (instance->m_PoseCallback) {
                instance->m_PoseCallback(instance->m_EventCBUserData);
            }

            // Convert every transform into model space
            for (uint32_t bi = 0; bi < pose.Size(); ++bi)
            {
                dmTransform::Transform& transform = pose[bi];
                if (bi > 0) {
                    const dmRigDDF::Bone* bone = &skeleton->m_Bones[bi];
                    if (bone->m_InheritScale)
                    {
                        transform = dmTransform::Mul(pose[bone->m_Parent], transform);
                    }
                    else
                    {
                        Vector3 scale = transform.GetScale();
                        transform = dmTransform::Mul(pose[bone->m_Parent], transform);
                        transform.SetScale(scale);
                    }
                }
            }
        }

        return dmRig::RESULT_OK;
    }

    Result Update(HRigContext context, float dt)
    {
        dmArray<RigInstance*>& instances = context->m_Instances.m_Objects;
        const uint32_t count = instances.Size();

        for (uint32_t i = 0; i < count; ++i)
        {
            UpdateMeshProperties(instances[i]);
        }

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

    static const dmRigDDF::MeshEntry* FindMeshEntry(const dmRigDDF::MeshSet* mesh_set, dmhash_t skin_id)
    {
        for (uint32_t i = 0; i < mesh_set->m_MeshEntries.m_Count; ++i)
        {
            const dmRigDDF::MeshEntry* mesh_entry = &mesh_set->m_MeshEntries[i];
            if (mesh_entry->m_Id == skin_id)
            {
                return mesh_entry;
            }
        }
        return 0x0;
    }

    static dmRig::Result CreatePose(HRigContext context, HRigInstance instance)
    {
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
        if (!player) {
            return 0.0f;
        }

        float duration = GetCursorDuration(player, player->m_Animation);
        if (duration == 0.0f) {
            return 0.0f;
        }

        float t = CursorToTime(player->m_Cursor, duration, player->m_Backwards, player->m_Playback == dmGameObject::PLAYBACK_ONCE_PINGPONG);
        if (normalized) {
            t = t / duration;
        }
        return t;

    }

    Result SetCursor(HRigInstance instance, float cursor, bool normalized)
    {
        float t = cursor;

        RigPlayer* player = GetPlayer(instance);
        if (!player) {
            return dmRig::RESULT_FAILED;
        }

        float duration = GetCursorDuration(player, player->m_Animation);
        if (normalized) {
            t = t * duration;
        }

        t = fmod(t, duration);
        if (cursor < 0) {
            t = duration + t;
        }

        player->m_Cursor = t;

        return dmRig::RESULT_OK;
    }

#define TO_BYTE(val) (uint8_t)(val * 255.0f)
#define TO_SHORT(val) (uint16_t)((val) * 65535.0f)

    static void UpdateMeshDrawOrder(RigContext* context, const RigInstance* instance, uint32_t mesh_count) {
        // Spine's approach to update draw order is to:
        // * Initialize with default draw order (integer sequence)
        // * Add entries with changed draw order
        // * Fill untouched slots with the unchanged entries
        // E.g.:
        // Init: [0, 1, 2]
        // Changed: 1 => 0, results in [1, 1, 2]
        // Unchanged: 0 => 0, 2 => 2, results in [1, 0, 2] (indices 1 and 2 were untouched and filled)
        context->m_DrawOrderToMesh.SetSize(mesh_count);
        // Intialize
        for (uint32_t i = 0; i < mesh_count; ++i) {
            context->m_DrawOrderToMesh[i] = i;
        }
        // Update changed
        for (uint32_t i = 0; i < mesh_count; ++i) {
            uint32_t order = instance->m_MeshProperties[i].m_Order;
            if (order != i) {
                context->m_DrawOrderToMesh[order] = i;
            }
        }
        // Fill with unchanged
        uint32_t draw_order = 0;
        for (uint32_t i = 0; i < mesh_count; ++i) {
            uint32_t order = instance->m_MeshProperties[i].m_Order;
            if (order == i) {
                // Find free slot
                while (context->m_DrawOrderToMesh[draw_order] != draw_order) {
                    ++draw_order;
                }
                context->m_DrawOrderToMesh[draw_order] = i;
                ++draw_order;
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

    RigVertexData* GenerateVertexData(HRigContext context, HRigInstance instance, const RigGenVertexDataParams& params)
    {
        DM_PROFILE(Rig, "GenerateVertexData");

        RigVertexData* write_ptr = *(dmRig::RigVertexData **)params.m_VertexData;
        const Matrix4& model_matrix = params.m_ModelMatrix;
        const dmArray<RigBone>& bind_pose = *instance->m_BindPose;
        const dmRigDDF::MeshEntry* mesh_entry = instance->m_MeshEntry;
        if (!instance->m_MeshEntry || !instance->m_DoRender) {
            return write_ptr;
        }
        uint32_t mesh_count = mesh_entry->m_Meshes.m_Count;

        if (context->m_DrawOrderToMesh.Capacity() < mesh_count)
            context->m_DrawOrderToMesh.SetCapacity(mesh_count);

        UpdateMeshDrawOrder(context, instance, mesh_count);
        for (uint32_t draw_index = 0; draw_index < mesh_count; ++draw_index) {
            uint32_t mesh_index = context->m_DrawOrderToMesh[draw_index];
            const MeshProperties* properties = &instance->m_MeshProperties[mesh_index];
            const dmRigDDF::Mesh* mesh = &mesh_entry->m_Meshes[mesh_index];
            if (!properties->m_Visible) {
                continue;
            }
            uint32_t index_count = mesh->m_Indices.m_Count;
            for (uint32_t ii = 0; ii < index_count; ++ii)
            {
                uint32_t vi = mesh->m_Indices[ii];
                uint32_t e = vi*3;
                Point3 in_p(mesh->m_Positions[e+0], mesh->m_Positions[e+1], mesh->m_Positions[e+2]);
                Point3 out_p(0.0f, 0.0f, 0.0f);
                uint32_t bi_offset = vi * 4;
                const uint32_t* bone_indices = &mesh->m_BoneIndices[bi_offset];
                const float* bone_weights = &mesh->m_Weights[bi_offset];
                for (uint32_t bi = 0; bi < 4; ++bi)
                {
                    if (bone_weights[bi] > 0.0f)
                    {
                        uint32_t bone_index = bone_indices[bi];
                        out_p += Vector3(dmTransform::Apply(instance->m_Pose[bone_index], dmTransform::Apply(bind_pose[bone_index].m_ModelToLocal, in_p))) * bone_weights[bi];
                    }
                }
                Vector4 posed_vertex = model_matrix * out_p;
                write_ptr->x = posed_vertex[0];
                write_ptr->y = posed_vertex[1];
                write_ptr->z = posed_vertex[2];
                e = vi*2;
                write_ptr->u = TO_SHORT(mesh->m_Texcoord0[e+0]);
                write_ptr->v = TO_SHORT(mesh->m_Texcoord0[e+1]);
                write_ptr->r = TO_BYTE(properties->m_Color[0]);
                write_ptr->g = TO_BYTE(properties->m_Color[1]);
                write_ptr->b = TO_BYTE(properties->m_Color[2]);
                write_ptr->a = TO_BYTE(properties->m_Color[3]);
                write_ptr = (dmRig::RigVertexData*)((uintptr_t)write_ptr + params.m_VertexStride);
            }
        }

        return write_ptr;
    }


#undef TO_BYTE
#undef TO_SHORT

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

    IKTarget* GetIKTarget(HRigInstance instance, dmhash_t constraint_id)
    {
        if (!instance) {
            return 0x0;
        }
        uint32_t ik_index = FindIKIndex(instance, constraint_id);
        if (ik_index == ~0u) {
            dmLogError("Could not find IK constraint (%llu)", constraint_id);
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
            dmLogError("Rig Instance could not be created since the buffer is full (%d), consider increasing %s.", context->m_Instances.Capacity(), MAX_RIG_INSTANCE_COUNT_KEY);
            return dmRig::RESULT_ERROR;
        }

        *params.m_Instance = new RigInstance;
        RigInstance* instance = *params.m_Instance;

        uint32_t index = context->m_Instances.Alloc();
        memset(instance, 0, sizeof(RigInstance));
        instance->m_Index = index;
        context->m_Instances.Set(index, instance);
        instance->m_Skin = params.m_Skin;

        instance->m_PoseCallback = params.m_PoseCallback;
        instance->m_EventCallback = params.m_EventCallback;
        instance->m_EventCBUserData = params.m_EventCBUserData;

        instance->m_BindPose = params.m_BindPose;
        instance->m_Skeleton = params.m_Skeleton;
        instance->m_MeshSet = params.m_MeshSet;
        instance->m_AnimationSet = params.m_AnimationSet;
        instance->m_Enabled = 1;

        AllocateMeshProperties(params.m_MeshSet, instance->m_MeshProperties);
        instance->m_MeshEntry = FindMeshEntry(params.m_MeshSet, instance->m_Skin);

        Result result = CreatePose(context, instance);
        if (result != dmRig::RESULT_OK) {
            DestroyInstance(context, index);
            return result;
        }

        if (params.m_DefaultAnimation != NULL_ANIMATION)
        {
            // Loop forward should be the most common for idle anims etc.
            (void)PlayAnimation(instance, params.m_DefaultAnimation, dmGameObject::PLAYBACK_LOOP_FORWARD, 0.0f);
        }

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

}
