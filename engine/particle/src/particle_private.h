#ifndef DM_PARTICLE_PRIVATE_H
#define DM_PARTICLE_PRIVATE_H

#include <dlib/configfile.h>
#include <dlib/index_pool.h>

#include "particle/particle_ddf.h"

namespace dmParticle
{
    /// Number of float fields that define a vertex; 3 position, 2 uv, 1 alpha.
    static const uint32_t VERTEX_FIELD_COUNT        = 6;
    /// Byte size of a vertex.
    static const uint32_t VERTEX_SIZE               = sizeof(float) * VERTEX_FIELD_COUNT;

    /**
     * Representation of a particle.
     *
     * TODO Separate source state from current (chaining modifiers)
     */
    struct Particle
    {
        /// Position, which is defined in emitter space or world space depending on how the emitter which spawned the particles is tweaked.
        Vectormath::Aos::Point3 m_Position;
        /// Rotation, which is defined in emitter space or world space depending on how the emitter which spawned the particles is tweaked.
        Vectormath::Aos::Quat m_Rotation;
        /// Velocity of the particle
        Vectormath::Aos::Vector3 m_Velocity;
        /// Time left before the particle dies.
        float       m_TimeLeft;
        /// The duration of this particle.
        float       m_MaxLifeTime;
        /// Inverted duration.
        float       m_ooMaxLifeTime;
        /// Particle size
        float       m_Size;
        /// Particle alpha
        float       m_Alpha;
    };

    /**
     * Representation of an emitter.
     */
    struct Emitter
    {
        Emitter()
        : m_Prototype(0x0)
        , m_Position(0.0f, 0.0f, 0.0f)
        , m_Rotation(0.0f, 0.0f, 0.0f, 1.0f)
        , m_VertexIndex(0)
        , m_VertexCount(0)
        , m_Timer(0.0f)
        , m_SpawnTimer(0.0f)
        , m_SpawnDelay(0.0f)
        , m_ParticleTimeLeft(0.0f)
        , m_Gain(1.0f)
        , m_VersionNumber(0)
        , m_IsSpawning(0)
        , m_Dangling(0)
        , m_RenderWarning(0)
        , m_ResizeWarning(0)
        {

        }

        /// Emitter resource.
        Prototype*              m_Prototype;
        /// World position of the emitter.
        Vectormath::Aos::Point3 m_Position;
        /// World rotation of the emitter.
        Vectormath::Aos::Quat   m_Rotation;
        /// Particle buffer.
        dmArray<Particle>       m_Particles;
        /// Vertex index of the render data for the particles spawned by this emitter.
        uint32_t                m_VertexIndex;
        /// Number of vertices of the render data for the particles spawned by this emitter.
        uint32_t                m_VertexCount;
        /// Used to see when the emitter should stop spawning particles.
        float                   m_Timer;
        /// Used to see when the emitter should spawn a new particle.
        float                   m_SpawnTimer;
        /// How long to wait before the next particle spawns. This value might change for each particle spawned.
        float                   m_SpawnDelay;
        /// The time left before the particle dies which has the longest time left to live.
        float                   m_ParticleTimeLeft;
        /// Gain signal to control the modification of the different emitter properties.
        float                   m_Gain;
        /// Version number used to check that the handle is still valid.
        uint16_t                m_VersionNumber;
        /// If the emitter is still spawning particles.
        uint16_t                m_IsSpawning : 1;
        /// True for emitters who are create through FireAndForget.
        uint16_t                m_Dangling : 1;
        /// If the user has been warned that all particles cannot be rendered.
        uint16_t                m_RenderWarning : 1;
        /// If the user has been warned that the emitters particle buffer could not be resized as a result from a reload.
        uint16_t                m_ResizeWarning : 1;
    };

    /**
     * Representation of a context to hold a set of emitters.
     */
    struct Context
    {
        Context(uint32_t max_emitter_count, uint32_t max_particle_count)
        : m_NextVersionNumber(1)
        {
            m_Emitters.SetCapacity(max_emitter_count);
            m_Emitters.SetSize(max_emitter_count);
            for (uint32_t i = 0; i < m_Emitters.Size(); ++i)
            {
                m_Emitters[i] = 0;
            }
            m_EmitterIndexPool.SetCapacity(max_emitter_count);

            m_MaxParticleCount = max_particle_count;
        }

        ~Context()
        {

        }

        /// Emitter buffer.
        dmArray<Emitter*>   m_Emitters;
        /// Index pool used to index the emitter buffer.
        dmIndexPool16       m_EmitterIndexPool;
        /// Maximum number of particles allowed
        uint32_t            m_MaxParticleCount;
        /// Version number used to create new handles.
        uint16_t            m_NextVersionNumber;
    };
}

#endif // DM_PARTICLE_PRIVATE_H
