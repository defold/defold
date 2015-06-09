#include <string.h>
#include <stdlib.h>
#include <new>
#include "res_gui.h"
#include "../proto/gui_ddf.h"
#include "../components/comp_gui.h"
#include "gamesys.h"
#include <gameobject/lua_ddf.h>
#include <gameobject/gameobject_script_util.h>

namespace dmGameSystem
{
    dmResource::Result ResPreloadGuiScript(dmResource::HFactory factory,
                                           dmResource::HPreloadHintInfo hint_info,
                                           void* context,
                                           const void* buffer, uint32_t buffer_size,
                                           void** preload_data,
                                           const char* filename)
    {
        dmLuaDDF::LuaModule* lua_module = 0;
        dmDDF::Result e = dmDDF::LoadMessage<dmLuaDDF::LuaModule>(buffer, buffer_size, &lua_module);
        if ( e != dmDDF::RESULT_OK )
            return dmResource::RESULT_FORMAT_ERROR;
            
        uint32_t n_modules = lua_module->m_Modules.m_Count;
        for (uint32_t i = 0; i < n_modules; ++i)
        {
            dmResource::PreloadHint(hint_info, lua_module->m_Resources[i]);
        }
        
        *preload_data = lua_module;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResCreateGuiScript(dmResource::HFactory factory,
                                             void* context,
                                             const void* buffer, uint32_t buffer_size,
                                             void* preload_data,
                                             dmResource::SResourceDescriptor* resource,
                                             const char* filename)
    {
        GuiContext* gui_context = (GuiContext*)context;
        dmLuaDDF::LuaModule* lua_module = (dmLuaDDF::LuaModule*) preload_data;

        if (!dmGameObject::RegisterSubModules(factory, gui_context->m_ScriptContext, lua_module))
        {
            dmDDF::FreeMessage(lua_module);
            return dmResource::RESULT_FORMAT_ERROR;
        }

        dmGui::HScript script = dmGui::NewScript(gui_context->m_GuiContext);
        dmGui::Result result = dmGui::SetScript(script, &lua_module->m_Source);
        if (result == dmGui::RESULT_OK)
        {
            resource->m_Resource = script;
            dmDDF::FreeMessage(lua_module);
            return dmResource::RESULT_OK;
        }
        else
        {
            dmDDF::FreeMessage(lua_module);
            return dmResource::RESULT_FORMAT_ERROR;
        }
    }

    dmResource::Result ResDestroyGuiScript(dmResource::HFactory factory,
                                              void* context,
                                              dmResource::SResourceDescriptor* resource)
    {
        dmGui::HScript script = (dmGui::HScript)resource->m_Resource;
        dmGui::DeleteScript(script);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResRecreateGuiScript(dmResource::HFactory factory,
                                          void* context,
                                          const void* buffer, uint32_t buffer_size,
                                          dmResource::SResourceDescriptor* resource,
                                          const char* filename)
    {
        GuiContext* gui_context = (GuiContext*)context;
        dmGui::HScript script = (dmGui::HScript)resource->m_Resource;

        dmLuaDDF::LuaModule* lua_module = 0;
        dmDDF::Result e = dmDDF::LoadMessage<dmLuaDDF::LuaModule>(buffer, buffer_size, &lua_module);
        if ( e != dmDDF::RESULT_OK ) {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        if (!dmGameObject::RegisterSubModules(factory, gui_context->m_ScriptContext, lua_module))
        {
            dmDDF::FreeMessage(lua_module);
            return dmResource::RESULT_FORMAT_ERROR;
        }

        dmGui::Result result = dmGui::SetScript(script, &lua_module->m_Source);
        if (result == dmGui::RESULT_OK)
        {
            GuiContext* gui_context = (GuiContext*)context;
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

    void ReleaseResources(dmResource::HFactory factory, GuiSceneResource* resource)
    {
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

    dmResource::Result ResPreloadSceneDesc(dmResource::HFactory factory, dmResource::HPreloadHintInfo hint_info,
                                          void* context,
                                          const void* buffer, uint32_t buffer_size,
                                          void** preload_data,
                                          const char* filename)
    {
        dmGuiDDF::SceneDesc* scene_desc;
        dmDDF::Result e = dmDDF::LoadMessage<dmGuiDDF::SceneDesc>(buffer, buffer_size, &scene_desc);
        if ( e != dmDDF::RESULT_OK )
            return dmResource::RESULT_FORMAT_ERROR;

        dmResource::PreloadHint(hint_info, scene_desc->m_Material);
        dmResource::PreloadHint(hint_info, scene_desc->m_Script);

        for (uint32_t i = 0; i < scene_desc->m_Fonts.m_Count; ++i)
        {
            dmResource::PreloadHint(hint_info, scene_desc->m_Fonts[i].m_Font);
        }

        for (uint32_t i = 0; i < scene_desc->m_Textures.m_Count; ++i)
        {
            dmResource::PreloadHint(hint_info, scene_desc->m_Textures[i].m_Texture);
        }

        *preload_data = scene_desc;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResCreateSceneDesc(dmResource::HFactory factory,
                                                void* context,
                                                const void* buffer, uint32_t buffer_size,
                                                void* preload_data,
                                                dmResource::SResourceDescriptor* resource,
                                                const char* filename)
    {
        GuiSceneResource* scene_resource = new GuiSceneResource();
        memset(scene_resource, 0, sizeof(GuiSceneResource));
        dmResource::Result r = AcquireResources(factory, ((GuiContext*)context)->m_GuiContext, (dmGuiDDF::SceneDesc*) preload_data, scene_resource, filename);
        if (r == dmResource::RESULT_OK)
        {
            resource->m_Resource = (void*)scene_resource;
        }
        else
        {
            ReleaseResources(factory, scene_resource);
            delete scene_resource;
        }
        return r;
    }

    dmResource::Result ResDestroySceneDesc(dmResource::HFactory factory,
                                                 void* context,
                                                 dmResource::SResourceDescriptor* resource)
    {
        GuiSceneResource* scene_resource = (GuiSceneResource*) resource->m_Resource;

        ReleaseResources(factory, scene_resource);
        delete scene_resource;

        return dmResource::RESULT_OK;
    }

    dmResource::Result ResRecreateSceneDesc(dmResource::HFactory factory,
                                          void* context,
                                          const void* buffer, uint32_t buffer_size,
                                          dmResource::SResourceDescriptor* resource,
                                          const char* filename)
    {
        dmGuiDDF::SceneDesc* scene_desc;
        dmDDF::Result e = dmDDF::LoadMessage<dmGuiDDF::SceneDesc>(buffer, buffer_size, &scene_desc);
        if ( e != dmDDF::RESULT_OK )
            return dmResource::RESULT_FORMAT_ERROR;

        GuiSceneResource tmp_scene_resource;
        memset(&tmp_scene_resource, 0, sizeof(GuiSceneResource));
        dmResource::Result r = AcquireResources(factory, ((GuiContext*)context)->m_GuiContext, scene_desc, &tmp_scene_resource, filename);
        if (r == dmResource::RESULT_OK)
        {
            GuiSceneResource* scene_resource = (GuiSceneResource*) resource->m_Resource;
            ReleaseResources(factory, scene_resource);
            scene_resource->m_SceneDesc = tmp_scene_resource.m_SceneDesc;
            scene_resource->m_Script = tmp_scene_resource.m_Script;
            scene_resource->m_FontMaps.Swap(tmp_scene_resource.m_FontMaps);
            scene_resource->m_GuiTextureSets.Swap(tmp_scene_resource.m_GuiTextureSets);
            scene_resource->m_Path = tmp_scene_resource.m_Path;
            scene_resource->m_GuiContext = tmp_scene_resource.m_GuiContext;
            scene_resource->m_Material = tmp_scene_resource.m_Material;
        }
        else
        {
            ReleaseResources(factory, &tmp_scene_resource);
        }
        return r;
    }
}
