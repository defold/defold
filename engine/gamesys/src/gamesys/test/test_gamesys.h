#include <gtest/gtest.h>

#include <resource/resource.h>

#include <hid/hid.h>

#include <sound/sound.h>
#include <gameobject/gameobject.h>
#include <rig/rig.h>

#include "gamesys/gamesys.h"

struct Params
{
    const char* m_ValidResource;
    const char* m_InvalidResource;
    const char* m_TempResource;
};

template<typename T>
class GamesysTest : public ::testing::TestWithParam<T>
{
protected:
    virtual void SetUp();
    virtual void TearDown();

    dmGameObject::UpdateContext m_UpdateContext;
    dmGameObject::HRegister m_Register;
    dmGameObject::HCollection m_Collection;
    dmResource::HFactory m_Factory;

    dmScript::HContext m_ScriptContext;
    dmGraphics::HContext m_GraphicsContext;
    dmRender::HRenderContext m_RenderContext;
    dmGameSystem::PhysicsContext m_PhysicsContext;
    dmGameSystem::ParticleFXContext m_ParticleFXContext;
    dmGameSystem::GuiContext m_GuiContext;
    dmHID::HContext m_HidContext;
    dmInput::HContext m_InputContext;
    dmInputDDF::GamepadMaps* m_GamepadMapsDDF;
    dmGameSystem::SpriteContext m_SpriteContext;
    dmGameSystem::CollectionProxyContext m_CollectionProxyContext;
    dmGameSystem::FactoryContext m_FactoryContext;
    dmGameSystem::CollectionFactoryContext m_CollectionFactoryContext;
    dmGameSystem::ModelContext m_ModelContext;
    dmGameSystem::SpineModelContext m_SpineModelContext;
    dmGameSystem::LabelContext m_LabelContext;
    dmGameObject::ModuleContext m_ModuleContext;
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
    virtual ~ResourceFailTest() {}
};

class ComponentTest : public GamesysTest<const char*>
{
public:
    virtual ~ComponentTest() {}
};

class ComponentFailTest : public GamesysTest<const char*>
{
public:
    virtual ~ComponentFailTest() {}
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
    virtual ~FactoryTest() {}
};

struct CollectionFactoryTestParams
{
    const char* m_GOPath;
    bool m_IsDynamic;
    bool m_IsPreloaded;
};

class CollectionFactoryTest : public GamesysTest<CollectionFactoryTestParams>
{
public:
    virtual ~CollectionFactoryTest() {}
};

class SpriteAnimTest : public GamesysTest<const char*>
{
public:
    virtual ~SpriteAnimTest() {}
};

class WindowEventTest : public GamesysTest<const char*>
{
public:
    virtual ~WindowEventTest() {}
};

struct DrawCountParams
{
    const char* m_GOPath;
    uint64_t m_ExpectedDrawCount;
};

class DrawCountTest : public GamesysTest<DrawCountParams>
{
public:
    virtual ~DrawCountTest() {}
};

struct TexturePropParams
{
    const char* go_path;
    dmhash_t comp_same_1;
    dmhash_t comp_same_2;
    dmhash_t comp_different;
};

class TexturePropTest : public GamesysTest<TexturePropParams>
{
protected:
    void SetUp()
    {
        GamesysTest::SetUp();
        hash_property_id = dmHashString64("texture0");
        hash_property_id_invalid = dmHashString64("texture");
    }

public:
    dmhash_t hash_property_id;
    dmhash_t hash_property_id_invalid;
    virtual ~TexturePropTest() {}
};

bool CopyResource(const char* src, const char* dst);
bool UnlinkResource(const char* name);

template<typename T>
void GamesysTest<T>::SetUp()
{
    dmSound::Initialize(0x0, 0x0);

    m_UpdateContext.m_DT = 1.0f / 60.0f;

    dmResource::NewFactoryParams params;
    params.m_MaxResources = 32;
    params.m_Flags = RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT;
    m_Factory = dmResource::NewFactory(&params, "build/default/src/gamesys/test");
    m_ScriptContext = dmScript::NewContext(0, m_Factory, true);
    dmScript::Initialize(m_ScriptContext);
    dmGameObject::Initialize(m_Register, m_ScriptContext);
    m_Register = dmGameObject::NewRegister();
    dmGameObject::RegisterResourceTypes(m_Factory, m_Register, m_ScriptContext, &m_ModuleContext);
    dmGameObject::RegisterComponentTypes(m_Factory, m_Register, m_ScriptContext);

    m_GraphicsContext = dmGraphics::NewContext(dmGraphics::ContextParams());
    dmRender::RenderContextParams render_params;
    render_params.m_MaxRenderTypes = 10;
    render_params.m_MaxInstances = 1000;
    render_params.m_MaxRenderTargets = 10;
    render_params.m_ScriptContext = m_ScriptContext;
    m_RenderContext = dmRender::NewRenderContext(m_GraphicsContext, render_params);
    m_GuiContext.m_RenderContext = m_RenderContext;
    m_GuiContext.m_ScriptContext = m_ScriptContext;
    dmGui::NewContextParams gui_params;
    gui_params.m_ScriptContext = m_ScriptContext;
    gui_params.m_GetURLCallback = dmGameSystem::GuiGetURLCallback;
    gui_params.m_GetUserDataCallback = dmGameSystem::GuiGetUserDataCallback;
    gui_params.m_ResolvePathCallback = dmGameSystem::GuiResolvePathCallback;
    m_GuiContext.m_GuiContext = dmGui::NewContext(&gui_params);
    m_GuiContext.m_MaxParticleFXCount = 64;
    m_GuiContext.m_MaxParticleCount = 1024;

    m_HidContext = dmHID::NewContext(dmHID::NewContextParams());
    dmHID::Init(m_HidContext);
    dmInput::NewContextParams input_params;
    input_params.m_HidContext = m_HidContext;
    input_params.m_RepeatDelay = 0.3f;
    input_params.m_RepeatInterval = 0.1f;
    m_InputContext = dmInput::NewContext(input_params);

    memset(&m_PhysicsContext, 0, sizeof(m_PhysicsContext));
    m_PhysicsContext.m_3D = false;
    m_PhysicsContext.m_Context2D = dmPhysics::NewContext2D(dmPhysics::NewContextParams());

    m_ParticleFXContext.m_Factory = m_Factory;
    m_ParticleFXContext.m_RenderContext = m_RenderContext;
    m_ParticleFXContext.m_MaxParticleFXCount = 64;
    m_ParticleFXContext.m_MaxParticleCount = 256;

    m_SpriteContext.m_RenderContext = m_RenderContext;
    m_SpriteContext.m_MaxSpriteCount = 32;

    m_CollectionProxyContext.m_Factory = m_Factory;
    m_CollectionProxyContext.m_MaxCollectionProxyCount = 8;

    m_FactoryContext.m_MaxFactoryCount = 128;
    m_FactoryContext.m_ScriptContext = m_ScriptContext;
    m_CollectionFactoryContext.m_MaxCollectionFactoryCount = 128;
    m_CollectionFactoryContext.m_ScriptContext = m_ScriptContext;

    m_SpineModelContext.m_RenderContext = m_RenderContext;
    m_SpineModelContext.m_Factory = m_Factory;
    m_SpineModelContext.m_MaxSpineModelCount = 32;

    m_LabelContext.m_RenderContext = m_RenderContext;
    m_LabelContext.m_MaxLabelCount = 32;
    m_LabelContext.m_Subpixels     = 0;

    m_ModelContext.m_RenderContext = m_RenderContext;
    m_ModelContext.m_Factory = m_Factory;
    m_ModelContext.m_MaxModelCount = 128;

    dmResource::Result r = dmGameSystem::RegisterResourceTypes(m_Factory, m_RenderContext, &m_GuiContext, m_InputContext, &m_PhysicsContext);
    assert(dmResource::RESULT_OK == r);

    dmResource::Get(m_Factory, "/input/valid.gamepadsc", (void**)&m_GamepadMapsDDF);
    assert(m_GamepadMapsDDF);
    dmInput::RegisterGamepads(m_InputContext, m_GamepadMapsDDF);

    assert(dmGameObject::RESULT_OK == dmGameSystem::RegisterComponentTypes(m_Factory, m_Register, m_RenderContext, &m_PhysicsContext, &m_ParticleFXContext, &m_GuiContext, &m_SpriteContext, &m_CollectionProxyContext, &m_FactoryContext, &m_CollectionFactoryContext, &m_SpineModelContext, &m_ModelContext, &m_LabelContext));

    m_Collection = dmGameObject::NewCollection("collection", m_Factory, m_Register, 1024, 4);
}

template<typename T>
void GamesysTest<T>::TearDown()
{
    dmGameObject::DeleteCollection(m_Collection);
    dmGameObject::PostUpdate(m_Register);
    dmResource::Release(m_Factory, m_GamepadMapsDDF);
    dmGui::DeleteContext(m_GuiContext.m_GuiContext, m_ScriptContext);
    dmRender::DeleteRenderContext(m_RenderContext, m_ScriptContext);
    dmGraphics::DeleteContext(m_GraphicsContext);
    dmScript::Finalize(m_ScriptContext);
    dmScript::DeleteContext(m_ScriptContext);
    dmResource::DeleteFactory(m_Factory);
    dmGameObject::DeleteRegister(m_Register);
    dmSound::Finalize();
    dmInput::DeleteContext(m_InputContext);
    dmHID::Final(m_HidContext);
    dmHID::DeleteContext(m_HidContext);
    dmPhysics::DeleteContext2D(m_PhysicsContext.m_Context2D);
}
