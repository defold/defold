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

#include "res_material.h"

#include <string.h>
#include <algorithm> // std::sort

#include <dlib/array.h>
#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/log.h>

#include <render/render.h>
#include <render/material_ddf.h>

#include <dmsdk/dlib/hashtable.h>

namespace dmGameSystem
{
    static inline bool ValidateFormat(dmRenderDDF::MaterialDesc* material_desc)
    {
        if (strlen(material_desc->m_Name) == 0)
            return false;
        return true;
    }

    struct MaterialResources
    {
        MaterialResources()
        {
            memset(this, 0, sizeof(*this));
        }

        dmGraphics::HProgram m_Program;
        TextureResource*     m_Textures[dmRender::RenderObject::MAX_TEXTURE_COUNT];
        dmhash_t             m_SamplerNames[dmRender::RenderObject::MAX_TEXTURE_COUNT];
    };

    static void ReleaseTextures(dmResource::HFactory factory, TextureResource** textures)
    {
        for (uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
        {
            if (textures[i])
                dmResource::Release(factory, (void*)textures[i]);
        }
        memset(textures, 0, sizeof(TextureResource*)*dmRender::RenderObject::MAX_TEXTURE_COUNT);
    }

    static void ReleaseResources(dmResource::HFactory factory, MaterialResources* resources)
    {
        if (resources->m_Program)
        {
            dmResource::Release(factory, (void*)resources->m_Program);
            resources->m_Program = 0;
        }

        ReleaseTextures(factory, resources->m_Textures);
    }

    static dmResource::Result AcquireResources(dmResource::HFactory factory, dmRenderDDF::MaterialDesc* ddf, MaterialResources* resources)
    {
        memset(resources->m_Textures, 0, sizeof(resources->m_Textures));
        memset(resources->m_SamplerNames, 0, sizeof(resources->m_SamplerNames));

        dmResource::Result factory_e = dmResource::Get(factory, ddf->m_Program, (void**) &resources->m_Program);
        if ( factory_e != dmResource::RESULT_OK)
        {
            ReleaseResources(factory, resources);
            return factory_e;
        }

        // Later, in the render.cpp, we do a mapping between the sampler's location and the texture unit.
        // We assume that the textures[8] are always sorted in the sampler appearance.
        // however, the order of samplers here may be different from the order in the actual material!
        dmRenderDDF::MaterialDesc::Sampler* sampler = ddf->m_Samplers.m_Data;
        for (uint32_t i = 0; i < ddf->m_Samplers.m_Count; i++)
        {
            resources->m_SamplerNames[i] = sampler[i].m_NameHash;
            const char* texture_path = sampler[i].m_Texture;
            if (*texture_path != 0)
            {
                factory_e = dmResource::Get(factory, texture_path, (void**)&resources->m_Textures[i]);
                if ( factory_e != dmResource::RESULT_OK)
                {
                    ReleaseResources(factory, resources);
                    return factory_e;
                }
            }
        }
        return dmResource::RESULT_OK;
    }

    static void SetMaterial(const char* path, MaterialResource* resource, MaterialResources* resources, dmRenderDDF::MaterialDesc* ddf)
    {
        dmhash_t tags[dmRender::MAX_MATERIAL_TAG_COUNT];
        uint32_t tag_count = ddf->m_Tags.m_Count;

        if (tag_count > dmRender::MAX_MATERIAL_TAG_COUNT) {
            dmLogError("The maximum number of tags per material is %d. Skipping the last ones for %s", dmRender::MAX_MATERIAL_TAG_COUNT, path);
            tag_count = dmRender::MAX_MATERIAL_TAG_COUNT;
        }

        for (uint32_t i = 0; i < tag_count; ++i)
        {
            tags[i] = dmHashString64(ddf->m_Tags[i]);
        }
        std::sort(tags, tags + tag_count);

        dmRender::HMaterial material = resource->m_Material;
        dmRender::SetMaterialTags(material, tag_count, tags);

        dmRender::SetMaterialVertexSpace(material, ddf->m_VertexSpace);
        dmRenderDDF::MaterialDesc::Constant* fragment_constant = ddf->m_FragmentConstants.m_Data;
        dmRenderDDF::MaterialDesc::Constant* vertex_constant = ddf->m_VertexConstants.m_Data;

        uint32_t fragment_count = ddf->m_FragmentConstants.m_Count;
        uint32_t vertex_count = ddf->m_VertexConstants.m_Count;

        // TODO: We should merge the vertex and fragment constants in the source formats.

        // save pre-set fragment constants
        for (uint32_t i = 0; i < fragment_count; i++)
        {
            const char* name = fragment_constant[i].m_Name;
            // TODO: Hash the name in the pipeline
            dmhash_t name_hash = dmHashString64(name);

            dmRender::HConstant constant;
            if (dmRender::GetMaterialProgramConstant(material, name_hash, constant))
            {
                dmRender::SetMaterialProgramConstantType(material, name_hash, fragment_constant[i].m_Type);
                dmRender::SetMaterialProgramConstant(material, name_hash,
                    (dmVMath::Vector4*) fragment_constant[i].m_Value.m_Data, fragment_constant[i].m_Value.m_Count);
            }
            else
            {
                dmLogWarning("Material %s has specified a fragment constant named '%s', but it does not exist or isn't used in any of the shaders.", path, name);
            }
        }
        // do the same for vertex constants
        for (uint32_t i = 0; i < vertex_count; i++)
        {
            const char* name = vertex_constant[i].m_Name;
            // TODO: Hash the name in the pipeline
            dmhash_t name_hash = dmHashString64(name);

            dmRender::HConstant constant;
            if (dmRender::GetMaterialProgramConstant(material, name_hash, constant))
            {
                dmRender::SetMaterialProgramConstantType(material, name_hash, vertex_constant[i].m_Type);
                dmRender::SetMaterialProgramConstant(material, name_hash,
                    (dmVMath::Vector4*) vertex_constant[i].m_Value.m_Data, vertex_constant[i].m_Value.m_Count);
            }
            else
            {
                dmLogWarning("Material %s has specified a vertex constant named '%s', but it does not exist or isn't used in any of the shaders.", path, name);
            }
        }

        // check for unused attributes and let the user know if they are
        for (int i = 0; i < ddf->m_Attributes.m_Count; ++i)
        {
            if (dmRender::GetMaterialAttributeIndex(material, ddf->m_Attributes[i].m_NameHash) == dmRender::INVALID_MATERIAL_ATTRIBUTE_INDEX)
            {
                dmLogWarning("Material %s has specified a vertex attribute named '%s', but it does not exist or isn't used in any of the shaders.", path, ddf->m_Attributes[i].m_Name);
            }
        }

        // Set all vertex attributes
        dmRender::SetMaterialProgramAttributes(material, ddf->m_Attributes.m_Data, ddf->m_Attributes.m_Count);

        // PBR parameters
        dmRender::SetMaterialPBRParameters(material, &ddf->m_PbrParameters);

        dmRenderDDF::MaterialDesc::Sampler* sampler = ddf->m_Samplers.m_Data;

        uint32_t sampler_unit = 0;
        for (uint32_t i = 0; i < ddf->m_Samplers.m_Count; i++)
        {
            dmhash_t base_name_hash             = dmHashString64(sampler[i].m_Name);
            dmGraphics::TextureWrap uwrap       = dmRender::WrapFromDDF(sampler[i].m_WrapU);
            dmGraphics::TextureWrap vwrap       = dmRender::WrapFromDDF(sampler[i].m_WrapV);
            dmGraphics::TextureFilter minfilter = dmRender::FilterMinFromDDF(sampler[i].m_FilterMin);
            dmGraphics::TextureFilter magfilter = dmRender::FilterMagFromDDF(sampler[i].m_FilterMag);
            float anisotropy                    = sampler[i].m_MaxAnisotropy;

            uint32_t sampler_unit_before = sampler_unit;
            if (dmRender::SetMaterialSampler(material, base_name_hash, sampler_unit, uwrap, vwrap, minfilter, magfilter, anisotropy))
            {
                sampler_unit++;
            }

            for (int j = 0; j < sampler[i].m_NameIndirections.m_Count; ++j)
            {
                if (dmRender::SetMaterialSampler(material, sampler[i].m_NameIndirections[j], sampler_unit, uwrap, vwrap, minfilter, magfilter, anisotropy))
                {
                    sampler_unit++;
                }
            }

            if (sampler_unit_before == sampler_unit)
            {
                dmLogWarning("Material %s has specified a sampler named '%s', but it does not exist or isn't used in any of the shaders.", path, sampler[i].m_Name);
            }
        }

        // Now we need to sort the textures based on sampler appearance
        for (uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
        {
            uint32_t unit = dmRender::GetMaterialSamplerUnit(material, resources->m_SamplerNames[i]);
            if (unit == dmRender::INVALID_SAMPLER_UNIT)
            {
                continue;
            }
            resource->m_Textures[unit] = resources->m_Textures[i];
            resource->m_SamplerNames[unit] = resources->m_SamplerNames[i];
            resource->m_NumTextures++;
        }
    }

    static dmRender::HMaterial CreateAndInitializeRenderMaterial(dmResource::HFactory factory, dmRender::HRenderContext render_context, const MaterialResources& resources, const dmRenderDDF::MaterialDesc* ddf)
    {
        dmRender::HMaterial material = dmRender::NewMaterial(render_context, resources.m_Program);
        if (!material)
        {
            dmResource::Release(factory, (void*)resources.m_Program);
            return 0;
        }
        return material;
    }

    dmResource::Result ResMaterialCreate(const dmResource::ResourceCreateParams* params)
    {
        dmRender::HRenderContext render_context = (dmRender::HRenderContext) params->m_Context;
        dmRenderDDF::MaterialDesc* ddf = (dmRenderDDF::MaterialDesc*)params->m_PreloadData;
        MaterialResources resources;
        dmResource::Result r = AcquireResources(params->m_Factory, ddf, &resources);
        if (r == dmResource::RESULT_OK)
        {
            dmRender::HMaterial material = CreateAndInitializeRenderMaterial(params->m_Factory, render_context, resources, ddf);

            if (!material)
            {
                return dmResource::RESULT_DDF_ERROR;
            }

            MaterialResource* resource = new MaterialResource;
            memset(resource, 0, sizeof(MaterialResource));

            resource->m_Material = material;
            SetMaterial(params->m_Filename, resource, &resources, ddf);

            dmResource::SetResource(params->m_Resource, resource);
        }
        dmDDF::FreeMessage(ddf);
        return r;
    }

    static void ReleaseMaterialFromResource(dmResource::HFactory factory, MaterialResource* resource, dmRender::HRenderContext render_context)
    {
        ReleaseTextures(factory, resource->m_Textures);

        dmRender::HMaterial material = resource->m_Material;
        dmGraphics::HProgram program = dmRender::GetMaterialProgram(material);

        dmRender::DeleteMaterial(render_context, material);

        dmResource::Release(factory, (void*) program);
    }

    dmResource::Result ResMaterialDestroy(const dmResource::ResourceDestroyParams* params)
    {
        dmRender::HRenderContext render_context = (dmRender::HRenderContext) params->m_Context;
        MaterialResource* resource = (MaterialResource*) dmResource::GetResource(params->m_Resource);

        ReleaseMaterialFromResource(params->m_Factory, resource, render_context);

        delete resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResMaterialRecreate(const dmResource::ResourceRecreateParams* params)
    {
        dmRenderDDF::MaterialDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage<dmRenderDDF::MaterialDesc>(params->m_Buffer, params->m_BufferSize, &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }
        if (!ValidateFormat(ddf))
        {
            dmDDF::FreeMessage(ddf);
            return dmResource::RESULT_FORMAT_ERROR;
        }

        MaterialResources resources;
        dmResource::Result r = AcquireResources(params->m_Factory, ddf, &resources);
        if (r == dmResource::RESULT_OK)
        {
            MaterialResource* resource = (MaterialResource*) dmResource::GetResource(params->m_Resource);
            dmRender::HRenderContext render_context = (dmRender::HRenderContext) params->m_Context;

            ReleaseMaterialFromResource(params->m_Factory, resource, render_context);

            dmRender::HMaterial material = CreateAndInitializeRenderMaterial(params->m_Factory, render_context, resources, ddf);
            if (!material)
            {
                return dmResource::RESULT_DDF_ERROR;
            }

            resource->m_Material = material;
            SetMaterial(params->m_Filename, resource, &resources, ddf);
        }
        dmDDF::FreeMessage(ddf);
        return r;
    }

    dmResource::Result ResMaterialPreload(const dmResource::ResourcePreloadParams* params)
    {
        dmRenderDDF::MaterialDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage<dmRenderDDF::MaterialDesc>(params->m_Buffer, params->m_BufferSize, &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }
        if (!ValidateFormat(ddf))
        {
            dmDDF::FreeMessage(ddf);
            return dmResource::RESULT_FORMAT_ERROR;
        }

        dmResource::PreloadHint(params->m_HintInfo, ddf->m_Program);

        dmRenderDDF::MaterialDesc::Sampler* sampler = ddf->m_Samplers.m_Data;
        for (uint32_t i = 0; i < ddf->m_Samplers.m_Count; i++)
        {
            if (sampler[i].m_Texture && sampler[i].m_Texture[0] != 0)
                dmResource::PreloadHint(params->m_HintInfo, sampler[i].m_Texture);
        }

        *params->m_PreloadData = ddf;
        return dmResource::RESULT_OK;
    }
}
