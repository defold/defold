#include "res_render_prototype.h"

#include <render/render_ddf.h>

#include "../gamesys.h"

namespace dmGameSystem
{
    void ResourceReloadedCallback(const dmResource::ResourceReloadedParams& params)
    {
        RenderScriptPrototype* prototype = (RenderScriptPrototype*) params.m_UserData;
        if (params.m_Resource->m_NameHash == prototype->m_NameHash)
        {
            dmRender::OnReloadRenderScriptInstance(prototype->m_Instance);
        }
    }

    dmResource::Result AcquireResources(dmResource::HFactory factory, const void* buffer, uint32_t buffer_size,
        dmRender::HRenderContext render_context, RenderScriptPrototype* prototype, const char* filename)
    {
        dmRenderDDF::RenderPrototypeDesc* prototype_desc;
        dmDDF::Result e  = dmDDF::LoadMessage(buffer, buffer_size, &prototype_desc);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }
        dmResource::Result result = dmResource::Get(factory, prototype_desc->m_Script, (void**)&prototype->m_Script);
        if (result == dmResource::RESULT_OK)
        {
            if (prototype->m_Instance == 0x0)
            {
                dmResource::SResourceDescriptor descriptor;
                if (dmResource::RESULT_OK == dmResource::GetDescriptor(factory, prototype_desc->m_Script, &descriptor))
                    prototype->m_NameHash = descriptor.m_NameHash;
                prototype->m_Instance = dmRender::NewRenderScriptInstance(render_context, prototype->m_Script);
            }
            else
            {
                dmRender::SetRenderScriptInstanceRenderScript(prototype->m_Instance, prototype->m_Script);
                dmRender::ClearRenderScriptInstanceMaterials(prototype->m_Instance);
            }
            prototype->m_Materials.SetCapacity(prototype_desc->m_Materials.m_Count);
            for (uint32_t i = 0; i < prototype_desc->m_Materials.m_Count; ++i)
            {
                dmRender::HMaterial material;
                if (dmResource::RESULT_OK == dmResource::Get(factory, prototype_desc->m_Materials[i].m_Material, (void**)&material))
                    prototype->m_Materials.Push(material);
                else
                    break;
            }
            if (!prototype->m_Materials.Full())
            {
                result = dmResource::RESULT_OUT_OF_RESOURCES;
            }
            else
            {
                for (uint32_t i = 0; i < prototype->m_Materials.Size(); ++i)
                {
                    dmRender::AddRenderScriptInstanceMaterial(prototype->m_Instance, prototype_desc->m_Materials[i].m_Name, prototype->m_Materials[i]);
                }
            }
        }
        dmDDF::FreeMessage(prototype_desc);
        return result;
    }

    void ReleaseResources(dmResource::HFactory factory, RenderScriptPrototype* prototype)
    {
        if (prototype->m_Script)
            dmResource::Release(factory, prototype->m_Script);
        for (uint32_t i = 0; i < prototype->m_Materials.Size(); ++i)
            dmResource::Release(factory, prototype->m_Materials[i]);
    }

    dmResource::Result ResRenderPrototypeCreate(const dmResource::ResourceCreateParams& params)
    {
        dmRender::HRenderContext render_context = (dmRender::HRenderContext) params.m_Context;
        RenderScriptPrototype* prototype = new RenderScriptPrototype();
        memset(prototype, 0, sizeof(RenderScriptPrototype));
        dmResource::Result r = AcquireResources(params.m_Factory, params.m_Buffer, params.m_BufferSize, render_context, prototype, params.m_Filename);
        if (r == dmResource::RESULT_OK)
        {
            params.m_Resource->m_Resource = (void*) prototype;
            dmResource::RegisterResourceReloadedCallback(params.m_Factory, ResourceReloadedCallback, prototype);
        }
        else
        {
            ReleaseResources(params.m_Factory, prototype);
            if (prototype->m_Instance)
                dmRender::DeleteRenderScriptInstance(prototype->m_Instance);
            delete prototype;
        }
        return r;
    }

    dmResource::Result ResRenderPrototypeDestroy(const dmResource::ResourceDestroyParams& params)
    {
        RenderScriptPrototype* prototype = (RenderScriptPrototype*)params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, prototype);
        if (prototype->m_Instance)
            dmRender::DeleteRenderScriptInstance(prototype->m_Instance);
        dmResource::UnregisterResourceReloadedCallback(params.m_Factory, ResourceReloadedCallback, prototype);
        delete prototype;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResRenderPrototypeRecreate(const dmResource::ResourceRecreateParams& params)
    {
        dmRender::HRenderContext render_context = (dmRender::HRenderContext) params.m_Context;
        RenderScriptPrototype* prototype = (RenderScriptPrototype*)params.m_Resource->m_Resource;
        RenderScriptPrototype tmp_prototype;
        memset(&tmp_prototype, 0, sizeof(RenderScriptPrototype));
        tmp_prototype.m_Instance = prototype->m_Instance;
        dmResource::Result r = AcquireResources(params.m_Factory, params.m_Buffer, params.m_BufferSize, render_context, &tmp_prototype, params.m_Filename);
        if (r == dmResource::RESULT_OK)
        {
            ReleaseResources(params.m_Factory, prototype);
            prototype->m_Script = tmp_prototype.m_Script;
            prototype->m_Materials.Swap(tmp_prototype.m_Materials);
        }
        else
        {
            ReleaseResources(params.m_Factory, &tmp_prototype);
        }
        return r;
    }

}
