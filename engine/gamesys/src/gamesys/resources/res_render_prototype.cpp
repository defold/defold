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

#include "res_render_prototype.h"
#include "res_texture.h"
#include "res_render_target.h"
#include "res_compute.h"

#include <render/render_ddf.h>

#include "../gamesys.h"
#include "../gamesys_private.h"

#include <dmsdk/gamesys/resources/res_material.h>

namespace dmGameSystem
{
    void ResourceReloadedCallback(const dmResource::ResourceReloadedParams* params)
    {
        RenderScriptPrototype* prototype = (RenderScriptPrototype*) params->m_UserData;
        if (params->m_FilenameHash == prototype->m_NameHash)
        {
            dmRender::OnReloadRenderScriptInstance(prototype->m_Instance);
        }
    }

    struct RenderResourceMeta
    {
        const char*                  m_Name;
        dmRender::RenderResourceType m_Type;
    };

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
                HResourceDescriptor descriptor;
                if (dmResource::RESULT_OK == dmResource::GetDescriptor(factory, prototype_desc->m_Script, &descriptor))
                    prototype->m_NameHash = dmResource::GetNameHash(descriptor);
                prototype->m_Instance = dmRender::NewRenderScriptInstance(render_context, prototype->m_Script);
            }
            else
            {
                dmRender::SetRenderScriptInstanceRenderScript(prototype->m_Instance, prototype->m_Script);
                dmRender::ClearRenderScriptInstanceRenderResources(prototype->m_Instance);
            }

            dmArray<RenderResourceMeta> render_resource_metas;
            render_resource_metas.SetCapacity(prototype_desc->m_RenderResources.m_Count);
            prototype->m_RenderResources.SetCapacity(prototype_desc->m_RenderResources.m_Count);

            if (prototype_desc->m_RenderResources.m_Count > 0)
            {
                for (uint32_t i = 0; i < prototype_desc->m_RenderResources.m_Count; ++i)
                {
                    void* res;
                    result = dmResource::Get(factory, prototype_desc->m_RenderResources[i].m_Path, &res);
                    if (result != dmResource::RESULT_OK)
                    {
                        break;
                    }

                    dmRender::RenderResourceType render_resource_type = ResourcePathToRenderResourceType(prototype_desc->m_RenderResources[i].m_Path);
                    if (render_resource_type == dmRender::RENDER_RESOURCE_TYPE_INVALID)
                    {
                        dmLogError("Resource extension '%s' not supported.", dmResource::GetExtFromPath(prototype_desc->m_RenderResources[i].m_Path));
                        result = dmResource::RESULT_NOT_SUPPORTED;
                        break;
                    }

                    prototype->m_RenderResources.Push(res);

                    RenderResourceMeta meta;
                    meta.m_Name = prototype_desc->m_RenderResources[i].m_Name;
                    meta.m_Type = render_resource_type;

                    render_resource_metas.Push(meta);
                }
            }

            if (result == dmResource::RESULT_OK)
            {
                for (uint32_t i = 0; i < prototype->m_RenderResources.Size(); ++i)
                {
                    void* render_resource = prototype->m_RenderResources[i];
                    uint64_t render_resource_val = 0;

                    switch(render_resource_metas[i].m_Type)
                    {
                        case dmRender::RENDER_RESOURCE_TYPE_RENDER_TARGET:
                        {
                            dmGameSystem::RenderTargetResource* render_target_res = (dmGameSystem::RenderTargetResource*) render_resource;
                            render_resource_val = (uint64_t) render_target_res->m_RenderTarget;
                        } break;
                        case dmRender::RENDER_RESOURCE_TYPE_MATERIAL:
                        {
                            dmGameSystem::MaterialResource* material_res = (dmGameSystem::MaterialResource*) render_resource;
                            render_resource_val = (uint64_t) material_res->m_Material;
                        } break;
                        case dmRender::RENDER_RESOURCE_TYPE_COMPUTE:
                        {
                            dmGameSystem::ComputeResource* compute_res = (dmGameSystem::ComputeResource*) render_resource;
                            render_resource_val = (uint64_t) compute_res->m_Program;
                        } break;
                        case dmRender::RENDER_RESOURCE_TYPE_INVALID:
                        default:break;
                    }

                    dmRender::AddRenderScriptInstanceRenderResource(
                        prototype->m_Instance,
                        render_resource_metas[i].m_Name,
                        render_resource_val,
                        render_resource_metas[i].m_Type);
                }
            }
            else
            {
                for (uint32_t i = 0; i < prototype->m_RenderResources.Size(); ++i)
                {
                    if (prototype->m_RenderResources[i])
                        dmResource::Release(factory, prototype->m_RenderResources[i]);
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
        for (uint32_t i = 0; i < prototype->m_RenderResources.Size(); ++i)
            dmResource::Release(factory, prototype->m_RenderResources[i]);
    }

    dmResource::Result ResRenderPrototypeCreate(const dmResource::ResourceCreateParams* params)
    {
        dmRender::HRenderContext render_context = (dmRender::HRenderContext) params->m_Context;
        RenderScriptPrototype* prototype = new RenderScriptPrototype();
        memset(prototype, 0, sizeof(RenderScriptPrototype));
        dmResource::Result r = AcquireResources(params->m_Factory, params->m_Buffer, params->m_BufferSize, render_context, prototype, params->m_Filename);
        if (r == dmResource::RESULT_OK)
        {
            dmResource::SetResource(params->m_Resource, prototype);
            dmResource::RegisterResourceReloadedCallback(params->m_Factory, ResourceReloadedCallback, prototype);
        }
        else
        {
            ReleaseResources(params->m_Factory, prototype);
            if (prototype->m_Instance)
                dmRender::DeleteRenderScriptInstance(prototype->m_Instance);
            delete prototype;
        }
        return r;
    }

    dmResource::Result ResRenderPrototypeDestroy(const dmResource::ResourceDestroyParams* params)
    {
        RenderScriptPrototype* prototype = (RenderScriptPrototype*)dmResource::GetResource(params->m_Resource);
        ReleaseResources(params->m_Factory, prototype);
        if (prototype->m_Instance)
            dmRender::DeleteRenderScriptInstance(prototype->m_Instance);
        dmResource::UnregisterResourceReloadedCallback(params->m_Factory, ResourceReloadedCallback, prototype);
        delete prototype;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResRenderPrototypeRecreate(const dmResource::ResourceRecreateParams* params)
    {
        dmRender::HRenderContext render_context = (dmRender::HRenderContext) params->m_Context;
        RenderScriptPrototype* prototype = (RenderScriptPrototype*)dmResource::GetResource(params->m_Resource);
        RenderScriptPrototype tmp_prototype;
        memset(&tmp_prototype, 0, sizeof(RenderScriptPrototype));
        tmp_prototype.m_Instance = prototype->m_Instance;
        dmResource::Result r = AcquireResources(params->m_Factory, params->m_Buffer, params->m_BufferSize, render_context, &tmp_prototype, params->m_Filename);
        if (r == dmResource::RESULT_OK)
        {
            ReleaseResources(params->m_Factory, prototype);
            prototype->m_Script = tmp_prototype.m_Script;
            prototype->m_RenderResources.Swap(tmp_prototype.m_RenderResources);
        }
        else
        {
            ReleaseResources(params->m_Factory, &tmp_prototype);
        }
        return r;
    }

}
