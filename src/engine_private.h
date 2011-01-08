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
        dmHashTable32<dmGameObject::HCollection>    m_Collections;
        dmGameObject::HCollection                   m_ActiveCollection;
        dmArray<dmGameObject::InputAction>          m_InputBuffer;
        uint32_t                                    m_SpawnCount;

        uint32_t                                    m_LastReloadMTime;

        float                                       m_MouseSensitivity;
        bool                                        m_ShowProfile;

        bool                                        m_WarpTimeStep;
        float                                       m_TimeStepFactor;
        dmEngineDDF::TimeStepMode                   m_TimeStepMode;

        dmGraphics::HDevice                         m_GraphicsDevice;
        dmRender::HRenderContext                    m_RenderContext;
        dmGameSystem::PhysicsContext                m_PhysicsContext;
        dmGameSystem::EmitterContext                m_EmitterContext;
        dmResource::HFactory                        m_Factory;

        dmRender::HFont                             m_Font;
        dmRender::HFont                             m_SmallFont;
        dmRender::HMaterial                         m_DebugMaterial;
        dmInput::HContext                           m_InputContext;
        dmInput::HBinding                           m_GameInputBinding;

        dmGameSystem::RenderScriptPrototype*        m_RenderScriptPrototype;

        Stats                                       m_Stats;
    };

    void ReloadResources(HEngine engine, const char* extension);
    void RegisterDDFTypes();
    bool LoadBootstrapContent(HEngine engine, dmConfigFile::HConfig config);
    void UnloadBootstrapContent(HEngine engine);
}

#endif // DM_ENGINE_PRIVATE_H
