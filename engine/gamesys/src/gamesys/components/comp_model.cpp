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
#include <dmsdk/dlib/vmath.h>
#include <graphics/graphics.h>
#include <rig/rig.h>
#include <render/render.h>
#include <gameobject/gameobject_ddf.h>

#include "../gamesys.h"
#include "../gamesys_private.h"
#include "../resources/res_model.h"
#include "../resources/res_rig_scene.h"
#include "../resources/res_skeleton.h"
#include "../resources/res_animationset.h"
#include "../resources/res_meshset.h"
#include "comp_private.h"

#include <gamesys/gamesys_ddf.h>
#include <gamesys/model_ddf.h>
#include <dmsdk/gamesys/render_constants.h>

DM_PROPERTY_EXTERN(rmtp_Components);
DM_PROPERTY_U32(rmtp_Model, 0, FrameReset, "# components", &rmtp_Components);
DM_PROPERTY_U32(rmtp_ModelVertexCount, 0, FrameReset, "# vertices", &rmtp_Model);
DM_PROPERTY_U32(rmtp_ModelVertexSize, 0, FrameReset, "size of vertices in bytes", &rmtp_Model);

namespace dmGameSystem
{
    using namespace dmVMath;
    using namespace dmGameSystemDDF;

    struct MeshRenderItem
    {
        dmVMath::Matrix4            m_World;
        struct ModelComponent*      m_Component;
        ModelResourceBuffers*       m_Buffers;
        dmRigDDF::Model*            m_Model;
        dmRigDDF::Mesh*             m_Mesh;
        uint32_t                    m_MaterialIndex : 4; // current max 16 materials per model
        uint32_t                    m_Enabled : 1;
        uint32_t                    : 27;
    };

    struct ModelComponent
    {
        dmGameObject::HInstance     m_Instance;
        dmTransform::Transform      m_Transform;
        Matrix4                     m_World;
        ModelResource*              m_Resource;
        dmRig::HRigInstance         m_RigInstance;
        uint32_t                    m_MixedHash;
        dmMessage::URL              m_Listener;
        int                         m_FunctionRef;
        HComponentRenderConstants   m_RenderConstants;
        dmGraphics::HTexture        m_Textures[dmRender::RenderObject::MAX_TEXTURE_COUNT];
        dmRender::HMaterial         m_Material;

        /// Node instances corresponding to the bones
        dmArray<dmGameObject::HInstance> m_NodeInstances;
        dmArray<MeshRenderItem>     m_RenderItems;
        uint16_t                    m_ComponentIndex;
        uint8_t                     m_Enabled : 1;
        uint8_t                     m_DoRender : 1;
        uint8_t                     m_AddedToUpdate : 1;
        uint8_t                     m_ReHash : 1;
        uint8_t                     : 4;
    };

    struct ModelWorld
    {
        dmObjectPool<ModelComponent*>   m_Components;
        dmArray<dmRender::RenderObject> m_RenderObjects;
        dmGraphics::HVertexDeclaration  m_VertexDeclaration;
        dmGraphics::HVertexBuffer*      m_VertexBuffers;
        dmArray<dmRig::RigModelVertex>* m_VertexBufferData;
        // Temporary scratch array for instances, only used during the creation phase of components
        dmArray<dmGameObject::HInstance> m_ScratchInstances;
        dmRig::HRigContext              m_RigContext;
        uint32_t                        m_MaxElementsVertices;
        uint32_t                        m_VertexBufferSwapChainIndex;
        uint32_t                        m_VertexBufferSwapChainSize;
    };

    static const uint32_t VERTEX_BUFFER_MAX_BATCHES = 16;     // Max dmRender::RenderListEntry.m_MinorOrder (4 bits)

    static const dmhash_t PROP_SKIN = dmHashString64("skin");
    static const dmhash_t PROP_ANIMATION = dmHashString64("animation");
    static const dmhash_t PROP_CURSOR = dmHashString64("cursor");
    static const dmhash_t PROP_PLAYBACK_RATE = dmHashString64("playback_rate");

    static const uint32_t MAX_TEXTURE_COUNT = dmRender::RenderObject::MAX_TEXTURE_COUNT;

    static void ResourceReloadedCallback(const dmResource::ResourceReloadedParams& params);
    static void DestroyComponent(ModelWorld* world, uint32_t index);

    dmGameObject::CreateResult CompModelNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        ModelContext* context = (ModelContext*)params.m_Context;
        dmRender::HRenderContext render_context = context->m_RenderContext;
        ModelWorld* world = new ModelWorld();
        uint32_t comp_count = dmMath::Min(params.m_MaxComponentInstances, context->m_MaxModelCount);

        dmRig::NewContextParams rig_params = {0};
        rig_params.m_MaxRigInstanceCount = comp_count;
        dmRig::Result rr = dmRig::NewContext(rig_params, &world->m_RigContext);
        if (rr != dmRig::RESULT_OK)
        {
            dmLogFatal("Unable to create model rig context: %d", rr);
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }

        world->m_Components.SetCapacity(comp_count);
        world->m_RenderObjects.SetCapacity(comp_count);

        dmGraphics::VertexElement ve[] =
        {
                {"position", 0, 3, dmGraphics::TYPE_FLOAT, false},
                {"normal", 1, 3, dmGraphics::TYPE_FLOAT, false},
                {"tangent", 2, 3, dmGraphics::TYPE_FLOAT, false},
                {"color", 3, 4, dmGraphics::TYPE_FLOAT, false},
                {"texcoord0", 4, 2, dmGraphics::TYPE_FLOAT, false},
                {"texcoord1", 5, 2, dmGraphics::TYPE_FLOAT, false},
        };
        DM_STATIC_ASSERT( sizeof(dmRig::RigModelVertex) == ((3+3+3+4+2+2)*4), Invalid_Struct_Size);

        dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(render_context);
        world->m_VertexDeclaration = dmGraphics::NewVertexDeclaration(graphics_context, ve, sizeof(ve) / sizeof(dmGraphics::VertexElement));
        world->m_MaxElementsVertices = dmGraphics::GetMaxElementsVertices(graphics_context);
        world->m_VertexBuffers = new dmGraphics::HVertexBuffer[VERTEX_BUFFER_MAX_BATCHES];
        world->m_VertexBufferData = new dmArray<dmRig::RigModelVertex>[VERTEX_BUFFER_MAX_BATCHES];
        for(uint32_t i = 0; i < VERTEX_BUFFER_MAX_BATCHES; ++i)
        {
            world->m_VertexBuffers[i] = dmGraphics::NewVertexBuffer(graphics_context, 0, 0x0, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
        }

        *params.m_World = world;

        dmResource::RegisterResourceReloadedCallback(context->m_Factory, ResourceReloadedCallback, world);

        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompModelDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        ModelWorld* world = (ModelWorld*)params.m_World;
        dmGraphics::DeleteVertexDeclaration(world->m_VertexDeclaration);
        for(uint32_t i = 0; i < VERTEX_BUFFER_MAX_BATCHES; ++i)
        {
            dmGraphics::DeleteVertexBuffer(world->m_VertexBuffers[i]);
        }

        dmResource::UnregisterResourceReloadedCallback(((ModelContext*)params.m_Context)->m_Factory, ResourceReloadedCallback, world);

        dmRig::DeleteContext(world->m_RigContext);

        delete [] world->m_VertexBufferData;
        delete [] world->m_VertexBuffers;

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
                dmMessage::Result result = dmMessage::Post(&sender, &receiver, message_id, 0, component->m_FunctionRef, descriptor, &message, data_size, 0);
                dmMessage::ResetURL(&component->m_Listener);
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
        dmArray<dmRig::BonePose>& pose = *dmRig::GetPose(component->m_RigInstance);

        if (!pose.Empty()) {
            uint32_t bone_count = pose.Size();
            dmArray<dmTransform::Transform> transforms;
            transforms.SetCapacity(bone_count);
            transforms.SetSize(bone_count);
            for (uint32_t i = 0; i < bone_count; ++i)
            {
                transforms[i] = pose[i].m_Local;
            }

            dmGameObject::SetBoneTransforms(component->m_NodeInstances[0], component->m_Transform, transforms.Begin(), transforms.Size());
        }
    }

    static inline dmRender::HMaterial GetMaterial(const ModelComponent* component, const ModelResource* resource, uint32_t index) {
        if (component->m_Material)
            return component->m_Material; // TODO: Add support for setting material on different indices
        return resource->m_Materials[index];
    }

    static inline dmGraphics::HTexture GetTexture(const ModelComponent* component, const ModelResource* resource, uint32_t index) {
        assert(index < MAX_TEXTURE_COUNT);
        return component->m_Textures[index] ? component->m_Textures[index] : resource->m_Textures[index];
    }

    static void ReHash(ModelComponent* component)
    {
        // material, textures and render constants
        HashState32 state;
        bool reverse = false;
        ModelResource* resource = component->m_Resource;
        dmHashInit32(&state, reverse);
// TODO: We need to create a hash for each mesh entry!
// TODO: Each skinned instance has its own state (pose is determined by animation, play rate, blending, time)
//  If they _do_ have the same state, we might use that fact so that they can batch together,
//  and we can later use instancing?
        for (uint32_t i = 0; i < resource->m_Materials.Size(); ++i)
        {
            dmRender::HMaterial material = GetMaterial(component, resource, i);
            dmHashUpdateBuffer32(&state, &material, sizeof(material));
        }
        // We have to hash individually since we don't know which textures are set as properties
        for (uint32_t i = 0; i < MAX_TEXTURE_COUNT; ++i) {
            dmGraphics::HTexture texture = GetTexture(component, resource, i);
            dmHashUpdateBuffer32(&state, &texture, sizeof(texture));
        }
        if (component->m_RenderConstants) {
            dmGameSystem::HashRenderConstants(component->m_RenderConstants, &state);
        }
        component->m_MixedHash = dmHashFinal32(&state);
        component->m_ReHash = 0;
    }

    static bool CreateGOBones(ModelWorld* world, ModelComponent* component)
    {
        if(!component->m_Resource->m_RigScene->m_SkeletonRes)
            return true;

        dmGameObject::HInstance instance = component->m_Instance;
        dmGameObject::HCollection collection = dmGameObject::GetCollection(instance);

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

            dmTransform::Transform transform;
            transform.SetIdentity(); // TODO: Get the first frame of the playing animation?
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

    static void SetupRenderItems(ModelComponent* component, ModelResource* resource)
    {
        component->m_RenderItems.SetCapacity(resource->m_Meshes.Size());
        component->m_RenderItems.SetSize(0);
        for (uint32_t i = 0; i < resource->m_Meshes.Size(); ++i)
        {
            MeshRenderItem item;
            item.m_Buffers = resource->m_Meshes[i].m_Buffers;
            item.m_Component = component;
            item.m_Model = resource->m_Meshes[i].m_Model;
            item.m_Mesh = resource->m_Meshes[i].m_Mesh;
            item.m_MaterialIndex = resource->m_Meshes[i].m_Mesh->m_MaterialIndex;
            item.m_Enabled = 1;
            component->m_RenderItems.Push(item);
        }
    }

    static dmGameObject::CreateResult SetupRigInstance(dmRig::HRigContext rig_context, ModelComponent* component, RigSceneResource* rig_resource, dmhash_t animation)
    {
        dmRig::InstanceCreateParams create_params = {0};

        create_params.m_PoseCallback = CompModelPoseCallback;
        create_params.m_PoseCBUserData1 = component;
        create_params.m_PoseCBUserData2 = 0;
        create_params.m_EventCallback = CompModelEventCallback;
        create_params.m_EventCBUserData1 = component;
        create_params.m_EventCBUserData2 = 0;

        create_params.m_BindPose         = &rig_resource->m_BindPose;
        create_params.m_AnimationSet     = rig_resource->m_AnimationSetRes == 0x0 ? 0x0 : rig_resource->m_AnimationSetRes->m_AnimationSet;
        create_params.m_Skeleton         = rig_resource->m_SkeletonRes == 0x0 ? 0x0 : rig_resource->m_SkeletonRes->m_Skeleton;
        create_params.m_MeshSet          = rig_resource->m_MeshSetRes->m_MeshSet;

        dmRigDDF::MeshSet* mesh_set      = rig_resource->m_MeshSetRes->m_MeshSet;
        // Let's choose the first model for the animation
        dmRigDDF::Model* model           = mesh_set->m_Models.m_Count > 0 ? &mesh_set->m_Models[0] : 0;
        create_params.m_ModelId          = model ? model->m_Id : 0;
        create_params.m_DefaultAnimation = animation;

        dmRig::Result res = dmRig::InstanceCreate(rig_context, create_params, &component->m_RigInstance);
        if (res != dmRig::RESULT_OK) {
            dmLogError("Failed to create a rig instance needed by model: %d.", res);
            if (res == dmRig::RESULT_ERROR_BUFFER_FULL) {
                dmLogError("Try increasing the model.max_count value in game.project");
            }
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
        return dmGameObject::CREATE_RESULT_OK;
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
        ModelResource* resource = (ModelResource*)params.m_Resource;
        component->m_Resource = resource;
        dmMessage::ResetURL(&component->m_Listener);

        component->m_ComponentIndex = params.m_ComponentIndex;
        component->m_Enabled = 1;
        component->m_World = Matrix4::identity();
        component->m_DoRender = 0;
        component->m_FunctionRef = 0;
        component->m_RenderConstants = 0;

        // Create GO<->bone representation
        // We need to make sure that bone GOs are created before we start the default animation.
        if (!CreateGOBones(world, component))
        {
            dmLogError("Failed to create game objects for bones in model. Consider increasing collection max instances (collection.max_instances).");
            DestroyComponent(world, index);
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }

        // Create rig instance
        component->m_RigInstance = 0;

        dmGameObject::CreateResult res = SetupRigInstance(world->m_RigContext, component, resource->m_RigScene, dmHashString64(resource->m_Model->m_DefaultAnimation));
        if (res != dmGameObject::CREATE_RESULT_OK)
        {
            DestroyComponent(world, index);
            return res;
        }

        SetupRenderItems(component, resource);

        component->m_ReHash = 1;

        *params.m_UserData = (uintptr_t)index;
        return dmGameObject::CREATE_RESULT_OK;
    }

    static void DestroyComponent(ModelWorld* world, uint32_t index)
    {
        ModelComponent* component = world->m_Components.Get(index);
        dmGameObject::DeleteBones(component->m_Instance);
        // If we're going to use memset, then we should explicitly clear pose and instance arrays.
        component->m_NodeInstances.SetCapacity(0);

        if (component->m_RigInstance)
        {
            dmRig::InstanceDestroy(world->m_RigContext, component->m_RigInstance);
        }

        if (component->m_RenderConstants) {
            dmGameSystem::DestroyRenderConstants(component->m_RenderConstants);
        }

        delete component;
        world->m_Components.Free(index, true);
    }

    dmGameObject::CreateResult CompModelDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        ModelWorld* world = (ModelWorld*)params.m_World;
        uint32_t index = *params.m_UserData;
        ModelComponent* component = world->m_Components.Get(index);
        dmResource::HFactory factory = dmGameObject::GetFactory(params.m_Instance);
        if (component->m_Material) {
            dmResource::Release(factory, component->m_Material);
        }
        for (uint32_t i = 0; i < MAX_TEXTURE_COUNT; ++i) {
            if (component->m_Textures[i]) {
                dmResource::Release(factory, component->m_Textures[i]);
            }
        }
        DestroyComponent(world, index);
        return dmGameObject::CREATE_RESULT_OK;
    }

    static inline void RenderBatchLocalVS(ModelWorld* world, dmRender::HMaterial material, dmRender::HRenderContext render_context, dmRender::RenderListEntry *buf, uint32_t* begin, uint32_t* end)
    {
        DM_PROFILE("RenderBatchLocal");

        for (uint32_t *i=begin;i!=end;i++)
        {
            const MeshRenderItem* render_item = (MeshRenderItem*) buf[*i].m_UserData;
            const ModelResourceBuffers* buffers = render_item->m_Buffers;
            const ModelComponent* component = render_item->m_Component;
            uint32_t material_index = render_item->m_MaterialIndex;

            // We currently have no support for instancing, so we generate a separate draw call for each render item
            world->m_RenderObjects.SetSize(world->m_RenderObjects.Size()+1);
            dmRender::RenderObject& ro = world->m_RenderObjects.Back();

            ro.Init();
            ro.m_VertexDeclaration = world->m_VertexDeclaration;
            ro.m_VertexBuffer = buffers->m_VertexBuffer;
            ro.m_Material = GetMaterial(component, component->m_Resource, material_index);
            ro.m_PrimitiveType = dmGraphics::PRIMITIVE_TRIANGLES;

            // These should be named "element" or "index" (as opposed to vertex)
            ro.m_VertexStart = 0;
            ro.m_VertexCount = buffers->m_IndexCount;

            ro.m_WorldTransform = render_item->m_World;

            ro.m_IndexBuffer = buffers->m_IndexBuffer;              // May be 0
            ro.m_IndexType = buffers->m_IndexBufferElementType;

            for(uint32_t i = 0; i < MAX_TEXTURE_COUNT; ++i)
            {
                ro.m_Textures[i] = GetTexture(component, component->m_Resource, i);
            }

            if (component->m_RenderConstants) {
                dmGameSystem::EnableRenderObjectConstants(&ro, component->m_RenderConstants);
            }

            dmRender::AddToRender(render_context, &ro);
        }
    }

    static inline void RenderBatchWorldVS(ModelWorld* world, dmRender::HMaterial material, dmRender::HRenderContext render_context, dmRender::RenderListEntry *buf, uint32_t* begin, uint32_t* end)
    {
        DM_PROFILE("RenderBatchWorld");

        uint32_t vertex_count = 0;
        uint32_t index_count = 0;
        uint32_t batchIndex = buf[*begin].m_MinorOrder;

        for (uint32_t *i=begin;i!=end;i++)
        {
            const MeshRenderItem* render_item = (MeshRenderItem*) buf[*i].m_UserData;

            uint32_t count = render_item->m_Buffers->m_VertexCount;
            uint32_t icount = render_item->m_Buffers->m_IndexCount;

            vertex_count += count;
            index_count += icount;
        }

        // Early exit if there is nothing to render
        if (vertex_count == 0 || index_count == 0) {
            return;
        }

        // Since we currently don't support index buffer, we need to produce the indexed vertices
        uint32_t required_vertex_count = dmMath::Max(vertex_count, index_count);

        dmArray<dmRig::RigModelVertex>& vertex_buffer = world->m_VertexBufferData[batchIndex];
        if (vertex_buffer.Remaining() < required_vertex_count)
            vertex_buffer.OffsetCapacity(required_vertex_count - vertex_buffer.Remaining());

        dmGraphics::HVertexBuffer& gfx_vertex_buffer = world->m_VertexBuffers[batchIndex];

        // Fill in vertex buffer
        dmRig::RigModelVertex *vb_begin = vertex_buffer.End();
        dmRig::RigModelVertex *vb_end = vb_begin;
        for (uint32_t *i=begin;i!=end;i++)
        {
            const MeshRenderItem* render_item = (MeshRenderItem*) buf[*i].m_UserData;
            const ModelComponent* c = render_item->m_Component;
            dmRig::HRigContext rig_context = world->m_RigContext;
            if (c->m_RigInstance)
            {
                dmVMath::Matrix4 model_matrix = dmTransform::ToMatrix4(render_item->m_Model->m_Local);
                dmVMath::Matrix4 world_matrix = c->m_World * model_matrix;

                vb_end = dmRig::GenerateVertexData(rig_context, c->m_RigInstance, render_item->m_Mesh, world_matrix, vb_end);
            }
        }
        vertex_buffer.SetSize(vb_end - vertex_buffer.Begin());

        // Ninja in-place writing of render object.
        world->m_RenderObjects.SetSize(world->m_RenderObjects.Size()+1);
        dmRender::RenderObject& ro = world->m_RenderObjects[world->m_RenderObjects.Size()-1];

        const MeshRenderItem* render_item = (MeshRenderItem*) buf[*begin].m_UserData;
        const ModelComponent* component = render_item->m_Component;
        uint32_t material_index = render_item->m_MaterialIndex;

        ro.Init();
        ro.m_VertexDeclaration = world->m_VertexDeclaration;
        ro.m_VertexBuffer = gfx_vertex_buffer;
        ro.m_PrimitiveType = dmGraphics::PRIMITIVE_TRIANGLES;
        ro.m_VertexStart = vb_begin - vertex_buffer.Begin();
        ro.m_VertexCount = vb_end - vb_begin;
        ro.m_Material = GetMaterial(component, component->m_Resource, material_index);
        ro.m_WorldTransform = Matrix4::identity(); // Pass identity world transform if outputing world positions directly.

        for(uint32_t i = 0; i < MAX_TEXTURE_COUNT; ++i)
        {
            ro.m_Textures[i] = GetTexture(component, component->m_Resource, i);
        }

        if (component->m_RenderConstants) {
            dmGameSystem::EnableRenderObjectConstants(&ro, component->m_RenderConstants);
        }

        dmRender::AddToRender(render_context, &ro);
    }

    static void RenderBatch(ModelWorld* world, dmRender::HRenderContext render_context, dmRender::RenderListEntry *buf, uint32_t* begin, uint32_t* end)
    {
        DM_PROFILE("ModelRenderBatch");

        const MeshRenderItem* render_item = (MeshRenderItem*) buf[*begin].m_UserData;
        const ModelComponent* component = render_item->m_Component;
        dmRender::HMaterial material = GetMaterial(component, component->m_Resource, 0);

        switch(dmRender::GetMaterialVertexSpace(material))
        {
            case dmRenderDDF::MaterialDesc::VERTEX_SPACE_WORLD:
                RenderBatchWorldVS(world, material, render_context, buf, begin, end);
            break;

            case dmRenderDDF::MaterialDesc::VERTEX_SPACE_LOCAL:
                RenderBatchLocalVS(world, material, render_context, buf, begin, end);
            break;

            default:
                assert(false);
            break;
        }
    }

    static void UpdateMeshTransforms(ModelComponent* component)
    {
        dmVMath::Matrix4 world = component->m_World;
        for (uint32_t i = 0; i < component->m_RenderItems.Size(); ++i)
        {
            MeshRenderItem& item = component->m_RenderItems[i];
            dmRigDDF::Model* model = item.m_Model;
            item.m_World = world * dmTransform::ToMatrix4(model->m_Local);
        }
    }

    // TODO: What are the dependencies here?
    // Why can we not call this in the CompModelUpdate() function?

    static void UpdateTransforms(ModelWorld* world)
    {
        DM_PROFILE(__FUNCTION__);

        dmArray<ModelComponent*>& components = world->m_Components.m_Objects;
        uint32_t n = components.Size();
        uint32_t num_render_items = 0;
        for (uint32_t i = 0; i < n; ++i)
        {
            ModelComponent* c = components[i];

            // NOTE: texture_set = c->m_Resource might be NULL so it's essential to "continue" here
            if (!c->m_Enabled || !c->m_AddedToUpdate)
                continue;

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

            UpdateMeshTransforms(c);

            num_render_items += c->m_RenderItems.Size();
        }

        if (world->m_RenderObjects.Capacity() < num_render_items)
            world->m_RenderObjects.SetCapacity(num_render_items);
    }

    dmGameObject::CreateResult CompModelAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params)
    {
        ModelWorld* world = (ModelWorld*)params.m_World;
        uint32_t index = (uint32_t)*params.m_UserData;
        ModelComponent* component = world->m_Components.Get(index);
        component->m_AddedToUpdate = true;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompModelUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
    {
        ModelWorld* world = (ModelWorld*)params.m_World;

        dmRig::Result rig_res = dmRig::Update(world->m_RigContext, params.m_UpdateContext->m_DT);

        dmArray<ModelComponent*>& components = world->m_Components.m_Objects;
        const uint32_t count = components.Size();

        for (uint32_t i = 0; i < count; ++i)
        {
            ModelComponent& component = *components[i];
            component.m_DoRender = 0;

            if (!component.m_Enabled || !component.m_AddedToUpdate)
                continue;

            if (component.m_ReHash || (component.m_RenderConstants && dmGameSystem::AreRenderConstantsUpdated(component.m_RenderConstants)))
            {
                ReHash(&component);
            }

            component.m_DoRender = 1;

            DM_PROPERTY_ADD_U32(rmtp_Model, 1);
        }

        update_result.m_TransformsUpdated = rig_res == dmRig::RESULT_UPDATED_POSE;
        return dmGameObject::UPDATE_RESULT_OK;
    }

    static void RenderListDispatch(dmRender::RenderListDispatchParams const &params)
    {
        ModelWorld *world = (ModelWorld *) params.m_UserData;

        switch (params.m_Operation)
        {
            case dmRender::RENDER_LIST_OPERATION_BEGIN:
            {
                world->m_RenderObjects.SetSize(0);
                for (uint32_t batch_index = 0; batch_index < VERTEX_BUFFER_MAX_BATCHES; ++batch_index)
                {
                    world->m_VertexBufferData[batch_index].SetSize(0);
                }
                break;
            }
            case dmRender::RENDER_LIST_OPERATION_BATCH:
            {
                RenderBatch(world, params.m_Context, params.m_Buf, params.m_Begin, params.m_End);
                break;
            }
            case dmRender::RENDER_LIST_OPERATION_END:
            {
                uint32_t total_count = 0;
                for (uint32_t batch_index = 0; batch_index < VERTEX_BUFFER_MAX_BATCHES; ++batch_index)
                {
                    dmArray<dmRig::RigModelVertex>& vertex_buffer_data = world->m_VertexBufferData[batch_index];
                    if (vertex_buffer_data.Empty())
                    {
                        continue;
                    }
                    uint32_t vb_size = sizeof(dmRig::RigModelVertex) * vertex_buffer_data.Size();
                    dmGraphics::HVertexBuffer& gfx_vertex_buffer = world->m_VertexBuffers[batch_index];
                    dmGraphics::SetVertexBufferData(gfx_vertex_buffer, vb_size, vertex_buffer_data.Begin(), dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
                    total_count += vertex_buffer_data.Size();
                }

                DM_PROPERTY_ADD_U32(rmtp_ModelVertexCount, total_count);
                DM_PROPERTY_ADD_U32(rmtp_ModelVertexSize, total_count * sizeof(dmRig::RigModelVertex));

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

        UpdateTransforms(world); // TODO: Why can't we move this to the CompModelUpdate()?

        dmArray<ModelComponent*>& components = world->m_Components.m_Objects;

        uint32_t num_components = components.Size();
        uint32_t mesh_count = 0;
        for (uint32_t i = 0; i < num_components; ++i)
        {
            ModelComponent& component = *components[i];
            if (!component.m_DoRender)
                continue;
            mesh_count += component.m_RenderItems.Size();
        }

        // Prepare list submit
        dmRender::RenderListEntry* render_list = dmRender::RenderListAlloc(render_context, mesh_count);
        dmRender::HRenderListDispatch dispatch = dmRender::RenderListMakeDispatch(render_context, &RenderListDispatch, world);
        dmRender::RenderListEntry* write_ptr = render_list;

        const uint32_t max_elements_vertices = world->m_MaxElementsVertices;
        uint32_t minor_order = 0; // Will translate to vb index. (When we're generating vertex buffers on the fly, if material vertex space == world space)
        uint32_t vertex_count_total = 0;
        for (uint32_t i = 0; i < num_components; ++i)
        {
            ModelComponent& component = *components[i];
            if (!component.m_DoRender)
                continue;

            uint32_t item_count = component.m_RenderItems.Size();
            for (uint32_t j = 0; j < item_count; ++j)
            {
                MeshRenderItem& render_item = component.m_RenderItems[j];

                uint32_t vertex_count = render_item.m_Buffers->m_VertexCount;
                if(vertex_count_total + vertex_count >= max_elements_vertices)
                {
                    vertex_count_total = 0;
                    minor_order = dmMath::Min(minor_order + 1, VERTEX_BUFFER_MAX_BATCHES-1);
                }
                vertex_count_total += vertex_count;

                const Vector4 trans = render_item.m_World.getCol(3);
                write_ptr->m_WorldPosition = Point3(trans.getX(), trans.getY(), trans.getZ());

                write_ptr->m_UserData = (uintptr_t) &render_item;
                // TODO: Currently assuming only one material for all meshes
                write_ptr->m_BatchKey = component.m_MixedHash;
                write_ptr->m_TagListKey = dmRender::GetMaterialTagListKey(GetMaterial(&component, component.m_Resource, render_item.m_MaterialIndex));
                write_ptr->m_Dispatch = dispatch;
                write_ptr->m_MinorOrder = minor_order;
                write_ptr->m_MajorOrder = dmRender::RENDER_ORDER_WORLD;
                ++write_ptr;
            }
        }

        dmRender::RenderListSubmit(render_context, render_list, write_ptr);
        return dmGameObject::UPDATE_RESULT_OK;
    }

    static bool CompModelGetConstantCallback(void* user_data, dmhash_t name_hash, dmRender::Constant** out_constant)
    {
        ModelComponent* component = (ModelComponent*)user_data;
        if (!component->m_RenderConstants)
            return false;
        return GetRenderConstant(component->m_RenderConstants, name_hash, out_constant);
    }

    static void CompModelSetConstantCallback(void* user_data, dmhash_t name_hash, int32_t value_index, uint32_t* element_index, const dmGameObject::PropertyVar& var)
    {
        ModelComponent* component = (ModelComponent*)user_data;
        if (!component->m_RenderConstants)
            component->m_RenderConstants = dmGameSystem::CreateRenderConstants();
        SetRenderConstant(component->m_RenderConstants, GetMaterial(component, component->m_Resource, 0), name_hash, value_index, element_index, var);
        component->m_ReHash = 1;
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

                dmRig::Result rig_result = dmRig::PlayAnimation(component->m_RigInstance, ddf->m_AnimationId, (dmRig::RigPlayback)ddf->m_Playback, ddf->m_BlendDuration, ddf->m_Offset, ddf->m_PlaybackRate);
                if (dmRig::RESULT_OK == rig_result)
                {
                    component->m_Listener = params.m_Message->m_Sender;
                    component->m_FunctionRef = params.m_Message->m_UserData2;
                } else if (dmRig::RESULT_ANIM_NOT_FOUND == rig_result) {
                    dmMessage::URL& receiver = params.m_Message->m_Receiver;
                    dmLogError("'%s:%s#%s' has no animation named '%s'",
                            dmMessage::GetSocketName(receiver.m_Socket),
                            dmHashReverseSafe64(receiver.m_Path),
                            dmHashReverseSafe64(receiver.m_Fragment),
                            dmHashReverseSafe64(ddf->m_AnimationId));
                }
            }
            else if (params.m_Message->m_Id == dmModelDDF::ModelCancelAnimation::m_DDFDescriptor->m_NameHash)
            {
                dmRig::CancelAnimation(component->m_RigInstance);
            }
            else if (params.m_Message->m_Id == dmGameSystemDDF::SetConstant::m_DDFDescriptor->m_NameHash)
            {
                dmGameSystemDDF::SetConstant* ddf = (dmGameSystemDDF::SetConstant*)params.m_Message->m_Data;
                dmGameObject::PropertyResult result = dmGameSystem::SetMaterialConstant(GetMaterial(component, component->m_Resource, 0), ddf->m_NameHash,
                        dmGameObject::PropertyVar(ddf->m_Value), ddf->m_Index, CompModelSetConstantCallback, component);
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
                if (component->m_RenderConstants && dmGameSystem::ClearRenderConstant(component->m_RenderConstants, ddf->m_NameHash))
                {
                    component->m_ReHash = 1;
                }
            }
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    static bool OnResourceReloaded(ModelWorld* world, ModelComponent* component, int index)
    {
        // Destroy old rig
        if (component->m_RigInstance != 0)
        {
            dmRig::InstanceDestroy(world->m_RigContext, component->m_RigInstance);
        }

        // Delete old bones, recreate with new data.
        // Make sure that bone GOs are created before we start the default animation.
        dmGameObject::DeleteBones(component->m_Instance);
        if (!CreateGOBones(world, component))
        {
            dmLogError("Failed to create game objects for bones in model. Consider increasing collection max instances (collection.max_instances).");
            DestroyComponent(world, index);
            return false;
        }

        // Create rig instance
        component->m_RigInstance = 0;

        ModelResource* resource = component->m_Resource;
        dmGameObject::CreateResult res = SetupRigInstance(world->m_RigContext, component, resource->m_RigScene, dmHashString64(resource->m_Model->m_DefaultAnimation));
        if (res != dmGameObject::CREATE_RESULT_OK)
        {
            DestroyComponent(world, index);
            return false;
        }

        SetupRenderItems(component, component->m_Resource);

        component->m_ReHash = 1;

        return true;
    }


    void CompModelOnReload(const dmGameObject::ComponentOnReloadParams& params)
    {
        ModelWorld* world = (ModelWorld*)params.m_World;
        int index = *params.m_UserData;
        ModelComponent* component = world->m_Components.Get(index);
        component->m_Resource = (ModelResource*)params.m_Resource;
        (void)OnResourceReloaded(world, component, index);
    }

    dmGameObject::PropertyResult CompModelGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value)
    {
        ModelWorld* world = (ModelWorld*)params.m_World;
        ModelComponent* component = world->m_Components.Get(*params.m_UserData);
        if (params.m_PropertyId == PROP_SKIN)
        {
            out_value.m_Variant = dmGameObject::PropertyVar(dmRig::GetModel(component->m_RigInstance));
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
            return GetResourceProperty(dmGameObject::GetFactory(params.m_Instance), GetMaterial(component, component->m_Resource, 0), out_value);
        }
        for (uint32_t i = 0; i < MAX_TEXTURE_COUNT; ++i)
        {
            if (params.m_PropertyId == PROP_TEXTURE[i])
            {
                return GetResourceProperty(dmGameObject::GetFactory(params.m_Instance), GetTexture(component, component->m_Resource, i), out_value);
            }
        }
        return GetMaterialConstant(GetMaterial(component, component->m_Resource, 0), params.m_PropertyId, params.m_Options.m_Index, out_value, true, CompModelGetConstantCallback, component);
    }

    dmGameObject::PropertyResult CompModelSetProperty(const dmGameObject::ComponentSetPropertyParams& params)
    {
        ModelWorld* world = (ModelWorld*)params.m_World;
        ModelComponent* component = world->m_Components.Get(*params.m_UserData);
        if (params.m_PropertyId == PROP_SKIN)
        {
            if (params.m_Value.m_Type != dmGameObject::PROPERTY_TYPE_HASH)
                return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;

            dmRig::Result res = dmRig::SetModel(component->m_RigInstance, params.m_Value.m_Hash);
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
        else if (params.m_PropertyId == PROP_MATERIAL)
        {
            dmGameObject::PropertyResult res = SetResourceProperty(dmGameObject::GetFactory(params.m_Instance), params.m_Value, MATERIAL_EXT_HASH, (void**)&component->m_Material);
            component->m_ReHash |= res == dmGameObject::PROPERTY_RESULT_OK;
            return res;
        }
        for(uint32_t i = 0; i < MAX_TEXTURE_COUNT; ++i)
        {
            if(params.m_PropertyId == PROP_TEXTURE[i])
            {
                dmGameObject::PropertyResult res = SetResourceProperty(dmGameObject::GetFactory(params.m_Instance), params.m_Value, TEXTURE_EXT_HASH, (void**)&component->m_Textures[i]);
                component->m_ReHash |= res == dmGameObject::PROPERTY_RESULT_OK;
                return res;
            }
        }
        return SetMaterialConstant(GetMaterial(component, component->m_Resource, 0), params.m_PropertyId, params.m_Value, params.m_Options.m_Index, CompModelSetConstantCallback, component);
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
                    OnResourceReloaded(world, component, i);
                    continue;
                }
                RigSceneResource *rig_scene_res = component->m_Resource->m_RigScene;
                if((rig_scene_res) && (rig_scene_res->m_AnimationSetRes == params.m_Resource->m_Resource))
                {
                    // Model resource reload because animset used in rig was reloaded
                    OnResourceReloaded(world, component, i);
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

    ModelResource* CompModelGetModelResource(ModelComponent* component)
    {
        return component->m_Resource;
    }

    dmGameObject::HInstance CompModelGetNodeInstance(ModelComponent* component, uint32_t bone_index)
    {
        return component->m_NodeInstances[bone_index];
    }

    ModelComponent* CompModelGetComponent(ModelWorld* world, uintptr_t user_data)
    {
        return world->m_Components.Get(user_data);;
    }

    static bool CompModelIterPropertiesGetNext(dmGameObject::SceneNodePropertyIterator* pit)
    {
        ModelWorld* world = (ModelWorld*)pit->m_Node->m_ComponentWorld;
        ModelComponent* component = world->m_Components.Get(pit->m_Node->m_Component);

        uint64_t index = pit->m_Next++;

        uint32_t num_bool_properties = 1;
        if (index < num_bool_properties)
        {
            if (index == 0)
            {
                pit->m_Property.m_Type = dmGameObject::SCENE_NODE_PROPERTY_TYPE_BOOLEAN;
                pit->m_Property.m_Value.m_Bool = component->m_Enabled;
                pit->m_Property.m_NameHash = dmHashString64("enabled");
            }
            return true;
        }
        index -= num_bool_properties;

        return false;
    }

    void CompModelIterProperties(dmGameObject::SceneNodePropertyIterator* pit, dmGameObject::SceneNode* node)
    {
        assert(node->m_Type == dmGameObject::SCENE_NODE_TYPE_COMPONENT);
        assert(node->m_ComponentType != 0);
        pit->m_Node = node;
        pit->m_Next = 0;
        pit->m_FnIterateNext = CompModelIterPropertiesGetNext;
    }
}
