// Copyright 2020-2024 The Defold Foundation
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
#include <dmsdk/dlib/intersection.h>
#include <graphics/graphics.h>
#include <rig/rig.h>
#include <render/render.h>
#include <gameobject/gameobject_ddf.h>

#include "../gamesys.h"
#include "../gamesys_private.h"
#include "../resources/res_animationset.h"
#include "../resources/res_material.h"
#include "../resources/res_meshset.h"
#include "../resources/res_model.h"
#include "../resources/res_rig_scene.h"
#include "../resources/res_skeleton.h"
#include "../resources/res_texture.h"
#include "comp_private.h"

#include <gamesys/gamesys_ddf.h>
#include <gamesys/model_ddf.h>
#include <dmsdk/gamesys/render_constants.h>
#include <dmsdk/resource/resource.h>

DM_PROPERTY_EXTERN(rmtp_Components);
DM_PROPERTY_U32(rmtp_Model, 0, FrameReset, "# components", &rmtp_Components);
DM_PROPERTY_U32(rmtp_ModelIndexCount, 0, FrameReset, "# indices", &rmtp_Model);
DM_PROPERTY_U32(rmtp_ModelVertexCount, 0, FrameReset, "# vertices", &rmtp_Model);
DM_PROPERTY_U32(rmtp_ModelVertexSize, 0, FrameReset, "size of vertices in bytes", &rmtp_Model);

namespace dmGameSystem
{
    using namespace dmVMath;
    using namespace dmGameSystemDDF;

    static const uint16_t ATTRIBUTE_RENDER_DATA_INDEX_UNUSED = 0xffff;

    struct MeshAttributeRenderData
    {
        dmGraphics::HVertexBuffer      m_VertexBuffer;
        dmGraphics::HVertexDeclaration m_VertexDeclaration;
    };

    struct MeshRenderItem
    {
        dmVMath::Matrix4            m_World;
        dmVMath::Vector3            m_AabbMin;
        dmVMath::Vector3            m_AabbMax;
        struct ModelComponent*      m_Component;
        ModelResourceBuffers*       m_Buffers;
        dmRigDDF::Model*            m_Model;    // Used for world space materials
        dmRigDDF::Mesh*             m_Mesh;     // Used for world space materials
        uint32_t                    m_BoneIndex;
        uint32_t                    m_MaterialIndex;
        uint32_t                    m_Enabled : 1;
        uint32_t                    m_AttributeRenderDataIndex : 16;
        uint32_t                    : 15;
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
        TextureResource*            m_Textures[dmRender::RenderObject::MAX_TEXTURE_COUNT];
        MaterialResource*           m_Material; // Override material

        /// Node instances corresponding to the bones
        dmArray<dmGameObject::HInstance> m_NodeInstances;
        dmArray<MeshRenderItem>          m_RenderItems;
        dmArray<MeshAttributeRenderData> m_MeshAttributeRenderDatas;
        uint16_t                    m_ComponentIndex;
        uint8_t                     m_Enabled : 1;
        uint8_t                     m_DoRender : 1;
        uint8_t                     m_AddedToUpdate : 1;
        uint8_t                     m_ReHash : 1;
        uint8_t                     : 4;
    };

    struct ModelWorld
    {
        dmObjectPool<ModelComponent*>    m_Components;
        dmArray<dmRender::RenderObject>  m_RenderObjects;
        dmGraphics::HVertexDeclaration   m_VertexDeclaration;
        dmRender::HBufferedRenderBuffer* m_VertexBuffers;
        dmArray<uint8_t>*                m_VertexBufferData;
        uint32_t*                        m_VertexBufferVertexCounts;
        uint32_t*                        m_VertexBufferDispatchCounts;
        // Temporary scratch array for instances, only used during the creation phase of components
        dmArray<dmGameObject::HInstance> m_ScratchInstances;
        dmRig::HRigContext               m_RigContext;
        uint32_t                         m_MaxElementsVertices;
        uint32_t                         m_MaxBatchIndex;
    };

    static const uint32_t VERTEX_BUFFER_MAX_BATCHES = 16;     // Max dmRender::RenderListEntry.m_MinorOrder (4 bits)

    static const dmhash_t PROP_SKIN = dmHashString64("skin");
    static const dmhash_t PROP_ANIMATION = dmHashString64("animation");
    static const dmhash_t PROP_CURSOR = dmHashString64("cursor");
    static const dmhash_t PROP_PLAYBACK_RATE = dmHashString64("playback_rate");

    static const uint32_t MAX_TEXTURE_COUNT = dmRender::RenderObject::MAX_TEXTURE_COUNT;

    static void ResourceReloadedCallback(const dmResource::ResourceReloadedParams* params);
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
        // position, normal, tangent, color, texcoord0, texcoord1 * sizeof(float)
        DM_STATIC_ASSERT( sizeof(dmRig::RigModelVertex) == ((3+3+4+4+2+2)*4), Invalid_Struct_Size);

        dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(render_context);
        dmGraphics::HVertexStreamDeclaration stream_declaration = dmGraphics::NewVertexStreamDeclaration(graphics_context);
        dmGraphics::AddVertexStream(stream_declaration, "position",  3, dmGraphics::TYPE_FLOAT, false);
        dmGraphics::AddVertexStream(stream_declaration, "normal",    3, dmGraphics::TYPE_FLOAT, false);
        dmGraphics::AddVertexStream(stream_declaration, "tangent",   4, dmGraphics::TYPE_FLOAT, false);
        dmGraphics::AddVertexStream(stream_declaration, "color",     4, dmGraphics::TYPE_FLOAT, false);
        dmGraphics::AddVertexStream(stream_declaration, "texcoord0", 2, dmGraphics::TYPE_FLOAT, false);
        dmGraphics::AddVertexStream(stream_declaration, "texcoord1", 2, dmGraphics::TYPE_FLOAT, false);

        world->m_MaxBatchIndex = 0;
        world->m_VertexDeclaration = dmGraphics::NewVertexDeclaration(graphics_context, stream_declaration);
        world->m_MaxElementsVertices = dmGraphics::GetMaxElementsVertices(graphics_context);
        world->m_VertexBuffers = new dmRender::HBufferedRenderBuffer[VERTEX_BUFFER_MAX_BATCHES];
        world->m_VertexBufferData = new dmArray<uint8_t>[VERTEX_BUFFER_MAX_BATCHES];
        world->m_VertexBufferVertexCounts = new uint32_t[VERTEX_BUFFER_MAX_BATCHES];
        world->m_VertexBufferDispatchCounts = new uint32_t[VERTEX_BUFFER_MAX_BATCHES];

        for(uint32_t i = 0; i < VERTEX_BUFFER_MAX_BATCHES; ++i)
        {
            world->m_VertexBuffers[i] = dmRender::NewBufferedRenderBuffer(context->m_RenderContext, dmRender::RENDER_BUFFER_TYPE_VERTEX_BUFFER);
            world->m_VertexBufferDispatchCounts[i] = 0;
        }

        dmGraphics::DeleteVertexStreamDeclaration(stream_declaration);

        *params.m_World = world;

        dmResource::RegisterResourceReloadedCallback(context->m_Factory, ResourceReloadedCallback, world);

        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompModelDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        ModelContext* context = (ModelContext*)params.m_Context;
        ModelWorld* world = (ModelWorld*)params.m_World;
        dmGraphics::DeleteVertexDeclaration(world->m_VertexDeclaration);
        for(uint32_t i = 0; i < VERTEX_BUFFER_MAX_BATCHES; ++i)
        {
            dmRender::DeleteBufferedRenderBuffer(context->m_RenderContext, world->m_VertexBuffers[i]);
        }

        dmResource::UnregisterResourceReloadedCallback(((ModelContext*)params.m_Context)->m_Factory, ResourceReloadedCallback, world);

        dmRig::DeleteContext(world->m_RigContext);

        delete [] world->m_VertexBufferData;
        delete [] world->m_VertexBufferVertexCounts;
        delete [] world->m_VertexBufferDispatchCounts;
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

    static inline bool IsDefaultStream(dmhash_t name_hash, dmGraphics::VertexAttribute::SemanticType semantic_type)
    {
        return (name_hash == dmRender::VERTEX_STREAM_POSITION  && semantic_type == dmGraphics::VertexAttribute::SEMANTIC_TYPE_POSITION) ||
               (name_hash == dmRender::VERTEX_STREAM_NORMAL    && semantic_type == dmGraphics::VertexAttribute::SEMANTIC_TYPE_NORMAL)   ||
               (name_hash == dmRender::VERTEX_STREAM_TANGENT   && semantic_type == dmGraphics::VertexAttribute::SEMANTIC_TYPE_TANGENT)  ||
               (name_hash == dmRender::VERTEX_STREAM_COLOR     && semantic_type == dmGraphics::VertexAttribute::SEMANTIC_TYPE_COLOR)    ||
               (name_hash == dmRender::VERTEX_STREAM_TEXCOORD0 && semantic_type == dmGraphics::VertexAttribute::SEMANTIC_TYPE_TEXCOORD) ||
               (name_hash == dmRender::VERTEX_STREAM_TEXCOORD1 && semantic_type == dmGraphics::VertexAttribute::SEMANTIC_TYPE_TEXCOORD);
    }

    static inline MaterialResource* GetMaterialResource(const ModelComponent* component, const ModelResource* resource, uint32_t index) {
        // TODO: Add support for setting material on different indices
        return component->m_Material ? component->m_Material : resource->m_Materials[index].m_Material;
    }

    static inline dmRender::HMaterial GetComponentMaterial(const ModelComponent* component, const ModelResource* resource, uint32_t index) {
        return GetMaterialResource(component, resource, index)->m_Material;
    }

    static inline dmRender::HMaterial GetRenderMaterial(dmRender::HMaterial context_material, const ModelComponent* component, const ModelResource* resource, uint32_t index) {
        return context_material ? context_material : GetComponentMaterial(component, resource, index);
    }

    static TextureResource* GetTextureFromSamplerNameHash(const MaterialInfo* material_info, const MaterialResource* material, uint32_t material_texture_index, dmhash_t sampler_name_hash)
    {
        // Material is the actual selected material (may be overridden at runtime)

        // If the model has textures, let's find the one associated with the sampler name
        for (uint32_t i = 0; i < material_info->m_TexturesCount; ++i)
        {
            if (material_info->m_Textures[i].m_SamplerNameHash == sampler_name_hash)
                return material_info->m_Textures[i].m_Texture;
        }

        // If it is contains materials, use that
        if (material_texture_index < material->m_NumTextures)
            return material->m_Textures[material_texture_index];

        return 0;
    }

    // A bit legacy, as we don't want to set textures based on index anymore, but sampler names.
    static TextureResource* GetTextureResource(const ModelComponent* component, uint32_t material_index, uint32_t material_texture_index)
    {
        MaterialResource* material = GetMaterialResource(component, component->m_Resource, material_index);
        MaterialInfo* material_info = &component->m_Resource->m_Materials[material_index];

        TextureResource* texture_res = component->m_Textures[material_texture_index]; // Check if it's overridden
        if (!texture_res && material_texture_index < material_info->m_TexturesCount)
        {
            texture_res = material_info->m_Textures[material_texture_index].m_Texture; // Check if the model has a texture
        }
        if (!texture_res && material_texture_index < material->m_NumTextures)
        {
            texture_res = material->m_Textures[material_texture_index]; // Check if the material has a texture
        }
        return texture_res;
    }

    static void FillTextures(dmRender::RenderObject* ro, const ModelComponent* component, uint32_t material_index)
    {
        MaterialResource* material = GetMaterialResource(component, component->m_Resource, material_index);
        for(uint32_t i = 0; i < material->m_NumTextures; ++i)
        {
            TextureResource* texture_res = component->m_Textures[i];
            if (!texture_res)
            {
                texture_res = GetTextureFromSamplerNameHash(&component->m_Resource->m_Materials[material_index], material, i, material->m_SamplerNames[i]);
            }

            ro->m_Textures[i] = texture_res ? texture_res->m_Texture : 0;
        }
    }
    static void HashMaterial(HashState32* state, const dmGameSystem::MaterialResource* material)
    {
        dmHashUpdateBuffer32(state, &material->m_Material, sizeof(material->m_Material));
        dmHashUpdateBuffer32(state, material->m_Textures, sizeof(dmGameSystem::TextureResource*)*material->m_NumTextures);
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
            HashMaterial(&state, resource->m_Materials[i].m_Material);

            dmHashUpdateBuffer32(&state, resource->m_Materials[i].m_Textures, sizeof(dmGameSystem::MaterialTextureInfo)*resource->m_Materials[i].m_TexturesCount);
        }

        // The unused slots should be 0
        // Note: In the future, we want the textures so be set on a per-material basis
        dmHashUpdateBuffer32(&state, component->m_Textures, DM_ARRAY_SIZE(component->m_Textures));

        if (component->m_Material)
        {
            HashMaterial(&state, component->m_Material);
        }

        if (component->m_RenderConstants)
        {
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

    static bool HasCustomVertexAttributes(dmRender::HMaterial material)
    {
        const dmGraphics::VertexAttribute* attributes = 0;
        uint32_t attribute_count = 0;
        dmRender::GetMaterialProgramAttributes(material, &attributes, &attribute_count);
        for (int i = 0; i < attribute_count; ++i)
        {
            const dmGraphics::VertexAttribute& attr = attributes[i];
            if (!IsDefaultStream(attr.m_NameHash, attr.m_SemanticType))
            {
                return true;
            }
        }
        return false;
    }

    static void SetupMeshAttributeRenderData(dmRender::HRenderContext render_context, dmRender::HMaterial material, const MeshRenderItem* render_item, dmGraphics::VertexAttribute* model_attributes, uint32_t model_attribute_count, MeshAttributeRenderData* rd)
    {
        assert(!rd->m_VertexBuffer);
        assert(!rd->m_VertexDeclaration);

        dmGraphics::HContext graphics_context                   = dmRender::GetGraphicsContext(render_context);
        dmGraphics::HVertexStreamDeclaration stream_declaration = dmGraphics::NewVertexStreamDeclaration(graphics_context);
        dmGraphics::HVertexDeclaration material_vx_decl         = dmRender::GetVertexDeclaration(material);

        dmGraphics::VertexAttributeInfos material_infos;
        FillMaterialAttributeInfos(material, material_vx_decl, &material_infos);

        dmGraphics::VertexAttributeInfos attribute_infos;
        FillAttributeInfos(0, INVALID_DYNAMIC_ATTRIBUTE_INDEX, // Dynamic vertex attributes are not supported yet
                    model_attributes,
                    model_attribute_count,
                    &material_infos,
                    &attribute_infos);

        uint8_t* scratch_attribute_vertex = (uint8_t*) malloc(dmGraphics::GetVertexDeclarationStride(material_vx_decl));

        dmGraphics::VertexAttributeInfos non_default_attribute;
        non_default_attribute.m_VertexStride = 0;
        non_default_attribute.m_NumInfos     = 0;

        for (int i = 0; i < material_infos.m_NumInfos; ++i)
        {
            const dmGraphics::VertexAttributeInfo& attr_material = material_infos.m_Infos[i];
            const dmGraphics::VertexAttributeInfo& attr_model    = attribute_infos.m_Infos[i];

            if (!IsDefaultStream(attr_model.m_NameHash, attr_material.m_SemanticType))
            {
                assert(attr_model.m_NameHash == attr_material.m_NameHash);
                dmGraphics::AddVertexStream(stream_declaration,
                    attr_model.m_NameHash,
                    attr_material.m_ElementCount, // Need the material attribute here to get the _actual_ element count
                    dmGraphics::GetGraphicsType(attr_model.m_DataType),
                    attr_model.m_Normalize);

                uint32_t attribute_index = non_default_attribute.m_NumInfos++;
                non_default_attribute.m_Infos[attribute_index]                 = attr_model;
                non_default_attribute.m_Infos[attribute_index].m_ElementCount  = attr_material.m_ElementCount;
                non_default_attribute.m_VertexStride                          += attr_model.m_ValueByteSize;
            }
        }

        uint32_t vertex_count     = render_item->m_Buffers->m_VertexCount;
        uint32_t vertex_data_size = non_default_attribute.m_VertexStride * vertex_count;
        void* attribute_data      = malloc(vertex_data_size);
        memset(attribute_data, 0, vertex_data_size);
        uint8_t* vertex_write_ptr = (uint8_t*) attribute_data;

        const dmRigDDF::Mesh* mesh = render_item->m_Mesh;
        const float* positions     = mesh->m_Positions.m_Count ? mesh->m_Positions.m_Data : 0;
        const float* normals       = mesh->m_Normals.m_Count ? mesh->m_Normals.m_Data : 0;
        const float* tangents      = mesh->m_Tangents.m_Count ? mesh->m_Tangents.m_Data : 0;
        const float* colors        = mesh->m_Colors.m_Count ? mesh->m_Colors.m_Data : 0;
        const float* uv0           = mesh->m_Texcoord0.m_Count ? mesh->m_Texcoord0.m_Data : 0;
        const float* uv1           = mesh->m_Texcoord1.m_Count ? mesh->m_Texcoord1.m_Data : 0;

        for (int i = 0; i < vertex_count; ++i)
        {
            vertex_write_ptr = dmRig::WriteSingleVertexDataByAttributes(vertex_write_ptr, i, &non_default_attribute,
                positions, normals, tangents, uv0, uv1, colors);
        }

        rd->m_VertexBuffer      = dmGraphics::NewVertexBuffer(graphics_context, vertex_data_size, attribute_data, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
        rd->m_VertexDeclaration = dmGraphics::NewVertexDeclaration(graphics_context, stream_declaration);

        dmGraphics::DeleteVertexStreamDeclaration(stream_declaration);

        free(attribute_data);
        free(scratch_attribute_vertex);
    }

    static void SetupRenderItems(ModelComponent* component, ModelResource* resource)
    {
        component->m_RenderItems.SetCapacity(resource->m_Meshes.Size());
        component->m_RenderItems.SetSize(0);

        uint32_t num_custom_attributes = 0;

        dmHashTable64<uint32_t>* bone_id_to_indices = 0;
        if (resource->m_RigScene->m_SkeletonRes)
            bone_id_to_indices = &resource->m_RigScene->m_SkeletonRes->m_BoneIndices;

        for (uint32_t i = 0; i < resource->m_Meshes.Size(); ++i)
        {
            MeshRenderItem item;
            item.m_Buffers = resource->m_Meshes[i].m_Buffers;
            item.m_Component = component;
            item.m_Model = resource->m_Meshes[i].m_Model;
            item.m_Mesh = resource->m_Meshes[i].m_Mesh;
            item.m_MaterialIndex = resource->m_Meshes[i].m_Mesh->m_MaterialIndex;
            item.m_AabbMin = item.m_Mesh->m_AabbMin;
            item.m_AabbMax = item.m_Mesh->m_AabbMax;
            item.m_Enabled = 1;
            item.m_BoneIndex = dmRig::INVALID_BONE_INDEX;
            item.m_AttributeRenderDataIndex = ATTRIBUTE_RENDER_DATA_INDEX_UNUSED;

            // This model is a child under a bone, but isn't actually skinned
            if (item.m_Model->m_BoneId && bone_id_to_indices)
            {
                uint32_t* idx = bone_id_to_indices->Get(item.m_Model->m_BoneId);
                if (idx)
                {
                    item.m_BoneIndex = *idx;
                }
            }

            if (HasCustomVertexAttributes(GetComponentMaterial(component, component->m_Resource, item.m_MaterialIndex)))
            {
                item.m_AttributeRenderDataIndex = num_custom_attributes;
                num_custom_attributes++;
            }

            component->m_RenderItems.Push(item);
        }

        component->m_MeshAttributeRenderDatas.SetCapacity(num_custom_attributes);
        component->m_MeshAttributeRenderDatas.SetSize(num_custom_attributes);
        memset(component->m_MeshAttributeRenderDatas.Begin(), 0, component->m_MeshAttributeRenderDatas.Size() * sizeof(MeshAttributeRenderData));
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
        create_params.m_Skeleton         = rig_resource->m_SkeletonRes == 0x0 ? 0x0 : rig_resource->m_SkeletonRes->m_Skeleton;
        if (create_params.m_Skeleton)
        {
            create_params.m_BoneIndices      = rig_resource->m_SkeletonRes == 0x0 ? 0x0 : &rig_resource->m_SkeletonRes->m_BoneIndices;
            create_params.m_AnimationSet     = rig_resource->m_AnimationSetRes == 0x0 ? 0x0 : rig_resource->m_AnimationSetRes->m_AnimationSet;
        }
        else
        {
            if (rig_resource->m_AnimationSetRes)
            {
                dmLogWarning("Model has animations but no skeleton set");
            }
        }
        create_params.m_MeshSet = rig_resource->m_MeshSetRes->m_MeshSet;

        // Let's choose the first model for the animation
        create_params.m_ModelId          = 0; // Let's use all models
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
            ShowFullBufferError("Model", "model.max_count", world->m_Components.Capacity());
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

    void* CompModelGetComponent(const dmGameObject::ComponentGetParams& params)
    {
        ModelWorld* world = (ModelWorld*)params.m_World;
        return (void*)world->m_Components.Get(params.m_UserData);
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

    static inline void RenderBatchLocalVS(ModelWorld* world, dmRender::HRenderContext render_context, dmRender::HMaterial render_context_material, dmRender::RenderListEntry *buf, uint32_t* begin, uint32_t* end)
    {
        DM_PROFILE("RenderBatchLocal");

        bool render_context_material_custom_attributes = false;
        if (render_context_material)
        {
            render_context_material_custom_attributes = HasCustomVertexAttributes(render_context_material);
        }

        for (uint32_t *i=begin;i!=end;i++)
        {
            MeshRenderItem* render_item = (MeshRenderItem*) buf[*i].m_UserData;
            const ModelResourceBuffers* buffers = render_item->m_Buffers;
            ModelComponent* component = render_item->m_Component;
            uint32_t material_index = render_item->m_MaterialIndex;
            dmRender::HMaterial render_material = GetRenderMaterial(render_context_material, component, component->m_Resource, material_index);

            // We currently have no support for instancing, so we generate a separate draw call for each render item
            world->m_RenderObjects.SetSize(world->m_RenderObjects.Size()+1);
            dmRender::RenderObject& ro = world->m_RenderObjects.Back();

            ro.Init();
            ro.m_Material              = GetComponentMaterial(component, component->m_Resource, material_index);
            ro.m_PrimitiveType         = dmGraphics::PRIMITIVE_TRIANGLES;
            ro.m_VertexDeclarations[0] = world->m_VertexDeclaration;
            ro.m_VertexBuffers[0]      = buffers->m_VertexBuffer;

            if (render_context_material_custom_attributes || render_item->m_AttributeRenderDataIndex != ATTRIBUTE_RENDER_DATA_INDEX_UNUSED)
            {
                // The overridden material from the render script might be setup with custom vertex attributes,
                // while the component material might not. In this case, we need to setup the attribute render data
                // specifically for the render material.
                if (render_item->m_AttributeRenderDataIndex == ATTRIBUTE_RENDER_DATA_INDEX_UNUSED)
                {
                    render_item->m_AttributeRenderDataIndex = component->m_MeshAttributeRenderDatas.Size();
                    component->m_MeshAttributeRenderDatas.OffsetCapacity(1);
                    component->m_MeshAttributeRenderDatas.SetSize(component->m_MeshAttributeRenderDatas.Capacity());
                }

                MeshAttributeRenderData* attribute_rd = &component->m_MeshAttributeRenderDatas[render_item->m_AttributeRenderDataIndex];

                if (!attribute_rd->m_VertexDeclaration)
                {
                    SetupMeshAttributeRenderData(render_context,
                        render_material,
                        render_item,
                        component->m_Resource->m_Materials[material_index].m_Attributes,
                        component->m_Resource->m_Materials[material_index].m_AttributeCount,
                        attribute_rd);
                }

                ro.m_VertexDeclarations[1] = attribute_rd->m_VertexDeclaration;
                ro.m_VertexBuffers[1]      = attribute_rd->m_VertexBuffer;
            }

            // These should be named "element" or "index" (as opposed to vertex)
            ro.m_VertexStart    = 0;
            ro.m_VertexCount    = buffers->m_IndexCount;
            ro.m_WorldTransform = render_item->m_World;
            ro.m_IndexBuffer    = buffers->m_IndexBuffer;              // May be 0
            ro.m_IndexType      = buffers->m_IndexBufferElementType;

            DM_PROPERTY_ADD_U32(rmtp_ModelIndexCount, buffers->m_IndexCount);
            DM_PROPERTY_ADD_U32(rmtp_ModelVertexCount, buffers->m_VertexCount);
            DM_PROPERTY_ADD_U32(rmtp_ModelVertexSize, buffers->m_VertexCount * sizeof(dmRig::RigModelVertex));

            FillTextures(&ro, component, material_index);

            if (component->m_RenderConstants)
            {
                dmGameSystem::EnableRenderObjectConstants(&ro, component->m_RenderConstants);
            }

            dmRender::AddToRender(render_context, &ro);
        }
    }

    #if 0
    static void OutputVector4(const dmVMath::Vector4& v)
    {
        printf("%f, %f, %f, %f\n", v.getX(), v.getY(), v.getZ(), v.getW());
    }

    static void OutputMatrix(dmVMath::Matrix4 mat)
    {
        printf("    "); OutputVector4(mat.getRow(0));
        printf("    "); OutputVector4(mat.getRow(1));
        printf("    "); OutputVector4(mat.getRow(2));
        printf("    "); OutputVector4(mat.getRow(3));
    }
    static void OutputTransform(const dmTransform::Transform& transform)
    {
        printf("t: %f, %f, %f  \n", transform.GetTranslation().getX(), transform.GetTranslation().getY(), transform.GetTranslation().getZ());
        printf("r: %f, %f, %f, %f  \n", transform.GetRotation().getX(), transform.GetRotation().getY(), transform.GetRotation().getZ(), transform.GetRotation().getW());
        printf("s: %f, %f, %f  \n", transform.GetScale().getX(), transform.GetScale().getY(), transform.GetScale().getZ());
    }
    #endif

    static inline void RenderBatchWorldVS(ModelWorld* world, dmRender::HRenderContext render_context, dmRender::HMaterial render_context_material, dmRender::RenderListEntry *buf, uint32_t* begin, uint32_t* end)
    {
        DM_PROFILE("RenderBatchWorld");

        const MeshRenderItem* render_item      = (MeshRenderItem*) buf[*begin].m_UserData;
        const ModelComponent* component        = render_item->m_Component;
        uint32_t material_index                = render_item->m_MaterialIndex;
        dmRender::HMaterial material           = GetRenderMaterial(render_context_material, component, component->m_Resource, material_index);
        dmGraphics::HVertexDeclaration vx_decl = dmRender::GetVertexDeclaration(material);
        bool has_custom_attributes             = HasCustomVertexAttributes(material);

        dmGraphics::VertexAttributeInfos material_infos;
        FillMaterialAttributeInfos(material, vx_decl, &material_infos);

        uint32_t vertex_count = 0;
        uint32_t index_count = 0;
        uint32_t batchIndex = buf[*begin].m_MinorOrder;

        world->m_MaxBatchIndex = dmMath::Max(batchIndex, world->m_MaxBatchIndex);

        for (uint32_t *i=begin;i!=end;i++)
        {
            MeshRenderItem* render_item = (MeshRenderItem*) buf[*i].m_UserData;

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
        world->m_VertexBufferVertexCounts[batchIndex] = required_vertex_count;

        uint32_t vertex_stride = dmGraphics::GetVertexDeclarationStride(vx_decl);

        if (!has_custom_attributes)
        {
            vertex_stride = sizeof(dmRig::RigModelVertex);
            vx_decl       = world->m_VertexDeclaration;
        }

        uint32_t required_vertex_memory_count = required_vertex_count * vertex_stride;

        dmArray<uint8_t>& vertex_buffer = world->m_VertexBufferData[batchIndex];

        // We need to pad the buffer if the vertex stride doesn't start at an even byte offset from the start
        const uint32_t vb_buffer_offset = vertex_buffer.Size();
        uint32_t vb_buffer_padding      = 0;

        if (vb_buffer_offset % vertex_stride != 0)
        {
            required_vertex_memory_count += vertex_stride;
            vb_buffer_padding             = vertex_stride - vb_buffer_offset % vertex_stride;
        }

        if (vertex_buffer.Remaining() < required_vertex_memory_count)
        {
            vertex_buffer.OffsetCapacity(required_vertex_memory_count - vertex_buffer.Remaining());
        }

        dmRender::HBufferedRenderBuffer& gfx_vertex_buffer = world->m_VertexBuffers[batchIndex];

        if (dmRender::GetBufferIndex(render_context, gfx_vertex_buffer) < world->m_VertexBufferDispatchCounts[batchIndex])
        {
            dmRender::AddRenderBuffer(render_context, gfx_vertex_buffer);
        }

        // Fill in vertex buffer
        uint8_t* vb_begin = vertex_buffer.End() + vb_buffer_padding;
        uint8_t* vb_end = vb_begin;

        for (uint32_t *i=begin;i!=end;i++)
        {
            const MeshRenderItem* render_item = (MeshRenderItem*) buf[*i].m_UserData;
            const ModelComponent* c = render_item->m_Component;
            if (c->m_RigInstance)
            {
                dmArray<dmRig::BonePose>& pose = *dmRig::GetPose(c->m_RigInstance);

                dmVMath::Matrix4 model_matrix;
                if (render_item->m_BoneIndex != dmRig::INVALID_BONE_INDEX)
                {
                    dmRig::BonePose bone_pose = pose[render_item->m_BoneIndex];
                    model_matrix = dmTransform::ToMatrix4(bone_pose.m_World) * dmTransform::ToMatrix4(render_item->m_Model->m_Local);
                }
                else
                {
                    model_matrix = dmTransform::ToMatrix4(render_item->m_Model->m_Local);
                }

                dmVMath::Matrix4 world_matrix = c->m_World * model_matrix;

                // Either generate the vertices by using the attributes or the 'old' way.
                // This should mean that we won't take a performance hit if we don't use attributes.
                // If there is a render context material (overridden by using render.enable_material),
                // we need to cater for that as well.
                if (has_custom_attributes)
                {
                    dmGraphics::VertexAttributeInfos attribute_infos;
                    FillAttributeInfos(0, INVALID_DYNAMIC_ATTRIBUTE_INDEX, // Not supported yet
                        c->m_Resource->m_Model->m_Materials[material_index].m_Attributes.m_Data,
                        c->m_Resource->m_Model->m_Materials[material_index].m_Attributes.m_Count,
                        &material_infos,
                        &attribute_infos);

                    vb_end = dmRig::GenerateVertexDataFromAttributes(world->m_RigContext, c->m_RigInstance, render_item->m_Mesh, world_matrix, &attribute_infos, vertex_stride, vb_end);
                }
                else
                {
                    vb_end = (uint8_t*) dmRig::GenerateVertexData(world->m_RigContext, c->m_RigInstance, render_item->m_Mesh, world_matrix, (dmRig::RigModelVertex*) vb_end);
                }
            }
        }

        uint32_t vx_start = (vb_begin - vertex_buffer.Begin()) / vertex_stride;
        uint32_t vx_count = (vb_end - vb_begin) / vertex_stride;

        vertex_buffer.SetSize(vb_end - vertex_buffer.Begin());

        // Ninja in-place writing of render object.
        world->m_RenderObjects.SetSize(world->m_RenderObjects.Size()+1);
        dmRender::RenderObject& ro = world->m_RenderObjects[world->m_RenderObjects.Size()-1];

        ro.Init();
        ro.m_Material          = GetComponentMaterial(component, component->m_Resource, material_index);;
        ro.m_VertexDeclaration = vx_decl;
        ro.m_VertexBuffer      = (dmGraphics::HVertexBuffer) dmRender::GetBuffer(render_context, gfx_vertex_buffer);
        ro.m_PrimitiveType     = dmGraphics::PRIMITIVE_TRIANGLES;
        ro.m_VertexStart       = vx_start;
        ro.m_VertexCount       = vx_count;
        ro.m_WorldTransform    = Matrix4::identity(); // Pass identity world transform if outputing world positions directly.

        FillTextures(&ro, component, material_index);

        if (component->m_RenderConstants)
        {
            dmGameSystem::EnableRenderObjectConstants(&ro, component->m_RenderConstants);
        }

        dmRender::AddToRender(render_context, &ro);
    }

    static void RenderBatch(ModelWorld* world, dmRender::HRenderContext render_context, dmRender::RenderListEntry *buf, uint32_t* begin, uint32_t* end)
    {
        DM_PROFILE("ModelRenderBatch");

        const MeshRenderItem* render_item = (MeshRenderItem*) buf[*begin].m_UserData;
        const ModelComponent* component = render_item->m_Component;

        dmRender::HMaterial render_context_material = dmRender::GetContextMaterial(render_context);
        dmRender::HMaterial material = GetRenderMaterial(render_context_material, component, component->m_Resource, 0);

        switch(dmRender::GetMaterialVertexSpace(material))
        {
            case dmRenderDDF::MaterialDesc::VERTEX_SPACE_WORLD:
                RenderBatchWorldVS(world, render_context, render_context_material, buf, begin, end);
            break;

            case dmRenderDDF::MaterialDesc::VERTEX_SPACE_LOCAL:
                RenderBatchLocalVS(world, render_context, render_context_material, buf, begin, end);
            break;

            default:
                assert(false);
            break;
        }
    }

    static void UpdateMeshTransforms(ModelComponent* component)
    {
        dmVMath::Matrix4 world = component->m_World;
        dmArray<dmRig::BonePose>* pose = component->m_RigInstance ? dmRig::GetPose(component->m_RigInstance) : 0;

        for (uint32_t i = 0; i < component->m_RenderItems.Size(); ++i)
        {
            MeshRenderItem& item = component->m_RenderItems[i];
            if (!item.m_Enabled)
                continue;
            dmRigDDF::Model* model = item.m_Model;
            if (item.m_BoneIndex != dmRig::INVALID_BONE_INDEX)
            {
                dmRig::BonePose bone_pose = (*pose)[item.m_BoneIndex];
                item.m_World = world * (dmTransform::ToMatrix4(bone_pose.m_World) * dmTransform::ToMatrix4(model->m_Local));
            }
            else
            {
                item.m_World = world * dmTransform::ToMatrix4(model->m_Local);
            }
        }
    }

    // TODO: What are the dependencies here?
    // Why can we not call this in the CompModelUpdate() function?

    static void UpdateTransforms(ModelWorld* world)
    {
        DM_PROFILE(__FUNCTION__);

        const dmArray<ModelComponent*>& components = world->m_Components.GetRawObjects();
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
        ModelContext* context = (ModelContext*)params.m_Context;

        dmRig::Result rig_res = dmRig::Update(world->m_RigContext, params.m_UpdateContext->m_DT);

        const dmArray<ModelComponent*>& components = world->m_Components.GetRawObjects();
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

        assert(world->m_MaxBatchIndex < VERTEX_BUFFER_MAX_BATCHES);
        for (int i = 0; i <= world->m_MaxBatchIndex; ++i)
        {
            dmRender::TrimBuffer(context->m_RenderContext, world->m_VertexBuffers[i]);
            dmRender::RewindBuffer(context->m_RenderContext, world->m_VertexBuffers[i]);
            world->m_VertexBufferDispatchCounts[i] = 0;
        }

        world->m_MaxBatchIndex = 0;

        update_result.m_TransformsUpdated = rig_res == dmRig::RESULT_UPDATED_POSE;
        return dmGameObject::UPDATE_RESULT_OK;
    }

    static void RenderListFrustumCulling(dmRender::RenderListVisibilityParams const &params)
    {
        DM_PROFILE("Model");

        const dmIntersection::Frustum frustum = *params.m_Frustum;
        uint32_t num_entries = params.m_NumEntries;
        for (uint32_t i = 0; i < num_entries; ++i)
        {
            dmRender::RenderListEntry* entry = &params.m_Entries[i];
            MeshRenderItem* render_item = (MeshRenderItem*)entry->m_UserData;

            bool intersect = dmIntersection::TestFrustumOBB(frustum, render_item->m_World, render_item->m_AabbMin, render_item->m_AabbMax);
            entry->m_Visibility = intersect ? dmRender::VISIBILITY_FULL : dmRender::VISIBILITY_NONE;
        }
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
                    dmArray<uint8_t>& vertex_buffer_data = world->m_VertexBufferData[batch_index];
                    if (vertex_buffer_data.Empty())
                    {
                        continue;
                    }

                    uint32_t vb_size = vertex_buffer_data.Size();
                    dmRender::HBufferedRenderBuffer& gfx_vertex_buffer = world->m_VertexBuffers[batch_index];
                    dmRender::SetBufferData(params.m_Context, gfx_vertex_buffer, vb_size, vertex_buffer_data.Begin(), dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
                    world->m_VertexBufferDispatchCounts[batch_index]++;

                    total_count += world->m_VertexBufferVertexCounts[batch_index];
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

        // This is currently called after the rig update
        UpdateTransforms(world); // TODO: Why can't we move this to the CompModelUpdate()?

        const dmArray<ModelComponent*>& components = world->m_Components.GetRawObjects();

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
        dmRender::HRenderListDispatch dispatch = dmRender::RenderListMakeDispatch(render_context, &RenderListDispatch, &RenderListFrustumCulling, world);
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
                if (!render_item.m_Enabled)
                    continue;

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
                write_ptr->m_TagListKey = dmRender::GetMaterialTagListKey(GetComponentMaterial(&component, component.m_Resource, render_item.m_MaterialIndex));
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
        SetRenderConstant(component->m_RenderConstants, GetComponentMaterial(component, component->m_Resource, 0), name_hash, value_index, element_index, var);
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
                dmGameObject::PropertyResult result = dmGameSystem::SetMaterialConstant(GetComponentMaterial(component, component->m_Resource, 0), ddf->m_NameHash,
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
            return GetResourceProperty(dmGameObject::GetFactory(params.m_Instance), GetMaterialResource(component, component->m_Resource, 0), out_value);
        }
        for (uint32_t i = 0; i < MAX_TEXTURE_COUNT; ++i)
        {
            if (params.m_PropertyId == PROP_TEXTURE[i])
            {
                return GetResourceProperty(dmGameObject::GetFactory(params.m_Instance), GetTextureResource(component, 0, i), out_value);
            }
        }
        return GetMaterialConstant(GetComponentMaterial(component, component->m_Resource, 0), params.m_PropertyId, params.m_Options.m_Index, out_value, true, CompModelGetConstantCallback, component);
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
                dmhash_t ext_hashes[] = { TEXTURE_EXT_HASH, RENDER_TARGET_EXT_HASH };
                dmGameObject::PropertyResult res = SetResourceProperty(dmGameObject::GetFactory(params.m_Instance), params.m_Value, ext_hashes, DM_ARRAY_SIZE(ext_hashes), (void**)&component->m_Textures[i]);
                component->m_ReHash |= res == dmGameObject::PROPERTY_RESULT_OK;
                return res;
            }
        }
        return SetMaterialConstant(GetComponentMaterial(component, component->m_Resource, 0), params.m_PropertyId, params.m_Value, params.m_Options.m_Index, CompModelSetConstantCallback, component);
    }

    static void ResourceReloadedCallback(const dmResource::ResourceReloadedParams* params)
    {
        ModelWorld* world = (ModelWorld*) params->m_UserData;
        const dmArray<ModelComponent*>& components = world->m_Components.GetRawObjects();
        uint32_t n = components.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            ModelComponent* component = components[i];
            if (component->m_Resource)
            {
                if(component->m_Resource == dmResource::GetResource(params->m_Resource))
                {
                    // Model resource reload
                    OnResourceReloaded(world, component, i);
                    continue;
                }
                RigSceneResource *rig_scene_res = component->m_Resource->m_RigScene;
                if((rig_scene_res) && (rig_scene_res->m_AnimationSetRes == dmResource::GetResource(params->m_Resource)))
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

    bool CompModelSetMeshEnabled(ModelComponent* component, dmhash_t mesh_id, bool enabled)
    {
        bool found = false;
        for (uint32_t i = 0; i < component->m_RenderItems.Size(); ++i)
        {
            MeshRenderItem& item = component->m_RenderItems[i];
            if (item.m_Model->m_Id == mesh_id)
            {
                item.m_Enabled = enabled?1:0;
                found = true;
            }
        }
        return found;
    }

    bool CompModelGetMeshEnabled(ModelComponent* component, dmhash_t mesh_id, bool* out)
    {
        bool found = false;
        for (uint32_t i = 0; i < component->m_RenderItems.Size(); ++i)
        {
            MeshRenderItem& item = component->m_RenderItems[i];
            if (item.m_Model->m_Id == mesh_id)
            {
                *out = item.m_Enabled != 0;
                return true;
            }
        }
        return found;
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

    // For tests
    void GetModelWorldRenderBuffers(void* model_world, dmRender::HBufferedRenderBuffer** vx_buffers, uint32_t* vx_buffers_count)
    {
        ModelWorld* world = (ModelWorld*) model_world;
        *vx_buffers       = world->m_VertexBuffers;
        *vx_buffers_count = VERTEX_BUFFER_MAX_BATCHES;
    }
}
