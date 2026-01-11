// Copyright 2020-2026 The Defold Foundation
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

#include "res_rig_scene.h"
#include "res_model.h"
#include "res_meshset.h"
#include "res_material.h"
#include "res_render_target.h"

#include "gamesys_private.h"

#include <gamesys/model_ddf.h>

#include <dlib/log.h>
#include <dlib/path.h>
#include <dlib/dstrings.h>
#include <dlib/memory.h>
#include <graphics/graphics.h>
#include <render/render.h>
#include <dmsdk/dlib/transform.h>
#include <rig/rig.h>
#include <algorithm> // std::sort

namespace dmGameSystem
{
    static void ReleaseResources(dmResource::HFactory factory, ModelResource* resource);

    // TODO: Sorting+flattening the structure could be done in the pipeline
    struct MeshSortPred
    {
        inline bool operator() (const MeshInfo& p1, const MeshInfo& p2)
        {
            return p1.m_Mesh->m_MaterialIndex < p2.m_Mesh->m_MaterialIndex;
        }
    };

    // We could sort them in the pipeline, but then we'd have to read the mesh data
    // for compiling each model which we might not want
    struct MaterialSortPred
    {
        dmRigDDF::MeshSet* m_MeshSet;
        MaterialSortPred(dmRigDDF::MeshSet* mesh_set) : m_MeshSet(mesh_set) {}
        int FindMaterial(const char* s)
        {
            for (int i = 0; i < m_MeshSet->m_Materials.m_Count; ++i)
            {
                if (strcmp(s, m_MeshSet->m_Materials.m_Data[i].m_Name) == 0)
                    return i;
            }
            return -1;
        }
        inline bool operator() (const MaterialInfo& mat1, const MaterialInfo& mat2)
        {
            int i1 = FindMaterial(mat1.m_Name);
            int i2 = FindMaterial(mat2.m_Name);
            return i1 < i2;
        }
    };

    // Flattens the meshes into a list, sorted on material
    static void FlattenMeshes(ModelResource* resource, dmRigDDF::MeshSet* mesh_set)
    {
        for (uint32_t i = 0; i < mesh_set->m_Models.m_Count; ++i)
        {
            dmRigDDF::Model* model = &mesh_set->m_Models[i];

            if (resource->m_Meshes.Remaining() < model->m_Meshes.m_Count)
            {
                resource->m_Meshes.OffsetCapacity(model->m_Meshes.m_Count - resource->m_Meshes.Remaining());
            }

            for (uint32_t j = 0; j < model->m_Meshes.m_Count; ++j)
            {
                dmRigDDF::Mesh* mesh = &model->m_Meshes[j];

                MeshInfo info;
                info.m_Model = model;
                info.m_Mesh = mesh;
                info.m_Buffers = 0;

                resource->m_Meshes.Push(info);
            }
        }

        // Sort the meshes by material
        std::sort(resource->m_Meshes.Begin(), resource->m_Meshes.End(), MeshSortPred());
    }

    static inline uint32_t GetRigModelVertexFormatSize(RigModelVertexFormat format)
    {
        switch (format)
        {
        case RIG_MODEL_VERTEX_FORMAT_STATIC:
            return sizeof(dmRig::RigModelVertex);
        case RIG_MODEL_VERTEX_FORMAT_SKINNED:
            return sizeof(dmRig::RigModelSkinnedVertex);
        }
        return 0;
    }

    static inline RigModelVertexFormat GetRigModelVertexFormat(const dmRigDDF::Mesh* mesh)
    {
        const float* weights = mesh->m_Weights.m_Count ? mesh->m_Weights.m_Data : 0;
        const uint32_t* indices = mesh->m_BoneIndices.m_Count ? mesh->m_BoneIndices.m_Data : 0;
        if (weights && indices)
            return RIG_MODEL_VERTEX_FORMAT_SKINNED;
        return RIG_MODEL_VERTEX_FORMAT_STATIC;
    }

    // TODO: Now that we don't split meshes at runtime, we should move this code to the build pipeline /MAWE
    static uint8_t* CreateVertexData(const dmRigDDF::Mesh* mesh, uint8_t* out_write_ptr, RigModelVertexFormat format)
    {
        uint32_t vertex_count = mesh->m_Positions.m_Count / 3;
        const float* positions = mesh->m_Positions.m_Count ? mesh->m_Positions.m_Data : 0;
        const float* normals = mesh->m_Normals.m_Count ? mesh->m_Normals.m_Data : 0;
        const float* tangents = mesh->m_Tangents.m_Count ? mesh->m_Tangents.m_Data : 0;
        const float* colors = mesh->m_Colors.m_Count ? mesh->m_Colors.m_Data : 0;
        const float* uv0 = mesh->m_Texcoord0.m_Count ? mesh->m_Texcoord0.m_Data : 0;
        const float* uv1 = mesh->m_Texcoord1.m_Count ? mesh->m_Texcoord1.m_Data : 0;
        const float* weights = mesh->m_Weights.m_Count ? mesh->m_Weights.m_Data : 0;
        const uint32_t* indices = mesh->m_BoneIndices.m_Count ? mesh->m_BoneIndices.m_Data : 0;

        uint32_t vertex_size = GetRigModelVertexFormatSize(format);

        for (uint32_t i = 0; i < vertex_count; ++i)
        {
            dmRig::RigModelVertex* vertex = (dmRig::RigModelVertex*) out_write_ptr;

            for (int c = 0; c < 3; ++c)
            {
                vertex->pos[c] = *positions++;
                vertex->normal[c] = normals ? *normals++ : 0.0f;
            }

            for (int c = 0; c < 4; ++c)
            {
                vertex->color[c] = colors ? *colors++ : 1.0f;
                vertex->tangent[c] = tangents ? *tangents++ : 0.0f;
            }

            for (int c = 0; c < 2; ++c)
            {
                vertex->uv0[c] = uv0 ? *uv0++ : 0.0f;
                vertex->uv1[c] = uv1 ? *uv1++ : 0.0f;
            }

            if (format == RIG_MODEL_VERTEX_FORMAT_SKINNED)
            {
                dmRig::RigModelSkinnedVertex* vertex_skinned = (dmRig::RigModelSkinnedVertex*) out_write_ptr;
                for (int c = 0; c < 4; ++c)
                {
                    vertex_skinned->m_BoneWeights[c] = weights ? *weights++ : 0.0f;
                }

                for (int c = 0; c < 4; ++c)
                {
                    vertex_skinned->m_BoneIndices[c] = indices ? (float) *indices++ : 0.0f;
                }
            }

            out_write_ptr += vertex_size;
        }

        return out_write_ptr;
    }

    static ModelResourceBuffers* CreateBuffers(dmGraphics::HContext context, const ModelResource* resource, const dmRigDDF::Mesh* ddf_mesh, dmArray<uint8_t>& scratch_buffer)
    {
        ModelResourceBuffers* buffers = new ModelResourceBuffers;
        memset(buffers, 0, sizeof(ModelResourceBuffers));

        uint32_t num_vertices = ddf_mesh->m_Positions.m_Count / 3;

        bool support_32bit_indices = dmGraphics::IsIndexBufferFormatSupported(context, dmGraphics::INDEXBUFFER_FORMAT_32);

        dmGraphics::Type index_element_type = dmGraphics::TYPE_UNSIGNED_SHORT;
        void* index_buffer = ddf_mesh->m_Indices.m_Data;
        uint32_t num_indices = ddf_mesh->m_Indices.m_Count / 2;
        if (ddf_mesh->m_IndicesFormat == dmRigDDF::INDEXBUFFER_FORMAT_32)
        {
            num_indices = ddf_mesh->m_Indices.m_Count / 4;
            index_element_type = dmGraphics::TYPE_UNSIGNED_INT;

            // We've moved the runtime check to the build step instead
            if (!support_32bit_indices)
            {
                dmLogError("The platform doesn't support 32 bit index buffers. See the setting 'model.split_large_meshes'");
                return buffers;
            }
        }

        RigModelVertexFormat format = GetRigModelVertexFormat(ddf_mesh);
        uint32_t vertex_size = GetRigModelVertexFormatSize(format);
        uint32_t data_size = num_vertices * vertex_size;

        if (scratch_buffer.Capacity() < data_size)
            scratch_buffer.SetCapacity(data_size);
        scratch_buffer.SetSize(data_size);

        CreateVertexData(ddf_mesh, scratch_buffer.Begin(), format);

        buffers->m_VertexBuffer = dmGraphics::NewVertexBuffer(context, data_size, scratch_buffer.Begin(), dmGraphics::BUFFER_USAGE_STATIC_DRAW);
        buffers->m_VertexCount = num_vertices;
        buffers->m_RigModelVertexFormat = format;

        buffers->m_IndexBuffer = 0;
        buffers->m_IndexCount = 0;
        if (index_buffer != 0)
        {
            uint32_t index_type_size = dmGraphics::TYPE_UNSIGNED_INT == index_element_type ? 4 : 2;
            buffers->m_IndexBuffer = dmGraphics::NewIndexBuffer(context, num_indices * index_type_size, index_buffer, dmGraphics::BUFFER_USAGE_STATIC_DRAW);
            buffers->m_IndexBufferElementType = index_element_type;
            buffers->m_IndexCount = num_indices;
        }

        return buffers;
    }

    static void CreateBuffers(dmGraphics::HContext context, ModelResource* resource)
    {
        dmArray<uint8_t> scratch_buffer;
        for (uint32_t i = 0; i < resource->m_Meshes.Size(); ++i)
        {
            MeshInfo& info = resource->m_Meshes[i];
            info.m_Buffers = CreateBuffers(context, resource, info.m_Mesh, scratch_buffer);
        }
    }

    static bool AreAllMaterialsWorldSpace(const ModelResource* resource)
    {
        for (uint32_t i = 0; i < resource->m_Materials.Size(); ++i)
        {
            dmRender::HMaterial material = resource->m_Materials[i].m_Material->m_Material;
            if (dmRender::GetMaterialVertexSpace(material) == dmRenderDDF::MaterialDesc::VERTEX_SPACE_LOCAL)
                return false;
        }
        return true; // All materials are currently world space
    }

    // We could sort them in the pipeline, but then we'd have to read the material data
    // for compiling each model which we might not want
    struct TextureSortPred
    {
        dmRender::HMaterial m_Material;
        TextureSortPred(dmRender::HMaterial material) : m_Material(material) {}

        bool operator ()(MaterialTextureInfo& a, MaterialTextureInfo& b) const
        {
            uintptr_t ia = dmRender::GetMaterialConstantLocation(m_Material, a.m_SamplerNameHash);
            uintptr_t ib = dmRender::GetMaterialConstantLocation(m_Material, b.m_SamplerNameHash);
            return ia < ib;
        }
    };

    void SortTextures(dmRender::HMaterial material, uint32_t num_textures, MaterialTextureInfo* textures)
    {
        TextureSortPred pred(material);
        std::sort(textures, textures + num_textures, pred);
    }

    dmResource::Result AcquireResources(dmGraphics::HContext context, dmResource::HFactory factory, ModelResource* resource, const char* filename)
    {
        dmResource::Result result = dmResource::Get(factory, resource->m_Model->m_RigScene, (void**) &resource->m_RigScene);
        if (result != dmResource::RESULT_OK)
            return result;

        dmRigDDF::MeshSet* mesh_set = resource->m_RigScene->m_MeshSetRes->m_MeshSet;
        FlattenMeshes(resource, mesh_set);
        CreateBuffers(context, resource);

        uint32_t material_count = dmMath::Max(resource->m_Model->m_Materials.m_Count, mesh_set->m_Materials.m_Count);
        resource->m_Materials.SetCapacity(material_count);
        for (uint32_t i = 0; i < material_count; ++i)
        {
            uint32_t material_index = i;

            // This case may come up if we use an old model which hasn't mapped between mesh materials properly
            // I.e. the model only has one "default" material
            if (i >= resource->m_Model->m_Materials.m_Count)
                material_index = 0;
            dmModelDDF::Material* model_material = &resource->m_Model->m_Materials[material_index];

            MaterialInfo info;
            memset(&info, 0, sizeof(info));

            result = dmResource::Get(factory, model_material->m_Material, (void**) &info.m_Material);
            if (result != dmResource::RESULT_OK)
            {
                return result;
            }

            info.m_Name = strdup(model_material->m_Name);

            info.m_Attributes = model_material->m_Attributes.m_Data;
            info.m_AttributeCount = model_material->m_Attributes.m_Count;

            // Currently, we don't support overriding the textures per-material on the model level

            // While the material may use less samplers than the model has textures,
            // We'll still respect the users wishes as they may change the material later on
            info.m_TexturesCount = model_material->m_Textures.m_Count;
            info.m_Textures = new MaterialTextureInfo[info.m_TexturesCount];
            memset(info.m_Textures, 0, sizeof(MaterialTextureInfo)*info.m_TexturesCount);
            for (uint32_t t = 0; t < info.m_TexturesCount; ++t)
            {
                dmModelDDF::Texture* texture = &model_material->m_Textures[t];
                MaterialTextureInfo* texture_info = &info.m_Textures[t];

                void* resource;
                result = dmResource::Get(factory, texture->m_Texture, &resource);

                if (result != dmResource::RESULT_OK)
                {
                    return result;
                }

                if (ResourcePathToRenderResourceType(texture->m_Texture) == dmRender::RENDER_RESOURCE_TYPE_RENDER_TARGET)
                {
                    texture_info->m_RenderTarget = (RenderTargetResource*) resource;
                    texture_info->m_Texture      = texture_info->m_RenderTarget->m_ColorAttachmentResources[0];
                }
                else
                {
                    texture_info->m_RenderTarget = 0;
                    texture_info->m_Texture      = (TextureResource*) resource;
                }

                texture_info->m_SamplerNameHash = dmHashString64(texture->m_Sampler);
                if (!texture_info->m_SamplerNameHash) // old content
                {
                    // Then we'll fallback to using the order of each sampler
                    texture_info->m_SamplerNameHash = dmRender::GetMaterialSamplerNameHash(info.m_Material->m_Material, t);
                }
            }

            // We need to sort the textures on sampler units
            SortTextures(info.m_Material->m_Material, info.m_TexturesCount, info.m_Textures);

            if (resource->m_Materials.Full())
                resource->m_Materials.OffsetCapacity(1);
            resource->m_Materials.Push(info);
        }

        // Sort the materials so that they have the same indices as specified in the MeshSet list of materials
        std::sort(resource->m_Materials.Begin(), resource->m_Materials.End(), MaterialSortPred(mesh_set));

        if (result != dmResource::RESULT_OK)
        {
            ReleaseResources(factory, resource);
            return result;
        }

        if(resource->m_RigScene->m_AnimationSetRes || resource->m_RigScene->m_SkeletonRes)
        {
            if (!AreAllMaterialsWorldSpace(resource) && !dmGraphics::IsTextureFormatSupported(context, BIND_POSE_CACHE_TEXTURE_FORMAT))
            {
                dmLogError(
                    "Failed to create Model component. One or more materials has vertex space option VERTEX_SPACE_LOCAL,"
                    "which requires a float texture for storing animation data but the format (%s) isn't supported.",
                    dmGraphics::GetTextureFormatLiteral(BIND_POSE_CACHE_TEXTURE_FORMAT));
                return dmResource::RESULT_NOT_SUPPORTED;
            }
        }

        return result;
    }

    static void ReleaseBuffers(ModelResourceBuffers* buffers)
    {
        dmGraphics::DeleteVertexBuffer(buffers->m_VertexBuffer);
        dmGraphics::DeleteIndexBuffer(buffers->m_IndexBuffer);
        delete buffers;
    }

    static void ReleaseResources(dmResource::HFactory factory, ModelResource* resource)
    {
        for (uint32_t i = 0; i < resource->m_Meshes.Size(); ++i)
        {
            MeshInfo& info = resource->m_Meshes[i];
            ReleaseBuffers(info.m_Buffers);
        }
        resource->m_Meshes.SetSize(0);

        if (resource->m_Model != 0x0)
            dmDDF::FreeMessage(resource->m_Model);
        resource->m_Model = 0x0;
        if (resource->m_RigScene != 0x0)
            dmResource::Release(factory, resource->m_RigScene);
        resource->m_RigScene = 0x0;

        for (uint32_t i = 0; i < resource->m_Materials.Size(); ++i)
        {
            MaterialInfo* material = &resource->m_Materials[i];
            free((void*)material->m_Name);
            dmResource::Release(factory, material->m_Material);

            for (uint32_t t = 0; t < material->m_TexturesCount; ++t)
            {
                if (material->m_Textures[t].m_RenderTarget)
                {
                    dmResource::Release(factory, (void*) material->m_Textures[t].m_RenderTarget);
                }
                else if (material->m_Textures[t].m_Texture)
                {
                    dmResource::Release(factory, (void*) material->m_Textures[t].m_Texture);
                }
            }
            delete[] material->m_Textures;
            material->m_TexturesCount = 0;
        }
        resource->m_Materials.SetSize(0);
    }

    dmResource::Result ResModelPreload(const dmResource::ResourcePreloadParams* params)
    {
        dmModelDDF::Model* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params->m_Buffer, params->m_BufferSize, &dmModelDDF_Model_DESCRIPTOR, (void**) &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }

        for (uint32_t i = 0; i < ddf->m_Materials.m_Count; ++i)
        {
            dmResource::PreloadHint(params->m_HintInfo, ddf->m_Materials[i].m_Material);

            for (uint32_t j = 0; j < ddf->m_Materials[i].m_Textures.m_Count && j < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++j)
            {
                dmResource::PreloadHint(params->m_HintInfo, ddf->m_Materials[i].m_Textures[j].m_Texture);
            }
        }

        dmResource::PreloadHint(params->m_HintInfo, ddf->m_RigScene);

        *params->m_PreloadData = ddf;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResModelCreate(const dmResource::ResourceCreateParams* params)
    {
        ModelResource* model_resource = new ModelResource();
        memset(model_resource, 0, sizeof(ModelResource));
        model_resource->m_Model = (dmModelDDF::Model*) params->m_PreloadData;
        dmResource::Result r = AcquireResources((dmGraphics::HContext) params->m_Context, params->m_Factory, model_resource, params->m_Filename);
        if (r == dmResource::RESULT_OK)
        {
            dmResource::SetResource(params->m_Resource, model_resource);
        }
        else
        {
            ReleaseResources(params->m_Factory, model_resource);
            delete model_resource;
        }
        return r;
    }

    dmResource::Result ResModelDestroy(const dmResource::ResourceDestroyParams* params)
    {
        ModelResource* model_resource = (ModelResource*)dmResource::GetResource(params->m_Resource);
        ReleaseResources(params->m_Factory, model_resource);
        delete model_resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResModelRecreate(const dmResource::ResourceRecreateParams* params)
    {
        dmModelDDF::Model* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params->m_Buffer, params->m_BufferSize, &dmModelDDF_Model_DESCRIPTOR, (void**) &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }
        ModelResource* model_resource = (ModelResource*)dmResource::GetResource(params->m_Resource);
        ReleaseResources(params->m_Factory, model_resource);
        model_resource->m_Model = ddf;
        return AcquireResources((dmGraphics::HContext) params->m_Context, params->m_Factory, model_resource, params->m_Filename);
    }
}
