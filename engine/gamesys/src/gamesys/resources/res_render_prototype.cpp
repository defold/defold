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
#include "../component_render_resource.h"
#include <render/render_frame.h>
#include "res_texture.h"
#include "res_render_target.h"
#include "res_compute.h"

#include <render/render_ddf.h>

#include "../gamesys.h"
#include "../gamesys_private.h"

#include <dmsdk/gamesys/resources/res_material.h>

namespace dmGameSystem
{
    static void ReadRenderResourceBinding(void* resource, RenderScriptResourceBinding* binding)
    {
        switch (binding->m_Type)
        {
            case dmRender::RENDER_RESOURCE_TYPE_RENDER_TARGET:
            {
                RenderTargetResource* target = (RenderTargetResource*)resource;
                binding->m_Handle = (uint64_t)target->m_RenderTarget;
                for (uint32_t i = 0; i < dmGraphics::MAX_BUFFER_COLOR_ATTACHMENTS; ++i)
                {
                    binding->m_AttachmentTextures[i] = target->m_ColorAttachmentResources[i] ? target->m_ColorAttachmentResources[i]->m_Texture : 0;
                }
                // Resource render targets expose color and optional depth textures, not stencil textures.
                binding->m_AttachmentTextures[dmGraphics::MAX_BUFFER_COLOR_ATTACHMENTS] = target->m_DepthAttachmentResource ? target->m_DepthAttachmentResource->m_Texture : 0;
                break;
            }
            case dmRender::RENDER_RESOURCE_TYPE_MATERIAL:
            {
                binding->m_Handle = (uint64_t)((MaterialResource*)resource)->m_Material;
                break;
            }
            case dmRender::RENDER_RESOURCE_TYPE_COMPUTE:
            {
                binding->m_Handle = (uint64_t)((ComputeResource*)resource)->m_Program;
                break;
            }
            default:
            {
                break;
            }
        }
    }

    void ResourceReloadedCallback(const dmResource::ResourceReloadedParams* params)
    {
        RenderScriptPrototype* prototype = (RenderScriptPrototype*) params->m_UserData;
        for (uint32_t i = 0; i < prototype->m_RenderResourceBindings.Size(); ++i)
        {
            RenderScriptResourceBinding& binding = prototype->m_RenderResourceBindings[i];
            if (binding.m_PathHash != params->m_FilenameHash)
            {
                continue;
            }
            RenderScriptResourceBinding previous = binding;
            prototype->m_RenderResources[i] = dmResource::GetResource(params->m_Resource);
            ReadRenderResourceBinding(prototype->m_RenderResources[i], &binding);
            uint32_t texture_count = binding.m_Type == dmRender::RENDER_RESOURCE_TYPE_RENDER_TARGET ? DM_ARRAY_SIZE(binding.m_AttachmentTextures) : 0;
            // Factory mutation admission has finished older readers before native
            // handle replacement. Refresh inherited and named bindings together.
            dmRender::RefreshRenderScriptInstanceRenderResource(prototype->m_Instance, binding.m_NameHash, binding.m_Handle,
                                                                previous.m_AttachmentTextures, binding.m_AttachmentTextures, texture_count);
        }
        if (params->m_FilenameHash == prototype->m_NameHash)
        {
            dmRender::OnReloadRenderScriptInstance(prototype->m_Instance);
        }
    }

    static void ClearInheritedRenderResources(RenderScriptPrototype* prototype)
    {
        const dmGraphics::HTexture textures[dmGraphics::MAX_BUFFER_COLOR_ATTACHMENTS + 1] = {};
        for (uint32_t i = 0; i < prototype->m_RenderResourceBindings.Size(); ++i)
        {
            const RenderScriptResourceBinding& binding = prototype->m_RenderResourceBindings[i];
            dmRender::RefreshRenderScriptInstanceRenderResource(prototype->m_Instance, binding.m_NameHash, 0,
                                                                binding.m_AttachmentTextures, textures, DM_ARRAY_SIZE(textures));
        }
    }

    dmResource::Result AcquireResources(dmResource::HFactory factory, const void* buffer, uint32_t buffer_size,
                                        dmRender::HRenderContext render_context, RenderScriptPrototype* prototype, const char* filename, RenderScriptPrototype* previous)
    {
        dmRenderDDF::RenderPrototypeDesc* prototype_desc;
        dmDDF::Result e  = dmDDF::LoadMessage(buffer, buffer_size, &prototype_desc);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }
        dmResource::Result result = dmResource::Get(factory, prototype_desc->m_Script, (void**)&prototype->m_Script);
        if (result == dmResource::RESULT_OK)
        {
            prototype->m_RenderResourceBindings.SetCapacity(prototype_desc->m_RenderResources.m_Count);
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
                        dmResource::Release(factory, res);
                        result = dmResource::RESULT_NOT_SUPPORTED;
                        break;
                    }

                    prototype->m_RenderResources.Push(res);

                    RenderScriptResourceBinding binding = {};
                    binding.m_NameHash = dmHashString64(prototype_desc->m_RenderResources[i].m_Name);
                    binding.m_Type = render_resource_type;
                    dmResource::GetPath(factory, res, &binding.m_PathHash);
                    ReadRenderResourceBinding(res, &binding);
                    prototype->m_RenderResourceBindings.Push(binding);
                }
            }

            if (result == dmResource::RESULT_OK)
            {
                // A failed dependency load must not modify the live script or its registry.
                dmResource::GetPath(factory, prototype->m_Script, &prototype->m_NameHash);
                if (previous)
                {
                    ClearInheritedRenderResources(previous);
                    dmRender::SetRenderScriptInstanceRenderScript(prototype->m_Instance, prototype->m_Script);
                    dmRender::ClearRenderScriptInstanceRenderResources(prototype->m_Instance);
                }
                else
                {
                    prototype->m_Instance = dmRender::NewRenderScriptInstance(render_context, prototype->m_Script);
                }
                for (uint32_t i = 0; i < prototype->m_RenderResources.Size(); ++i)
                {
                    const RenderScriptResourceBinding& binding = prototype->m_RenderResourceBindings[i];
                    dmRender::AddRenderScriptInstanceRenderResource(
                        prototype->m_Instance,
                        prototype_desc->m_RenderResources[i].m_Name,
                        binding.m_Handle,
                        binding.m_Type);
                }
            }
        }
        dmDDF::FreeMessage(prototype_desc);
        return result;
    }

    void ReleaseResources(dmResource::HFactory factory, RenderScriptPrototype* prototype)
    {
        if (prototype->m_Script)
        {
            dmResource::Release(factory, prototype->m_Script);
        }
        for (uint32_t i = 0; i < prototype->m_RenderResources.Size(); ++i)
        {
            dmResource::Release(factory, prototype->m_RenderResources[i]);
        }
    }

    static dmRender::RenderFrameResult CaptureRenderResources(void* context, void* resource, dmRender::HRenderFrameStorage frame)
    {
        // The prototype owns all declared materials/programs/targets and their
        // dependent resources. Pin it as one unit, including inherited bindings
        // and attachments sampled without a target command in this frame.
        return RetainComponentRenderResource((dmResource::HFactory)context, frame, resource);
    }

    dmResource::Result ResRenderPrototypeCreate(const dmResource::ResourceCreateParams* params)
    {
        dmRender::HRenderContext render_context = (dmRender::HRenderContext) params->m_Context;
        RenderScriptPrototype* prototype = new RenderScriptPrototype();
        memset(prototype, 0, sizeof(RenderScriptPrototype));
        dmResource::Result r = AcquireResources(params->m_Factory, params->m_Buffer, params->m_BufferSize, render_context, prototype, params->m_Filename, 0);
        if (r == dmResource::RESULT_OK)
        {
            dmResource::SetResource(params->m_Resource, prototype);
            dmRender::SetRenderScriptInstanceResourceCapture(prototype->m_Instance, CaptureRenderResources, params->m_Factory, prototype);
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
        ClearInheritedRenderResources(prototype);
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
        dmResource::Result r = AcquireResources(params->m_Factory, params->m_Buffer, params->m_BufferSize, render_context, &tmp_prototype, params->m_Filename, prototype);
        if (r == dmResource::RESULT_OK)
        {
            ReleaseResources(params->m_Factory, prototype);
            prototype->m_Script = tmp_prototype.m_Script;
            prototype->m_NameHash = tmp_prototype.m_NameHash;
            prototype->m_RenderResources.Swap(tmp_prototype.m_RenderResources);
            prototype->m_RenderResourceBindings.Swap(tmp_prototype.m_RenderResourceBindings);
        }
        else
        {
            ReleaseResources(params->m_Factory, &tmp_prototype);
        }
        return r;
    }

}
