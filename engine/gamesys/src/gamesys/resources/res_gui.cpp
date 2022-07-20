// Copyright 2020-2022 The Defold Foundation
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

#include <string.h>
#include <stdlib.h>
#include <gamesys/resources/res_gui.h>
#include <gamesys/gui_ddf.h>
#include "../components/comp_gui_private.h"
#include "gamesys.h"
#include <gameobject/lua_ddf.h>
#include <gameobject/gameobject_script_util.h>
#include <particle/particle.h>

namespace dmGameSystem
{
    dmResource::Result AcquireResources(dmResource::HFactory factory, dmGui::HContext context,
        dmGuiDDF::SceneDesc *desc, GuiSceneResource* resource, const char* filename)
    {
        resource->m_SceneDesc = desc;

        dmResource::Result fr = dmResource::Get(factory, resource->m_SceneDesc->m_Material, (void**) &resource->m_Material);
        if (fr != dmResource::RESULT_OK) {
            return fr;
        }
        if(dmRender::GetMaterialVertexSpace(resource->m_Material) != dmRenderDDF::MaterialDesc::VERTEX_SPACE_WORLD)
        {
            dmLogError("Failed to create Gui component. This component only supports materials with the Vertex Space property set to 'vertex-space-world'");
            return dmResource::RESULT_NOT_SUPPORTED;
        }

        if (resource->m_SceneDesc->m_Script != 0x0 && *resource->m_SceneDesc->m_Script != '\0')
        {
            dmResource::Result fr = dmResource::Get(factory, resource->m_SceneDesc->m_Script, (void**) &resource->m_Script);
            if (fr != dmResource::RESULT_OK)
                return fr;
        }

        uint32_t table_size = dmMath::Max(1U, resource->m_SceneDesc->m_Resources.m_Count/3);
        resource->m_Resources.SetCapacity(table_size, resource->m_SceneDesc->m_Resources.m_Count);
        resource->m_ResourceTypes.SetCapacity(table_size, resource->m_SceneDesc->m_Resources.m_Count);
        for (uint32_t i = 0; i < resource->m_SceneDesc->m_Resources.m_Count; ++i)
        {
            void* custom_resource = 0;
            dmResource::Result r = dmResource::Get(factory, resource->m_SceneDesc->m_Resources[i].m_Path, &custom_resource);
            if (r != dmResource::RESULT_OK)
                return r;
            const char* suffix = strrchr(resource->m_SceneDesc->m_Resources[i].m_Path, '.');

            dmhash_t resource_id = dmHashString64(resource->m_SceneDesc->m_Resources[i].m_Name);
            dmhash_t suffix_hash = dmHashString64(suffix);
            resource->m_Resources.Put(resource_id, custom_resource);
            resource->m_ResourceTypes.Put(resource_id, suffix_hash);
        }

        resource->m_ParticlePrototypes.SetCapacity(resource->m_SceneDesc->m_Particlefxs.m_Count);
        resource->m_ParticlePrototypes.SetSize(0);
        for (uint32_t i = 0; i < resource->m_SceneDesc->m_Particlefxs.m_Count; ++i)
        {
            dmParticle::HPrototype pfx_res = 0x0;
            dmResource::Result r = dmResource::Get(factory, resource->m_SceneDesc->m_Particlefxs.m_Data[i].m_Particlefx, (void**) &pfx_res);
            if (r != dmResource::RESULT_OK)
                return r;
            resource->m_ParticlePrototypes.Push(pfx_res);
        }

        resource->m_FontMaps.SetCapacity(resource->m_SceneDesc->m_Fonts.m_Count);
        resource->m_FontMaps.SetSize(0);
        resource->m_FontMapPaths.SetCapacity(resource->m_FontMaps.Capacity());
        resource->m_FontMapPaths.SetSize(0);
        for (uint32_t i = 0; i < resource->m_SceneDesc->m_Fonts.m_Count; ++i)
        {
            dmRender::HFontMap font_map;
            dmResource::Result r = dmResource::Get(factory, resource->m_SceneDesc->m_Fonts[i].m_Font, (void**) &font_map);
            if (r != dmResource::RESULT_OK)
                return r;
            resource->m_FontMaps.Push(font_map);

            dmhash_t path_hash = 0;
            dmResource::GetPath(factory, font_map, &path_hash);
            resource->m_FontMapPaths.Push(path_hash);
        }

        // Note: For backwards compability, we add proxy textureset resources containing texures for single texture file resources (deprecated)
        // Once this is not supported anymore, we can remove this behaviour and rely on resource being loaded to be a textureset resource.
        // This needs to be reflected in the ReleaseResources method.
        dmResource::ResourceType resource_type_textureset;
        dmResource::GetTypeFromExtension(factory, "texturesetc", &resource_type_textureset);
        resource->m_GuiTextureSets.SetCapacity(resource->m_SceneDesc->m_Textures.m_Count);
        resource->m_GuiTextureSets.SetSize(0);
        for (uint32_t i = 0; i < resource->m_SceneDesc->m_Textures.m_Count; ++i)
        {
            TextureSetResource* texture_set_resource;
            dmResource::Result r = dmResource::Get(factory, resource->m_SceneDesc->m_Textures[i].m_Texture, (void**) &texture_set_resource);
            if (r != dmResource::RESULT_OK)
                return r;
            dmResource::ResourceType resource_type;
            r = dmResource::GetType(factory, texture_set_resource, &resource_type);
            if (r != dmResource::RESULT_OK)
                return r;
            GuiSceneTextureSetResource tsr;
            if(resource_type != resource_type_textureset)
            {
                tsr.m_TextureSet = 0;
                tsr.m_Texture = (dmGraphics::HTexture) texture_set_resource;
            }
            else
            {
                tsr.m_TextureSet = texture_set_resource;
                tsr.m_Texture = texture_set_resource->m_Textures[0];
            }
            resource->m_GuiTextureSets.Push(tsr);
        }

        resource->m_Path = strdup(resource->m_SceneDesc->m_Script);

        resource->m_GuiContext = context;

        return dmResource::RESULT_OK;
    }

    static uint32_t GetResourceSize(GuiSceneResource* res, uint32_t ddf_size)
    {
        uint32_t size = sizeof(GuiSceneResource);
        size += ddf_size;
        size += res->m_FontMaps.Capacity()*sizeof(dmRender::HFontMap);
        size += res->m_GuiTextureSets.Capacity()*sizeof(GuiSceneTextureSetResource);
        //size += res->m_Resources.Capacity()* ? // We should probably collect the sizes when we get them from the resource factory
        size += res->m_ParticlePrototypes.Capacity()*sizeof(dmParticle::HPrototype);
        return size;
    }

    static void HTReleaseResource(dmResource::HFactory factory, const dmhash_t* key, void** value)
    {
        dmResource::Release(factory, *value);
    }

    void ReleaseResources(dmResource::HFactory factory, GuiSceneResource* resource)
    {
        for (uint32_t j = 0; j < resource->m_ParticlePrototypes.Size(); ++j)
        {
            dmResource::Release(factory, resource->m_ParticlePrototypes[j]);
        }
        for (uint32_t j = 0; j < resource->m_FontMaps.Size(); ++j)
        {
            dmResource::Release(factory, resource->m_FontMaps[j]);
        }
        for (uint32_t j = 0; j < resource->m_GuiTextureSets.Size(); ++j)
        {
            if(resource->m_GuiTextureSets[j].m_TextureSet)
                dmResource::Release(factory, resource->m_GuiTextureSets[j].m_TextureSet);
            else
                dmResource::Release(factory, resource->m_GuiTextureSets[j].m_Texture);
        }

        resource->m_Resources.Iterate(HTReleaseResource, factory);

        if (resource->m_Script)
            dmResource::Release(factory, resource->m_Script);
        if (resource->m_SceneDesc)
            dmDDF::FreeMessage(resource->m_SceneDesc);
        if (resource->m_Path)
            free((void*)resource->m_Path);
        if (resource->m_Material)
            dmResource::Release(factory, resource->m_Material);
    }

    static dmResource::Result ResPreloadSceneDesc(const dmResource::ResourcePreloadParams& params)
    {
        dmGuiDDF::SceneDesc* scene_desc;
        dmDDF::Result e = dmDDF::LoadMessage<dmGuiDDF::SceneDesc>(params.m_Buffer, params.m_BufferSize, &scene_desc);
        if ( e != dmDDF::RESULT_OK )
            return dmResource::RESULT_FORMAT_ERROR;

        dmResource::PreloadHint(params.m_HintInfo, scene_desc->m_Material);
        if (*scene_desc->m_Script != 0)
            dmResource::PreloadHint(params.m_HintInfo, scene_desc->m_Script);

        for (uint32_t i = 0; i < scene_desc->m_Fonts.m_Count; ++i)
        {
            dmResource::PreloadHint(params.m_HintInfo, scene_desc->m_Fonts[i].m_Font);
        }

        for (uint32_t i = 0; i < scene_desc->m_Textures.m_Count; ++i)
        {
            dmResource::PreloadHint(params.m_HintInfo, scene_desc->m_Textures[i].m_Texture);
        }

        for (uint32_t i = 0; i < scene_desc->m_Particlefxs.m_Count; ++i)
        {
            dmResource::PreloadHint(params.m_HintInfo, scene_desc->m_Particlefxs[i].m_Particlefx);
        }

        for (uint32_t i = 0; i < scene_desc->m_Resources.m_Count; ++i)
        {
            dmResource::PreloadHint(params.m_HintInfo, scene_desc->m_Resources[i].m_Path);
        }

        *params.m_PreloadData = scene_desc;
        return dmResource::RESULT_OK;
    }

    static dmResource::Result ResCreateSceneDesc(const dmResource::ResourceCreateParams& params)
    {
        GuiSceneResource* scene_resource = new GuiSceneResource();
        memset(scene_resource, 0, sizeof(GuiSceneResource));
        dmResource::Result r = AcquireResources(params.m_Factory, (dmGui::HContext)params.m_Context, (dmGuiDDF::SceneDesc*) params.m_PreloadData, scene_resource, params.m_Filename);
        if (r == dmResource::RESULT_OK)
        {
            params.m_Resource->m_Resource = (void*)scene_resource;
            params.m_Resource->m_ResourceSize = GetResourceSize(scene_resource, params.m_BufferSize);
        }
        else
        {
            ReleaseResources(params.m_Factory, scene_resource);
            delete scene_resource;
        }
        return r;
    }

    static dmResource::Result ResDestroySceneDesc(const dmResource::ResourceDestroyParams& params)
    {
        GuiSceneResource* scene_resource = (GuiSceneResource*) params.m_Resource->m_Resource;

        ReleaseResources(params.m_Factory, scene_resource);
        delete scene_resource;

        return dmResource::RESULT_OK;
    }

    static dmResource::Result ResRecreateSceneDesc(const dmResource::ResourceRecreateParams& params)
    {
        dmGuiDDF::SceneDesc* scene_desc;
        dmDDF::Result e = dmDDF::LoadMessage<dmGuiDDF::SceneDesc>(params.m_Buffer, params.m_BufferSize, &scene_desc);
        if ( e != dmDDF::RESULT_OK )
            return dmResource::RESULT_FORMAT_ERROR;

        GuiSceneResource tmp_scene_resource;
        memset(&tmp_scene_resource, 0, sizeof(GuiSceneResource));
        dmResource::Result r = AcquireResources(params.m_Factory, (dmGui::HContext)params.m_Context, scene_desc, &tmp_scene_resource, params.m_Filename);
        if (r == dmResource::RESULT_OK)
        {
            GuiSceneResource* scene_resource = (GuiSceneResource*) params.m_Resource->m_Resource;
            ReleaseResources(params.m_Factory, scene_resource);
            scene_resource->m_SceneDesc = tmp_scene_resource.m_SceneDesc;
            scene_resource->m_Script = tmp_scene_resource.m_Script;
            scene_resource->m_FontMaps.Swap(tmp_scene_resource.m_FontMaps);
            scene_resource->m_GuiTextureSets.Swap(tmp_scene_resource.m_GuiTextureSets);
            scene_resource->m_Path = tmp_scene_resource.m_Path;
            scene_resource->m_GuiContext = tmp_scene_resource.m_GuiContext;
            scene_resource->m_Material = tmp_scene_resource.m_Material;
            params.m_Resource->m_ResourceSize = GetResourceSize(scene_resource, params.m_BufferSize);
        }
        else
        {
            ReleaseResources(params.m_Factory, &tmp_scene_resource);
        }
        return r;
    }

    static dmResource::Result ResourceTypeGui_Register(dmResource::ResourceTypeRegisterContext& ctx)
    {
        dmGui::HContext* gui_ctx = (dmGui::HContext*)ctx.m_Contexts->Get(dmHashString64("guic"));
        if (gui_ctx == 0)
        {
            dmLogError("Missing resource context 'guic' when registering resource type 'guic'");
            return dmResource::RESULT_INVAL;
        }

        return dmResource::RegisterType(ctx.m_Factory,
                                           ctx.m_Name,
                                           *gui_ctx, // context
                                           ResPreloadSceneDesc,
                                           ResCreateSceneDesc,
                                           0, // post create
                                           ResDestroySceneDesc,
                                           ResRecreateSceneDesc);
    }

}

DM_DECLARE_RESOURCE_TYPE(ResourceTypeGui, "guic", dmGameSystem::ResourceTypeGui_Register, 0);

