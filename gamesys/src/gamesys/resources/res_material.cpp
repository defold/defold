#include "res_material.h"

#include <string.h>

#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/log.h>

#include <render/render.h>
#include <render/material_ddf.h>

namespace dmGameSystem
{
    struct MaterialResources
    {
        MaterialResources() : m_DDF(0x0), m_FragmentProgram(0), m_VertexProgram(0) {}

        dmRenderDDF::MaterialDesc* m_DDF;
        dmGraphics::HFragmentProgram m_FragmentProgram;
        dmGraphics::HVertexProgram m_VertexProgram;
    };

    bool ValidateFormat(dmRenderDDF::MaterialDesc* material_desc)
    {
        if (strlen(material_desc->m_Name) == 0)
            return false;
        return true;
    }

    bool AcquireResources(dmResource::HFactory factory, const void* buffer, uint32_t buffer_size, MaterialResources* resources, const char* filename)
    {
        dmDDF::Result e = dmDDF::LoadMessage(buffer, buffer_size, &dmRenderDDF_MaterialDesc_DESCRIPTOR, (void**) &resources->m_DDF);
        if ( e != dmDDF::RESULT_OK )
            return false;
        if (!ValidateFormat(resources->m_DDF))
            return false;

        dmResource::FactoryResult factory_e;
        factory_e = dmResource::Get(factory, resources->m_DDF->m_VertexProgram, (void**) &resources->m_VertexProgram);
        if ( factory_e != dmResource::FACTORY_RESULT_OK)
            return false;

        factory_e = dmResource::Get(factory, resources->m_DDF->m_FragmentProgram, (void**) &resources->m_FragmentProgram);
        if ( factory_e != dmResource::FACTORY_RESULT_OK)
            return false;

        return true;
    }

    static void ResourceReloadedCallback(void* user_data, dmResource::SResourceDescriptor* descriptor, const char* name)
    {
        dmRender::HMaterial material = (dmRender::HMaterial) user_data;

        uint64_t vertex_name_hash = dmRender::GetMaterialUserData1(material);
        uint64_t fragment_name_hash = dmRender::GetMaterialUserData2(material);

        if (descriptor->m_NameHash == vertex_name_hash || descriptor->m_NameHash == fragment_name_hash)
        {
            dmRender::HRenderContext render_context = dmRender::GetMaterialRenderContext(material);
            dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(render_context);
            dmGraphics::HProgram program = dmRender::GetMaterialProgram(material);
            dmGraphics::ReloadProgram(graphics_context, program);
        }
    }

    void SetMaterial(dmRender::HMaterial material, MaterialResources* resources)
    {
        for (uint32_t i = 0; i < resources->m_DDF->m_Tags.m_Count; ++i)
        {
            dmRender::AddMaterialTag(material, dmHashString32(resources->m_DDF->m_Tags[i]));
        }

        dmRenderDDF::MaterialDesc::Constant* fragment_constant = resources->m_DDF->m_FragmentConstants.m_Data;
        dmRenderDDF::MaterialDesc::Constant* vertex_constant = resources->m_DDF->m_VertexConstants.m_Data;

        uint32_t fragment_count = resources->m_DDF->m_FragmentConstants.m_Count;
        uint32_t vertex_count = resources->m_DDF->m_VertexConstants.m_Count;

        // save pre-set fragment constants
        for (uint32_t i = 0; i < fragment_count; i++)
        {
            const char* name = fragment_constant[i].m_Name;
            dmhash_t name_hash = dmHashString64(name);
            dmRender::SetMaterialProgramConstantType(material, name_hash, resources->m_DDF->m_FragmentConstants[i].m_Type);
            dmRender::SetMaterialProgramConstant(material, name_hash, resources->m_DDF->m_FragmentConstants[i].m_Value);
        }
        // do the same for vertex constants
        for (uint32_t i = 0; i < vertex_count; i++)
        {
            const char* name = vertex_constant[i].m_Name;
            dmhash_t name_hash = dmHashString64(name);
            dmRender::SetMaterialProgramConstantType(material, name_hash, resources->m_DDF->m_VertexConstants[i].m_Type);
            dmRender::SetMaterialProgramConstant(material, name_hash, resources->m_DDF->m_VertexConstants[i].m_Value);
        }


        const char** textures = resources->m_DDF->m_Textures.m_Data;
        uint32_t texture_count = resources->m_DDF->m_Textures.m_Count;
        for (uint32_t i = 0; i < texture_count; i++)
        {
            dmhash_t name_hash = dmHashString64(textures[i]);
            dmRender::SetMaterialSampler(material, name_hash, i);
        }

        dmDDF::FreeMessage(resources->m_DDF);
    }

    void ReleaseResources(dmResource::HFactory factory, MaterialResources* resources)
    {
        if (resources->m_DDF)
            dmDDF::FreeMessage(resources->m_DDF);
        if (resources->m_FragmentProgram)
            dmResource::Release(factory, (void*)resources->m_FragmentProgram);
        if (resources->m_VertexProgram)
            dmResource::Release(factory, (void*)resources->m_VertexProgram);
    }

    dmResource::CreateResult ResMaterialCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        dmRender::HRenderContext render_context = (dmRender::HRenderContext) context;
        MaterialResources resources;
        if (AcquireResources(factory, buffer, buffer_size, &resources, filename))
        {
            dmRender::HMaterial material = dmRender::NewMaterial(render_context, resources.m_VertexProgram, resources.m_FragmentProgram);

            dmResource::SResourceDescriptor desc;
            dmResource::FactoryResult factory_e;

            factory_e = dmResource::GetDescriptor(factory, resources.m_DDF->m_VertexProgram, &desc);
            assert(factory_e == dmResource::FACTORY_RESULT_OK); // Should not fail at this point
            dmRender::SetMaterialUserData1(material, desc.m_NameHash);

            factory_e = dmResource::GetDescriptor(factory, resources.m_DDF->m_FragmentProgram, &desc);
            assert(factory_e == dmResource::FACTORY_RESULT_OK); // Should not fail at this point
            dmRender::SetMaterialUserData2(material, desc.m_NameHash);

            dmResource::RegisterResourceReloadedCallback(factory, ResourceReloadedCallback, material);

            SetMaterial(material, &resources);
            resource->m_Resource = (void*) material;
            return dmResource::CREATE_RESULT_OK;
        }
        else
        {
            ReleaseResources(factory, &resources);
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
    }

    dmResource::CreateResult ResMaterialDestroy(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource)
    {
        dmRender::HRenderContext render_context = (dmRender::HRenderContext) context;
        dmRender::HMaterial material = (dmRender::HMaterial) resource->m_Resource;
        dmResource::UnregisterResourceReloadedCallback(factory, ResourceReloadedCallback, material);

        dmResource::Release(factory, (void*)dmRender::GetMaterialFragmentProgram(material));
        dmResource::Release(factory, (void*)dmRender::GetMaterialVertexProgram(material));
        dmRender::DeleteMaterial(render_context, material);

        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResMaterialRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        MaterialResources resources;
        if (AcquireResources(factory, buffer, buffer_size, &resources, filename))
        {
            dmRender::HMaterial material = (dmRender::HMaterial) resource->m_Resource;
            dmResource::Release(factory, (void*)dmRender::GetMaterialFragmentProgram(material));
            dmResource::Release(factory, (void*)dmRender::GetMaterialVertexProgram(material));
            dmRender::ClearMaterialTags(material);
            SetMaterial(material, &resources);
            return dmResource::CREATE_RESULT_OK;
        }
        else
        {
            ReleaseResources(factory, &resources);
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
    }
}
