// Copyright 2020-2025 The Defold Foundation
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

#ifndef DM_ENGINE_PRIVATE_H
#define DM_ENGINE_PRIVATE_H

#include <stdint.h>

#include <dmsdk/dlib/configfile.h>
#include <dlib/hashtable.h>
#include <dlib/message.h>
#include <dlib/http_cache.h>

#include <resource/resource.h>

#include <render/render.h>
#include <render/font_renderer.h>
#include <rig/rig.h>

#include <hid/hid.h>
#include <input/input.h>

#include <gameobject/gameobject.h>

#include <gui/gui.h>

#include <gamesys/gamesys.h>

#include <record/record.h>

#include "engine.h"
#include "engine_service.h"
#include "engine.h"
#include <engine/engine_ddf.h>
#include <dmsdk/gamesys/resources/res_font.h>

namespace dmEngine
{
    const uint32_t MAX_RUN_RESULT_ARGS = 32;
    struct RunResult
    {
        RunResult()
        {
            memset(this, 0, sizeof(*this));
        }

        //`RunResult::EXIT` value (as `run_action` in `AppDelegate.m` `ShutdownEngine()`)
        // compares with GLFW_APP_RUN_EXIT, that's why `RunResult` should have the same values as `glfwAppRunAction`
        enum Action
        {
            NONE,
            EXIT = -1,
            REBOOT = 1,
        };

        void Free()
        {
            for (uint32_t i = 0; i < MAX_RUN_RESULT_ARGS; ++i)
            {
                if (m_Argv[i])
                {
                    free(m_Argv[i]);
                }
            }
        }

        int     m_Argc;
        char*   m_Argv[MAX_RUN_RESULT_ARGS];
        int32_t m_ExitCode;
        Action  m_Action;
    };

    struct Stats
    {
        Stats();

        uint32_t m_FrameCount;
        float    m_TotalTime;   // Total running time of the game
    };

    struct RecordData
    {
        RecordData()
        {
            memset(this, 0, sizeof(*this));
        }

        dmRecord::HRecorder m_Recorder;
        char*               m_Buffer;
        uint32_t            m_FrameCount;
        uint32_t            m_FramePeriod;
        uint32_t            m_Fps;
    };

    struct Engine
    {
        Engine(dmEngineService::HEngineService engine_service);
        dmEngineService::HEngineService             m_EngineService;
        dmConfigFile::HConfig                       m_Config;
        dmPlatform::HWindow                         m_Window;

        RunResult                                   m_RunResult;
        bool                                        m_Alive;

        dmGameObject::HRegister                     m_Register;
        dmGameObject::HCollection                   m_MainCollection;
        dmArray<dmGameObject::InputAction>          m_InputBuffer;
        dmHashTable64<void*>                        m_ResourceTypeContexts;

        uint32_t                                    m_LastReloadMTime;

        float                                       m_MouseSensitivity;

        dmJobThread::HContext                       m_JobThreadContext;
        dmGraphics::HContext                        m_GraphicsContext;
        dmRender::HRenderContext                    m_RenderContext;
        dmGameSystem::PhysicsContextBox2D           m_PhysicsContextBox2D;
        dmGameSystem::PhysicsContextBullet3D        m_PhysicsContextBullet3D;
        dmGameSystem::ParticleFXContext             m_ParticleFXContext;
        /// If the shared context is set, the three environment specific contexts below will point to the same context
        dmScript::HContext                          m_SharedScriptContext;
        dmScript::HContext                          m_GOScriptContext;
        dmScript::HContext                          m_RenderScriptContext;
        dmScript::HContext                          m_GuiScriptContext;
        dmResource::HFactory                        m_Factory;
        dmGui::HContext                             m_GuiContext;
        dmMessage::HSocket                          m_SystemSocket;
        dmGameSystem::SpriteContext                 m_SpriteContext;
        dmGameSystem::CollectionProxyContext        m_CollectionProxyContext;
        dmGameSystem::FactoryContext                m_FactoryContext;
        dmGameSystem::CollectionFactoryContext      m_CollectionFactoryContext;
        dmGameSystem::ModelContext                  m_ModelContext;
        dmGameSystem::LabelContext                  m_LabelContext;
        dmGameSystem::TilemapContext                m_TilemapContext;
        dmGameObject::ModuleContext                 m_ModuleContext;

        dmGameSystem::FontResource*                 m_SystemFont;
        dmHID::HContext                             m_HidContext;
        dmInput::HContext                           m_InputContext;
        dmInput::HBinding                           m_GameInputBinding;
        dmRender::HDisplayProfiles                  m_DisplayProfiles;
        dmHttpCache::HCache                         m_HttpCache;

        dmGameSystem::RenderScriptPrototype*        m_RenderScriptPrototype;

        Stats                                       m_Stats;

        bool                                        m_WasIconified;
        bool                                        m_QuitOnEsc;
        bool                                        m_ConnectionAppMode;        //!< If the app was started on a device, listening for connections
        bool                                        m_RunWhileIconified;
        bool                                        m_UseSwVSync;
        uint64_t                                    m_PreviousFrameTime;        // Used to calculate dt
        float                                       m_AccumFrameTime;           // Used to trigger frame updates when using m_UpdateFrequency != 0
        uint32_t                                    m_UpdateFrequency;
        uint32_t                                    m_FixedUpdateFrequency;
        uint32_t                                    m_Width;
        uint32_t                                    m_Height;
        uint32_t                                    m_ClearColor;
        float                                       m_InvPhysicalWidth;
        float                                       m_InvPhysicalHeight;
        float                                       m_MaxTimeStep;

        float                                       m_ThrottleCooldownMax;
        float                                       m_ThrottleCooldown;
        bool                                        m_ThrottleEnabled;

        RecordData                                  m_RecordData;
    };


    HEngine New(dmEngineService::HEngineService engine_service);
    void Delete(HEngine engine);
    bool Init(HEngine engine, int argc, char *argv[]);
    void Step(HEngine engine);

    void ReloadResources(HEngine engine, const char* extension);
    bool LoadBootstrapContent(HEngine engine, HConfigFile config);
    void UnloadBootstrapContent(HEngine engine);

    void SetEngineThrottle(HEngine engine, bool enable, float cooldown);

    // Creates and initializes the engine. Returns the engine instance
    typedef HEngine (*EngineCreate)(int argc, char** argv);
    // Destroys the engine instance after finalizing each system
    typedef void (*EngineDestroy)(HEngine engine);
    // Steps the engine 1 tick
    typedef dmEngine::UpdateResult (*EngineUpdate)(HEngine engine);
    // Called before the destroy function
    typedef void (*EngineGetResult)(HEngine engine, int* run_action, int* exit_code, int* argc, char*** argv);

    struct RunLoopParams
    {
        int     m_Argc;
        char**  m_Argv;

        void*   m_AppCtx;
        void    (*m_AppCreate)(void* ctx);
        void    (*m_AppDestroy)(void* ctx);

        EngineCreate        m_EngineCreate;
        EngineDestroy       m_EngineDestroy;
        EngineUpdate        m_EngineUpdate;
        EngineGetResult     m_EngineGetResult;
    };

    /**
     *
     */
    int RunLoop(const RunLoopParams* params);

    // For unit testing
    void GetStats(HEngine engine, Stats& stats);

    bool PlatformInitialize();
    void PlatformFinalize();
}

#endif // DM_ENGINE_PRIVATE_H
