#include "engine.h"

#include "engine_private.h"

#include <vectormath/cpp/vectormath_aos.h>
#include <sys/stat.h>

#include <stdio.h>
#include <algorithm>

#include <dlib/dlib.h>
#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/profile.h>
#include <dlib/time.h>
#include <dlib/math.h>
#include <dlib/path.h>
#include <dlib/sys.h>
#include <dlib/http_client.h>
#include <extension/extension.h>
#include <gamesys/gamesys.h>
#include <gamesys/model_ddf.h>
#include <gamesys/physics_ddf.h>
#include <gameobject/gameobject_ddf.h>
#include <gameobject/gameobject_script_util.h>
#include <hid/hid.h>
#include <sound/sound.h>
#include <render/render.h>
#include <render/render_ddf.h>
#include <particle/particle.h>
#include <tracking/tracking.h>
#include <tracking/tracking_ddf.h>
#include <liveupdate/liveupdate.h>

#include "engine_service.h"
#include "engine_version.h"
#include "physics_debug_render.h"
#include "profile_render.h"

// Embedded resources
#if defined(DM_RELEASE)
    static unsigned char* DEBUG_VPC = 0;
    static uint32_t DEBUG_VPC_SIZE = 0;
    static unsigned char* DEBUG_FPC = 0;
    static uint32_t DEBUG_FPC_SIZE = 0;

    static unsigned char* BUILTINS_ARCD = 0;
    static uint32_t BUILTINS_ARCD_SIZE = 0;
    static unsigned char* BUILTINS_ARCI = 0;
    static uint32_t BUILTINS_ARCI_SIZE = 0;
    static unsigned char* BUILTINS_DMANIFEST = 0;
    static uint32_t BUILTINS_DMANIFEST_SIZE = 0;
#else
    extern unsigned char DEBUG_VPC[];
    extern uint32_t DEBUG_VPC_SIZE;
    extern unsigned char DEBUG_FPC[];
    extern uint32_t DEBUG_FPC_SIZE;
    
    extern unsigned char BUILTINS_ARCD[];
    extern uint32_t BUILTINS_ARCD_SIZE;
    extern unsigned char BUILTINS_ARCI[];
    extern uint32_t BUILTINS_ARCI_SIZE;
    extern unsigned char BUILTINS_DMANIFEST[];
    extern uint32_t BUILTINS_DMANIFEST_SIZE;
    
    extern unsigned char CONNECT_PROJECT[];
    extern uint32_t CONNECT_PROJECT_SIZE;
#endif

using namespace Vectormath::Aos;

#if defined(__ANDROID__)
// On Android we need to notify the activity which input method to use
// before the keyboard is brought up. This choice is stored as a
// game.project config and used in dmEngine::Init(), passed along to
// the GLFW Android implementation.
extern "C" {
    extern void _glfwAndroidSetInputMethod(int);
    extern void _glfwAndroidSetImmersiveMode(int);
}
#endif

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
        dmhash_t message_id = dmRenderDDF::WindowResized::m_DDFDescriptor->m_NameHash;

        dmRenderDDF::WindowResized window_resized;
        window_resized.m_Width = width;
        window_resized.m_Height = height;

        dmMessage::URL receiver;
        dmMessage::ResetURL(receiver);
        dmMessage::Result result = dmMessage::GetSocket(dmRender::RENDER_SOCKET_NAME, &receiver.m_Socket);
        if (result != dmMessage::RESULT_OK)
        {
            dmLogError("Could not find '%s' socket.", dmRender::RENDER_SOCKET_NAME);
        }
        else
        {
            result = dmMessage::Post(0x0, &receiver, message_id, 0, descriptor, &window_resized, data_size, 0);
            if (result != dmMessage::RESULT_OK)
            {
                dmLogError("Could not send 'window_resized' to '%s' socket.", dmRender::RENDER_SOCKET_NAME);
            }
        }

        Engine* engine = (Engine*)user_data;
        engine->m_InvPhysicalWidth = 1.0f / width;
        engine->m_InvPhysicalHeight = 1.0f / height;
        // update gui context
        dmGui::SetPhysicalResolution(engine->m_GuiContext.m_GuiContext, width, height);

        dmGameSystem::OnWindowResized(width, height);
    }

    bool OnWindowClose(void* user_data)
    {
        Engine* engine = (Engine*)user_data;
        engine->m_Alive = false;
        // Never allow closing the window here, clean up and then close manually
        return false;
    }

    void Dispatch(dmMessage::Message *message_object, void* user_ptr);

    static void OnWindowFocus(void* user_data, uint32_t focus)
    {
        Engine* engine = (Engine*)user_data;
        dmExtension::Params params;
        params.m_ConfigFile = engine->m_Config;
        params.m_L          = 0;
        dmExtension::Event event;
        event.m_Event = focus ? dmExtension::EVENT_ID_ACTIVATEAPP : dmExtension::EVENT_ID_DEACTIVATEAPP;
        dmExtension::DispatchEvent( &params, &event );

        dmGameSystem::OnWindowFocus(focus != 0);
    }

    Stats::Stats()
    : m_FrameCount(0)
    {

    }

    Engine::Engine(dmEngineService::HEngineService engine_service)
    : m_Config(0)
    , m_Alive(true)
    , m_MainCollection(0)
    , m_LastReloadMTime(0)
    , m_MouseSensitivity(1.0f)
    , m_ShowProfile(false)
    , m_GraphicsContext(0)
    , m_RenderContext(0)
    , m_SharedScriptContext(0x0)
    , m_GOScriptContext(0x0)
    , m_RenderScriptContext(0x0)
    , m_GuiScriptContext(0x0)
    , m_Factory(0x0)
    , m_SystemSocket(0x0)
    , m_SystemFontMap(0x0)
    , m_HidContext(0x0)
    , m_InputContext(0x0)
    , m_GameInputBinding(0x0)
    , m_DisplayProfiles(0x0)
    , m_TrackingContext(0x0)
    , m_RenderScriptPrototype(0x0)
    , m_Stats()
    , m_WasIconified(true)
    , m_QuitOnEsc(false)
    , m_ConnectionAppMode(false)
    , m_Width(960)
    , m_Height(640)
    , m_InvPhysicalWidth(1.0f/960)
    , m_InvPhysicalHeight(1.0f/640)
    {
        m_EngineService = engine_service;
        m_Register = dmGameObject::NewRegister();
        m_InputBuffer.SetCapacity(64);

        m_PhysicsContext.m_Context3D = 0x0;
        m_PhysicsContext.m_Debug = false;
        m_PhysicsContext.m_3D = false;
        m_GuiContext.m_GuiContext = 0x0;
        m_GuiContext.m_RenderContext = 0x0;
        m_SpriteContext.m_RenderContext = 0x0;
        m_SpriteContext.m_MaxSpriteCount = 0;
        m_SpineModelContext.m_RenderContext = 0x0;
        m_SpineModelContext.m_MaxSpineModelCount = 0;
        m_ModelContext.m_RenderContext = 0x0;
        m_ModelContext.m_MaxModelCount = 0;
    }

    HEngine New(dmEngineService::HEngineService engine_service)
    {
        return new Engine(engine_service);
    }

    void Delete(HEngine engine)
    {
        if (engine->m_MainCollection)
            dmResource::Release(engine->m_Factory, engine->m_MainCollection);
        dmGameObject::PostUpdate(engine->m_Register);

        dmHttpClient::ShutdownConnectionPool();

        dmLiveUpdate::Finalize();

        dmGameSystem::ScriptLibContext script_lib_context;
        script_lib_context.m_Factory = engine->m_Factory;
        script_lib_context.m_Register = engine->m_Register;
        if (engine->m_SharedScriptContext) {
            script_lib_context.m_LuaState = dmScript::GetLuaState(engine->m_SharedScriptContext);
            dmGameSystem::FinalizeScriptLibs(script_lib_context);
        } else {
            script_lib_context.m_LuaState = dmScript::GetLuaState(engine->m_GOScriptContext);
            dmGameSystem::FinalizeScriptLibs(script_lib_context);
            if (engine->m_GuiContext.m_GuiContext != 0x0)
            {
                script_lib_context.m_LuaState = dmGui::GetLuaState(engine->m_GuiContext.m_GuiContext);
                dmGameSystem::FinalizeScriptLibs(script_lib_context);
            }
        }

        dmHttpClient::ReopenConnectionPool();

        dmGameObject::DeleteRegister(engine->m_Register);

        UnloadBootstrapContent(engine);

        dmSound::Finalize();

        dmInput::DeleteContext(engine->m_InputContext);

        dmRender::DeleteRenderContext(engine->m_RenderContext, engine->m_RenderScriptContext);

        if (engine->m_HidContext)
        {
            dmHID::Final(engine->m_HidContext);
            dmHID::DeleteContext(engine->m_HidContext);
        }

        if (engine->m_GuiContext.m_GuiContext)
            dmGui::DeleteContext(engine->m_GuiContext.m_GuiContext, engine->m_GuiScriptContext);

        if (engine->m_TrackingContext)
        {
            dmTracking::Finalize(engine->m_TrackingContext);
            dmTracking::Delete(engine->m_TrackingContext);
        }

        if (engine->m_SharedScriptContext) {
            dmScript::Finalize(engine->m_SharedScriptContext);
            dmScript::DeleteContext(engine->m_SharedScriptContext);
        } else {
            if (engine->m_GOScriptContext) {
                dmScript::Finalize(engine->m_GOScriptContext);
                dmScript::DeleteContext(engine->m_GOScriptContext);
            }
            if (engine->m_RenderScriptContext) {
                dmScript::Finalize(engine->m_RenderScriptContext);
                dmScript::DeleteContext(engine->m_RenderScriptContext);
            }
            if (engine->m_GuiScriptContext) {
                dmScript::Finalize(engine->m_GuiScriptContext);
                dmScript::DeleteContext(engine->m_GuiScriptContext);
            }
        }

        if (engine->m_Factory) {
            dmResource::DeleteFactory(engine->m_Factory);
        }

        if (engine->m_GraphicsContext)
        {
            dmGraphics::CloseWindow(engine->m_GraphicsContext);
            dmGraphics::DeleteContext(engine->m_GraphicsContext);
        }

        if (engine->m_SystemSocket)
            dmMessage::DeleteSocket(engine->m_SystemSocket);

        if (engine->m_PhysicsContext.m_Context3D)
        {
            if (engine->m_PhysicsContext.m_3D)
                dmPhysics::DeleteContext3D(engine->m_PhysicsContext.m_Context3D);
            else
                dmPhysics::DeleteContext2D(engine->m_PhysicsContext.m_Context2D);
        }

        dmExtension::AppParams app_params;
        app_params.m_ConfigFile = engine->m_Config;
        dmExtension::AppFinalize(&app_params);

        dmBuffer::DeleteContext();

        if (engine->m_Config)
        {
            dmConfigFile::Delete(engine->m_Config);
        }

        delete engine;
    }

    dmGraphics::TextureFilter ConvertMinTextureFilter(const char* filter)
    {
        if (strcmp(filter, "linear") == 0)
        {
            return dmGraphics::TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST;
        }
        else
        {
            return dmGraphics::TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST;
        }
    }

    dmGraphics::TextureFilter ConvertMagTextureFilter(const char* filter)
    {
        if (strcmp(filter, "linear") == 0)
        {
            return dmGraphics::TEXTURE_FILTER_LINEAR;
        }
        else
        {
            return dmGraphics::TEXTURE_FILTER_NEAREST;
        }
    }

    static bool GetProjectFile(int argc, char *argv[], char* project_file, uint32_t project_file_size)
    {
        if (argc > 1 && argv[argc-1][0] != '-')
        {
            dmStrlCpy(project_file, argv[argc-1], project_file_size);
            return true;
        }
        else
        {
            char p1[DMPATH_MAX_PATH];
            char p2[DMPATH_MAX_PATH];
            char p3[DMPATH_MAX_PATH];
            char tmp[DMPATH_MAX_PATH];
            char* paths[] = { p1, p2, p3 };
            uint32_t count = 0;

            dmStrlCpy(p1, "./game.projectc", sizeof(p1));
            dmStrlCpy(p2, "build/default/game.projectc", sizeof(p2));
            if (dmSys::GetResourcesPath(argc, argv, tmp, sizeof(tmp)) == dmSys::RESULT_OK)
            {
                dmPath::Concat(tmp, "game.projectc", p3, sizeof(p3));
                count = 3;
            }
            else
            {
                count = 2;
            }

            for (uint32_t i = 0; i < count; ++i)
            {
                if (dmSys::ResourceExists(paths[i]))
                {
                    dmStrlCpy(project_file, paths[i], project_file_size);
                    return true;
                }
            }
        }

        return false;
    }

    static void SetSwapInterval(HEngine engine, int swap_interval)
    {
        swap_interval = dmMath::Max(0, swap_interval);
#if !(defined(__arm__) || defined(__arm64__) || defined(__EMSCRIPTEN__))
        engine->m_UseSwVsync = (!engine->m_UseVariableDt && swap_interval == 0);
#endif
        dmGraphics::SetSwapInterval(engine->m_GraphicsContext, swap_interval);
    }

    static void SetUpdateFrequency(HEngine engine, uint32_t frequency)
    {
        engine->m_UpdateFrequency = frequency;
        engine->m_UpdateFrequency = dmMath::Max(1U, engine->m_UpdateFrequency);
        engine->m_UpdateFrequency = dmMath::Min(60U, engine->m_UpdateFrequency);
        int swap_interval = 60 / engine->m_UpdateFrequency;
        SetSwapInterval(engine, swap_interval);
    }

    /*
     The game.projectc is located using the following scheme:

     A.
      1. If an argument is specified load the game.project from specified file
     B.
      1. Look for game.project (relative path)
      2. Look for build/default/game.projectc (relative path)
      3. Look for dmSys::GetResourcePath()/game.project
      4. Load first game.project-file found. If none is
         found start the built-in connect application

      The content-root is set to the directory name of
      the project if not overridden in project-file
      (resource.uri)
    */
    bool Init(HEngine engine, int argc, char *argv[])
    {
        dmSys::EngineInfoParam engine_info;
        engine_info.m_Version = dmEngineVersion::VERSION;
        engine_info.m_VersionSHA1 = dmEngineVersion::VERSION_SHA1;
        engine_info.m_IsDebug = dLib::IsDebugMode();
        dmSys::SetEngineInfo(engine_info);

        char* qoe_s = getenv("DM_QUIT_ON_ESC");
        engine->m_QuitOnEsc = ((qoe_s != 0x0) && (qoe_s[0] == '1'));

        char project_file[DMPATH_MAX_PATH];
        char content_root[DMPATH_MAX_PATH] = ".";
        bool loaded_ok = false;
        if (GetProjectFile(argc, argv, project_file, sizeof(project_file)))
        {
            dmConfigFile::Result cr = dmConfigFile::Load(project_file, argc, (const char**) argv, &engine->m_Config);
            if (cr != dmConfigFile::RESULT_OK)
            {
                if (!engine->m_ConnectionAppMode)
                {
                    dmLogFatal("Unable to load project file: '%s' (%d)", project_file, cr);
                    return false;
                }
                dmLogError("Unable to load project file: '%s' (%d)", project_file, cr);
            }
            else
            {
                loaded_ok = true;
                dmPath::Dirname(project_file, content_root, sizeof(content_root));

                char tmp[DMPATH_MAX_PATH];
                dmStrlCpy(tmp, content_root, sizeof(tmp));
                if (content_root[0])
                {
                    dmStrlCat(tmp, "/game.dmanifest", sizeof(tmp));
                }
                else
                {
                    dmStrlCat(tmp, "game.dmanifest", sizeof(tmp));
                }
                if (dmSys::ResourceExists(tmp))
                {
                    dmStrlCpy(content_root, "dmanif:", sizeof(content_root));
                    dmStrlCat(content_root, tmp, sizeof(content_root));
                }
            }
        }

        if( !loaded_ok )
        {
#if defined(DM_RELEASE)
            dmLogFatal("Unable to load project");
            return false;
#else
            dmConfigFile::Result cr = dmConfigFile::LoadFromBuffer((const char*) CONNECT_PROJECT, CONNECT_PROJECT_SIZE, argc, (const char**) argv, &engine->m_Config);
            if (cr != dmConfigFile::RESULT_OK)
            {
                dmLogFatal("Unable to load builtin connect project");
                return false;
            }
            engine->m_ConnectionAppMode = true;
#endif
        }

        // Catch engine specific arguments
        bool verify_graphics_calls = dLib::IsDebugMode();
        const char verify_graphics_calls_arg[] = "--verify-graphics-calls=";
        for (int i = 0; i < argc; ++i)
        {
            const char* arg = argv[i];
            if (strncmp(verify_graphics_calls_arg, arg, sizeof(verify_graphics_calls_arg)-1) == 0)
            {
                const char* eq = strchr(arg, '=');
                if (strncmp("true", eq+1, sizeof("true")-1) == 0) {
                    verify_graphics_calls = true;
                } else if (strncmp("false", eq+1, sizeof("false")-1) == 0) {
                    verify_graphics_calls = false;
                } else {
                    dmLogWarning("Invalid value used for %s%s.", verify_graphics_calls_arg, eq);
                }
            }
        }

        dmBuffer::NewContext();

        dmExtension::AppParams app_params;
        app_params.m_ConfigFile = engine->m_Config;
        dmExtension::Result er = dmExtension::AppInitialize(&app_params);
        if (er != dmExtension::RESULT_OK) {
            dmLogFatal("Failed to initialize extensions (%d)", er);
            return false;
        }

        int write_log = dmConfigFile::GetInt(engine->m_Config, "project.write_log", 0);
        if (write_log) {
            char sys_path[DMPATH_MAX_PATH];
            if (dmSys::GetLogPath(sys_path, sizeof(sys_path)) == dmSys::RESULT_OK) {
                const char* path = dmConfigFile::GetString(engine->m_Config, "project.log_dir", sys_path);
                char full[DMPATH_MAX_PATH];
                dmPath::Concat(path, "log.txt", full, sizeof(full));
                dmSetLogFile(full);
            } else {
                dmLogFatal("Unable to get log-file path");
            }
        }

        const char* update_order = dmConfigFile::GetString(engine->m_Config, "gameobject.update_order", 0);

        // This scope is mainly here to make sure the "Main" scope is created first
        DM_PROFILE(Engine, "Init");

        dmGraphics::ContextParams graphics_context_params;
        graphics_context_params.m_DefaultTextureMinFilter = ConvertMinTextureFilter(dmConfigFile::GetString(engine->m_Config, "graphics.default_texture_min_filter", "linear"));
        graphics_context_params.m_DefaultTextureMagFilter = ConvertMagTextureFilter(dmConfigFile::GetString(engine->m_Config, "graphics.default_texture_mag_filter", "linear"));
        graphics_context_params.m_VerifyGraphicsCalls = verify_graphics_calls;

        engine->m_GraphicsContext = dmGraphics::NewContext(graphics_context_params);
        if (engine->m_GraphicsContext == 0x0)
        {
            dmLogFatal("Unable to create the graphics context.");
            return false;
        }

        engine->m_Width = dmConfigFile::GetInt(engine->m_Config, "display.width", 960);
        engine->m_Height = dmConfigFile::GetInt(engine->m_Config, "display.height", 640);

        dmGraphics::WindowParams window_params;
        window_params.m_ResizeCallback = OnWindowResize;
        window_params.m_ResizeCallbackUserData = engine;
        window_params.m_CloseCallback = OnWindowClose;
        window_params.m_CloseCallbackUserData = engine;
        window_params.m_FocusCallback = OnWindowFocus;
        window_params.m_FocusCallbackUserData = engine;
        window_params.m_Width = engine->m_Width;
        window_params.m_Height = engine->m_Height;
        window_params.m_Samples = dmConfigFile::GetInt(engine->m_Config, "display.samples", 0);
        window_params.m_Title = dmConfigFile::GetString(engine->m_Config, "project.title", "TestTitle");
        window_params.m_Fullscreen = (bool) dmConfigFile::GetInt(engine->m_Config, "display.fullscreen", 0);
        window_params.m_PrintDeviceInfo = false;
        window_params.m_HighDPI = (bool) dmConfigFile::GetInt(engine->m_Config, "display.high_dpi", 0);

        dmGraphics::WindowResult window_result = dmGraphics::OpenWindow(engine->m_GraphicsContext, &window_params);
        if (window_result != dmGraphics::WINDOW_RESULT_OK)
        {
            dmLogFatal("Could not open window (%d).", window_result);
            return false;
        }

        uint32_t physical_dpi = dmGraphics::GetDisplayDpi(engine->m_GraphicsContext);
        uint32_t physical_width = dmGraphics::GetWindowWidth(engine->m_GraphicsContext);
        uint32_t physical_height = dmGraphics::GetWindowHeight(engine->m_GraphicsContext);
        engine->m_InvPhysicalWidth = 1.0f / physical_width;
        engine->m_InvPhysicalHeight = 1.0f / physical_height;

        engine->m_UseVariableDt = dmConfigFile::GetInt(engine->m_Config, "display.variable_dt", 0) != 0;
        engine->m_PreviousFrameTime = dmTime::GetTime();
        engine->m_FlipTime = dmTime::GetTime();
        engine->m_PreviousRenderTime = 0;
        engine->m_UseSwVsync = false;
        SetUpdateFrequency(engine, dmConfigFile::GetInt(engine->m_Config, "display.update_frequency", 60));

        const uint32_t max_resources = dmConfigFile::GetInt(engine->m_Config, dmResource::MAX_RESOURCES_KEY, 1024);
        dmResource::NewFactoryParams params;
        int32_t http_cache = dmConfigFile::GetInt(engine->m_Config, "resource.http_cache", 1);
        params.m_MaxResources = max_resources;
        params.m_Flags = 0;
        if (dLib::IsDebugMode())
        {
            params.m_Flags = RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT;
            if (http_cache)
                params.m_Flags |= RESOURCE_FACTORY_FLAGS_HTTP_CACHE;
        }

        params.m_ArchiveIndex.m_Data = (const void*) BUILTINS_ARCI;
        params.m_ArchiveIndex.m_Size = BUILTINS_ARCI_SIZE;
        params.m_ArchiveData.m_Data = (const void*) BUILTINS_ARCD;
        params.m_ArchiveData.m_Size = BUILTINS_ARCD_SIZE;
        params.m_ArchiveManifest.m_Data = (const void*) BUILTINS_DMANIFEST;
        params.m_ArchiveManifest.m_Size = BUILTINS_DMANIFEST_SIZE;

        const char* resource_uri = dmConfigFile::GetString(engine->m_Config, "resource.uri", content_root);
        dmLogInfo("Loading data from: %s", resource_uri);
        engine->m_Factory = dmResource::NewFactory(&params, resource_uri);
        if (!engine->m_Factory)
        {
            return false;
        }

        dmScript::ClearLuaRefCount(); // Reset the debug counter to 0

        dmArray<dmScript::HContext>& module_script_contexts = engine->m_ModuleContext.m_ScriptContexts;

        bool shared = dmConfigFile::GetInt(engine->m_Config, "script.shared_state", 0);
        if (shared) {
            engine->m_SharedScriptContext = dmScript::NewContext(engine->m_Config, engine->m_Factory, true);
            dmScript::Initialize(engine->m_SharedScriptContext);
            engine->m_GOScriptContext = engine->m_SharedScriptContext;
            engine->m_RenderScriptContext = engine->m_SharedScriptContext;
            engine->m_GuiScriptContext = engine->m_SharedScriptContext;
            module_script_contexts.SetCapacity(1);
            module_script_contexts.Push(engine->m_SharedScriptContext);
        } else {
            engine->m_GOScriptContext = dmScript::NewContext(engine->m_Config, engine->m_Factory, true);
            dmScript::Initialize(engine->m_GOScriptContext);
            engine->m_RenderScriptContext = dmScript::NewContext(engine->m_Config, engine->m_Factory, true);
            dmScript::Initialize(engine->m_RenderScriptContext);
            engine->m_GuiScriptContext = dmScript::NewContext(engine->m_Config, engine->m_Factory, true);
            dmScript::Initialize(engine->m_GuiScriptContext);
            module_script_contexts.SetCapacity(3);
            module_script_contexts.Push(engine->m_GOScriptContext);
            module_script_contexts.Push(engine->m_RenderScriptContext);
            module_script_contexts.Push(engine->m_GuiScriptContext);
        }

        dmHID::NewContextParams new_hid_params = dmHID::NewContextParams();

        // Accelerometer
        int32_t use_accelerometer = dmConfigFile::GetInt(engine->m_Config, "input.use_accelerometer", 1);
        if (use_accelerometer) {
        	dmHID::EnableAccelerometer(); // Creates and enables the accelerometer
        }
        new_hid_params.m_IgnoreAcceleration = use_accelerometer ? 0 : 1;

#if defined(__EMSCRIPTEN__)
        // DEF-2450 Reverse scroll direction for firefox browser
        dmSys::SystemInfo info;
        dmSys::GetSystemInfo(&info);
        if (info.m_UserAgent != 0x0)
        {
            const char* str_firefox = "firefox";
            new_hid_params.m_FlipScrollDirection = (strcasestr(info.m_UserAgent, str_firefox) != NULL) ? 1 : 0;
        }
#endif
        engine->m_HidContext = dmHID::NewContext(new_hid_params);
        dmHID::Init(engine->m_HidContext);

        // The attempt to fallback to other audio devices only has meaning if:
        // - sound is being used
        // - the matching device symbols have been exported for the target device
        dmSound::InitializeParams sound_params;
        static const char* audio_devices[] = {
                "default",
                "null",
                NULL
        };
        int deviceIndex = 0;
        while (NULL != audio_devices[deviceIndex]) {
            sound_params.m_OutputDevice = audio_devices[deviceIndex];
            dmSound::Result soundInit = dmSound::Initialize(engine->m_Config, &sound_params);
            if (dmSound::RESULT_OK == soundInit) {
                dmLogInfo("Initialised sound device '%s'\n", sound_params.m_OutputDevice);
                break;
            }
            ++deviceIndex;
        }

        dmGameObject::Result go_result = dmGameObject::SetCollectionDefaultCapacity(engine->m_Register, dmConfigFile::GetInt(engine->m_Config, dmGameObject::COLLECTION_MAX_INSTANCES_KEY, dmGameObject::DEFAULT_MAX_COLLECTION_CAPACITY));
        if(go_result != dmGameObject::RESULT_OK)
        {
            dmLogFatal("Failed to set max instance count for collections (%d)", go_result);
            return false;
        }

        go_result = dmGameObject::SetCollectionDefaultRigCapacity(engine->m_Register, dmConfigFile::GetInt(engine->m_Config, dmRig::RIG_MAX_INSTANCES_KEY, dmRig::DEFAULT_MAX_RIG_CAPACITY));
        if(go_result != dmGameObject::RESULT_OK)
        {
            dmLogFatal("Failed to set max rig instance count for collections (%d)", go_result);
            return false;
        }

        dmRender::RenderContextParams render_params;
        render_params.m_MaxRenderTypes = 16;
        render_params.m_MaxInstances = (uint32_t) dmConfigFile::GetInt(engine->m_Config, "graphics.max_draw_calls", 1024);
        render_params.m_MaxRenderTargets = 32;
        render_params.m_VertexProgramData = ::DEBUG_VPC;
        render_params.m_VertexProgramDataSize = ::DEBUG_VPC_SIZE;
        render_params.m_FragmentProgramData = ::DEBUG_FPC;
        render_params.m_FragmentProgramDataSize = ::DEBUG_FPC_SIZE;
        render_params.m_MaxCharacters = (uint32_t) dmConfigFile::GetInt(engine->m_Config, "graphics.max_characters", 2048 * 4);;
        render_params.m_CommandBufferSize = 1024;
        render_params.m_ScriptContext = engine->m_RenderScriptContext;
        render_params.m_MaxDebugVertexCount = (uint32_t) dmConfigFile::GetInt(engine->m_Config, "graphics.max_debug_vertices", 10000);
        render_params.m_Factory = engine->m_Factory;
        engine->m_RenderContext = dmRender::NewRenderContext(engine->m_GraphicsContext, render_params);

        dmGameObject::Initialize(engine->m_GOScriptContext);

        engine->m_ParticleFXContext.m_Factory = engine->m_Factory;
        engine->m_ParticleFXContext.m_RenderContext = engine->m_RenderContext;
        engine->m_ParticleFXContext.m_MaxParticleFXCount = dmConfigFile::GetInt(engine->m_Config, dmParticle::MAX_INSTANCE_COUNT_KEY, 64);
        engine->m_ParticleFXContext.m_MaxParticleCount = dmConfigFile::GetInt(engine->m_Config, dmParticle::MAX_PARTICLE_COUNT_KEY, 1024);
        engine->m_ParticleFXContext.m_Debug = false;

        dmInput::NewContextParams input_params;
        input_params.m_HidContext = engine->m_HidContext;
        input_params.m_RepeatDelay = dmConfigFile::GetFloat(engine->m_Config, "input.repeat_delay", 0.5f);
        input_params.m_RepeatInterval = dmConfigFile::GetFloat(engine->m_Config, "input.repeat_interval", 0.2f);
        engine->m_InputContext = dmInput::NewContext(input_params);

        dmMessage::Result mr = dmMessage::NewSocket(SYSTEM_SOCKET_NAME, &engine->m_SystemSocket);
        if (mr != dmMessage::RESULT_OK)
        {
            dmLogFatal("Unable to create system socket: %s (%d)", SYSTEM_SOCKET_NAME, mr);
            return false;
        }

        dmGui::NewContextParams gui_params;
        gui_params.m_ScriptContext = engine->m_GuiScriptContext;
        gui_params.m_GetURLCallback = dmGameSystem::GuiGetURLCallback;
        gui_params.m_GetUserDataCallback = dmGameSystem::GuiGetUserDataCallback;
        gui_params.m_ResolvePathCallback = dmGameSystem::GuiResolvePathCallback;
        gui_params.m_GetTextMetricsCallback = dmGameSystem::GuiGetTextMetricsCallback;
        gui_params.m_PhysicalWidth = physical_width;
        gui_params.m_PhysicalHeight = physical_height;
        gui_params.m_DefaultProjectWidth = engine->m_Width;
        gui_params.m_DefaultProjectHeight = engine->m_Height;
        gui_params.m_Dpi = physical_dpi;
        gui_params.m_HidContext = engine->m_HidContext;
        engine->m_GuiContext.m_GuiContext = dmGui::NewContext(&gui_params);
        engine->m_GuiContext.m_RenderContext = engine->m_RenderContext;
        engine->m_GuiContext.m_ScriptContext = engine->m_GuiScriptContext;
        engine->m_GuiContext.m_MaxGuiComponents = dmConfigFile::GetInt(engine->m_Config, "gui.max_count", 64);
        engine->m_GuiContext.m_MaxParticleFXCount = dmConfigFile::GetInt(engine->m_Config, "gui.max_particlefx_count", 64);
        engine->m_GuiContext.m_MaxParticleCount = dmConfigFile::GetInt(engine->m_Config, "gui.max_particle_count", 1024);

        dmPhysics::NewContextParams physics_params;
        physics_params.m_WorldCount = dmConfigFile::GetInt(engine->m_Config, "physics.world_count", 4);
        const char* physics_type = dmConfigFile::GetString(engine->m_Config, "physics.type", "2D");
        physics_params.m_Gravity.setX(dmConfigFile::GetFloat(engine->m_Config, "physics.gravity_x", 0.0f));
        physics_params.m_Gravity.setY(dmConfigFile::GetFloat(engine->m_Config, "physics.gravity_y", -10.0f));
        physics_params.m_Gravity.setZ(dmConfigFile::GetFloat(engine->m_Config, "physics.gravity_z", 0.0f));
        physics_params.m_Scale = dmConfigFile::GetFloat(engine->m_Config, "physics.scale", 1.0f);
        physics_params.m_RayCastLimit2D = dmConfigFile::GetInt(engine->m_Config, "physics.ray_cast_limit_2d", 64);
        physics_params.m_RayCastLimit3D = dmConfigFile::GetInt(engine->m_Config, "physics.ray_cast_limit_3d", 128);
        physics_params.m_TriggerOverlapCapacity = dmConfigFile::GetInt(engine->m_Config, "physics.trigger_overlap_capacity", 16);
        if (physics_params.m_Scale < dmPhysics::MIN_SCALE || physics_params.m_Scale > dmPhysics::MAX_SCALE)
        {
            dmLogWarning("Physics scale must be in the range %.2f - %.2f and has been clamped.", dmPhysics::MIN_SCALE, dmPhysics::MAX_SCALE);
            if (physics_params.m_Scale < dmPhysics::MIN_SCALE)
                physics_params.m_Scale = dmPhysics::MIN_SCALE;
            if (physics_params.m_Scale > dmPhysics::MAX_SCALE)
                physics_params.m_Scale = dmPhysics::MAX_SCALE;
        }
        physics_params.m_ContactImpulseLimit = dmConfigFile::GetFloat(engine->m_Config, "physics.contact_impulse_limit", 0.0f);
        if (dmStrCaseCmp(physics_type, "3D") == 0)
        {
            engine->m_PhysicsContext.m_3D = true;
            engine->m_PhysicsContext.m_Context3D = dmPhysics::NewContext3D(physics_params);
        }
        else if (dmStrCaseCmp(physics_type, "2D") == 0)
        {
            engine->m_PhysicsContext.m_3D = false;
            engine->m_PhysicsContext.m_Context2D = dmPhysics::NewContext2D(physics_params);
        }
        else
        {
            dmLogWarning("Unsupported physics type '%s'. Defaults to 2D", physics_type);
            engine->m_PhysicsContext.m_3D = false;
            engine->m_PhysicsContext.m_Context2D = dmPhysics::NewContext2D(physics_params);
        }
        engine->m_PhysicsContext.m_MaxCollisionCount = dmConfigFile::GetInt(engine->m_Config, dmGameSystem::PHYSICS_MAX_COLLISIONS_KEY, 64);
        engine->m_PhysicsContext.m_MaxContactPointCount = dmConfigFile::GetInt(engine->m_Config, dmGameSystem::PHYSICS_MAX_CONTACTS_KEY, 128);
        engine->m_PhysicsContext.m_Debug = (bool) dmConfigFile::GetInt(engine->m_Config, "physics.debug", 0);

#if !defined(DM_RELEASE)
        dmPhysics::DebugCallbacks debug_callbacks;
        debug_callbacks.m_UserData = engine->m_RenderContext;
        debug_callbacks.m_DrawLines = PhysicsDebugRender::DrawLines;
        debug_callbacks.m_DrawTriangles = PhysicsDebugRender::DrawTriangles;
        debug_callbacks.m_Alpha = dmConfigFile::GetFloat(engine->m_Config, "physics.debug_alpha", 0.9f);
        debug_callbacks.m_Scale = physics_params.m_Scale;
        debug_callbacks.m_InvScale = 1.0f / physics_params.m_Scale;
        debug_callbacks.m_DebugScale = dmConfigFile::GetFloat(engine->m_Config, "physics.debug_scale", 30.0f);
        if (engine->m_PhysicsContext.m_3D)
            dmPhysics::SetDebugCallbacks3D(engine->m_PhysicsContext.m_Context3D, debug_callbacks);
        else
            dmPhysics::SetDebugCallbacks2D(engine->m_PhysicsContext.m_Context2D, debug_callbacks);
#endif

        engine->m_SpriteContext.m_RenderContext = engine->m_RenderContext;
        engine->m_SpriteContext.m_MaxSpriteCount = dmConfigFile::GetInt(engine->m_Config, "sprite.max_count", 128);
        engine->m_SpriteContext.m_Subpixels = dmConfigFile::GetInt(engine->m_Config, "sprite.subpixels", 1);

        engine->m_ModelContext.m_RenderContext = engine->m_RenderContext;
        engine->m_ModelContext.m_Factory = engine->m_Factory;
        engine->m_ModelContext.m_MaxModelCount = dmConfigFile::GetInt(engine->m_Config, "model.max_count", 128);

        engine->m_SpineModelContext.m_RenderContext = engine->m_RenderContext;
        engine->m_SpineModelContext.m_Factory = engine->m_Factory;
        engine->m_SpineModelContext.m_MaxSpineModelCount = dmConfigFile::GetInt(engine->m_Config, "spine.max_count", 128);

        engine->m_LabelContext.m_RenderContext      = engine->m_RenderContext;
        engine->m_LabelContext.m_MaxLabelCount      = dmConfigFile::GetInt(engine->m_Config, "label.max_count", 64);
        engine->m_LabelContext.m_Subpixels          = dmConfigFile::GetInt(engine->m_Config, "label.subpixels", 1);

        engine->m_CollectionProxyContext.m_Factory = engine->m_Factory;
        engine->m_CollectionProxyContext.m_MaxCollectionProxyCount = dmConfigFile::GetInt(engine->m_Config, dmGameSystem::COLLECTION_PROXY_MAX_COUNT_KEY, 8);

        engine->m_FactoryContext.m_MaxFactoryCount = dmConfigFile::GetInt(engine->m_Config, dmGameSystem::FACTORY_MAX_COUNT_KEY, 128);
        engine->m_CollectionFactoryContext.m_MaxCollectionFactoryCount = dmConfigFile::GetInt(engine->m_Config, dmGameSystem::COLLECTION_FACTORY_MAX_COUNT_KEY, 128);
        if (shared)
        {
            engine->m_FactoryContext.m_ScriptContext = engine->m_SharedScriptContext;
            engine->m_CollectionFactoryContext.m_ScriptContext = engine->m_SharedScriptContext;
        }
        else
        {
            engine->m_FactoryContext.m_ScriptContext = engine->m_GOScriptContext;
            engine->m_CollectionFactoryContext.m_ScriptContext = engine->m_GOScriptContext;
        }

        dmResource::Result fact_result;
        dmGameSystem::ScriptLibContext script_lib_context;

        fact_result = dmGameObject::RegisterResourceTypes(engine->m_Factory, engine->m_Register, engine->m_GOScriptContext, &engine->m_ModuleContext);
        if (fact_result != dmResource::RESULT_OK)
            goto bail;
        fact_result = dmGameSystem::RegisterResourceTypes(engine->m_Factory, engine->m_RenderContext, &engine->m_GuiContext, engine->m_InputContext, &engine->m_PhysicsContext);
        if (fact_result != dmResource::RESULT_OK)
            goto bail;

        if (dmGameObject::RegisterComponentTypes(engine->m_Factory, engine->m_Register, engine->m_GOScriptContext) != dmGameObject::RESULT_OK)
            goto bail;

        go_result = dmGameSystem::RegisterComponentTypes(engine->m_Factory, engine->m_Register, engine->m_RenderContext, &engine->m_PhysicsContext, &engine->m_ParticleFXContext, &engine->m_GuiContext, &engine->m_SpriteContext,
                                                                                                &engine->m_CollectionProxyContext, &engine->m_FactoryContext, &engine->m_CollectionFactoryContext, &engine->m_SpineModelContext,
                                                                                                &engine->m_ModelContext, &engine->m_LabelContext);
        if (go_result != dmGameObject::RESULT_OK)
            goto bail;

        if (!LoadBootstrapContent(engine, engine->m_Config))
        {
            dmLogWarning("Unable to load bootstrap data.");
            goto bail;
        }

#if !defined(DM_RELEASE)
        {
            const char* init_script = dmConfigFile::GetString(engine->m_Config, "bootstrap.debug_init_script", 0);
            if (init_script) {
                char* tmp = strdup(init_script);
                char* iter = 0;
                char* filename = dmStrTok(tmp, ",", &iter);
                do
                {
                    // We need the size, in order to send it as a proper LuaModule message
                    void* data;
                    uint32_t datasize;
                    dmResource::Result r = dmResource::GetRaw(engine->m_Factory, filename, (void**)&data, &datasize);
                    if (r != dmResource::RESULT_OK) {
                        dmLogWarning("Failed to load script: %s (%d)", filename, r);
                        free(tmp);
                        return false;
                    }


                    dmLuaDDF::LuaModule* lua_module = 0;
                    dmDDF::Result e = dmDDF::LoadMessage<dmLuaDDF::LuaModule>(data, datasize, &lua_module);
                    if ( e != dmDDF::RESULT_OK ) {
                        free(tmp);
                        free(data);
                        dmLogWarning("Failed to load LuaModule message from: %s (%d)", filename, r);
                        return dmResource::RESULT_FORMAT_ERROR;
                    }

                    // Due to the fact that the same message can be loaded in two different ways, we have two separate call sites
                    // Here, we have an already resolved filename string.
                    if (engine->m_SharedScriptContext) {
                        dmGameObject::LuaLoad(engine->m_Factory, engine->m_SharedScriptContext, lua_module);
                    }
                    else {
                        dmGameObject::LuaLoad(engine->m_Factory, engine->m_GOScriptContext, lua_module);
                        dmGameObject::LuaLoad(engine->m_Factory, engine->m_GuiScriptContext, lua_module);
                        dmGameObject::LuaLoad(engine->m_Factory, engine->m_RenderScriptContext, lua_module);
                    }

                    dmDDF::FreeMessage(lua_module);
                    free(data);

                } while( (filename = dmStrTok(0, ",", &iter)) );
                free(tmp);
            }
        }
#endif

        dmGui::SetDefaultFont(engine->m_GuiContext.m_GuiContext, engine->m_SystemFontMap);
        dmGui::SetDisplayProfiles(engine->m_GuiContext.m_GuiContext, engine->m_DisplayProfiles);

        if (engine->m_RenderScriptPrototype) {
            dmRender::RenderScriptResult script_result = InitRenderScriptInstance(engine->m_RenderScriptPrototype->m_Instance);
            if (script_result != dmRender::RENDER_SCRIPT_RESULT_OK) {
                dmLogFatal("Render script could not be initialized.");
                goto bail;
            }
        }

        script_lib_context.m_Factory = engine->m_Factory;
        script_lib_context.m_Register = engine->m_Register;
        if (engine->m_SharedScriptContext) {
            script_lib_context.m_LuaState = dmScript::GetLuaState(engine->m_SharedScriptContext);
            if (!dmGameSystem::InitializeScriptLibs(script_lib_context))
                goto bail;
        } else {
            script_lib_context.m_LuaState = dmScript::GetLuaState(engine->m_GOScriptContext);
            if (!dmGameSystem::InitializeScriptLibs(script_lib_context))
                goto bail;
            script_lib_context.m_LuaState = dmGui::GetLuaState(engine->m_GuiContext.m_GuiContext);
            if (!dmGameSystem::InitializeScriptLibs(script_lib_context))
                goto bail;
        }

        dmLiveUpdate::Initialize(engine->m_Factory);

        engine->m_TrackingContext = dmTracking::New(engine->m_Config);
        if (engine->m_TrackingContext)
        {
            dmTracking::Start(engine->m_TrackingContext, "Defold", dmEngineVersion::VERSION);
        }
        else
        {
            dmLogWarning("Failed to create tracking context");
        }

        fact_result = dmResource::Get(engine->m_Factory, dmConfigFile::GetString(engine->m_Config, "bootstrap.main_collection", "/logic/main.collectionc"), (void**) &engine->m_MainCollection);
        if (fact_result != dmResource::RESULT_OK)
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
                dmResource::ResourceType type;
                fact_result = dmResource::GetTypeFromExtension(engine->m_Factory, s, &type);
                if (fact_result == dmResource::RESULT_OK)
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
        dmGameObject::SortComponentTypes(engine->m_Register);

#if defined(__ANDROID__)
        {
            const char* input_method = dmConfigFile::GetString(engine->m_Config, "android.input_method", "KeyEvents");

            int use_hidden_inputfield = 0;
            if (!strcmp(input_method, "HiddenInputField"))
                use_hidden_inputfield = 1;
            else if (strcmp(input_method, "KeyEvents"))
                dmLogWarning("Unknown Android input method [%s], defaulting to key events", input_method);

            _glfwAndroidSetInputMethod(use_hidden_inputfield);
        }
        {
            int immersive_mode = dmConfigFile::GetInt(engine->m_Config, "android.immersive_mode", 0);
            _glfwAndroidSetImmersiveMode(immersive_mode);
        }
#endif

        return true;

bail:
        return false;
    }

    void GOActionCallback(dmhash_t action_id, dmInput::Action* action, void* user_data)
    {
        Engine* engine = (Engine*)user_data;
        int32_t window_height = dmGraphics::GetWindowHeight(engine->m_GraphicsContext);
        dmArray<dmGameObject::InputAction>* input_buffer = &engine->m_InputBuffer;
        dmGameObject::InputAction input_action;
        input_action.m_ActionId = action_id;
        input_action.m_Value = action->m_Value;
        input_action.m_Pressed = action->m_Pressed;
        input_action.m_Released = action->m_Released;
        input_action.m_Repeated = action->m_Repeated;
        input_action.m_PositionSet = action->m_PositionSet;
        input_action.m_AccelerationSet = action->m_AccelerationSet;
        float width_ratio = engine->m_InvPhysicalWidth * engine->m_Width;
        float height_ratio = engine->m_InvPhysicalHeight * engine->m_Height;
        input_action.m_X = (action->m_X + 0.5f) * width_ratio;
        input_action.m_Y = engine->m_Height - (action->m_Y + 0.5f) * height_ratio;
        input_action.m_DX = action->m_DX * width_ratio;
        input_action.m_DY = -action->m_DY * height_ratio;
        input_action.m_ScreenX = action->m_X;
        input_action.m_ScreenY = window_height - action->m_Y;
        input_action.m_ScreenDX = action->m_DX;
        input_action.m_ScreenDY = -action->m_DY;
        input_action.m_AccX = action->m_AccX;
        input_action.m_AccY = action->m_AccY;
        input_action.m_AccZ = action->m_AccZ;

        input_action.m_TouchCount = action->m_TouchCount;
        int tc = action->m_TouchCount;
        for (int i = 0; i < tc; ++i) {
            dmHID::Touch& a = action->m_Touch[i];
            dmHID::Touch& ia = input_action.m_Touch[i];
            ia = action->m_Touch[i];
            ia.m_Id = a.m_Id;
            ia.m_X = (a.m_X + 0.5f) * width_ratio;
            ia.m_Y = engine->m_Height - (a.m_Y + 0.5f) * height_ratio;
            ia.m_DX = a.m_DX * width_ratio;
            ia.m_DY = -a.m_DY * height_ratio;
            ia.m_ScreenX = a.m_X;
            ia.m_ScreenY = window_height - a.m_Y;
            ia.m_ScreenDX = a.m_DX;
            ia.m_ScreenDY = -a.m_DY;
        }

        input_action.m_TextCount = action->m_TextCount;
        input_action.m_HasText = action->m_HasText;
        tc = action->m_TextCount;
        for (int i = 0; i < tc; ++i) {
            input_action.m_Text[i] = action->m_Text[i];
        }

        input_action.m_IsGamepad = action->m_IsGamepad;
        input_action.m_GamepadIndex = action->m_GamepadIndex;

        input_buffer->Push(input_action);
    }

    uint16_t GetHttpPort(HEngine engine)
    {

#if !defined(DM_RELEASE)
        if (engine->m_EngineService)
        {
            return dmEngineService::GetPort(engine->m_EngineService);
        }
#endif
        return 0;
    }

    static int InputBufferOrderSort(const void * a, const void * b)
    {
        dmGameObject::InputAction *ipa = (dmGameObject::InputAction *)a;
        dmGameObject::InputAction *ipb = (dmGameObject::InputAction *)b;
        bool a_is_text = ipa->m_HasText || ipa->m_TextCount > 0;
        bool b_is_text = ipb->m_HasText || ipb->m_TextCount > 0;
        return a_is_text - b_is_text;
    }

    static uint32_t GetLuaMemCount(HEngine engine)
    {
        uint32_t memcount = 0;
        if (engine->m_SharedScriptContext) {
            memcount += dmScript::GetLuaGCCount(dmScript::GetLuaState(engine->m_SharedScriptContext));
        } else {
            memcount += dmScript::GetLuaGCCount(dmScript::GetLuaState(engine->m_GOScriptContext));
            if (engine->m_GuiContext.m_GuiContext != 0x0)
            {
                memcount += dmScript::GetLuaGCCount(dmGui::GetLuaState(engine->m_GuiContext.m_GuiContext));
            }
        }
        return memcount;
    }

    void Step(HEngine engine)
    {
        engine->m_Alive = true;
        engine->m_RunResult.m_ExitCode = 0;

        // uint64_t target_frametime = (uint64_t)((1.f / engine->m_UpdateFrequency) * 1000000.0);
        uint64_t target_frametime = 1000000 / engine->m_UpdateFrequency;
        uint64_t time = dmTime::GetTime();
        uint64_t prev_flip_time = engine->m_FlipTime;

        float fps = engine->m_UpdateFrequency;
        float fixed_dt = 1.0f / fps;
        float dt = fixed_dt;
        bool variable_dt = engine->m_UseVariableDt;
        if (variable_dt && time > engine->m_PreviousFrameTime) {
            dt = (float)((time - engine->m_PreviousFrameTime) * 0.000001);
            // safety mechanism for crazy; GetTime() is not guaranteed to always
            // produce small deltas between calls, cap to 25 frames in one.
            const float max = fixed_dt * 25.0f;
            if (dt > max) {
                dt = max;
            }
        }
        engine->m_PreviousFrameTime = time;

        if (engine->m_Alive)
        {
            if (engine->m_TrackingContext)
            {
                DM_PROFILE(Engine, "Tracking")
                dmTracking::Update(engine->m_TrackingContext, dt);
            }

            if (dmGraphics::GetWindowState(engine->m_GraphicsContext, dmGraphics::WINDOW_STATE_ICONIFIED))
            {
                // NOTE: Polling the event queue is crucial on iOS for life-cycle management
                // NOTE: Also running graphics on iOS while transitioning is not permitted and will crash the application
                dmHID::Update(engine->m_HidContext);
                dmTime::Sleep(1000 * 100);
                // Update time again after the sleep to avoid big leaps after iconified.
                // In practice, it makes the delta time 1/freq even though we slept for long

                time = dmTime::GetTime();
                uint64_t i_dt = fixed_dt * 1000000;
                if (i_dt > time) {
                    engine->m_PreviousFrameTime = 0;
                } else {
                    engine->m_PreviousFrameTime = time - i_dt;
                }

                engine->m_WasIconified = true;
                return;
            }
            else
            {
                if (engine->m_WasIconified)
                {
                    if (engine->m_TrackingContext)
                    {
                        dmTracking::PostSimpleEvent(engine->m_TrackingContext, "@Invoke");
                    }
                    engine->m_WasIconified = false;
                }
            }

            dmProfile::HProfile profile = dmProfile::Begin();
            {
                DM_PROFILE(Engine, "Frame");


#if !defined(DM_RELEASE)
                if (engine->m_EngineService)
                {
                    dmEngineService::Update(engine->m_EngineService);
                }
#endif

                {
                    DM_PROFILE(Engine, "Sim");

                    dmLiveUpdate::Update();
                    dmResource::UpdateFactory(engine->m_Factory);

                    dmHID::Update(engine->m_HidContext);
                    if (dmGraphics::GetWindowState(engine->m_GraphicsContext, dmGraphics::WINDOW_STATE_ICONIFIED))
                    {
                        // NOTE: This is a bit ugly but os event are polled in dmHID::Update and an iOS application
                        // might have entered background at this point and OpenGL calls are not permitted and will
                        // crash the application
                        dmProfile::Release(profile);
                        return;
                    }


                    /* Extension updates */
                    if (engine->m_SharedScriptContext) {
                        dmScript::UpdateExtensions(engine->m_SharedScriptContext);
                    } else {
                        if (engine->m_GOScriptContext) {
                            dmScript::UpdateExtensions(engine->m_GOScriptContext);
                        }
                        if (engine->m_RenderScriptContext) {
                            dmScript::UpdateExtensions(engine->m_RenderScriptContext);
                        }
                        if (engine->m_GuiScriptContext) {
                            dmScript::UpdateExtensions(engine->m_GuiScriptContext);
                        }
                    }

                    dmSound::Update();

                    dmHID::KeyboardPacket keybdata;
                    dmHID::GetKeyboardPacket(engine->m_HidContext, &keybdata);

                    if ((engine->m_QuitOnEsc && dmHID::GetKey(&keybdata, dmHID::KEY_ESC)) || !dmGraphics::GetWindowState(engine->m_GraphicsContext, dmGraphics::WINDOW_STATE_OPENED))
                    {
                        engine->m_Alive = false;
                        return;
                    }

                    dmInput::UpdateBinding(engine->m_GameInputBinding, dt);

                    engine->m_InputBuffer.SetSize(0);
                    dmInput::ForEachActive(engine->m_GameInputBinding, GOActionCallback, engine);

                    // Sort input so that text and marked text is triggered last
                    // NOTE: Due to Korean keyboards on iOS will send a backspace sometimes to "replace" a character with a new one,
                    //       we want to make sure these keypresses arrive to the input listeners before the "new" character.
                    //       If the backspace arrive after the text, it will instead remove the new character that
                    //       actually should replace the old one.
                    qsort(engine->m_InputBuffer.Begin(), engine->m_InputBuffer.Size(), sizeof(dmGameObject::InputAction), InputBufferOrderSort);

                    dmArray<dmGameObject::InputAction>& input_buffer = engine->m_InputBuffer;
                    uint32_t input_buffer_size = input_buffer.Size();
                    if (input_buffer_size > 0)
                    {
                        dmGameObject::DispatchInput(engine->m_MainCollection, &input_buffer[0], input_buffer.Size());
                    }


                    dmGameObject::UpdateContext update_context;
                    update_context.m_DT = dt;
                    dmGameObject::Update(engine->m_MainCollection, &update_context);

                    // Make the render list that will be used later.
                    dmRender::RenderListBegin(engine->m_RenderContext);
                    dmGameObject::Render(engine->m_MainCollection);

                    // Make sure we dispatch messages to the render script
                    // since it could have some "draw_text" messages waiting.
                    if (engine->m_RenderScriptPrototype)
                    {
                        dmRender::DispatchRenderScriptInstance(engine->m_RenderScriptPrototype->m_Instance);
                    }

                    dmRender::RenderListEnd(engine->m_RenderContext);

                    if (engine->m_RenderScriptPrototype)
                    {
                        dmRender::UpdateRenderScriptInstance(engine->m_RenderScriptPrototype->m_Instance);
                    }
                    else
                    {
                        dmGraphics::SetViewport(engine->m_GraphicsContext, 0, 0, dmGraphics::GetWindowWidth(engine->m_GraphicsContext), dmGraphics::GetWindowHeight(engine->m_GraphicsContext));
                        dmGraphics::Clear(engine->m_GraphicsContext, dmGraphics::BUFFER_TYPE_COLOR_BIT | dmGraphics::BUFFER_TYPE_DEPTH_BIT | dmGraphics::BUFFER_TYPE_STENCIL_BIT, 0, 0, 0, 0, 1.0, 0);
                        dmRender::DrawRenderList(engine->m_RenderContext, 0x0, 0x0);
                    }

                    dmGameObject::PostUpdate(engine->m_MainCollection);
                    dmGameObject::PostUpdate(engine->m_Register);

                    dmRender::ClearRenderObjects(engine->m_RenderContext);


                    dmMessage::Dispatch(engine->m_SystemSocket, Dispatch, engine);
                }

                DM_COUNTER("Lua.Refs", dmScript::GetLuaRefCount());
                DM_COUNTER("Lua.Mem", GetLuaMemCount(engine));

                if (dLib::IsDebugMode())
                {
                    // We had buffering problems with the output when running the engine inside the editor
                    // Flushing stdout/stderr solves this problem.
                    fflush(stdout);
                    fflush(stderr);
                }

#if !defined(DM_RELEASE)
                if(engine->m_ShowProfile)
                {
                    DM_PROFILE(Profile, "Draw");
                    dmProfile::Pause(true);

                    dmRender::RenderListBegin(engine->m_RenderContext);
                    dmProfileRender::Draw(profile, engine->m_RenderContext, engine->m_SystemFontMap);
                    dmRender::RenderListEnd(engine->m_RenderContext);
                    dmRender::SetViewMatrix(engine->m_RenderContext, Matrix4::identity());
                    dmRender::SetProjectionMatrix(engine->m_RenderContext, Matrix4::orthographic(0.0f, dmGraphics::GetWindowWidth(engine->m_GraphicsContext), 0.0f, dmGraphics::GetWindowHeight(engine->m_GraphicsContext), 1.0f, -1.0f));
                    dmRender::DrawRenderList(engine->m_RenderContext, 0x0, 0x0);
                    dmRender::ClearRenderObjects(engine->m_RenderContext);
                    dmProfile::Pause(false);
                }
#endif

#if !(defined(__arm__) || defined(__arm64__) || defined(__EMSCRIPTEN__))
                if (engine->m_UseSwVsync)
                {
                    uint64_t flip_dt = dmTime::GetTime() - prev_flip_time;
                    int remainder = (int)((target_frametime - flip_dt) - engine->m_PreviousRenderTime);
                    if (!engine->m_UseVariableDt && flip_dt < target_frametime && remainder > 1000) // only bother with sleep if diff b/w target and actual time is big enough
                    {
                        DM_PROFILE(Engine, "SoftwareVsync");
                        while (remainder > 500) // dont bother with less than 0.5ms
                        {
                            uint64_t t1 = dmTime::GetTime();
                            dmTime::Sleep(100); // sleep in chunks of 0.1ms
                            uint64_t t2 = dmTime::GetTime();
                            remainder -= (t2-t1);
                        }
                    }
                }
#endif

                uint64_t flip_time_start = dmTime::GetTime();
                dmGraphics::Flip(engine->m_GraphicsContext);
                engine->m_FlipTime = dmTime::GetTime();
                engine->m_PreviousRenderTime = engine->m_FlipTime - flip_time_start;

                RecordData* record_data = &engine->m_RecordData;
                if (record_data->m_Recorder)
                {
                    if (record_data->m_FrameCount % record_data->m_FramePeriod == 0)
                    {
                        uint32_t width = dmGraphics::GetWidth(engine->m_GraphicsContext);
                        uint32_t height = dmGraphics::GetHeight(engine->m_GraphicsContext);
                        uint32_t buffer_size = width * height * 4;

                        dmGraphics::ReadPixels(engine->m_GraphicsContext, record_data->m_Buffer, buffer_size);

                        dmRecord::Result r = dmRecord::RecordFrame(record_data->m_Recorder, record_data->m_Buffer, buffer_size, dmRecord::BUFFER_FORMAT_BGRA);
                        if (r != dmRecord::RESULT_OK)
                        {
                            dmLogError("Error while recoding frame (%d)", r);
                        }
                    }
                    record_data->m_FrameCount++;
                }
            }
            dmProfile::Release(profile);


            ++engine->m_Stats.m_FrameCount;
        }
    }

    static int IsRunning(void* context)
    {
        HEngine engine = (HEngine)context;
        return engine->m_Alive;
    }

    static void PerformStep(void* context)
    {
        HEngine engine = (HEngine)context;
        Step(engine);
    }

    static void Exit(HEngine engine, int32_t code)
    {
        engine->m_Alive = false;
        engine->m_RunResult.m_ExitCode = code;
    }

    static void Reboot(HEngine engine, dmEngineDDF::Reboot* reboot)
    {
        int argc = 0;
        engine->m_RunResult.m_Argv[argc++] = strdup("dmengine");

        // This value should match the count in dmEngineDDF::Reboot
        const int ARG_COUNT = 6;
        char* args[ARG_COUNT] =
        {
            strdup(reboot->m_Arg1),
            strdup(reboot->m_Arg2),
            strdup(reboot->m_Arg3),
            strdup(reboot->m_Arg4),
            strdup(reboot->m_Arg5),
            strdup(reboot->m_Arg6),
        };

        bool empty_found = false;
        for (int i = 0; i < ARG_COUNT; ++i)
        {
            // NOTE: +1 here, see above
            engine->m_RunResult.m_Argv[i + 1] = args[i];
            if (args[i][0] == '\0')
            {
                empty_found = true;
            }

            if (!empty_found)
                argc++;
        }

        engine->m_RunResult.m_Argc = argc;

        engine->m_Alive = false;
        engine->m_RunResult.m_Action = dmEngine::RunResult::REBOOT;
    }

    static RunResult InitRun(dmEngineService::HEngineService engine_service, int argc, char *argv[], PreRun pre_run, PostRun post_run, void* context)
    {
        dmEngine::HEngine engine = dmEngine::New(engine_service);
        dmEngine::RunResult run_result;
        dmLogInfo("Defold Engine %s (%.7s)", dmEngineVersion::VERSION, dmEngineVersion::VERSION_SHA1);
        if (dmEngine::Init(engine, argc, argv))
        {
            if (pre_run)
            {
                pre_run(engine, context);
            }

            dmGraphics::RunApplicationLoop(engine, PerformStep, IsRunning);
            run_result = engine->m_RunResult;

            if (post_run)
            {
                post_run(engine, context);
            }
        }
        else
        {
            run_result.m_ExitCode = 1;
            run_result.m_Action = dmEngine::RunResult::EXIT;
        }
        dmEngine::Delete(engine);

        return run_result;
    }

    int Launch(int argc, char *argv[], PreRun pre_run, PostRun post_run, void* context)
    {
        dmEngineService::HEngineService engine_service = 0;

#if !defined(DM_RELEASE)
        if (dLib::FeaturesSupported(DM_FEATURE_BIT_SOCKET_SERVER_TCP | DM_FEATURE_BIT_SOCKET_SERVER_UDP))
        {
            uint16_t engine_port = 8001;

            char* service_port_env = getenv("DM_SERVICE_PORT");

            // editor 2 specifies DM_SERVICE_PORT=dynamic when launching dmengine
            if (service_port_env) {
                unsigned int env_port = 0;
                if (sscanf(service_port_env, "%u", &env_port) == 1) {
                    engine_port = (uint16_t) env_port;
                }
                else if (strcmp(service_port_env, "dynamic") == 0) {
                    engine_port = 0;
                }
            }

            engine_service = dmEngineService::New(engine_port);
        }
#endif

        dmEngine::RunResult run_result = InitRun(engine_service, argc, argv, pre_run, post_run, context);
        while (run_result.m_Action == dmEngine::RunResult::REBOOT)
        {
            dmEngine::RunResult tmp = InitRun(engine_service, run_result.m_Argc, run_result.m_Argv, pre_run, post_run, context);
            run_result.Free();
            run_result = tmp;
        }
        run_result.Free();

#if !defined(DM_RELEASE)
        if (engine_service)
        {
            dmEngineService::Delete(engine_service);
        }
#endif
        return run_result.m_ExitCode;
    }

    void Dispatch(dmMessage::Message* message, void* user_ptr)
    {
        Engine* self = (Engine*) user_ptr;

        if (message->m_Descriptor != 0)
        {
            dmDDF::Descriptor* descriptor = (dmDDF::Descriptor*)message->m_Descriptor;

            dmDDF::ResolvePointers(descriptor, message->m_Data);

            if (descriptor == dmEngineDDF::Exit::m_DDFDescriptor)
            {
                dmEngineDDF::Exit* ddf = (dmEngineDDF::Exit*) message->m_Data;
                dmEngine::Exit(self, ddf->m_Code);
            }
            else if (descriptor == dmEngineDDF::Reboot::m_DDFDescriptor)
            {
                dmEngineDDF::Reboot* reboot = (dmEngineDDF::Reboot*) message->m_Data;
                dmEngine::Reboot(self, reboot);
            }
            else if (descriptor == dmEngineDDF::ToggleProfile::m_DDFDescriptor)
            {
                self->m_ShowProfile = !self->m_ShowProfile;
            }
            else if (descriptor == dmEngineDDF::TogglePhysicsDebug::m_DDFDescriptor)
            {
                if(dLib::IsDebugMode())
                {
                    self->m_PhysicsContext.m_Debug = !self->m_PhysicsContext.m_Debug;
                }
            }
            else if (descriptor == dmEngineDDF::StartRecord::m_DDFDescriptor)
            {
                dmEngineDDF::StartRecord* start_record = (dmEngineDDF::StartRecord*) message->m_Data;
                RecordData* record_data = &self->m_RecordData;

                record_data->m_FramePeriod = start_record->m_FramePeriod;

                uint32_t width = dmGraphics::GetWidth(self->m_GraphicsContext);
                uint32_t height = dmGraphics::GetHeight(self->m_GraphicsContext);
                dmRecord::NewParams params;
                params.m_Width = width;
                params.m_Height = height;
                params.m_Fps = start_record->m_Fps;

                dmRecord::Result r = dmRecord::New(&params, &record_data->m_Recorder);
                if (r == dmRecord::RESULT_OK)
                {
                    record_data->m_Buffer = new char[width * height * 4];
                    record_data->m_FrameCount = 0;
                }
                else
                {
                    dmLogError("Unable to start recording (%d)", r);
                    record_data->m_Recorder = 0;
                }
            }
            else if (descriptor == dmEngineDDF::StopRecord::m_DDFDescriptor)
            {
                RecordData* record_data = &self->m_RecordData;
                if (record_data->m_Recorder)
                {
                    dmRecord::Delete(record_data->m_Recorder);
                    delete[] record_data->m_Buffer;
                    record_data->m_Recorder = 0;
                    record_data->m_Buffer = 0;
                }
                else
                {
                    dmLogError("No recording in progress");
                }
            }
            else if (descriptor == dmEngineDDF::SetUpdateFrequency::m_DDFDescriptor)
            {
                dmEngineDDF::SetUpdateFrequency* m = (dmEngineDDF::SetUpdateFrequency*) message->m_Data;
                SetUpdateFrequency(self, (uint32_t) m->m_Frequency);
            }
            else if (descriptor == dmEngineDDF::HideApp::m_DDFDescriptor)
            {
                dmGraphics::IconifyWindow(self->m_GraphicsContext);
            }
            else if (descriptor == dmEngineDDF::SetVsync::m_DDFDescriptor)
            {
                dmEngineDDF::SetVsync* m = (dmEngineDDF::SetVsync*) message->m_Data;
                SetSwapInterval(self, m->m_SwapInterval);
            }
            else if (descriptor == dmEngineDDF::RunScript::m_DDFDescriptor)
            {
                dmEngineDDF::RunScript* run_script = (dmEngineDDF::RunScript*) message->m_Data;

                dmResource::HFactory factory = self->m_Factory;
                if (self->m_SharedScriptContext) {
                    dmGameObject::LuaLoad(factory, self->m_SharedScriptContext, &run_script->m_Module);
                }
                else {
                    dmGameObject::LuaLoad(factory, self->m_GOScriptContext, &run_script->m_Module);
                    dmGameObject::LuaLoad(factory, self->m_GuiScriptContext, &run_script->m_Module);
                    dmGameObject::LuaLoad(factory, self->m_RenderScriptContext, &run_script->m_Module);
                }
            }
            else
            {
                const dmMessage::URL* sender = &message->m_Sender;
                const char* socket_name = dmMessage::GetSocketName(sender->m_Socket);
                const char* path_name = dmHashReverseSafe64(sender->m_Path);
                const char* fragment_name = dmHashReverseSafe64(sender->m_Fragment);
                dmLogError("Unknown system message '%s' sent to socket '%s' from %s:%s#%s.",
                           descriptor->m_Name, SYSTEM_SOCKET_NAME, socket_name, path_name, fragment_name);
            }
        }
        else
        {
            const dmMessage::URL* sender = &message->m_Sender;
            const char* socket_name = dmMessage::GetSocketName(sender->m_Socket);
            const char* path_name = dmHashReverseSafe64(sender->m_Path);
            const char* fragment_name = dmHashReverseSafe64(sender->m_Fragment);

            dmLogError("Only system messages can be sent to the '%s' socket. Message sent from: %s:%s#%s",
                       SYSTEM_SOCKET_NAME, socket_name, path_name, fragment_name);
        }
    }

    bool LoadBootstrapContent(HEngine engine, dmConfigFile::HConfig config)
    {
        dmResource::Result fact_error;
#if !defined(DM_RELEASE)
        const char* system_font_map = "/builtins/fonts/system_font.fontc";
        fact_error = dmResource::Get(engine->m_Factory, system_font_map, (void**) &engine->m_SystemFontMap);
        if (fact_error != dmResource::RESULT_OK)
        {
            dmLogFatal("Could not load system font map '%s'.", system_font_map);
            return false;
        }
        dmRender::SetSystemFontMap(engine->m_RenderContext, engine->m_SystemFontMap);
#endif

        const char* gamepads = dmConfigFile::GetString(config, "input.gamepads", "/builtins/input/default.gamepadsc");
        dmInputDDF::GamepadMaps* gamepad_maps_ddf;
        fact_error = dmResource::Get(engine->m_Factory, gamepads, (void**)&gamepad_maps_ddf);
        if (fact_error != dmResource::RESULT_OK)
            return false;
        dmInput::RegisterGamepads(engine->m_InputContext, gamepad_maps_ddf);
        dmResource::Release(engine->m_Factory, gamepad_maps_ddf);

        const char* game_input_binding = dmConfigFile::GetString(config, "input.game_binding", "/input/game.input_bindingc");
        fact_error = dmResource::Get(engine->m_Factory, game_input_binding, (void**)&engine->m_GameInputBinding);
        if (fact_error != dmResource::RESULT_OK)
            return false;

        const char* render_path = dmConfigFile::GetString(config, "bootstrap.render", "/builtins/render/default.renderc");
        fact_error = dmResource::Get(engine->m_Factory, render_path, (void**)&engine->m_RenderScriptPrototype);
        if (fact_error != dmResource::RESULT_OK)
            return false;

        const char* display_profiles_path = dmConfigFile::GetString(config, "display.display_profiles", "/builtins/render/default.display_profilesc");
        fact_error = dmResource::Get(engine->m_Factory, display_profiles_path, (void**)&engine->m_DisplayProfiles);
        if (fact_error != dmResource::RESULT_OK)
            return false;

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
        if (engine->m_DisplayProfiles)
            dmResource::Release(engine->m_Factory, engine->m_DisplayProfiles);
    }

    uint32_t GetFrameCount(HEngine engine)
    {
        return engine->m_Stats.m_FrameCount;
    }
}
