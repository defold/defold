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

#include <gui/gui.h>

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
        char buf[sizeof(dmGameObject::InstanceMessageData) + sizeof(dmRenderDDF::WindowResized)];

        dmGameObject::InstanceMessageData* msg = (dmGameObject::InstanceMessageData*) buf;
        msg->m_BufferSize = sizeof(dmRenderDDF::WindowResized);
        msg->m_DDFDescriptor = dmRenderDDF::WindowResized::m_DDFDescriptor;
        msg->m_MessageId = dmHashString64("window_resized");

        dmRenderDDF::WindowResized* window_resized = (dmRenderDDF::WindowResized*) (buf + sizeof(dmGameObject::InstanceMessageData));
        window_resized->m_Width = width;
        window_resized->m_Height = height;

        dmMessage::HSocket socket_id = dmMessage::GetSocket("render");
        dmMessage::Post(socket_id, msg->m_MessageId, buf, sizeof(buf));
    }

    void Dispatch(dmMessage::Message *message_object, void* user_ptr);
    void DispatchRenderScript(dmMessage::Message *message_object, void* user_ptr);

    Stats::Stats()
    : m_FrameCount(0)
    {

    }

    Engine::Engine()
    : m_Alive(true)
    , m_MainCollection(0)
    , m_Collections()
    , m_ActiveCollection(0)
    , m_SpawnCount(0)
    , m_LastReloadMTime(0)
    , m_MouseSensitivity(1.0f)
    , m_ShowProfile(false)
    , m_WarpTimeStep(false)
    , m_TimeStepFactor(1.0f)
    , m_TimeStepMode(dmEngineDDF::TIME_STEP_MODE_DISCRETE)
    , m_GraphicsContext(0)
    , m_RenderContext(0)
    , m_Factory(0x0)
    , m_Font(0x0)
    , m_SmallFont(0x0)
    , m_DebugMaterial(0)
    , m_InputContext(0x0)
    , m_GameInputBinding(0x0)
    , m_RenderScriptPrototype(0x0)
    , m_Stats()
    {
        m_Register = dmGameObject::NewRegister(Dispatch, this);
        m_Collections.SetCapacity(16, 32);
        m_InputBuffer.SetCapacity(64);

        m_PhysicsContext.m_Debug = false;
        m_EmitterContext.m_Debug = false;
    }

    HEngine New()
    {
        return new Engine();
    }

    void DeleteCollection(Engine* context, const uint32_t* key, dmGameObject::HCollection* collection)
    {
        dmResource::Release(context->m_Factory, *collection);
    }

    void Delete(HEngine engine)
    {
        engine->m_Collections.Iterate<Engine>(DeleteCollection, engine);
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

        if (engine->m_GraphicsContext)
            dmGraphics::DeleteContext(engine->m_GraphicsContext);

        dmProfile::Finalize();

        delete engine;
    }

    bool Init(HEngine engine, int argc, char *argv[])
    {
        const char* project_file;

        if (argc > 1 && argv[argc-1][0] != '-')
            project_file = argv[argc-1];
        else
            project_file = "build/default/content/game.projectc";

        dmConfigFile::HConfig config;
        dmConfigFile::Result config_result = dmConfigFile::Load(project_file, argc, (const char**) argv, &config);
        if (config_result != dmConfigFile::RESULT_OK)
        {
            dmLogFatal("Unable to load %s", project_file);
            return false;
        }
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

        dmGameObject::Initialize();

        RegisterDDFTypes();

        dmHID::Initialize();

        dmSound::InitializeParams sound_params;
        dmSound::Initialize(config, &sound_params);

        dmRender::RenderContextParams render_params;
        render_params.m_DispatchCallback = DispatchRenderScript;
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
        params.m_MaxResources = max_resources;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT | RESOURCE_FACTORY_FLAGS_HTTP_SERVER;
        params.m_StreamBufferSize = 8 * 1024 * 1024; // We have some *large* textures...!

        engine->m_Factory = dmResource::NewFactory(&params, dmConfigFile::GetString(config, "resource.uri", "build/default/content"));

        dmPhysics::SetDebugRenderer(engine->m_RenderContext, PhysicsDebugRender::RenderLine);

        float repeat_delay = dmConfigFile::GetFloat(config, "input.repeat_delay", 0.5f);
        float repeat_interval = dmConfigFile::GetFloat(config, "input.repeat_interval", 0.2f);
        engine->m_InputContext = dmInput::NewContext(repeat_delay, repeat_interval);

        dmResource::FactoryResult fact_result;
        dmGameObject::Result res;

        fact_result = dmGameObject::RegisterResourceTypes(engine->m_Factory, engine->m_Register);
        if (fact_result != dmResource::FACTORY_RESULT_OK)
            goto bail;
        fact_result = dmGameSystem::RegisterResourceTypes(engine->m_Factory, engine->m_RenderContext);
        if (fact_result != dmResource::FACTORY_RESULT_OK)
            goto bail;

        if (dmGameObject::RegisterComponentTypes(engine->m_Factory, engine->m_Register) != dmGameObject::RESULT_OK)
            goto bail;

        res = dmGameSystem::RegisterComponentTypes(engine->m_Factory, engine->m_Register, engine->m_RenderContext, &engine->m_PhysicsContext, &engine->m_EmitterContext);
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

    void Reload(HEngine engine)
    {
        struct stat file_stat;
        if (stat("build/default/content/reload", &file_stat) == 0 && engine->m_LastReloadMTime != (uint32_t) file_stat.st_mtime)
        {
            engine->m_LastReloadMTime = (uint32_t) file_stat.st_mtime;
            ReloadResources(engine, "scriptc");
            ReloadResources(engine, "emitterc");
            ReloadResources(engine, "render_scriptc");
        }
    }

    void ReloadResources(HEngine engine, const char* extension)
    {
        uint32_t type;
        dmResource::FactoryResult r;
        r = dmResource::GetTypeFromExtension(engine->m_Factory, extension, &type);
        assert(r == dmResource::FACTORY_RESULT_OK);
        r = dmResource::ReloadType(engine->m_Factory, type);
        if (r != dmResource::FACTORY_RESULT_OK)
        {
            dmLogWarning("Failed to reload resources with extension \"%s\".", extension);
        }
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

        float accumulated_time = 0.0f;

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
                Reload(engine);

                dmHID::Update();
                dmSound::Update();

                dmHID::KeyboardPacket keybdata;
                dmHID::GetKeyboardPacket(&keybdata);

                if (dmHID::GetKey(&keybdata, dmHID::KEY_ESC) || !dmGraphics::GetWindowState(engine->m_GraphicsContext, dmGraphics::WINDOW_STATE_OPENED))
                {
                    engine->m_Alive = false;
                    break;
                }

                float fixed_dt = 1.0f / fps;
                float dt = fixed_dt;

                dmInput::UpdateBinding(engine->m_GameInputBinding, fixed_dt);

                if (engine->m_WarpTimeStep)
                {
                    float warped_dt = dt * engine->m_TimeStepFactor;
                    switch (engine->m_TimeStepMode)
                    {
                    case dmEngineDDF::TIME_STEP_MODE_CONTINUOUS:
                        dt = warped_dt;
                        break;
                    case dmEngineDDF::TIME_STEP_MODE_DISCRETE:
                        accumulated_time += warped_dt;
                        if (accumulated_time >= fixed_dt)
                        {
                            dt = fixed_dt + accumulated_time - fixed_dt;
                            accumulated_time = 0.0f;
                        }
                        else
                        {
                            dt = 0.0f;
                        }
                        break;
                    default:
                        break;
                    }
                }

                engine->m_InputBuffer.SetSize(0);
                dmInput::ForEachActive(engine->m_GameInputBinding, GOActionCallback, &engine->m_InputBuffer);
                if (engine->m_InputBuffer.Size() > 0)
                {
                    dmGameObject::HCollection collections[2] = {engine->m_MainCollection, engine->m_ActiveCollection};
                    dmGameObject::DispatchInput(collections, 2, &engine->m_InputBuffer[0], engine->m_InputBuffer.Size());
                }

                dmGameObject::UpdateContext update_contexts[2];
                update_contexts[0].m_DT = dt;
                update_contexts[1].m_DT = fixed_dt;
                dmGameObject::HCollection collections[2] = {engine->m_ActiveCollection, engine->m_MainCollection};
                dmGameObject::Update(collections, update_contexts, 2);

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

                dmGameObject::PostUpdate(collections, 2);

                dmRender::ClearRenderObjects(engine->m_RenderContext);
            }

            dmProfile::Pause(true);
            if (engine->m_ShowProfile)
            {
                dmProfileRender::Draw(profile, engine->m_RenderContext, engine->m_SmallFont);
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

    void Dispatch(dmMessage::Message *message_object, void* user_ptr)
    {
        Engine* self = (Engine*) user_ptr;
        dmGameObject::InstanceMessageData* instance_message_data = (dmGameObject::InstanceMessageData*) message_object->m_Data;

        if (instance_message_data->m_DDFDescriptor == dmEngineDDF::Exit::m_DDFDescriptor)
        {
            dmEngineDDF::Exit* ddf = (dmEngineDDF::Exit*) instance_message_data->m_Buffer;
            dmEngine::Exit(self, ddf->m_Code);
        }
        else if (instance_message_data->m_DDFDescriptor == dmGameObjectDDF::LoadCollection::m_DDFDescriptor)
        {
            dmGameObjectDDF::LoadCollection* ll = (dmGameObjectDDF::LoadCollection*) instance_message_data->m_Buffer;
            ll->m_Collection = (const char*)((uintptr_t)ll + (uintptr_t)ll->m_Collection);
            dmEngine::LoadCollection(self, ll->m_Collection);
        }
        else if (instance_message_data->m_DDFDescriptor == dmGameObjectDDF::UnloadCollection::m_DDFDescriptor)
        {
            dmGameObjectDDF::UnloadCollection* ll = (dmGameObjectDDF::UnloadCollection*) instance_message_data->m_Buffer;
            ll->m_Collection = (const char*)((uintptr_t)ll + (uintptr_t)ll->m_Collection);
            dmEngine::UnloadCollection(self, ll->m_Collection);
        }
        else if (instance_message_data->m_DDFDescriptor == dmGameObjectDDF::ActivateCollection::m_DDFDescriptor)
        {
            dmGameObjectDDF::ActivateCollection* ddf = (dmGameObjectDDF::ActivateCollection*) instance_message_data->m_Buffer;
            ddf->m_Collection = (const char*)((uintptr_t)ddf + (uintptr_t)ddf->m_Collection);
            dmEngine::ActivateCollection(self, ddf->m_Collection);
        }
        else if (instance_message_data->m_DDFDescriptor == dmGameObjectDDF::AcquireInputFocus::m_DDFDescriptor)
        {
            dmGameObjectDDF::AcquireInputFocus* ddf = (dmGameObjectDDF::AcquireInputFocus*) instance_message_data->m_Buffer;
            dmGameObject::HCollection collection = self->m_MainCollection;
            dmGameObject::HInstance instance = dmGameObject::GetInstanceFromIdentifier(self->m_MainCollection, ddf->m_GameObjectId);
            if (!instance && self->m_ActiveCollection != 0)
            {
                instance = dmGameObject::GetInstanceFromIdentifier(self->m_ActiveCollection, ddf->m_GameObjectId);
                collection = self->m_ActiveCollection;
            }
            if (instance)
            {
                dmGameObject::AcquireInputFocus(collection, instance);
            }
            else
            {
                dmLogWarning("Game object with id %llu could not be found when trying to acquire input focus.", ddf->m_GameObjectId);
            }
        }
        else if (instance_message_data->m_DDFDescriptor == dmGameObjectDDF::ReleaseInputFocus::m_DDFDescriptor)
        {
            dmGameObjectDDF::ReleaseInputFocus* ddf = (dmGameObjectDDF::ReleaseInputFocus*) instance_message_data->m_Buffer;
            dmGameObject::HCollection collection = self->m_MainCollection;
            dmGameObject::HInstance instance = dmGameObject::GetInstanceFromIdentifier(self->m_MainCollection, ddf->m_GameObjectId);
            if (!instance && self->m_ActiveCollection != 0)
            {
                instance = dmGameObject::GetInstanceFromIdentifier(self->m_ActiveCollection, ddf->m_GameObjectId);
                collection = self->m_ActiveCollection;
            }
            if (instance)
            {
                dmGameObject::ReleaseInputFocus(collection, instance);
            }
        }
        else if (instance_message_data->m_DDFDescriptor == dmGameObjectDDF::GameObjectTransformQuery::m_DDFDescriptor)
        {
            dmGameObject::InstanceMessageData* instance_message_data = (dmGameObject::InstanceMessageData*) message_object->m_Data;
            dmGameObjectDDF::GameObjectTransformQuery* pq = (dmGameObjectDDF::GameObjectTransformQuery*) instance_message_data->m_Buffer;
            dmGameObject::HInstance instance = dmGameObject::GetInstanceFromIdentifier(self->m_MainCollection, pq->m_GameObjectId);
            if (!instance && self->m_ActiveCollection != 0)
                instance = dmGameObject::GetInstanceFromIdentifier(self->m_ActiveCollection, pq->m_GameObjectId);
            if (instance)
            {
                const uint32_t offset = sizeof(dmGameObject::InstanceMessageData) + sizeof(dmGameObjectDDF::GameObjectTransformResult);
                char buf[offset];
                dmGameObject::InstanceMessageData* out_instance_message_data = (dmGameObject::InstanceMessageData*)buf;
                out_instance_message_data->m_MessageId = dmHashString64(dmGameObjectDDF::GameObjectTransformResult::m_DDFDescriptor->m_ScriptName);
                out_instance_message_data->m_Instance = instance_message_data->m_Instance;
                out_instance_message_data->m_Component = 0xff;
                out_instance_message_data->m_DDFDescriptor = dmGameObjectDDF::GameObjectTransformResult::m_DDFDescriptor;

                dmGameObjectDDF::GameObjectTransformResult* result = (dmGameObjectDDF::GameObjectTransformResult*)(buf + sizeof(dmGameObject::InstanceMessageData));
                result->m_GameObjectId = pq->m_GameObjectId;
                result->m_Position = dmGameObject::GetPosition(instance);
                result->m_Rotation = dmGameObject::GetRotation(instance);
                dmMessage::HSocket reply_socket = dmGameObject::GetReplyMessageSocket(self->m_Register);
                dmhash_t reply_message_id = dmGameObject::GetMessageId(self->m_Register);
                dmMessage::Post(reply_socket, reply_message_id, buf, dmGameObject::INSTANCE_MESSAGE_MAX);
            }
            else
            {
                dmLogWarning("Could not find instance with id %llu.", pq->m_GameObjectId);
            }
        }
        else if (instance_message_data->m_DDFDescriptor == dmRenderDDF::DrawText::m_DDFDescriptor)
        {
            dmRenderDDF::DrawText* dt = (dmRenderDDF::DrawText*) instance_message_data->m_Buffer;
            dmRender::DrawTextParams params;
            params.m_Text = (const char*) ((uintptr_t) dt + (uintptr_t) dt->m_Text);
            params.m_X = dt->m_Position.getX();
            params.m_Y = dt->m_Position.getY();
            params.m_FaceColor = Vectormath::Aos::Vector4(0.0f, 0.0f, 1.0f, 1.0f);
            dmRender::DrawText(self->m_RenderContext, self->m_Font, params);
        }
        else if (instance_message_data->m_DDFDescriptor == dmRenderDDF::DrawLine::m_DDFDescriptor)
        {
            dmRenderDDF::DrawLine* dl = (dmRenderDDF::DrawLine*) instance_message_data->m_Buffer;
            dmRender::Line3D(self->m_RenderContext, dl->m_StartPoint, dl->m_EndPoint, dl->m_Color, dl->m_Color);
        }
        else if (instance_message_data->m_DDFDescriptor == dmEngineDDF::SetTimeStep::m_DDFDescriptor)
        {
            dmEngineDDF::SetTimeStep* ddf = (dmEngineDDF::SetTimeStep*)instance_message_data->m_Buffer;
            self->m_TimeStepFactor = ddf->m_Factor;
            self->m_TimeStepMode = ddf->m_Mode;
            self->m_WarpTimeStep = true;
        }
        else if (instance_message_data->m_DDFDescriptor == dmPhysicsDDF::RayCastRequest::m_DDFDescriptor)
        {
            uint32_t id = dmGameObject::GetIdentifier(instance_message_data->m_Instance);
            dmGameObject::HCollection collection = self->m_MainCollection;
            dmGameObject::HInstance instance = dmGameObject::GetInstanceFromIdentifier(self->m_MainCollection, id);
            if (!instance && self->m_ActiveCollection != 0)
            {
                instance = dmGameObject::GetInstanceFromIdentifier(self->m_ActiveCollection, id);
                collection = self->m_ActiveCollection;
            }
            dmPhysicsDDF::RayCastRequest* ddf = (dmPhysicsDDF::RayCastRequest*)instance_message_data->m_Buffer;
            dmGameSystem::RequestRayCast(collection, instance_message_data->m_Instance, ddf->m_From, ddf->m_To, ddf->m_Mask);
        }
        else
        {
            if (instance_message_data->m_MessageId == dmHashString64("toggle_profile"))
            {
                self->m_ShowProfile = !self->m_ShowProfile;
            }
            else if (instance_message_data->m_MessageId == dmHashString64("reset_time_step"))
            {
                self->m_WarpTimeStep = false;
            }
            else
            {
                dmLogError("Unknown message: %s\n", instance_message_data->m_DDFDescriptor->m_Name);
            }
        }
    }

    void DispatchRenderScript(dmMessage::Message *message_object, void* user_ptr)
    {
        dmRender::HRenderScriptInstance instance = (dmRender::HRenderScriptInstance)user_ptr;
        dmGameObject::InstanceMessageData* instance_message_data = (dmGameObject::InstanceMessageData*) message_object->m_Data;
        dmRender::Message message;
        message.m_Id = message_object->m_ID;
        message.m_DDFDescriptor = instance_message_data->m_DDFDescriptor;
        message.m_BufferSize = instance_message_data->m_BufferSize;
        message.m_Buffer = instance_message_data->m_Buffer;
        dmRender::OnMessageRenderScriptInstance(instance, &message);
    }

    void LoadCollection(HEngine engine, const char* collection_name)
    {
        dmGameObject::HCollection collection;
        dmResource::FactoryResult r = dmResource::Get(engine->m_Factory, collection_name, (void**) &collection);
        if (r == dmResource::FACTORY_RESULT_OK)
        {
            engine->m_Collections.Put(dmHashString32(collection_name), collection);
        }
    }

    void UnloadCollection(HEngine engine, const char* collection_name)
    {
        uint32_t collection_id = dmHashString32(collection_name);
        dmGameObject::HCollection* collection = engine->m_Collections.Get(collection_id);
        if (collection != 0x0)
        {
            dmResource::Release(engine->m_Factory, *collection);
            engine->m_Collections.Erase(collection_id);
            if (*collection == engine->m_ActiveCollection)
                engine->m_ActiveCollection = 0;
        }
    }

    void ActivateCollection(HEngine engine, const char* collection_name)
    {
        uint32_t collection_id = dmHashString32(collection_name);
        dmGameObject::HCollection* collection = engine->m_Collections.Get(collection_id);
        if (collection != 0x0)
        {
            dmGameObject::Init(*collection);
            engine->m_ActiveCollection = *collection;
        }
    }

    void RegisterDDFTypes()
    {
        dmGameSystem::RegisterDDFTypes();

        dmGameObject::RegisterDDFType(dmEngineDDF::Exit::m_DDFDescriptor);
        dmGameObject::RegisterDDFType(dmEngineDDF::SetTimeStep::m_DDFDescriptor);
        dmGameObject::RegisterDDFType(dmRenderDDF::DrawText::m_DDFDescriptor);
        dmGameObject::RegisterDDFType(dmRenderDDF::DrawLine::m_DDFDescriptor);
        dmGameObject::RegisterDDFType(dmModelDDF::SetTexture::m_DDFDescriptor);
        dmGameObject::RegisterDDFType(dmModelDDF::SetVertexConstant::m_DDFDescriptor);
        dmGameObject::RegisterDDFType(dmModelDDF::ResetVertexConstant::m_DDFDescriptor);
        dmGameObject::RegisterDDFType(dmModelDDF::SetFragmentConstant::m_DDFDescriptor);
        dmGameObject::RegisterDDFType(dmModelDDF::ResetFragmentConstant::m_DDFDescriptor);
        dmGameObject::RegisterDDFType(dmGameObjectDDF::LoadCollection::m_DDFDescriptor);
        dmGameObject::RegisterDDFType(dmGameObjectDDF::UnloadCollection::m_DDFDescriptor);
        dmGameObject::RegisterDDFType(dmGameObjectDDF::ActivateCollection::m_DDFDescriptor);
        dmGameObject::RegisterDDFType(dmGameObjectDDF::AcquireInputFocus::m_DDFDescriptor);
        dmGameObject::RegisterDDFType(dmGameObjectDDF::ReleaseInputFocus::m_DDFDescriptor);
        dmGameObject::RegisterDDFType(dmGameObjectDDF::GameObjectTransformQuery::m_DDFDescriptor);
        dmGameObject::RegisterDDFType(dmGameObjectDDF::GameObjectTransformResult::m_DDFDescriptor);

        dmGui::RegisterDDFType(dmGameObjectDDF::GameObjectTransformQuery::m_DDFDescriptor);
        dmGui::RegisterDDFType(dmGameObjectDDF::GameObjectTransformResult::m_DDFDescriptor);
    }

    bool LoadBootstrapContent(HEngine engine, dmConfigFile::HConfig config)
    {
        dmResource::FactoryResult fact_error = dmResource::Get(engine->m_Factory, dmConfigFile::GetString(config, "bootstrap.font", "fonts/VeraMoBd.fontc"), (void**) &engine->m_Font);
        if (fact_error != dmResource::FACTORY_RESULT_OK)
        {
            return false;
        }

        fact_error = dmResource::Get(engine->m_Factory, dmConfigFile::GetString(config, "bootstrap.small_font", "fonts/VeraMoBd2.fontc"), (void**) &engine->m_SmallFont);
        if (fact_error != dmResource::FACTORY_RESULT_OK)
        {
            return false;
        }

        const char* gamepads = dmConfigFile::GetString(config, "bootstrap.gamepads", "input/default.gamepadsc");
        dmInputDDF::GamepadMaps* gamepad_maps_ddf;
        fact_error = dmResource::Get(engine->m_Factory, gamepads, (void**)&gamepad_maps_ddf);
        if (fact_error != dmResource::FACTORY_RESULT_OK)
            return false;
        dmInput::RegisterGamepads(engine->m_InputContext, gamepad_maps_ddf);
        dmResource::Release(engine->m_Factory, gamepad_maps_ddf);

        const char* game_input_binding = dmConfigFile::GetString(config, "bootstrap.game_binding", "input/game.input_bindingc");
        dmInputDDF::InputBinding* ddf;
        fact_error = dmResource::Get(engine->m_Factory, game_input_binding, (void**)&ddf);
        if (fact_error != dmResource::FACTORY_RESULT_OK)
            return false;
        engine->m_GameInputBinding = dmInput::NewBinding(engine->m_InputContext, ddf);
        dmResource::Release(engine->m_Factory, ddf);
        if (!engine->m_GameInputBinding)
        {
            dmLogError("Unable to load game input context, did you forget to specify %s in the config file?", "bootstrap.game_config");
            return false;
        }

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
        if (engine->m_Font)
            dmResource::Release(engine->m_Factory, engine->m_Font);
        if (engine->m_SmallFont)
            dmResource::Release(engine->m_Factory, engine->m_SmallFont);

        if (engine->m_DebugMaterial)
        {
            dmGraphics::HVertexProgram debug_vp = dmRender::GetMaterialVertexProgram(engine->m_DebugMaterial);
            if (debug_vp)
                dmGraphics::DeleteVertexProgram(debug_vp);
            dmGraphics::HFragmentProgram debug_fp = dmRender::GetMaterialFragmentProgram(engine->m_DebugMaterial);
            if (debug_fp)
                dmGraphics::DeleteFragmentProgram(debug_fp);
            dmRender::DeleteMaterial(engine->m_DebugMaterial);
        }

        if (engine->m_GameInputBinding)
            dmInput::DeleteBinding(engine->m_GameInputBinding);
    }

    uint32_t GetFrameCount(HEngine engine)
    {
        return engine->m_Stats.m_FrameCount;
    }
}

