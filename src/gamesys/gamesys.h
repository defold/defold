#ifndef DM_GAMESYS_H
#define DM_GAMESYS_H

#include <dlib/configfile.h>

#include <resource/resource.h>

#include <gameobject/gameobject.h>

#include <render/render.h>
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
        uint32_t m_ParticleRenderType;
        bool m_Debug;
    };

    void RegisterDDFTypes();

    dmResource::FactoryResult RegisterResourceTypes(dmResource::HFactory factory);

    dmGameObject::Result RegisterComponentTypes(dmResource::HFactory factory,
                                                  dmGameObject::HRegister regist,
                                                  dmRender::HRenderContext render_context,
                                                  PhysicsContext* physics_context,
                                                  EmitterContext* emitter_context);

    void RequestRayCast(dmGameObject::HCollection collection, dmGameObject::HInstance instance, const Vectormath::Aos::Point3& from, const Vectormath::Aos::Point3& to, uint32_t mask);
}

#endif // DM_GAMESYS_H
