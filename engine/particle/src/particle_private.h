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
    /// Number of samples per property (spline => linear segments)
    static const uint32_t PROPERTY_SAMPLE_COUNT     = 64;

    struct EmitterPrototype;
    struct Prototype;

    /**
     * Key when sorting particles, based on life time with additional index for stable sort
     */
    union SortKey
    {
        struct
        {
            uint32_t m_Index : 16;  // Index is used to ensure stable sort
            uint32_t m_LifeTime : 16; // Quantified relative life time
        };
        uint32_t     m_Key;
    };

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
        float       m_SourceSize;
        float       m_Size;
        /// Particle alpha
        float       m_SourceAlpha;
        float       m_Alpha;
        // Sorting
        SortKey     m_SortKey;
    };

    /**
     * Representation of an emitter.
     */
    struct Emitter
    {
        Emitter()
        {
            memset(this, 0, sizeof(Emitter));
        }

        AnimationData           m_AnimationData;
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
        /// The max life time of any particle spawned so far (used for quantizing particle life time when sorting)
        float                   m_MaxParticleLifeTime;
        /// If the emitter is still spawning particles.
        uint16_t                m_IsSpawning : 1;
        /// If the user has been warned that all particles cannot be rendered.
        uint16_t                m_RenderWarning : 1;
        /// If the user has been warned that the emitters animation could not be fetched
        uint16_t                m_FetchAnimWarning : 1;
    };

    struct Instance
    {
        Instance()
        : m_Emitters()
        , m_Position(0.0f, 0.0f, 0.0f)
        , m_Rotation(0.0f, 0.0f, 0.0f, 1.0f)
        , m_Prototype(0x0)
        , m_VersionNumber(0)
        , m_Dangling(0)
        {

        }

        // Emitter buffer
        dmArray<Emitter>        m_Emitters;
        /// World position of the emitter.
        Vectormath::Aos::Point3 m_Position;
        /// World rotation of the emitter.
        Vectormath::Aos::Quat   m_Rotation;
        /// DDF resource.
        Prototype*              m_Prototype;
        /// Version number used to check that the handle is still valid.
        uint16_t                m_VersionNumber;
        /// True for instances who are create through FireAndForget.
        uint16_t                m_Dangling : 1;
    };

    /**
     * Representation of a context to hold a set of emitters.
     */
    struct Context
    {
        Context(uint32_t max_instance_count, uint32_t max_particle_count)
        : m_MaxParticleCount(max_particle_count)
        , m_NextVersionNumber(1)
        {
            m_Instances.SetCapacity(max_instance_count);
            m_Instances.SetSize(max_instance_count);
            if (max_instance_count > 0)
            {
                memset(&m_Instances.Front(), 0, max_instance_count * sizeof(Instance*));
            }
            m_InstanceIndexPool.SetCapacity(max_instance_count);
        }

        ~Context()
        {

        }

        /// Instance buffer.
        dmArray<Instance*>  m_Instances;
        /// Index pool used to index the instance buffer.
        dmIndexPool16       m_InstanceIndexPool;
        /// Maximum number of particles allowed
        uint32_t            m_MaxParticleCount;
        /// Version number used to create new handles.
        uint16_t            m_NextVersionNumber;
    };

    struct LinearSegment
    {
        float m_X;
        float m_Y;
        float m_K;
    };

    struct Property
    {
        LinearSegment m_Segments[PROPERTY_SAMPLE_COUNT];
    };

    /**
     * Representation of an emitter resource.
     *
     * NOTE The size of the properties-arrays is roughly 10 kB.
     */
    struct EmitterPrototype
    {
        EmitterPrototype()
        : m_TileSource(0)
        , m_Material(0)
        {

        }

        /// Emitter properties
        Property                m_Properties[dmParticleDDF::EMITTER_KEY_COUNT];
        /// Particle properties
        Property                m_ParticleProperties[dmParticleDDF::PARTICLE_KEY_COUNT];
        dmhash_t                m_Animation;
        /// Tile source to use when rendering particles.
        void*                   m_TileSource;
        /// Material to use when rendering particles.
        void*                   m_Material;
    };

    /**
     * Representation of an instance resource.
     */
    struct Prototype
    {
        Prototype()
        : m_Emitters()
        , m_DDF(0x0)
        {
        }

        /// Emitter prototypes
        dmArray<EmitterPrototype>   m_Emitters;
        /// DDF structure read from the resource.
        dmParticleDDF::ParticleFX*  m_DDF;
    };

}

#endif // DM_PARTICLE_PRIVATE_H
