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
        resource->m_Resource = strdup((char*) buffer);
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResDestroyGuiScript(dmResource::HFactory factory,
                                              void* context,
                                              dmResource::SResourceDescriptor* resource)
    {
        free(resource->m_Resource);
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResCreateSceneDesc(dmResource::HFactory factory,
                                                void* context,
                                                const void* buffer, uint32_t buffer_size,
                                                dmResource::SResourceDescriptor* resource,
                                                const char* filename)
    {

        dmGuiDDF::SceneDesc* scene_desc;
        dmDDF::Result e = dmDDF::LoadMessage<dmGuiDDF::SceneDesc>(buffer, buffer_size, &scene_desc);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }

        const char* script;
        dmResource::FactoryResult fr = dmResource::Get(factory, scene_desc->m_Script, (void**) &script);
        if (fr != dmResource::FACTORY_RESULT_OK)
        {
            dmDDF::FreeMessage(scene_desc);
            return dmResource::CREATE_RESULT_UNKNOWN;
        }

        GuiScenePrototype* scene_prototype = new GuiScenePrototype();
        scene_prototype->m_SceneDesc = scene_desc;
        scene_prototype->m_Script = script;
        scene_prototype->m_FontMaps.SetCapacity(scene_desc->m_Fonts.m_Count);
        for (uint32_t i = 0; i < scene_desc->m_Fonts.m_Count; ++i)
        {
            dmRender::HFontMap font_map;
            dmResource::FactoryResult r = dmResource::Get(factory, scene_desc->m_Fonts[i].m_Font, (void**) &font_map);
            if (r != dmResource::FACTORY_RESULT_OK)
            {
                goto bail;
            }
            scene_prototype->m_FontMaps.Push(font_map);
        }

        scene_prototype->m_Textures.SetCapacity(scene_desc->m_Textures.m_Count);
        for (uint32_t i = 0; i < scene_desc->m_Textures.m_Count; ++i)
        {
            dmGraphics::HTexture texture;
            dmResource::FactoryResult r = dmResource::Get(factory, scene_desc->m_Textures[i], (void**) &texture);
            if (r != dmResource::FACTORY_RESULT_OK)
            {
                goto bail;
            }
            scene_prototype->m_Textures.Push(texture);
        }

        scene_prototype->m_Path = strdup(scene_desc->m_Script);
        resource->m_Resource = (void*) scene_prototype;

        return dmResource::CREATE_RESULT_OK;
bail:
        for (uint32_t j = 0; j < scene_prototype->m_FontMaps.Size(); ++j)
        {
            dmResource::Release(factory, scene_prototype->m_FontMaps[j]);
        }
        for (uint32_t j = 0; j < scene_prototype->m_Textures.Size(); ++j)
        {
            dmResource::Release(factory, scene_prototype->m_Textures[j]);
        }
        dmResource::Release(factory, (void*) scene_prototype->m_Script);
        dmDDF::FreeMessage(scene_desc);
        delete scene_prototype;
        return dmResource::CREATE_RESULT_UNKNOWN;
    }

    dmResource::CreateResult ResDestroySceneDesc(dmResource::HFactory factory,
                                                 void* context,
                                                 dmResource::SResourceDescriptor* resource)
    {
        GuiScenePrototype* scene_prototype = (GuiScenePrototype*) resource->m_Resource;

        for (uint32_t i = 0; i < scene_prototype->m_FontMaps.Size(); ++i)
        {
            dmResource::Release(factory, (void*) scene_prototype->m_FontMaps[i]);
        }

        for (uint32_t i = 0; i < scene_prototype->m_Textures.Size(); ++i)
        {
            dmResource::Release(factory, (void*) scene_prototype->m_Textures[i]);
        }

        dmResource::Release(factory, (void*) scene_prototype->m_Script);
        dmDDF::FreeMessage(scene_prototype->m_SceneDesc);
        free((void*)scene_prototype->m_Path);
        delete scene_prototype;

        return dmResource::CREATE_RESULT_OK;
    }
}
