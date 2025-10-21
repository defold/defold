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

#ifndef DM_TEST_GAMESYS_H
#define DM_TEST_GAMESYS_H

#include <resource/resource.h>

#include <dlib/buffer.h>
#include <dlib/testutil.h>
#include <hid/hid.h>

#include <sound/sound.h>
#include <gameobject/component.h>
#include <extension/extension.hpp>
#include <physics/physics.h>
#include <rig/rig.h>

#include "gamesys/gamesys.h"
#include "gamesys/scripts/script_buffer.h"
#include "../components/comp_gui_private.h" // BoxVertex
#include "../components/comp_gui.h" // The GuiGetURLCallback et.al
#include "../../../../graphics/src/graphics_private.h" // for unit test functions

#include <dmsdk/script/script.h>
#include <dmsdk/gamesys/script.h>

#include <testmain/testmain.h>

#include <jc_test/jc_test.h>


static inline dmGameObject::HInstance Spawn(dmResource::HFactory factory, dmGameObject::HCollection collection, const char* prototype_name, dmhash_t id, dmGameObject::HPropertyContainer properties,
                                            const dmVMath::Point3& position, const dmVMath::Quat& rotation, const dmVMath::Vector3& scale)
{
    dmGameObject::HPrototype prototype = 0x0;
    if (dmResource::Get(factory, prototype_name, (void**)&prototype) == dmResource::RESULT_OK) {
        dmGameObject::HInstance result = dmGameObject::Spawn(collection, prototype, prototype_name, id, properties, position, rotation, scale);
        dmResource::Release(factory, prototype);
        return result;
    }
    return 0x0;
}

static inline dmGameObject::HInstance Spawn(dmResource::HFactory factory, dmGameObject::HCollection collection, const char* prototype_name, dmhash_t id)
{
    return Spawn(factory, collection, prototype_name, id, 0, dmVMath::Point3(0, 0, 0), dmVMath::Quat(0, 0, 0, 1), dmVMath::Vector3(1, 1, 1));
}

struct Params
{
    const char* m_ValidResource;
    const char* m_InvalidResource;
    const char* m_TempResource;
};

struct ProjectOptions {
  uint32_t m_MaxCollisionCount;
  uint32_t m_MaxCollisionObjectCount;
  uint32_t m_MaxContactPointCount;
  uint32_t m_MaxInstances;
  uint32_t m_TriggerOverlapCapacity;
  bool m_3D;
  float m_Scale;
  float m_VelocityThreshold;
};

template<typename T>
class CustomizableGamesysTest : public jc_test_params_class<T>
{
public:
    CustomizableGamesysTest() {
      memset(&m_projectOptions, 0, sizeof(m_projectOptions));
    }

    ProjectOptions m_projectOptions;
protected:
    void SetUp(ProjectOptions& options) {
        m_projectOptions = options;
    }

    void SetUp() override = 0;
};

template<typename T>
class GamesysTest : public CustomizableGamesysTest<T>
{
public:
    GamesysTest() {
        // Default configuration values for the engine. Subclass GamesysTest and ovewrite in constructor.
        this->m_projectOptions.m_MaxCollisionCount = 0;
        this->m_projectOptions.m_MaxCollisionObjectCount = 0;
        this->m_projectOptions.m_MaxContactPointCount = 0;
        this->m_projectOptions.m_MaxInstances = 1024;
        this->m_projectOptions.m_3D = false;
        this->m_projectOptions.m_Scale = 1.0f;
        this->m_projectOptions.m_VelocityThreshold = 1.0f;
    }
protected:
    void SetUp() override;
    void TearDown() override;
    void SetupComponentCreateContext(dmGameObject::ComponentTypeCreateCtx& component_create_ctx);

    void WaitForTestsDone(int update_count, bool render, bool* result);

    dmGameObject::UpdateContext m_UpdateContext;
    dmGameObject::HRegister m_Register;
    dmGameObject::HCollection m_Collection;
    dmResource::HFactory m_Factory;
    dmConfigFile::HConfig m_Config;

    dmPlatform::HWindow m_Window;
    dmScript::HContext m_ScriptContext;
    dmGraphics::HContext m_GraphicsContext;
    dmJobThread::HContext m_JobThread;
    dmRender::HRenderContext m_RenderContext;
    dmGameSystem::PhysicsContextBox2D m_PhysicsContextBox2D;
    dmGameSystem::PhysicsContextBullet3D m_PhysicsContextBullet3D;
    dmGameSystem::ParticleFXContext m_ParticleFXContext;
    dmGui::HContext m_GuiContext;
    dmHID::HContext m_HidContext;
    dmInput::HContext m_InputContext;
    dmInputDDF::GamepadMaps* m_GamepadMapsDDF;
    dmGameSystem::SpriteContext m_SpriteContext;
    dmGameSystem::CollectionProxyContext m_CollectionProxyContext;
    dmGameSystem::FactoryContext m_FactoryContext;
    dmGameSystem::CollectionFactoryContext m_CollectionFactoryContext;
    dmGameSystem::ModelContext m_ModelContext;
    dmGameSystem::LabelContext m_LabelContext;
    dmGameSystem::TilemapContext m_TilemapContext;
    dmRig::HRigContext m_RigContext;
    dmGameObject::ModuleContext m_ModuleContext;
    dmHashTable64<void*> m_Contexts;
    ExtensionAppParams  m_AppParams;
    ExtensionParams     m_Params;
};

class ScriptBaseTest : public GamesysTest<const char*>
{
public:
    void SetUp()
    {
        GamesysTest::SetUp();

        m_Scriptlibcontext.m_Factory         = m_Factory;
        m_Scriptlibcontext.m_Register        = m_Register;
        m_Scriptlibcontext.m_LuaState        = dmScript::GetLuaState(m_ScriptContext);
        m_Scriptlibcontext.m_GraphicsContext = m_GraphicsContext;
        m_Scriptlibcontext.m_ScriptContext   = m_ScriptContext;
        m_Scriptlibcontext.m_JobThread       = m_JobThread;

        dmGameSystem::InitializeScriptLibs(m_Scriptlibcontext);
    }

    void TearDown() override
    {
        dmGameSystem::FinalizeScriptLibs(m_Scriptlibcontext);

        GamesysTest::TearDown();
    }

    virtual ~ScriptBaseTest() { }

    dmGameSystem::ScriptLibContext m_Scriptlibcontext;
};

// sets up test context for various 2D physics/collision-object tests
class CollisionObject2DTest : public GamesysTest<const char*>
{
public:
    CollisionObject2DTest() {
      // override configuration values specified in GamesysTest()
      m_projectOptions.m_MaxCollisionCount = 32;
      m_projectOptions.m_MaxContactPointCount = 64;
      m_projectOptions.m_3D = false;
    }
};

struct GroupAndMaskParams {
    const char* m_Actions;
    const bool m_CollisionExpected;
};

class GroupAndMask2DTest : public GamesysTest<GroupAndMaskParams>
{
public:
    GroupAndMask2DTest() {
      // override configuration values specified in GamesysTest()
      m_projectOptions.m_MaxCollisionCount = 32;
      m_projectOptions.m_MaxContactPointCount = 64;
      m_projectOptions.m_3D = false;
    }
};

class GroupAndMask3DTest : public GamesysTest<GroupAndMaskParams>
{
public:
    GroupAndMask3DTest() {
      // override configuration values specified in GamesysTest()
      m_projectOptions.m_MaxCollisionCount = 32;
      m_projectOptions.m_MaxContactPointCount = 64;
      m_projectOptions.m_MaxCollisionObjectCount = 512;
      m_projectOptions.m_3D = true;
    }
};

class Trigger2DTest : public CollisionObject2DTest
{
public:
	Trigger2DTest() {
		m_projectOptions.m_TriggerOverlapCapacity = 1;
	}
};

class VelocityThreshold2DTest : public CollisionObject2DTest
{
public:
	VelocityThreshold2DTest() {
		m_projectOptions.m_Scale = 0.1;
		m_projectOptions.m_VelocityThreshold = 20;
	}
};

class ResourceTest : public GamesysTest<const char*>
{
public:
    virtual ~ResourceTest() {}
};

struct ResourceReloadParams
{
    const char* m_FilenameEnding;
    const char* m_InitialResource;
    const char* m_SecondResource;
};

class ResourceReloadTest : public GamesysTest<ResourceReloadParams>
{
public:
    virtual ~ResourceReloadTest() {}
};

struct ResourceFailParams
{
    const char* m_ValidResource;
    const char* m_InvalidResource;
};

class ResourceFailTest : public GamesysTest<ResourceFailParams>
{
public:
    ~ResourceFailTest() override = default;
};

class InvalidVertexSpaceTest : public GamesysTest<const char*>
{
public:
    ~InvalidVertexSpaceTest() override = default;
};

class ComponentTest : public ScriptBaseTest
{
public:
    ~ComponentTest() override = default;
};

class ComponentFailTest : public GamesysTest<const char*>
{
public:
    ~ComponentFailTest() override = default;
};

class BufferMetadataTest : public GamesysTest<const char*>
{
public:
    ~BufferMetadataTest() override = default;
};

struct FactoryTestParams
{
    const char* m_GOPath;
    bool m_IsDynamic;
    bool m_IsPreloaded;
};

class FactoryTest : public GamesysTest<FactoryTestParams>
{
public:
    ~FactoryTest() override = default;
};

struct CollectionFactoryTestParams
{
    const char* m_GOPath;
    const char* m_PrototypePath; // If set, then it uses "Dynamic Prototype"
    bool m_IsDynamic;
    bool m_IsPreloaded;
};

class CollectionFactoryTest : public GamesysTest<CollectionFactoryTestParams>
{
public:
    ~CollectionFactoryTest() override = default;
};

class SpriteTest : public ScriptBaseTest
{
public:
    ~SpriteTest() override = default;
};

class ParticleFxTest : public GamesysTest<const char*>
{
public:
    ~ParticleFxTest() override = default;
};


class WindowTest : public GamesysTest<const char*>
{
public:
    ~WindowTest() override = default;
};

struct DrawCountParams
{
    const char* m_GOPath;
    uint64_t m_ExpectedDrawCount;
};

class DrawCountTest : public GamesysTest<DrawCountParams>
{
public:
    ~DrawCountTest() override = default;
};

struct BoxRenderParams
{
    const static uint8_t MAX_VERTICES_IN_9_SLICED_QUAD = 16;
    const static uint8_t MAX_INDICES_IN_9_SLICED_QUAD = 3 * 2 * 9;

    const char* m_GOPath;
    dmGameSystem::BoxVertex m_ExpectedVertices[MAX_VERTICES_IN_9_SLICED_QUAD];
    uint8_t m_ExpectedVerticesCount;
    int m_ExpectedIndices[MAX_INDICES_IN_9_SLICED_QUAD];
};

class BoxRenderTest : public GamesysTest<BoxRenderParams>
{
public:
    ~BoxRenderTest() override = default;
};

class GamepadConnectedTest : public GamesysTest<const char*>
{
public:
    ~GamepadConnectedTest() override = default;
};

struct ResourcePropParams {
    const char* m_PropertyName;
    const char* m_ResourcePath;
    const char* m_ResourcePathNotFound;
    const char* m_ResourcePathInvExt;
    const char* m_Component0;
    const char* m_Component1;
    const char* m_Component2;
    const char* m_Component3;
    const char* m_Component4;
    const char* m_Component5;
};

class ResourcePropTest : public GamesysTest<ResourcePropParams>
{
protected:
    void SetUp()
    {
        GamesysTest::SetUp();
    }
public:
    ~ResourcePropTest() override = default;
};

class FlipbookTest : public GamesysTest<const char*>
{
public:
    ~FlipbookTest() override = default;
};

struct CursorTestParams
{
    const char* m_AnimationId;
    float m_CursorStart;
    float m_PlaybackRate;
    float m_Expected[16];
    uint8_t m_ExpectedCount;
};

class CursorTest : public GamesysTest<CursorTestParams>
{
public:
    ~CursorTest() override = default;
};

class FontTest : public GamesysTest<const char*>
{
public:
    ~FontTest() override = default;
};

class GuiTest : public ScriptBaseTest
{
public:
    ~GuiTest() override = default;
};

struct ScriptComponentTestParams
{
    const char* m_GOPath;
    const char* m_ComponentType; // E.g. "modelc"
    const char* m_ComponentName; // E.g. "model"
};

class ScriptComponentTest : public GamesysTest<ScriptComponentTestParams>
{
public:
    ~ScriptComponentTest() override = default;
};

class SoundTest : public GamesysTest<const char*>
{
public:
    ~SoundTest() override = default;
};

class RenderConstantsTest : public GamesysTest<const char*>
{
public:
    virtual ~RenderConstantsTest() {}
};

class MaterialTest : public ScriptBaseTest
{
public:
    ~MaterialTest() override = default;
};

class ModelTest : public ScriptBaseTest
{
public:
    ~ModelTest() override = default;
};

class ShaderTest : public GamesysTest<const char*>
{
public:
    ~ShaderTest() override = default;
};

class SysTest : public ScriptBaseTest
{
public:
    ~SysTest() override = default;
};

bool CopyResource(const char* src, const char* dst);
bool UnlinkResource(const char* name);

template<typename T>
void GamesysTest<T>::SetupComponentCreateContext(dmGameObject::ComponentTypeCreateCtx& component_create_ctx)
{
    component_create_ctx.m_Script = m_ScriptContext;
    component_create_ctx.m_Register = m_Register;
    component_create_ctx.m_Factory = m_Factory;
    component_create_ctx.m_Config = m_Config;
    component_create_ctx.m_Contexts.SetCapacity(3, 8);
    component_create_ctx.m_Contexts.Put(dmHashString64("graphics"), m_GraphicsContext);
    component_create_ctx.m_Contexts.Put(dmHashString64("render"), m_RenderContext);
    component_create_ctx.m_Contexts.Put(dmHashString64("guic"), m_GuiContext);
    component_create_ctx.m_Contexts.Put(dmHashString64("gui_scriptc"), m_ScriptContext);
}

template<typename T>
void GamesysTest<T>::SetUp()
{
    dmSound::Initialize(0x0, 0x0);

    m_UpdateContext.m_DT = 1.0f / 60.0f;

    dmJobThread::JobThreadCreationParams job_thread_create_param = {0};
    job_thread_create_param.m_ThreadCount = 1;
    m_JobThread = dmJobThread::Create(job_thread_create_param);

    dmResource::NewFactoryParams params;
    params.m_MaxResources = 64;
    params.m_Flags = RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT;
    params.m_JobThreadContext = m_JobThread;

    m_Factory = dmResource::NewFactory(&params, "build/src/gamesys/test");
    ASSERT_NE((dmResource::HFactory)0, m_Factory); // Probably a sign that the previous test wasn't properly shut down

    dmPlatform::WindowParams win_params = {};
    m_Window = dmPlatform::NewWindow();
    dmPlatform::OpenWindow(m_Window, win_params);

    m_HidContext = dmHID::NewContext(dmHID::NewContextParams());
    dmHID::Init(m_HidContext);
    dmHID::SetWindow(m_HidContext, m_Window);


    dmGraphics::InstallAdapter();
    dmGraphics::ResetDrawCount(); // for the unit test

    dmGraphics::ContextParams graphics_context_params;
    graphics_context_params.m_Window = m_Window;
    graphics_context_params.m_JobThread = m_JobThread;

    m_GraphicsContext = dmGraphics::NewContext(graphics_context_params);

    dmScript::ContextParams script_context_params = {};
    script_context_params.m_Factory = m_Factory;
    script_context_params.m_GraphicsContext = m_GraphicsContext;
    m_ScriptContext = dmScript::NewContext(script_context_params);
    dmScript::Initialize(m_ScriptContext);

    lua_State* L = dmScript::GetLuaState(m_ScriptContext);
    #ifdef DM_PHYSICS_BOX2D_V3
        lua_pushstring(L, "box2dv3");
    #else
        lua_pushstring(L, "box2dv2");
    #endif
    lua_setglobal(L, "PHYSICS");

    dmGui::NewContextParams gui_params;
    gui_params.m_ScriptContext = m_ScriptContext;
    gui_params.m_HidContext = m_HidContext;
    gui_params.m_GetURLCallback = dmGameSystem::GuiGetURLCallback;
    gui_params.m_GetUserDataCallback = dmGameSystem::GuiGetUserDataCallback;
    gui_params.m_ResolvePathCallback = dmGameSystem::GuiResolvePathCallback;
    m_GuiContext = dmGui::NewContext(&gui_params);

    m_Register = dmGameObject::NewRegister();
    dmGameObject::Initialize(m_Register, m_ScriptContext);

    dmConfigFile::LoadFromBuffer(0, 0, 0, 0, &m_Config);

    ExtensionAppParamsInitialize(&m_AppParams);
    ExtensionParamsInitialize(&m_Params);

    m_Params.m_L = dmScript::GetLuaState(m_ScriptContext);
    m_Params.m_ResourceFactory = m_Factory;
    m_Params.m_ConfigFile = m_Config;
    ExtensionParamsSetContext(&m_Params, "lua", dmScript::GetLuaState(m_ScriptContext));
    ExtensionParamsSetContext(&m_Params, "config", m_Config);

    dmExtension::AppInitialize(&m_AppParams);
    dmExtension::Initialize(&m_Params);

    dmRender::RenderContextParams render_params;
    render_params.m_MaxRenderTypes = 10;
    render_params.m_MaxInstances = 1000;
    render_params.m_MaxRenderTargets = 10;
    render_params.m_ScriptContext = m_ScriptContext;
    render_params.m_MaxCharacters = 256;
    render_params.m_MaxBatches = 128;
    m_RenderContext = dmRender::NewRenderContext(m_GraphicsContext, render_params);

    dmInput::NewContextParams input_params;
    input_params.m_HidContext = m_HidContext;
    input_params.m_RepeatDelay = 0.3f;
    input_params.m_RepeatInterval = 0.1f;
    m_InputContext = dmInput::NewContext(input_params);

    dmGameSystem::PhysicsContext* physics_context = 0;

    memset(&m_PhysicsContextBox2D, 0, sizeof(m_PhysicsContextBox2D));
    memset(&m_PhysicsContextBullet3D, 0, sizeof(m_PhysicsContextBullet3D));

    if (this->m_projectOptions.m_3D)
    {
        m_PhysicsContextBullet3D.m_BaseContext.m_MaxCollisionCount = this->m_projectOptions.m_MaxCollisionCount;
        m_PhysicsContextBullet3D.m_BaseContext.m_MaxContactPointCount = this->m_projectOptions.m_MaxContactPointCount;
        m_PhysicsContextBullet3D.m_BaseContext.m_MaxCollisionObjectCount = this->m_projectOptions.m_MaxCollisionObjectCount;

        m_PhysicsContextBullet3D.m_BaseContext.m_MaxCollisionCount = 64;
        m_PhysicsContextBullet3D.m_BaseContext.m_MaxContactPointCount = 128;
        m_PhysicsContextBullet3D.m_BaseContext.m_MaxCollisionObjectCount = 512;
        m_PhysicsContextBullet3D.m_BaseContext.m_PhysicsType = dmGameSystem::PHYSICS_ENGINE_BULLET3D;

        m_PhysicsContextBullet3D.m_Context = dmPhysics::NewContext3D(dmPhysics::NewContextParams());

        physics_context = &m_PhysicsContextBullet3D.m_BaseContext;
    }
    else
    {
        m_PhysicsContextBox2D.m_BaseContext.m_MaxCollisionCount = this->m_projectOptions.m_MaxCollisionCount;
        m_PhysicsContextBox2D.m_BaseContext.m_MaxContactPointCount = this->m_projectOptions.m_MaxContactPointCount;
        m_PhysicsContextBox2D.m_BaseContext.m_MaxCollisionObjectCount = this->m_projectOptions.m_MaxCollisionObjectCount;

        m_PhysicsContextBox2D.m_BaseContext.m_MaxCollisionCount = 64;
        m_PhysicsContextBox2D.m_BaseContext.m_MaxContactPointCount = 128;
        m_PhysicsContextBox2D.m_BaseContext.m_MaxCollisionObjectCount = 512;
        m_PhysicsContextBox2D.m_BaseContext.m_PhysicsType = dmGameSystem::PHYSICS_ENGINE_BOX2D;
        m_PhysicsContextBox2D.m_BaseContext.m_Box2DVelocityIterations = 10;
        m_PhysicsContextBox2D.m_BaseContext.m_Box2DPositionIterations = 10;
        m_PhysicsContextBox2D.m_BaseContext.m_Box2DSubStepCount = 4;

        dmPhysics::NewContextParams context2DParams = dmPhysics::NewContextParams();
        context2DParams.m_Scale = this->m_projectOptions.m_Scale;
        context2DParams.m_VelocityThreshold = this->m_projectOptions.m_VelocityThreshold;
        context2DParams.m_TriggerOverlapCapacity = this->m_projectOptions.m_TriggerOverlapCapacity;
        m_PhysicsContextBox2D.m_Context = dmPhysics::NewContext2D(context2DParams);

        physics_context = &m_PhysicsContextBox2D.m_BaseContext;
    }

    m_ParticleFXContext.m_Factory = m_Factory;
    m_ParticleFXContext.m_RenderContext = m_RenderContext;
    m_ParticleFXContext.m_MaxParticleFXCount = 64;
    m_ParticleFXContext.m_MaxParticleCount = 256;
    m_ParticleFXContext.m_MaxParticleBufferCount = 256;
    m_ParticleFXContext.m_MaxEmitterCount = 8;

    m_SpriteContext.m_RenderContext = m_RenderContext;
    m_SpriteContext.m_MaxSpriteCount = 32;

    m_CollectionProxyContext.m_Factory = m_Factory;
    m_CollectionProxyContext.m_MaxCollectionProxyCount = 8;

    m_FactoryContext.m_MaxFactoryCount = 128;
    m_FactoryContext.m_ScriptContext = m_ScriptContext;
    m_FactoryContext.m_Factory = m_Factory;
    m_CollectionFactoryContext.m_MaxCollectionFactoryCount = 128;
    m_CollectionFactoryContext.m_ScriptContext = m_ScriptContext;
    m_CollectionFactoryContext.m_Factory = m_Factory;

    m_LabelContext.m_RenderContext = m_RenderContext;
    m_LabelContext.m_MaxLabelCount = 32;
    m_LabelContext.m_Subpixels     = 0;

    m_TilemapContext.m_RenderContext = m_RenderContext;
    m_TilemapContext.m_MaxTilemapCount = 16;
    m_TilemapContext.m_MaxTileCount = 512;

    m_ModelContext.m_RenderContext = m_RenderContext;
    m_ModelContext.m_Factory = m_Factory;
    m_ModelContext.m_MaxModelCount = 128;

    dmBuffer::NewContext(); // ???

    m_Contexts.OffsetCapacity(16);
    m_Contexts.Put(dmHashString64("goc"), m_Register);
    m_Contexts.Put(dmHashString64("collectionc"), m_Register);
    m_Contexts.Put(dmHashString64("scriptc"), m_ScriptContext);
    m_Contexts.Put(dmHashString64("luac"), &m_ModuleContext);
    m_Contexts.Put(dmHashString64("guic"), m_GuiContext);
    m_Contexts.Put(dmHashString64("gui_scriptc"), m_ScriptContext);
    m_Contexts.Put(dmHashString64("fontc"), m_RenderContext);

    dmResource::RegisterTypes(m_Factory, &m_Contexts);

    dmResource::Result r = dmGameSystem::RegisterResourceTypes(m_Factory, m_RenderContext, m_InputContext, physics_context);
    ASSERT_EQ(dmResource::RESULT_OK, r);

    dmResource::Get(m_Factory, "/input/valid.gamepadsc", (void**)&m_GamepadMapsDDF);
    ASSERT_NE((void*)0, m_GamepadMapsDDF);
    dmInput::RegisterGamepads(m_InputContext, m_GamepadMapsDDF);

    dmGameObject::ComponentTypeCreateCtx component_create_ctx;
    SetupComponentCreateContext(component_create_ctx);
    dmGameObject::CreateRegisteredComponentTypes(&component_create_ctx);

    assert(dmGameObject::RESULT_OK == dmGameSystem::RegisterComponentTypes(m_Factory, m_Register, m_RenderContext, physics_context, &m_ParticleFXContext, &m_SpriteContext,
                                                                                                    &m_CollectionProxyContext, &m_FactoryContext, &m_CollectionFactoryContext,
                                                                                                    &m_ModelContext, &m_LabelContext, &m_TilemapContext));

    dmGameObject::SortComponentTypes(m_Register);

    // TODO: Investigate why the ConsumeInputInCollectionProxy test fails if the components are actually sorted (the way they're supposed to)
    //dmGameObject::SortComponentTypes(m_Register);

    m_Collection = dmGameObject::NewCollection("collection", m_Factory, m_Register, this->m_projectOptions.m_MaxInstances, 0x0);
}

template<typename T>
void GamesysTest<T>::TearDown()
{
    dmGameObject::DeleteCollection(m_Collection);
    dmGameObject::PostUpdate(m_Register);
    dmResource::Release(m_Factory, m_GamepadMapsDDF);

    dmGameObject::ComponentTypeCreateCtx component_create_ctx;
    SetupComponentCreateContext(component_create_ctx);
    dmGameObject::DestroyRegisteredComponentTypes(&component_create_ctx);

    dmExtension::Finalize(&m_Params);
    dmExtension::AppFinalize(&m_AppParams);

    ExtensionParamsFinalize(&m_Params);
    ExtensionAppParamsFinalize(&m_AppParams);

    dmGui::DeleteContext(m_GuiContext, m_ScriptContext);
    dmRender::DeleteRenderContext(m_RenderContext, m_ScriptContext);
    dmJobThread::Destroy(m_JobThread);
    dmGraphics::DeleteContext(m_GraphicsContext);
    dmPlatform::CloseWindow(m_Window);
    dmPlatform::DeleteWindow(m_Window);
    dmScript::Finalize(m_ScriptContext);
    dmScript::DeleteContext(m_ScriptContext);
    dmResource::DeleteFactory(m_Factory);
    dmGameObject::DeleteRegister(m_Register);
    dmSound::Finalize();
    dmInput::DeleteContext(m_InputContext);
    dmHID::Final(m_HidContext);
    dmHID::DeleteContext(m_HidContext);

    if (m_PhysicsContextBullet3D.m_Context)
    {
        dmPhysics::DeleteContext3D(m_PhysicsContextBullet3D.m_Context);
    }

    if (m_PhysicsContextBox2D.m_Context)
    {
        dmPhysics::DeleteContext2D(m_PhysicsContextBox2D.m_Context);
    }
    dmBuffer::DeleteContext();
    dmConfigFile::Delete(m_Config);
}

template<typename T>
void GamesysTest<T>::WaitForTestsDone(int update_count, bool render, bool* result)
{
    if (result)
        *result = false;

    lua_State* L = dmScript::GetLuaState(m_ScriptContext);
    bool tests_done = false;
    int count = update_count;
    while (!tests_done && --count > 0)
    {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

        if (render)
        {
            dmRender::RenderListBegin(m_RenderContext);
            dmGameObject::Render(m_Collection);

            dmRender::RenderListEnd(m_RenderContext);
            dmRender::DrawRenderList(m_RenderContext, 0x0, 0x0, 0x0, dmRender::SORT_BACK_TO_FRONT);
        }

        // check if tests are done
        lua_getglobal(L, "tests_done");
        tests_done = lua_toboolean(L, -1);
        lua_pop(L, 1);
    }
    if (count >= 0)
    {
        dmLogError("Waited %d frames for test to finish. Aborting.", update_count);
    }
    ASSERT_LT(0, count);

    if (!result)
    {
        ASSERT_TRUE(tests_done);
    }
    else
    {
        *result = tests_done;
    }
}

class ScriptImageTest : public GamesysTest<const char*>
{
protected:
    void SetUp() override
    {
        GamesysTest::SetUp();

        m_ScriptLibContext.m_Factory         = m_Factory;
        m_ScriptLibContext.m_Register        = m_Register;
        m_ScriptLibContext.m_LuaState        = dmScript::GetLuaState(m_ScriptContext);
        m_ScriptLibContext.m_GraphicsContext = m_GraphicsContext;
        m_ScriptLibContext.m_ScriptContext   = m_ScriptContext;
        dmGameSystem::InitializeScriptLibs(m_ScriptLibContext);

        L = dmScript::GetLuaState(m_ScriptContext);
    }
    void TearDown() override
    {
        dmGameSystem::FinalizeScriptLibs(m_ScriptLibContext);
        GamesysTest::TearDown();
    }

    lua_State* L;
    dmGameSystem::ScriptLibContext m_ScriptLibContext;
};

// Specific test class for testing dmBuffers in scripts
class ScriptBufferTest : public jc_test_base_class
{
protected:
    void SetUp() override
    {
        dmBuffer::NewContext();

        dmScript::ContextParams script_context_params = {};
        m_Context = dmScript::NewContext(script_context_params);
        dmScript::Initialize(m_Context);

        m_ScriptLibContext.m_Factory = 0x0;
        m_ScriptLibContext.m_Register = 0x0;
        m_ScriptLibContext.m_LuaState = dmScript::GetLuaState(m_Context);
        m_ScriptLibContext.m_ScriptContext = m_Context;
        dmGameSystem::InitializeScriptLibs(m_ScriptLibContext);

        L = dmScript::GetLuaState(m_Context);

        const dmBuffer::StreamDeclaration streams_decl[] = {
            {dmHashString64("rgb"), dmBuffer::VALUE_TYPE_UINT16, 3},
            {dmHashString64("a"), dmBuffer::VALUE_TYPE_FLOAT32, 1},
        };

        m_Count = 256;
        dmBuffer::Create(m_Count, streams_decl, 2, &m_Buffer);
    }

    void TearDown() override
    {
        if( m_Buffer )
            dmBuffer::Destroy(m_Buffer);

        dmGameSystem::FinalizeScriptLibs(m_ScriptLibContext);
        dmScript::Finalize(m_Context);
        dmScript::DeleteContext(m_Context);

        dmBuffer::DeleteContext();
    }

    dmGameSystem::ScriptLibContext m_ScriptLibContext;
    dmScript::HContext m_Context;
    lua_State* L;
    dmBuffer::HBuffer m_Buffer;
    uint32_t m_Count;
};

struct CopyBufferTestParams
{
    uint32_t m_Count;
    uint32_t m_DstOffset;
    uint32_t m_SrcOffset;
    uint32_t m_CopyCount;
    bool m_ExpectedOk;
};

class ScriptBufferCopyTest : public jc_test_params_class<CopyBufferTestParams>
{
protected:
    void SetUp() override
    {
        dmBuffer::NewContext();
        dmScript::ContextParams script_context_params = {};
        m_Context = dmScript::NewContext(script_context_params);

        m_ScriptLibContext.m_Factory = 0x0;
        m_ScriptLibContext.m_Register = 0x0;
        m_ScriptLibContext.m_LuaState = dmScript::GetLuaState(m_Context);
        m_ScriptLibContext.m_ScriptContext = m_Context;
        dmGameSystem::InitializeScriptLibs(m_ScriptLibContext);

        dmScript::Initialize(m_Context);
        L = dmScript::GetLuaState(m_Context);


        const dmBuffer::StreamDeclaration streams_decl[] = {
            {dmHashString64("rgb"), dmBuffer::VALUE_TYPE_UINT16, 3},
            {dmHashString64("a"), dmBuffer::VALUE_TYPE_FLOAT32, 1},
        };

        const CopyBufferTestParams& p = GetParam();
        dmBuffer::Create(p.m_Count, streams_decl, 2, &m_Buffer);
    }

    void TearDown() override
    {
        dmBuffer::Destroy(m_Buffer);

        dmGameSystem::FinalizeScriptLibs(m_ScriptLibContext);

        dmScript::Finalize(m_Context);
        dmScript::DeleteContext(m_Context);

        dmBuffer::DeleteContext();
    }

    dmGameSystem::ScriptLibContext m_ScriptLibContext;
    dmScript::HContext m_Context;
    lua_State* L;
    dmBuffer::HBuffer m_Buffer;
};

class LabelTest : public jc_test_base_class
{
protected:
    void SetUp() override
    {
        m_Position = dmVMath::Point3(0.0);
        m_Size = dmVMath::Vector3(2.0, 2.0, 0.0);
        m_Scale = dmVMath::Vector3(1.0, 1.0, 0.0);

        m_BottomLeft = dmVMath::Point3(0.0, 0.0, 0.0);
        m_TopLeft = dmVMath::Point3(0.0, m_Size.getY(), 0.0);
        m_TopRight = dmVMath::Point3(m_Size.getX(), m_Size.getY(), 0.0);
        m_BottomRight = dmVMath::Point3(m_Size.getX(), 0.0, 0.0);

        m_Rotation = dmVMath::EulerToQuat(dmVMath::Vector3(0, 0, -180));
        m_Rotation = normalize(m_Rotation);
    }

    dmVMath::Quat m_Rotation;
    dmVMath::Point3 m_Position;
    dmVMath::Point3 m_BottomLeft;
    dmVMath::Point3 m_TopLeft;
    dmVMath::Point3 m_TopRight;
    dmVMath::Point3 m_BottomRight;
    dmVMath::Vector3 m_Size;
    dmVMath::Vector3 m_Scale;
};

#endif // DM_TEST_GAMESYS_H
