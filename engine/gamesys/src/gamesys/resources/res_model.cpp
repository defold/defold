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
#include "res_model_buffers.h"
#include "res_meshset.h"
#include "res_skeleton.h"
#include "res_animationset.h"
#include "res_material.h"
#include "res_render_target.h"
#include "res_texture.h"

#include "gamesys_private.h"
#include "gamesys.h"

#include <gamesys/model_ddf.h>

#include <dlib/log.h>
#include <dlib/path.h>
#include <dlib/dstrings.h>
#include <dlib/memory.h>
#include <graphics/graphics.h>
#include <render/render.h>
#include <dmsdk/dlib/transform.h>
#include <rig/rig.h>
#include <stdlib.h> // qsort

namespace dmGameSystem
{
    static void ReleaseResources(dmGraphics::HContext context, dmResource::HFactory factory, ModelResource* resource);

    // TODO: Sorting+flattening the structure could be done in the pipeline
    static int MeshSortCompare(const void* a, const void* b)
    {
        const MeshInfo* mesh_a = (const MeshInfo*) a;
        const MeshInfo* mesh_b = (const MeshInfo*) b;
        uint32_t material_a = mesh_a->m_Mesh->m_MaterialIndex;
        uint32_t material_b = mesh_b->m_Mesh->m_MaterialIndex;
        if (material_a != material_b)
        {
            return material_a < material_b ? -1 : 1;
        }
        return 0;
    }

    // We could sort them in the pipeline, but then we'd have to read the mesh data
    // for compiling each model which we might not want
    static int FindMaterialIndex(dmRigDDF::MeshSet* mesh_set, const char* name)
    {
        for (int i = 0; i < mesh_set->m_Materials.m_Count; ++i)
        {
            if (strcmp(name, mesh_set->m_Materials.m_Data[i].m_Name) == 0)
                return i;
        }
        return -1;
    }

    static int MaterialSortCompare(const void* a, const void* b)
    {
        const MaterialInfo* mat_a = (const MaterialInfo*) a;
        const MaterialInfo* mat_b = (const MaterialInfo*) b;
        if (mat_a->m_SortOrder != mat_b->m_SortOrder)
        {
            return mat_a->m_SortOrder < mat_b->m_SortOrder ? -1 : 1;
        }
        return 0;
    }

    // Flattens the meshes into a list, sorted on material
    static void FlattenMeshes(ModelResource* resource, dmRigDDF::MeshSet* mesh_set)
    {
        uint32_t selected_mesh_index = resource->m_Model->m_MeshIndex;
        dmRigDDF::Model* models = mesh_set->m_Models.m_Data;
        dmRigDDF::Model* geometry_models = models;
        dmRigDDF::Model* morph_model = 0;
        uint32_t model_count = mesh_set->m_Models.m_Count;
        if (selected_mesh_index != ~0u)
        {
            dmRigDDF::Model* selected_model = 0;
            dmRigDDF::Model* geometry_model = 0;
            for (uint32_t i = 0; i < mesh_set->m_RawModels.m_Count; ++i)
            {
                if (mesh_set->m_RawModels[i].m_MeshIndex == selected_mesh_index)
                {
                    selected_model = &mesh_set->m_RawModels[i];
                    if (selected_model->m_Meshes.m_Count > 0)
                    {
                        geometry_model = selected_model;
                    }
                    break;
                }
            }
            for (uint32_t i = 0; i < mesh_set->m_Models.m_Count; ++i)
            {
                if (mesh_set->m_Models[i].m_MeshIndex == selected_mesh_index)
                {
                    morph_model = &mesh_set->m_Models[i];
                    if (!geometry_model)
                    {
                        geometry_model = morph_model;
                    }
                    break;
                }
            }
            if (!geometry_model)
            {
                return;
            }
            if (!selected_model)
            {
                selected_model = geometry_model;
            }
            if (!morph_model)
            {
                morph_model = selected_model;
            }

            resource->m_SelectedModel = *selected_model;
            resource->m_SelectedModel.m_Local.SetIdentity();
            resource->m_SelectedModel.m_BoneId = 0;
            models = &resource->m_SelectedModel;
            geometry_models = geometry_model;
            model_count = 1;
        }

        for (uint32_t i = 0; i < model_count; ++i)
        {
            dmRigDDF::Model* model = &models[i];
            dmRigDDF::Model* geometry_model = &geometry_models[i];

            if (resource->m_Meshes.Remaining() < geometry_model->m_Meshes.m_Count)
            {
                resource->m_Meshes.OffsetCapacity(geometry_model->m_Meshes.m_Count - resource->m_Meshes.Remaining());
            }

            for (uint32_t j = 0; j < geometry_model->m_Meshes.m_Count; ++j)
            {
                dmRigDDF::Mesh* mesh = &geometry_model->m_Meshes[j];

                MeshInfo info;
                info.m_Model = model;
                // CPU stream pointers are attached lazily and belong to the shared
                // geometry resource. Keep them out of the shared mesh-set metadata.
                info.m_Mesh = new dmRigDDF::Mesh(*mesh);
                info.m_Buffers = 0;
                info.m_MorphTargetTexture = 0;
                info.m_MorphModelId = selected_mesh_index != ~0u ? morph_model->m_Id : model->m_Id;

                resource->m_Meshes.Push(info);
            }
        }

        // Sort the meshes by material
        if (resource->m_Meshes.Size() > 1)
        {
            qsort(resource->m_Meshes.Begin(), resource->m_Meshes.Size(), sizeof(MeshInfo), MeshSortCompare);
        }
    }

    static dmResource::Result AcquireRigResources(dmResource::HFactory factory, ModelResource* model_resource)
    {
        const dmModelDDF::Model* model = model_resource->m_Model;
        RigSceneResource* resource = new RigSceneResource();
        model_resource->m_RigScene = resource;

        // The rig view borrows mesh metadata and dependency paths from the model DDF.
        resource->m_RigScene = new dmRigDDF::RigScene();
        resource->m_RigScene->m_Skeleton = model->m_Skeleton;
        resource->m_RigScene->m_AnimationSet = model->m_AnimationSet;
        resource->m_RigScene->m_TextureSet = "";
        resource->m_RigScene->m_MeshSet = "";
        resource->m_MeshSetRes = new MeshSetResource();
        resource->m_MeshSetRes->m_MeshSet = &model_resource->m_Model->m_MeshSet;

        if (model->m_Skeleton[0])
        {
            dmResource::Result result = dmResource::Get(factory, model->m_Skeleton, (void**)&resource->m_SkeletonRes);
            if (result != dmResource::RESULT_OK)
            {
                return result;
            }

            dmRig::CopyBindPose(*resource->m_SkeletonRes->m_Skeleton, resource->m_BindPose);
        }

        if (model->m_AnimationSet[0])
        {
            dmResource::Result result = dmResource::Get(factory, model->m_AnimationSet, (void**)&resource->m_AnimationSetRes);
            if (result != dmResource::RESULT_OK)
            {
                return result;
            }
        }

        return dmResource::RESULT_OK;
    }

    static void ReleaseRigResources(dmResource::HFactory factory, RigSceneResource* resource)
    {
        if (resource->m_SkeletonRes)
        {
            dmResource::Release(factory, resource->m_SkeletonRes);
        }

        if (resource->m_AnimationSetRes)
        {
            dmResource::Release(factory, resource->m_AnimationSetRes);
        }

        // These wrappers do not own the model DDF data they reference.
        delete resource->m_MeshSetRes;
        delete resource->m_RigScene;
        delete resource;
    }

    static dmResource::Result AcquireBuffers(dmResource::HFactory factory, ModelResource* resource)
    {
        dmResource::Result result = dmResource::Get(factory, resource->m_Model->m_Buffers, (void**)&resource->m_Buffers);
        if (result != dmResource::RESULT_OK)
        {
            return result;
        }

        for (uint32_t i = 0; i < resource->m_Meshes.Size(); ++i)
        {
            MeshInfo& info = resource->m_Meshes[i];
            info.m_Buffers = GetModelBuffers(resource->m_Buffers, info.m_Mesh->m_BufferIndex);
            if (!info.m_Buffers || info.m_Buffers->m_VertexCount != info.m_Mesh->m_VertexCount)
            {
                return dmResource::RESULT_INVALID_DATA;
            }
        }

        return dmResource::RESULT_OK;
    }

    static dmResource::Result AcquireMorphTargetTextures(dmResource::HFactory factory, ModelResource* resource)
    {
        for (uint32_t i = 0; i < resource->m_Meshes.Size(); ++i)
        {
            MeshInfo& info = resource->m_Meshes[i];
            if (info.m_Mesh->m_MorphTargetTexture && info.m_Mesh->m_MorphTargetTexture[0])
            {
                dmResource::Result result = dmResource::Get(factory, info.m_Mesh->m_MorphTargetTexture, (void**)&info.m_MorphTargetTexture);
                if (result != dmResource::RESULT_OK)
                {
                    return result;
                }
            }
        }
        return dmResource::RESULT_OK;
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

    static uintptr_t GetTextureSortOrder(dmRender::HMaterial material, const MaterialTextureInfo* texture)
    {
        return dmRender::GetMaterialConstantLocation(material, texture->m_SamplerNameHash);
    }

    static void SortTextures(dmRender::HMaterial material, uint32_t num_textures, MaterialTextureInfo* textures)
    {
        for (uint32_t i = 0; i < num_textures; ++i)
        {
            uint32_t best_index = i;
            uintptr_t best_order = GetTextureSortOrder(material, &textures[i]);
            for (uint32_t j = i + 1; j < num_textures; ++j)
            {
                uintptr_t order = GetTextureSortOrder(material, &textures[j]);
                if (order < best_order)
                {
                    best_index = j;
                    best_order = order;
                }
            }

            if (best_index != i)
            {
                MaterialTextureInfo tmp = textures[i];
                textures[i] = textures[best_index];
                textures[best_index] = tmp;
            }
        }
    }

    dmResource::Result AcquireResources(dmGraphics::HContext context, dmResource::HFactory factory, ModelResource* resource)
    {
        dmResource::Result result = AcquireRigResources(factory, resource);
        if (result != dmResource::RESULT_OK)
        {
            return result;
        }

        dmRigDDF::MeshSet* mesh_set = resource->m_RigScene->m_MeshSetRes->m_MeshSet;
        FlattenMeshes(resource, mesh_set);
        result = AcquireBuffers(factory, resource);
        if (result != dmResource::RESULT_OK)
        {
            return result;
        }
        result = AcquireMorphTargetTextures(factory, resource);
        if (result != dmResource::RESULT_OK)
        {
            ReleaseResources(context, factory, resource);
            return result;
        }

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
            info.m_SortOrder = FindMaterialIndex(mesh_set, info.m_Name);

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
        qsort(resource->m_Materials.Begin(), resource->m_Materials.Size(), sizeof(MaterialInfo), MaterialSortCompare);

        if (result != dmResource::RESULT_OK)
        {
            ReleaseResources(context, factory, resource);
            return result;
        }

        if(resource->m_RigScene->m_AnimationSetRes || resource->m_RigScene->m_SkeletonRes)
        {
            if (!AreAllMaterialsWorldSpace(resource) && !dmGraphics::IsTextureFormatSupported(context, BIND_POSE_CACHE_TEXTURE_FORMAT))
            {
                dmLogError(
                    "Failed to create model component. One or more materials have vertex space option VERTEX_SPACE_LOCAL, "
                    "which requires a float texture for storing animation data, but texture format %s is not supported.",
                    dmGraphics::GetTextureFormatLiteral(BIND_POSE_CACHE_TEXTURE_FORMAT));
                return dmResource::RESULT_NOT_SUPPORTED;
            }
        }

        return result;
    }

    static void ReleaseResources(dmGraphics::HContext context, dmResource::HFactory factory, ModelResource* resource)
    {
        (void) context;
        for (uint32_t i = 0; i < resource->m_Meshes.Size(); ++i)
        {
            MeshInfo& info = resource->m_Meshes[i];
            if (info.m_MorphTargetTexture)
            {
                dmResource::Release(factory, info.m_MorphTargetTexture);
                info.m_MorphTargetTexture = 0;
            }
            delete info.m_Mesh;
        }
        resource->m_Meshes.SetSize(0);
        if (resource->m_Buffers)
        {
            dmResource::Release(factory, resource->m_Buffers);
        }
        resource->m_Buffers = 0;

        if (resource->m_RigScene != 0x0)
        {
            ReleaseRigResources(factory, resource->m_RigScene);
        }
        resource->m_RigScene = 0x0;
        if (resource->m_Model != 0x0)
        {
            dmDDF::FreeMessage(resource->m_Model);
        }
        resource->m_Model = 0x0;

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

    static void PreloadMorphTextures(const dmResource::ResourcePreloadParams* params, const dmRigDDF::Model* models, uint32_t model_count)
    {
        for (uint32_t i = 0; i < model_count; ++i)
        {
            const dmRigDDF::Model& model = models[i];
            for (uint32_t j = 0; j < model.m_Meshes.m_Count; ++j)
            {
                const char* texture = model.m_Meshes[j].m_MorphTargetTexture;
                if (texture[0])
                {
                    dmResource::PreloadHint(params->m_HintInfo, texture);
                }
            }
        }
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

        if (ddf->m_Skeleton[0])
        {
            dmResource::PreloadHint(params->m_HintInfo, ddf->m_Skeleton);
        }

        if (ddf->m_AnimationSet[0])
        {
            dmResource::PreloadHint(params->m_HintInfo, ddf->m_AnimationSet);
        }

        dmResource::PreloadHint(params->m_HintInfo, ddf->m_Buffers);
        PreloadMorphTextures(params, ddf->m_MeshSet.m_Models.m_Data, ddf->m_MeshSet.m_Models.m_Count);
        PreloadMorphTextures(params, ddf->m_MeshSet.m_RawModels.m_Data, ddf->m_MeshSet.m_RawModels.m_Count);

        *params->m_PreloadData = ddf;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResModelCreate(const dmResource::ResourceCreateParams* params)
    {
        ModelContext* model_ctx = (ModelContext*) params->m_Context;
        dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(model_ctx->m_RenderContext);

        ModelResource* model_resource = new ModelResource();
        memset(model_resource, 0, sizeof(ModelResource));
        model_resource->m_Model = (dmModelDDF::Model*) params->m_PreloadData;
        dmResource::Result r = AcquireResources(graphics_context, params->m_Factory, model_resource);
        if (r == dmResource::RESULT_OK)
        {
            dmResource::SetResource(params->m_Resource, model_resource);
        }
        else
        {
            ReleaseResources(dmRender::GetGraphicsContext(((ModelContext*)params->m_Context)->m_RenderContext), params->m_Factory, model_resource);
            delete model_resource;
        }
        return r;
    }

    dmResource::Result ResModelDestroy(const dmResource::ResourceDestroyParams* params)
    {
        ModelResource* model_resource = (ModelResource*)dmResource::GetResource(params->m_Resource);
        ReleaseResources(dmRender::GetGraphicsContext(((ModelContext*)params->m_Context)->m_RenderContext), params->m_Factory, model_resource);
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
        ModelContext* model_ctx = (ModelContext*) params->m_Context;
        dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(model_ctx->m_RenderContext);

        ModelResource* model_resource = (ModelResource*)dmResource::GetResource(params->m_Resource);
        ReleaseResources(graphics_context, params->m_Factory, model_resource);
        model_resource->m_Model = ddf;
        return AcquireResources(graphics_context, params->m_Factory, model_resource);
    }
}
