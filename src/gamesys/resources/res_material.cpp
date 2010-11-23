#include "res_material.h"

#include <dlib/dstrings.h>
#include <dlib/hash.h>

#include <render/material.h>
#include <render/material_ddf.h>

namespace dmGameSystem
{
    dmResource::CreateResult ResMaterialCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        dmRenderDDF::MaterialDesc* material_desc;
        dmDDF::Result e = dmDDF::LoadMessage(buffer, buffer_size, &dmRenderDDF_MaterialDesc_DESCRIPTOR, (void**) &material_desc);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }

        dmResource::FactoryResult factory_e;
        dmGraphics::HVertexProgram vertex_program;
        factory_e = dmResource::Get(factory, material_desc->m_VertexProgram, (void**) &vertex_program);
        if ( factory_e != dmResource::FACTORY_RESULT_OK)
        {
            dmDDF::FreeMessage((void*) material_desc);
            return dmResource::CREATE_RESULT_UNKNOWN;
        }

        dmGraphics::HFragmentProgram fragment_program;
        factory_e = dmResource::Get(factory, material_desc->m_FragmentProgram, (void**) &fragment_program);
        if ( factory_e != dmResource::FACTORY_RESULT_OK)
        {
            dmDDF::FreeMessage((void*) material_desc);
            dmResource::Release(factory, (void*) vertex_program);
            return dmResource::CREATE_RESULT_UNKNOWN;
        }

        dmRender::HMaterial material = dmRender::NewMaterial();
        for (uint32_t i = 0; i < material_desc->m_Tags.m_Count; ++i)
        {
            dmRender::AddMaterialTag(material, dmHashString32(material_desc->m_Tags[i]));
        }
        dmRender::SetMaterialVertexProgram(material, vertex_program);
        dmRender::SetMaterialFragmentProgram(material, fragment_program);

        // save pre-set fragment constants
        if (material_desc->m_FragmentConstants.m_Count < dmRender::MAX_CONSTANT_COUNT)
        {
            for (uint32_t i=0; i<material_desc->m_FragmentConstants.m_Count; i++)
            {
                if (material_desc->m_FragmentConstants[i].m_Register >= dmRender::MAX_CONSTANT_COUNT)
                {
                    dmDDF::FreeMessage((void*) material_desc);
                    dmResource::Release(factory, (void*) vertex_program);
                    dmResource::Release(factory, (void*) fragment_program);

                    dmRender::DeleteMaterial(material);
                    return dmResource::CREATE_RESULT_CONSTANT_ERROR;
                }
                uint32_t reg = material_desc->m_FragmentConstants[i].m_Register;

                dmRender::SetMaterialFragmentProgramConstantType(material, reg, material_desc->m_FragmentConstants[i].m_Type);
                dmRender::SetMaterialFragmentProgramConstant(material, reg, material_desc->m_FragmentConstants[i].m_Value);
                uint32_t mask = dmRender::GetMaterialFragmentConstantMask(material);
                dmRender::SetMaterialFragmentConstantMask(material, mask | 1 << reg);
            }
        }

        // do the same for vertex constants
        if (material_desc->m_VertexConstants.m_Count < dmRender::MAX_CONSTANT_COUNT)
        {
            for (uint32_t i=0; i<material_desc->m_VertexConstants.m_Count; i++)
            {
                if (material_desc->m_VertexConstants[i].m_Register >= dmRender::MAX_CONSTANT_COUNT)
                {
                    dmDDF::FreeMessage((void*) material_desc);
                    dmResource::Release(factory, (void*) vertex_program);
                    dmResource::Release(factory, (void*) fragment_program);

                    dmRender::DeleteMaterial(material);
                    return dmResource::CREATE_RESULT_CONSTANT_ERROR;
                }
                uint32_t reg = material_desc->m_VertexConstants[i].m_Register;
                dmRender::SetMaterialVertexProgramConstantType(material, reg, material_desc->m_VertexConstants[i].m_Type);
                dmRender::SetMaterialVertexProgramConstant(material, reg, material_desc->m_VertexConstants[i].m_Value);
                uint32_t mask = dmRender::GetMaterialVertexConstantMask(material);
                dmRender::SetMaterialVertexConstantMask(material, mask | 1 << reg);
            }
        }

        resource->m_Resource = (void*) material;

        dmDDF::FreeMessage(material_desc);
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResMaterialDestroy(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource)
    {
        dmRender::HMaterial material = (dmRender::HMaterial) resource->m_Resource;

        dmResource::Release(factory, (void*) GetMaterialVertexProgram(material));
        dmResource::Release(factory, (void*) GetMaterialFragmentProgram(material));

        dmRender::DeleteMaterial(material);

        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResMaterialRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        // TODO: Implement me!
        return dmResource::CREATE_RESULT_UNKNOWN;
    }
}
