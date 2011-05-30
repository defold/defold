#ifndef DM_ENGINE_PRIVATE_H
#define DM_ENGINE_PRIVATE_H

#include <stdint.h>

#include <dlib/configfile.h>
#include <dlib/hashtable.h>
#include <dlib/message.h>

#include <resource/resource.h>

#include <render/render.h>
#include <render/font_renderer.h>

#include <input/input.h>

#include <gameobject/gameobject.h>

#include <gui/gui.h>

#include <gamesys/gamesys.h>

#include "engine_ddf.h"

namespace dmEngine
{
    struct Stats
    {
        Stats();

        uint32_t m_FrameCount;
    };

    struct Engine
    {
        Engine();

        bool                                        m_Alive;
        int32_t                                     m_ExitCode;

        dmGameObject::HRegister                     m_Register;
        dmGameObject::HCollection                   m_MainCollection;
        dmArray<dmGameObject::InputAction>          m_InputBuffer;

        uint32_t                                    m_LastReloadMTime;

        float                                       m_MouseSensitivity;
        bool                                        m_ShowProfile;

        dmGraphics::HContext                        m_GraphicsContext;
        dmRender::HRenderContext                    m_RenderContext;
        dmGameSystem::PhysicsContext                m_PhysicsContext;
        dmGameSystem::EmitterContext                m_EmitterContext;
        dmScript::HContext                          m_ScriptContext;
        dmResource::HFactory                        m_Factory;
        dmGameSystem::GuiContext                    m_GuiContext;
        dmMessage::HSocket                          m_SystemSocket;
        dmGameSystem::SpriteContext                 m_SpriteContext;

        dmRender::HFontMap                          m_SystemFontMap;
        dmInput::HContext                           m_InputContext;
        dmInput::HBinding                           m_GameInputBinding;

        dmGameSystem::RenderScriptPrototype*        m_RenderScriptPrototype;

        Stats                                       m_Stats;
    };

    void ReloadResources(HEngine engine, const char* extension);
    void RegisterDDFTypes(HEngine engine);
    bool LoadBootstrapContent(HEngine engine, dmConfigFile::HConfig config);
    void UnloadBootstrapContent(HEngine engine);
}

#endif // DM_ENGINE_PRIVATE_H
