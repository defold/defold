#include "comp_spine_model.h"

#include <string.h>
#include <float.h>
#include <algorithm>

#include <dlib/array.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/message.h>
#include <dlib/profile.h>
#include <dlib/dstrings.h>
#include <dlib/object_pool.h>
#include <dlib/math.h>
#include <dlib/vmath.h>
#include <graphics/graphics.h>
#include <render/render.h>
#include <gameobject/gameobject_ddf.h>

#include "../gamesys.h"
#include "../gamesys_private.h"

#include "spine_ddf.h"
#include "sprite_ddf.h"
#include "tile_ddf.h"

using namespace Vectormath::Aos;

namespace dmGameSystem
{
    using namespace Vectormath::Aos;
    using namespace dmGameSystemDDF;

    // static const dmhash_t PROP_SKIN = dmHashString64("skin");
    // static const dmhash_t PROP_ANIMATION = dmHashString64("animation");

    // static const uint32_t INVALID_BONE_INDEX = 0xffff;

    static void ResourceReloadedCallback(const dmResource::ResourceReloadedParams& params);
    static void DestroyComponent(SpineModelWorld* world, uint32_t index);

    dmGameObject::CreateResult CompSpineModelNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        SpineModelContext* context = (SpineModelContext*)params.m_Context;
        dmRender::HRenderContext render_context = context->m_RenderContext;
        SpineModelWorld* world = new SpineModelWorld();

        world->m_Components.SetCapacity(context->m_MaxSpineModelCount);
        world->m_RigContext = context->m_RigContext;
        world->m_RenderObjects.SetCapacity(context->m_MaxSpineModelCount);

        dmGraphics::VertexElement ve[] =
        {
                {"position", 0, 3, dmGraphics::TYPE_FLOAT, false},
                {"texcoord0", 1, 2, dmGraphics::TYPE_UNSIGNED_SHORT, true},
                {"color", 2, 4, dmGraphics::TYPE_UNSIGNED_BYTE, true},
        };

        world->m_VertexDeclaration = dmGraphics::NewVertexDeclaration(dmRender::GetGraphicsContext(render_context), ve, sizeof(ve) / sizeof(dmGraphics::VertexElement));
        world->m_VertexBuffer = dmGraphics::NewVertexBuffer(dmRender::GetGraphicsContext(render_context), 0, 0x0, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);

        // Assume 4 vertices per mesh
        world->m_VertexBufferData.SetCapacity(4 * world->m_Components.Capacity());

        *params.m_World = world;

        dmResource::RegisterResourceReloadedCallback(context->m_Factory, ResourceReloadedCallback, world);

        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompSpineModelDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        SpineModelWorld* world = (SpineModelWorld*)params.m_World;
        dmGraphics::DeleteVertexDeclaration(world->m_VertexDeclaration);
        dmGraphics::DeleteVertexBuffer(world->m_VertexBuffer);
        // world->m_ScratchInstances.SetCapacity(0);

        dmResource::UnregisterResourceReloadedCallback(((SpineModelContext*)params.m_Context)->m_Factory, ResourceReloadedCallback, world);

        delete world;

        return dmGameObject::CREATE_RESULT_OK;
    }

    // static bool GetSender(SpineModelComponent* component, dmMessage::URL* out_sender)
    // {
    //     dmMessage::URL sender;
    //     sender.m_Socket = dmGameObject::GetMessageSocket(dmGameObject::GetCollection(component->m_Instance));
    //     if (dmMessage::IsSocketValid(sender.m_Socket))
    //     {
    //         dmGameObject::Result go_result = dmGameObject::GetComponentId(component->m_Instance, component->m_ComponentIndex, &sender.m_Fragment);
    //         if (go_result == dmGameObject::RESULT_OK)
    //         {
    //             sender.m_Path = dmGameObject::GetIdentifier(component->m_Instance);
    //             *out_sender = sender;
    //             return true;
    //         }
    //     }
    //     return false;
    // }

    static void ReHash(SpineModelComponent* component)
    {
        // Hash resource-ptr, material-handle, blend mode and render constants
        HashState32 state;
        bool reverse = false;
        SpineModelResource* resource = component->m_Resource;
        dmGameSystemDDF::SpineModelDesc* ddf = resource->m_Model;
        dmHashInit32(&state, reverse);
        dmHashUpdateBuffer32(&state, &resource->m_RigScene->m_TextureSet, sizeof(resource->m_RigScene->m_TextureSet));
        dmHashUpdateBuffer32(&state, &resource->m_Material, sizeof(resource->m_Material));
        dmHashUpdateBuffer32(&state, &ddf->m_BlendMode, sizeof(ddf->m_BlendMode));
        dmArray<dmRender::Constant>& constants = component->m_RenderConstants;
        uint32_t size = constants.Size();
        // Padding in the SetConstant-struct forces us to copy the components by hand
        for (uint32_t i = 0; i < size; ++i)
        {
            dmRender::Constant& c = constants[i];
            dmHashUpdateBuffer32(&state, &c.m_NameHash, sizeof(uint64_t));
            dmHashUpdateBuffer32(&state, &c.m_Value, sizeof(Vector4));
            component->m_PrevRenderConstants[i] = c.m_Value;
        }
        component->m_MixedHash = dmHashFinal32(&state);
    }

    dmGameObject::CreateResult CompSpineModelCreate(const dmGameObject::ComponentCreateParams& params)
    {
        SpineModelWorld* world = (SpineModelWorld*)params.m_World;

        if (world->m_Components.Full())
        {
            dmLogError("Spine Model could not be created since the buffer is full (%d).", world->m_Components.Capacity());
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
        uint32_t index = world->m_Components.Alloc();
        SpineModelComponent* component = new SpineModelComponent;
        memset(component, 0, sizeof(SpineModelComponent));
        world->m_Components.Set(index, component);
        component->m_Instance = params.m_Instance;
        component->m_Transform = dmTransform::Transform(Vector3(params.m_Position), params.m_Rotation, 1.0f);
        component->m_Resource = (SpineModelResource*)params.m_Resource;

        // dmMessage::ResetURL(component->m_Listener);
        component->m_ComponentIndex = params.m_ComponentIndex;
        component->m_Enabled = 1;
        component->m_World = Matrix4::identity();
        component->m_DoRender = 0;

        // Create rig instance
        dmRig::InstanceCreateParams create_params;
        create_params.m_Context = world->m_RigContext;
        create_params.m_Instance = &component->m_RigInstance;

        RigSceneResource* rig_resource = component->m_Resource->m_RigScene;
        create_params.m_BindPose         = &rig_resource->m_BindPose;
        create_params.m_Skeleton         = rig_resource->m_SkeletonRes->m_Skeleton;
        create_params.m_MeshSet          = rig_resource->m_MeshSetRes->m_MeshSet;
        create_params.m_AnimationSet     = rig_resource->m_AnimationSetRes->m_AnimationSet;
        create_params.m_SkinId           = component->m_Resource->m_Model->m_Skin;
        create_params.m_DefaultAnimation = component->m_Resource->m_Model->m_DefaultAnimation;

        dmRig::CreateResult res = dmRig::InstanceCreate(create_params);
        if (res != dmRig::CREATE_RESULT_OK) {
            dmLogError("Failed to create a rig instance needed by spine model: %d.", res);
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }

        ReHash(component);

        *params.m_UserData = (uintptr_t)index;
        return dmGameObject::CREATE_RESULT_OK;
    }

    static void DestroyComponent(SpineModelWorld* world, uint32_t index)
    {
        SpineModelComponent* component = world->m_Components.Get(index);
        // DestroyPose(component);
        // If we're going to use memset, then we should explicitly clear pose and instance arrays.
        // component->m_Pose.SetCapacity(0);
        // component->m_NodeInstances.SetCapacity(0);
        // component->m_IKTargets.SetCapacity(0);
        // component->m_MeshProperties.SetCapacity(0);

        dmRig::InstanceDestroyParams params;
        params.m_Context = world->m_RigContext;
        params.m_Instance = component->m_RigInstance;
        dmRig::InstanceDestroy(params);

        delete component;
        world->m_Components.Free(index, true);
    }

    dmGameObject::CreateResult CompSpineModelDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        SpineModelWorld* world = (SpineModelWorld*)params.m_World;
        DestroyComponent(world, *params.m_UserData);
        return dmGameObject::CREATE_RESULT_OK;
    }

    static void RenderBatch(SpineModelWorld* world, dmRender::HRenderContext render_context, dmRender::RenderListEntry *buf, uint32_t* begin, uint32_t* end)
    {
        DM_PROFILE(SpineModel, "RenderBatch");

        const SpineModelComponent* first = (SpineModelComponent*) buf[*begin].m_UserData;

        TextureSetResource* texture_set = first->m_Resource->m_RigScene->m_TextureSet;

        uint32_t vertex_count = 0;
        for (uint32_t *i=begin;i!=end;i++)
        {
            const SpineModelComponent* c = (SpineModelComponent*) buf[*i].m_UserData;
            vertex_count += dmRig::GetVertexCount(c->m_RigInstance);
        }

        dmArray<SpineModelVertex> &vertex_buffer = world->m_VertexBufferData;
        if (vertex_buffer.Remaining() < vertex_count)
            vertex_buffer.OffsetCapacity(vertex_count - vertex_buffer.Remaining());

        // Fill in vertex buffer
        SpineModelVertex *vb_begin = vertex_buffer.End();
        SpineModelVertex *vb_end = vb_begin;
        for (uint32_t *i=begin;i!=end;i++)
        {
            const SpineModelComponent* c = (SpineModelComponent*) buf[*i].m_UserData;
            dmRig::RigGenVertexDataParams params;
            params.m_ModelMatrix = c->m_World;
            params.m_VertexData = (void**)&vb_end;
            // params.m_VertexDataSize = ;
            params.m_VertexStride = sizeof(SpineModelVertex);

            vb_end = (SpineModelVertex *)dmRig::GenerateVertexData(world->m_RigContext, c->m_RigInstance, params);
        }
        vertex_buffer.SetSize(vb_end - vertex_buffer.Begin());

        // Ninja in-place writing of render object.
        dmRender::RenderObject& ro = *world->m_RenderObjects.End();
        world->m_RenderObjects.SetSize(world->m_RenderObjects.Size()+1);

        ro.Init();
        ro.m_VertexDeclaration = world->m_VertexDeclaration;
        ro.m_VertexBuffer = world->m_VertexBuffer;
        ro.m_PrimitiveType = dmGraphics::PRIMITIVE_TRIANGLES;
        ro.m_VertexStart = vb_begin - vertex_buffer.Begin();
        ro.m_VertexCount = vb_end - vb_begin;
        ro.m_Material = first->m_Resource->m_Material;
        ro.m_Textures[0] = texture_set->m_Texture;
        ro.m_WorldTransform = first->m_World;

        const dmArray<dmRender::Constant>& constants = first->m_RenderConstants;
        uint32_t size = constants.Size();
        for (uint32_t i = 0; i < size; ++i)
        {
            const dmRender::Constant& c = constants[i];
            dmRender::EnableRenderObjectConstant(&ro, c.m_NameHash, c.m_Value);
        }

        dmGameSystemDDF::SpineModelDesc::BlendMode blend_mode = first->m_Resource->m_Model->m_BlendMode;
        switch (blend_mode)
        {
            case dmGameSystemDDF::SpineModelDesc::BLEND_MODE_ALPHA:
                ro.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
                ro.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            break;

            case dmGameSystemDDF::SpineModelDesc::BLEND_MODE_ADD:
                ro.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
                ro.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
            break;

            case dmGameSystemDDF::SpineModelDesc::BLEND_MODE_MULT:
                ro.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_DST_COLOR;
                ro.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            break;

            default:
                dmLogError("Unknown blend mode: %d\n", blend_mode);
                assert(0);
            break;
        }

        ro.m_SetBlendFactors = 1;

        dmRender::AddToRender(render_context, &ro);
    }

    void UpdateTransforms(SpineModelWorld* world)
    {
        DM_PROFILE(SpineModel, "UpdateTransforms");

        dmArray<SpineModelComponent*>& components = world->m_Components.m_Objects;
        uint32_t n = components.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            SpineModelComponent* c = components[i];

            // NOTE: texture_set = c->m_Resource might be NULL so it's essential to "continue" here
            if (!c->m_Enabled || !c->m_AddedToUpdate)
                continue;

            // if (c->m_MeshEntry != 0x0)
            {
                const Matrix4& go_world = dmGameObject::GetWorldMatrix(c->m_Instance);
                const Matrix4 local = dmTransform::ToMatrix4(c->m_Transform);
                if (dmGameObject::ScaleAlongZ(c->m_Instance))
                {
                    c->m_World = go_world * local;
                }
                else
                {
                    c->m_World = dmTransform::MulNoScaleZ(go_world, local);
                }
            }
        }
    }

    // static void PostEvent(dmMessage::URL* sender, dmMessage::URL* receiver, dmhash_t event_id, dmhash_t animation_id, float blend_weight, dmGameSystemDDF::EventKey* key)
    // {
    //     dmGameSystemDDF::SpineEvent event;
    //     event.m_EventId = event_id;
    //     event.m_AnimationId = animation_id;
    //     event.m_BlendWeight = blend_weight;
    //     event.m_T = key->m_T;
    //     event.m_Integer = key->m_Integer;
    //     event.m_Float = key->m_Float;
    //     event.m_String = key->m_String;

    //     dmhash_t message_id = dmGameSystemDDF::SpineEvent::m_DDFDescriptor->m_NameHash;

    //     uintptr_t descriptor = (uintptr_t)dmGameSystemDDF::SpineEvent::m_DDFDescriptor;
    //     uint32_t data_size = sizeof(dmGameSystemDDF::SpineEvent);
    //     dmMessage::Result result = dmMessage::Post(sender, receiver, message_id, 0, descriptor, &event, data_size);
    //     if (result != dmMessage::RESULT_OK)
    //     {
    //         dmLogError("Could not send spine_event to listener.");
    //     }
    // }

    // static void PostEventsInterval(dmMessage::URL* sender, dmMessage::URL* receiver, dmGameSystemDDF::RigAnimation* animation, float start_cursor, float end_cursor, float duration, bool backwards, float blend_weight)
    // {
    //     const uint32_t track_count = animation->m_EventTracks.m_Count;
    //     for (uint32_t ti = 0; ti < track_count; ++ti)
    //     {
    //         dmGameSystemDDF::EventTrack* track = &animation->m_EventTracks[ti];
    //         const uint32_t key_count = track->m_Keys.m_Count;
    //         for (uint32_t ki = 0; ki < key_count; ++ki)
    //         {
    //             dmGameSystemDDF::EventKey* key = &track->m_Keys[ki];
    //             float cursor = key->m_T;
    //             if (backwards)
    //                 cursor = duration - cursor;
    //             if (start_cursor <= cursor && cursor < end_cursor)
    //             {
    //                 PostEvent(sender, receiver, track->m_EventId, animation->m_Id, blend_weight, key);
    //             }
    //         }
    //     }
    // }

    // static void PostEvents(SpinePlayer* player, dmMessage::URL* sender, dmMessage::URL* listener, dmGameSystemDDF::RigAnimation* animation, float dt, float prev_cursor, float duration, bool completed, float blend_weight)
    // {
    //     dmMessage::URL receiver = *listener;
    //     if (!dmMessage::IsSocketValid(receiver.m_Socket))
    //     {
    //         receiver = *sender;
    //         receiver.m_Fragment = 0; // broadcast to sibling components
    //     }
    //     float cursor = player->m_Cursor;
    //     // Since the intervals are defined as t0 <= t < t1, make sure we include the end of the animation, i.e. when t1 == duration
    //     if (completed)
    //         cursor += dt;
    //     // If the start cursor is greater than the end cursor, we have looped and handle that as two distinct intervals: [0,end_cursor) and [start_cursor,duration)
    //     // Note that for looping ping pong, one event can be triggered twice during the same frame by appearing in both intervals
    //     if (prev_cursor > cursor)
    //     {
    //         bool prev_backwards = player->m_Backwards;
    //         // Handle the flipping nature of ping pong
    //         if (player->m_Playback == dmGameObject::PLAYBACK_LOOP_PINGPONG)
    //         {
    //             prev_backwards = !player->m_Backwards;
    //         }
    //         PostEventsInterval(sender, &receiver, animation, prev_cursor, duration, duration, prev_backwards, blend_weight);
    //         PostEventsInterval(sender, &receiver, animation, 0.0f, cursor, duration, player->m_Backwards, blend_weight);
    //     }
    //     else
    //     {
    //         // Special handling when we reach the way back of once ping pong playback
    //         float half_duration = duration * 0.5f;
    //         if (player->m_Playback == dmGameObject::PLAYBACK_ONCE_PINGPONG && cursor > half_duration)
    //         {
    //             // If the previous cursor was still in the forward direction, treat it as two distinct intervals: [start_cursor,half_duration) and [half_duration, end_cursor)
    //             if (prev_cursor < half_duration)
    //             {
    //                 PostEventsInterval(sender, &receiver, animation, prev_cursor, half_duration, duration, false, blend_weight);
    //                 PostEventsInterval(sender, &receiver, animation, half_duration, cursor, duration, true, blend_weight);
    //             }
    //             else
    //             {
    //                 PostEventsInterval(sender, &receiver, animation, prev_cursor, cursor, duration, true, blend_weight);
    //             }
    //         }
    //         else
    //         {
    //             PostEventsInterval(sender, &receiver, animation, prev_cursor, cursor, duration, player->m_Backwards, blend_weight);
    //         }
    //     }
    // }

    // static inline dmTransform::Transform GetPoseTransform(const dmArray<SpineBone>& bind_pose, const dmArray<dmTransform::Transform>& pose, dmTransform::Transform transform, const uint32_t index) {
    //     if(bind_pose[index].m_ParentIndex == INVALID_BONE_INDEX)
    //         return transform;
    //     transform = dmTransform::Mul(pose[bind_pose[index].m_ParentIndex], transform);
    //     return GetPoseTransform(bind_pose, pose, transform, bind_pose[index].m_ParentIndex);
    // }

    // static inline float ToEulerZ(const dmTransform::Transform& t)
    // {
    //     Vectormath::Aos::Quat q(t.GetRotation());
    //     return dmVMath::QuatToEuler(q.getZ(), q.getY(), q.getX(), q.getW()).getZ() * (M_PI/180.0f);
    // }

    // static void ApplyOneBoneIKConstraint(const dmGameSystemDDF::IK* ik, const dmArray<SpineBone>& bind_pose, dmArray<dmTransform::Transform>& pose, const Vector3 target_wp, const Vector3 parent_wp, const float mix)
    // {
    //     if (mix == 0.0f)
    //         return;
    //     const dmTransform::Transform& parent_bt = bind_pose[ik->m_Parent].m_LocalToParent;
    //     dmTransform::Transform& parent_t = pose[ik->m_Parent];
    //     float parentRotation = ToEulerZ(parent_bt);
    //     // Based on code by Ryan Juckett with permission: Copyright (c) 2008-2009 Ryan Juckett, http://www.ryanjuckett.com/
    //     float rotationIK = atan2(target_wp.getY() - parent_wp.getY(), target_wp.getX() - parent_wp.getX());
    //     parentRotation = parentRotation + (rotationIK - parentRotation) * mix;
    //     parent_t.SetRotation( dmVMath::QuatFromAngle(2, parentRotation) );
    // }

    // // Based on http://www.ryanjuckett.com/programming/analytic-two-bone-ik-in-2d/
    // static void ApplyTwoBoneIKConstraint(const dmGameSystemDDF::IK* ik, const dmArray<SpineBone>& bind_pose, dmArray<dmTransform::Transform>& pose, const Vector3 target_wp, const Vector3 parent_wp, const bool bend_positive, const float mix)
    // {
    //     if (mix == 0.0f)
    //         return;
    //     const dmTransform::Transform& parent_bt = bind_pose[ik->m_Parent].m_LocalToParent;
    //     const dmTransform::Transform& child_bt = bind_pose[ik->m_Child].m_LocalToParent;
    //     dmTransform::Transform& parent_t = pose[ik->m_Parent];
    //     dmTransform::Transform& child_t = pose[ik->m_Child];
    //     float childRotation = ToEulerZ(child_bt), parentRotation = ToEulerZ(parent_bt);

    //     // recalc target position to local (relative parent)
    //     const Vector3 target(target_wp.getX() - parent_wp.getX(), target_wp.getY() - parent_wp.getY(), 0.0f);
    //     const Vector3 child_p = child_bt.GetTranslation();
    //     const float childX = child_p.getX(), childY = child_p.getY();
    //     const float offset = atan2(childY, childX);
    //     const float len1 = (float)sqrt(childX * childX + childY * childY);
    //     const float len2 = bind_pose[ik->m_Child].m_Length;

    //     // Based on code by Ryan Juckett with permission: Copyright (c) 2008-2009 Ryan Juckett, http://www.ryanjuckett.com/
    //     const float cosDenom = 2.0f * len1 * len2;
    //     if (cosDenom < 0.0001f) {
    //         childRotation = childRotation + ((float)atan2(target.getY(), target.getX()) - parentRotation - childRotation) * mix;
    //         child_t.SetRotation( dmVMath::QuatFromAngle(2, childRotation) );
    //         return;
    //     }
    //     float cosValue = (target.getX() * target.getX() + target.getY() * target.getY() - len1 * len1 - len2 * len2) / cosDenom;
    //     cosValue = dmMath::Max(-1.0f, dmMath::Min(1.0f, cosValue));
    //     const float childAngle = (float)acos(cosValue) * (bend_positive ? 1.0f : -1.0f);
    //     const float adjacent = len1 + len2 * cosValue;
    //     const float opposite = len2 * sin(childAngle);
    //     const float parentAngle = (float)atan2(target.getY() * adjacent - target.getX() * opposite, target.getX() * adjacent + target.getY() * opposite);
    //     parentRotation = ((parentAngle - offset) - parentRotation) * mix;
    //     childRotation = ((childAngle + offset) - childRotation) * mix;
    //     parent_t.SetRotation( dmVMath::QuatFromAngle(2, parentRotation) );
    //     child_t.SetRotation( dmVMath::QuatFromAngle(2, childRotation) );
    // }

    dmGameObject::CreateResult CompSpineModelAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params)
    {
        SpineModelWorld* world = (SpineModelWorld*)params.m_World;
        uint32_t index = (uint32_t)*params.m_UserData;
        SpineModelComponent* component = world->m_Components.Get(index);
        component->m_AddedToUpdate = true;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompSpineModelUpdate(const dmGameObject::ComponentsUpdateParams& params)
    {
        SpineModelWorld* world = (SpineModelWorld*)params.m_World;

        dmArray<SpineModelComponent*>& components = world->m_Components.m_Objects;
        const uint32_t count = components.Size();

        for (uint32_t i = 0; i < count; ++i)
        {
            SpineModelComponent& component = *components[i];
            component.m_DoRender = 0;

            if (!component.m_Enabled || !component.m_AddedToUpdate)
                continue;

            uint32_t const_count = component.m_RenderConstants.Size();
            for (uint32_t const_i = 0; const_i < const_count; ++const_i)
            {
                if (lengthSqr(component.m_RenderConstants[const_i].m_Value - component.m_PrevRenderConstants[const_i]) > 0)
                {
                    ReHash(&component);
                    break;
                }
            }

            // NOTE we used to set DoRender here, figure out how to handle this
            component.m_DoRender = 1;
        }
        // Animate(world, params.m_UpdateContext->m_DT);

        return dmGameObject::UPDATE_RESULT_OK;
    }

    static void RenderListDispatch(dmRender::RenderListDispatchParams const &params)
    {
        SpineModelWorld *world = (SpineModelWorld *) params.m_UserData;

        switch (params.m_Operation)
        {
            case dmRender::RENDER_LIST_OPERATION_BEGIN:
            {
                dmGraphics::SetVertexBufferData(world->m_VertexBuffer, 0, 0, dmGraphics::BUFFER_USAGE_STATIC_DRAW);
                world->m_RenderObjects.SetSize(0);
                dmArray<SpineModelVertex>& vertex_buffer = world->m_VertexBufferData;
                vertex_buffer.SetSize(0);
                break;
            }
            case dmRender::RENDER_LIST_OPERATION_BATCH:
            {
                RenderBatch(world, params.m_Context, params.m_Buf, params.m_Begin, params.m_End);
                break;
            }
            case dmRender::RENDER_LIST_OPERATION_END:
            {
                dmGraphics::SetVertexBufferData(world->m_VertexBuffer, sizeof(SpineModelVertex) * world->m_VertexBufferData.Size(),
                                                world->m_VertexBufferData.Begin(), dmGraphics::BUFFER_USAGE_STATIC_DRAW);
                DM_COUNTER("SpineVertexBuffer", world->m_VertexBufferData.Size() * sizeof(SpineModelVertex));
                break;
            }
            default:
                assert(false);
                break;
        }
    }

    dmGameObject::UpdateResult CompSpineModelRender(const dmGameObject::ComponentsRenderParams& params)
    {
        SpineModelContext* context = (SpineModelContext*)params.m_Context;
        dmRender::HRenderContext render_context = context->m_RenderContext;
        SpineModelWorld* world = (SpineModelWorld*)params.m_World;

        UpdateTransforms(world);

        dmArray<SpineModelComponent*>& components = world->m_Components.m_Objects;
        const uint32_t count = components.Size();

        // Prepare list submit
        dmRender::RenderListEntry* render_list = dmRender::RenderListAlloc(render_context, count);
        dmRender::HRenderListDispatch dispatch = dmRender::RenderListMakeDispatch(render_context, &RenderListDispatch, world);
        dmRender::RenderListEntry* write_ptr = render_list;

        for (uint32_t i = 0; i < count; ++i)
        {
            SpineModelComponent& component = *components[i];
            if (!component.m_DoRender)
                continue;

            const Vector4 trans = component.m_World.getCol(3);
            write_ptr->m_WorldPosition = Point3(trans.getX(), trans.getY(), trans.getZ());
            write_ptr->m_UserData = (uintptr_t) &component;
            write_ptr->m_BatchKey = component.m_MixedHash;
            write_ptr->m_TagMask = dmRender::GetMaterialTagMask(component.m_Resource->m_Material);
            write_ptr->m_Dispatch = dispatch;
            write_ptr->m_MajorOrder = dmRender::RENDER_ORDER_WORLD;
            ++write_ptr;
        }

        dmRender::RenderListSubmit(render_context, render_list, write_ptr);
        return dmGameObject::UPDATE_RESULT_OK;
    }

    static bool CompSpineModelGetConstantCallback(void* user_data, dmhash_t name_hash, dmRender::Constant** out_constant)
    {
        SpineModelComponent* component = (SpineModelComponent*)user_data;
        // dmArray<dmRender::Constant>& constants = component->m_RenderConstants;
        // uint32_t count = constants.Size();
        // for (uint32_t i = 0; i < count; ++i)
        // {
        //     dmRender::Constant& c = constants[i];
        //     if (c.m_NameHash == name_hash)
        //     {
        //         *out_constant = &c;
        //         return true;
        //     }
        // }
        return false;
    }

    static void CompSpineModelSetConstantCallback(void* user_data, dmhash_t name_hash, uint32_t* element_index, const dmGameObject::PropertyVar& var)
    {
        SpineModelComponent* component = (SpineModelComponent*)user_data;
        // dmArray<dmRender::Constant>& constants = component->m_RenderConstants;
        // uint32_t count = constants.Size();
        // Vector4* v = 0x0;
        // for (uint32_t i = 0; i < count; ++i)
        // {
        //     dmRender::Constant& c = constants[i];
        //     if (c.m_NameHash == name_hash)
        //     {
        //         v = &c.m_Value;
        //         break;
        //     }
        // }
        // if (v == 0x0)
        // {
        //     if (constants.Full())
        //     {
        //         uint32_t capacity = constants.Capacity() + 4;
        //         constants.SetCapacity(capacity);
        //         component->m_PrevRenderConstants.SetCapacity(capacity);
        //     }
        //     dmRender::Constant c;
        //     dmRender::GetMaterialProgramConstant(component->m_Resource->m_Material, name_hash, c);
        //     constants.Push(c);
        //     component->m_PrevRenderConstants.Push(c.m_Value);
        //     v = &(constants[constants.Size()-1].m_Value);
        // }
        // if (element_index == 0x0)
        //     *v = Vector4(var.m_V4[0], var.m_V4[1], var.m_V4[2], var.m_V4[3]);
        // else
        //     v->setElem(*element_index, (float)var.m_Number);
        // ReHash(component);
    }

    dmGameObject::UpdateResult CompSpineModelOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        SpineModelWorld* world = (SpineModelWorld*)params.m_World;
        SpineModelComponent* component = world->m_Components.Get(*params.m_UserData);
        if (params.m_Message->m_Id == dmGameObjectDDF::Enable::m_DDFDescriptor->m_NameHash)
        {
            component->m_Enabled = 1;
        }
        else if (params.m_Message->m_Id == dmGameObjectDDF::Disable::m_DDFDescriptor->m_NameHash)
        {
            component->m_Enabled = 0;
        }
        else if (params.m_Message->m_Descriptor != 0x0)
        {
            if (params.m_Message->m_Id == dmGameSystemDDF::SpinePlayAnimation::m_DDFDescriptor->m_NameHash)
            {
                dmGameSystemDDF::SpinePlayAnimation* ddf = (dmGameSystemDDF::SpinePlayAnimation*)params.m_Message->m_Data;
                if (dmRig::PLAY_RESULT_OK == dmRig::PlayAnimation(component->m_RigInstance, ddf->m_AnimationId, (dmGameObject::Playback)ddf->m_Playback, ddf->m_BlendDuration))
                {
                    // component->m_Listener = params.m_Message->m_Sender;
                }
            }
            else if (params.m_Message->m_Id == dmGameSystemDDF::SpineCancelAnimation::m_DDFDescriptor->m_NameHash)
            {
                dmRig::CancelAnimation(component->m_RigInstance);
            }
            // else if (params.m_Message->m_Id == dmGameSystemDDF::SetConstantSpineModel::m_DDFDescriptor->m_NameHash)
            // {
            //     dmGameSystemDDF::SetConstantSpineModel* ddf = (dmGameSystemDDF::SetConstantSpineModel*)params.m_Message->m_Data;
            //     dmGameObject::PropertyResult result = dmGameSystem::SetMaterialConstant(component->m_Resource->m_Material, ddf->m_NameHash,
            //             dmGameObject::PropertyVar(ddf->m_Value), CompSpineModelSetConstantCallback, component);
            //     if (result == dmGameObject::PROPERTY_RESULT_NOT_FOUND)
            //     {
            //         dmMessage::URL& receiver = params.m_Message->m_Receiver;
            //         dmLogError("'%s:%s#%s' has no constant named '%s'",
            //                 dmMessage::GetSocketName(receiver.m_Socket),
            //                 (const char*)dmHashReverse64(receiver.m_Path, 0x0),
            //                 (const char*)dmHashReverse64(receiver.m_Fragment, 0x0),
            //                 (const char*)dmHashReverse64(ddf->m_NameHash, 0x0));
            //     }
            // }
            // else if (params.m_Message->m_Id == dmGameSystemDDF::ResetConstantSpineModel::m_DDFDescriptor->m_NameHash)
            // {
            //     dmGameSystemDDF::ResetConstantSpineModel* ddf = (dmGameSystemDDF::ResetConstantSpineModel*)params.m_Message->m_Data;
            //     dmArray<dmRender::Constant>& constants = component->m_RenderConstants;
            //     uint32_t size = constants.Size();
            //     for (uint32_t i = 0; i < size; ++i)
            //     {
            //         if( constants[i].m_NameHash == ddf->m_NameHash)
            //         {
            //             constants[i] = constants[size - 1];
            //             component->m_PrevRenderConstants[i] = component->m_PrevRenderConstants[size - 1];
            //             constants.Pop();
            //             ReHash(component);
            //             break;
            //         }
            //     }
            // }
        }

        return dmGameObject::UPDATE_RESULT_OK;
    }

    static void OnResourceReloaded(SpineModelWorld* world, SpineModelComponent* component)
    {
        // dmGameSystem::SpineSceneResource* scene = component->m_Resource->m_Scene;
        // component->m_Skin = dmHashString64(component->m_Resource->m_Model->m_Skin);
        // AllocateMeshProperties(scene->m_MeshSetRes->m_MeshSet, component->m_MeshProperties);
        // component->m_MeshEntry = FindMeshEntry(scene->m_MeshSetRes->m_MeshSet, component->m_Skin);
        // dmhash_t default_anim_id = dmHashString64(component->m_Resource->m_Model->m_DefaultAnimation);
        // for (uint32_t i = 0; i < 2; ++i)
        // {
        //     SpinePlayer* player = &component->m_Players[i];
        //     if (player->m_Playing)
        //     {
        //         player->m_Animation = FindAnimation(scene->m_AnimationSetRes->m_AnimationSet, player->m_AnimationId);
        //         if (player->m_Animation == 0x0)
        //         {
        //             player->m_AnimationId = default_anim_id;
        //             player->m_Animation = FindAnimation(scene->m_AnimationSetRes->m_AnimationSet, player->m_AnimationId);
        //         }
        //     }
        // }
        // DestroyPose(component);
        // CreatePose(world, component);
    }

    void CompSpineModelOnReload(const dmGameObject::ComponentOnReloadParams& params)
    {
        SpineModelWorld* world = (SpineModelWorld*)params.m_World;
        SpineModelComponent* component = world->m_Components.Get(*params.m_UserData);
        component->m_Resource = (SpineModelResource*)params.m_Resource;
        OnResourceReloaded(world, component);
    }

    dmGameObject::PropertyResult CompSpineModelGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value)
    {
        SpineModelWorld* world = (SpineModelWorld*)params.m_World;
        // SpineModelComponent* component = world->m_Components.Get(*params.m_UserData);
        // if (params.m_PropertyId == PROP_SKIN)
        // {
        //     out_value.m_Variant = dmGameObject::PropertyVar(component->m_Skin);
        //     return dmGameObject::PROPERTY_RESULT_OK;
        // }
        // else if (params.m_PropertyId == PROP_ANIMATION)
        // {
        //     SpinePlayer* player = GetPlayer(component);
        //     out_value.m_Variant = dmGameObject::PropertyVar(player->m_AnimationId);
        //     return dmGameObject::PROPERTY_RESULT_OK;
        // }
        // return GetMaterialConstant(component->m_Resource->m_Material, params.m_PropertyId, out_value, CompSpineModelGetConstantCallback, component);
        return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
    }

    dmGameObject::PropertyResult CompSpineModelSetProperty(const dmGameObject::ComponentSetPropertyParams& params)
    {
        SpineModelWorld* world = (SpineModelWorld*)params.m_World;
        SpineModelComponent* component = world->m_Components.Get(*params.m_UserData);
        // if (params.m_PropertyId == PROP_SKIN)
        // {
        //     if (params.m_Value.m_Type != dmGameObject::PROPERTY_TYPE_HASH)
        //         return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
        //     dmGameSystemDDF::MeshEntry* mesh_entry = 0x0;
        //     dmGameSystemDDF::MeshSet* mesh_set = component->m_Resource->m_Scene->m_MeshSetRes->m_MeshSet;
        //     dmhash_t skin = params.m_Value.m_Hash;
        //     for (uint32_t i = 0; i < mesh_set->m_MeshEntries.m_Count; ++i)
        //     {
        //         if (skin == mesh_set->m_MeshEntries[i].m_Id)
        //         {
        //             mesh_entry = &mesh_set->m_MeshEntries[i];
        //             break;
        //         }
        //     }
        //     if (mesh_entry == 0x0)
        //     {
        //         dmLogError("Could not find skin '%s' in the mesh set.", (const char*)dmHashReverse64(skin, 0x0));
        //         return dmGameObject::PROPERTY_RESULT_UNSUPPORTED_VALUE;
        //     }
        //     component->m_MeshEntry = mesh_entry;
        //     component->m_Skin = params.m_Value.m_Hash;
        //     return dmGameObject::PROPERTY_RESULT_OK;
        // }
        // return SetMaterialConstant(component->m_Resource->m_Material, params.m_PropertyId, params.m_Value, CompSpineModelSetConstantCallback, component);
        return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
    }

    static void ResourceReloadedCallback(const dmResource::ResourceReloadedParams& params)
    {
        SpineModelWorld* world = (SpineModelWorld*) params.m_UserData;
        dmArray<SpineModelComponent*>& components = world->m_Components.m_Objects;
        uint32_t n = components.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            SpineModelComponent* component = components[i];
            if (component->m_Resource != 0x0 && component->m_Resource->m_RigScene == params.m_Resource->m_Resource)
                OnResourceReloaded(world, component);
        }
    }
}
