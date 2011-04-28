#include "engine.h"
#include "engine_private.h"

#include <vectormath/cpp/vectormath_aos.h>

#include <sys/stat.h>

#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/profile.h>
#include <dlib/time.h>

#include <sound/sound.h>

#include <render/material.h>

// Windows defines DrawText
#undef DrawText
#include <render/render_ddf.h>

#include <particle/particle.h>

#include <gameobject/gameobject_ddf.h>

#include <render/render_ddf.h>

#include <gamesys/model_ddf.h>
#include <gamesys/physics_ddf.h>

#include "physics_debug_render.h"
#include "profile_render.h"

using namespace Vectormath::Aos;

namespace dmEngine
{
#define SYSTEM_SOCKET_NAME "@system"

    void GetWorldTransform(void* user_data, Point3& position, Quat& rotation)
    {
        if (!user_data)
            return;
        dmGameObject::HInstance instance = (dmGameObject::HInstance)user_data;
        position = dmGameObject::GetWorldPosition(instance);
        rotation = dmGameObject::GetWorldRotation(instance);
    }

    void SetWorldTransform(void* user_data, const Point3& position, const Quat& rotation)
    {
        if (!user_data)
            return;
        dmGameObject::HInstance instance = (dmGameObject::HInstance)user_data;
        dmGameObject::SetPosition(instance, position);
        dmGameObject::SetRotation(instance, rotation);
    }

    void SetObjectModel(void* visual_object, Quat* rotation, Point3* position)
    {
        if (!visual_object) return;
        dmGameObject::HInstance go = (dmGameObject::HInstance) visual_object;
        *position = dmGameObject::GetWorldPosition(go);
        *rotation = dmGameObject::GetWorldRotation(go);
    }

    void OnWindowResize(void* user_data, uint32_t width, uint32_t height)
    {
        uint32_t data_size = sizeof(dmRenderDDF::WindowResized);
        uintptr_t descriptor = (uintptr_t)dmRenderDDF::WindowResized::m_DDFDescriptor;
        dmhash_t message_id = dmHashString64(dmRenderDDF::WindowResized::m_DDFDescriptor->m_Name);

        dmRenderDDF::WindowResized window_resized;
        window_resized.m_Width = width;
        window_resized.m_Height = height;

        dmMessage::URL receiver;
        dmMessage::Result result = dmMessage::GetSocket(dmRender::RENDER_SOCKET_NAME, &receiver.m_Socket);
        if (result != dmMessage::RESULT_OK)
        {
            dmLogError("Could not find '%s' socket.", dmRender::RENDER_SOCKET_NAME);
        }
        else
        {
            result = dmMessage::Post(0x0, &receiver, message_id, 0, descriptor, &window_resized, data_size);
            if (result != dmMessage::RESULT_OK)
            {
                dmLogError("Could not send 'window_resized' to '%s' socket.", dmRender::RENDER_SOCKET_NAME);
            }
        }
    }

    void Dispatch(dmMessage::Message *message_object, void* user_ptr);

    Stats::Stats()
    : m_FrameCount(0)
    {

    }

    Engine::Engine()
    : m_Alive(true)
    , m_MainCollection(0)
    , m_LastReloadMTime(0)
    , m_MouseSensitivity(1.0f)
    , m_ShowProfile(false)
    , m_GraphicsContext(0)
    , m_RenderContext(0)
    , m_ScriptContext(0x0)
    , m_Factory(0x0)
    , m_SystemSocket(0x0)
    , m_SystemFontMap(0x0)
    , m_InputContext(0x0)
    , m_GameInputBinding(0x0)
    , m_RenderScriptPrototype(0x0)
    , m_Stats()
    {
        m_Register = dmGameObject::NewRegister();
        m_InputBuffer.SetCapacity(64);

        m_PhysicsContext.m_Context3D = 0x0;
        m_PhysicsContext.m_Debug = false;
        m_PhysicsContext.m_3D = true;
        m_EmitterContext.m_Debug = false;
        m_GuiRenderContext.m_GuiContext = 0x0;
        m_GuiRenderContext.m_RenderContext = 0x0;
        m_SpriteContext.m_RenderContext = 0x0;
        m_SpriteContext.m_MaxSpriteCount = 0;
    }

    HEngine New()
    {
        return new Engine();
    }

    void Delete(HEngine engine)
    {
        if (engine->m_MainCollection)
            dmResource::Release(engine->m_Factory, engine->m_MainCollection);
        dmGameObject::DeleteRegister(engine->m_Register);

        UnloadBootstrapContent(engine);

        dmSound::Finalize();

        dmInput::DeleteContext(engine->m_InputContext);

        dmRender::DeleteRenderContext(engine->m_RenderContext);

        dmHID::Finalize();

        dmGameObject::Finalize();

        if (engine->m_Factory)
            dmResource::DeleteFactory(engine->m_Factory);

        if (engine->m_ScriptContext)
            dmScript::DeleteContext(engine->m_ScriptContext);

        if (engine->m_GraphicsContext)
            dmGraphics::DeleteContext(engine->m_GraphicsContext);

        if (engine->m_GuiRenderContext.m_GuiContext)
            dmGui::DeleteContext(engine->m_GuiRenderContext.m_GuiContext);
        if (engine->m_SystemSocket)
            dmMessage::DeleteSocket(engine->m_SystemSocket);

        if (engine->m_PhysicsContext.m_Context3D)
        {
            if (engine->m_PhysicsContext.m_3D)
                dmPhysics::DeleteContext3D(engine->m_PhysicsContext.m_Context3D);
            else
                dmPhysics::DeleteContext2D(engine->m_PhysicsContext.m_Context2D);
        }

        dmProfile::Finalize();

        delete engine;
    }

    bool Init(HEngine engine, int argc, char *argv[])
    {
        const char* default_project_files[] =
        {
            "build/default/game.projectc",
            "build/default/content/game.projectc"
        };
        const char* default_content_roots[] =
        {
            "build/default",
            "build/default/content"
        };
        const char** project_files = default_project_files;
        uint32_t project_file_count = 2;
        if (argc > 1 && argv[argc-1][0] != '-')
        {
            project_files = (const char**)&argv[argc-1];
            project_file_count = 1;
        }

        dmConfigFile::HConfig config;
        dmConfigFile::Result config_result = dmConfigFile::RESULT_FILE_NOT_FOUND;
        uint32_t current_project_file = 0;
        while (config_result != dmConfigFile::RESULT_OK && current_project_file < project_file_count)
        {
            config_result = dmConfigFile::Load(project_files[current_project_file], argc, (const char**) argv, &config);
            ++current_project_file;
        }
        if (config_result != dmConfigFile::RESULT_OK)
        {
            dmLogFatal("Unable to load project file from any of the locations:");
            for (uint32_t i = 0; i < project_file_count; ++i)
                dmLogFatal("%s", project_files[i]);
            return false;
        }
        const char* content_root = default_content_roots[current_project_file - 1];
        const char* update_order = dmConfigFile::GetString(config, "gameobject.update_order", 0);

        dmProfile::Initialize(256, 1024 * 16, 128);
        // This scope is mainly here to make sure the "Main" scope is created first.
        DM_PROFILE(Engine, "Init");

        engine->m_GraphicsContext = dmGraphics::NewContext();

        dmGraphics::WindowParams window_params;
        window_params.m_ResizeCallback = OnWindowResize;
        window_params.m_ResizeCallbackUserData = engine;
        window_params.m_Width = dmConfigFile::GetInt(config, "display.width", 960);
        window_params.m_Height = dmConfigFile::GetInt(config, "display.height", 540);
        window_params.m_Samples = dmConfigFile::GetInt(config, "display.samples", 0);
        window_params.m_Title = dmConfigFile::GetString(config, "project.title", "TestTitle");
        window_params.m_Fullscreen = false;
        window_params.m_PrintDeviceInfo = false;

        dmGraphics::WindowResult window_result = dmGraphics::OpenWindow(engine->m_GraphicsContext, &window_params);
        if (window_result != dmGraphics::WINDOW_RESULT_OK)
        {
            dmLogFatal("Could not open window (%d).", window_result);
            return false;
        }

        engine->m_ScriptContext = dmScript::NewContext();

        dmGameObject::Initialize(engine->m_ScriptContext);

        RegisterDDFTypes(engine);

        dmHID::Initialize();

        dmSound::InitializeParams sound_params;
        dmSound::Initialize(config, &sound_params);

        dmRender::RenderContextParams render_params;
        render_params.m_MaxRenderTypes = 16;
        render_params.m_MaxInstances = 1024; // TODO: Should be configurable
        render_params.m_MaxRenderTargets = 32;
        render_params.m_VertexProgramData = ::DEBUG_VPC;
        render_params.m_VertexProgramDataSize = ::DEBUG_VPC_SIZE;
        render_params.m_FragmentProgramData = ::DEBUG_FPC;
        render_params.m_FragmentProgramDataSize = ::DEBUG_FPC_SIZE;
        render_params.m_MaxCharacters = 2048 * 4;
        render_params.m_CommandBufferSize = 1024;
        engine->m_RenderContext = dmRender::NewRenderContext(engine->m_GraphicsContext, render_params);

        engine->m_EmitterContext.m_RenderContext = engine->m_RenderContext;
        engine->m_EmitterContext.m_MaxEmitterCount = dmConfigFile::GetInt(config, dmParticle::MAX_EMITTER_COUNT_KEY, 0);
        engine->m_EmitterContext.m_MaxParticleCount = dmConfigFile::GetInt(config, dmParticle::MAX_PARTICLE_COUNT_KEY, 0);
        engine->m_EmitterContext.m_Debug = false;

        const uint32_t max_resources = 256;

        dmResource::NewFactoryParams params;
        int32_t http_cache = dmConfigFile::GetInt(config, "resource.http_cache", 1);
        params.m_MaxResources = max_resources;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT | RESOURCE_FACTORY_FLAGS_HTTP_SERVER;
        if (http_cache)
            params.m_Flags = RESOURCE_FACTORY_FLAGS_HTTP_CACHE;
        params.m_StreamBufferSize = 8 * 1024 * 1024; // We have some *large* textures...!
        params.m_BuiltinsArchive = (const void*) BUILTINS_ARC;
        params.m_BuiltinsArchiveSize = BUILTINS_ARC_SIZE;

        engine->m_Factory = dmResource::NewFactory(&params, dmConfigFile::GetString(config, "resource.uri", content_root));

        float repeat_delay = dmConfigFile::GetFloat(config, "input.repeat_delay", 0.5f);
        float repeat_interval = dmConfigFile::GetFloat(config, "input.repeat_interval", 0.2f);
        engine->m_InputContext = dmInput::NewContext(repeat_delay, repeat_interval);

        dmMessage::Result mr = dmMessage::NewSocket(SYSTEM_SOCKET_NAME, &engine->m_SystemSocket);
        if (mr != dmMessage::RESULT_OK)
        {
            dmLogFatal("Unable to create system socket: %s (%d)", SYSTEM_SOCKET_NAME, mr);
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }

        dmGui::NewContextParams gui_params;
        gui_params.m_ScriptContext = engine->m_ScriptContext;
        gui_params.m_GetURLCallback = dmGameSystem::GuiGetURLCallback;
        gui_params.m_GetUserDataCallback = dmGameSystem::GuiGetUserDataCallback;
        gui_params.m_ResolvePathCallback = dmGameSystem::GuiResolvePathCallback;
        engine->m_GuiRenderContext.m_GuiContext = dmGui::NewContext(&gui_params);
        engine->m_GuiRenderContext.m_RenderContext = engine->m_RenderContext;

        dmPhysics::NewContextParams physics_params;
        physics_params.m_WorldCount = dmConfigFile::GetInt(config, "physics.world_count", 4);
        const char* physics_type = dmConfigFile::GetString(config, "physics.type", "3D");
        physics_params.m_Gravity.setX(dmConfigFile::GetFloat(config, "physics.gravity_x", 0.0f));
        physics_params.m_Gravity.setY(dmConfigFile::GetFloat(config, "physics.gravity_y", -10.0f));
        physics_params.m_Gravity.setZ(dmConfigFile::GetFloat(config, "physics.gravity_z", 0.0f));
        if (strncmp(physics_type, "3D", 2) == 0)
        {
            engine->m_PhysicsContext.m_3D = true;
            engine->m_PhysicsContext.m_Context3D = dmPhysics::NewContext3D(physics_params);
        }
        else if (strncmp(physics_type, "2D", 2) == 0)
        {
            engine->m_PhysicsContext.m_3D = false;
            engine->m_PhysicsContext.m_Context2D = dmPhysics::NewContext2D(physics_params);
        }
        engine->m_PhysicsContext.m_Debug = dmConfigFile::GetInt(config, "physics.debug", 0);

        dmPhysics::DebugCallbacks debug_callbacks;
        debug_callbacks.m_UserData = engine->m_RenderContext;
        debug_callbacks.m_DrawLines = PhysicsDebugRender::DrawLines;
        debug_callbacks.m_DrawTriangles = 0x0;
        if (engine->m_PhysicsContext.m_3D)
            dmPhysics::SetDebugCallbacks3D(engine->m_PhysicsContext.m_Context3D, debug_callbacks);
        else
            dmPhysics::SetDebugCallbacks2D(engine->m_PhysicsContext.m_Context2D, debug_callbacks);

        engine->m_SpriteContext.m_RenderContext = engine->m_RenderContext;
        engine->m_SpriteContext.m_MaxSpriteCount = dmConfigFile::GetInt(config, "sprite.max_count", 64);

        dmResource::FactoryResult fact_result;
        dmGameObject::Result res;

        fact_result = dmGameObject::RegisterResourceTypes(engine->m_Factory, engine->m_Register);
        if (fact_result != dmResource::FACTORY_RESULT_OK)
            goto bail;
        fact_result = dmGameSystem::RegisterResourceTypes(engine->m_Factory, engine->m_RenderContext, engine->m_GuiRenderContext.m_GuiContext, engine->m_InputContext, &engine->m_PhysicsContext);
        if (fact_result != dmResource::FACTORY_RESULT_OK)
            goto bail;

        if (dmGameObject::RegisterComponentTypes(engine->m_Factory, engine->m_Register) != dmGameObject::RESULT_OK)
            goto bail;

        res = dmGameSystem::RegisterComponentTypes(engine->m_Factory, engine->m_Register, engine->m_RenderContext, &engine->m_PhysicsContext, &engine->m_EmitterContext, &engine->m_GuiRenderContext, &engine->m_SpriteContext);
        if (res != dmGameObject::RESULT_OK)
            goto bail;

        if (!LoadBootstrapContent(engine, config))
        {
            dmLogWarning("Unable to load bootstrap data.");
            goto bail;
        }

        if (engine->m_RenderScriptPrototype)
            InitRenderScriptInstance(engine->m_RenderScriptPrototype->m_Instance);

        fact_result = dmResource::Get(engine->m_Factory, dmConfigFile::GetString(config, "bootstrap.main_collection", "logic/main.collectionc"), (void**) &engine->m_MainCollection);
        if (fact_result != dmResource::FACTORY_RESULT_OK)
            goto bail;
        dmGameObject::Init(engine->m_MainCollection);

        engine->m_LastReloadMTime = 0;
        struct stat file_stat;
        if (stat("build/default/content/reload", &file_stat) == 0)
        {
            engine->m_LastReloadMTime = (uint32_t) file_stat.st_mtime;
        }

        if (update_order)
        {
            char* tmp = strdup(update_order);
            char* s, *last;
            s = dmStrTok(tmp, ",", &last);
            uint16_t prio = 0;
            while (s)
            {
                uint32_t type;
                fact_result = dmResource::GetTypeFromExtension(engine->m_Factory, s, &type);
                if (fact_result == dmResource::FACTORY_RESULT_OK)
                {
                    dmGameObject::SetUpdateOrderPrio(engine->m_Register, type, prio++);
                }
                else
                {
                    dmLogError("Unknown resource-type extension for update_order: %s", s);
                }
                s = dmStrTok(0, ",", &last);
            }
            free(tmp);
        }

        dmConfigFile::Delete(config);
        return true;

bail:
        dmConfigFile::Delete(config);
        return false;
    }

    void GOActionCallback(dmhash_t action_id, dmInput::Action* action, void* user_data)
    {
        dmArray<dmGameObject::InputAction>* input_buffer = (dmArray<dmGameObject::InputAction>*)user_data;
        dmGameObject::InputAction input_action;
        input_action.m_ActionId = action_id;
        input_action.m_Value = action->m_Value;
        input_action.m_Pressed = action->m_Pressed;
        input_action.m_Released = action->m_Released;
        input_action.m_Repeated = action->m_Repeated;
        input_buffer->Push(input_action);
    }

    int32_t Run(HEngine engine)
    {
        const float fps = 60.0f;
        float actual_fps = fps;
        float fixed_dt = 1.0f / fps;

        uint64_t time_stamp = dmTime::GetTime();

        engine->m_Alive = true;
        engine->m_ExitCode = 0;

        while (engine->m_Alive)
        {
            dmProfile::HProfile profile = dmProfile::Begin();
            {
                DM_PROFILE(Engine, "Frame");

                // We had buffering problems with the output when running the engine inside the editor
                // Flushing stdout/stderr solves this problem.
                fflush(stdout);
                fflush(stderr);

                dmResource::UpdateFactory(engine->m_Factory);

                dmHID::Update();
                dmSound::Update();

                dmHID::KeyboardPacket keybdata;
                dmHID::GetKeyboardPacket(&keybdata);

                if (dmHID::GetKey(&keybdata, dmHID::KEY_ESC) || !dmGraphics::GetWindowState(engine->m_GraphicsContext, dmGraphics::WINDOW_STATE_OPENED))
                {
                    engine->m_Alive = false;
                    break;
                }

                dmInput::UpdateBinding(engine->m_GameInputBinding, fixed_dt);

                engine->m_InputBuffer.SetSize(0);
                dmInput::ForEachActive(engine->m_GameInputBinding, GOActionCallback, &engine->m_InputBuffer);
                if (engine->m_InputBuffer.Size() > 0)
                {
                    dmGameObject::DispatchInput(engine->m_MainCollection, &engine->m_InputBuffer[0], engine->m_InputBuffer.Size());
                }

                dmGameObject::UpdateContext update_context;
                update_context.m_DT = fixed_dt;
                dmGameObject::Update(engine->m_MainCollection, &update_context);

                if (engine->m_RenderScriptPrototype)
                {
                    dmRender::UpdateRenderScriptInstance(engine->m_RenderScriptPrototype->m_Instance);
                }
                else
                {
                    dmGraphics::SetViewport(engine->m_GraphicsContext, 0, 0, dmGraphics::GetWindowWidth(engine->m_GraphicsContext), dmGraphics::GetWindowHeight(engine->m_GraphicsContext));
                    dmGraphics::Clear(engine->m_GraphicsContext, dmGraphics::BUFFER_TYPE_COLOR_BIT | dmGraphics::BUFFER_TYPE_DEPTH_BIT, 0, 0, 0, 0, 1.0, 0);
                    dmRender::Draw(engine->m_RenderContext, 0x0);
                }

                dmMessage::Dispatch(engine->m_SystemSocket, Dispatch, engine);

                dmGameObject::PostUpdate(engine->m_MainCollection);

                dmRender::ClearRenderObjects(engine->m_RenderContext);
            }

            dmProfile::Pause(true);
            if (engine->m_ShowProfile)
            {
                dmProfileRender::Draw(profile, engine->m_RenderContext, engine->m_SystemFontMap);
                dmRender::SetViewMatrix(engine->m_RenderContext, Matrix4::identity());
                dmRender::SetProjectionMatrix(engine->m_RenderContext, Matrix4::orthographic(0.0f, dmGraphics::GetWindowWidth(engine->m_GraphicsContext), dmGraphics::GetWindowHeight(engine->m_GraphicsContext), 0.0f, 1.0f, -1.0f));
                dmRender::Draw(engine->m_RenderContext, 0x0);
                dmRender::ClearRenderObjects(engine->m_RenderContext);
            }
            dmProfile::Pause(false);
            dmProfile::Release(profile);

            dmGraphics::Flip(engine->m_GraphicsContext);

            uint64_t new_time_stamp, delta;
            new_time_stamp = dmTime::GetTime();
            delta = new_time_stamp - time_stamp;
            time_stamp = new_time_stamp;

            float actual_dt = delta / 1000000.0f;
            if (actual_dt > 0.0f)
                actual_fps = 1.0f / actual_dt;
            else
                actual_fps = -1.0f;

            ++engine->m_Stats.m_FrameCount;
        }
        return engine->m_ExitCode;
    }

    void Exit(HEngine engine, int32_t code)
    {
        engine->m_Alive = false;
        engine->m_ExitCode = code;
    }

    void Dispatch(dmMessage::Message* message, void* user_ptr)
    {
        Engine* self = (Engine*) user_ptr;

        if (message->m_Descriptor != 0)
        {
            dmDDF::Descriptor* descriptor = (dmDDF::Descriptor*)message->m_Descriptor;

            if (descriptor == dmEngineDDF::Exit::m_DDFDescriptor)
            {
                dmEngineDDF::Exit* ddf = (dmEngineDDF::Exit*) message->m_Data;
                dmEngine::Exit(self, ddf->m_Code);
            }
            else if (descriptor == dmEngineDDF::ToggleProfile::m_DDFDescriptor)
            {
                self->m_ShowProfile = !self->m_ShowProfile;
            }
            else
            {
                dmLogError("Unknown ddf message '%s' sent to socket '%s'.\n", descriptor->m_Name, SYSTEM_SOCKET_NAME);
            }
        }
        else
        {
            dmLogError("Only system messages can be sent to the '%s' socket.\n", SYSTEM_SOCKET_NAME);
        }
    }

    void RegisterDDFTypes(HEngine engine)
    {
        dmGameSystem::RegisterDDFTypes(engine->m_ScriptContext);

        dmScript::RegisterDDFType(engine->m_ScriptContext, dmEngineDDF::Exit::m_DDFDescriptor);
        dmScript::RegisterDDFType(engine->m_ScriptContext, dmEngineDDF::ToggleProfile::m_DDFDescriptor);
        dmScript::RegisterDDFType(engine->m_ScriptContext, dmRenderDDF::DrawText::m_DDFDescriptor);
        dmScript::RegisterDDFType(engine->m_ScriptContext, dmRenderDDF::DrawLine::m_DDFDescriptor);
        dmScript::RegisterDDFType(engine->m_ScriptContext, dmModelDDF::SetTexture::m_DDFDescriptor);
        dmScript::RegisterDDFType(engine->m_ScriptContext, dmModelDDF::SetVertexConstant::m_DDFDescriptor);
        dmScript::RegisterDDFType(engine->m_ScriptContext, dmModelDDF::ResetVertexConstant::m_DDFDescriptor);
        dmScript::RegisterDDFType(engine->m_ScriptContext, dmModelDDF::SetFragmentConstant::m_DDFDescriptor);
        dmScript::RegisterDDFType(engine->m_ScriptContext, dmModelDDF::ResetFragmentConstant::m_DDFDescriptor);
        dmScript::RegisterDDFType(engine->m_ScriptContext, dmGameObjectDDF::AcquireInputFocus::m_DDFDescriptor);
        dmScript::RegisterDDFType(engine->m_ScriptContext, dmGameObjectDDF::ReleaseInputFocus::m_DDFDescriptor);
        dmScript::RegisterDDFType(engine->m_ScriptContext, dmGameObjectDDF::GameObjectTransformQuery::m_DDFDescriptor);
        dmScript::RegisterDDFType(engine->m_ScriptContext, dmGameObjectDDF::GameObjectTransformResponse::m_DDFDescriptor);
        dmScript::RegisterDDFType(engine->m_ScriptContext, dmGameObjectDDF::SetParent::m_DDFDescriptor);
    }

    bool LoadBootstrapContent(HEngine engine, dmConfigFile::HConfig config)
    {
        const char* system_font_map = "builtins/fonts/system_font.fontc";
        dmResource::FactoryResult fact_error = dmResource::Get(engine->m_Factory, system_font_map, (void**) &engine->m_SystemFontMap);
        if (fact_error != dmResource::FACTORY_RESULT_OK)
        {
            dmLogFatal("Could not load system font map '%s'.", system_font_map);
            return false;
        }
        dmRender::SetSystemFontMap(engine->m_RenderContext, engine->m_SystemFontMap);

        const char* gamepads = dmConfigFile::GetString(config, "bootstrap.gamepads", "input/default.gamepadsc");
        dmInputDDF::GamepadMaps* gamepad_maps_ddf;
        fact_error = dmResource::Get(engine->m_Factory, gamepads, (void**)&gamepad_maps_ddf);
        if (fact_error != dmResource::FACTORY_RESULT_OK)
            return false;
        dmInput::RegisterGamepads(engine->m_InputContext, gamepad_maps_ddf);
        dmResource::Release(engine->m_Factory, gamepad_maps_ddf);

        const char* game_input_binding = dmConfigFile::GetString(config, "bootstrap.game_binding", "input/game.input_bindingc");
        fact_error = dmResource::Get(engine->m_Factory, game_input_binding, (void**)&engine->m_GameInputBinding);
        if (fact_error != dmResource::FACTORY_RESULT_OK)
            return false;

        const char* render_path = dmConfigFile::GetString(config, "bootstrap.render", 0x0);
        if (render_path != 0x0)
        {
            fact_error = dmResource::Get(engine->m_Factory, render_path, (void**)&engine->m_RenderScriptPrototype);
            if (fact_error != dmResource::FACTORY_RESULT_OK)
                return false;
        }

        return true;
    }

    void UnloadBootstrapContent(HEngine engine)
    {
        if (engine->m_RenderScriptPrototype)
            dmResource::Release(engine->m_Factory, engine->m_RenderScriptPrototype);
        if (engine->m_SystemFontMap)
            dmResource::Release(engine->m_Factory, engine->m_SystemFontMap);
        if (engine->m_GameInputBinding)
            dmResource::Release(engine->m_Factory, engine->m_GameInputBinding);
    }

    uint32_t GetFrameCount(HEngine engine)
    {
        return engine->m_Stats.m_FrameCount;
    }
}

