#include "material.h"

#include <dlib/dstrings.h>
// TODO: Remove after haxx below is removed
#include <dlib/hashtable.h>

#include <graphics/graphics_device.h>
#include <graphics/material.h>

#include "render/material_ddf.h"

namespace dmMaterial
{
    struct Resource
    {
        dmRender::MaterialDesc* m_DDF;
        dmGraphics::HVertexProgram m_VertexProgram;
        dmGraphics::HFragmentProgram m_FragmentProgram;
    };

    // TODO: Fix this ugly thing! Materials as resources should be stored using a struct like Resource above so no resources are lost when it's time to destroy.
    // Currently the model resource only store dmGraphics::HMaterial's so some more refactoring is needed to get it to work.
    // Right now material is leaky too when e.g. a fragment program can't be constructed and the vertex program is leaked.
    static dmHashTable<uintptr_t, Resource*> s_MaterialResources;
    static bool s_Initialized = false;

    dmResource::CreateResult CreateResource(dmResource::HFactory factory,
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

        // TODO: Remove haxx, see above
        if (!s_Initialized)
        {
            s_MaterialResources.SetCapacity(32, 128);
            s_Initialized = true;
        }
        Resource* material_resources = new Resource();
        material_resources->m_DDF = material_desc;
        material_resources->m_VertexProgram = vertex_program;
        material_resources->m_FragmentProgram = fragment_program;
        assert(s_MaterialResources.Get((uintptr_t)material) == 0x0);
        s_MaterialResources.Put((uintptr_t)material, material_resources);

        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult DestroyResource(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource)
    {
        dmGraphics::HMaterial m = (dmGraphics::HMaterial) resource->m_Resource;

        // TODO: Remove haxx, see above
        // Crash hard if there is no material in the hashtable, it means we are leaking anyway
        Resource* material_resource = *s_MaterialResources.Get((uintptr_t)m);
        assert(material_resource);
        dmDDF::FreeMessage((void*) material_resource->m_DDF);
        dmResource::Release(factory, (void*) material_resource->m_VertexProgram);
        dmResource::Release(factory, (void*) material_resource->m_FragmentProgram);
        delete material_resource;
        s_MaterialResources.Erase((uintptr_t)m);

        dmGraphics::DeleteMaterial(m);

        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult RecreateResource(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        // TODO: Implement me!
        return dmResource::CREATE_RESULT_UNKNOWN;
    }
}
