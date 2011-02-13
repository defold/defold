#include <gtest/gtest.h>

#include <resource/resource.h>

#include <sound/sound.h>
#include <gameobject/gameobject.h>

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

    dmGraphics::HContext m_GraphicsContext;
    dmRender::HRenderContext m_RenderContext;
    dmGameSystem::PhysicsContext m_PhysicsContext;
    dmGameSystem::EmitterContext m_EmitterContext;
    dmGameSystem::GuiRenderContext m_GuiRenderContext;
    dmInput::HContext m_InputContext;
    dmInputDDF::GamepadMaps* m_GamepadMapsDDF;
    dmGameSystem::SpriteContext m_SpriteContext;
};

class ResourceTest : public GamesysTest<const char*>
{

};

struct ResourceFailParams
{
    const char* m_ValidResource;
    const char* m_InvalidResource;
};

class ResourceFailTest : public GamesysTest<ResourceFailParams>
{

};

class ComponentTest : public GamesysTest<const char*>
{

};

class ComponentFailTest : public GamesysTest<const char*>
{

};

bool CopyResource(const char* src, const char* dst);
bool UnlinkResource(const char* name);

template<typename T>
void GamesysTest<T>::SetUp()
{
    dmSound::Initialize(0x0, 0x0);
    dmGameObject::Initialize();

    m_UpdateContext.m_DT = 1.0f / 60.0f;

    dmResource::NewFactoryParams params;
    params.m_MaxResources = 16;
    params.m_Flags = RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT;
    m_Factory = dmResource::NewFactory(&params, "build/default/src/gamesys/test");
    m_Register = dmGameObject::NewRegister(0, 0);
    dmGameObject::RegisterResourceTypes(m_Factory, m_Register);
    dmGameObject::RegisterComponentTypes(m_Factory, m_Register);

    m_GraphicsContext = dmGraphics::NewContext();
    dmRender::RenderContextParams render_params;
    render_params.m_MaxRenderTypes = 10;
    render_params.m_MaxInstances = 1000;
    render_params.m_MaxRenderTargets = 10;
    m_RenderContext = dmRender::NewRenderContext(m_GraphicsContext, render_params);
    m_GuiRenderContext.m_RenderContext = m_RenderContext;
    dmGui::NewContextParams gui_params;
    gui_params.m_MaxMessageDataSize = 256;
    assert(dmMessage::RESULT_OK == dmMessage::NewSocket("test", &gui_params.m_Socket));
    m_GuiRenderContext.m_GuiContext = dmGui::NewContext(&gui_params);

    m_InputContext = dmInput::NewContext(0.3f, 0.1f);

    memset(&m_PhysicsContext, 0, sizeof(m_PhysicsContext));
    m_PhysicsContext.m_Context = dmPhysics::NewContext(dmPhysics::NewContextParams());

    memset(&m_EmitterContext, 0, sizeof(m_EmitterContext));
    m_EmitterContext.m_RenderContext = m_RenderContext;

    m_SpriteContext.m_RenderContext = m_RenderContext;
    m_SpriteContext.m_MaxSpriteCount = 32;

    assert(dmResource::FACTORY_RESULT_OK == dmGameSystem::RegisterResourceTypes(m_Factory, m_RenderContext, m_GuiRenderContext.m_GuiContext, m_InputContext, m_PhysicsContext.m_Context));

    dmResource::Get(m_Factory, "input/valid.gamepadsc", (void**)&m_GamepadMapsDDF);
    assert(m_GamepadMapsDDF);
    dmInput::RegisterGamepads(m_InputContext, m_GamepadMapsDDF);

    assert(dmGameObject::RESULT_OK == dmGameSystem::RegisterComponentTypes(m_Factory, m_Register, m_RenderContext, &m_PhysicsContext, &m_EmitterContext, &m_GuiRenderContext, &m_SpriteContext));

    m_Collection = dmGameObject::NewCollection(m_Factory, m_Register, 1024);
}

template<typename T>
void GamesysTest<T>::TearDown()
{
    dmGameObject::DeleteCollection(m_Collection);
    dmMessage::DeleteSocket(dmGui::GetSocket(m_GuiRenderContext.m_GuiContext));
    dmResource::Release(m_Factory, m_GamepadMapsDDF);
    dmGui::DeleteContext(m_GuiRenderContext.m_GuiContext);
    dmRender::DeleteRenderContext(m_RenderContext);
    dmGraphics::DeleteContext(m_GraphicsContext);
    dmResource::DeleteFactory(m_Factory);
    dmGameObject::DeleteRegister(m_Register);
    dmGameObject::Finalize();
    dmSound::Finalize();
    dmInput::DeleteContext(m_InputContext);
    dmPhysics::DeleteContext(m_PhysicsContext.m_Context);
}
