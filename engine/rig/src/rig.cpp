#include "rig.h"

#include <dlib/log.h>
#include <dlib/profile.h>

namespace dmRig
{

    static const dmhash_t NULL_ANIMATION = dmHashString64("");

    /// Config key to use for tweaking the total maximum number of rig instances in a context.
    const char* MAX_RIG_INSTANCE_COUNT_KEY = "rig.max_instance_count";

    CreateResult NewContext(const NewContextParams& params)
    {
        RigContext* context = new RigContext();
        if (!context) {
            return dmRig::CREATE_RESULT_ERROR;
        }

        context->m_Instances.SetCapacity(params.m_MaxRigInstanceCount);

        *params.m_Context = context;
        return dmRig::CREATE_RESULT_OK;
    }

    CreateResult DeleteContext(const DeleteContextParams& params)
    {
        RigContext* world = (RigContext*)params.m_Context;
        delete world;
        return dmRig::CREATE_RESULT_OK;
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

    PlayResult PlayAnimation(HRigInstance instance, dmhash_t animation_id, dmGameObject::Playback playback, float blend_duration)
    {
        const dmRigDDF::RigAnimation* anim = FindAnimation(instance->m_AnimationSet, animation_id);
        if (anim != 0x0)
        {
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
            return PLAY_RESULT_OK;
        }
        return PLAY_RESULT_ANIM_NOT_FOUND;
    }

    PlayResult CancelAnimation(HRigInstance instance)
    {
        RigPlayer* player = GetPlayer(instance);
        player->m_Playing = 0;

        return PLAY_RESULT_OK;
    }

    dmhash_t GetAnimation(HRigInstance instance)
    {
        RigPlayer* player = GetPlayer(instance);
        return player->m_AnimationId;
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
        float duration = animation->m_Duration;
        if (player->m_Playback == dmGameObject::PLAYBACK_ONCE_PINGPONG)
        {
            duration *= 2.0f;
        }
        return duration;
    }

    static void UpdatePlayer(RigInstance* instance, RigPlayer* player, float dt/*, dmMessage::URL* listener*/, float blend_weight)
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

        // dmMessage::URL sender;
        // if (prev_cursor != player->m_Cursor && GetSender(component, &sender))
        // {
        //     dmMessage::URL receiver = *listener;
        //     receiver.m_Function = 0;
        //     PostEvents(player, &sender, &receiver, animation, dt, prev_cursor, duration, completed, blend_weight);
        // }

        // if (completed)
        // {
        //     player->m_Playing = 0;
        //     // Only report completeness for the primary player
        //     if (player == GetPlayer(component) && dmMessage::IsSocketValid(listener->m_Socket))
        //     {
        //         dmMessage::URL sender;
        //         if (GetSender(component, &sender))
        //         {
        //             dmhash_t message_id = dmRigDDF::SpineAnimationDone::m_DDFDescriptor->m_NameHash;
        //             dmRigDDF::SpineAnimationDone message;
        //             message.m_AnimationId = player->m_AnimationId;
        //             message.m_Playback = player->m_Playback;

        //             dmMessage::URL receiver = *listener;
        //             uintptr_t descriptor = (uintptr_t)dmRigDDF::SpineAnimationDone::m_DDFDescriptor;
        //             uint32_t data_size = sizeof(dmRigDDF::SpineAnimationDone);
        //             dmMessage::Result result = dmMessage::Post(&sender, &receiver, message_id, 0, descriptor, &message, data_size);
        //             dmMessage::ResetURL(*listener);
        //             if (result != dmMessage::RESULT_OK)
        //             {
        //                 dmLogError("Could not send animation_done to listener.");
        //             }
        //         }
        //         else
        //         {
        //             dmLogError("Could not send animation_done to listener because of incomplete component.");
        //         }
        //     }
        // }
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

    static void ApplyAnimation(RigPlayer* player, dmArray<dmTransform::Transform>& pose/*, dmArray<IKAnimation>& ik_animation*/, dmArray<MeshProperties>& properties, float blend_weight, dmhash_t skin_id, bool draw_order)
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

        // track_count = animation->m_IkTracks.m_Count;
        // for (uint32_t ti = 0; ti < track_count; ++ti)
        // {
        //     dmRigDDF::IKAnimationTrack* track = &animation->m_IkTracks[ti];
        //     uint32_t ik_index = track->m_IkIndex;
        //     IKAnimation& anim = ik_animation[ik_index];
        //     if (track->m_Mix.m_Count > 0)
        //     {
        //         anim.m_Mix = dmMath::LinearBezier(blend_weight, anim.m_Mix, dmMath::LinearBezier(fraction, track->m_Mix.m_Data[sample], track->m_Mix.m_Data[sample+1]));
        //     }
        //     if (track->m_Positive.m_Count > 0)
        //     {
        //         if (blend_weight >= 0.5f)
        //         {
        //             anim.m_Positive = track->m_Positive[sample];
        //         }
        //     }
        // }

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
            // NOTE we previously checked for (!instance->m_Enabled || !instance->m_AddedToUpdatehere) also
            if (instance->m_Pose.Empty())
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
            // dmArray<IKAnimation>& ik_animation = instance->m_IKAnimation;
            // uint32_t ik_animation_count = ik_animation.Size();
            // for (uint32_t ii = 0; ii < ik_animation_count; ++ii)
            // {
            //     dmRigDDF::IK* ik = &skeleton->m_Iks[ii];
            //     ik_animation[ii].m_Mix = ik->m_Mix;
            //     ik_animation[ii].m_Positive = ik->m_Positive;
            // }

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
                    UpdatePlayer(instance, p, dt/*, &instance->m_Listener*/, blend_weight);
                    bool draw_order = true;
                    if (player == p) {
                        draw_order = fade_rate >= 0.5f;
                    } else {
                        draw_order = fade_rate < 0.5f;
                    }
                    ApplyAnimation(p, pose/*, ik_animation*/, properties, alpha, instance->m_Skin, draw_order);
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
                UpdatePlayer(instance, player, dt/*, &instance->m_Listener*/, 1.0f);
                ApplyAnimation(player, pose/*, ik_animation*/, properties, 1.0f, instance->m_Skin, true);
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

            // if (skeleton->m_Iks.m_Count > 0) {
            //     DM_PROFILE(Rig, "IK");
            //     const uint32_t count = skeleton->m_Iks.m_Count;
            //     dmArray<IKTarget>& ik_targets = instance->m_IKTargets;


            //     for (int32_t i = 0; i < count; ++i) {
            //         dmRigDDF::IK* ik = &skeleton->m_Iks[i];

            //         // transform local space hiearchy for pose
            //         dmTransform::Transform parent_t = GetPoseTransform(bind_pose, pose, pose[ik->m_Parent], ik->m_Parent);
            //         dmTransform::Transform parent_t_local = parent_t;
            //         dmTransform::Transform target_t = GetPoseTransform(bind_pose, pose, pose[ik->m_Target], ik->m_Target);
            //         const uint32_t parent_parent_index = skeleton->m_Bones[ik->m_Parent].m_Parent;
            //         dmTransform::Transform parent_parent_t;
            //         if(parent_parent_index != INVALID_BONE_INDEX)
            //         {
            //             parent_parent_t = dmTransform::Inv(GetPoseTransform(bind_pose, pose, pose[skeleton->m_Bones[ik->m_Parent].m_Parent], skeleton->m_Bones[ik->m_Parent].m_Parent));
            //             parent_t = dmTransform::Mul(parent_parent_t, parent_t);
            //             target_t = dmTransform::Mul(parent_parent_t, target_t);
            //         }
            //         Vector3 parent_position = parent_t.GetTranslation();
            //         Vector3 target_position = target_t.GetTranslation();

            //         const float target_mix = ik_targets[i].m_Mix;
            //         if(target_mix != 0.0f)
            //         {
            //             // get custom target position either from go or vector position
            //             Vector3 user_target_position = target_position;
            //             const dmhash_t target_instance_id = ik_targets[i].m_InstanceId;
            //             if(target_instance_id == 0)
            //             {
            //                 user_target_position = ik_targets[i].m_Position;
            //             }
            //             else
            //             {
            //                 dmGameObject::HInstance target_instance = dmGameObject::GetInstanceFromIdentifier(dmGameObject::GetCollection(instance->m_Instance), target_instance_id);
            //                 if(target_instance == 0x0)
            //                 {
            //                     // instance have been removed, disable animation
            //                     ik_targets[i].m_InstanceId = 0;
            //                     ik_targets[i].m_Mix = 0.0f;
            //                 }
            //                 else
            //                 {
            //                     user_target_position = (Vector3)dmGameObject::GetWorldPosition(target_instance);
            //                 }
            //             }

            //             // transform local pose of parent bone into worldspace, since we can only rely on worldspace coordinates for target
            //             dmTransform::Transform t = parent_t_local;
            //             t = dmTransform::Mul((dmTransform::Mul(dmGameObject::GetWorldTransform(instance->m_Instance), instance->m_Transform)), t);
            //             user_target_position -=  t.GetTranslation();
            //             Quat rotation = dmTransform::conj(dmGameObject::GetWorldRotation(instance->m_Instance) * instance->m_Transform.GetRotation());
            //             user_target_position = dmTransform::mulPerElem(dmTransform::rotate(rotation, user_target_position), dmGameObject::GetWorldScale(instance->m_Instance));
            //             if(parent_parent_index != INVALID_BONE_INDEX)
            //                 user_target_position =  dmTransform::Apply(parent_parent_t, user_target_position);

            //             // blend default target pose and target pose
            //             target_position = target_mix == 1.0f ? user_target_position : dmTransform::lerp(target_mix, target_position, user_target_position);
            //         }

            //         if(ik->m_Child == ik->m_Parent)
            //             ApplyOneBoneIKConstraint(ik, bind_pose, pose, target_position, parent_position, ik_animation[i].m_Mix);
            //         else
            //             ApplyTwoBoneIKConstraint(ik, bind_pose, pose, target_position, parent_position, ik_animation[i].m_Positive, ik_animation[i].m_Mix);
            //     }
            // }

            // Include instance transform in the GO instance reflecting the root bone
            // dmTransform::Transform root_t = pose[0];
            // pose[0] = dmTransform::Mul(instance->m_Transform, root_t);
            // dmGameObject::HInstance first_bone_go = instance->m_NodeInstances[0];
            // dmGameObject::SetBoneTransforms(first_bone_go, pose.Begin(), pose.Size());
            // pose[0] = root_t;
            // for (uint32_t bi = 0; bi < bone_count; ++bi)
            // {
            //     dmTransform::Transform& transform = pose[bi];
            //     // Convert every transform into model space
            //     if (bi > 0) {
            //         dmRigDDF::Bone* bone = &skeleton->m_Bones[bi];
            //         if (bone->m_InheritScale)
            //         {
            //             transform = dmTransform::Mul(pose[bone->m_Parent], transform);
            //         }
            //         else
            //         {
            //             Vector3 scale = transform.GetScale();
            //             transform = dmTransform::Mul(pose[bone->m_Parent], transform);
            //             transform.SetScale(scale);
            //         }
            //     }
            // }
        }
    }

    UpdateResult Update(HRigContext context, float dt)
    {
        dmArray<RigInstance*>& instances = context->m_Instances.m_Objects;
        const uint32_t count = instances.Size();

        for (uint32_t i = 0; i < count; ++i)
        {
            RigInstance& instance = *instances[i];
            if (instance.m_MeshEntry != 0x0) {
                uint32_t mesh_count = instance.m_MeshEntry->m_Meshes.m_Count;
                instance.m_MeshProperties.SetSize(mesh_count);
                for (uint32_t mesh_index = 0; mesh_index < mesh_count; ++mesh_index) {
                    const dmRigDDF::Mesh* mesh = &instance.m_MeshEntry->m_Meshes[mesh_index];
                    float* color = mesh->m_Color.m_Data;
                    MeshProperties* properties = &instance.m_MeshProperties[mesh_index];
                    properties->m_Color[0] = color[0];
                    properties->m_Color[1] = color[1];
                    properties->m_Color[2] = color[2];
                    properties->m_Color[3] = color[3];
                    properties->m_Order = mesh->m_DrawOrder;
                    properties->m_Visible = mesh->m_Visible;
                }
                // instance.m_DoRender = 1;
            } else {
                instance.m_MeshProperties.SetSize(0);
            }
        }

        Animate(context, dt);

        return dmRig::UPDATE_RESULT_OK;
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

    static dmRig::CreateResult CreatePose(HRigContext context, HRigInstance instance)
    {
        const dmArray<RigBone>* bind_pose = instance->m_BindPose;
        const dmRigDDF::Skeleton* skeleton = instance->m_Skeleton;
        uint32_t bone_count = skeleton->m_Bones.m_Count;
        instance->m_Pose.SetCapacity(bone_count);
        instance->m_Pose.SetSize(bone_count);
        for (uint32_t i = 0; i < bone_count; ++i)
        {
            instance->m_Pose[i].SetIdentity();
        }
    //     component->m_NodeInstances.SetCapacity(bone_count);
    //     component->m_NodeInstances.SetSize(bone_count);
    //     if (bone_count > world->m_ScratchInstances.Capacity()) {
    //         world->m_ScratchInstances.SetCapacity(bone_count);
    //     }
    //     world->m_ScratchInstances.SetSize(0);
    //     for (uint32_t i = 0; i < bone_count; ++i)
    //     {
    //         dmGameObject::HInstance inst = dmGameObject::New(collection, 0x0);
    //         if (inst == 0x0) {
    //             component->m_NodeInstances.SetSize(i);
    //             return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
    //         }

    //         dmhash_t id = dmGameObject::GenerateUniqueInstanceId(collection);
    //         dmGameObject::Result result = dmGameObject::SetIdentifier(collection, inst, id);
    //         if (dmGameObject::RESULT_OK != result) {
    //             dmGameObject::Delete(collection, inst);
    //             component->m_NodeInstances.SetSize(i);
    //             return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
    //         }

    //         dmGameObject::SetBone(inst, true);
    //         dmTransform::Transform transform = bind_pose[i].m_LocalToParent;
    //         if (i == 0)
    //         {
    //             transform = dmTransform::Mul(component->m_Transform, transform);
    //         }
    //         dmGameObject::SetPosition(inst, Point3(transform.GetTranslation()));
    //         dmGameObject::SetRotation(inst, transform.GetRotation());
    //         dmGameObject::SetScale(inst, transform.GetScale());
    //         component->m_NodeInstances[i] = inst;
    //         world->m_ScratchInstances.Push(inst);
    //     }
    //     // Set parents in reverse to account for child-prepending
    //     for (uint32_t i = 0; i < bone_count; ++i)
    //     {
    //         uint32_t index = bone_count - 1 - i;
    //         dmGameObject::HInstance inst = world->m_ScratchInstances[index];
    //         dmGameObject::HInstance parent = instance;
    //         if (index > 0)
    //         {
    //             parent = world->m_ScratchInstances[skeleton->m_Bones[index].m_Parent];
    //         }
    //         dmGameObject::SetParent(inst, parent);
    //     }

    //     component->m_IKTargets.SetCapacity(skeleton->m_Iks.m_Count);
    //     component->m_IKTargets.SetSize(skeleton->m_Iks.m_Count);
    //     memset(component->m_IKTargets.Begin(), 0x0, component->m_IKTargets.Size()*sizeof(IKTarget));

    //     component->m_IKAnimation.SetCapacity(skeleton->m_Iks.m_Count);
    //     component->m_IKAnimation.SetSize(skeleton->m_Iks.m_Count);

        return dmRig::CREATE_RESULT_OK;
    }

    static void DestroyPose(HRigInstance instance)
    {
        // Delete bone game objects
        // dmGameObject::DeleteBones(component->m_Instance);
    }

    const dmArray<dmTransform::Transform>& GetPose(HRigInstance instance)
    {
        return instance->m_Pose;
    }

    static void DestroyInstance(HRigContext context, uint32_t index)
    {
        RigInstance* instance = context->m_Instances.Get(index);
        DestroyPose(instance);
        // If we're going to use memset, then we should explicitly clear pose and instance arrays.
        // component->m_Pose.SetCapacity(0);
        // component->m_NodeInstances.SetCapacity(0);
        // component->m_IKTargets.SetCapacity(0);
        // component->m_MeshProperties.SetCapacity(0);
        delete instance;
        context->m_Instances.Free(index, true);
    }

    CreateResult InstanceCreate(const InstanceCreateParams& params)
    {
        RigContext* context = (RigContext*)params.m_Context;

        if (context->m_Instances.Full())
        {
            dmLogError("Rig Instance could not be created since the buffer is full (%d).", context->m_Instances.Capacity());
            return dmRig::CREATE_RESULT_ERROR;
        }

        // RigInstance* instance = new RigInstance;
        *params.m_Instance = new RigInstance;
        RigInstance* instance = *params.m_Instance;

        uint32_t index = context->m_Instances.Alloc();
        instance->m_Index = index;
        memset(instance, 0, sizeof(RigInstance));
        context->m_Instances.Set(index, instance);
        // dmMessage::ResetURL(component->m_Listener);
        instance->m_Skin = dmHashString64(params.m_SkinId);

        instance->m_BindPose = params.m_BindPose;
        instance->m_Skeleton = params.m_Skeleton;
        instance->m_MeshSet = params.m_MeshSet;
        instance->m_AnimationSet = params.m_AnimationSet;

        AllocateMeshProperties(params.m_MeshSet, instance->m_MeshProperties);
        instance->m_MeshEntry = FindMeshEntry(params.m_MeshSet, instance->m_Skin);

        CreateResult result = CreatePose(context, instance);
        if (result != CREATE_RESULT_OK) {
            DestroyInstance(context, index);
            return result;
        }

        dmhash_t default_animation_id = dmHashString64(params.m_DefaultAnimation);
        if (default_animation_id != NULL_ANIMATION)
        {
            // Loop forward should be the most common for idle anims etc.
            PlayResult r = PlayAnimation(instance, default_animation_id, dmGameObject::PLAYBACK_LOOP_FORWARD, 0.0f);
            if (r == PLAY_RESULT_ANIM_NOT_FOUND) {
                dmLogError("Could not find (and play) default rig animation: %s", params.m_DefaultAnimation);
            }
        }

        // *params.m_Index = (uintptr_t)index;

        return dmRig::CREATE_RESULT_OK;
    }

    CreateResult InstanceDestroy(const InstanceDestroyParams& params)
    {
        RigContext* context = (RigContext*)params.m_Context;
        RigInstance* instance = params.m_Instance;
        // RigInstance* instance = context->m_Instances.Get(params.m_Index);
        // DestroyPose(instance);
        // If we're going to use memset, then we should explicitly clear pose and instance arrays.
        // instance->m_Pose.SetCapacity(0);
        // instance->m_NodeInstances.SetCapacity(0);
        // instance->m_IKTargets.SetCapacity(0);
        instance->m_MeshProperties.SetCapacity(0);
        context->m_Instances.Free(instance->m_Index, true);
        delete instance;

        return dmRig::CREATE_RESULT_OK;
    }

}
