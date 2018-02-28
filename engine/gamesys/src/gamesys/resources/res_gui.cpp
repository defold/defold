#include <string.h>
#include <stdlib.h>
#include <new>
#include "res_gui.h"
#include "../proto/gui_ddf.h"
#include "../components/comp_gui.h"
#include "gamesys.h"
#include <gameobject/lua_ddf.h>
#include <gameobject/gameobject_script_util.h>
#include <particle/particle.h>

namespace dmGameSystem
{
    dmResource::Result ResPreloadGuiScript(const dmResource::ResourcePreloadParams& params)
    {
        dmLuaDDF::LuaModule* lua_module = 0;
        dmDDF::Result e = dmDDF::LoadMessage<dmLuaDDF::LuaModule>(params.m_Buffer, params.m_BufferSize, &lua_module);
        if ( e != dmDDF::RESULT_OK )
            return dmResource::RESULT_FORMAT_ERROR;

        uint32_t n_modules = lua_module->m_Modules.m_Count;
        for (uint32_t i = 0; i < n_modules; ++i)
        {
            dmResource::PreloadHint(params.m_HintInfo, lua_module->m_Resources[i]);
        }

        *params.m_PreloadData = lua_module;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResCreateGuiScript(const dmResource::ResourceCreateParams& params)
    {
        GuiContext* gui_context = (GuiContext*) params.m_Context;
        dmLuaDDF::LuaModule* lua_module = (dmLuaDDF::LuaModule*) params.m_PreloadData;

        if (!dmGameObject::RegisterSubModules(params.m_Factory, gui_context->m_ScriptContext, lua_module))
        {
            dmDDF::FreeMessage(lua_module);
            return dmResource::RESULT_FORMAT_ERROR;
        }

        dmGui::HScript script = dmGui::NewScript(gui_context->m_GuiContext);
        dmGui::Result result = dmGui::SetScript(script, &lua_module->m_Source);
        if (result == dmGui::RESULT_OK)
        {
            params.m_Resource->m_Resource = script;
            params.m_Resource->m_ResourceSize = params.m_BufferSize - lua_module->m_Source.m_Script.m_Count;
            dmDDF::FreeMessage(lua_module);
            return dmResource::RESULT_OK;
        }
        else
        {
            dmDDF::FreeMessage(lua_module);
            return dmResource::RESULT_FORMAT_ERROR;
        }
    }

    dmResource::Result ResDestroyGuiScript(const dmResource::ResourceDestroyParams& params)
    {
        dmGui::HScript script = (dmGui::HScript)params.m_Resource->m_Resource;
        dmGui::DeleteScript(script);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResRecreateGuiScript(const dmResource::ResourceRecreateParams& params)
    {
        GuiContext* gui_context = (GuiContext*) params.m_Context;
        dmGui::HScript script = (dmGui::HScript)params.m_Resource->m_Resource;

        dmLuaDDF::LuaModule* lua_module = 0;
        dmDDF::Result e = dmDDF::LoadMessage<dmLuaDDF::LuaModule>(params.m_Buffer, params.m_BufferSize, &lua_module);
        if ( e != dmDDF::RESULT_OK ) {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        if (!dmGameObject::RegisterSubModules(params.m_Factory, gui_context->m_ScriptContext, lua_module))
        {
            dmDDF::FreeMessage(lua_module);
            return dmResource::RESULT_FORMAT_ERROR;
        }

        dmGui::Result result = dmGui::SetScript(script, &lua_module->m_Source);
        if (result == dmGui::RESULT_OK)
        {
            GuiContext* gui_context = (GuiContext*) params.m_Context;
            for (uint32_t i = 0; i < gui_context->m_Worlds.Size(); ++i)
            {
                GuiWorld* world = (GuiWorld*)gui_context->m_Worlds[i];
                for (uint32_t j = 0; j < world->m_Components.Size(); ++j)
                {
                    GuiComponent* component = world->m_Components[j];
                    if (script == dmGui::GetSceneScript(component->m_Scene))
                    {
                        dmGui::ReloadScene(component->m_Scene);
                    }
                }
            }
            params.m_Resource->m_ResourceSize = params.m_BufferSize - lua_module->m_Source.m_Script.m_Count;
            dmDDF::FreeMessage(lua_module);
            return dmResource::RESULT_OK;
        }
        else
        {
            dmDDF::FreeMessage(lua_module);
            return dmResource::RESULT_FORMAT_ERROR;
        }
    }

    dmResource::Result AcquireResources(dmResource::HFactory factory, dmGui::HContext context,
        dmGuiDDF::SceneDesc *desc, GuiSceneResource* resource, const char* filename)
    {
        resource->m_SceneDesc = desc;

        dmResource::Result fr = dmResource::Get(factory, resource->m_SceneDesc->m_Material, (void**) &resource->m_Material);
        if (fr != dmResource::RESULT_OK) {
            return fr;
        }

        if (resource->m_SceneDesc->m_Script != 0x0 && *resource->m_SceneDesc->m_Script != '\0')
        {
            dmResource::Result fr = dmResource::Get(factory, resource->m_SceneDesc->m_Script, (void**) &resource->m_Script);
            if (fr != dmResource::RESULT_OK)
                return fr;
        }

        resource->m_RigScenes.SetCapacity(resource->m_SceneDesc->m_SpineScenes.m_Count);
        resource->m_RigScenes.SetSize(0);
        for (uint32_t i = 0; i < resource->m_SceneDesc->m_SpineScenes.m_Count; ++i)
        {
            RigSceneResource* spine_scene = 0x0;
            dmResource::Result r = dmResource::Get(factory, resource->m_SceneDesc->m_SpineScenes[i].m_SpineScene, (void**) &spine_scene);
            if (r != dmResource::RESULT_OK)
                return r;
            resource->m_RigScenes.Push(spine_scene);
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
        for (uint32_t i = 0; i < resource->m_SceneDesc->m_Fonts.m_Count; ++i)
        {
            dmRender::HFontMap font_map;
            dmResource::Result r = dmResource::Get(factory, resource->m_SceneDesc->m_Fonts[i].m_Font, (void**) &font_map);
            if (r != dmResource::RESULT_OK)
                return r;
            resource->m_FontMaps.Push(font_map);
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
                tsr.m_Texture = texture_set_resource->m_Texture;
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
        size += res->m_RigScenes.Capacity()*sizeof(RigSceneResource*);
        size += res->m_ParticlePrototypes.Capacity()*sizeof(dmParticle::HPrototype);
        return size;
    }

    void ReleaseResources(dmResource::HFactory factory, GuiSceneResource* resource)
    {
        for (uint32_t j = 0; j < resource->m_RigScenes.Size(); ++j)
        {
            dmResource::Release(factory, resource->m_RigScenes[j]);
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
        if (resource->m_Script)
            dmResource::Release(factory, resource->m_Script);
        if (resource->m_SceneDesc)
            dmDDF::FreeMessage(resource->m_SceneDesc);
        if (resource->m_Path)
            free((void*)resource->m_Path);
        if (resource->m_Material)
            dmResource::Release(factory, resource->m_Material);
    }

    dmResource::Result ResPreloadSceneDesc(const dmResource::ResourcePreloadParams& params)
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

        *params.m_PreloadData = scene_desc;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResCreateSceneDesc(const dmResource::ResourceCreateParams& params)
    {
        GuiSceneResource* scene_resource = new GuiSceneResource();
        memset(scene_resource, 0, sizeof(GuiSceneResource));
        dmResource::Result r = AcquireResources(params.m_Factory, ((GuiContext*) params.m_Context)->m_GuiContext, (dmGuiDDF::SceneDesc*) params.m_PreloadData, scene_resource, params.m_Filename);
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

    dmResource::Result ResDestroySceneDesc(const dmResource::ResourceDestroyParams& params)
    {
        GuiSceneResource* scene_resource = (GuiSceneResource*) params.m_Resource->m_Resource;

        ReleaseResources(params.m_Factory, scene_resource);
        delete scene_resource;

        return dmResource::RESULT_OK;
    }

    dmResource::Result ResRecreateSceneDesc(const dmResource::ResourceRecreateParams& params)
    {
        dmGuiDDF::SceneDesc* scene_desc;
        dmDDF::Result e = dmDDF::LoadMessage<dmGuiDDF::SceneDesc>(params.m_Buffer, params.m_BufferSize, &scene_desc);
        if ( e != dmDDF::RESULT_OK )
            return dmResource::RESULT_FORMAT_ERROR;

        GuiSceneResource tmp_scene_resource;
        memset(&tmp_scene_resource, 0, sizeof(GuiSceneResource));
        dmResource::Result r = AcquireResources(params.m_Factory, ((GuiContext*) params.m_Context)->m_GuiContext, scene_desc, &tmp_scene_resource, params.m_Filename);
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
}
