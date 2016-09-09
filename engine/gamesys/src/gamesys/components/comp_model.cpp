#include "comp_model.h"

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

#include "model_ddf.h"
#include "sprite_ddf.h"
#include "tile_ddf.h"

using namespace Vectormath::Aos;

namespace dmGameSystem
{
    using namespace Vectormath::Aos;
    using namespace dmGameSystemDDF;

    static const dmhash_t PROP_SKIN = dmHashString64("skin");
    static const dmhash_t PROP_ANIMATION = dmHashString64("animation");

    static void ResourceReloadedCallback(const dmResource::ResourceReloadedParams& params);
    static void DestroyComponent(ModelWorld* world, uint32_t index);

    dmGameObject::CreateResult CompModelNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        ModelContext* context = (ModelContext*)params.m_Context;
        dmRender::HRenderContext render_context = context->m_RenderContext;
        ModelWorld* world = new ModelWorld();

        world->m_Components.SetCapacity(context->m_MaxModelCount);
        world->m_RigContext = context->m_RigContext;
        world->m_RenderObjects.SetCapacity(context->m_MaxModelCount);

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

    static void CompModelEventCallback(void* user_data, const dmRig::RigEventType event_type, const void* event_data)
    {
        ModelComponent* component = (ModelComponent*)user_data;

        dmMessage::URL sender;
        dmMessage::URL receiver = component->m_Listener;
        if (!GetSender(component, &sender))
        {
            dmLogError("Could not send animation_done to listener because of incomplete component.");
            return;
        }
/*
        switch (event_data.m_Type) {
            case dmRig::RIG_EVENT_TYPE_DONE: {
                    dmhash_t message_id = dmModelDDF::m_DDFDescriptor->m_NameHash;

                    dmModelDDF::ModelAnimationDone message;
                    message.m_AnimationId = event_data.m_DataDone.m_AnimationId;
                    message.m_Playback    = event_data.m_DataDone.m_Playback;

                    uintptr_t descriptor = (uintptr_t)dmModelDDF::ModelAnimationDone::m_DDFDescriptor;
                    uint32_t data_size = sizeof(dmModelDDF::ModelAnimationDone);
                    dmMessage::Result result = dmMessage::Post(&sender, &receiver, message_id, 0, descriptor, &message, data_size);
                    dmMessage::ResetURL(component->m_Listener);
                    if (result != dmMessage::RESULT_OK)
                    {
                        dmLogError("Could not send animation_done to listener.");
                    }

                } break;
            case dmRig::RIG_EVENT_TYPE_MODEL: {

                    if (!dmMessage::IsSocketValid(receiver.m_Socket))
                    {
                        receiver = sender;
                        receiver.m_Fragment = 0; // broadcast to sibling components
                    }

                    dmhash_t message_id = dmGameSystemDDF::ModelEvent::m_DDFDescriptor->m_NameHash;

                    dmModelDDF::ModelEvent event;
                    event.m_EventId     = event_data.m_DataModel.m_EventId;
                    event.m_AnimationId = event_data.m_DataModel.m_AnimationId;
                    event.m_BlendWeight = event_data.m_DataModel.m_BlendWeight;
                    event.m_T           = event_data.m_DataModel.m_T;
                    event.m_Integer     = event_data.m_DataModel.m_Integer;
                    event.m_Float       = event_data.m_DataModel.m_Float;
                    event.m_String      = event_data.m_DataModel.m_String;

                    uintptr_t descriptor = (uintptr_t)dmModelDDF::ModelEvent::m_DDFDescriptor;
                    uint32_t data_size = sizeof(dmGameSystemDDF::ModelEvent);
                    dmMessage::Result result = dmMessage::Post(&sender, &receiver, message_id, 0, descriptor, &event, data_size);
                    if (result != dmMessage::RESULT_OK)
                    {
                        dmLogError("Could not send event to listener.");
                    }
                } break;
            default:
                dmLogError("Unknown rig event received (%d).", event_data.m_Type);
                break;
        }
        */

    }

    static void CompModelPoseCallback(void* user_data)
    {
        ModelComponent* component = (ModelComponent*)user_data;

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
        dmHashUpdateBuffer32(&state, &resource->m_RigScene->m_TextureSet, sizeof(resource->m_RigScene->m_TextureSet));
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
        create_params.m_Context = world->m_RigContext;
        create_params.m_Instance = &component->m_RigInstance;

        create_params.m_PoseCallback = CompModelPoseCallback;
        create_params.m_EventCallback = CompModelEventCallback;
        create_params.m_EventCBUserData = component;

        RigSceneResource* rig_resource = component->m_Resource->m_RigScene;
        create_params.m_BindPose         = &rig_resource->m_BindPose;
        create_params.m_Skeleton         = rig_resource->m_SkeletonRes->m_Skeleton;
        create_params.m_MeshSet          = rig_resource->m_MeshSetRes->m_MeshSet;
        create_params.m_AnimationSet     = rig_resource->m_AnimationSetRes->m_AnimationSet;
        create_params.m_MeshId           = 0; //dmHashString64(component->m_Resource->m_Model->m_Skin);
        create_params.m_DefaultAnimation = 0; // dmHashString64(component->m_Resource->m_Model->m_DefaultAnimation);

        dmRig::Result res = dmRig::InstanceCreate(create_params);
        if (res != dmRig::RESULT_OK) {
            dmLogError("Failed to create a rig instance needed by model: %d.", res);
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }

        ReHash(component);

        // Create GO<->bone representation
        // CreateGOBones(world, component);

        *params.m_UserData = (uintptr_t)index;
        return dmGameObject::CREATE_RESULT_OK;
    }

    static void DestroyComponent(ModelWorld* world, uint32_t index)
    {
        ModelComponent* component = world->m_Components.Get(index);
        // If we're going to use memset, then we should explicitly clear pose and instance arrays.
        component->m_NodeInstances.SetCapacity(0);

        dmRig::InstanceDestroyParams params = {0};
        params.m_Context = world->m_RigContext;
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

        // TextureSetResource* texture_set = first->m_Resource->m_RigScene->m_TextureSet;

        uint32_t vertex_count = 0;
        for (uint32_t *i=begin;i!=end;i++)
        {
            const ModelComponent* c = (ModelComponent*) buf[*i].m_UserData;
            vertex_count += dmRig::GetVertexCount(c->m_RigInstance);
        }
        assert(vertex_count > 0);

        dmArray<ModelVertex> &vertex_buffer = world->m_VertexBufferData;
        if (vertex_buffer.Remaining() < vertex_count)
            vertex_buffer.OffsetCapacity(vertex_count - vertex_buffer.Remaining());

        // Fill in vertex buffer
        ModelVertex *vb_begin = vertex_buffer.End();
        ModelVertex *vb_end = vb_begin;
        dmRig::RigGenVertexDataParams params;
        for (uint32_t *i=begin;i!=end;i++)
        {
            const ModelComponent* c = (ModelComponent*) buf[*i].m_UserData;
            params.m_ModelMatrix = Matrix4::identity();
            params.m_VertexData = (void**)&vb_end;
            params.m_VertexStride = sizeof(ModelVertex);

            vb_end = (ModelVertex *)dmRig::GenerateVertexData(world->m_RigContext, c->m_RigInstance, params);
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
        // ro.m_Textures[0] = texture_set->m_Texture;
        for (uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
            ro.m_Textures[i] = first->m_Resource->m_Textures[i];

        const dmArray<dmRender::Constant>& constants = first->m_RenderConstants;
        uint32_t size = constants.Size();
        for (uint32_t i = 0; i < size; ++i)
        {
            const dmRender::Constant& c = constants[i];
            dmRender::EnableRenderObjectConstant(&ro, c.m_NameHash, c.m_Value);
        }


        ro.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
        ro.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
/*
        dmModelDDF::ModelDesc::BlendMode blend_mode = first->m_Resource->m_Model->m_BlendMode;
        switch (blend_mode)
        {
            case dmGameSystemDDF::ModelDesc::BLEND_MODE_ALPHA:
                ro.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
                ro.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            break;

            case dmGameSystemDDF::ModelDesc::BLEND_MODE_ADD:
                ro.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
                ro.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
            break;

            case dmGameSystemDDF::ModelDesc::BLEND_MODE_MULT:
                ro.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_DST_COLOR;
                ro.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            break;

            default:
                dmLogError("Unknown blend mode: %d\n", blend_mode);
                assert(0);
            break;
        }
*/
        ro.m_SetBlendFactors = 1;

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
                dmArray<ModelVertex>& vertex_buffer = world->m_VertexBufferData;
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
                dmGraphics::SetVertexBufferData(world->m_VertexBuffer, sizeof(ModelVertex) * world->m_VertexBufferData.Size(),
                                                world->m_VertexBufferData.Begin(), dmGraphics::BUFFER_USAGE_STATIC_DRAW);
                DM_COUNTER("ModelVertexBuffer", world->m_VertexBufferData.Size() * sizeof(ModelVertex));
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
/*
        else if (params.m_Message->m_Descriptor != 0x0)
        {
            if (params.m_Message->m_Id == dmModelDDF::PlayAnimation::m_DDFDescriptor->m_NameHash)
            {
                dmModelDDF::ModelPlayAnimation* ddf = (dmModelDDF::PlayAnimation*)params.m_Message->m_Data;
                if (dmRig::RESULT_OK == dmRig::PlayAnimation(component->m_RigInstance, ddf->m_AnimationId, (dmGameObject::Playback)ddf->m_Playback, ddf->m_BlendDuration))
                {
                    component->m_Listener = params.m_Message->m_Sender;
                }
            }
            else if (params.m_Message->m_Id == dmModelDDF::CancelAnimation::m_DDFDescriptor->m_NameHash)
            {
                dmRig::CancelAnimation(component->m_RigInstance);
            }


            else if (params.m_Message->m_Id == dmModelDDF::SetConstantModel::m_DDFDescriptor->m_NameHash)
            {
                dmGameSystemDDF::SetConstantModel* ddf = (dmGameSystemDDF::SetConstantModel*)params.m_Message->m_Data;
                dmGameObject::PropertyResult result = dmGameSystem::SetMaterialConstant(component->m_Resource->m_Material, ddf->m_NameHash,
                        dmGameObject::PropertyVar(ddf->m_Value), CompModelSetConstantCallback, component);
                if (result == dmGameObject::PROPERTY_RESULT_NOT_FOUND)
                {
                    dmMessage::URL& receiver = params.m_Message->m_Receiver;
                    dmLogError("'%s:%s#%s' has no constant named '%s'",
                            dmMessage::GetSocketName(receiver.m_Socket),
                            (const char*)dmHashReverse64(receiver.m_Path, 0x0),
                            (const char*)dmHashReverse64(receiver.m_Fragment, 0x0),
                            (const char*)dmHashReverse64(ddf->m_NameHash, 0x0));
                }
            }
            else if (params.m_Message->m_Id == dmGameSystemDDF::ResetConstantModel::m_DDFDescriptor->m_NameHash)
            {
                dmGameSystemDDF::ResetConstantModel* ddf = (dmGameSystemDDF::ResetConstantModel*)params.m_Message->m_Data;
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
*/
        return dmGameObject::UPDATE_RESULT_OK;
    }

    static bool OnResourceReloaded(ModelWorld* world, ModelComponent* component)
    {
        // Destroy old rig
        dmRig::InstanceDestroyParams destroy_params = {0};
        destroy_params.m_Context = world->m_RigContext;
        destroy_params.m_Instance = component->m_RigInstance;
        dmRig::InstanceDestroy(destroy_params);

        // Create rig instance
        dmRig::InstanceCreateParams create_params = {0};
        create_params.m_Context = world->m_RigContext;
        create_params.m_Instance = &component->m_RigInstance;

        create_params.m_PoseCallback = CompModelPoseCallback;
        create_params.m_EventCallback = CompModelEventCallback;
        create_params.m_EventCBUserData = component;

        RigSceneResource* rig_resource = component->m_Resource->m_RigScene;
        create_params.m_BindPose         = &rig_resource->m_BindPose;
        create_params.m_Skeleton         = rig_resource->m_SkeletonRes->m_Skeleton;
        create_params.m_MeshSet          = rig_resource->m_MeshSetRes->m_MeshSet;
        create_params.m_AnimationSet     = rig_resource->m_AnimationSetRes->m_AnimationSet;
        create_params.m_MeshId           = 0; //dmHashString64(component->m_Resource->m_Model->m_Skin);
        create_params.m_DefaultAnimation = 0; //dmHashString64(component->m_Resource->m_Model->m_DefaultAnimation);

        dmRig::Result res = dmRig::InstanceCreate(create_params);
        if (res != dmRig::RESULT_OK) {
            dmLogError("Failed to create a rig instance needed by model: %d.", res);
            return false;
        }

        ReHash(component);

        // Create GO<->bone representation
        // CreateGOBones(world, component);
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
        return GetMaterialConstant(component->m_Resource->m_Material, params.m_PropertyId, out_value, CompModelGetConstantCallback, component);
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
                dmLogError("Could not find skin '%s' on the model.", (const char*)dmHashReverse64(params.m_Value.m_Hash, 0x0));
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
            if (component->m_Resource != 0x0 && component->m_Resource->m_RigScene == params.m_Resource->m_Resource)
                OnResourceReloaded(world, component);
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
            dmLogError("Could not get IK position for target %s, removed?", (const char*)dmHashReverse64(target_instance_id, 0x0))
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
