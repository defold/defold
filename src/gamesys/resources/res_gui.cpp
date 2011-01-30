#include <string.h>
#include "res_gui.h"
#include "../proto/gui_ddf.h"

// TODO: Namespace. dmGui is the namespace for the gui-library...
namespace dmGameSystem
{
    dmResource::CreateResult ResCreateGuiScript(dmResource::HFactory factory,
                                             void* context,
                                             const void* buffer, uint32_t buffer_size,
                                             dmResource::SResourceDescriptor* resource,
                                             const char* filename)
    {
        dmGui::HContext gui_context = (dmGui::HContext)context;
        dmGui::HScript script = dmGui::NewScript(gui_context);
        dmGui::Result result = dmGui::SetScript(script, (const char*)buffer, buffer_size, filename);
        if (result == dmGui::RESULT_OK)
        {
            resource->m_Resource = script;
            return dmResource::CREATE_RESULT_OK;
        }
        else
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
    }

    dmResource::CreateResult ResDestroyGuiScript(dmResource::HFactory factory,
                                              void* context,
                                              dmResource::SResourceDescriptor* resource)
    {
        dmGui::HScript script = (dmGui::HScript)resource->m_Resource;
        dmGui::DeleteScript(script);
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResRecreateGuiScript(dmResource::HFactory factory,
                                          void* context,
                                          const void* buffer, uint32_t buffer_size,
                                          dmResource::SResourceDescriptor* resource,
                                          const char* filename)
    {
        dmGui::HScript script = (dmGui::HScript)resource->m_Resource;
        dmGui::Result result = dmGui::SetScript(script, (const char*)buffer, buffer_size, filename);
        if (result == dmGui::RESULT_OK)
            return dmResource::CREATE_RESULT_OK;
        else
            return dmResource::CREATE_RESULT_UNKNOWN;
    }

    bool AcquireResources(dmResource::HFactory factory, dmGui::HContext context,
        const void* buffer, uint32_t buffer_size, GuiSceneResource* resource, const char* filename)
    {
        dmDDF::Result e = dmDDF::LoadMessage<dmGuiDDF::SceneDesc>(buffer, buffer_size, &resource->m_SceneDesc);
        if ( e != dmDDF::RESULT_OK )
            return false;

        dmResource::FactoryResult fr = dmResource::Get(factory, resource->m_SceneDesc->m_Script, (void**) &resource->m_Script);
        if (fr != dmResource::FACTORY_RESULT_OK)
            return false;

        resource->m_FontMaps.SetCapacity(resource->m_SceneDesc->m_Fonts.m_Count);
        resource->m_FontMaps.SetSize(0);
        for (uint32_t i = 0; i < resource->m_SceneDesc->m_Fonts.m_Count; ++i)
        {
            dmRender::HFontMap font_map;
            dmResource::FactoryResult r = dmResource::Get(factory, resource->m_SceneDesc->m_Fonts[i].m_Font, (void**) &font_map);
            if (r != dmResource::FACTORY_RESULT_OK)
                return false;
            resource->m_FontMaps.Push(font_map);
        }

        resource->m_Textures.SetCapacity(resource->m_SceneDesc->m_Textures.m_Count);
        resource->m_Textures.SetSize(0);
        for (uint32_t i = 0; i < resource->m_SceneDesc->m_Textures.m_Count; ++i)
        {
            dmGraphics::HTexture texture;
            dmResource::FactoryResult r = dmResource::Get(factory, resource->m_SceneDesc->m_Textures[i], (void**) &texture);
            if (r != dmResource::FACTORY_RESULT_OK)
            {
                return false;
            }
            resource->m_Textures.Push(texture);
        }

        resource->m_Path = strdup(resource->m_SceneDesc->m_Script);

        resource->m_GuiContext = context;

        return true;
    }

    void ReleaseResources(dmResource::HFactory factory, GuiSceneResource* resource)
    {
        for (uint32_t j = 0; j < resource->m_FontMaps.Size(); ++j)
        {
            dmResource::Release(factory, resource->m_FontMaps[j]);
        }
        for (uint32_t j = 0; j < resource->m_Textures.Size(); ++j)
        {
            dmResource::Release(factory, resource->m_Textures[j]);
        }
        if (resource->m_Script)
            dmResource::Release(factory, resource->m_Script);
        if (resource->m_SceneDesc)
            dmDDF::FreeMessage(resource->m_SceneDesc);
        if (resource->m_Path)
            free((void*)resource->m_Path);
    }

    dmResource::CreateResult ResCreateSceneDesc(dmResource::HFactory factory,
                                                void* context,
                                                const void* buffer, uint32_t buffer_size,
                                                dmResource::SResourceDescriptor* resource,
                                                const char* filename)
    {
        GuiSceneResource* scene_resource = new GuiSceneResource();
        memset(scene_resource, 0, sizeof(GuiSceneResource));
        if (AcquireResources(factory, (dmGui::HContext)context, buffer, buffer_size, scene_resource, filename))
        {
            resource->m_Resource = (void*)scene_resource;
            return dmResource::CREATE_RESULT_OK;
        }
        else
        {
            ReleaseResources(factory, scene_resource);
            delete scene_resource;
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
    }

    dmResource::CreateResult ResDestroySceneDesc(dmResource::HFactory factory,
                                                 void* context,
                                                 dmResource::SResourceDescriptor* resource)
    {
        GuiSceneResource* scene_resource = (GuiSceneResource*) resource->m_Resource;

        ReleaseResources(factory, scene_resource);
        delete scene_resource;

        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResRecreateSceneDesc(dmResource::HFactory factory,
                                          void* context,
                                          const void* buffer, uint32_t buffer_size,
                                          dmResource::SResourceDescriptor* resource,
                                          const char* filename)
    {
        GuiSceneResource tmp_scene_resource;
        memset(&tmp_scene_resource, 0, sizeof(GuiSceneResource));
        if (AcquireResources(factory, (dmGui::HContext)context, buffer, buffer_size, &tmp_scene_resource, filename))
        {
            GuiSceneResource* scene_resource = (GuiSceneResource*) resource->m_Resource;
            ReleaseResources(factory, scene_resource);
            scene_resource->m_SceneDesc = tmp_scene_resource.m_SceneDesc;
            scene_resource->m_Script = tmp_scene_resource.m_Script;
            scene_resource->m_FontMaps.Swap(tmp_scene_resource.m_FontMaps);
            scene_resource->m_Textures.Swap(tmp_scene_resource.m_Textures);
            scene_resource->m_Path = tmp_scene_resource.m_Path;
            scene_resource->m_GuiContext = tmp_scene_resource.m_GuiContext;
            return dmResource::CREATE_RESULT_OK;
        }
        else
        {
            ReleaseResources(factory, &tmp_scene_resource);
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
    }
}
