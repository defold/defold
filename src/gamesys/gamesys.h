#ifndef DM_GAMESYS_H
#define DM_GAMESYS_H

#include <dlib/configfile.h>

#include <resource/resource.h>

#include <gameobject/gameobject.h>

#include <gui/gui.h>

#include <render/render.h>
#include <render/font_renderer.h>
#include <physics/physics.h>

namespace dmGameSystem
{
    struct PhysicsContext
    {
        bool m_Debug;
    };

    struct EmitterContext
    {
        dmRender::HRenderContext m_RenderContext;
        uint32_t m_MaxEmitterCount;
        uint32_t m_MaxParticleCount;
        bool m_Debug;
    };

    struct RenderScriptPrototype
    {
        dmArray<dmRender::HMaterial>    m_Materials;
        dmhash_t                        m_NameHash;
        dmRender::HRenderScriptInstance m_Instance;
        dmRender::HRenderScript         m_Script;
    };

    struct GuiRenderContext
    {
        dmRender::HRenderContext    m_RenderContext;
        dmGui::HContext             m_GuiContext;
    };

    void RegisterDDFTypes();

    dmResource::FactoryResult RegisterResourceTypes(dmResource::HFactory factory, dmRender::HRenderContext render_context, dmGui::HContext gui_context);

    dmGameObject::Result RegisterComponentTypes(dmResource::HFactory factory,
                                                  dmGameObject::HRegister regist,
                                                  dmRender::HRenderContext render_context,
                                                  PhysicsContext* physics_context,
                                                  EmitterContext* emitter_context,
                                                  GuiRenderContext* gui_render_context);

    void RequestRayCast(dmGameObject::HCollection collection, dmGameObject::HInstance instance, const Vectormath::Aos::Point3& from, const Vectormath::Aos::Point3& to, uint32_t mask);
}

#endif // DM_GAMESYS_H
