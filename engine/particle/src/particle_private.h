
#ifndef DM_PARTICLE_PRIVATE_H
#define DM_PARTICLE_PRIVATE_H

#include <dlib/configfile.h>
#include <dlib/index_pool.h>
#include <dlib/transform.h>

#include "particle/particle_ddf.h"

namespace dmParticle
{
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
#define GET_SET(property, type)\
        inline type Get##property() const { return m_##property; }\
        inline void Set##property(type v) { m_##property = v; }\

        GET_SET(Position, Point3)
        GET_SET(SourceRotation, Quat)
        GET_SET(Rotation, Quat)
        GET_SET(Velocity, Vector3)
        GET_SET(TimeLeft, float)
        GET_SET(MaxLifeTime, float)
        GET_SET(ooMaxLifeTime, float)
        GET_SET(SpreadFactor, float)
        GET_SET(SourceSize, float)
        GET_SET(Scale, float)
        GET_SET(SourceColor, Vector4)
        GET_SET(Color, Vector4)
        GET_SET(SortKey, SortKey)
#undef GET_SET

    private:
        /// Position, which is defined in emitter space or world space depending on how the emitter which spawned the particles is tweaked.
        Point3 m_Position;
        /// Rotation, which is defined in emitter space or world space depending on how the emitter which spawned the particles is tweaked.
        Quat m_SourceRotation;
        Quat m_Rotation;
        /// Velocity of the particle
        Vector3 m_Velocity;
        /// Time left before the particle dies.
        float       m_TimeLeft;
        /// The duration of this particle.
        float       m_MaxLifeTime;
        /// Inverted duration.
        float       m_ooMaxLifeTime;
        /// Factor used for spread
        float       m_SpreadFactor;
        /// Particle source size
        float       m_SourceSize;
        /// Particle scale
        float       m_Scale;
        // Particle color
        Vector4     m_SourceColor;
        Vector4     m_Color;
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
        dmArray<RenderConstant> m_RenderConstants;
        Vector3                 m_Velocity;
        Point3                  m_LastPosition;
        dmhash_t                m_Id;
        EmitterRenderData       m_RenderData;
        /// Vertex index of the render data for the particles spawned by this emitter.
        uint32_t                m_VertexIndex;
        /// Number of vertices of the render data for the particles spawned by this emitter.
        uint32_t                m_VertexCount;
        /// Used to see when the emitter should stop spawning particles.
        float                   m_Timer;
        /// The amount of particles to spawn. It is accumulated over frames to handle spawn rates below the timestep.
        float                   m_ParticlesToSpawn;
        /// Seed used to ensure a deterministic simulation
        uint32_t                m_OriginalSeed;
        uint32_t                m_Seed;
        /// Which state the emitter is currently in
        EmitterState            m_State;
        /// Duration with spread applied, calculated on emitter creation.
        float                   m_Duration;
        /// Start delay with spread applied, calculated on emitter creation.
        float                   m_StartDelay;
        /// Particle spawn rate spread, randomized on emitter creation and used for the duration of the emitter.
        float                   m_SpawnRateSpread;
        /// If the user has been warned that all particles cannot be rendered.
        uint16_t                m_RenderWarning : 1;
        /// If the user has been warned that the emitters animation could not be fetched
        uint16_t                m_FetchAnimWarning : 1;
        uint16_t                m_LastPositionSet : 1;
        /// If this emitter is retiring, if set it means that a looping instance should act like a once instance
        uint16_t                m_Retiring : 1;
        /// If this emitter needs to be rehashed
        uint16_t                m_ReHash : 1;
    };

    struct Instance
    {
        Instance()
        : m_Emitters()
        , m_NumAwakeEmitters(0)
        , m_Prototype(0x0)
        , m_EmitterStateChangedData()
        , m_PlayTime(0.0f)
        , m_VersionNumber(0)
        , m_ScaleAlongZ(0)
        {
            m_WorldTransform.SetIdentity();
        }

        /// Emitter buffer
        dmArray<Emitter>        m_Emitters;
        /// Number of awake emitters
        uint32_t                m_NumAwakeEmitters;
        /// World transform of the emitter.
        dmTransform::TransformS1 m_WorldTransform;
        /// DDF resource.
        Prototype*              m_Prototype;
        /// Emitter state changed callback
        EmitterStateChangedData m_EmitterStateChangedData;
        /// Used when reloading to fast forward new emitters
        float                   m_PlayTime;
        /// Version number used to check that the handle is still valid.
        uint16_t                m_VersionNumber;
        /// Whether the scale of the world transform should be used along Z.
        uint16_t                m_ScaleAlongZ : 1;
    };

    /**
     * Representation of a context to hold a set of emitters.
     */
    struct Context
    {
        Context(uint32_t max_instance_count, uint32_t max_particle_count)
        : m_MaxParticleCount(max_particle_count)
        , m_NextVersionNumber(1)
        , m_InstanceSeeding(0)
        {
            memset(&m_Stats, 0, sizeof(m_Stats));
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
        /// Instance seeding to avoid same frame instances to look the same.
        uint16_t            m_InstanceSeeding;
        /// Stats
        Stats               m_Stats;
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
        float m_Spread;
    };

    struct ModifierPrototype
    {
        Property m_Properties[dmParticleDDF::MODIFIER_KEY_COUNT];
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
        Property                    m_Properties[dmParticleDDF::EMITTER_KEY_COUNT];
        /// Particle properties
        Property                    m_ParticleProperties[dmParticleDDF::PARTICLE_KEY_COUNT];
        dmArray<ModifierPrototype>  m_Modifiers;
        dmhash_t                    m_Animation;
        /// Tile source to use when rendering particles.
        void*                       m_TileSource;
        /// Material to use when rendering particles.
        void*                       m_Material;
        /// Blend mode
        dmParticleDDF::BlendMode    m_BlendMode;
        /// The max life time possible of a particle (used for quantizing particle life time when sorting)
        float                   m_MaxParticleLifeTime;
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

    void UpdateRenderData(HParticleContext context, HInstance instance, uint32_t emitter_index);
}

#endif // DM_PARTICLE_PRIVATE_H
