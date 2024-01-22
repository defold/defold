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

#include "res_render_prototype.h"
#include "res_texture.h"

#include <render/render_ddf.h>

#include "../gamesys.h"

#include <dmsdk/gamesys/resources/res_material.h>

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
                dmResource::SResourceDescriptor descriptor;
                if (dmResource::RESULT_OK == dmResource::GetDescriptor(factory, prototype_desc->m_Script, &descriptor))
                    prototype->m_NameHash = descriptor.m_NameHash;
                prototype->m_Instance = dmRender::NewRenderScriptInstance(render_context, prototype->m_Script);
            }
            else
            {
                dmRender::SetRenderScriptInstanceRenderScript(prototype->m_Instance, prototype->m_Script);
                dmRender::ClearRenderScriptInstanceRenderResources(prototype->m_Instance);
            }

            // The materials field is deprecated (should we support loading old content?)
            uint32_t total_resource_count = prototype_desc->m_RenderResources.m_Count + prototype_desc->m_Materials.m_Count;

            dmArray<RenderResourceMeta> render_resource_metas;
            render_resource_metas.SetCapacity(total_resource_count);
            prototype->m_RenderResources.SetCapacity(total_resource_count);

            for (uint32_t i = 0; i < prototype_desc->m_Materials.m_Count; ++i)
            {
                void* material;
                result = dmResource::Get(factory, prototype_desc->m_Materials[i].m_Material, (void**)&material);
                if (result != dmResource::RESULT_OK)
                    break;

                RenderResourceMeta meta;
                meta.m_Name = prototype_desc->m_Materials[i].m_Name;
                meta.m_Type = dmRender::RENDER_RESOURCE_TYPE_MATERIAL;

                prototype->m_RenderResources.Push(material);
                render_resource_metas.Push(meta);
            }

            if (prototype_desc->m_RenderResources.m_Count > 0)
            {
                // Supported render resource types:
                dmResource::ResourceType res_type_material;
                dmResource::ResourceType res_type_render_target;

                dmResource::Result type_res = dmResource::GetTypeFromExtension(factory, "materialc", &res_type_material);
                assert(type_res == dmResource::RESULT_OK);

                type_res = dmResource::GetTypeFromExtension(factory, "render_targetc", &res_type_render_target);
                assert(type_res == dmResource::RESULT_OK);

                for (uint32_t i = 0; i < prototype_desc->m_RenderResources.m_Count; ++i)
                {
                    void* res;
                    result = dmResource::Get(factory, prototype_desc->m_RenderResources[i].m_Path, &res);
                    if (result != dmResource::RESULT_OK)
                        break;

                    dmResource::ResourceType res_type;
                    dmResource::GetType(factory, res, &res_type);
                    dmRender::RenderResourceType render_resource_type = dmRender::RENDER_RESOURCE_TYPE_INVALID;

                    if (res_type == res_type_material)
                    {
                        render_resource_type = dmRender::RENDER_RESOURCE_TYPE_MATERIAL;
                    }
                    else if (res_type == res_type_render_target)
                    {
                        render_resource_type = dmRender::RENDER_RESOURCE_TYPE_RENDER_TARGET;
                    }
                    else
                    {
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
                            render_resource_val = (uint64_t) ((dmGameSystem::TextureResource*) render_resource)->m_Texture;
                            break;
                        case dmRender::RENDER_RESOURCE_TYPE_MATERIAL:
                            render_resource_val = (uint64_t) render_resource;
                            break;
                    }

                    dmRender::AddRenderScriptInstanceRenderResource(
                        prototype->m_Instance,
                        render_resource_metas[i].m_Name,
                        render_resource_val,
                        render_resource_metas[i].m_Type);
                }
            }
            else if (!prototype->m_RenderResources.Full())
            {
                result = dmResource::RESULT_OUT_OF_RESOURCES;
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
            prototype->m_RenderResources.Swap(tmp_prototype.m_RenderResources);
        }
        else
        {
            ReleaseResources(params.m_Factory, &tmp_prototype);
        }
        return r;
    }

}
