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

#include "res_material.h"

#include <string.h>
#include <algorithm> // std::sort

#include <dlib/array.h>
#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dmsdk/dlib/hashtable.h>

#include <render/render.h>
#include <render/material_ddf.h>

#include "res_fragment_program.h"
#include "res_vertex_program.h"

namespace dmGameSystem
{
    static dmGraphics::TextureWrap wrap_lut[] = {dmGraphics::TEXTURE_WRAP_REPEAT,
                                                 dmGraphics::TEXTURE_WRAP_MIRRORED_REPEAT,
                                                 dmGraphics::TEXTURE_WRAP_CLAMP_TO_EDGE};

    static dmGraphics::TextureWrap WrapFromDDF(dmRenderDDF::MaterialDesc::WrapMode wrap_mode)
    {
        assert(wrap_mode <= dmRenderDDF::MaterialDesc::WRAP_MODE_CLAMP_TO_EDGE);
        return wrap_lut[wrap_mode];
    }

    static dmGraphics::TextureFilter FilterMinFromDDF(dmRenderDDF::MaterialDesc::FilterModeMin min_filter)
    {
        switch(min_filter)
        {
            case dmRenderDDF::MaterialDesc::FILTER_MODE_MIN_NEAREST:                return dmGraphics::TEXTURE_FILTER_NEAREST;
            case dmRenderDDF::MaterialDesc::FILTER_MODE_MIN_LINEAR:                 return dmGraphics::TEXTURE_FILTER_LINEAR;
            case dmRenderDDF::MaterialDesc::FILTER_MODE_MIN_NEAREST_MIPMAP_NEAREST: return dmGraphics::TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST;
            case dmRenderDDF::MaterialDesc::FILTER_MODE_MIN_NEAREST_MIPMAP_LINEAR:  return dmGraphics::TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR;
            case dmRenderDDF::MaterialDesc::FILTER_MODE_MIN_LINEAR_MIPMAP_NEAREST:  return dmGraphics::TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST;
            case dmRenderDDF::MaterialDesc::FILTER_MODE_MIN_LINEAR_MIPMAP_LINEAR:   return dmGraphics::TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR;
            case dmRenderDDF::MaterialDesc::FILTER_MODE_MIN_DEFAULT:                return dmGraphics::TEXTURE_FILTER_DEFAULT;
            default:break;
        }
        return dmGraphics::TEXTURE_FILTER_DEFAULT;
    }

    static dmGraphics::TextureFilter FilterMagFromDDF(dmRenderDDF::MaterialDesc::FilterModeMag mag_filter)
    {
        switch(mag_filter)
        {
            case dmRenderDDF::MaterialDesc::FILTER_MODE_MAG_NEAREST: return dmGraphics::TEXTURE_FILTER_NEAREST;
            case dmRenderDDF::MaterialDesc::FILTER_MODE_MAG_LINEAR:  return dmGraphics::TEXTURE_FILTER_LINEAR;
            case dmRenderDDF::MaterialDesc::FILTER_MODE_MAG_DEFAULT: return dmGraphics::TEXTURE_FILTER_DEFAULT;
            default:break;
        }
        return dmGraphics::TEXTURE_FILTER_DEFAULT;
    }

    static bool ValidateFormat(dmRenderDDF::MaterialDesc* material_desc)
    {
        if (strlen(material_desc->m_Name) == 0)
            return false;
        return true;
    }

    struct MaterialResources
    {
        MaterialResources() : m_FragmentProgram(0), m_VertexProgram(0) {}

        dmGraphics::HFragmentProgram    m_FragmentProgram;
        dmGraphics::HVertexProgram      m_VertexProgram;
        TextureResource*                m_Textures[dmRender::RenderObject::MAX_TEXTURE_COUNT];
        dmhash_t                        m_SamplerNames[dmRender::RenderObject::MAX_TEXTURE_COUNT];
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
        if (resources->m_FragmentProgram)
            dmResource::Release(factory, (void*)resources->m_FragmentProgram);
        resources->m_FragmentProgram = 0;
        if (resources->m_VertexProgram)
            dmResource::Release(factory, (void*)resources->m_VertexProgram);
        resources->m_VertexProgram = 0;

        ReleaseTextures(factory, resources->m_Textures);
    }

    static dmResource::Result AcquireResources(dmResource::HFactory factory, dmRenderDDF::MaterialDesc* ddf, MaterialResources* resources)
    {
        memset(resources->m_Textures, 0, sizeof(resources->m_Textures));
        memset(resources->m_SamplerNames, 0, sizeof(resources->m_SamplerNames));

        dmResource::Result factory_e;
        factory_e = dmResource::Get(factory, ddf->m_VertexProgram, (void**) &resources->m_VertexProgram);
        if ( factory_e != dmResource::RESULT_OK)
        {
            ReleaseResources(factory, resources);
            return factory_e;
        }

        factory_e = dmResource::Get(factory, ddf->m_FragmentProgram, (void**) &resources->m_FragmentProgram);
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

    static void ResourceReloadedCallback(const dmResource::ResourceReloadedParams& params)
    {
        dmRender::HMaterial material = (dmRender::HMaterial) params.m_UserData;

        uint64_t vertex_name_hash = dmRender::GetMaterialUserData1(material);
        uint64_t fragment_name_hash = dmRender::GetMaterialUserData2(material);

        if (params.m_Resource->m_NameHash == vertex_name_hash || params.m_Resource->m_NameHash == fragment_name_hash)
        {
            dmRender::HRenderContext render_context = dmRender::GetMaterialRenderContext(material);
            dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(render_context);
            dmGraphics::HProgram program = dmRender::GetMaterialProgram(material);
            dmGraphics::HVertexProgram vert_program = dmRender::GetMaterialVertexProgram(material);
            dmGraphics::HFragmentProgram frag_program = dmRender::GetMaterialFragmentProgram(material);

            if (!dmGraphics::ReloadProgram(graphics_context, program, vert_program, frag_program))
            {
                dmLogWarning("Reloading the material failed, some shaders might not have been correctly linked.");
            }
        }
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

        // save pre-set fragment constants
        for (uint32_t i = 0; i < fragment_count; i++)
        {
            const char* name = fragment_constant[i].m_Name;
            dmhash_t name_hash = dmHashString64(name);
            dmRender::SetMaterialProgramConstantType(material, name_hash, fragment_constant[i].m_Type);
            dmRender::SetMaterialProgramConstant(material, name_hash,
                (dmVMath::Vector4*) fragment_constant[i].m_Value.m_Data, fragment_constant[i].m_Value.m_Count);
        }
        // do the same for vertex constants
        for (uint32_t i = 0; i < vertex_count; i++)
        {
            const char* name = vertex_constant[i].m_Name;
            dmhash_t name_hash = dmHashString64(name);
            dmRender::SetMaterialProgramConstantType(material, name_hash, vertex_constant[i].m_Type);
            dmRender::SetMaterialProgramConstant(material, name_hash,
                (dmVMath::Vector4*) vertex_constant[i].m_Value.m_Data, vertex_constant[i].m_Value.m_Count);
        }

        // Set vertex attributes
        dmRender::SetMaterialProgramAttributes(material, ddf->m_Attributes.m_Data, ddf->m_Attributes.m_Count);

        dmRenderDDF::MaterialDesc::Sampler* sampler = ddf->m_Samplers.m_Data;

        uint32_t sampler_unit = 0;
        for (uint32_t i = 0; i < ddf->m_Samplers.m_Count; i++)
        {
            dmhash_t base_name_hash             = dmHashString64(sampler[i].m_Name);
            dmGraphics::TextureWrap uwrap       = WrapFromDDF(sampler[i].m_WrapU);
            dmGraphics::TextureWrap vwrap       = WrapFromDDF(sampler[i].m_WrapV);
            dmGraphics::TextureFilter minfilter = FilterMinFromDDF(sampler[i].m_FilterMin);
            dmGraphics::TextureFilter magfilter = FilterMagFromDDF(sampler[i].m_FilterMag);
            float anisotropy                    = sampler[i].m_MaxAnisotropy;

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
        }

        // Now we need to sort the textures based on sampler appearance
        for (uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
        {
            uint32_t unit = dmRender::GetMaterialSamplerUnit(material, resources->m_SamplerNames[i]);
            if (unit == 0xFFFFFFFF)
                continue;
            resource->m_Textures[unit] = resources->m_Textures[i];
            resource->m_SamplerNames[unit] = resources->m_SamplerNames[i];
            resource->m_NumTextures++;
        }
    }

    dmResource::Result ResMaterialCreate(const dmResource::ResourceCreateParams& params)
    {
        dmRender::HRenderContext render_context = (dmRender::HRenderContext) params.m_Context;
        dmRenderDDF::MaterialDesc* ddf = (dmRenderDDF::MaterialDesc*)params.m_PreloadData;
        MaterialResources resources;
        dmResource::Result r = AcquireResources(params.m_Factory, ddf, &resources);
        if (r == dmResource::RESULT_OK)
        {
            dmRender::HMaterial material = dmRender::NewMaterial(render_context, resources.m_VertexProgram, resources.m_FragmentProgram);
            if (!material)
            {
                dmResource::Release(params.m_Factory, (void*)resources.m_VertexProgram);
                dmResource::Release(params.m_Factory, (void*)resources.m_FragmentProgram);
                return dmResource::RESULT_DDF_ERROR;
            }

            dmResource::SResourceDescriptor desc;
            dmResource::Result factory_e;

            factory_e = dmResource::GetDescriptor(params.m_Factory, ddf->m_VertexProgram, &desc);
            assert(factory_e == dmResource::RESULT_OK); // Should not fail at this point
            dmRender::SetMaterialUserData1(material, desc.m_NameHash);

            factory_e = dmResource::GetDescriptor(params.m_Factory, ddf->m_FragmentProgram, &desc);
            assert(factory_e == dmResource::RESULT_OK); // Should not fail at this point
            dmRender::SetMaterialUserData2(material, desc.m_NameHash);

            dmResource::RegisterResourceReloadedCallback(params.m_Factory, ResourceReloadedCallback, material);

            MaterialResource* resource = new MaterialResource;
            memset(resource, 0, sizeof(MaterialResource));

            resource->m_Material = material;
            SetMaterial(params.m_Filename, resource, &resources, ddf);

            params.m_Resource->m_Resource = (void*) resource;
        }
        dmDDF::FreeMessage(ddf);
        return r;
    }

    dmResource::Result ResMaterialDestroy(const dmResource::ResourceDestroyParams& params)
    {
        dmRender::HRenderContext render_context = (dmRender::HRenderContext) params.m_Context;
        MaterialResource* resource = (MaterialResource*) params.m_Resource->m_Resource;

        ReleaseTextures(params.m_Factory, resource->m_Textures);

        dmRender::HMaterial material = resource->m_Material;
        dmResource::UnregisterResourceReloadedCallback(params.m_Factory, ResourceReloadedCallback, material);

        dmResource::Release(params.m_Factory, (void*)dmRender::GetMaterialFragmentProgram(material));
        dmResource::Release(params.m_Factory, (void*)dmRender::GetMaterialVertexProgram(material));
        dmRender::DeleteMaterial(render_context, material);

        return dmResource::RESULT_OK;
    }

    dmResource::Result ResMaterialRecreate(const dmResource::ResourceRecreateParams& params)
    {
        dmRenderDDF::MaterialDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage<dmRenderDDF::MaterialDesc>(params.m_Buffer, params.m_BufferSize, &ddf);
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
        dmResource::Result r = AcquireResources(params.m_Factory, ddf, &resources);
        if (r == dmResource::RESULT_OK)
        {
            MaterialResource* resource = (MaterialResource*) params.m_Resource->m_Resource;
            // Release old resources
            ReleaseTextures(params.m_Factory, resource->m_Textures);
            dmResource::Release(params.m_Factory, (void*)dmRender::GetMaterialFragmentProgram(resource->m_Material));
            dmResource::Release(params.m_Factory, (void*)dmRender::GetMaterialVertexProgram(resource->m_Material));
            dmRender::ClearMaterialTags(resource->m_Material);
            // Set up resources
            SetMaterial(params.m_Filename, resource, &resources, ddf);
        }
        dmDDF::FreeMessage(ddf);
        return r;
    }

    dmResource::Result ResMaterialPreload(const dmResource::ResourcePreloadParams& params)
    {
        dmRenderDDF::MaterialDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage<dmRenderDDF::MaterialDesc>(params.m_Buffer, params.m_BufferSize, &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }
        if (!ValidateFormat(ddf))
        {
            dmDDF::FreeMessage(ddf);
            return dmResource::RESULT_FORMAT_ERROR;
        }

        dmResource::PreloadHint(params.m_HintInfo, ddf->m_VertexProgram);
        dmResource::PreloadHint(params.m_HintInfo, ddf->m_FragmentProgram);

        dmRenderDDF::MaterialDesc::Sampler* sampler = ddf->m_Samplers.m_Data;
        for (uint32_t i = 0; i < ddf->m_Samplers.m_Count; i++)
        {
            if (sampler[i].m_Texture && sampler[i].m_Texture[0] != 0)
                dmResource::PreloadHint(params.m_HintInfo, sampler[i].m_Texture);
        }

        *params.m_PreloadData = ddf;
        return dmResource::RESULT_OK;
    }
}
