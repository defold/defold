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

#include "res_compute.h"
#include "res_texture.h"

#include <render/render.h>
#include <render/compute_ddf.h>

namespace dmGameSystem
{
    struct ComputeProgramResources
    {
        dmGraphics::HProgram m_ComputeProgram;
        TextureResource*     m_Textures[dmRender::RenderObject::MAX_TEXTURE_COUNT];
        dmhash_t             m_SamplerNames[dmRender::RenderObject::MAX_TEXTURE_COUNT];
    };

    static void ReleaseTextures(dmResource::HFactory factory, TextureResource** textures)
    {
        for (uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
        {
            if (textures[i])
            {
                dmResource::Release(factory, (void*)textures[i]);
            }
        }
        memset(textures, 0, sizeof(TextureResource*)*dmRender::RenderObject::MAX_TEXTURE_COUNT);
    }

    static void ReleaseResources(dmResource::HFactory factory, ComputeProgramResources* resources)
    {
        if (resources->m_ComputeProgram)
        {
            dmResource::Release(factory, (void*) resources->m_ComputeProgram);
        }
        resources->m_ComputeProgram = 0;

        ReleaseTextures(factory, resources->m_Textures);
    }

    static dmResource::Result AcquireResources(dmResource::HFactory factory, dmRenderDDF::ComputeDesc* ddf, ComputeProgramResources* resources)
    {
        dmResource::Result factory_e;
        factory_e = dmResource::Get(factory, ddf->m_ComputeProgram, (void**) &resources->m_ComputeProgram);
        if ( factory_e != dmResource::RESULT_OK)
        {
            ReleaseResources(factory, resources);
            return factory_e;
        }

        // From res_material.cpp:
        //
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

    static void SetProgram(const char* path, ComputeResource* resource, dmRenderDDF::ComputeDesc* ddf, ComputeProgramResources* resources)
    {
        //////////////////////////////
        // Set program constants
        //////////////////////////////
        dmRenderDDF::MaterialDesc::Constant* constants = ddf->m_Constants.m_Data;
        uint32_t constants_count = ddf->m_Constants.m_Count;

        for (uint32_t i = 0; i < constants_count; i++)
        {
            const char* name   = constants[i].m_Name;
            dmhash_t name_hash = dmHashString64(name);

            dmRender::HConstant constant;
            if (dmRender::GetComputeProgramConstant(resource->m_Program, name_hash, constant))
            {
                dmRender::SetComputeProgramConstantType(resource->m_Program, name_hash, constants[i].m_Type);
                dmRender::SetComputeProgramConstant(resource->m_Program, name_hash, (dmVMath::Vector4*) constants[i].m_Value.m_Data, constants[i].m_Value.m_Count);
            }
            else
            {
                dmLogWarning("Compute %s has specified a constant named '%s', but it does not exist or isn't used in the shader.", path, name);
            }
        }

        //////////////////////////////
        // Set program samplers
        //////////////////////////////
        dmRenderDDF::MaterialDesc::Sampler* sampler = ddf->m_Samplers.m_Data;

        uint32_t sampler_unit = 0;
        for (uint32_t i = 0; i < ddf->m_Samplers.m_Count; i++)
        {
            // Note from PR (https://github.com/defold/defold/pull/8975):
            // The sampler does have a m_NameHash.
            // The downside would be that the reverse lookup of hashes, wouldn't work (need to hash the string at least once)
            // Perhaps we should add a helper: dmHashAddDebugString(hash, string), which we can then disable in release mode.
            dmhash_t base_name_hash             = dmHashString64(sampler[i].m_Name);
            dmGraphics::TextureWrap uwrap       = dmRender::WrapFromDDF(sampler[i].m_WrapU);
            dmGraphics::TextureWrap vwrap       = dmRender::WrapFromDDF(sampler[i].m_WrapV);
            dmGraphics::TextureFilter minfilter = dmRender::FilterMinFromDDF(sampler[i].m_FilterMin);
            dmGraphics::TextureFilter magfilter = dmRender::FilterMagFromDDF(sampler[i].m_FilterMag);
            float anisotropy                    = sampler[i].m_MaxAnisotropy;

            uint32_t sampler_unit_before = sampler_unit;
            if (dmRender::SetComputeProgramSampler(resource->m_Program, base_name_hash, sampler_unit, uwrap, vwrap, minfilter, magfilter, anisotropy))
            {
                sampler_unit++;
            }

            for (int j = 0; j < sampler[i].m_NameIndirections.m_Count; ++j)
            {
                if (dmRender::SetComputeProgramSampler(resource->m_Program, sampler[i].m_NameIndirections[j], sampler_unit, uwrap, vwrap, minfilter, magfilter, anisotropy))
                {
                    sampler_unit++;
                }
            }

            if (sampler_unit_before == sampler_unit)
            {
                dmLogWarning("Compute %s has specified a sampler named '%s', but it does not exist or isn't used in the shader.", path, sampler[i].m_Name);
            }
        }

        ////////////////////////////////////////////////////////////
        // Sort the textures based on sampler appearance
        ////////////////////////////////////////////////////////////
        for (uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
        {
            if (resources->m_SamplerNames[i])
            {
                uint32_t unit = dmRender::GetComputeProgramSamplerUnit(resource->m_Program, resources->m_SamplerNames[i]);
                if (unit == dmRender::INVALID_SAMPLER_UNIT)
                {
                    continue;
                }
                resource->m_Textures[unit]     = resources->m_Textures[i];
                resource->m_SamplerNames[unit] = resources->m_SamplerNames[i];
                resource->m_NumTextures++;
            }
        }
    }

    dmResource::Result ResComputeCreate(const dmResource::ResourceCreateParams* params)
    {
        dmRender::HRenderContext render_context = (dmRender::HRenderContext) params->m_Context;
        dmRenderDDF::ComputeDesc* ddf   = (dmRenderDDF::ComputeDesc*)params->m_PreloadData;

        ComputeProgramResources resources = {};
        dmResource::Result r = AcquireResources(params->m_Factory, ddf, &resources);
        if (r == dmResource::RESULT_OK)
        {
            dmRender::HComputeProgram compute_program = dmRender::NewComputeProgram(render_context, resources.m_ComputeProgram);

            HResourceDescriptor desc;
            dmResource::Result res = dmResource::GetDescriptor(params->m_Factory, ddf->m_ComputeProgram, &desc);
            assert(res == dmResource::RESULT_OK);

            ComputeResource* resource = new ComputeResource();
            resource->m_Program       = compute_program;
            SetProgram(params->m_Filename, resource, ddf, &resources);

            ResourceDescriptorSetResource(params->m_Resource, resource);
        }
        dmDDF::FreeMessage(ddf);
        return r;
    }

    dmResource::Result ResComputeDestroy(const dmResource::ResourceDestroyParams* params)
    {
        ComputeResource* resource               = (ComputeResource*) dmResource::GetResource(params->m_Resource);
        dmRender::HRenderContext render_context = (dmRender::HRenderContext) params->m_Context;
        dmRender::HComputeProgram program       = resource->m_Program;
        dmGraphics::HProgram gfx_program        = dmRender::GetComputeProgram(program);

        ReleaseTextures(params->m_Factory, resource->m_Textures);

        dmResource::Release(params->m_Factory, (void*) gfx_program);
        dmRender::DeleteComputeProgram(render_context, program);

        delete resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResComputeRecreate(const dmResource::ResourceRecreateParams* params)
    {
        dmRenderDDF::ComputeDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage<dmRenderDDF::ComputeDesc>(params->m_Buffer, params->m_BufferSize, &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }

        ComputeProgramResources resources = {};
        dmResource::Result r = AcquireResources(params->m_Factory, ddf, &resources);

        if (r == dmResource::RESULT_OK)
        {
            ComputeResource* resource         = (ComputeResource*) dmResource::GetResource(params->m_Resource);
            dmRender::HComputeProgram program = resource->m_Program;

            dmGraphics::HProgram gfx_program = dmRender::GetComputeProgram(program);

            // Release old resources
            dmResource::Release(params->m_Factory, (void*) gfx_program);

            // Set up resources
            SetProgram(params->m_Filename, resource, ddf, &resources);
        }
        dmDDF::FreeMessage(ddf);
        return r;
    }

    dmResource::Result ResComputePreload(const dmResource::ResourcePreloadParams* params)
    {
        dmRenderDDF::ComputeDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage<dmRenderDDF::ComputeDesc>(params->m_Buffer, params->m_BufferSize, &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }

        dmResource::PreloadHint(params->m_HintInfo, ddf->m_ComputeProgram);
        *params->m_PreloadData = ddf;
        return dmResource::RESULT_OK;
    }
}
