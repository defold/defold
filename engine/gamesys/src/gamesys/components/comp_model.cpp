#include "comp_model.h"

#include <string.h>
#include <float.h>

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

#include "gamesys_ddf.h"
#include "model_ddf.h"

using namespace Vectormath::Aos;

namespace dmGameSystem
{
    using namespace Vectormath::Aos;
    using namespace dmGameSystemDDF;

    static const dmhash_t PROP_SKIN = dmHashString64("skin");
    static const dmhash_t PROP_ANIMATION = dmHashString64("animation");
    static const dmhash_t PROP_CURSOR = dmHashString64("cursor");
    static const dmhash_t PROP_PLAYBACK_RATE = dmHashString64("playback_rate");
    static const dmhash_t PROP_TEXTURE[dmRender::RenderObject::MAX_TEXTURE_COUNT] = {
        dmHashString64("texture0"), dmHashString64("texture1"), dmHashString64("texture2"), dmHashString64("texture3"), dmHashString64("texture4"), dmHashString64("texture5"), dmHashString64("texture6"), dmHashString64("texture7"), dmHashString64("texture8"), dmHashString64("texture9"), dmHashString64("texture10"), dmHashString64("texture11"), dmHashString64("texture12"), dmHashString64("texture13"), dmHashString64("texture14"), dmHashString64("texture15"), dmHashString64("texture16"), dmHashString64("texture17"), dmHashString64("texture18"), dmHashString64("texture19"), dmHashString64("texture20"), dmHashString64("texture21"), dmHashString64("texture22"), dmHashString64("texture23"), dmHashString64("texture24"), dmHashString64("texture25"), dmHashString64("texture26"), dmHashString64("texture27"), dmHashString64("texture28"), dmHashString64("texture29"), dmHashString64("texture30"), dmHashString64("texture31")
    };

    static void ResourceReloadedCallback(const dmResource::ResourceReloadedParams& params);
    static void DestroyComponent(ModelWorld* world, uint32_t index);

    dmGameObject::CreateResult CompModelNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        ModelContext* context = (ModelContext*)params.m_Context;
        dmRender::HRenderContext render_context = context->m_RenderContext;
        ModelWorld* world = new ModelWorld();

        world->m_Components.SetCapacity(context->m_MaxModelCount);
        // world->m_RigContext = context->m_RigContext;
        world->m_RenderObjects.SetCapacity(context->m_MaxModelCount);

        dmGraphics::VertexElement ve[] =
        {
                {"position", 0, 0, 3, dmGraphics::TYPE_FLOAT, false},
                {"texcoord0", 0, 1, 2, dmGraphics::TYPE_FLOAT, false},
                {"normal", 0, 2, 3, dmGraphics::TYPE_FLOAT, false},
        };

        world->m_VertexDeclaration = dmGraphics::NewVertexDeclaration(dmRender::GetGraphicsContext(render_context), ve, sizeof(ve) / sizeof(dmGraphics::VertexElement));
        world->m_VertexBuffer = dmGraphics::NewVertexBuffer(dmRender::GetGraphicsContext(render_context), 0, 0x0, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
        *params.m_World = world;

        dmResource::RegisterResourceReloadedCallback(context->m_Factory, ResourceReloadedCallback, world);

        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompModelDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        ModelWorld* world = (ModelWorld*)params.m_World;
        dmGraphics::DeleteVertexDeclaration(world->m_VertexDeclaration);
        dmGraphics::DeleteVertexBuffer(world->m_VertexBuffer);

        dmResource::UnregisterResourceReloadedCallback(((ModelContext*)params.m_Context)->m_Factory, ResourceReloadedCallback, world);

        delete world;

        return dmGameObject::CREATE_RESULT_OK;
    }

    static bool GetSender(ModelComponent* component, dmMessage::URL* out_sender)
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

    static void CompModelEventCallback(dmRig::RigEventType event_type, void* event_data, void* user_data1, void* user_data2)
    {
        ModelComponent* component = (ModelComponent*)user_data1;

        dmMessage::URL sender;
        dmMessage::URL receiver = component->m_Listener;
        switch (event_type) {
            case dmRig::RIG_EVENT_TYPE_COMPLETED:
            {
                if (!GetSender(component, &sender))
                {
                    dmLogError("Could not send animation_done to listener because of incomplete component.");
                    return;
                }

                dmhash_t message_id = dmModelDDF::ModelAnimationDone::m_DDFDescriptor->m_NameHash;
                const dmRig::RigCompletedEventData* completed_event = (const dmRig::RigCompletedEventData*)event_data;

                dmModelDDF::ModelAnimationDone message;
                message.m_AnimationId = completed_event->m_AnimationId;
                message.m_Playback    = completed_event->m_Playback;

                uintptr_t descriptor = (uintptr_t)dmModelDDF::ModelAnimationDone::m_DDFDescriptor;
                uint32_t data_size = sizeof(dmModelDDF::ModelAnimationDone);
                dmMessage::Result result = dmMessage::Post(&sender, &receiver, message_id, 0, descriptor, &message, data_size, 0);
                dmMessage::ResetURL(component->m_Listener);
                if (result != dmMessage::RESULT_OK)
                {
                    dmLogError("Could not send animation_done to listener.");
                }

                break;
            }
            default:
                dmLogError("Unknown rig event received (%d).", event_type);
                break;
        }

    }

    static void CompModelPoseCallback(void* user_data1, void* user_data2)
    {
        ModelComponent* component = (ModelComponent*)user_data1;

        // Include instance transform in the GO instance reflecting the root bone
        dmArray<dmTransform::Transform>& pose = *dmRig::GetPose(component->m_RigInstance);
        if (!pose.Empty()) {
            dmGameObject::SetBoneTransforms(component->m_NodeInstances[0], pose.Begin(), pose.Size());
        }
    }

    static void ReHash(ModelComponent* component)
    {
        // Hash resource-ptr, material-handle, blend mode and render constants
        HashState32 state;
        bool reverse = false;
        ModelResource* resource = component->m_Resource;
        dmHashInit32(&state, reverse);
        dmHashUpdateBuffer32(&state, &resource->m_Textures[0], sizeof(resource->m_Textures[0])); // only one texture for now. Should we really support up to 32 textures per model?
        dmHashUpdateBuffer32(&state, &resource->m_Material, sizeof(resource->m_Material));
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

    static bool CreateGOBones(ModelWorld* world, ModelComponent* component)
    {
        if(!component->m_Resource->m_RigScene->m_SkeletonRes)
            return true;

        dmGameObject::HInstance instance = component->m_Instance;
        dmGameObject::HCollection collection = dmGameObject::GetCollection(instance);

        const dmArray<dmRig::RigBone>& bind_pose = component->m_Resource->m_RigScene->m_BindPose;
        const dmRigDDF::Skeleton* skeleton = component->m_Resource->m_RigScene->m_SkeletonRes->m_Skeleton;
        uint32_t bone_count = skeleton->m_Bones.m_Count;

        // When reloading, we want to preserve the existing instances.
        // We need to be able to dynamically add objects on reloading since we can mix mesh, skeleton, animations. We could possibly delete all and recreate all,
        // but except for performance it would also need double the instance count (which is preallocated) since we're using deferred deletes, which would not reflect the actual max instance need.
        uint32_t prev_bone_count = component->m_NodeInstances.Size();
        if (bone_count > component->m_NodeInstances.Capacity()) {
            component->m_NodeInstances.OffsetCapacity(bone_count - prev_bone_count);
        }
        component->m_NodeInstances.SetSize(bone_count);
        if (bone_count > world->m_ScratchInstances.Capacity()) {
            world->m_ScratchInstances.SetCapacity(bone_count);
        }
        world->m_ScratchInstances.SetSize(0);
        for (uint32_t i = 0; i < bone_count; ++i)
        {
            dmGameObject::HInstance bone_inst;
            if(i < prev_bone_count)
            {
                bone_inst = component->m_NodeInstances[i];
            }
            else
            {
                bone_inst = dmGameObject::New(collection, 0x0);
                if (bone_inst == 0x0) {
                    component->m_NodeInstances.SetSize(i);
                    return false;
                }

                uint32_t index = dmGameObject::AcquireInstanceIndex(collection);
                if (index == dmGameObject::INVALID_INSTANCE_POOL_INDEX)
                {
                    dmGameObject::Delete(collection, bone_inst, false);
                    component->m_NodeInstances.SetSize(i);
                    return false;
                }

                dmhash_t id = dmGameObject::ConstructInstanceId(index);
                dmGameObject::AssignInstanceIndex(index, bone_inst);

                dmGameObject::Result result = dmGameObject::SetIdentifier(collection, bone_inst, id);
                if (dmGameObject::RESULT_OK != result)
                {
                    dmGameObject::Delete(collection, bone_inst, false);
                    component->m_NodeInstances.SetSize(i);
                    return false;
                }
                dmGameObject::SetBone(bone_inst, true);
                component->m_NodeInstances[i] = bone_inst;
            }

            dmTransform::Transform transform = bind_pose[i].m_LocalToParent;
            if (i == 0)
            {
                transform = dmTransform::Mul(component->m_Transform, transform);
            }
            dmGameObject::SetPosition(bone_inst, Point3(transform.GetTranslation()));
            dmGameObject::SetRotation(bone_inst, transform.GetRotation());
            dmGameObject::SetScale(bone_inst, transform.GetScale());
            world->m_ScratchInstances.Push(bone_inst);
        }
        // Set parents in reverse to account for child-prepending
        for (uint32_t i = 0; i < bone_count; ++i)
        {
            uint32_t index = bone_count - 1 - i;
            dmGameObject::HInstance inst = world->m_ScratchInstances[index];
            dmGameObject::HInstance parent = instance;
            if (index > 0)
            {
                parent = world->m_ScratchInstances[skeleton->m_Bones[index].m_Parent];
            }
            dmGameObject::SetParent(inst, parent);
        }

        return true;
    }

    dmGameObject::CreateResult CompModelCreate(const dmGameObject::ComponentCreateParams& params)
    {
        ModelWorld* world = (ModelWorld*)params.m_World;

        if (world->m_Components.Full())
        {
            dmLogError("Model could not be created since the buffer is full (%d).", world->m_Components.Capacity());
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
        uint32_t index = world->m_Components.Alloc();
        ModelComponent* component = new ModelComponent;
        memset(component, 0, sizeof(ModelComponent));
        world->m_Components.Set(index, component);
        component->m_Instance = params.m_Instance;
        component->m_Transform = dmTransform::Transform(Vector3(params.m_Position), params.m_Rotation, 1.0f);
        component->m_Resource = (ModelResource*)params.m_Resource;
        dmMessage::ResetURL(component->m_Listener);

        component->m_ComponentIndex = params.m_ComponentIndex;
        component->m_Enabled = 1;
        component->m_World = Matrix4::identity();
        component->m_DoRender = 0;

        // Create rig instance
        dmRig::InstanceCreateParams create_params = {0};
        create_params.m_Context = dmGameObject::GetRigContext(dmGameObject::GetCollection(component->m_Instance));
        create_params.m_Instance = &component->m_RigInstance;

        create_params.m_PoseCallback = CompModelPoseCallback;
        create_params.m_PoseCBUserData1 = component;
        create_params.m_PoseCBUserData2 = 0;
        create_params.m_EventCallback = CompModelEventCallback;
        create_params.m_EventCBUserData1 = component;
        create_params.m_EventCBUserData2 = 0;

        RigSceneResource* rig_resource = component->m_Resource->m_RigScene;
        create_params.m_BindPose         = &rig_resource->m_BindPose;
        create_params.m_AnimationSet     = rig_resource->m_AnimationSetRes == 0x0 ? 0x0 : rig_resource->m_AnimationSetRes->m_AnimationSet;
        create_params.m_Skeleton         = rig_resource->m_SkeletonRes == 0x0 ? 0x0 : rig_resource->m_SkeletonRes->m_Skeleton;
        create_params.m_MeshSet          = rig_resource->m_MeshSetRes->m_MeshSet;
        create_params.m_PoseIdxToInfluence = &rig_resource->m_PoseIdxToInfluence;
        create_params.m_TrackIdxToPose     = &rig_resource->m_TrackIdxToPose;
        create_params.m_MeshId           = 0; // not implemented for models
        create_params.m_DefaultAnimation = dmHashString64(component->m_Resource->m_Model->m_DefaultAnimation);

        dmRig::Result res = dmRig::InstanceCreate(create_params);
        if (res != dmRig::RESULT_OK) {
            dmLogError("Failed to create a rig instance needed by model: %d.", res);
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }

        ReHash(component);

        // Create GO<->bone representation
        CreateGOBones(world, component);

        *params.m_UserData = (uintptr_t)index;
        return dmGameObject::CREATE_RESULT_OK;
    }

    static void DestroyComponent(ModelWorld* world, uint32_t index)
    {
        ModelComponent* component = world->m_Components.Get(index);
        dmGameObject::DeleteBones(component->m_Instance);
        // If we're going to use memset, then we should explicitly clear pose and instance arrays.
        component->m_NodeInstances.SetCapacity(0);

        dmRig::InstanceDestroyParams params = {0};
        params.m_Context = dmGameObject::GetRigContext(dmGameObject::GetCollection(component->m_Instance));
        params.m_Instance = component->m_RigInstance;
        dmRig::InstanceDestroy(params);

        delete component;
        world->m_Components.Free(index, true);
    }

    dmGameObject::CreateResult CompModelDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        ModelWorld* world = (ModelWorld*)params.m_World;
        DestroyComponent(world, *params.m_UserData);
        return dmGameObject::CREATE_RESULT_OK;
    }

    static void RenderBatch(ModelWorld* world, dmRender::HRenderContext render_context, dmRender::RenderListEntry *buf, uint32_t* begin, uint32_t* end)
    {
        DM_PROFILE(Model, "RenderBatch");

        const ModelComponent* first = (ModelComponent*) buf[*begin].m_UserData;
        uint32_t vertex_count = 0;
        uint32_t max_component_vertices = 0;
        for (uint32_t *i=begin;i!=end;i++)
        {
            const ModelComponent* c = (ModelComponent*) buf[*i].m_UserData;
            uint32_t count = dmRig::GetVertexCount(c->m_RigInstance);
            vertex_count += count;
            max_component_vertices = dmMath::Max(max_component_vertices, count);
        }

        // Early exit if there is nothing to render
        if (vertex_count == 0) {
            return;
        }

        dmArray<dmRig::RigModelVertex> &vertex_buffer = world->m_VertexBufferData;
        if (vertex_buffer.Remaining() < vertex_count)
            vertex_buffer.OffsetCapacity(vertex_count - vertex_buffer.Remaining());

        // Fill in vertex buffer
        dmRig::RigModelVertex *vb_begin = vertex_buffer.End();
        dmRig::RigModelVertex *vb_end = vb_begin;
        for (uint32_t *i=begin;i!=end;i++)
        {
            const ModelComponent* c = (ModelComponent*) buf[*i].m_UserData;
            dmRig::HRigContext rig_context = dmGameObject::GetRigContext(dmGameObject::GetCollection(c->m_Instance));
            Matrix4 normal_matrix = inverse(c->m_World);
            normal_matrix = transpose(normal_matrix);
            vb_end = (dmRig::RigModelVertex *)dmRig::GenerateVertexData(rig_context, c->m_RigInstance, c->m_World, normal_matrix, Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_MODEL, (void*)vb_end);
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
        ro.m_WorldTransform = first->m_World;
        for (uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
            ro.m_Textures[i] = first->m_Resource->m_Textures[i];

        const dmArray<dmRender::Constant>& constants = first->m_RenderConstants;
        uint32_t size = constants.Size();
        for (uint32_t i = 0; i < size; ++i)
        {
            const dmRender::Constant& c = constants[i];
            dmRender::EnableRenderObjectConstant(&ro, c.m_NameHash, c.m_Value);
        }

        dmRender::AddToRender(render_context, &ro);
    }

    void UpdateTransforms(ModelWorld* world)
    {
        DM_PROFILE(Model, "UpdateTransforms");

        dmArray<ModelComponent*>& components = world->m_Components.m_Objects;
        uint32_t n = components.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            ModelComponent* c = components[i];

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

    dmGameObject::CreateResult CompModelAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params)
    {
        ModelWorld* world = (ModelWorld*)params.m_World;
        uint32_t index = (uint32_t)*params.m_UserData;
        ModelComponent* component = world->m_Components.Get(index);
        component->m_AddedToUpdate = true;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompModelUpdate(const dmGameObject::ComponentsUpdateParams& params)
    {
        ModelWorld* world = (ModelWorld*)params.m_World;

        dmArray<ModelComponent*>& components = world->m_Components.m_Objects;
        const uint32_t count = components.Size();

        for (uint32_t i = 0; i < count; ++i)
        {
            ModelComponent& component = *components[i];
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

            component.m_DoRender = 1;
        }

        return dmGameObject::UPDATE_RESULT_OK;
    }

    static void RenderListDispatch(dmRender::RenderListDispatchParams const &params)
    {
        ModelWorld *world = (ModelWorld *) params.m_UserData;

        switch (params.m_Operation)
        {
            case dmRender::RENDER_LIST_OPERATION_BEGIN:
            {
                dmGraphics::SetVertexBufferData(world->m_VertexBuffer, 0, 0, dmGraphics::BUFFER_USAGE_STATIC_DRAW);
                world->m_RenderObjects.SetSize(0);
                dmArray<dmRig::RigModelVertex>& vertex_buffer = world->m_VertexBufferData;
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
                dmGraphics::SetVertexBufferData(world->m_VertexBuffer, sizeof(dmRig::RigModelVertex) * world->m_VertexBufferData.Size(),
                                                world->m_VertexBufferData.Begin(), dmGraphics::BUFFER_USAGE_STATIC_DRAW);
                DM_COUNTER("ModelVertexBuffer", world->m_VertexBufferData.Size() * sizeof(dmRig::RigModelVertex));
                break;
            }
            default:
                assert(false);
                break;
        }
    }

    dmGameObject::UpdateResult CompModelRender(const dmGameObject::ComponentsRenderParams& params)
    {
        ModelContext* context = (ModelContext*)params.m_Context;
        dmRender::HRenderContext render_context = context->m_RenderContext;
        ModelWorld* world = (ModelWorld*)params.m_World;

        UpdateTransforms(world);

        dmArray<ModelComponent*>& components = world->m_Components.m_Objects;
        const uint32_t count = components.Size();

        // Prepare list submit
        dmRender::RenderListEntry* render_list = dmRender::RenderListAlloc(render_context, count);
        dmRender::HRenderListDispatch dispatch = dmRender::RenderListMakeDispatch(render_context, &RenderListDispatch, world);
        dmRender::RenderListEntry* write_ptr = render_list;

        for (uint32_t i = 0; i < count; ++i)
        {
            ModelComponent& component = *components[i];
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

    static bool CompModelGetConstantCallback(void* user_data, dmhash_t name_hash, dmRender::Constant** out_constant)
    {
        ModelComponent* component = (ModelComponent*)user_data;
        dmArray<dmRender::Constant>& constants = component->m_RenderConstants;
        uint32_t count = constants.Size();
        for (uint32_t i = 0; i < count; ++i)
        {
            dmRender::Constant& c = constants[i];
            if (c.m_NameHash == name_hash)
            {
                *out_constant = &c;
                return true;
            }
        }
        return false;
    }

    static void CompModelSetConstantCallback(void* user_data, dmhash_t name_hash, uint32_t* element_index, const dmGameObject::PropertyVar& var)
    {
        ModelComponent* component = (ModelComponent*)user_data;
        dmArray<dmRender::Constant>& constants = component->m_RenderConstants;
        uint32_t count = constants.Size();
        Vector4* v = 0x0;
        for (uint32_t i = 0; i < count; ++i)
        {
            dmRender::Constant& c = constants[i];
            if (c.m_NameHash == name_hash)
            {
                v = &c.m_Value;
                break;
            }
        }
        if (v == 0x0)
        {
            if (constants.Full())
            {
                uint32_t capacity = constants.Capacity() + 4;
                constants.SetCapacity(capacity);
                component->m_PrevRenderConstants.SetCapacity(capacity);
            }
            dmRender::Constant c;
            dmRender::GetMaterialProgramConstant(component->m_Resource->m_Material, name_hash, c);
            constants.Push(c);
            component->m_PrevRenderConstants.Push(c.m_Value);
            v = &(constants[constants.Size()-1].m_Value);
        }
        if (element_index == 0x0)
            *v = Vector4(var.m_V4[0], var.m_V4[1], var.m_V4[2], var.m_V4[3]);
        else
            v->setElem(*element_index, (float)var.m_Number);
        ReHash(component);
    }

    dmGameObject::UpdateResult CompModelOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        ModelWorld* world = (ModelWorld*)params.m_World;
        ModelComponent* component = world->m_Components.Get(*params.m_UserData);
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
            if (params.m_Message->m_Id == dmModelDDF::ModelPlayAnimation::m_DDFDescriptor->m_NameHash)
            {
                dmModelDDF::ModelPlayAnimation* ddf = (dmModelDDF::ModelPlayAnimation*)params.m_Message->m_Data;
                if (dmRig::RESULT_OK == dmRig::PlayAnimation(component->m_RigInstance, ddf->m_AnimationId, (dmRig::RigPlayback)ddf->m_Playback, ddf->m_BlendDuration, ddf->m_Offset, ddf->m_PlaybackRate))
                {
                    component->m_Listener = params.m_Message->m_Sender;
                }
            }
            else if (params.m_Message->m_Id == dmModelDDF::ModelCancelAnimation::m_DDFDescriptor->m_NameHash)
            {
                dmRig::CancelAnimation(component->m_RigInstance);
            }
            else if (params.m_Message->m_Id == dmGameSystemDDF::SetConstant::m_DDFDescriptor->m_NameHash)
            {
                dmGameSystemDDF::SetConstant* ddf = (dmGameSystemDDF::SetConstant*)params.m_Message->m_Data;
                dmGameObject::PropertyResult result = dmGameSystem::SetMaterialConstant(component->m_Resource->m_Material, ddf->m_NameHash,
                        dmGameObject::PropertyVar(ddf->m_Value), CompModelSetConstantCallback, component);
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
            else if (params.m_Message->m_Id == dmGameSystemDDF::ResetConstant::m_DDFDescriptor->m_NameHash)
            {
                dmGameSystemDDF::ResetConstant* ddf = (dmGameSystemDDF::ResetConstant*)params.m_Message->m_Data;
                dmArray<dmRender::Constant>& constants = component->m_RenderConstants;
                uint32_t size = constants.Size();
                for (uint32_t i = 0; i < size; ++i)
                {
                    if( constants[i].m_NameHash == ddf->m_NameHash)
                    {
                        constants[i] = constants[size - 1];
                        component->m_PrevRenderConstants[i] = component->m_PrevRenderConstants[size - 1];
                        constants.Pop();
                        ReHash(component);
                        break;
                    }
                }
            }
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    static bool OnResourceReloaded(ModelWorld* world, ModelComponent* component)
    {
        dmRig::HRigContext rig_context = dmGameObject::GetRigContext(dmGameObject::GetCollection(component->m_Instance));

        // Destroy old rig
        dmRig::InstanceDestroyParams destroy_params = {0};
        destroy_params.m_Context = rig_context;
        destroy_params.m_Instance = component->m_RigInstance;
        dmRig::InstanceDestroy(destroy_params);

        // Create rig instance
        dmRig::InstanceCreateParams create_params = {0};
        create_params.m_Context = rig_context;
        create_params.m_Instance = &component->m_RigInstance;

        create_params.m_PoseCallback = CompModelPoseCallback;
        create_params.m_PoseCBUserData1 = component;
        create_params.m_PoseCBUserData2 = 0;
        create_params.m_EventCallback = CompModelEventCallback;
        create_params.m_EventCBUserData1 = component;
        create_params.m_EventCBUserData2 = 0;

        RigSceneResource* rig_resource = component->m_Resource->m_RigScene;
        create_params.m_BindPose         = &rig_resource->m_BindPose;
        create_params.m_AnimationSet     = rig_resource->m_AnimationSetRes == 0x0 ? 0x0 : rig_resource->m_AnimationSetRes->m_AnimationSet;
        create_params.m_Skeleton         = rig_resource->m_SkeletonRes == 0x0 ? 0x0 : rig_resource->m_SkeletonRes->m_Skeleton;
        create_params.m_MeshSet          = rig_resource->m_MeshSetRes->m_MeshSet;
        create_params.m_PoseIdxToInfluence = &rig_resource->m_PoseIdxToInfluence;
        create_params.m_TrackIdxToPose     = &rig_resource->m_TrackIdxToPose;
        create_params.m_MeshId           = 0; // not implemented for models
        create_params.m_DefaultAnimation = dmHashString64(component->m_Resource->m_Model->m_DefaultAnimation);

        dmRig::Result res = dmRig::InstanceCreate(create_params);
        if (res != dmRig::RESULT_OK) {
            dmLogError("Failed to create a rig instance needed by model: %d.", res);
            return false;
        }

        ReHash(component);

        // Create GO<->bone representation
        CreateGOBones(world, component);
        return true;
    }


    void CompModelOnReload(const dmGameObject::ComponentOnReloadParams& params)
    {
        ModelWorld* world = (ModelWorld*)params.m_World;
        ModelComponent* component = world->m_Components.Get(*params.m_UserData);
        component->m_Resource = (ModelResource*)params.m_Resource;
        (void)OnResourceReloaded(world, component);
    }

    dmGameObject::PropertyResult CompModelGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value)
    {
        ModelWorld* world = (ModelWorld*)params.m_World;
        ModelComponent* component = world->m_Components.Get(*params.m_UserData);
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

        for (uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
        {
            if (params.m_PropertyId == PROP_TEXTURE[i]) {
                out_value.m_Variant = dmGameObject::PropertyVar(component->m_Resource->m_TexturePaths[i]);
                return dmGameObject::PROPERTY_RESULT_OK;
            }
        }

        return GetMaterialConstant(component->m_Resource->m_Material, params.m_PropertyId, out_value, true, CompModelGetConstantCallback, component);
    }

    dmGameObject::PropertyResult CompModelSetProperty(const dmGameObject::ComponentSetPropertyParams& params)
    {
        ModelWorld* world = (ModelWorld*)params.m_World;
        ModelComponent* component = world->m_Components.Get(*params.m_UserData);
        if (params.m_PropertyId == PROP_SKIN)
        {
            if (params.m_Value.m_Type != dmGameObject::PROPERTY_TYPE_HASH)
                return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;

            dmRig::Result res = dmRig::SetMesh(component->m_RigInstance, params.m_Value.m_Hash);
            if (res == dmRig::RESULT_ERROR)
            {
                dmLogError("Could not find skin '%s' on the model.", dmHashReverseSafe64(params.m_Value.m_Hash));
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
                dmLogError("Could not set cursor %f on the model.", params.m_Value.m_Number);
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
                dmLogError("Could not set playback rate %f on the model.", params.m_Value.m_Number);
                return dmGameObject::PROPERTY_RESULT_UNSUPPORTED_VALUE;
            }
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        return SetMaterialConstant(component->m_Resource->m_Material, params.m_PropertyId, params.m_Value, CompModelSetConstantCallback, component);
    }

    static void ResourceReloadedCallback(const dmResource::ResourceReloadedParams& params)
    {
        ModelWorld* world = (ModelWorld*) params.m_UserData;
        dmArray<ModelComponent*>& components = world->m_Components.m_Objects;
        uint32_t n = components.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            ModelComponent* component = components[i];
            if (component->m_Resource)
            {
                if(component->m_Resource == params.m_Resource->m_Resource)
                {
                    // Model resource reload
                    OnResourceReloaded(world, component);
                    continue;
                }
                RigSceneResource *rig_scene_res = component->m_Resource->m_RigScene;
                if((rig_scene_res) && (rig_scene_res->m_AnimationSetRes == params.m_Resource->m_Resource))
                {
                    // Model resource reload because animset used in rig was reloaded
                    OnResourceReloaded(world, component);
                }
            }
        }
    }

    static Vector3 UpdateIKInstanceCallback(dmRig::IKTarget* ik_target)
    {
        ModelComponent* component = (ModelComponent*)ik_target->m_UserPtr;
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

        return (Vector3)dmGameObject::GetWorldPosition(target_instance);
    }

    bool CompModelSetIKTargetInstance(ModelComponent* component, dmhash_t constraint_id, float mix, dmhash_t instance_id)
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

    bool CompModelSetIKTargetPosition(ModelComponent* component, dmhash_t constraint_id, float mix, Point3 position)
    {
        dmRig::IKTarget* target = dmRig::GetIKTarget(component->m_RigInstance, constraint_id);
        if (!target) {
            return false;
        }
        target->m_Callback = 0x0;
        target->m_Mix = mix;
        target->m_Position = (Vector3)position;
        return true;
    }

}
