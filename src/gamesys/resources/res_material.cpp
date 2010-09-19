#include "res_material.h"

#include <dlib/dstrings.h>

#include <graphics/material.h>

#include <render/material_ddf.h>

namespace dmGameSystem
{
    dmResource::CreateResult ResCreateMaterial(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        dmRender::MaterialDesc* material_desc;
        dmDDF::Result e = dmDDF::LoadMessage(buffer, buffer_size, &dmRender_MaterialDesc_DESCRIPTOR, (void**) &material_desc);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }

        char vertex_program_buf[1024];
        DM_SNPRINTF(vertex_program_buf, sizeof(vertex_program_buf), "%s.arbvp", material_desc->m_VertexProgram);
        char fragment_program_buf[1024];
        DM_SNPRINTF(fragment_program_buf, sizeof(fragment_program_buf), "%s.arbfp", material_desc->m_FragmentProgram);

        dmResource::FactoryResult factory_e;
        dmGraphics::HVertexProgram vertex_program;
        factory_e = dmResource::Get(factory, vertex_program_buf, (void**) &vertex_program);
        if ( factory_e != dmResource::FACTORY_RESULT_OK)
        {
            dmDDF::FreeMessage((void*) material_desc);
            dmResource::Release(factory, (void*) vertex_program);
            return dmResource::CREATE_RESULT_UNKNOWN;
        }

        dmGraphics::HFragmentProgram fragment_program;
        factory_e = dmResource::Get(factory, fragment_program_buf, (void**) &fragment_program);
        if ( factory_e != dmResource::FACTORY_RESULT_OK)
        {
            dmDDF::FreeMessage((void*) material_desc);
            dmResource::Release(factory, (void*) vertex_program);
            dmResource::Release(factory, (void*) fragment_program);
            return dmResource::CREATE_RESULT_UNKNOWN;
        }

        dmGraphics::HMaterial material = dmGraphics::NewMaterial();
        dmGraphics::SetMaterialVertexProgram(material, vertex_program);
        dmGraphics::SetMaterialFragmentProgram(material, fragment_program);

        // save pre-set fragment constants
        if (material_desc->m_FragmentParameters.m_Count < dmGraphics::MAX_MATERIAL_CONSTANTS)
        {
            for (uint32_t i=0; i<material_desc->m_FragmentParameters.m_Count; i++)
            {
                if (material_desc->m_FragmentParameters[i].m_Register >= dmGraphics::MAX_MATERIAL_CONSTANTS)
                {
                    dmDDF::FreeMessage((void*) material_desc);
                    dmResource::Release(factory, (void*) vertex_program);
                    dmResource::Release(factory, (void*) fragment_program);

                    dmGraphics::DeleteMaterial(material);
                    return dmResource::CREATE_RESULT_CONSTANT_ERROR;
                }
                uint32_t reg = material_desc->m_FragmentParameters[i].m_Semantic;

                // TODO: fix when Vector4 can be used properly in ddf
                Vector4 v(material_desc->m_FragmentParameters[i].m_Value.m_x,
                            material_desc->m_FragmentParameters[i].m_Value.m_y,
                            material_desc->m_FragmentParameters[i].m_Value.m_z,
                            material_desc->m_FragmentParameters[i].m_Value.m_w);

                dmGraphics::SetMaterialFragmentProgramConstant(material, reg, v);
                uint32_t mask = dmGraphics::GetMaterialFragmentConstantMask(material);
                dmGraphics::SetMaterialFragmentConstantMask(material, mask | 1 << reg);
            }
        }

        // do the same for vertex constants
        if (material_desc->m_VertexParameters.m_Count < dmGraphics::MAX_MATERIAL_CONSTANTS)
        {
            for (uint32_t i=0; i<material_desc->m_VertexParameters.m_Count; i++)
            {
                if (material_desc->m_VertexParameters[i].m_Register >= dmGraphics::MAX_MATERIAL_CONSTANTS)
                {
                    dmDDF::FreeMessage((void*) material_desc);
                    dmResource::Release(factory, (void*) vertex_program);
                    dmResource::Release(factory, (void*) fragment_program);

                    dmGraphics::DeleteMaterial(material);
                    return dmResource::CREATE_RESULT_CONSTANT_ERROR;
                }
                uint32_t reg = material_desc->m_VertexParameters[i].m_Register;
                // TODO: fix when Vector4 can be used properly in ddf
                Vector4 v(material_desc->m_VertexParameters[i].m_Value.m_x,
                            material_desc->m_VertexParameters[i].m_Value.m_y,
                            material_desc->m_VertexParameters[i].m_Value.m_z,
                            material_desc->m_VertexParameters[i].m_Value.m_w);

                dmGraphics::SetMaterialVertexProgramConstant(material, reg, v);
                uint32_t mask = dmGraphics::GetMaterialVertexConstantMask(material);
                dmGraphics::SetMaterialVertexConstantMask(material, mask | 1 << reg);
            }
        }

        resource->m_Resource = (void*) material;

        dmDDF::FreeMessage(material_desc);
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResDestroyMaterial(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource)
    {
        dmGraphics::HMaterial material = (dmGraphics::HMaterial) resource->m_Resource;

        dmResource::Release(factory, (void*) GetMaterialVertexProgram(material));
        dmResource::Release(factory, (void*) GetMaterialFragmentProgram(material));

        dmGraphics::DeleteMaterial(material);

        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResRecreateMaterial(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        // TODO: Implement me!
        return dmResource::CREATE_RESULT_UNKNOWN;
    }
}
