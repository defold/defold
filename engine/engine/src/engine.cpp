#include "engine.h"
#include "engine_private.h"

#include <vectormath/cpp/vectormath_aos.h>
#include <sys/stat.h>

#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/profile.h>
#include <dlib/time.h>
#include <dlib/math.h>
#include <gamesys/model_ddf.h>
#include <gamesys/physics_ddf.h>
#include <gameobject/gameobject_ddf.h>
#include <sound/sound.h>
#include <render/render.h>
#include <render/render_ddf.h>
#include <particle/particle.h>

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
        dmMessage::ResetURL(receiver);
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

        Engine* engine = (Engine*)user_data;
        engine->m_InvPhysicalWidth = 1.0f / width;
        engine->m_InvPhysicalHeight = 1.0f / height;
        // update gui context
        dmGui::SetPhysicalResolution(engine->m_GuiContext.m_GuiContext, width, height);
    }

    void Dispatch(dmMessage::Message *message_object, void* user_ptr);

    Stats::Stats()
    : m_FrameCount(0)
    {

    }

    Engine::Engine()
    : m_Config(0)
    , m_Alive(true)
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
    , m_HidContext(0x0)
    , m_InputContext(0x0)
    , m_GameInputBinding(0x0)
    , m_RenderScriptPrototype(0x0)
    , m_Stats()
    , m_Width(960)
    , m_Height(640)
    , m_InvPhysicalWidth(1.0f/960)
    , m_InvPhysicalHeight(1.0f/640)
    , m_HttpServer(0)
    {
        m_Register = dmGameObject::NewRegister();
        m_InputBuffer.SetCapacity(64);

        m_PhysicsContext.m_Context3D = 0x0;
        m_PhysicsContext.m_Debug = false;
        m_PhysicsContext.m_3D = false;
        m_EmitterContext.m_Debug = false;
        m_GuiContext.m_GuiContext = 0x0;
        m_GuiContext.m_RenderContext = 0x0;
        m_SpriteContext.m_RenderContext = 0x0;
        m_SpriteContext.m_MaxSpriteCount = 0;
    }

    HEngine New()
    {
        return new Engine();
    }

    void Delete(HEngine engine)
    {
        if (engine->m_HttpServer)
        {
            dmHttpServer::Delete(engine->m_HttpServer);
        }

        if (engine->m_MainCollection)
            dmResource::Release(engine->m_Factory, engine->m_MainCollection);

        dmGameSystem::ScriptLibContext script_lib_context;
        script_lib_context.m_Factory = engine->m_Factory;
        script_lib_context.m_Register = engine->m_Register;
        script_lib_context.m_LuaState = dmGameObject::GetLuaState();
        dmGameSystem::FinalizeScriptLibs(script_lib_context);
        if (engine->m_GuiContext.m_GuiContext != 0x0)
        {
            script_lib_context.m_LuaState = dmGui::GetLuaState(engine->m_GuiContext.m_GuiContext);
            dmGameSystem::FinalizeScriptLibs(script_lib_context);
        }

        dmGameObject::DeleteRegister(engine->m_Register);

        UnloadBootstrapContent(engine);

        dmSound::Finalize();

        dmInput::DeleteContext(engine->m_InputContext);

        dmRender::DeleteRenderContext(engine->m_RenderContext);

        if (engine->m_HidContext)
        {
            dmHID::Final(engine->m_HidContext);
            dmHID::DeleteContext(engine->m_HidContext);
        }

        dmGameObject::Finalize();

        if (engine->m_Factory)
            dmResource::DeleteFactory(engine->m_Factory);

        if (engine->m_ScriptContext)
            dmScript::DeleteContext(engine->m_ScriptContext);

        if (engine->m_GraphicsContext)
        {
            dmGraphics::CloseWindow(engine->m_GraphicsContext);
            dmGraphics::DeleteContext(engine->m_GraphicsContext);
        }

        if (engine->m_GuiContext.m_GuiContext)
            dmGui::DeleteContext(engine->m_GuiContext.m_GuiContext);
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

        if (engine->m_Config)
        {
            dmConfigFile::Delete(engine->m_Config);
        }

        delete engine;
    }

    dmGraphics::TextureFilter ConvertTextureFilter(const char* filter)
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

    static void HttpServerHeader(void* user_data, const char* key, const char* value)
    {
        (void) user_data;
        (void) key;
        (void) value;
    }

    static bool ParsePostUrl(const char* resource, dmMessage::HSocket* socket, const dmDDF::Descriptor** desc, dmhash_t* message_id)
    {
        // Syntax: http://host:port/post/socket/message_type

        char buf[256];
        dmStrlCpy(buf, resource, sizeof(buf));

        char* last;
        int i = 0;
        char* s = dmStrTok(buf, "/", &last);
        bool error = false;

        while (s && !error)
        {
            switch (i)
            {
                case 0:
                {
                    if (strcmp(s, "post") != 0)
                    {
                        error = true;
                    }
                }
                break;

                case 1:
                {
                    dmMessage::Result mr = dmMessage::GetSocket(s, socket);
                    if (mr != dmMessage::RESULT_OK)
                    {
                        error = true;
                    }
                }
                break;

                case 2:
                {
                    *message_id = dmHashString64(s);
                    *desc = dmDDF::GetDescriptorFromHash(*message_id);
                    if (*desc == 0)
                    {
                        error = true;
                    }
                }
                break;
            }

            s = dmStrTok(0, "/", &last);
            ++i;
        }

        return !error;
    }

    static void SlurpHttpContent(const dmHttpServer::Request* request)
    {
        char buf[256];
        uint32_t total_recv = 0;

        while (total_recv < request->m_ContentLength)
        {
            uint32_t recv_bytes = 0;
            uint32_t to_read = dmMath::Min((uint32_t) sizeof(buf), request->m_ContentLength - total_recv);
            dmHttpServer::Result r = dmHttpServer::Receive(request, buf, to_read, &recv_bytes);
            if (r != dmHttpServer::RESULT_OK)
                return;
            total_recv += recv_bytes;
        }
    }

    static void HandlePost(void* user_data, const dmHttpServer::Request* request)
    {
        char msg_buf[1024];
        const char* error_msg = "";
        dmHttpServer::Result r;
        uint32_t recv_bytes = 0;
        dmMessage::HSocket socket = 0;
        const dmDDF::Descriptor* desc = 0;
        dmhash_t message_id;

        if (request->m_ContentLength > sizeof(msg_buf))
        {
            error_msg = "Too large message";
            goto bail;
        }

        if (!ParsePostUrl(request->m_Resource, &socket, &desc, &message_id))
        {
            error_msg = "Invalid request";
            goto bail;
        }

        r = dmHttpServer::Receive(request, msg_buf, request->m_ContentLength, &recv_bytes);
        if (r == dmHttpServer::RESULT_OK)
        {
            void* msg;
            uint32_t msg_size;
            dmDDF::Result ddf_r = dmDDF::LoadMessage(msg_buf, recv_bytes, desc, &msg, dmDDF::OPTION_OFFSET_STRINGS, &msg_size);
            if (ddf_r == dmDDF::RESULT_OK)
            {
                dmMessage::URL url;
                url.m_Socket = socket;
                url.m_Path = 0;
                url.m_Fragment = 0;
                dmMessage::Post(0, &url, message_id, 0, (uintptr_t) desc, msg, msg_size);
                dmDDF::FreeMessage(msg);
            }
        }
        else
        {
            dmLogError("Error while reading message post data (%d)", r);
            error_msg = "Internal error";
            goto bail;
        }

        dmHttpServer::SetStatusCode(request, 200);
        dmHttpServer::Send(request, "OK", strlen("OK"));
        return;

bail:
        SlurpHttpContent(request);
        dmLogError("%s", error_msg);
        dmHttpServer::SetStatusCode(request, 400);
        dmHttpServer::Send(request, error_msg, strlen(error_msg));
    }

    static void HttpServerResponse(void* user_data, const dmHttpServer::Request* request)
    {
        if (strcmp(request->m_Method, "POST") == 0)
        {
            HandlePost(user_data, request);
        }
        else
        {
            SlurpHttpContent(request);
            dmHttpServer::SetStatusCode(request, 400);
            const char* s = "Invalid request";
            dmHttpServer::Send(request, s, strlen(s));
        }
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

        dmConfigFile::Result config_results[2];
        dmConfigFile::Result config_result = dmConfigFile::RESULT_FILE_NOT_FOUND;
        uint32_t current_project_file = 0;
        while (config_result != dmConfigFile::RESULT_OK && current_project_file < project_file_count)
        {
            config_results[current_project_file] = dmConfigFile::Load(project_files[current_project_file], argc, (const char**) argv, &engine->m_Config);
            config_result = config_results[current_project_file];
            ++current_project_file;
        }
        if (config_result != dmConfigFile::RESULT_OK)
        {
            dmLogFatal("Unable to load project file from any of the locations:");
            for (uint32_t i = 0; i < project_file_count; ++i)
                // TODO: Translate code
                dmLogFatal("Error %d: %s", config_results[i], project_files[i]);
            return false;
        }
        const char* content_root = default_content_roots[current_project_file - 1];
        const char* update_order = dmConfigFile::GetString(engine->m_Config, "gameobject.update_order", 0);

        dmProfile::Initialize(256, 1024 * 16, 128);
        // This scope is mainly here to make sure the "Main" scope is created first.
        DM_PROFILE(Engine, "Init");

        dmGraphics::ContextParams graphics_context_params;
        graphics_context_params.m_DefaultTextureMinFilter = ConvertTextureFilter(dmConfigFile::GetString(engine->m_Config, "graphics.default_texture_min_filter", "linear"));
        graphics_context_params.m_DefaultTextureMagFilter = ConvertTextureFilter(dmConfigFile::GetString(engine->m_Config, "graphics.default_texture_mag_filter", "linear"));
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
        window_params.m_Width = engine->m_Width;
        window_params.m_Height = engine->m_Height;
        window_params.m_Samples = dmConfigFile::GetInt(engine->m_Config, "display.samples", 0);
        window_params.m_Title = dmConfigFile::GetString(engine->m_Config, "project.title", "TestTitle");
        window_params.m_Fullscreen = dmConfigFile::GetInt(engine->m_Config, "display.fullscreen", 0);
        window_params.m_PrintDeviceInfo = false;

        dmGraphics::WindowResult window_result = dmGraphics::OpenWindow(engine->m_GraphicsContext, &window_params);
        if (window_result != dmGraphics::WINDOW_RESULT_OK)
        {
            dmLogFatal("Could not open window (%d).", window_result);
            return false;
        }

        uint32_t physical_width = dmGraphics::GetWindowWidth(engine->m_GraphicsContext);
        uint32_t physical_height = dmGraphics::GetWindowHeight(engine->m_GraphicsContext);
        engine->m_InvPhysicalWidth = 1.0f / physical_width;
        engine->m_InvPhysicalHeight = 1.0f / physical_height;

        engine->m_ScriptContext = dmScript::NewContext(engine->m_Config);
        dmGameObject::Initialize(engine->m_ScriptContext);

        engine->m_HidContext = dmHID::NewContext(dmHID::NewContextParams());
        dmHID::Init(engine->m_HidContext);

        dmSound::InitializeParams sound_params;
        dmSound::Initialize(engine->m_Config, &sound_params);

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
        render_params.m_ScriptContext = engine->m_ScriptContext;
        render_params.m_MaxDebugVertexCount = (uint32_t) dmConfigFile::GetInt(engine->m_Config, "graphics.max_debug_vertices", 10000);
        engine->m_RenderContext = dmRender::NewRenderContext(engine->m_GraphicsContext, render_params);

        engine->m_EmitterContext.m_RenderContext = engine->m_RenderContext;
        engine->m_EmitterContext.m_MaxEmitterCount = dmConfigFile::GetInt(engine->m_Config, dmParticle::MAX_EMITTER_COUNT_KEY, 0);
        engine->m_EmitterContext.m_MaxParticleCount = dmConfigFile::GetInt(engine->m_Config, dmParticle::MAX_PARTICLE_COUNT_KEY, 0);
        engine->m_EmitterContext.m_Debug = false;

        const uint32_t max_resources = 256;

        dmResource::NewFactoryParams params;
        int32_t http_cache = dmConfigFile::GetInt(engine->m_Config, "resource.http_cache", 1);
        params.m_MaxResources = max_resources;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT | RESOURCE_FACTORY_FLAGS_HTTP_SERVER;
        if (http_cache)
            params.m_Flags |= RESOURCE_FACTORY_FLAGS_HTTP_CACHE;
        params.m_StreamBufferSize = 8 * 1024 * 1024; // We have some *large* textures...!
        params.m_BuiltinsArchive = (const void*) BUILTINS_ARC;
        params.m_BuiltinsArchiveSize = BUILTINS_ARC_SIZE;

        engine->m_Factory = dmResource::NewFactory(&params, dmConfigFile::GetString(engine->m_Config, "resource.uri", content_root));

        dmInput::NewContextParams input_params;
        input_params.m_HidContext = engine->m_HidContext;
        input_params.m_RepeatDelay = dmConfigFile::GetFloat(engine->m_Config, "input.repeat_delay", 0.5f);
        input_params.m_RepeatInterval = dmConfigFile::GetFloat(engine->m_Config, "input.repeat_interval", 0.2f);
        engine->m_InputContext = dmInput::NewContext(input_params);

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
        gui_params.m_GetTextMetricsCallback = dmGameSystem::GuiGetTextMetricsCallback;
        gui_params.m_Width = engine->m_Width;
        gui_params.m_Height = engine->m_Height;
        gui_params.m_PhysicalWidth = physical_width;
        gui_params.m_PhysicalHeight = physical_height;
        engine->m_GuiContext.m_GuiContext = dmGui::NewContext(&gui_params);
        engine->m_GuiContext.m_RenderContext = engine->m_RenderContext;
        dmPhysics::NewContextParams physics_params;
        physics_params.m_WorldCount = dmConfigFile::GetInt(engine->m_Config, "physics.world_count", 4);
        const char* physics_type = dmConfigFile::GetString(engine->m_Config, "physics.type", "2D");
        physics_params.m_Gravity.setX(dmConfigFile::GetFloat(engine->m_Config, "physics.gravity_x", 0.0f));
        physics_params.m_Gravity.setY(dmConfigFile::GetFloat(engine->m_Config, "physics.gravity_y", -10.0f));
        physics_params.m_Gravity.setZ(dmConfigFile::GetFloat(engine->m_Config, "physics.gravity_z", 0.0f));
        physics_params.m_Scale = dmConfigFile::GetFloat(engine->m_Config, "physics.scale", 1.0f);
        if (physics_params.m_Scale < dmPhysics::MIN_SCALE || physics_params.m_Scale > dmPhysics::MAX_SCALE)
        {
            dmLogWarning("Physics scale must be in the range %.2f - %.2f and has been clamped.", dmPhysics::MIN_SCALE, dmPhysics::MAX_SCALE);
            if (physics_params.m_Scale < dmPhysics::MIN_SCALE)
                physics_params.m_Scale = dmPhysics::MIN_SCALE;
            if (physics_params.m_Scale > dmPhysics::MAX_SCALE)
                physics_params.m_Scale = dmPhysics::MAX_SCALE;
        }
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
        engine->m_PhysicsContext.m_Debug = dmConfigFile::GetInt(engine->m_Config, "physics.debug", 0);

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

        engine->m_SpriteContext.m_RenderContext = engine->m_RenderContext;
        engine->m_SpriteContext.m_MaxSpriteCount = dmConfigFile::GetInt(engine->m_Config, "sprite.max_count", 128);
        engine->m_SpriteContext.m_Subpixels = dmConfigFile::GetInt(engine->m_Config, "sprite.subpixels", 1);

        dmResource::Result fact_result;
        dmGameObject::Result res;
        dmGameSystem::ScriptLibContext script_lib_context;

        fact_result = dmGameObject::RegisterResourceTypes(engine->m_Factory, engine->m_Register);
        if (fact_result != dmResource::RESULT_OK)
            goto bail;
        fact_result = dmGameSystem::RegisterResourceTypes(engine->m_Factory, engine->m_RenderContext, &engine->m_GuiContext, engine->m_InputContext, &engine->m_PhysicsContext);
        if (fact_result != dmResource::RESULT_OK)
            goto bail;

        if (dmGameObject::RegisterComponentTypes(engine->m_Factory, engine->m_Register) != dmGameObject::RESULT_OK)
            goto bail;

        res = dmGameSystem::RegisterComponentTypes(engine->m_Factory, engine->m_Register, engine->m_RenderContext, &engine->m_PhysicsContext, &engine->m_EmitterContext, &engine->m_GuiContext, &engine->m_SpriteContext);
        if (res != dmGameObject::RESULT_OK)
            goto bail;

        if (!LoadBootstrapContent(engine, engine->m_Config))
        {
            dmLogWarning("Unable to load bootstrap data.");
            goto bail;
        }

        if (engine->m_RenderScriptPrototype)
            InitRenderScriptInstance(engine->m_RenderScriptPrototype->m_Instance);

        script_lib_context.m_Factory = engine->m_Factory;
        script_lib_context.m_Register = engine->m_Register;
        script_lib_context.m_LuaState = dmGameObject::GetLuaState();
        if (!dmGameSystem::InitializeScriptLibs(script_lib_context))
            goto bail;
        script_lib_context.m_LuaState = dmGui::GetLuaState(engine->m_GuiContext.m_GuiContext);
        if (!dmGameSystem::InitializeScriptLibs(script_lib_context))
            goto bail;

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
                uint32_t type;
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


        {
            dmHttpServer::NewParams http_server_params;
            http_server_params.m_HttpHeader = HttpServerHeader;
            http_server_params.m_HttpResponse = HttpServerResponse;

            dmHttpServer::Result hsr = dmHttpServer::New(&http_server_params, 0, &engine->m_HttpServer);
            if (hsr != dmHttpServer::RESULT_OK)
                return false;

            dmSocket::Address address;
            dmHttpServer::GetName(engine->m_HttpServer, &address, &engine->m_HttpPort);
        }

        return true;

bail:
        return false;
    }

    void GOActionCallback(dmhash_t action_id, dmInput::Action* action, void* user_data)
    {
        Engine* engine = (Engine*)user_data;
        dmArray<dmGameObject::InputAction>* input_buffer = &engine->m_InputBuffer;
        dmGameObject::InputAction input_action;
        input_action.m_ActionId = action_id;
        input_action.m_Value = action->m_Value;
        input_action.m_Pressed = action->m_Pressed;
        input_action.m_Released = action->m_Released;
        input_action.m_Repeated = action->m_Repeated;
        input_action.m_PositionSet = action->m_PositionSet;
        float width_ratio = engine->m_InvPhysicalWidth * engine->m_Width;
        float height_ratio = engine->m_InvPhysicalHeight * engine->m_Height;
        input_action.m_X = (action->m_X + 0.5f) * width_ratio;
        input_action.m_Y = engine->m_Height - (action->m_Y + 0.5f) * height_ratio;
        input_action.m_DX = action->m_DX * width_ratio;
        input_action.m_DY = -action->m_DY * height_ratio;
        input_buffer->Push(input_action);
    }

    uint16_t GetHttpPort(HEngine engine)
    {
        return engine->m_HttpPort;
    }

    RunResult Run(HEngine engine)
    {
        const float fps = 60.0f;
        float fixed_dt = 1.0f / fps;

        engine->m_Alive = true;
        engine->m_RunResult.m_ExitCode = 0;

        while (engine->m_Alive)
        {
            dmProfile::HProfile profile = dmProfile::Begin();
            {
                DM_PROFILE(Engine, "Frame");

                // We had buffering problems with the output when running the engine inside the editor
                // Flushing stdout/stderr solves this problem.
                fflush(stdout);
                fflush(stderr);

                dmHttpServer::Update(engine->m_HttpServer);

                {
                    DM_PROFILE(Engine, "Sim");

                    dmResource::UpdateFactory(engine->m_Factory);

                    dmHID::Update(engine->m_HidContext);
                    dmSound::Update();

                    dmHID::KeyboardPacket keybdata;
                    dmHID::GetKeyboardPacket(engine->m_HidContext, &keybdata);

                    if (dmHID::GetKey(&keybdata, dmHID::KEY_ESC) || !dmGraphics::GetWindowState(engine->m_GraphicsContext, dmGraphics::WINDOW_STATE_OPENED))
                    {
                        engine->m_Alive = false;
                        break;
                    }

                    dmInput::UpdateBinding(engine->m_GameInputBinding, fixed_dt);

                    engine->m_InputBuffer.SetSize(0);
                    dmInput::ForEachActive(engine->m_GameInputBinding, GOActionCallback, engine);
                    dmArray<dmGameObject::InputAction>& input_buffer = engine->m_InputBuffer;
                    uint32_t input_buffer_size = input_buffer.Size();
                    if (input_buffer_size > 0)
                    {
                        dmGameObject::DispatchInput(engine->m_MainCollection, &input_buffer[0], input_buffer.Size());
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
                        dmRender::Draw(engine->m_RenderContext, 0x0, 0x0);
                    }

                    dmGameObject::PostUpdate(engine->m_MainCollection);

                    dmRender::ClearRenderObjects(engine->m_RenderContext);

                    dmMessage::Dispatch(engine->m_SystemSocket, Dispatch, engine);
                }

                if (engine->m_ShowProfile)
                {
                    DM_PROFILE(Profile, "Draw");
                    dmProfile::Pause(true);
                    dmProfileRender::Draw(profile, engine->m_RenderContext, engine->m_SystemFontMap);
                    dmRender::SetViewMatrix(engine->m_RenderContext, Matrix4::identity());
                    dmRender::SetProjectionMatrix(engine->m_RenderContext, Matrix4::orthographic(0.0f, dmGraphics::GetWindowWidth(engine->m_GraphicsContext), 0.0f, dmGraphics::GetWindowHeight(engine->m_GraphicsContext), 1.0f, -1.0f));
                    dmRender::Draw(engine->m_RenderContext, 0x0, 0x0);
                    dmRender::ClearRenderObjects(engine->m_RenderContext);
                    dmProfile::Pause(false);
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
            dmProfile::Release(profile);

            ++engine->m_Stats.m_FrameCount;
        }
        return engine->m_RunResult;
    }

    static void Exit(HEngine engine, int32_t code)
    {
        engine->m_Alive = false;
        engine->m_RunResult.m_ExitCode = code;
    }

    static void Reboot(HEngine engine, dmEngineDDF::Reboot* reboot)
    {
#define RELOCATE_STRING(field) (const char*) ((uintptr_t) reboot + (uintptr_t) reboot->field)

        int argc = 0;
        engine->m_RunResult.m_Argv[argc++] = strdup("dmengine");

        const int MAX_ARGS = 6;
        char* args[MAX_ARGS] =
        {
            strdup(RELOCATE_STRING(m_Arg1)),
            strdup(RELOCATE_STRING(m_Arg2)),
            strdup(RELOCATE_STRING(m_Arg3)),
            strdup(RELOCATE_STRING(m_Arg4)),
            strdup(RELOCATE_STRING(m_Arg5)),
            strdup(RELOCATE_STRING(m_Arg6)),
        };

        for (int i = 0; i < MAX_ARGS; ++i)
        {
            if (args[i][0] != '\0')
            {
                engine->m_RunResult.m_Argv[argc++] = args[i];
            }
        }

        engine->m_RunResult.m_Argc = argc;

#undef RELOCATE_STRING
        engine->m_Alive = false;
        engine->m_RunResult.m_Action = dmEngine::RunResult::REBOOT;
    }

    static RunResult InitRun(int argc, char *argv[], PreRun pre_run, PostRun post_run, void* context)
    {
        dmEngine::HEngine engine = dmEngine::New();
        dmEngine::RunResult run_result;
        if (dmEngine::Init(engine, argc, argv))
        {
            if (pre_run)
            {
                pre_run(engine, context);
            }
            run_result = dmEngine::Run(engine);

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
        dmEngine::RunResult run_result = InitRun(argc, argv, pre_run, post_run, context);
        while (run_result.m_Action == dmEngine::RunResult::REBOOT)
        {
            run_result = InitRun(run_result.m_Argc, run_result.m_Argv, pre_run, post_run, context);
            run_result.Free();
        }
        return run_result.m_ExitCode;
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
            else if (descriptor == dmEngineDDF::Reboot::m_DDFDescriptor)
            {
                dmEngineDDF::Reboot* reboot = (dmEngineDDF::Reboot*) message->m_Data;
                dmEngine::Reboot(self, reboot);
            }
            else if (descriptor == dmEngineDDF::ToggleProfile::m_DDFDescriptor)
            {
                self->m_ShowProfile = !self->m_ShowProfile;
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
                const char* file_name = (const char*) ((uintptr_t) start_record + (uintptr_t) start_record->m_FileName);

                params.m_Filename = file_name;

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

    bool LoadBootstrapContent(HEngine engine, dmConfigFile::HConfig config)
    {
        const char* system_font_map = "/builtins/fonts/system_font.fontc";
        dmResource::Result fact_error = dmResource::Get(engine->m_Factory, system_font_map, (void**) &engine->m_SystemFontMap);
        if (fact_error != dmResource::RESULT_OK)
        {
            dmLogFatal("Could not load system font map '%s'.", system_font_map);
            return false;
        }
        dmRender::SetSystemFontMap(engine->m_RenderContext, engine->m_SystemFontMap);

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

