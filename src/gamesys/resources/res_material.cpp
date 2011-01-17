#include "res_material.h"

#include <string.h>

#include <dlib/dstrings.h>
#include <dlib/hash.h>

#include <render/material.h>
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

        // check pre-set fragment constants
        if (material_desc->m_FragmentConstants.m_Count < dmRender::MAX_CONSTANT_COUNT)
        {
            for (uint32_t i = 0; i < material_desc->m_FragmentConstants.m_Count; i++)
            {
                if (material_desc->m_FragmentConstants[i].m_Register >= dmRender::MAX_CONSTANT_COUNT)
                {
                    return false;
                }
            }
        }
        // do the same for vertex constants
        if (material_desc->m_VertexConstants.m_Count < dmRender::MAX_CONSTANT_COUNT)
        {
            for (uint32_t i=0; i<material_desc->m_VertexConstants.m_Count; i++)
            {
                if (material_desc->m_VertexConstants[i].m_Register >= dmRender::MAX_CONSTANT_COUNT)
                {
                    return false;
                }
            }
        }
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

    void SetMaterial(dmRender::HMaterial material, MaterialResources* resources)
    {
        for (uint32_t i = 0; i < resources->m_DDF->m_Tags.m_Count; ++i)
        {
            dmRender::AddMaterialTag(material, dmHashString32(resources->m_DDF->m_Tags[i]));
        }
        dmRender::SetMaterialVertexProgram(material, resources->m_VertexProgram);
        dmRender::SetMaterialFragmentProgram(material, resources->m_FragmentProgram);

        // save pre-set fragment constants
        if (resources->m_DDF->m_FragmentConstants.m_Count < dmRender::MAX_CONSTANT_COUNT)
        {
            for (uint32_t i = 0; i < resources->m_DDF->m_FragmentConstants.m_Count; i++)
            {
                uint32_t reg = resources->m_DDF->m_FragmentConstants[i].m_Register;
                dmRender::SetMaterialFragmentProgramConstantType(material, reg, resources->m_DDF->m_FragmentConstants[i].m_Type);
                dmRender::SetMaterialFragmentProgramConstant(material, reg, resources->m_DDF->m_FragmentConstants[i].m_Value);
                uint32_t mask = dmRender::GetMaterialFragmentConstantMask(material);
                dmRender::SetMaterialFragmentConstantMask(material, mask | 1 << reg);
            }
        }
        // do the same for vertex constants
        if (resources->m_DDF->m_VertexConstants.m_Count < dmRender::MAX_CONSTANT_COUNT)
        {
            for (uint32_t i = 0; i < resources->m_DDF->m_VertexConstants.m_Count; i++)
            {
                uint32_t reg = resources->m_DDF->m_VertexConstants[i].m_Register;
                dmRender::SetMaterialVertexProgramConstantType(material, reg, resources->m_DDF->m_VertexConstants[i].m_Type);
                dmRender::SetMaterialVertexProgramConstant(material, reg, resources->m_DDF->m_VertexConstants[i].m_Value);
                uint32_t mask = dmRender::GetMaterialVertexConstantMask(material);
                dmRender::SetMaterialVertexConstantMask(material, mask | 1 << reg);
            }
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
        MaterialResources resources;
        if (AcquireResources(factory, buffer, buffer_size, &resources, filename))
        {
            dmRender::HMaterial material = dmRender::NewMaterial();
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
        dmRender::HMaterial material = (dmRender::HMaterial) resource->m_Resource;

        dmResource::Release(factory, (void*)dmRender::GetMaterialFragmentProgram(material));
        dmResource::Release(factory, (void*)dmRender::GetMaterialVertexProgram(material));
        dmRender::DeleteMaterial(material);

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
            dmRender::SetMaterialFragmentConstantMask(material, 0);
            dmRender::SetMaterialVertexConstantMask(material, 0);
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
