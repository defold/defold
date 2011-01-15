#include "test_gamesys.h"

const char* GamesysTest::ROOT = "build/default/src/gamesys/test";

void GamesysTest::SetUp()
{
    dmSound::Initialize(0x0, 0x0);
    dmGameObject::Initialize();

    m_UpdateContext.m_DT = 1.0f / 60.0f;

    dmResource::NewFactoryParams params;
    params.m_MaxResources = 16;
    params.m_Flags = RESOURCE_FACTORY_FLAGS_EMPTY;
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

    assert(dmResource::FACTORY_RESULT_OK == dmGameSystem::RegisterResourceTypes(m_Factory, m_RenderContext));

    memset(&m_PhysicsContext, 0, sizeof(m_PhysicsContext));
    memset(&m_EmitterContext, 0, sizeof(m_EmitterContext));

    m_EmitterContext.m_RenderContext = m_RenderContext;

    assert(dmGameObject::RESULT_OK == dmGameSystem::RegisterComponentTypes(m_Factory, m_Register, m_RenderContext, &m_PhysicsContext, &m_EmitterContext));

    m_Collection = dmGameObject::NewCollection(m_Factory, m_Register, 1024);
}

void GamesysTest::TearDown()
{
    dmRender::DeleteRenderContext(m_RenderContext);
    dmGraphics::DeleteContext(m_GraphicsContext);
    dmGameObject::DeleteCollection(m_Collection);
    dmResource::DeleteFactory(m_Factory);
    dmGameObject::DeleteRegister(m_Register);
    dmGameObject::Finalize();
    dmSound::Finalize();
}
