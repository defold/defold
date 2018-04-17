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

    static const dmhash_t PROP_SKIN = dmHashString64("skin");
    static const dmhash_t PROP_ANIMATION = dmHashString64("animation");
    static const dmhash_t PROP_CURSOR = dmHashString64("cursor");
    static const dmhash_t PROP_PLAYBACK_RATE = dmHashString64("playback_rate");

    static void ResourceReloadedCallback(const dmResource::ResourceReloadedParams& params);
    static void DestroyComponent(SpineModelWorld* world, uint32_t index);

    // Translation table to translate from dmGameObject playback mode into dmRig playback mode.
    static struct PlaybackGameObjectToRig
    {
        dmRig::RigPlayback m_Table[dmGameObject::PLAYBACK_COUNT];
        PlaybackGameObjectToRig()
        {
            m_Table[dmGameObject::PLAYBACK_NONE]            = dmRig::PLAYBACK_NONE;
            m_Table[dmGameObject::PLAYBACK_ONCE_FORWARD]    = dmRig::PLAYBACK_ONCE_FORWARD;
            m_Table[dmGameObject::PLAYBACK_ONCE_BACKWARD]   = dmRig::PLAYBACK_ONCE_BACKWARD;
            m_Table[dmGameObject::PLAYBACK_LOOP_FORWARD]    = dmRig::PLAYBACK_LOOP_FORWARD;
            m_Table[dmGameObject::PLAYBACK_LOOP_BACKWARD]   = dmRig::PLAYBACK_LOOP_BACKWARD;
            m_Table[dmGameObject::PLAYBACK_LOOP_PINGPONG]   = dmRig::PLAYBACK_LOOP_PINGPONG;
            m_Table[dmGameObject::PLAYBACK_ONCE_PINGPONG]   = dmRig::PLAYBACK_ONCE_PINGPONG;
        }
    } ddf_playback_map;

    dmGameObject::CreateResult CompSpineModelNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        SpineModelContext* context = (SpineModelContext*)params.m_Context;
        dmRender::HRenderContext render_context = context->m_RenderContext;
        SpineModelWorld* world = new SpineModelWorld();

        dmRig::NewContextParams rig_params = {0};
        rig_params.m_Context = &world->m_RigContext;
        rig_params.m_MaxRigInstanceCount = context->m_MaxSpineModelCount;
        dmRig::Result rr = dmRig::NewContext(rig_params);
        if (rr != dmRig::RESULT_OK)
        {
            dmLogFatal("Unable to create spine rig context: %d", rr);
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }

        world->m_Components.SetCapacity(context->m_MaxSpineModelCount);
        world->m_RenderObjects.SetCapacity(context->m_MaxSpineModelCount);

        dmGraphics::VertexElement ve[] =
        {
                {"position", 0, 3, dmGraphics::TYPE_FLOAT, false},
                {"texcoord0", 1, 2, dmGraphics::TYPE_FLOAT, true},
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

        dmResource::UnregisterResourceReloadedCallback(((SpineModelContext*)params.m_Context)->m_Factory, ResourceReloadedCallback, world);

        dmRig::DeleteContext(world->m_RigContext);

        delete world;

        return dmGameObject::CREATE_RESULT_OK;
    }

    static bool GetSender(SpineModelComponent* component, dmMessage::URL* out_sender)
    {
        dmMessage::URL sender;
        sender.m_Socket = dmGameObject::GetMessageSocket(dmGameObject::GetCollection(component->m_Instance));
        if (dmMessage::IsSocketValid(sender.m_Socket))
        {
            dmGameObject::Result go_result = dmGameObject::GetComponentId(component->m_Instance, component->m_ComponentIndex, &sender.m_Fragment);
            if (go_result == dmGameObject::RESULT_OK)
            {
                sender.m_Path = dmGameObject::GetIdentifier(component->m_Instance);
                *out_sender = sender;
                return true;
            }
        }
        return false;
    }

    static void CompSpineModelEventCallback(dmRig::RigEventType event_type, void* event_data, void* user_data1, void* user_data2)
    {
        SpineModelComponent* component = (SpineModelComponent*)user_data1;

        dmMessage::URL sender;
        dmMessage::URL receiver = component->m_Listener;

        switch (event_type)
        {
            case dmRig::RIG_EVENT_TYPE_COMPLETED:
            {
                if (!GetSender(component, &sender))
                {
                    dmLogError("Could not send animation_done to listener because of incomplete component.");
                    return;
                }

                dmhash_t message_id = dmGameSystemDDF::SpineAnimationDone::m_DDFDescriptor->m_NameHash;
                const dmRig::RigCompletedEventData* completed_event = (const dmRig::RigCompletedEventData*)event_data;

                dmGameSystemDDF::SpineAnimationDone message;
                message.m_AnimationId = completed_event->m_AnimationId;
                message.m_Playback    = completed_event->m_Playback;

                uintptr_t descriptor = (uintptr_t)dmGameSystemDDF::SpineAnimationDone::m_DDFDescriptor;
                uint32_t data_size = sizeof(dmGameSystemDDF::SpineAnimationDone);
                dmMessage::Result result = dmMessage::Post(&sender, &receiver, message_id, 0, descriptor, &message, data_size, 0);
                dmMessage::ResetURL(component->m_Listener);
                if (result != dmMessage::RESULT_OK)
                {
                    dmLogError("Could not send animation_done to listener.");
                }

                break;
            }
            case dmRig::RIG_EVENT_TYPE_KEYFRAME:
            {
                if (!GetSender(component, &sender))
                {
                    return;
                }
                receiver.m_Function = 0;

                if (!dmMessage::IsSocketValid(receiver.m_Socket))
                {
                    receiver = sender;
                    receiver.m_Fragment = 0;
                }

                dmhash_t message_id = dmGameSystemDDF::SpineEvent::m_DDFDescriptor->m_NameHash;
                const dmRig::RigKeyframeEventData* keyframe_event = (const dmRig::RigKeyframeEventData*)event_data;

                dmGameSystemDDF::SpineEvent event;
                event.m_EventId     = keyframe_event->m_EventId;
                event.m_AnimationId = keyframe_event->m_AnimationId;
                event.m_BlendWeight = keyframe_event->m_BlendWeight;
                event.m_T           = keyframe_event->m_T;
                event.m_Integer     = keyframe_event->m_Integer;
                event.m_Float       = keyframe_event->m_Float;
                event.m_String      = keyframe_event->m_String;

                uintptr_t descriptor = (uintptr_t)dmGameSystemDDF::SpineEvent::m_DDFDescriptor;
                uint32_t data_size = sizeof(dmGameSystemDDF::SpineEvent);
                dmMessage::Result result = dmMessage::Post(&sender, &receiver, message_id, 0, descriptor, &event, data_size, 0);
                if (result != dmMessage::RESULT_OK)
                {
                    dmLogError("Could not send spine_event to listener.");
                }

                break;
            }
            default:
                dmLogError("Unknown rig event received (%d).", event_type);
                break;
        }

    }

    static void CompSpineModelPoseCallback(void* user_data1, void* user_data2)
    {
        SpineModelComponent* component = (SpineModelComponent*)user_data1;

        // Include instance transform in the GO instance reflecting the root bone
        dmArray<dmTransform::Transform>& pose = *dmRig::GetPose(component->m_RigInstance);
        if (!pose.Empty()) {
            dmGameObject::SetBoneTransforms(component->m_NodeInstances[0], component->m_Transform, pose.Begin(), pose.Size());
        }
    }

    static inline dmRender::HMaterial GetMaterial(const SpineModelComponent* component, const SpineModelResource* resource) {
        return component->m_Material ? component->m_Material : resource->m_Material;
    }

    static void ReHash(SpineModelComponent* component)
    {
        // material, texture set, blend mode and render constants
        HashState32 state;
        bool reverse = false;
        SpineModelResource* resource = component->m_Resource;
        dmGameSystemDDF::SpineModelDesc* ddf = resource->m_Model;
        dmRender::HMaterial material = GetMaterial(component, resource);
        TextureSetResource* texture_set = resource->m_RigScene->m_TextureSet;
        dmHashInit32(&state, reverse);
        dmHashUpdateBuffer32(&state, &material, sizeof(material));
        dmHashUpdateBuffer32(&state, &texture_set, sizeof(texture_set));
        dmHashUpdateBuffer32(&state, &ddf->m_BlendMode, sizeof(ddf->m_BlendMode));
        ReHashRenderConstants(&component->m_RenderConstants, &state);
        component->m_MixedHash = dmHashFinal32(&state);
        component->m_ReHash = 0;
    }

    static bool CreateGOBones(SpineModelWorld* world, SpineModelComponent* component)
    {
        dmGameObject::HInstance spine_instance = component->m_Instance;
        dmGameObject::HCollection collection = dmGameObject::GetCollection(spine_instance);

        const dmArray<dmRig::RigBone>& bind_pose = component->m_Resource->m_RigScene->m_BindPose;
        const dmRigDDF::Skeleton* skeleton = component->m_Resource->m_RigScene->m_SkeletonRes->m_Skeleton;
        uint32_t bone_count = skeleton->m_Bones.m_Count;

        component->m_NodeInstances.SetCapacity(bone_count);
        component->m_NodeInstances.SetSize(bone_count);
        if (bone_count > world->m_ScratchInstances.Capacity()) {
            world->m_ScratchInstances.SetCapacity(bone_count);
        }
        world->m_ScratchInstances.SetSize(0);
        for (uint32_t i = 0; i < bone_count; ++i)
        {
            dmGameObject::HInstance bone_instance = dmGameObject::New(collection, 0x0);
            if (bone_instance == 0x0) {
                component->m_NodeInstances.SetSize(i);
                return false;
            }

            uint32_t index = dmGameObject::AcquireInstanceIndex(collection);
            if (index == dmGameObject::INVALID_INSTANCE_POOL_INDEX)
            {
                dmGameObject::Delete(collection, bone_instance, false);
                component->m_NodeInstances.SetSize(i);
                return false;
            }

            dmhash_t id = dmGameObject::ConstructInstanceId(index);
            dmGameObject::AssignInstanceIndex(index, bone_instance);

            dmGameObject::Result result = dmGameObject::SetIdentifier(collection, bone_instance, id);
            if (dmGameObject::RESULT_OK != result)
            {
                dmGameObject::Delete(collection, bone_instance, false);
                component->m_NodeInstances.SetSize(i);
                return false;
            }

            dmGameObject::SetBone(bone_instance, true);
            dmTransform::Transform transform = bind_pose[i].m_LocalToParent;
            if (i == 0)
            {
                transform = dmTransform::Mul(component->m_Transform, transform);
            }
            dmGameObject::SetPosition(bone_instance, Point3(transform.GetTranslation()));
            dmGameObject::SetRotation(bone_instance, transform.GetRotation());
            dmGameObject::SetScale(bone_instance, transform.GetScale());
            component->m_NodeInstances[i] = bone_instance;
            world->m_ScratchInstances.Push(bone_instance);
        }
        // Set parents in reverse to account for child-prepending
        for (uint32_t i = 0; i < bone_count; ++i)
        {
            uint32_t index = bone_count - 1 - i;
            dmGameObject::HInstance bone_instance = world->m_ScratchInstances[index];
            dmGameObject::HInstance parent = spine_instance;
            if (index > 0)
            {
                parent = world->m_ScratchInstances[skeleton->m_Bones[index].m_Parent];
            }
            dmGameObject::SetParent(bone_instance, parent);
        }

        return true;
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
        dmMessage::ResetURL(component->m_Listener);

        component->m_ComponentIndex = params.m_ComponentIndex;
        component->m_Enabled = 1;
        component->m_World = Matrix4::identity();
        component->m_DoRender = 0;

        // Create GO<->bone representation
        // We need to make sure that bone GOs are created before we start the default animation.
        if (!CreateGOBones(world, component))
        {
            dmLogError("Failed to create game objects for bones in spine model. Consider increasing collection max instances (collection.max_instances).");
            DestroyComponent(world, index);
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }

        // Create rig instance
        dmRig::InstanceCreateParams create_params = {0};
        create_params.m_Context = world->m_RigContext;
        create_params.m_Instance = &component->m_RigInstance;

        create_params.m_PoseCallback = CompSpineModelPoseCallback;
        create_params.m_PoseCBUserData1 = component;
        create_params.m_PoseCBUserData2 = 0;
        create_params.m_EventCallback = CompSpineModelEventCallback;
        create_params.m_EventCBUserData1 = component;
        create_params.m_EventCBUserData2 = 0;

        RigSceneResource* rig_resource = component->m_Resource->m_RigScene;
        create_params.m_BindPose         = &rig_resource->m_BindPose;
        create_params.m_Skeleton         = rig_resource->m_SkeletonRes->m_Skeleton;
        create_params.m_MeshSet          = rig_resource->m_MeshSetRes->m_MeshSet;
        create_params.m_AnimationSet     = rig_resource->m_AnimationSetRes->m_AnimationSet;
        create_params.m_PoseIdxToInfluence = &rig_resource->m_PoseIdxToInfluence;
        create_params.m_TrackIdxToPose     = &rig_resource->m_TrackIdxToPose;
        create_params.m_MeshId           = dmHashString64(component->m_Resource->m_Model->m_Skin);
        create_params.m_DefaultAnimation = dmHashString64(component->m_Resource->m_Model->m_DefaultAnimation);

        dmRig::Result res = dmRig::InstanceCreate(create_params);
        if (res != dmRig::RESULT_OK) {
            dmLogError("Failed to create a rig instance needed by spine model: %d.", res);
            if (res == dmRig::RESULT_ERROR_BUFFER_FULL) {
                dmLogError("Try increasing the spine.max_count value in game.project");
            }
            DestroyComponent(world, index);
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }

        component->m_ReHash = 1;

        *params.m_UserData = (uintptr_t)index;
        return dmGameObject::CREATE_RESULT_OK;
    }

    static void DestroyComponent(SpineModelWorld* world, uint32_t index)
    {
        SpineModelComponent* component = world->m_Components.Get(index);
        dmGameObject::DeleteBones(component->m_Instance);
        // If we're going to use memset, then we should explicitly clear pose and instance arrays.
        component->m_NodeInstances.SetCapacity(0);

        dmRig::InstanceDestroyParams params = {0};
        params.m_Context = world->m_RigContext;
        params.m_Instance = component->m_RigInstance;
        dmRig::InstanceDestroy(params);

        delete component;
        world->m_Components.Free(index, true);
    }

    dmGameObject::CreateResult CompSpineModelDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        SpineModelWorld* world = (SpineModelWorld*)params.m_World;
        uint32_t index = *params.m_UserData;
        SpineModelComponent* component = world->m_Components.Get(index);
        if (component->m_Material) {
            dmResource::Release(dmGameObject::GetFactory(params.m_Instance), (void*)component->m_Material);
        }
        DestroyComponent(world, index);
        return dmGameObject::CREATE_RESULT_OK;
    }

    static void RenderBatch(SpineModelWorld* world, dmRender::HRenderContext render_context, dmRender::RenderListEntry *buf, uint32_t* begin, uint32_t* end)
    {
        DM_PROFILE(SpineModel, "RenderBatch");

        const SpineModelComponent* first = (SpineModelComponent*) buf[*begin].m_UserData;
        const SpineModelResource* resource = first->m_Resource;

        uint32_t vertex_count = 0;
        for (uint32_t *i=begin;i!=end;i++)
        {
            const SpineModelComponent* c = (SpineModelComponent*) buf[*i].m_UserData;
            uint32_t count = dmRig::GetVertexCount(c->m_RigInstance);
            vertex_count += count;
        }

        dmArray<dmRig::RigSpineModelVertex> &vertex_buffer = world->m_VertexBufferData;
        if (vertex_buffer.Remaining() < vertex_count)
            vertex_buffer.OffsetCapacity(vertex_count - vertex_buffer.Remaining());

        // Fill in vertex buffer
        dmRig::RigSpineModelVertex *vb_begin = vertex_buffer.End();
        dmRig::RigSpineModelVertex *vb_end = vb_begin;
        dmRig::HRigContext rig_context = world->m_RigContext;
        for (uint32_t *i=begin;i!=end;i++)
        {
            const SpineModelComponent* c = (SpineModelComponent*) buf[*i].m_UserData;
            vb_end = (dmRig::RigSpineModelVertex*)dmRig::GenerateVertexData(rig_context, c->m_RigInstance, c->m_World, Matrix4::identity(), Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_SPINE, (void*)vb_end);
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
        ro.m_Textures[0] = resource->m_RigScene->m_TextureSet->m_Texture;
        ro.m_Material = GetMaterial(first, resource);

        const dmRender::Constant* constants = first->m_RenderConstants.m_RenderConstants;
        uint32_t size = first->m_RenderConstants.m_ConstantCount;
        for (uint32_t i = 0; i < size; ++i)
        {
            const dmRender::Constant& c = constants[i];
            dmRender::EnableRenderObjectConstant(&ro, c.m_NameHash, c.m_Value);
        }

        dmGameSystemDDF::SpineModelDesc::BlendMode blend_mode = resource->m_Model->m_BlendMode;
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

            if (dmRig::IsValid(c->m_RigInstance))
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

    dmGameObject::CreateResult CompSpineModelAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params)
    {
        SpineModelWorld* world = (SpineModelWorld*)params.m_World;
        uint32_t index = (uint32_t)*params.m_UserData;
        SpineModelComponent* component = world->m_Components.Get(index);
        component->m_AddedToUpdate = true;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompSpineModelUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
    {
        SpineModelWorld* world = (SpineModelWorld*)params.m_World;

        dmRig::Result rig_res = dmRig::Update(world->m_RigContext, params.m_UpdateContext->m_DT);

        dmArray<SpineModelComponent*>& components = world->m_Components.m_Objects;
        const uint32_t count = components.Size();

        for (uint32_t i = 0; i < count; ++i)
        {
            SpineModelComponent& component = *components[i];
            component.m_DoRender = 0;

            if (!component.m_Enabled || !component.m_AddedToUpdate)
                continue;

            bool rehash = component.m_ReHash;
            if(!rehash)
            {
                uint32_t const_count = component.m_RenderConstants.m_ConstantCount;
                for (uint32_t const_i = 0; const_i < const_count; ++const_i)
                {
                    if (lengthSqr(component.m_RenderConstants.m_RenderConstants[const_i].m_Value - component.m_RenderConstants.m_PrevRenderConstants[const_i]) > 0)
                    {
                        rehash = true;
                        break;
                    }
                }
            }
            if(rehash)
            {
                ReHash(&component);
            }

            component.m_DoRender = 1;
        }

        update_result.m_TransformsUpdated = rig_res == dmRig::RESULT_UPDATED_POSE;
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
                dmArray<dmRig::RigSpineModelVertex>& vertex_buffer = world->m_VertexBufferData;
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
                dmGraphics::SetVertexBufferData(world->m_VertexBuffer, sizeof(dmRig::RigSpineModelVertex) * world->m_VertexBufferData.Size(),
                                                world->m_VertexBufferData.Begin(), dmGraphics::BUFFER_USAGE_STATIC_DRAW);
                DM_COUNTER("SpineVertexBuffer", world->m_VertexBufferData.Size() * sizeof(dmRig::RigSpineModelVertex));
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
            if (!component.m_DoRender || !component.m_Enabled)
                continue;

            const Vector4 trans = component.m_World.getCol(3);
            write_ptr->m_WorldPosition = Point3(trans.getX(), trans.getY(), trans.getZ());
            write_ptr->m_UserData = (uintptr_t) &component;
            write_ptr->m_BatchKey = component.m_MixedHash;
            write_ptr->m_TagMask = dmRender::GetMaterialTagMask(GetMaterial(&component, component.m_Resource));
            write_ptr->m_Dispatch = dispatch;
            write_ptr->m_MinorOrder = 0;
            write_ptr->m_MajorOrder = dmRender::RENDER_ORDER_WORLD;
            ++write_ptr;
        }

        dmRender::RenderListSubmit(render_context, render_list, write_ptr);
        return dmGameObject::UPDATE_RESULT_OK;
    }

    static bool CompSpineModelGetConstantCallback(void* user_data, dmhash_t name_hash, dmRender::Constant** out_constant)
    {
        SpineModelComponent* component = (SpineModelComponent*)user_data;
        return GetRenderConstant(&component->m_RenderConstants, name_hash, out_constant);
    }

    static void CompSpineModelSetConstantCallback(void* user_data, dmhash_t name_hash, uint32_t* element_index, const dmGameObject::PropertyVar& var)
    {
        SpineModelComponent* component = (SpineModelComponent*)user_data;
        SetRenderConstant(&component->m_RenderConstants, GetMaterial(component, component->m_Resource), name_hash, element_index, var);
        component->m_ReHash = 1;
    }

    dmGameObject::UpdateResult CompSpineModelOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        SpineModelWorld* world = (SpineModelWorld*)params.m_World;
        SpineModelComponent* component = world->m_Components.Get(*params.m_UserData);
        if (params.m_Message->m_Id == dmGameObjectDDF::Enable::m_DDFDescriptor->m_NameHash)
        {
            component->m_Enabled = 1;
            dmRig::SetEnabled(component->m_RigInstance, true);
        }
        else if (params.m_Message->m_Id == dmGameObjectDDF::Disable::m_DDFDescriptor->m_NameHash)
        {
            component->m_Enabled = 0;
            dmRig::SetEnabled(component->m_RigInstance, false);
        }
        else if (params.m_Message->m_Descriptor != 0x0)
        {
            if (params.m_Message->m_Id == dmGameSystemDDF::SpinePlayAnimation::m_DDFDescriptor->m_NameHash)
            {
                dmGameSystemDDF::SpinePlayAnimation* ddf = (dmGameSystemDDF::SpinePlayAnimation*)params.m_Message->m_Data;
                if (dmRig::RESULT_OK == dmRig::PlayAnimation(component->m_RigInstance, ddf->m_AnimationId, ddf_playback_map.m_Table[ddf->m_Playback], ddf->m_BlendDuration, ddf->m_Offset, ddf->m_PlaybackRate))
                {
                    component->m_Listener = params.m_Message->m_Sender;
                }
            }
            else if (params.m_Message->m_Id == dmGameSystemDDF::SpineCancelAnimation::m_DDFDescriptor->m_NameHash)
            {
                dmRig::CancelAnimation(component->m_RigInstance);
            }
            else if (params.m_Message->m_Id == dmGameSystemDDF::SetConstantSpineModel::m_DDFDescriptor->m_NameHash)
            {
                dmGameSystemDDF::SetConstantSpineModel* ddf = (dmGameSystemDDF::SetConstantSpineModel*)params.m_Message->m_Data;
                dmGameObject::PropertyResult result = dmGameSystem::SetMaterialConstant(GetMaterial(component, component->m_Resource), ddf->m_NameHash,
                        dmGameObject::PropertyVar(ddf->m_Value), CompSpineModelSetConstantCallback, component);
                if (result == dmGameObject::PROPERTY_RESULT_NOT_FOUND)
                {
                    dmMessage::URL& receiver = params.m_Message->m_Receiver;
                    dmLogError("'%s:%s#%s' has no constant named '%s'",
                            dmMessage::GetSocketName(receiver.m_Socket),
                            dmHashReverseSafe64(receiver.m_Path),
                            dmHashReverseSafe64(receiver.m_Fragment),
                            dmHashReverseSafe64(ddf->m_NameHash));
                }
            }
            else if (params.m_Message->m_Id == dmGameSystemDDF::ResetConstantSpineModel::m_DDFDescriptor->m_NameHash)
            {
                dmGameSystemDDF::ResetConstantSpineModel* ddf = (dmGameSystemDDF::ResetConstantSpineModel*)params.m_Message->m_Data;
                dmRender::Constant* constants = component->m_RenderConstants.m_RenderConstants;
                uint32_t size = component->m_RenderConstants.m_ConstantCount;
                for (uint32_t i = 0; i < size; ++i)
                {
                    if( constants[i].m_NameHash == ddf->m_NameHash)
                    {
                        constants[i] = constants[size - 1];
                        component->m_RenderConstants.m_PrevRenderConstants[i] = component->m_RenderConstants.m_PrevRenderConstants[size - 1];
                        component->m_RenderConstants.m_ConstantCount--;
                        component->m_ReHash = 1;
                        break;
                    }
                }
            }
        }

        return dmGameObject::UPDATE_RESULT_OK;
    }

    static bool OnResourceReloaded(SpineModelWorld* world, SpineModelComponent* component, int index)
    {
        dmRig::HRigContext rig_context = world->m_RigContext;

        // Destroy old rig
        dmRig::InstanceDestroyParams destroy_params = {0};
        destroy_params.m_Context = rig_context;
        destroy_params.m_Instance = component->m_RigInstance;
        dmRig::InstanceDestroy(destroy_params);

        // Delete old bones, then recreate with new data.
        // We need to make sure that bone GOs are created before we start the default animation.
        dmGameObject::DeleteBones(component->m_Instance);
        if (!CreateGOBones(world, component))
        {
            dmLogError("Failed to create game objects for bones in spine model. Consider increasing collection max instances (collection.max_instances).");
            DestroyComponent(world, index);
            return false;
        }

        // Create rig instance
        dmRig::InstanceCreateParams create_params = {0};
        create_params.m_Context = rig_context;
        create_params.m_Instance = &component->m_RigInstance;

        create_params.m_PoseCallback = CompSpineModelPoseCallback;
        create_params.m_PoseCBUserData1 = component;
        create_params.m_PoseCBUserData2 = 0;
        create_params.m_EventCallback = CompSpineModelEventCallback;
        create_params.m_EventCBUserData1 = component;
        create_params.m_EventCBUserData2 = 0;

        RigSceneResource* rig_resource = component->m_Resource->m_RigScene;
        create_params.m_BindPose         = &rig_resource->m_BindPose;
        create_params.m_Skeleton         = rig_resource->m_SkeletonRes->m_Skeleton;
        create_params.m_MeshSet          = rig_resource->m_MeshSetRes->m_MeshSet;
        create_params.m_AnimationSet     = rig_resource->m_AnimationSetRes->m_AnimationSet;
        create_params.m_PoseIdxToInfluence = &rig_resource->m_PoseIdxToInfluence;
        create_params.m_TrackIdxToPose     = &rig_resource->m_TrackIdxToPose;
        create_params.m_MeshId           = dmHashString64(component->m_Resource->m_Model->m_Skin);
        create_params.m_DefaultAnimation = dmHashString64(component->m_Resource->m_Model->m_DefaultAnimation);

        dmRig::Result res = dmRig::InstanceCreate(create_params);
        if (res != dmRig::RESULT_OK) {
            dmLogError("Failed to create a rig instance needed by spine model: %d.", res);
            if (res == dmRig::RESULT_ERROR_BUFFER_FULL) {
                dmLogError("Try increasing the spine.max_count value in game.project");
            }
            DestroyComponent(world, index);
            return false;
        }

        component->m_ReHash = 1;

        return true;
    }

    void CompSpineModelOnReload(const dmGameObject::ComponentOnReloadParams& params)
    {
        SpineModelWorld* world = (SpineModelWorld*)params.m_World;
        int index = *params.m_UserData;
        SpineModelComponent* component = world->m_Components.Get(index);
        component->m_Resource = (SpineModelResource*)params.m_Resource;
        (void)OnResourceReloaded(world, component, index);
    }

    dmGameObject::PropertyResult CompSpineModelGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value)
    {
        SpineModelWorld* world = (SpineModelWorld*)params.m_World;
        SpineModelComponent* component = world->m_Components.Get(*params.m_UserData);
        if (params.m_PropertyId == PROP_SKIN)
        {
            out_value.m_Variant = dmGameObject::PropertyVar(dmRig::GetMesh(component->m_RigInstance));
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (params.m_PropertyId == PROP_ANIMATION)
        {
            out_value.m_Variant = dmGameObject::PropertyVar(dmRig::GetAnimation(component->m_RigInstance));
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (params.m_PropertyId == PROP_CURSOR)
        {
            out_value.m_Variant = dmGameObject::PropertyVar(dmRig::GetCursor(component->m_RigInstance, true));
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (params.m_PropertyId == PROP_PLAYBACK_RATE)
        {
            out_value.m_Variant = dmGameObject::PropertyVar(dmRig::GetPlaybackRate(component->m_RigInstance));
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (params.m_PropertyId == PROP_MATERIAL)
        {
            dmRender::HMaterial material = GetMaterial(component, component->m_Resource);
            return GetResourceProperty(dmGameObject::GetFactory(params.m_Instance), material, out_value);
        }
        return GetMaterialConstant(GetMaterial(component, component->m_Resource), params.m_PropertyId, out_value, true, CompSpineModelGetConstantCallback, component);
    }

    dmGameObject::PropertyResult CompSpineModelSetProperty(const dmGameObject::ComponentSetPropertyParams& params)
    {
        SpineModelWorld* world = (SpineModelWorld*)params.m_World;
        SpineModelComponent* component = world->m_Components.Get(*params.m_UserData);
        if (params.m_PropertyId == PROP_SKIN)
        {
            if (params.m_Value.m_Type != dmGameObject::PROPERTY_TYPE_HASH)
                return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;

            dmRig::Result res = dmRig::SetMesh(component->m_RigInstance, params.m_Value.m_Hash);
            if (res == dmRig::RESULT_ERROR)
            {
                dmLogError("Could not find skin '%s' on the spine model.", dmHashReverseSafe64(params.m_Value.m_Hash));
                return dmGameObject::PROPERTY_RESULT_UNSUPPORTED_VALUE;
            }
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (params.m_PropertyId == PROP_CURSOR)
        {
            if (params.m_Value.m_Type != dmGameObject::PROPERTY_TYPE_NUMBER)
                return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;

            dmRig::Result res = dmRig::SetCursor(component->m_RigInstance, params.m_Value.m_Number, true);
            if (res == dmRig::RESULT_ERROR)
            {
                dmLogError("Could not set cursor %f on the spine model.", params.m_Value.m_Number);
                return dmGameObject::PROPERTY_RESULT_UNSUPPORTED_VALUE;
            }
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (params.m_PropertyId == PROP_PLAYBACK_RATE)
        {
            if (params.m_Value.m_Type != dmGameObject::PROPERTY_TYPE_NUMBER)
                return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;

            dmRig::Result res = dmRig::SetPlaybackRate(component->m_RigInstance, params.m_Value.m_Number);
            if (res == dmRig::RESULT_ERROR)
            {
                dmLogError("Could not set playback rate %f on the spine model.", params.m_Value.m_Number);
                return dmGameObject::PROPERTY_RESULT_UNSUPPORTED_VALUE;
            }
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (params.m_PropertyId == PROP_MATERIAL)
        {
            dmGameObject::PropertyResult res = SetResourceProperty(dmGameObject::GetFactory(params.m_Instance), params.m_Value, MATERIAL_EXT_HASH, (void**)&component->m_Material);
            component->m_ReHash |= res == dmGameObject::PROPERTY_RESULT_OK;
            return res;
        }
        return SetMaterialConstant(GetMaterial(component, component->m_Resource), params.m_PropertyId, params.m_Value, CompSpineModelSetConstantCallback, component);
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
                OnResourceReloaded(world, component, i);
        }
    }

    static Vector3 UpdateIKInstanceCallback(dmRig::IKTarget* ik_target)
    {
        SpineModelComponent* component = (SpineModelComponent*)ik_target->m_UserPtr;
        dmhash_t target_instance_id = ik_target->m_UserHash;
        dmGameObject::HInstance target_instance = dmGameObject::GetInstanceFromIdentifier(dmGameObject::GetCollection(component->m_Instance), target_instance_id);
        if(target_instance == 0x0)
        {
            // instance have been removed, disable animation
            dmLogError("Could not get IK position for target %s, removed?", dmHashReverseSafe64(target_instance_id))
            ik_target->m_Callback = 0x0;
            ik_target->m_Mix = 0x0;
            return Vector3(0.0f);
        }
        return (Vector3)dmTransform::Apply(dmTransform::Inv(dmTransform::Mul(dmGameObject::GetWorldTransform(component->m_Instance), component->m_Transform)), dmGameObject::GetWorldPosition(target_instance));
    }

    static Vector3 UpdateIKPositionCallback(dmRig::IKTarget* ik_target)
    {
        SpineModelComponent* component = (SpineModelComponent*)ik_target->m_UserPtr;
        return (Vector3)dmTransform::Apply(dmTransform::Inv(dmTransform::Mul(dmGameObject::GetWorldTransform(component->m_Instance), component->m_Transform)), (Point3)ik_target->m_Position);
    }

    bool CompSpineModelSetIKTargetInstance(SpineModelComponent* component, dmhash_t constraint_id, float mix, dmhash_t instance_id)
    {
        dmRig::IKTarget* target = dmRig::GetIKTarget(component->m_RigInstance, constraint_id);
        if (!target) {
            return false;
        }
        target->m_Callback = UpdateIKInstanceCallback;
        target->m_Mix = mix;
        target->m_UserPtr = component;
        target->m_UserHash = instance_id;
        return true;
    }

    bool CompSpineModelSetIKTargetPosition(SpineModelComponent* component, dmhash_t constraint_id, float mix, Point3 position)
    {
        dmRig::IKTarget* target = dmRig::GetIKTarget(component->m_RigInstance, constraint_id);
        if (!target) {
            return false;
        }
        target->m_Callback = UpdateIKPositionCallback;
        target->m_Mix = mix;
        target->m_UserPtr = component;
        target->m_Position = (Vector3)position;
        return true;
    }

}
