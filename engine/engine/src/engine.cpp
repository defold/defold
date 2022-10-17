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

#include "engine.h"

#include "engine_private.h"

#include <dmsdk/dlib/vmath.h>
#include <sys/stat.h>

#include <stdio.h>
#include <algorithm>

#include <crash/crash.h>
#include <dlib/buffer.h>
#include <dlib/dlib.h>
#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/http_client.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/memprofile.h>
#include <dlib/path.h>
#include <dlib/profile.h>
#include <dlib/socket.h>
#include <dlib/sslsocket.h>
#include <dlib/sys.h>
#include <dlib/thread.h>
#include <dlib/time.h>
#include <graphics/graphics.h>
#include <extension/extension.h>
#include <gamesys/gamesys.h>
#include <gamesys/model_ddf.h>
#include <gamesys/physics_ddf.h>
#include <gamesys/components/comp_gui.h> // For the URL callbacks etc
#include <gameobject/gameobject.h>
#include <gameobject/component.h>
#include <gameobject/gameobject_ddf.h>
#include <gameobject/gameobject_script_util.h>
#include <hid/hid.h>
#include <sound/sound.h>
#include <render/render.h>
#include <render/render_ddf.h>
#include <profiler/profiler.h>
#include <particle/particle.h>
#include <script/sys_ddf.h>
#include <liveupdate/liveupdate.h>

#include "extension.h"
#include "engine_service.h"
#include "engine_version.h"
#include "physics_debug_render.h"

#ifdef __EMSCRIPTEN__
    #include <emscripten/emscripten.h>
#endif

// Embedded resources
// Unfortunately, the draw_line et. al are used in production code
extern unsigned char DEBUG_VPC[];
extern uint32_t DEBUG_VPC_SIZE;
extern unsigned char DEBUG_FPC[];
extern uint32_t DEBUG_FPC_SIZE;

#if defined(DM_RELEASE)
    extern unsigned char BUILTINS_RELEASE_ARCD[];
    extern uint32_t BUILTINS_RELEASE_ARCD_SIZE;
    extern unsigned char BUILTINS_RELEASE_ARCI[];
    extern uint32_t BUILTINS_RELEASE_ARCI_SIZE;
    extern unsigned char BUILTINS_RELEASE_DMANIFEST[];
    extern uint32_t BUILTINS_RELEASE_DMANIFEST_SIZE;

#else
    extern unsigned char BUILTINS_ARCD[];
    extern uint32_t BUILTINS_ARCD_SIZE;
    extern unsigned char BUILTINS_ARCI[];
    extern uint32_t BUILTINS_ARCI_SIZE;
    extern unsigned char BUILTINS_DMANIFEST[];
    extern uint32_t BUILTINS_DMANIFEST_SIZE;

    extern unsigned char GAME_PROJECT[];
    extern uint32_t GAME_PROJECT_SIZE;
#endif

#if defined(__ANDROID__)
// On Android we need to notify the activity which input method to use
// before the keyboard is brought up. This choice is stored as a
// game.project config and used in dmEngine::Init(), passed along to
// the GLFW Android implementation.
extern "C" {
    extern void _glfwAndroidSetInputMethod(int);
    extern void _glfwAndroidSetFullscreenParameters(int, int);
}
#endif

DM_PROPERTY_EXTERN(rmtp_Script);
DM_PROPERTY_U32(rmtp_LuaMem, 0, FrameReset, "kb", &rmtp_Script); // kilo bytes
DM_PROPERTY_U32(rmtp_LuaRefs, 0, FrameReset, "# Lua references", &rmtp_Script);

namespace dmEngine
{
    using namespace dmVMath;

#define SYSTEM_SOCKET_NAME "@system"

    dmEngineService::HEngineService g_EngineService = 0;

    static void OnWindowResize(void* user_data, uint32_t width, uint32_t height)
    {
        uint32_t data_size = sizeof(dmRenderDDF::WindowResized);
        uintptr_t descriptor = (uintptr_t)dmRenderDDF::WindowResized::m_DDFDescriptor;
        dmhash_t message_id = dmRenderDDF::WindowResized::m_DDFDescriptor->m_NameHash;

        dmRenderDDF::WindowResized window_resized;
        window_resized.m_Width = width;
        window_resized.m_Height = height;

        dmMessage::URL receiver;
        dmMessage::ResetURL(&receiver);
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
        // update gui context if it exists
        if (engine->m_GuiContext)
        {
            dmGui::SetPhysicalResolution(engine->m_GuiContext, width, height);
        }

        dmGameSystem::OnWindowResized(width, height);
    }

    static bool OnWindowClose(void* user_data)
    {
        Engine* engine = (Engine*)user_data;
        engine->m_Alive = false;
        // Never allow closing the window here, clean up and then close manually
        return false;
    }

    static void Dispatch(dmMessage::Message *message_object, void* user_ptr);

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

    static void OnWindowIconify(void* user_data, uint32_t iconify)
    {
        Engine* engine = (Engine*)user_data;

        // We reset the time on both events because
        // on some platforms both events will arrive when regaining focus
        engine->m_PreviousFrameTime = dmTime::GetTime(); // we might have stalled for a long time

        dmExtension::Params params;
        params.m_ConfigFile = engine->m_Config;
        params.m_L          = 0;
        dmExtension::Event event;
        event.m_Event = iconify ? dmExtension::EVENT_ID_ICONIFYAPP : dmExtension::EVENT_ID_DEICONIFYAPP;
        dmExtension::DispatchEvent( &params, &event );

        dmGameSystem::OnWindowIconify(iconify != 0);
    }

    static void SetupComponentCreateContext(HEngine engine, dmGameObject::ComponentTypeCreateCtx& component_create_ctx)
    {
        component_create_ctx.m_Config = engine->m_Config;
        component_create_ctx.m_Script = engine->m_GOScriptContext;
        component_create_ctx.m_Register = engine->m_Register;
        component_create_ctx.m_Factory = engine->m_Factory;
        component_create_ctx.m_Contexts.SetCapacity(3, 8);
        component_create_ctx.m_Contexts.Put(dmHashString64("graphics"), engine->m_GraphicsContext);
        component_create_ctx.m_Contexts.Put(dmHashString64("render"), engine->m_RenderContext);
        if (engine->m_GuiContext)
        {
            component_create_ctx.m_Contexts.Put(dmHashString64("gui_scriptc"), engine->m_GuiScriptContext);
            component_create_ctx.m_Contexts.Put(dmHashString64("guic"), engine->m_GuiContext);
        }
    }

    Stats::Stats()
    : m_FrameCount(0)
    , m_TotalTime(0.0f)
    {

    }

    Engine::Engine(dmEngineService::HEngineService engine_service)
    : m_Config(0)
    , m_Alive(true)
    , m_MainCollection(0)
    , m_LastReloadMTime(0)
    , m_MouseSensitivity(1.0f)
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
    , m_RenderScriptPrototype(0x0)
    , m_Stats()
    , m_WasIconified(true)
    , m_QuitOnEsc(false)
    , m_ConnectionAppMode(false)
    , m_RunWhileIconified(0)
    , m_Width(960)
    , m_Height(640)
    , m_InvPhysicalWidth(1.0f/960)
    , m_InvPhysicalHeight(1.0f/640)
    {
        m_EngineService = engine_service;
        m_Register = dmGameObject::NewRegister();
        m_InputBuffer.SetCapacity(64);
        m_ResourceTypeContexts.SetCapacity(31, 64);

        m_PhysicsContext.m_Context3D = 0x0; // it'a union
        m_PhysicsContext.m_Debug = false;
        m_PhysicsContext.m_3D = false;
        m_GuiContext = 0x0;
        m_SpriteContext.m_RenderContext = 0x0;
        m_SpriteContext.m_MaxSpriteCount = 0;
        m_ModelContext.m_RenderContext = 0x0;
        m_ModelContext.m_MaxModelCount = 0;
        m_MeshContext.m_RenderContext = 0x0;
        m_MeshContext.m_MaxMeshCount = 0;
        m_AccumFrameTime = 0;
        m_PreviousFrameTime = dmTime::GetTime();
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

        dmGameObject::DeleteCollections(engine->m_Register); // Delete all collections and game objects

        dmHttpClient::ShutdownConnectionPool();

        dmLiveUpdate::Finalize();

        // Reregister the types before the rest of the contexts are deleted
        if (engine->m_Factory) {
            dmResource::DeregisterTypes(engine->m_Factory, &engine->m_ResourceTypeContexts);
        }

        dmGameObject::ComponentTypeCreateCtx component_create_ctx;
        SetupComponentCreateContext(engine, component_create_ctx);

        dmGameObject::DestroyRegisteredComponentTypes(&component_create_ctx);

        dmGameSystem::ScriptLibContext script_lib_context;
        script_lib_context.m_Factory = engine->m_Factory;
        script_lib_context.m_Register = engine->m_Register;
        if (engine->m_SharedScriptContext) {
            script_lib_context.m_LuaState = dmScript::GetLuaState(engine->m_SharedScriptContext);
            dmGameSystem::FinalizeScriptLibs(script_lib_context);
        } else {
            script_lib_context.m_LuaState = dmScript::GetLuaState(engine->m_GOScriptContext);
            dmGameSystem::FinalizeScriptLibs(script_lib_context);
            script_lib_context.m_LuaState = dmScript::GetLuaState(engine->m_GuiScriptContext);
            dmGameSystem::FinalizeScriptLibs(script_lib_context);
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

        if (engine->m_GuiContext)
            dmGui::DeleteContext(engine->m_GuiContext, engine->m_GuiScriptContext);

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

        dmEngine::ExtensionAppParams app_params;
        app_params.m_ConfigFile = engine->m_Config;
        app_params.m_WebServer = dmEngineService::GetWebServer(engine->m_EngineService);
        app_params.m_GameObjectRegister = engine->m_Register;
        app_params.m_HIDContext = engine->m_HidContext;
        dmExtension::AppFinalize((dmExtension::AppParams*)&app_params);

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

    static bool GetProjectFile(int argc, char *argv[], char* resources_path, char* project_file, uint32_t project_file_size)
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
            char* paths[3];
            uint32_t count = 0;

            const char* mountstr = "";
#if defined(__NX__)
            mountstr = "data:/";
            // there's no way to check for a named mount, and it will assert
            // So we'll only enter here if it's set on this platform
            if (dmSys::GetEnv("DM_MOUNT_HOST") != 0)
                mountstr = "host:/";
#endif
            dmSnPrintf(p1, sizeof(p1), "%sgame.projectc", mountstr);
            dmSnPrintf(p2, sizeof(p2), "%sbuild/default/game.projectc", mountstr);
            paths[count++] = p1;
            paths[count++] = p2;

            if (resources_path)
            {
                dmPath::Concat(resources_path, "game.projectc", p3, sizeof(p3));
                paths[count++] = p3;
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

    static bool LoadAndSetSslKeys(const char* ssl_keys_path)
    {
        if (!dmSys::ResourceExists(ssl_keys_path))
        {
            return false;
        }
        uint32_t file_size;
        if (dmSys::ResourceSize(ssl_keys_path, &file_size) != dmSys::RESULT_OK)
        {
            return false;
        }
        uint8_t* ssl_keys_buf = (uint8_t*)malloc(file_size);
        uint32_t loaded_file_size = 0;
        dmSys::Result load_result = dmSys::LoadResource(ssl_keys_path, ssl_keys_buf, file_size, &loaded_file_size);
        if (load_result != dmSys::RESULT_OK)
        {
            dmLogError("Failed to read ssl_keys.pem at %s (%i)", ssl_keys_path, load_result);
            free(ssl_keys_buf);
            return false;
        }
        if (loaded_file_size != file_size)
        {
            dmLogError("Failed to load ssl_keys.pem: tried reading %d bytes, got %d bytes", file_size, loaded_file_size);
            free(ssl_keys_buf);
            return false;
        }
        dmSSLSocket::Result loadind_key_result = dmSSLSocket::SetSslPublicKeys(ssl_keys_buf, loaded_file_size);
        free(ssl_keys_buf);
        return loadind_key_result == dmSSLSocket::RESULT_OK;
    }

    static void SetSwapInterval(HEngine engine, int swap_interval)
    {
        swap_interval = dmMath::Max(0, swap_interval);
        dmGraphics::SetSwapInterval(engine->m_GraphicsContext, swap_interval);
    }

    static void SetUpdateFrequency(HEngine engine, uint32_t frequency)
    {
        engine->m_UpdateFrequency = frequency;
    }

    struct LuaCallstackCtx
    {
        bool     m_First;
        char*    m_Buffer;
        uint32_t m_BufferSize;
    };

    static void GetLuaStackTraceCbk(lua_State* L, lua_Debug* entry, void* _ctx)
    {
        LuaCallstackCtx* ctx = (LuaCallstackCtx*)_ctx;

        if (ctx->m_First)
        {
            int32_t nwritten = dmSnPrintf(ctx->m_Buffer, ctx->m_BufferSize, "Lua Callstack:\n");
            if (nwritten < 0)
                nwritten = 0;
            ctx->m_Buffer += nwritten;
            ctx->m_BufferSize -= nwritten;
            ctx->m_First = false;
        }

        uint32_t nwritten = dmScript::WriteLuaTracebackEntry(entry, ctx->m_Buffer, ctx->m_BufferSize);
        ctx->m_Buffer += nwritten;
        ctx->m_BufferSize -= nwritten;
    }

    static void CrashHandlerCallback(void* ctx, char* buffer, uint32_t buffersize)
    {
        HEngine engine = (HEngine)ctx;
        if (engine->m_SharedScriptContext) {
            LuaCallstackCtx ctx;
            ctx.m_First = true;
            ctx.m_Buffer = buffer;
            ctx.m_BufferSize = buffersize;
            dmScript::GetLuaTraceback(dmScript::GetLuaState(engine->m_SharedScriptContext), "Sln", GetLuaStackTraceCbk, &ctx);
        }
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
        dmLogInfo("Defold Engine %s (%.7s)", dmEngineVersion::VERSION, dmEngineVersion::VERSION_SHA1);

        dmCrash::SetExtraInfoCallback(CrashHandlerCallback, engine);

        dmSys::EngineInfoParam engine_info;
        engine_info.m_Platform = dmEngineVersion::PLATFORM;
        engine_info.m_Version = dmEngineVersion::VERSION;
        engine_info.m_VersionSHA1 = dmEngineVersion::VERSION_SHA1;
        engine_info.m_IsDebug = dLib::IsDebugMode();
        dmSys::SetEngineInfo(engine_info);

        char* qoe_s = dmSys::GetEnv("DM_QUIT_ON_ESC");
        engine->m_QuitOnEsc = ((qoe_s != 0x0) && (qoe_s[0] == '1'));

        char project_file[DMPATH_MAX_PATH];
        char project_file_uri[DMPATH_MAX_PATH];
        char project_file_folder[DMPATH_MAX_PATH] = ".";
        bool loaded_ok = false;

        char resources_path[DMPATH_MAX_PATH];
        dmSys::GetResourcesPath(argc, argv, resources_path, sizeof(resources_path));

        if (GetProjectFile(argc, argv, resources_path, project_file, sizeof(project_file)))
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
                dmPath::Dirname(project_file, project_file_folder, sizeof(project_file_folder));
                dmStrlCpy(project_file_uri, project_file_folder, sizeof(project_file_uri));

                char tmp[DMPATH_MAX_PATH];
                dmStrlCpy(tmp, project_file_folder, sizeof(tmp));
                if (project_file_folder[0])
                {
                    dmStrlCat(tmp, "/game.dmanifest", sizeof(tmp));
                }
                else
                {
                    dmStrlCat(tmp, "game.dmanifest", sizeof(tmp));
                }
                if (dmSys::ResourceExists(tmp))
                {
                    dmStrlCpy(project_file_uri, "dmanif:", sizeof(project_file_uri));
                    dmStrlCat(project_file_uri, tmp, sizeof(project_file_uri));
                }
            }
        }

        if( !loaded_ok )
        {
#if defined(DM_RELEASE)
            dmLogFatal("Unable to load project");
            return false;
#else
            dmConfigFile::Result cr = dmConfigFile::LoadFromBuffer((const char*) GAME_PROJECT, GAME_PROJECT_SIZE, argc, (const char**) argv, &engine->m_Config);
            if (cr != dmConfigFile::RESULT_OK)
            {
                dmLogFatal("Unable to load builtin connect project");
                return false;
            }
            engine->m_ConnectionAppMode = true;
#endif
        }

        // Try loading SSL keys
        char engine_ssl_keys_path[DMPATH_MAX_PATH];
        dmPath::Concat(resources_path, "/ssl_keys.pem", engine_ssl_keys_path, sizeof(engine_ssl_keys_path));
        char editor_ssl_keys_path[DMPATH_MAX_PATH];
        dmPath::Concat(project_file_folder, dmConfigFile::GetString(engine->m_Config, "network.ssl_certificates", ""), editor_ssl_keys_path, sizeof(editor_ssl_keys_path));
        const char* paths[] = {engine_ssl_keys_path, editor_ssl_keys_path};
        for (uint32_t i = 0; i < DM_ARRAY_SIZE(paths); ++i)
        {
            if (paths[i] && LoadAndSetSslKeys(paths[i]))
            {
                dmLogInfo("SSL verification enabled");
                break;
            }
        }

        // Set HTML5 console banner "MadeWithDefold"
        #if defined(__EMSCRIPTEN__)
        if (1 == dmConfigFile::GetInt(engine->m_Config, "html5.show_console_banner", 1))
        {
            EM_ASM({
                if (navigator.userAgent.toLowerCase().indexOf('chrome') > -1) {
                    console.log("%c    %c    Made with Defold    %c    %c    https://www.defold.com",
                        "background: #fd6623; padding:5px 0; border: 5px;",
                        "background: #272c31; color: #fafafa; padding:5px 0;",
                        "background: #39a3e4; padding:5px 0;",
                        "background: #ffffff; color: #000000; padding:5px 0;"
                    );
                }
                else {
                    console.log("Made with Defold -=[ https://www.defold.com ]=-");
                }
            });
        }
        #endif

        // Catch engine specific arguments
        bool verify_graphics_calls = dLib::IsDebugMode();

        // The default is 1, and the only way to know if the property is manually set, is if it's 0
        // since the values are always written to the project file
        if (0 == dmConfigFile::GetInt(engine->m_Config, "graphics.verify_graphics_calls", 1))
            verify_graphics_calls = false;

        bool renderdoc_support = false;
        bool use_validation_layers = false;
        const char verify_graphics_calls_arg[] = "--verify-graphics-calls=";
        const char renderdoc_support_arg[] = "--renderdoc";
        const char validation_layers_support_arg[] = "--use-validation-layers";
        const char verbose_long[] = "--verbose";
        const char verbose_short[] = "-v";
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
            else if (strncmp(renderdoc_support_arg, arg, sizeof(renderdoc_support_arg)-1) == 0)
            {
                renderdoc_support = true;
            }
            else if (strncmp(validation_layers_support_arg, arg, sizeof(validation_layers_support_arg)-1) == 0)
            {
                use_validation_layers = true;
            }
            else if (strncmp(verbose_long, arg, sizeof(verbose_long)-1) == 0 ||
                     strncmp(verbose_short, arg, sizeof(verbose_short)-1) == 0)
            {
                dmLogSetLevel(LOG_SEVERITY_DEBUG);
            }
        }

        dmBuffer::NewContext();


        dmHID::NewContextParams new_hid_params = dmHID::NewContextParams();
        new_hid_params.m_GamepadConnectivityCallback = dmInput::GamepadConnectivityCallback;

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

        dmEngine::ExtensionAppParams app_params;
        app_params.m_ConfigFile = engine->m_Config;
        app_params.m_WebServer = dmEngineService::GetWebServer(engine->m_EngineService);
        app_params.m_GameObjectRegister = engine->m_Register;
        app_params.m_HIDContext = engine->m_HidContext;

        dmExtension::Result er = dmExtension::AppInitialize((dmExtension::AppParams*)&app_params);
        if (er != dmExtension::RESULT_OK) {
            dmLogFatal("Failed to initialize extensions (%d)", er);
            return false;
        }

        int write_log = dmConfigFile::GetInt(engine->m_Config, "project.write_log", 0);
        if (write_log) {
            uint32_t count = 0;
            char* log_paths[3];

            const char* LOG_FILE_NAME = "log.txt";

            const char* log_dir = dmConfigFile::GetString(engine->m_Config, "project.log_dir", NULL);
            char log_config_path[DMPATH_MAX_PATH];
            if (log_dir != NULL)
            {
                dmPath::Concat(log_dir, LOG_FILE_NAME, log_config_path, sizeof(log_config_path));
                log_paths[count] = log_config_path;
                count++;
            }

            char sys_path[DMPATH_MAX_PATH];
            char main_log_path[DMPATH_MAX_PATH];
            if (dmSys::GetLogPath(sys_path, sizeof(sys_path)) == dmSys::RESULT_OK)
            {
                dmPath::Concat(sys_path, LOG_FILE_NAME, main_log_path, sizeof(main_log_path));
                log_paths[count] = main_log_path;
                count++;
            }

            char application_support_path[DMPATH_MAX_PATH];
            char application_support_log_path[DMPATH_MAX_PATH];
            const char* logs_dir = dmConfigFile::GetString(engine->m_Config, "project.title_as_file_name", "defoldlogs");
            if (dmSys::GetApplicationSavePath(logs_dir, application_support_path, sizeof(application_support_path)) == dmSys::RESULT_OK)
            {
                dmPath::Concat(application_support_path, LOG_FILE_NAME, application_support_log_path, sizeof(application_support_log_path));
                log_paths[count] = application_support_log_path;
                count++;
            }
            for (uint32_t i = 0; i < count; ++i)
            {
                if (dmLog::SetLogFile(log_paths[i]))
                {
                    break;
                }
            }
        }

        const char* update_order = dmConfigFile::GetString(engine->m_Config, "gameobject.update_order", 0);

        // This scope is mainly here to make sure the "Main" scope is created first
        DM_PROFILE("Init");

        dmGraphics::ContextParams graphics_context_params;
        graphics_context_params.m_DefaultTextureMinFilter = ConvertMinTextureFilter(dmConfigFile::GetString(engine->m_Config, "graphics.default_texture_min_filter", "linear"));
        graphics_context_params.m_DefaultTextureMagFilter = ConvertMagTextureFilter(dmConfigFile::GetString(engine->m_Config, "graphics.default_texture_mag_filter", "linear"));
        graphics_context_params.m_VerifyGraphicsCalls     = verify_graphics_calls;
        graphics_context_params.m_RenderDocSupport        = renderdoc_support || dmConfigFile::GetInt(engine->m_Config, "graphics.use_renderdoc", 0) != 0;

        graphics_context_params.m_UseValidationLayers = use_validation_layers || dmConfigFile::GetInt(engine->m_Config, "graphics.use_validationlayers", 0) != 0;
        graphics_context_params.m_GraphicsMemorySize = dmConfigFile::GetInt(engine->m_Config, "graphics.memory_size", 0) * 1024*1024; // MB -> bytes

        engine->m_GraphicsContext = dmGraphics::NewContext(graphics_context_params);
        if (engine->m_GraphicsContext == 0x0)
        {
            dmLogFatal("Unable to create the graphics context.");
            return false;
        }

        engine->m_Width = dmConfigFile::GetInt(engine->m_Config, "display.width", 960);
        engine->m_Height = dmConfigFile::GetInt(engine->m_Config, "display.height", 640);

        float clear_color_red = dmConfigFile::GetFloat(engine->m_Config, "render.clear_color_red", 0.0);
        float clear_color_green = dmConfigFile::GetFloat(engine->m_Config, "render.clear_color_green", 0.0);
        float clear_color_blue = dmConfigFile::GetFloat(engine->m_Config, "render.clear_color_blue", 0.0);
        float clear_color_alpha = dmConfigFile::GetFloat(engine->m_Config, "render.clear_color_alpha", 0.0);
        uint32_t clear_color = ((uint32_t)(clear_color_red * 255.0) & 0x000000ff)
                             | (((uint32_t)(clear_color_green * 255.0) & 0x000000ff) << 8)
                             | (((uint32_t)(clear_color_blue * 255.0) & 0x000000ff) << 16)
                             | (((uint32_t)(clear_color_alpha * 255.0) & 0x000000ff) << 24);
        engine->m_ClearColor = clear_color;

        dmGraphics::WindowParams window_params;
        window_params.m_ResizeCallback = OnWindowResize;
        window_params.m_ResizeCallbackUserData = engine;
        window_params.m_CloseCallback = OnWindowClose;
        window_params.m_CloseCallbackUserData = engine;
        window_params.m_FocusCallback = OnWindowFocus;
        window_params.m_FocusCallbackUserData = engine;
        window_params.m_IconifyCallback = OnWindowIconify;
        window_params.m_IconifyCallbackUserData = engine;
        window_params.m_Width = engine->m_Width;
        window_params.m_Height = engine->m_Height;
        window_params.m_Samples = dmConfigFile::GetInt(engine->m_Config, "display.samples", 0);
        window_params.m_Title = dmConfigFile::GetString(engine->m_Config, "project.title", "TestTitle");
        window_params.m_Fullscreen = (bool) dmConfigFile::GetInt(engine->m_Config, "display.fullscreen", 0);
        window_params.m_PrintDeviceInfo = dmConfigFile::GetInt(engine->m_Config, "display.display_device_info", 0);
        window_params.m_HighDPI = (bool) dmConfigFile::GetInt(engine->m_Config, "display.high_dpi", 0);
        window_params.m_BackgroundColor = clear_color;

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

#if defined(__MACH__) || defined(__linux__) || defined(_WIN32)
        engine->m_RunWhileIconified = dmConfigFile::GetInt(engine->m_Config, "engine.run_while_iconified", 0);
#endif

        engine->m_FixedUpdateFrequency = dmConfigFile::GetInt(engine->m_Config, "engine.fixed_update_frequency", 60);

        dmGameSystem::OnWindowCreated(physical_width, physical_height);

        engine->m_UpdateFrequency = dmConfigFile::GetInt(engine->m_Config, "display.update_frequency", 0);

        SetUpdateFrequency(engine, engine->m_UpdateFrequency);

        bool setting_vsync = dmConfigFile::GetInt(engine->m_Config, "display.vsync", true); // Deprecated

        uint32_t opengl_swap_interval = dmConfigFile::GetInt(engine->m_Config, "display.swap_interval", 1);
        if (!setting_vsync)
        {
            opengl_swap_interval = 0;
        }
        SetSwapInterval(engine, opengl_swap_interval);

        const uint32_t max_resources = dmConfigFile::GetInt(engine->m_Config, dmResource::MAX_RESOURCES_KEY, 1024);
        dmResource::NewFactoryParams params;
        params.m_MaxResources = max_resources;
        params.m_Flags = 0;

        dmResourceArchive::ClearArchiveLoaders(); // in case we've rebooted
        dmResourceArchive::RegisterDefaultArchiveLoader();

        if (dLib::IsDebugMode())
        {
            params.m_Flags = RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT;

            int32_t http_cache = dmConfigFile::GetInt(engine->m_Config, "resource.http_cache", 1);
            if (http_cache)
                params.m_Flags |= RESOURCE_FACTORY_FLAGS_HTTP_CACHE;
        }

        int32_t liveupdate_enable = dmConfigFile::GetInt(engine->m_Config, "liveupdate.enabled", 1);
        if (liveupdate_enable)
        {
            params.m_Flags |= RESOURCE_FACTORY_FLAGS_LIVE_UPDATE;

            dmLiveUpdate::RegisterArchiveLoaders();
        }

#if defined(DM_RELEASE)
        params.m_ArchiveIndex.m_Data = (const void*) BUILTINS_RELEASE_ARCI;
        params.m_ArchiveIndex.m_Size = BUILTINS_RELEASE_ARCI_SIZE;
        params.m_ArchiveData.m_Data = (const void*) BUILTINS_RELEASE_ARCD;
        params.m_ArchiveData.m_Size = BUILTINS_RELEASE_ARCD_SIZE;
        params.m_ArchiveManifest.m_Data = (const void*) BUILTINS_RELEASE_DMANIFEST;
        params.m_ArchiveManifest.m_Size = BUILTINS_RELEASE_DMANIFEST_SIZE;
#else
        params.m_ArchiveIndex.m_Data = (const void*) BUILTINS_ARCI;
        params.m_ArchiveIndex.m_Size = BUILTINS_ARCI_SIZE;
        params.m_ArchiveData.m_Data = (const void*) BUILTINS_ARCD;
        params.m_ArchiveData.m_Size = BUILTINS_ARCD_SIZE;
        params.m_ArchiveManifest.m_Data = (const void*) BUILTINS_DMANIFEST;
        params.m_ArchiveManifest.m_Size = BUILTINS_DMANIFEST_SIZE;
#endif

        const char* resource_uri = dmConfigFile::GetString(engine->m_Config, "resource.uri", project_file_uri);
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

        dmHID::Init(engine->m_HidContext);

        dmSound::InitializeParams sound_params;
        sound_params.m_OutputDevice = "default";
#if defined(__EMSCRIPTEN__)
        sound_params.m_UseThread = false;
#else
        sound_params.m_UseThread = dmConfigFile::GetInt(engine->m_Config, "sound.use_thread", 1) != 0;
#endif
        dmSound::Result soundInit = dmSound::Initialize(engine->m_Config, &sound_params);
        if (dmSound::RESULT_OK == soundInit) {
            dmLogInfo("Initialised sound device '%s'", sound_params.m_OutputDevice);
        } else {
            dmLogWarning("Failed to initialize sound system.");
        }

        dmGameObject::Result go_result = dmGameObject::SetCollectionDefaultCapacity(engine->m_Register, dmConfigFile::GetInt(engine->m_Config, dmGameObject::COLLECTION_MAX_INSTANCES_KEY, dmGameObject::DEFAULT_MAX_COLLECTION_CAPACITY));
        if(go_result != dmGameObject::RESULT_OK)
        {
            dmLogFatal("Failed to set max instance count for collections (%d)", go_result);
            return false;
        }
        dmGameObject::SetInputStackDefaultCapacity(engine->m_Register, dmConfigFile::GetInt(engine->m_Config, dmGameObject::COLLECTION_MAX_INPUT_STACK_ENTRIES_KEY, dmGameObject::DEFAULT_MAX_INPUT_STACK_CAPACITY));

        dmRender::RenderContextParams render_params;
        render_params.m_MaxRenderTypes = 16;
        render_params.m_MaxInstances = (uint32_t) dmConfigFile::GetInt(engine->m_Config, "graphics.max_draw_calls", 1024);
        render_params.m_MaxRenderTargets = 32;
        render_params.m_VertexShaderDesc = ::DEBUG_VPC;
        render_params.m_VertexShaderDescSize = ::DEBUG_VPC_SIZE;
        render_params.m_FragmentShaderDesc = ::DEBUG_FPC;
        render_params.m_FragmentShaderDescSize = ::DEBUG_FPC_SIZE;
        render_params.m_MaxCharacters = (uint32_t) dmConfigFile::GetInt(engine->m_Config, "graphics.max_characters", 2048 * 4);;
        render_params.m_CommandBufferSize = 1024;
        render_params.m_ScriptContext = engine->m_RenderScriptContext;
        render_params.m_MaxDebugVertexCount = (uint32_t) dmConfigFile::GetInt(engine->m_Config, "graphics.max_debug_vertices", 10000);
        engine->m_RenderContext = dmRender::NewRenderContext(engine->m_GraphicsContext, render_params);

        dmGameObject::Initialize(engine->m_Register, engine->m_GOScriptContext);

        engine->m_ParticleFXContext.m_Factory = engine->m_Factory;
        engine->m_ParticleFXContext.m_RenderContext = engine->m_RenderContext;
        engine->m_ParticleFXContext.m_MaxParticleFXCount = dmConfigFile::GetInt(engine->m_Config, dmParticle::MAX_INSTANCE_COUNT_KEY, 64);
        engine->m_ParticleFXContext.m_MaxEmitterCount = dmConfigFile::GetInt(engine->m_Config, dmParticle::MAX_EMITTER_COUNT_KEY, 64);
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

        // rig.max_instance_count is deprecated in favour of component specific max count values.
        // For backwards combatibility we get the rig generic value and take the max of it and each
        // specific component max value.
        int32_t max_rig_instance = dmConfigFile::GetInt(engine->m_Config, "rig.max_instance_count", 128);
        int32_t max_model_count = dmMath::Max(dmConfigFile::GetInt(engine->m_Config, "model.max_count", 128), max_rig_instance);

        dmGui::NewContextParams gui_params;
        gui_params.m_ScriptContext = engine->m_GuiScriptContext;
        gui_params.m_GetURLCallback = dmGameSystem::GuiGetURLCallback;
        gui_params.m_GetUserDataCallback = dmGameSystem::GuiGetUserDataCallback;
        gui_params.m_ResolvePathCallback = dmGameSystem::GuiResolvePathCallback;
        gui_params.m_GetTextMetricsCallback = dmGameSystem::GuiGetTextMetricsCallback;

      // If an extension changes window size at extensions initialization phase, engine should read that.
        physical_width = dmGraphics::GetWindowWidth(engine->m_GraphicsContext);
        physical_height = dmGraphics::GetWindowHeight(engine->m_GraphicsContext);

        gui_params.m_PhysicalWidth = physical_width;
        gui_params.m_PhysicalHeight = physical_height;
        gui_params.m_DefaultProjectWidth = engine->m_Width;
        gui_params.m_DefaultProjectHeight = engine->m_Height;
        gui_params.m_Dpi = physical_dpi;

        engine->m_GuiContext = dmGui::NewContext(&gui_params);

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
        physics_params.m_VelocityThreshold = dmConfigFile::GetFloat(engine->m_Config, "physics.velocity_threshold", 1.0f);
        if (physics_params.m_Scale < dmPhysics::MIN_SCALE || physics_params.m_Scale > dmPhysics::MAX_SCALE)
        {
            dmLogWarning("Physics scale must be in the range %.2f - %.2f and has been clamped.", dmPhysics::MIN_SCALE, dmPhysics::MAX_SCALE);
            if (physics_params.m_Scale < dmPhysics::MIN_SCALE)
                physics_params.m_Scale = dmPhysics::MIN_SCALE;
            if (physics_params.m_Scale > dmPhysics::MAX_SCALE)
                physics_params.m_Scale = dmPhysics::MAX_SCALE;
        }
        physics_params.m_ContactImpulseLimit = dmConfigFile::GetFloat(engine->m_Config, "physics.contact_impulse_limit", 0.0f);
        physics_params.m_AllowDynamicTransforms = dmConfigFile::GetInt(engine->m_Config, "physics.allow_dynamic_transforms", 1) ? 1 : 0;
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
        engine->m_PhysicsContext.m_UseFixedTimestep = dmConfigFile::GetInt(engine->m_Config, dmGameSystem::PHYSICS_USE_FIXED_TIMESTEP, 1) ? 1 : 0;
        engine->m_PhysicsContext.m_MaxFixedTimesteps = dmConfigFile::GetInt(engine->m_Config, dmGameSystem::PHYSICS_MAX_FIXED_TIMESTEPS, 2);
        // TODO: Should move inside the ifdef release? Is this usable without the debug callbacks?
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
        engine->m_ModelContext.m_MaxModelCount = max_model_count;

        engine->m_MeshContext.m_RenderContext = engine->m_RenderContext;
        engine->m_MeshContext.m_Factory       = engine->m_Factory;
        engine->m_MeshContext.m_MaxMeshCount = dmConfigFile::GetInt(engine->m_Config, "mesh.max_count", 128);

        engine->m_LabelContext.m_RenderContext      = engine->m_RenderContext;
        engine->m_LabelContext.m_MaxLabelCount      = dmConfigFile::GetInt(engine->m_Config, "label.max_count", 64);
        engine->m_LabelContext.m_Subpixels          = dmConfigFile::GetInt(engine->m_Config, "label.subpixels", 1);

        engine->m_TilemapContext.m_RenderContext    = engine->m_RenderContext;
        engine->m_TilemapContext.m_MaxTilemapCount  = dmConfigFile::GetInt(engine->m_Config, "tilemap.max_count", 16);
        engine->m_TilemapContext.m_MaxTileCount     = dmConfigFile::GetInt(engine->m_Config, "tilemap.max_tile_count", 2048);

        engine->m_SoundContext.m_MaxComponentCount  = dmConfigFile::GetInt(engine->m_Config, "sound.max_component_count", 32);
        engine->m_SoundContext.m_MaxSoundInstances  = dmConfigFile::GetInt(engine->m_Config, "sound.max_sound_instances", 256);

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

        dmGameObject::ComponentTypeCreateCtx component_create_ctx;
        SetupComponentCreateContext(engine, component_create_ctx);

        dmResource::Result fact_result;
        dmGameSystem::ScriptLibContext script_lib_context;

        // Variables need to be declared up here due to the goto's
        bool has_host_mount = dmSys::GetEnv("DM_MOUNT_HOST") != 0;

        engine->m_ResourceTypeContexts.Put(dmHashString64("goc"), engine->m_Register);
        engine->m_ResourceTypeContexts.Put(dmHashString64("collectionc"), engine->m_Register);
        engine->m_ResourceTypeContexts.Put(dmHashString64("luac"), &engine->m_ModuleContext);
        engine->m_ResourceTypeContexts.Put(dmHashString64("scriptc"), engine->m_GOScriptContext);
        if (engine->m_GuiContext)
        {
            engine->m_ResourceTypeContexts.Put(dmHashString64("gui_scriptc"), engine->m_GuiScriptContext);
            engine->m_ResourceTypeContexts.Put(dmHashString64("guic"), engine->m_GuiContext);
        }

        fact_result = dmResource::RegisterTypes(engine->m_Factory, &engine->m_ResourceTypeContexts);
        if (fact_result != dmResource::RESULT_OK)
            goto bail;

        fact_result = dmGameSystem::RegisterResourceTypes(engine->m_Factory, engine->m_RenderContext, engine->m_InputContext, &engine->m_PhysicsContext);
        if (fact_result != dmResource::RESULT_OK)
            goto bail;

        go_result = dmGameSystem::RegisterComponentTypes(engine->m_Factory, engine->m_Register, engine->m_RenderContext, &engine->m_PhysicsContext, &engine->m_ParticleFXContext, &engine->m_SpriteContext,
                                                                                                &engine->m_CollectionProxyContext, &engine->m_FactoryContext, &engine->m_CollectionFactoryContext,
                                                                                                &engine->m_ModelContext, &engine->m_MeshContext, &engine->m_LabelContext, &engine->m_TilemapContext,
                                                                                                &engine->m_SoundContext);
        if (go_result != dmGameObject::RESULT_OK)
            goto bail;

        // register the component extensions

        go_result = dmGameObject::CreateRegisteredComponentTypes(&component_create_ctx);
        if (go_result != dmGameObject::RESULT_OK)
            goto bail;

        if (!LoadBootstrapContent(engine, engine->m_Config))
        {
            dmLogError("Unable to load bootstrap data.");
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

        dmGui::SetDefaultFont(engine->m_GuiContext, engine->m_SystemFontMap);
        dmGui::SetDisplayProfiles(engine->m_GuiContext, engine->m_DisplayProfiles);

        // clear it a couple of times, due to initialization of extensions might stall the updates
        for (int i = 0; i < 3; ++i) {
            dmGraphics::BeginFrame(engine->m_GraphicsContext);
            dmGraphics::SetViewport(engine->m_GraphicsContext, 0, 0, dmGraphics::GetWindowWidth(engine->m_GraphicsContext), dmGraphics::GetWindowHeight(engine->m_GraphicsContext));
            dmGraphics::Clear(engine->m_GraphicsContext, dmGraphics::BUFFER_TYPE_COLOR0_BIT,
                                        (float)((engine->m_ClearColor>> 0)&0xFF),
                                        (float)((engine->m_ClearColor>> 8)&0xFF),
                                        (float)((engine->m_ClearColor>>16)&0xFF),
                                        (float)((engine->m_ClearColor>>24)&0xFF),
                                        1.0f, 0);
            dmGraphics::Flip(engine->m_GraphicsContext);
        }

        if (engine->m_RenderScriptPrototype) {
            dmRender::RenderScriptResult script_result = InitRenderScriptInstance(engine->m_RenderScriptPrototype->m_Instance);
            if (script_result != dmRender::RENDER_SCRIPT_RESULT_OK) {
                dmLogFatal("Render script could not be initialized.");
                goto bail;
            }
        }

        script_lib_context.m_Factory    = engine->m_Factory;
        script_lib_context.m_Register   = engine->m_Register;
        script_lib_context.m_HidContext = engine->m_HidContext;

        if (engine->m_SharedScriptContext) {
            script_lib_context.m_LuaState = dmScript::GetLuaState(engine->m_SharedScriptContext);
            if (!dmGameSystem::InitializeScriptLibs(script_lib_context))
                goto bail;
        } else {
            script_lib_context.m_LuaState = dmScript::GetLuaState(engine->m_GOScriptContext);
            if (!dmGameSystem::InitializeScriptLibs(script_lib_context))
                goto bail;
            script_lib_context.m_LuaState = dmScript::GetLuaState(engine->m_GuiScriptContext);
            if (!dmGameSystem::InitializeScriptLibs(script_lib_context))
                goto bail;
        }

        dmLiveUpdate::Initialize(engine->m_Factory);

        fact_result = dmResource::Get(engine->m_Factory, dmConfigFile::GetString(engine->m_Config, "bootstrap.main_collection", "/logic/main.collectionc"), (void**) &engine->m_MainCollection);
        if (fact_result != dmResource::RESULT_OK)
            goto bail;
        dmGameObject::Init(engine->m_MainCollection);

        engine->m_LastReloadMTime = 0;

#if defined(__NX__)
        // there's no way to check for a named mount, and it will assert
        // So we'll only enter here if it's set on this platform
        if (has_host_mount)
#endif
        {
            const char* mountstr = has_host_mount ? "host:/" : "";
            char path[512];
            dmSnPrintf(path, sizeof(path), "%sbuild/default/content/reload", mountstr);
            struct stat file_stat;
            if (stat(path, &file_stat) == 0)
            {
                engine->m_LastReloadMTime = (uint32_t) file_stat.st_mtime;
            }
        }

        // Tbh, I have never heard of this feature of reordering the component types being utilized.
        // I vote for removing it. /MAWE
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
            int display_cutout = dmConfigFile::GetInt(engine->m_Config, "android.display_cutout", 1);
            _glfwAndroidSetFullscreenParameters(immersive_mode, display_cutout);
        }
#endif

        if (engine->m_EngineService)
        {
            dmEngineService::InitProfiler(engine->m_EngineService, engine->m_Factory, engine->m_Register);
        }

        engine->m_PreviousFrameTime = dmTime::GetTime();

        return true;

bail:
        return false;
    }

    static void GOActionCallback(dmhash_t action_id, dmInput::Action* action, void* user_data)
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
        input_action.m_GamepadDisconnected = action->m_GamepadDisconnected;
        input_action.m_GamepadConnected = action->m_GamepadConnected;
        input_action.m_GamepadPacket = action->m_GamepadPacket;
        input_action.m_HasGamepadPacket = action->m_HasGamepadPacket;

        input_buffer->Push(input_action);
    }

    uint16_t GetHttpPort(HEngine engine)
    {
        if (engine->m_EngineService)
        {
            return dmEngineService::GetPort(engine->m_EngineService);
        }
        return 0;
    }

    static int InputBufferOrderSort(const void * a, const void * b)
    {
        dmGameObject::InputAction *ipa = (dmGameObject::InputAction *)a;
        dmGameObject::InputAction *ipb = (dmGameObject::InputAction *)b;
        bool a_is_text = ipa->m_HasText;
        bool b_is_text = ipb->m_HasText;
        return a_is_text - b_is_text;
    }

    static uint32_t GetLuaMemCount(HEngine engine)
    {
        uint32_t memcount = 0;
        if (engine->m_SharedScriptContext) {
            memcount += dmScript::GetLuaGCCount(dmScript::GetLuaState(engine->m_SharedScriptContext));
        } else {
            memcount += dmScript::GetLuaGCCount(dmScript::GetLuaState(engine->m_GOScriptContext));
            memcount += dmScript::GetLuaGCCount(dmScript::GetLuaState(engine->m_GuiScriptContext));
        }
        return memcount;
    }

    static void StepFrame(HEngine engine, float dt)
    {
        dmProfiler::SetUpdateFrequency((uint32_t)(1.0f / dt));

        if (dmGraphics::GetWindowState(engine->m_GraphicsContext, dmGraphics::WINDOW_STATE_ICONIFIED))
        {
            if (!engine->m_WasIconified)
            {
                engine->m_WasIconified = true;

                if (!engine->m_RunWhileIconified)
                {
                    dmSound::Pause(true);
                }
            }

            if (!engine->m_RunWhileIconified) {
                // NOTE: Polling the event queue is crucial on iOS for life-cycle management
                // NOTE: Also running graphics on iOS while transitioning is not permitted and will crash the application
                dmHID::Update(engine->m_HidContext);
                dmTime::Sleep(1000 * 100);
                return;
            }
        }
        else
        {
            if (engine->m_WasIconified)
            {
                engine->m_WasIconified = false;

                dmSound::Pause(false);
            }
        }

        dmProfile::HProfile profile = dmProfile::BeginFrame();
        {
            DM_PROFILE("Frame");

            {
                DM_PROFILE("Sim");

                dmLiveUpdate::Update();

                {
                    DM_PROFILE("Resource");
                    dmResource::UpdateFactory(engine->m_Factory);
                }

                {
                    DM_PROFILE("Hid");
                    dmHID::Update(engine->m_HidContext);
                }
                if (!engine->m_RunWhileIconified) {
                    if (dmGraphics::GetWindowState(engine->m_GraphicsContext, dmGraphics::WINDOW_STATE_ICONIFIED))
                    {
                        // NOTE: This is a bit ugly but os event are polled in dmHID::Update and an iOS application
                        // might have entered background at this point and OpenGL calls are not permitted and will
                        // crash the application
                        dmProfile::EndFrame(profile);
                        return;
                    }
                }
                {
                    DM_PROFILE("Script");

                    // Script context updates
                    if (engine->m_SharedScriptContext) {
                        dmScript::Update(engine->m_SharedScriptContext);
                    } else {
                        if (engine->m_GOScriptContext) {
                            dmScript::Update(engine->m_GOScriptContext);
                        }
                        if (engine->m_RenderScriptContext) {
                            dmScript::Update(engine->m_RenderScriptContext);
                        }
                        if (engine->m_GuiScriptContext) {
                            dmScript::Update(engine->m_GuiScriptContext);
                        }
                    }
                }

                dmSound::Update();

                bool esc_pressed = false;
                if (engine->m_QuitOnEsc)
                {
                    dmHID::KeyboardPacket keybdata;
                    dmHID::HKeyboard keyboard = dmHID::GetKeyboard(engine->m_HidContext, 0);
                    dmHID::GetKeyboardPacket(keyboard, &keybdata);
                    esc_pressed = dmHID::GetKey(&keybdata, dmHID::KEY_ESC);
                }

                if (esc_pressed || !dmGraphics::GetWindowState(engine->m_GraphicsContext, dmGraphics::WINDOW_STATE_OPENED))
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
                update_context.m_TimeScale = 1.0f;
                update_context.m_DT = dt;
                update_context.m_FixedUpdateFrequency = engine->m_FixedUpdateFrequency;
                update_context.m_AccumFrameTime = engine->m_AccumFrameTime;
                dmGameObject::Update(engine->m_MainCollection, &update_context);

                // Don't render while iconified
                if (!dmGraphics::GetWindowState(engine->m_GraphicsContext, dmGraphics::WINDOW_STATE_ICONIFIED))
                {
                    // Call pre render functions for extensions, if available.
                    // We do it here before we render rest of the frame
                    // if any extension wants to render on under of the game.
                    dmExtension::Params ext_params;
                    ext_params.m_ConfigFile = engine->m_Config;
                    if (engine->m_SharedScriptContext) {
                        ext_params.m_L = dmScript::GetLuaState(engine->m_SharedScriptContext);
                    } else {
                        ext_params.m_L = dmScript::GetLuaState(engine->m_GOScriptContext);
                    }
                    dmExtension::PreRender(&ext_params);

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

                    dmGraphics::BeginFrame(engine->m_GraphicsContext);

                    if (engine->m_RenderScriptPrototype)
                    {
                        dmRender::UpdateRenderScriptInstance(engine->m_RenderScriptPrototype->m_Instance, dt);
                    }
                    else
                    {
                        dmGraphics::SetViewport(engine->m_GraphicsContext, 0, 0, dmGraphics::GetWindowWidth(engine->m_GraphicsContext), dmGraphics::GetWindowHeight(engine->m_GraphicsContext));
                        dmGraphics::Clear(engine->m_GraphicsContext, dmGraphics::BUFFER_TYPE_COLOR0_BIT | dmGraphics::BUFFER_TYPE_DEPTH_BIT | dmGraphics::BUFFER_TYPE_STENCIL_BIT,
                                            (float)((engine->m_ClearColor>> 0)&0xFF),
                                            (float)((engine->m_ClearColor>> 8)&0xFF),
                                            (float)((engine->m_ClearColor>>16)&0xFF),
                                            (float)((engine->m_ClearColor>>24)&0xFF),
                                            1.0f, 0);
                        dmRender::DrawRenderList(engine->m_RenderContext, 0x0, 0x0, 0x0);
                    }
                }

                dmGameObject::PostUpdate(engine->m_MainCollection);
                dmGameObject::PostUpdate(engine->m_Register);

                dmRender::ClearRenderObjects(engine->m_RenderContext);


                dmMessage::Dispatch(engine->m_SystemSocket, Dispatch, engine);
            } // Sim

            DM_PROPERTY_SET_U32(rmtp_LuaRefs, dmScript::GetLuaRefCount());
            DM_PROPERTY_SET_U32(rmtp_LuaMem, GetLuaMemCount(engine));

            if (dLib::IsDebugMode())
            {
                // We had buffering problems with the output when running the engine inside the editor
                // Flushing stdout/stderr solves this problem.
                fflush(stdout);
                fflush(stderr);
            }

            if (engine->m_EngineService)
            {
                dmEngineService::Update(engine->m_EngineService, profile);
            }

            dmProfiler::RenderProfiler(profile, engine->m_GraphicsContext, engine->m_RenderContext, engine->m_SystemFontMap);

            // Call post render functions for extensions, if available.
            // We do it here at the end of the frame (before swap buffers/flip)
            // in case any extension wants to render just before the Flip().
            // Don't do this while iconified
            if (!dmGraphics::GetWindowState(engine->m_GraphicsContext, dmGraphics::WINDOW_STATE_ICONIFIED))
            {
                dmExtension::Params ext_params;
                ext_params.m_ConfigFile = engine->m_Config;
                if (engine->m_SharedScriptContext) {
                    ext_params.m_L = dmScript::GetLuaState(engine->m_SharedScriptContext);
                } else {
                    ext_params.m_L = dmScript::GetLuaState(engine->m_GOScriptContext);
                }
                dmExtension::PostRender(&ext_params);
            }

            dmGraphics::Flip(engine->m_GraphicsContext);

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
        dmProfile::EndFrame(profile);

        ++engine->m_Stats.m_FrameCount;
        engine->m_Stats.m_TotalTime += dt;
    }

    static void CalcTimeStep(HEngine engine, float& step_dt, uint32_t& num_steps)
    {
        uint64_t time = dmTime::GetTime();
        uint64_t frame_time = time - engine->m_PreviousFrameTime; // The actual time between two engine frames
        engine->m_PreviousFrameTime = time;

        float frame_dt = (float)(frame_time / 1000000.0);

        // Never allow for large hitches
        if (frame_dt > 0.5f) {
            frame_dt = 0.5f;
        }

        // Variable frame rate
        if (engine->m_UpdateFrequency == 0)
        {
            step_dt = frame_dt;
            num_steps = 1;
            return;
        }

        // Fixed frame rate
        float fixed_dt = 1.0f / (float)engine->m_UpdateFrequency;

        // We don't allow having a higher framerate than the actual variable frame rate
        // since the update+render is currently coupled together and also Flip() would be called more than once.
        // E.g. if the fixed_dt == 1/120 and the frame_dt == 1/60
        if (fixed_dt < frame_dt)
        {
            fixed_dt = frame_dt;
        }

        engine->m_AccumFrameTime += frame_dt;

        float num_steps_f = engine->m_AccumFrameTime / fixed_dt;

        num_steps = (uint32_t)num_steps_f;
        step_dt = fixed_dt;

        engine->m_AccumFrameTime = engine->m_AccumFrameTime - num_steps * fixed_dt;
    }

    void Step(HEngine engine)
    {
        DM_PROFILE("Step");
        engine->m_Alive = true;
        engine->m_RunResult.m_ExitCode = 0;
        engine->m_RunResult.m_Action = dmEngine::RunResult::NONE;

        float step_dt;      // The dt for each step (the game frame)
        uint32_t num_steps; // Number of times to loop over the StepFrame function

        CalcTimeStep(engine, step_dt, num_steps);

        for (uint32_t i = 0; i < num_steps; ++i)
        {
            // We currently cannot separate the update from the render,
            // since some of the update is done in the render updates (e.g. sprite transforms)
            StepFrame(engine, step_dt);

            if (!engine->m_Alive)
                break;
        }

    }

    static int IsRunning(void* context)
    {
        HEngine engine = (HEngine)context;
        return engine->m_Alive;
    }

    static void Exit(HEngine engine, int32_t code)
    {
        engine->m_Alive = false;
        engine->m_RunResult.m_ExitCode = code;
        engine->m_RunResult.m_Action = dmEngine::RunResult::EXIT;
    }

    static void Reboot(HEngine engine, dmSystemDDF::Reboot* reboot)
    {
        int argc = 0;
        engine->m_RunResult.m_Argv[argc++] = strdup("dmengine");

        // This value should match the count in dmSystemDDF::Reboot
        const int ARG_COUNT = 6;
        char* args[ARG_COUNT] =
        {
            reboot->m_Arg1 ? strdup(reboot->m_Arg1) : 0,
            reboot->m_Arg2 ? strdup(reboot->m_Arg2) : 0,
            reboot->m_Arg3 ? strdup(reboot->m_Arg3) : 0,
            reboot->m_Arg4 ? strdup(reboot->m_Arg4) : 0,
            reboot->m_Arg5 ? strdup(reboot->m_Arg5) : 0,
            reboot->m_Arg6 ? strdup(reboot->m_Arg6) : 0,
        };

        for (int i = 0; i < ARG_COUNT; ++i)
        {
            // NOTE: +1 here, see above
            engine->m_RunResult.m_Argv[i + 1] = args[i];
            if (args[i] == 0 || args[i][0] == '\0')
            {
                break;
            }

            argc++;
        }

        engine->m_RunResult.m_Argc = argc;

        engine->m_Alive = false;
        engine->m_RunResult.m_Action = dmEngine::RunResult::REBOOT;
    }

    static void Dispatch(dmMessage::Message* message, void* user_ptr)
    {
        Engine* self = (Engine*) user_ptr;

        if (message->m_Descriptor != 0)
        {
            dmDDF::Descriptor* descriptor = (dmDDF::Descriptor*)message->m_Descriptor;

            dmDDF::ResolvePointers(descriptor, message->m_Data);

            if (descriptor == dmSystemDDF::Exit::m_DDFDescriptor)
            {
                dmSystemDDF::Exit* ddf = (dmSystemDDF::Exit*) message->m_Data;
                dmEngine::Exit(self, ddf->m_Code);
            }
            else if (descriptor == dmSystemDDF::Reboot::m_DDFDescriptor)
            {
                dmSystemDDF::Reboot* reboot = (dmSystemDDF::Reboot*) message->m_Data;
                dmEngine::Reboot(self, reboot);
            }
            else if (descriptor == dmSystemDDF::ToggleProfile::m_DDFDescriptor) // "toogle_profile"
            {
                dmProfiler::ToggleProfiler();
            }
            else if (descriptor == dmSystemDDF::TogglePhysicsDebug::m_DDFDescriptor) // "toggle_physics"
            {
                if(dLib::IsDebugMode())
                {
                    self->m_PhysicsContext.m_Debug = !self->m_PhysicsContext.m_Debug;
                }
            }
            else if (descriptor == dmSystemDDF::StartRecord::m_DDFDescriptor) // "start_record"
            {
                dmSystemDDF::StartRecord* start_record = (dmSystemDDF::StartRecord*) message->m_Data;
                RecordData* record_data = &self->m_RecordData;

                record_data->m_FramePeriod = start_record->m_FramePeriod;

                uint32_t width = dmGraphics::GetWidth(self->m_GraphicsContext);
                uint32_t height = dmGraphics::GetHeight(self->m_GraphicsContext);
                dmRecord::NewParams params;
                params.m_Width = width;
                params.m_Height = height;
                params.m_Filename = start_record->m_FileName;
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
            else if (descriptor == dmSystemDDF::StopRecord::m_DDFDescriptor) // "stop_record"
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
            else if (descriptor == dmSystemDDF::SetUpdateFrequency::m_DDFDescriptor) // "set_update_frequency"
            {
                dmSystemDDF::SetUpdateFrequency* m = (dmSystemDDF::SetUpdateFrequency*) message->m_Data;
                SetUpdateFrequency(self, (uint32_t) m->m_Frequency);
            }
            else if (descriptor == dmEngineDDF::HideApp::m_DDFDescriptor) // "hide_app"
            {
                dmGraphics::IconifyWindow(self->m_GraphicsContext);
            }
            else if (descriptor == dmSystemDDF::SetVsync::m_DDFDescriptor) // "set_vsync"
            {
                dmSystemDDF::SetVsync* m = (dmSystemDDF::SetVsync*) message->m_Data;
                SetSwapInterval(self, m->m_SwapInterval);
            }
            else if (descriptor == dmEngineDDF::RunScript::m_DDFDescriptor) // "run_script"
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

        const char* system_font_map = "/builtins/fonts/system_font.fontc";
        fact_error = dmResource::Get(engine->m_Factory, system_font_map, (void**) &engine->m_SystemFontMap);
        if (fact_error != dmResource::RESULT_OK)
        {
            dmLogFatal("Could not load system font map '%s'.", system_font_map);
            return false;
        }
        dmRender::SetSystemFontMap(engine->m_RenderContext, engine->m_SystemFontMap);

        // The system font is currently the only resource we need from the connection app
        // After this point, the rest of the resources should be loaded the ordinary way
        if (!engine->m_ConnectionAppMode)
        {
            int unload = dmConfigFile::GetInt(engine->m_Config, "dmengine.unload_builtins", 1);
            if (unload)
            {
                dmResource::ReleaseBuiltinsManifest(engine->m_Factory);
            }
        }

        const char* gamepads = dmConfigFile::GetString(config, "input.gamepads", 0);
        if (gamepads)
        {
            dmInputDDF::GamepadMaps* gamepad_maps_ddf;
            fact_error = dmResource::Get(engine->m_Factory, gamepads, (void**)&gamepad_maps_ddf);
            if (fact_error != dmResource::RESULT_OK)
                return false;
            dmInput::RegisterGamepads(engine->m_InputContext, gamepad_maps_ddf);
            dmResource::Release(engine->m_Factory, gamepad_maps_ddf);
        }

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

    void GetStats(HEngine engine, Stats& stats)
    {
        stats = engine->m_Stats;
    }
}

void dmEngineInitialize()
{
    dmThread::SetThreadName(dmThread::GetCurrentThread(), "engine_main");

#if DM_RELEASE
    dLib::SetDebugMode(false);
#endif
    dmHashEnableReverseHash(dLib::IsDebugMode());

    dmCrash::Init(dmEngineVersion::VERSION, dmEngineVersion::VERSION_SHA1);
    dmDDF::RegisterAllTypes();
    dmSocket::Initialize();
    dmSSLSocket::Initialize();
    dmMemProfile::Initialize();
    dmLog::LogParams params;
    dmLog::LogInitialize(&params);

    if (dLib::FeaturesSupported(DM_FEATURE_BIT_SOCKET_SERVER_TCP | DM_FEATURE_BIT_SOCKET_SERVER_UDP))
    {
        uint16_t engine_port = dmEngineService::GetServicePort(8001);
        dmEngine::g_EngineService = dmEngineService::New(engine_port);
    }
}

void dmEngineFinalize()
{
    if (dmEngine::g_EngineService)
    {
        dmEngineService::Delete(dmEngine::g_EngineService);
    }
    dmGraphics::Finalize();
    dmLog::LogFinalize();
    dmMemProfile::Finalize();
    dmSSLSocket::Finalize();
    dmSocket::Finalize();
}

const char* ParseArgOneOperand(const char* arg_str, int argc, char *argv[])
{
    for (int i = 0; i < argc; ++i)
    {
        const char* arg = argv[i];
        if (strncmp(arg_str, arg, sizeof(arg_str)-1) == 0)
        {
            const char* res = strchr(arg, '=');
            if (res)
            {
                return res + 1;
            }
        }
    }

    return 0;
}


dmEngine::HEngine dmEngineCreate(int argc, char *argv[])
{
    const char* arg_adapter_type = ParseArgOneOperand("--graphics-adapter", argc, argv);

    if (!dmGraphics::Initialize(arg_adapter_type))
    {
        return 0;
    }

    dmEngine::HEngine engine = dmEngine::New(dmEngine::g_EngineService);
    bool initialized = dmEngine::Init(engine, argc, argv);

    if (!initialized)
    {
        // Leave cleaning up of engine service to the finalize call
        Delete(engine);
        return 0;
    }

    return engine;
}

void dmEngineDestroy(dmEngine::HEngine engine)
{
    engine->m_RunResult.Free();

    Delete(engine);
}

static dmEngine::UpdateResult GetAppResultFromAction(int action)
{
    switch(action) {
    case dmEngine::RunResult::REBOOT:   return dmEngine::RESULT_REBOOT;
    case dmEngine::RunResult::EXIT:     return dmEngine::RESULT_EXIT;
    default:                            return dmEngine::RESULT_OK;
    }
}

dmEngine::UpdateResult dmEngineUpdate(dmEngine::HEngine engine)
{
    if (dmEngine::IsRunning(engine))
    {
        dmEngine::Step(engine);
    }
    else {
        if (engine->m_RunResult.m_Action == dmEngine::RunResult::NONE)
            return dmEngine::RESULT_EXIT;
    }

    return GetAppResultFromAction(engine->m_RunResult.m_Action);
}

void dmEngineGetResult(dmEngine::HEngine engine, int* run_action, int* exit_code, int* argc, char*** argv)
{
    if (run_action)
        *run_action = (int)GetAppResultFromAction(engine->m_RunResult.m_Action);
    if (exit_code)
        *exit_code = engine->m_RunResult.m_ExitCode;

    int _argc = engine->m_RunResult.m_Argc;
    if (argc)
        *argc = _argc;

    if (argv)
    {
        *argv = (char**)malloc(sizeof(char*) * _argc);

        for (int i = 0; i < _argc; ++i)
        {
            (*argv)[i] = strdup(engine->m_RunResult.m_Argv[i]);
        }
    }
}
