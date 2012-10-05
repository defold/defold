#include <string.h>
#include <stdint.h>
#include <algorithm>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/profile.h>

#include "particle.h"
#include "particle_private.h"

namespace dmParticle
{
    using namespace dmParticleDDF;
    using namespace Vectormath::Aos;

    struct Vertex
    {
        float m_Position[3];
        float m_UV[2];
        float m_Red, m_Green, m_Blue, m_Alpha;
    };

    /// Config key to use for tweaking maximum number of instances in a context.
    const char* MAX_INSTANCE_COUNT_KEY        = "particle_system.max_instance_count";
    /// Config key to use for tweaking the total maximum number of particles in a context.
    const char* MAX_PARTICLE_COUNT_KEY       = "particle_system.max_particle_count";

    AnimationData::AnimationData()
    {
        memset(this, 0, sizeof(*this));
    }

    HContext CreateContext(uint32_t max_instance_count, uint32_t max_particle_count)
    {
        return new Context(max_instance_count, max_particle_count);
    }

    void DestroyContext(HContext context)
    {
        uint32_t lingering = 0;
        for (uint32_t i=0; i < context->m_Instances.Size(); ++i)
        {
            Instance* instance = context->m_Instances[i];
            if (instance != 0x0 && instance->m_Dangling == 0)
                ++lingering;
            delete instance;
        }
        if (lingering > 0)
            dmLogWarning("Destroyed %d client-owned emitters (this might indicate leakage).", lingering);
        delete context;
    }

    static Instance* GetInstance(HContext context, HInstance instance)
    {
        if (instance == INVALID_INSTANCE)
            return 0x0;
        uint16_t version = instance >> 16;
        Instance* i = context->m_Instances[instance & 0xffff];

        if (version != i->m_VersionNumber)
        {
            dmLogError("Stale instance handle");
            return 0;
        }
        return i;
    }

    double Hermite(float x0, float x1, float t0, float t1, float t) {
        return (2 * t * t * t - 3 * t * t + 1) * x0 +
               (t * t * t - 2 * t * t + t) * t0 +
               (- 2 * t * t * t + 3 * t * t) * x1 +
               (t * t * t - t * t) * t1;
    }

    float GetValue(const dmParticleDDF::SplinePoint* segments, int segment, float t)
    {
        SplinePoint p0 = segments[segment];
        SplinePoint p1 = segments[segment + 1];
        float dx = p1.m_X - p0.m_X;

        float py0 = p0.m_Y;
        float py1 = p1.m_Y;
        float pt0 = dx * p0.m_TY / p0.m_TX;
        float pt1 = dx * p1.m_TY / p1.m_TX;

        return Hermite(py0, py1, pt0, pt1, t);
    }

    float GetY(const dmParticleDDF::SplinePoint* segments, uint32_t segment_count, float x)
    {
        if (segment_count == 1)
        {
            // Fall-back to linear interpolation
            const SplinePoint& p = *segments;
            return p.m_Y + (x - p.m_X) * p.m_TY / p.m_TX;
        }
        uint32_t segment_index = 0;
        float t = 0;
        for (uint32_t s = 0; s < segment_count - 1; ++s) {
            const SplinePoint& p0 = segments[s];
            const SplinePoint& p1 = segments[s + 1];
            if (x >= p0.m_X && x < p1.m_X) {
                t = (x - p0.m_X) / (p1.m_X - p0.m_X);
                segment_index = s;
                break;
            }
        }

        return GetValue(segments, segment_index, t);
    }

    void SampleProperty(const dmParticleDDF::SplinePoint* segments, uint32_t segments_count, LinearSegment* out_segments)
    {
        float dx = 1.0f / PROPERTY_SAMPLE_COUNT;
        float x0 = 0.0f;
        float y0 = GetY(segments, segments_count, x0);
        for (uint32_t j = 0; j < PROPERTY_SAMPLE_COUNT; ++j)
        {
            float y1 = GetY(segments, segments_count, x0 + dx);
            out_segments[j].m_X = x0;
            out_segments[j].m_Y = y0;
            out_segments[j].m_K = (y1 - y0) * PROPERTY_SAMPLE_COUNT;
            x0 += dx;
            y0 = y1;
        }
    }

    HInstance CreateInstance(HContext context, HPrototype prototype)
    {
        if (context->m_InstanceIndexPool.Remaining() == 0)
        {
            dmLogWarning("Instance could not be created since the buffer is full (%d). Tweak \"%s\" in the config file.", context->m_Instances.Capacity(), MAX_INSTANCE_COUNT_KEY);
            return 0;
        }
        dmParticleDDF::ParticleFX* ddf = prototype->m_DDF;
        uint32_t emitter_count = ddf->m_Emitters.m_Count;
        for (uint32_t i = 0; i < emitter_count; ++i)
        {
            dmParticleDDF::Emitter* emitter_ddf = &ddf->m_Emitters[i];
            if (emitter_ddf->m_MaxParticleCount > context->m_MaxParticleCount)
            {
                dmLogWarning("Instance could not be created since it has a higher particle count than the maximum number allowed (%d). Tweak \"%s\" in the config file.", context->m_MaxParticleCount, MAX_PARTICLE_COUNT_KEY);
                return 0;
            }
        }
        Instance* instance = new Instance;
        uint16_t index = context->m_InstanceIndexPool.Pop();

        // Avoid zero in order to ensure that HInstance != INVALID_INSTANCE for valid handles.
        if (context->m_NextVersionNumber == INVALID_INSTANCE) context->m_NextVersionNumber++;
        instance->m_VersionNumber = context->m_NextVersionNumber++;

        context->m_Instances[index] = instance;

        instance->m_Prototype = prototype;
        instance->m_Emitters.SetCapacity(emitter_count);
        instance->m_Emitters.SetSize(emitter_count);

        memset(instance->m_Emitters.Begin(), 0, emitter_count * sizeof(Emitter));
        for (uint32_t i = 0; i < emitter_count; ++i)
        {
            Emitter* emitter = &instance->m_Emitters[i];
            dmParticleDDF::Emitter* emitter_ddf = &ddf->m_Emitters[i];
            uint32_t particle_count = emitter_ddf->m_MaxParticleCount;
            emitter->m_Particles.SetCapacity(particle_count);
        }

        return instance->m_VersionNumber << 16 | index;
    }

    void DestroyInstance(HContext context, HInstance instance)
    {
        if (instance == INVALID_INSTANCE) return;
        Instance* i = GetInstance(context, instance);
        if (!i) return;
        uint32_t index = instance & 0xffff;
        context->m_InstanceIndexPool.Push(index);
        context->m_Instances[index] = 0;
        uint32_t emitter_count = i->m_Emitters.Size();
        for (uint32_t emitter_i = 0; emitter_i < emitter_count; ++emitter_i)
        {
            i->m_Emitters[emitter_i].m_Particles.SetCapacity(0);
        }
        delete i;
    }

    void ReloadInstance(HContext context, HInstance instance)
    {
        if (instance == INVALID_INSTANCE) return;
        Instance* i = GetInstance(context, instance);
        if (!i) return;
        dmArray<Emitter>& emitters = i->m_Emitters;
        uint32_t emitter_count = emitters.Size();
        Prototype* prototype = i->m_Prototype;
        dmParticleDDF::ParticleFX* ddf = prototype->m_DDF;
        uint32_t prototype_emitter_count = prototype->m_Emitters.Size();
        if (emitter_count != prototype_emitter_count)
        {
            // Deallocate particle data if we are shrinking
            if (prototype_emitter_count < emitter_count)
            {
                for (uint32_t emitter_i = prototype_emitter_count; emitter_i < emitter_count; ++emitter_i)
                {
                    emitters[emitter_i].m_Particles.SetCapacity(0);
                }
            }
            bool spawning = IsSpawning(context, instance);
            emitters.SetCapacity(prototype_emitter_count);
            emitters.SetSize(prototype_emitter_count);
            // Make sure old particle buffers have the right capacity
            uint32_t old_count = dmMath::Min(emitter_count, prototype_emitter_count);
            for (uint32_t emitter_i = 0; emitter_i < old_count; ++emitter_i)
            {
                emitters[emitter_i].m_Particles.SetCapacity(ddf->m_Emitters[emitter_i].m_MaxParticleCount);
            }
            // Initialize emitters if we have grown
            if (emitter_count < prototype_emitter_count)
            {
                memset(&emitters.Begin()[emitter_count], 0, (prototype_emitter_count - emitter_count) * sizeof(Emitter));
                for (uint32_t emitter_i = emitter_count; emitter_i < prototype_emitter_count; ++emitter_i)
                {
                    Emitter* emitter = &emitters[emitter_i];
                    dmParticleDDF::Emitter* emitter_ddf = &prototype->m_DDF->m_Emitters[emitter_i];
                    uint32_t particle_count = emitter_ddf->m_MaxParticleCount;
                    emitter->m_Particles.SetCapacity(particle_count);
                }
                ResetInstance(context, instance);
                if (spawning)
                    StartInstance(context, instance);
            }
        }
    }

    void StartInstance(HContext context, HInstance instance)
    {
        if (instance == INVALID_INSTANCE) return;
        Instance* i = GetInstance(context, instance);
        if (!i) return;
        dmArray<Emitter>& emitters = i->m_Emitters;
        uint32_t emitter_count = emitters.Size();
        for (uint32_t emitter_i = 0; emitter_i < emitter_count; ++emitter_i)
        {
            // TODO: Fix auto-start
            emitters[emitter_i].m_IsSpawning = 1;
        }
    }

    void StopInstance(HContext context, HInstance instance)
    {
        if (instance == INVALID_INSTANCE) return;
        Instance* i = GetInstance(context, instance);
        if (!i) return;
        dmArray<Emitter>& emitters = i->m_Emitters;
        uint32_t emitter_count = emitters.Size();
        for (uint32_t emitter_i = 0; emitter_i < emitter_count; ++emitter_i)
        {
            emitters[emitter_i].m_IsSpawning = 0;
        }
    }

    void RestartInstance(HContext context, HInstance instance)
    {
        if (instance == INVALID_INSTANCE) return;
        Instance* i = GetInstance(context, instance);
        if (!i) return;
        dmArray<Emitter>& emitters = i->m_Emitters;
        uint32_t emitter_count = emitters.Size();
        for (uint32_t emitter_i = 0; emitter_i < emitter_count; ++emitter_i)
        {
            Emitter* emitter = &emitters[emitter_i];
            // TODO: Fix auto-start
            emitter->m_IsSpawning = 1;
            emitter->m_Timer = 0.0f;
            emitter->m_SpawnTimer = 0.0f;
            emitter->m_SpawnDelay = 0.0f;
        }
    }

    void ResetInstance(HContext context, HInstance instance)
    {
        if (instance == INVALID_INSTANCE) return;
        Instance* i = GetInstance(context, instance);
        if (!i) return;
        dmArray<Emitter>& emitters = i->m_Emitters;
        uint32_t emitter_count = emitters.Size();
        for (uint32_t emitter_i = 0; emitter_i < emitter_count; ++emitter_i)
        {
            Emitter* emitter = &emitters[emitter_i];
            // Save particles array
            dmArray<Particle> tmp;
            tmp.Swap(emitter->m_Particles);
            // Clear emitter
            memset(emitter, 0, sizeof(Emitter));
            // Restore particles
            tmp.Swap(emitter->m_Particles);
            // Remove living particles
            emitter->m_Particles.SetSize(0);
        }
    }

    void FireAndForget(HContext context, Prototype* prototype, Point3 position, Quat rotation)
    {
        if (prototype == 0x0)
            return;
        HInstance i = CreateInstance(context, prototype);
        Instance* instance = GetInstance(context, i);
        if (instance != 0x0)
        {
            dmArray<Emitter>& emitters = instance->m_Emitters;
            // A looping emitter would never be removed, destroy it and report error
            uint32_t emitter_count = prototype->m_Emitters.Size();
            dmParticleDDF::ParticleFX* ddf = prototype->m_DDF;
            for (uint32_t emitter_i = 0; emitter_i < emitter_count; ++emitter_i)
            {
                if (ddf->m_Emitters[emitter_i].m_Mode == PLAY_MODE_LOOP)
                {
                    dmLogError("An emitter is set to loop and can't be created in this manner, it would never be destroyed.");
                    DestroyInstance(context, i);
                    return;
                }
                emitters[emitter_i].m_IsSpawning = 1;
            }
            instance->m_Dangling = 1;
            instance->m_Position = position;
            instance->m_Rotation = rotation;
        }
    }

    void SetPosition(HContext context, HInstance instance, Point3 position)
    {
        Instance* i = GetInstance(context, instance);
        if (!i) return;
        i->m_Position = position;
    }

    void SetRotation(HContext context, HInstance instance, Quat rotation)
    {
        Instance* i = GetInstance(context, instance);
        if (!i) return;
        i->m_Rotation = rotation;
    }

    bool IsSpawning(Emitter* emitter)
    {
        return emitter->m_IsSpawning;
    }

    bool IsSpawning(HContext context, HInstance instance)
    {
        Instance* i = GetInstance(context, instance);
        if (!i) return false;

        bool is_spawning = false;
        dmArray<Emitter>& emitters = i->m_Emitters;
        uint32_t emitter_count = emitters.Size();
        for (uint32_t emitter_i = 0; emitter_i < emitter_count; ++emitter_i)
        {
            if (IsSpawning(&emitters[emitter_i]))
            {
                is_spawning = true;
                break;
            }
        }
        return is_spawning;
    }

    bool IsSleeping(Emitter* emitter, dmParticleDDF::Emitter* ddf)
    {
        if (emitter->m_IsSpawning || emitter->m_ParticleTimeLeft > 0.0f)
            return false;
        return true;
    }

    bool IsSleeping(Instance* instance)
    {
        // Consider 0x0 instances as sleeping
        if (!instance) return true;
        bool is_sleeping = true;
        dmArray<Emitter>& emitters = instance->m_Emitters;
        uint32_t emitter_count = emitters.Size();
        dmParticleDDF::ParticleFX* ddf = instance->m_Prototype->m_DDF;
        for (uint32_t emitter_i = 0; emitter_i < emitter_count; ++emitter_i)
        {
            if (!IsSleeping(&emitters[emitter_i], &ddf->m_Emitters[emitter_i]))
            {
                is_sleeping = false;
                break;
            }
        }
        return is_sleeping;
    }

    bool IsSleeping(HContext context, HInstance instance)
    {
        return IsSleeping(GetInstance(context, instance));
    }

    // helper functions in update
    static void FetchAnimation(Emitter* emitter, EmitterPrototype* prototype, FetchAnimationCallback fetch_animation_callback);
    static void Advance(Emitter* emitter, dmParticleDDF::Emitter* emitter_ddf, float dt);
    static void SpawnParticles(Instance* instance, Emitter* emitter, EmitterPrototype* prototype, dmParticleDDF::Emitter* ddf, float dt);
    static void EvaluateEmitterProperties(Emitter* emitter, Property* emitter_properties, float duration, float properties[EMITTER_KEY_COUNT]);
    static void EvaluateParticleProperties(Emitter* emitter, Property* particle_properties);
    static uint32_t UpdateRenderData(HContext context, Instance* instance, Emitter* emitter, dmParticleDDF::Emitter* ddf, uint32_t vertex_index, float* vertex_buffer, uint32_t vertex_buffer_size);
    static void GenerateKeys(Emitter* emitter);
    static void SortParticles(Emitter* emitter);
    static void Simulate(Emitter* emitter, EmitterPrototype* prototype, dmParticleDDF::Emitter* ddf, float dt);

    void Update(HContext context, float dt, float* vertex_buffer, uint32_t vertex_buffer_size, uint32_t* out_vertex_buffer_size, FetchAnimationCallback fetch_animation_callback)
    {
        DM_PROFILE(Particle, "Update");

        // vertex buffer index for each emitter
        uint32_t vertex_index = 0;

        uint32_t size = context->m_Instances.Size();
        for (uint32_t i = 0; i < size; i++)
        {
            Instance* instance = context->m_Instances[i];
            // empty slot
            if (instance == INVALID_INSTANCE) continue;
            // don't update sleeping instances
            if (IsSleeping(instance))
            {
                // delete fire and forget
                if (instance->m_Dangling)
                    DestroyInstance(context, instance->m_VersionNumber << 16 | i);
                else
                {
                    // don't render
                    uint32_t emitter_count = instance->m_Emitters.Size();
                    for (uint32_t emitter_i = 0; emitter_i < emitter_count; ++emitter_i)
                    {
                        instance->m_Emitters[emitter_i].m_VertexCount = 0;
                    }
                }
                continue;
            }

            Prototype* prototype = instance->m_Prototype;
            uint32_t emitter_count = instance->m_Emitters.Size();
            for (uint32_t emitter_i = 0; emitter_i < emitter_count; ++emitter_i)
            {
                Emitter* emitter = &instance->m_Emitters[emitter_i];
                EmitterPrototype* emitter_prototype = &prototype->m_Emitters[emitter_i];
                dmParticleDDF::Emitter* emitter_ddf = &prototype->m_DDF->m_Emitters[emitter_i];

                FetchAnimation(emitter, emitter_prototype, fetch_animation_callback);

                Advance(emitter, emitter_ddf, dt);

                SpawnParticles(instance, emitter, emitter_prototype, emitter_ddf, dt);

                GenerateKeys(emitter);
                SortParticles(emitter);

                Simulate(emitter, emitter_prototype, emitter_ddf, dt);

                // Render data
                if (vertex_buffer != 0x0 && vertex_buffer_size > 0)
                    vertex_index += UpdateRenderData(context, instance, emitter, emitter_ddf, vertex_index, vertex_buffer, vertex_buffer_size);
            }
        }
        if (out_vertex_buffer_size != 0x0)
        {
            *out_vertex_buffer_size = vertex_index * sizeof(Vertex);
        }
    }

    static void FetchAnimation(Emitter* emitter, EmitterPrototype* prototype, FetchAnimationCallback fetch_animation_callback)
    {
        DM_PROFILE(Particle, "FetchAnimation");

        // Needed to avoid autoread of AnimationData when calling java through JNA
        memset(&emitter->m_AnimationData, 0, sizeof(AnimationData));
        if (fetch_animation_callback != 0x0 && prototype->m_TileSource)
        {
            FetchAnimationResult result = fetch_animation_callback(prototype->m_TileSource, prototype->m_Animation, &emitter->m_AnimationData);
            if (result != FETCH_ANIMATION_OK)
            {
                if (!emitter->m_FetchAnimWarning)
                {
                    emitter->m_FetchAnimWarning = 1;
                    const char* anim = (const char*)dmHashReverse64(prototype->m_Animation, 0x0);
                    if (anim == 0x0)
                        anim = "<unknown>";
                    dmLogWarning("The animation '%s' could not be found", anim);
                }
            } else {
                assert(emitter->m_AnimationData.m_StructSize == sizeof(AnimationData) && "AnimationData::m_StructSize has an invalid size");
                emitter->m_FetchAnimWarning = 0;
            }
        }
    }

    static void Advance(Emitter* emitter, dmParticleDDF::Emitter* emitter_ddf, float dt)
    {
        DM_PROFILE(Particle, "Advance");

        // Step particle life, prune dead particles
        uint32_t particle_count = emitter->m_Particles.Size();
        uint32_t j = 0;
        while (j < particle_count)
        {
            Particle* p = &emitter->m_Particles[j];
            p->SetTimeLeft(p->GetTimeLeft() - dt);
            if (p->GetTimeLeft() < 0.0f)
            {
                // TODO Handle death-action
                emitter->m_Particles.EraseSwap(j);
                --particle_count;
            } else {
                ++j;
            }
        }
        if (emitter->m_ParticleTimeLeft > 0.0f)
            emitter->m_ParticleTimeLeft -= dt;

        // Step emitter life
        if (emitter->m_IsSpawning)
        {
            emitter->m_Timer += dt;

            // Avoid accumulating spawn timer while the emitter is choking on particles
            if (!emitter->m_Particles.Full() || emitter->m_SpawnTimer < emitter->m_SpawnDelay)
                emitter->m_SpawnTimer += dt;
            // stop once-emitters that have lived their life
            if (emitter_ddf->m_Mode == PLAY_MODE_ONCE && emitter->m_Timer > emitter_ddf->m_Duration)
                emitter->m_IsSpawning = 0;
            // wrap looping emitters when they reach the end
            if (emitter_ddf->m_Mode == PLAY_MODE_LOOP)
                emitter->m_Timer -= dmMath::Select(emitter->m_Timer - emitter_ddf->m_Duration, emitter_ddf->m_Duration, 0.0f);
        }
    }

    static void SpawnParticles(Instance* instance, Emitter* emitter, EmitterPrototype* prototype, dmParticleDDF::Emitter* ddf, float dt)
    {
        DM_PROFILE(Particle, "Spawn");
        if (emitter->m_IsSpawning)
        {
            while (!emitter->m_Particles.Full() && emitter->m_SpawnTimer >= emitter->m_SpawnDelay)
            {
                emitter->m_SpawnTimer -= emitter->m_SpawnDelay;

                uint32_t particle_count = emitter->m_Particles.Size();
                emitter->m_Particles.SetSize(particle_count + 1);
                Particle *particle = &emitter->m_Particles[particle_count];
                memset(particle, 0, sizeof(Particle));

                // TODO Handle birth-action

                float emitter_properties[EMITTER_KEY_COUNT];
                memset(emitter_properties, 0, sizeof(emitter_properties));
                EvaluateEmitterProperties(emitter, prototype->m_Properties, ddf->m_Duration, emitter_properties);

                emitter->m_SpawnDelay = emitter_properties[EMITTER_KEY_SPAWN_DELAY];
                particle->SetMaxLifeTime(emitter_properties[EMITTER_KEY_PARTICLE_LIFE_TIME]);
                particle->SetooMaxLifeTime(1.0f / particle->GetMaxLifeTime());
                // Include dt since already existing particles have already been advanced
                particle->SetTimeLeft(particle->GetMaxLifeTime() - dt);
                particle->SetSourceSize(emitter_properties[EMITTER_KEY_PARTICLE_SIZE]);
                particle->SetSourceColor(Vector4(
                        emitter_properties[EMITTER_KEY_PARTICLE_RED],
                        emitter_properties[EMITTER_KEY_PARTICLE_GREEN],
                        emitter_properties[EMITTER_KEY_PARTICLE_BLUE],
                        emitter_properties[EMITTER_KEY_PARTICLE_ALPHA]));

                emitter->m_MaxParticleLifeTime = dmMath::Max(emitter->m_MaxParticleLifeTime, particle->GetMaxLifeTime());
                emitter->m_ParticleTimeLeft = dmMath::Max(emitter->m_ParticleTimeLeft, particle->GetTimeLeft());

                Vector3 local_position;

                switch (ddf->m_Type)
                {
                    case EMITTER_TYPE_SPHERE:
                    {
                        Vectormath::Aos::Vector3 p = Vectormath::Aos::Vector3::zAxis();
                        Vectormath::Aos::Vector3 v(dmMath::Rand11(), dmMath::Rand11(), dmMath::Rand11());
                        if (lengthSqr(v) > 0.0f)
                            p = normalize(v);

                        local_position = p * emitter_properties[EMITTER_KEY_SIZE_X];

                        break;
                    }

                    case EMITTER_TYPE_CONE:
                    {
                        float height = emitter_properties[EMITTER_KEY_SIZE_X];
                        float radius = emitter_properties[EMITTER_KEY_SIZE_Y];
                        float angle = 2.0f * ((float) M_PI) * dmMath::RandOpen01();

                        local_position = Vector3(cosf(angle) * radius, sinf(angle) * radius, height);

                        break;
                    }

                    case EMITTER_TYPE_BOX:
                    {
                        float width = emitter_properties[EMITTER_KEY_SIZE_X];
                        float height = emitter_properties[EMITTER_KEY_SIZE_Y];
                        float depth = emitter_properties[EMITTER_KEY_SIZE_Z];
                        local_position = Vector3(dmMath::Rand11() * width, dmMath::Rand11() * height, dmMath::Rand11() * depth);

                        break;
                    }

                    default:
                        dmLogWarning("Unknown emitter type (%d), particle is spawned at emitter.", ddf->m_Type);
                        local_position = Vector3(0.0f, 0.0f, 0.0f);
                        break;
                }

                Vector3 velocity = Vector3::zAxis();
                if (lengthSqr(local_position) > 0.0f)
                    velocity = normalize(local_position);
                velocity *= emitter_properties[EMITTER_KEY_PARTICLE_SPEED];

                particle->SetPosition(ddf->m_Position + rotate(ddf->m_Rotation, local_position));
                particle->SetRotation(ddf->m_Rotation);
                particle->SetVelocity(rotate(ddf->m_Rotation, velocity));

                if (ddf->m_Space == EMISSION_SPACE_WORLD)
                {
                    particle->SetPosition(instance->m_Position + rotate(instance->m_Rotation, Vector3(particle->GetPosition())));
                    particle->SetRotation(instance->m_Rotation * particle->GetRotation());
                    particle->SetVelocity(rotate(instance->m_Rotation, particle->GetVelocity()));
                }
            }
        }
    }

    static float unit_tex_coords[] =
    {
            0.0f, 0.0f, 1.0f, 1.0f
    };

    static uint32_t UpdateRenderData(HContext context, Instance* instance, Emitter* emitter, dmParticleDDF::Emitter* ddf, uint32_t vertex_index, float* vertex_buffer, uint32_t vertex_buffer_size)
    {
        DM_PROFILE(Particle, "UpdateRenderData");

        emitter->m_VertexIndex = vertex_index;
        emitter->m_VertexCount = 0;

        // texture animation
        uint32_t start_tile = emitter->m_AnimationData.m_StartTile - 1;
        uint32_t end_tile = emitter->m_AnimationData.m_EndTile - 1;
        uint32_t tile_count = end_tile - start_tile + 1;
        float* tex_coords = emitter->m_AnimationData.m_TexCoords;

        if (tex_coords == 0x0)
        {
            tex_coords = unit_tex_coords;
            start_tile = 0;
            end_tile = 0;
            tile_count = 1;
        }

        // calculate emission space
        Quat emission_rotation = Quat::identity();
        Vector3 emission_position(0.0f, 0.0f, 0.0f);
        if (ddf->m_Space == EMISSION_SPACE_EMITTER)
        {
            emission_rotation = instance->m_Rotation;
            emission_position = Vector3(instance->m_Position);
        }

        const uint32_t vertex_field_count = sizeof(Vertex) / sizeof(float);
        uint32_t max_vertex_count = vertex_buffer_size / sizeof(Vertex);
        uint32_t particle_count = emitter->m_Particles.Size();
        uint32_t j;
        for (j = 0; j < particle_count && vertex_index + 6 <= max_vertex_count; j++)
        {
            Particle* particle = &emitter->m_Particles[j];

            float size = particle->GetSize();

            Vector3 particle_position = rotate(emission_rotation, Vector3(particle->GetPosition())) + emission_position;
            Quat particle_rotation = emission_rotation * particle->GetRotation();

            Vector3 x = rotate(particle_rotation, Vector3(size, 0.0f, 0.0f));
            Vector3 y = rotate(particle_rotation, Vector3(0.0f, size, 0.0f));

            Vector3 p0 = -x - y + particle_position;
            Vector3 p1 = -x + y + particle_position;
            Vector3 p2 = x - y + particle_position;
            Vector3 p3 = x + y + particle_position;

            // avoid wrapping for dead particles
            float time_left = dmMath::Select(particle->GetTimeLeft(), particle->GetTimeLeft(), 0.0f);
            float t = (1.0f - time_left * particle->GetooMaxLifeTime());
            uint32_t tile = (uint32_t)(tile_count * t);
            // TODO only for once
            if (tile == tile_count)
                --tile;
            tile += start_tile;
            float* tex_coord = &tex_coords[tile * 4];
            float u0 = tex_coord[0];
            float v0 = tex_coord[1];
            float u1 = tex_coord[2];
            float v1 = tex_coord[3];

            // store values in the buffer
            uint32_t field_index = vertex_index * vertex_field_count;
            Vector4 c = particle->GetColor();

            vertex_buffer[field_index + 0] = u0;
            vertex_buffer[field_index + 1] = v1;
            vertex_buffer[field_index + 2] = p0.getX();
            vertex_buffer[field_index + 3] = p0.getY();
            vertex_buffer[field_index + 4] = p0.getZ();
            memcpy(&vertex_buffer[field_index + 5], &c, sizeof(Vector4));

            field_index += vertex_field_count;
            vertex_buffer[field_index + 0] = u0;
            vertex_buffer[field_index + 1] = v0;
            vertex_buffer[field_index + 2] = p1.getX();
            vertex_buffer[field_index + 3] = p1.getY();
            vertex_buffer[field_index + 4] = p1.getZ();
            memcpy(&vertex_buffer[field_index + 5], &c, sizeof(Vector4));

            field_index += vertex_field_count;
            vertex_buffer[field_index + 0] = u1;
            vertex_buffer[field_index + 1] = v1;
            vertex_buffer[field_index + 2] = p2.getX();
            vertex_buffer[field_index + 3] = p2.getY();
            vertex_buffer[field_index + 4] = p2.getZ();
            memcpy(&vertex_buffer[field_index + 5], &c, sizeof(Vector4));

            field_index += vertex_field_count;
            vertex_buffer[field_index + 0] = u1;
            vertex_buffer[field_index + 1] = v1;
            vertex_buffer[field_index + 2] = p2.getX();
            vertex_buffer[field_index + 3] = p2.getY();
            vertex_buffer[field_index + 4] = p2.getZ();
            memcpy(&vertex_buffer[field_index + 5], &c, sizeof(Vector4));

            field_index += vertex_field_count;
            vertex_buffer[field_index + 0] = u0;
            vertex_buffer[field_index + 1] = v0;
            vertex_buffer[field_index + 2] = p1.getX();
            vertex_buffer[field_index + 3] = p1.getY();
            vertex_buffer[field_index + 4] = p1.getZ();
            memcpy(&vertex_buffer[field_index + 5], &c, sizeof(Vector4));

            field_index += vertex_field_count;
            vertex_buffer[field_index + 0] = u1;
            vertex_buffer[field_index + 1] = v0;
            vertex_buffer[field_index + 2] = p3.getX();
            vertex_buffer[field_index + 3] = p3.getY();
            vertex_buffer[field_index + 4] = p3.getZ();
            memcpy(&vertex_buffer[field_index + 5], &c, sizeof(Vector4));

            vertex_index += 6;
        }
        if (j < particle_count)
        {
            if (emitter->m_RenderWarning == 0)
            {
                dmLogWarning("Maximum number of particles (%d) exceeded, particles will not be rendered. Change \"%s\" in the config file.", context->m_MaxParticleCount, MAX_PARTICLE_COUNT_KEY);
                emitter->m_RenderWarning = 1;
            }
        }
        emitter->m_VertexCount = vertex_index - emitter->m_VertexIndex;
        return emitter->m_VertexCount;
    }

    struct SortPred
    {
        inline bool operator () (const Particle& p1, const Particle& p2)
        {
            return p1.GetSortKey().m_Key < p2.GetSortKey().m_Key;
        }

    };

    void GenerateKeys(Emitter* emitter)
    {
        dmArray<Particle>& particles = emitter->m_Particles;
        uint32_t n = particles.Size();

        float range = 1.0f / emitter->m_MaxParticleLifeTime;

        Particle* first = particles.Begin();
        for (uint32_t i = 0; i < n; ++i)
        {
            Particle* p = &particles[i];
            uint32_t index = p - first;

            float life_time = (1.0f - p->GetTimeLeft() * range) * 65535;
            life_time = dmMath::Clamp(life_time, 0.0f, 65535.0f);
            uint16_t lt = (uint16_t) life_time;
            SortKey key;
            key.m_LifeTime = lt;
            key.m_Index = index;
            p->SetSortKey(key);
        }
    }

    void SortParticles(Emitter* emitter)
    {
        DM_PROFILE(Particle, "Sort");

        std::sort(emitter->m_Particles.Begin(), emitter->m_Particles.End(), SortPred());
    }

#define SAMPLE_PROP(segment, x, target)\
    {\
        LinearSegment* s = &segment;\
        target = (x - s->m_X) * s->m_K + s->m_Y;\
    }\

    void EvaluateEmitterProperties(Emitter* emitter, Property* emitter_properties, float duration, float properties[EMITTER_KEY_COUNT])
    {
        float x = dmMath::Select(-duration, 0.0f, emitter->m_Timer / duration);
        uint32_t segment_index = dmMath::Min((uint32_t)(x * PROPERTY_SAMPLE_COUNT), PROPERTY_SAMPLE_COUNT - 1);
        for (uint32_t i = 0; i < EMITTER_KEY_COUNT; ++i)
        {
            SAMPLE_PROP(emitter_properties[i].m_Segments[segment_index], x, properties[i])
        }
    }

    void EvaluateParticleProperties(Emitter* emitter, Property* particle_properties)
    {
        float properties[PARTICLE_KEY_COUNT];
        uint32_t count = emitter->m_Particles.Size();
        // TODO Optimize this
        dmArray<Particle>& particles = emitter->m_Particles;
        for (uint32_t i = 0; i < count; ++i)
        {
            Particle* particle = &particles[i];
            float x = dmMath::Select(-particle->GetMaxLifeTime(), 0.0f, 1.0f - particle->GetTimeLeft() * particle->GetooMaxLifeTime());
            uint32_t segment_index = dmMath::Min((uint32_t)(x * PROPERTY_SAMPLE_COUNT), PROPERTY_SAMPLE_COUNT - 1);

            SAMPLE_PROP(particle_properties[PARTICLE_KEY_SCALE].m_Segments[segment_index], x, properties[PARTICLE_KEY_SCALE])
            SAMPLE_PROP(particle_properties[PARTICLE_KEY_RED].m_Segments[segment_index], x, properties[PARTICLE_KEY_RED])
            SAMPLE_PROP(particle_properties[PARTICLE_KEY_GREEN].m_Segments[segment_index], x, properties[PARTICLE_KEY_GREEN])
            SAMPLE_PROP(particle_properties[PARTICLE_KEY_BLUE].m_Segments[segment_index], x, properties[PARTICLE_KEY_BLUE])
            SAMPLE_PROP(particle_properties[PARTICLE_KEY_ALPHA].m_Segments[segment_index], x, properties[PARTICLE_KEY_ALPHA])

            Vector4 c = particle->GetSourceColor();
            particle->SetSize(particle->GetSourceSize() * properties[PARTICLE_KEY_SCALE]);
            particle->SetColor(Vector4(c.getX() * properties[PARTICLE_KEY_RED],
                    c.getY() * properties[PARTICLE_KEY_GREEN],
                    c.getZ() * properties[PARTICLE_KEY_BLUE],
                    c.getW() * properties[PARTICLE_KEY_ALPHA]));
        }
    }

    void ApplyAcceleration(dmArray<Particle>& particles, Property* modifier_properties, Vector3 direction, float dt)
    {
        uint32_t particle_count = particles.Size();
        uint32_t key = MODIFIER_KEY_MAGNITUDE;
        Vector3 unit_acc = direction * dt;
        for (uint32_t i = 0; i < particle_count; ++i)
        {
            Particle* particle = &particles[i];
            float x = dmMath::Select(-particle->GetMaxLifeTime(), 0.0f, 1.0f - particle->GetTimeLeft() * particle->GetooMaxLifeTime());
            uint32_t segment_index = dmMath::Min((uint32_t)(x * PROPERTY_SAMPLE_COUNT), PROPERTY_SAMPLE_COUNT - 1);

            float magnitude;
            SAMPLE_PROP(modifier_properties[key].m_Segments[segment_index], x, magnitude)

            particle->SetVelocity(particle->GetVelocity() + unit_acc * magnitude);
        }
    }

    void ApplyDrag(dmArray<Particle>& particles, Property* modifier_properties, dmParticleDDF::Modifier* modifier_ddf, Quat world_rotation, float dt)
    {
        uint32_t particle_count = particles.Size();
        const Point3& position = modifier_ddf->m_Position;
        const Vector3 direction = rotate(world_rotation, Vector3::xAxis());
        for (uint32_t i = 0; i < particle_count; ++i)
        {
            Particle* particle = &particles[i];
            float x = dmMath::Select(-particle->GetMaxLifeTime(), 0.0f, 1.0f - particle->GetTimeLeft() * particle->GetooMaxLifeTime());
            uint32_t segment_index = dmMath::Min((uint32_t)(x * PROPERTY_SAMPLE_COUNT), PROPERTY_SAMPLE_COUNT - 1);

            float attenuation, magnitude;
            SAMPLE_PROP(modifier_properties[MODIFIER_KEY_ATTENUATION].m_Segments[segment_index], x, attenuation)
            SAMPLE_PROP(modifier_properties[MODIFIER_KEY_MAGNITUDE].m_Segments[segment_index], x, magnitude)

            float denumerator = 1.0f + attenuation * distSqr(particle->GetPosition(), position);
            float c = dmMath::Select(-dmMath::Abs(denumerator), 0.0f, magnitude / denumerator);
            Vector3 v = particle->GetVelocity();
            if (modifier_ddf->m_UseDirection)
                v = projection(Point3(particle->GetVelocity()), direction) * direction;
            particle->SetVelocity(particle->GetVelocity() - c * v * dt);
        }
    }

    void Simulate(Emitter* emitter, EmitterPrototype* prototype, dmParticleDDF::Emitter* ddf, float dt)
    {
        DM_PROFILE(Particle, "Simulate");

        dmArray<Particle>& particles = emitter->m_Particles;
        EvaluateParticleProperties(emitter, prototype->m_ParticleProperties);
        // Apply modifiers
        uint32_t modifier_count = prototype->m_Modifiers.Size();
        for (uint32_t i = 0; i < modifier_count; ++i)
        {
            ModifierPrototype* modifier = &prototype->m_Modifiers[i];
            dmParticleDDF::Modifier* modifier_ddf = &ddf->m_Modifiers[i];
            switch (modifier_ddf->m_Type)
            {
            case dmParticleDDF::MODIFIER_TYPE_ACCELERATION:
                ApplyAcceleration(particles, modifier->m_Properties, rotate(modifier_ddf->m_Rotation, Vector3::xAxis()), dt);
                break;
            case dmParticleDDF::MODIFIER_TYPE_DRAG:
                ApplyDrag(particles, modifier->m_Properties, modifier_ddf, ddf->m_Rotation * modifier_ddf->m_Rotation, dt);
                break;
            }
        }
        uint32_t particle_count = particles.Size();
        for (uint32_t i = 0; i < particle_count; ++i)
        {
            Particle* p = &particles[i];
            // NOTE This velocity integration has a larger error than normal since we don't use the velocity at the
            // beginning of the frame, but it's ok since particle movement does not need to be very exact
            p->SetPosition(p->GetPosition() + p->GetVelocity() * dt);
        }
    }

    void Render(HContext context, void* usercontext, RenderInstanceCallback render_emitter_callback)
    {
        DM_PROFILE(Particle, "Render");

        if (context->m_Instances.Size() == 0)
            return;

        if (render_emitter_callback != 0x0)
        {
            uint32_t instance_count = context->m_Instances.Size();
            for (uint32_t i = 0; i < instance_count; i++)
            {
                Instance* instance = context->m_Instances[i];
                if (instance == INVALID_INSTANCE) continue;
                uint32_t emitter_count = instance->m_Emitters.Size();
                for (uint32_t j = 0; j < emitter_count; ++j)
                {
                    Emitter* emitter = &instance->m_Emitters[j];
                    if (!emitter || emitter->m_VertexCount == 0) continue;

                dmParticle::EmitterPrototype* emitter_proto = &instance->m_Prototype->m_Emitters[j];
                render_emitter_callback(usercontext, emitter_proto->m_Material, emitter->m_AnimationData.m_Texture, emitter_proto->m_BlendMode, emitter->m_VertexIndex, emitter->m_VertexCount);
                }
            }
        }
    }

    void DebugRender(HContext context, void* user_context, RenderLineCallback render_line_callback)
    {
        uint32_t instance_count = context->m_Instances.Size();
        for (uint32_t i = 0; i < instance_count; ++i)
        {
            Instance* instance = context->m_Instances[i];
            if (instance == INVALID_INSTANCE) continue;
            Prototype* prototype = instance->m_Prototype;

            uint32_t emitter_count = instance->m_Emitters.Size();
            for (uint32_t j = 0; j < emitter_count; j++)
            {
                Emitter* e = &instance->m_Emitters[j];

                dmParticleDDF::Emitter* ddf = &prototype->m_DDF->m_Emitters[j];
                Vectormath::Aos::Vector4 color(0.0f, 1.0f, 0.0f, 1.0f);
                if (IsSleeping(e, ddf))
                {
                    color.setY(0.0f);
                    color.setZ(1.0f);
                }
                else if (ddf->m_Mode == dmParticle::PLAY_MODE_ONCE)
                {
                    float t = dmMath::Select(-ddf->m_Duration, 0.0f, e->m_Timer / ddf->m_Duration);
                    color.setY(1.0f - t);
                    color.setZ(t);
                }

                Point3 position = instance->m_Position + rotate(instance->m_Rotation, Vector3(ddf->m_Position));
                Quat rotation = instance->m_Rotation * ddf->m_Rotation;
                switch (ddf->m_Type)
                {
                case EMITTER_TYPE_SPHERE:
                {
                    const float radius = ddf->m_Properties[EMITTER_KEY_SIZE_X].m_Points[0].m_Y;

                    const uint32_t segment_count = 16;
                    Vector3 vertices[segment_count + 1][3];
                    for (uint32_t j = 0; j < segment_count + 1; ++j)
                    {
                        float angle = 2.0f * ((float) M_PI) * j / segment_count;
                        vertices[j][0] = Vector3(radius * cos(angle), radius * sin(angle), 0.0f);
                        vertices[j][1] = Vector3(0.0f, radius * cos(angle), radius * sin(angle));
                        vertices[j][2] = Vector3(radius * cos(angle), 0.0f, radius * sin(angle));
                    }
                    for (uint32_t j = 1; j < segment_count + 1; ++j)
                    {
                        for (uint32_t k = 0; k < 3; ++k)
                            render_line_callback(user_context, position + rotate(rotation, vertices[j-1][k]), position + rotate(rotation, vertices[j][k]), color);
                    }
                    break;
                }
                case EMITTER_TYPE_CONE:
                {
                    const float radius = ddf->m_Properties[EMITTER_KEY_SIZE_X].m_Points[0].m_Y;
                    const float height = ddf->m_Properties[EMITTER_KEY_SIZE_Y].m_Points[0].m_Y;

                    // 4 pillars
                    render_line_callback(user_context, position, position + rotate(rotation, Vector3(radius, 0.0f, height)), color);
                    render_line_callback(user_context, position, position + rotate(rotation, Vector3(-radius, 0.0f, height)), color);
                    render_line_callback(user_context, position, position + rotate(rotation, Vector3(0.0f, radius, height)), color);
                    render_line_callback(user_context, position, position + rotate(rotation, Vector3(0.0f, -radius, height)), color);
                    // circle
                    const uint32_t segment_count = 16;
                    Vector3 vertices[segment_count];
                    for (uint32_t j = 0; j < segment_count; ++j)
                    {
                        float angle = 2.0f * ((float) M_PI) * j / segment_count;
                        vertices[j] = Vector3(radius * cos(angle), radius * sin(angle), height);
                    }
                    for (uint32_t j = 1; j < segment_count; ++j)
                    {
                        render_line_callback(user_context, position + rotate(rotation, vertices[j-1]), position + rotate(rotation, vertices[j]), color);
                    }
                    render_line_callback(user_context, position + rotate(rotation, vertices[segment_count - 1]), position + rotate(rotation, vertices[0]), color);
                    break;
                }
                case EMITTER_TYPE_BOX:
                {
                    const float x_ext = 0.5f * ddf->m_Properties[EMITTER_KEY_SIZE_X].m_Points[0].m_Y;
                    const float y_ext = 0.5f * ddf->m_Properties[EMITTER_KEY_SIZE_Y].m_Points[0].m_Y;
                    const float z_ext = 0.5f * ddf->m_Properties[EMITTER_KEY_SIZE_Z].m_Points[0].m_Y;

                    render_line_callback(user_context, position + rotate(rotation, Vector3(-x_ext, -y_ext, -z_ext)), position + rotate(rotation, Vector3(x_ext, -y_ext, -z_ext)), color);
                    render_line_callback(user_context, position + rotate(rotation, Vector3(x_ext, -y_ext, -z_ext)), position + rotate(rotation, Vector3(x_ext, y_ext, -z_ext)), color);
                    render_line_callback(user_context, position + rotate(rotation, Vector3(x_ext, y_ext, -z_ext)), position + rotate(rotation, Vector3(-x_ext, y_ext, -z_ext)), color);
                    render_line_callback(user_context, position + rotate(rotation, Vector3(-x_ext, y_ext, -z_ext)), position + rotate(rotation, Vector3(-x_ext, -y_ext, -z_ext)), color);

                    render_line_callback(user_context, position + rotate(rotation, Vector3(-x_ext, -y_ext, z_ext)), position + rotate(rotation, Vector3(x_ext, -y_ext, z_ext)), color);
                    render_line_callback(user_context, position + rotate(rotation, Vector3(x_ext, -y_ext, z_ext)), position + rotate(rotation, Vector3(x_ext, y_ext, z_ext)), color);
                    render_line_callback(user_context, position + rotate(rotation, Vector3(x_ext, y_ext, z_ext)), position + rotate(rotation, Vector3(-x_ext, y_ext, z_ext)), color);
                    render_line_callback(user_context, position + rotate(rotation, Vector3(-x_ext, y_ext, z_ext)), position + rotate(rotation, Vector3(-x_ext, -y_ext, z_ext)), color);

                    render_line_callback(user_context, position + rotate(rotation, Vector3(-x_ext, -y_ext, -z_ext)), position + rotate(rotation, Vector3(-x_ext, -y_ext, z_ext)), color);
                    render_line_callback(user_context, position + rotate(rotation, Vector3(x_ext, -y_ext, -z_ext)), position + rotate(rotation, Vector3(x_ext, -y_ext, z_ext)), color);
                    render_line_callback(user_context, position + rotate(rotation, Vector3(x_ext, y_ext, -z_ext)), position + rotate(rotation, Vector3(x_ext, y_ext, z_ext)), color);
                    render_line_callback(user_context, position + rotate(rotation, Vector3(-x_ext, y_ext, -z_ext)), position + rotate(rotation, Vector3(-x_ext, y_ext, z_ext)), color);

                    break;
                }

                    default:
                        break;
                }
            }
        }
    }

    bool LoadResources(Prototype* prototype, const void* buffer, uint32_t buffer_size)
    {
        dmParticleDDF::ParticleFX* ddf = 0;
        dmDDF::Result r = dmDDF::LoadMessage<dmParticleDDF::ParticleFX>(buffer, buffer_size, &ddf);
        if (r == dmDDF::RESULT_OK)
        {
            uint32_t emitter_count = ddf->m_Emitters.m_Count;
            if (prototype->m_DDF != 0x0)
            {
                dmDDF::FreeMessage(prototype->m_DDF);
            }
            prototype->m_DDF = ddf;
            prototype->m_Emitters.SetCapacity(emitter_count);
            prototype->m_Emitters.SetSize(emitter_count);

            memset(prototype->m_Emitters.Begin(), 0, emitter_count * sizeof(EmitterPrototype));
            for (uint32_t i = 0; i < emitter_count; ++i)
            {
                dmParticleDDF::Emitter* emitter_ddf = &ddf->m_Emitters[i];
                EmitterPrototype* emitter = &prototype->m_Emitters[i];
                emitter->m_Animation = dmHashString64(emitter_ddf->m_Animation);
                emitter->m_BlendMode = emitter_ddf->m_BlendMode;
                // Approximate splines with linear segments
                memset(emitter->m_Properties, 0, sizeof(emitter->m_Properties));
                memset(emitter->m_ParticleProperties, 0, sizeof(emitter->m_ParticleProperties));
                uint32_t prop_count = emitter_ddf->m_Properties.m_Count;
                for (uint32_t i = 0; i < prop_count; ++i)
                {
                    const dmParticleDDF::Emitter::Property& p = emitter_ddf->m_Properties[i];
                    if (p.m_Key < dmParticleDDF::EMITTER_KEY_COUNT)
                    {
                        SampleProperty(p.m_Points.m_Data, p.m_Points.m_Count, emitter->m_Properties[p.m_Key].m_Segments);
                    }
                    else
                    {
                        dmLogWarning("The key %d is not a valid emitter key.", p.m_Key);
                    }
                }
                prop_count = emitter_ddf->m_ParticleProperties.m_Count;
                for (uint32_t i = 0; i < prop_count; ++i)
                {
                    const dmParticleDDF::Emitter::ParticleProperty& p = emitter_ddf->m_ParticleProperties[i];
                    if (p.m_Key < dmParticleDDF::PARTICLE_KEY_COUNT)
                    {
                        SampleProperty(p.m_Points.m_Data, p.m_Points.m_Count, emitter->m_ParticleProperties[p.m_Key].m_Segments);
                    }
                    else
                    {
                        dmLogWarning("The key %d is not a valid particle key.", p.m_Key);
                    }
                }
                uint32_t modifier_count = emitter_ddf->m_Modifiers.m_Count;
                emitter->m_Modifiers.SetCapacity(modifier_count);
                emitter->m_Modifiers.SetSize(modifier_count);
                memset(emitter->m_Modifiers.Begin(), 0, modifier_count * sizeof(ModifierPrototype));
                for (uint32_t i = 0; i < modifier_count; ++i)
                {
                    ModifierPrototype& modifier = emitter->m_Modifiers[i];
                    const dmParticleDDF::Modifier& modifier_ddf = emitter_ddf->m_Modifiers[i];
                    prop_count = modifier_ddf.m_Properties.m_Count;
                    for (uint32_t j = 0; j < prop_count; ++j)
                    {
                        const dmParticleDDF::Modifier::Property& p = modifier_ddf.m_Properties[i];
                        if (p.m_Key < dmParticleDDF::MODIFIER_KEY_COUNT)
                        {
                            SampleProperty(p.m_Points.m_Data, p.m_Points.m_Count, modifier.m_Properties[p.m_Key].m_Segments);
                        }
                        else
                        {
                            dmLogWarning("The key %d is not a valid modifier key.", p.m_Key);
                        }
                    }
                }
            }
            return true;
        }
        return false;
    }

    Prototype* NewPrototype(const void* buffer, uint32_t buffer_size)
    {
        Prototype* prototype = new Prototype();
        if (LoadResources(prototype, buffer, buffer_size))
        {
            return prototype;
        }
        else
        {
            delete prototype;
            dmLogError("Failed to load particle data");
            return 0x0;
        }
    }

    void DeletePrototype(HPrototype prototype)
    {
        uint32_t emitter_count = prototype->m_Emitters.Size();
        for (uint32_t i = 0; i < emitter_count; ++i)
        {
            prototype->m_Emitters[i].m_Modifiers.SetCapacity(0);
        }
        dmDDF::FreeMessage(prototype->m_DDF);
        delete prototype;
    }

    bool ReloadPrototype(HPrototype prototype, const void* buffer, uint32_t buffer_size)
    {
        return LoadResources(prototype, buffer, buffer_size);
    }

    uint32_t GetEmitterCount(HPrototype prototype)
    {
        return prototype->m_Emitters.Size();
    }

    const char* GetMaterialPath(HPrototype prototype, uint32_t emitter_index)
    {
        return prototype->m_DDF->m_Emitters[emitter_index].m_Material;
    }

    const char* GetTileSourcePath(HPrototype prototype, uint32_t emitter_index)
    {
        return prototype->m_DDF->m_Emitters[emitter_index].m_TileSource;
    }

    void* GetMaterial(HPrototype prototype, uint32_t emitter_index)
    {
        return prototype->m_Emitters[emitter_index].m_Material;
    }

    void* GetTileSource(HPrototype prototype, uint32_t emitter_index)
    {
        return prototype->m_Emitters[emitter_index].m_TileSource;
    }

    void SetMaterial(HPrototype prototype, uint32_t emitter_index, void* material)
    {
        prototype->m_Emitters[emitter_index].m_Material = material;
    }

    void SetTileSource(HPrototype prototype, uint32_t emitter_index, void* tile_source)
    {
        prototype->m_Emitters[emitter_index].m_TileSource = tile_source;
    }

#define DM_PARTICLE_TRAMPOLINE1(ret, name, t1) \
    ret Particle_##name(t1 a1)\
    {\
        return name(a1);\
    }\

#define DM_PARTICLE_TRAMPOLINE2(ret, name, t1, t2) \
    ret Particle_##name(t1 a1, t2 a2)\
    {\
        return name(a1, a2);\
    }\

#define DM_PARTICLE_TRAMPOLINE3(ret, name, t1, t2, t3) \
    ret Particle_##name(t1 a1, t2 a2, t3 a3)\
    {\
        return name(a1, a2, a3);\
    }\

#define DM_PARTICLE_TRAMPOLINE4(ret, name, t1, t2, t3, t4) \
    ret Particle_##name(t1 a1, t2 a2, t3 a3, t4 a4)\
    {\
        return name(a1, a2, a3, a4);\
    }\

#define DM_PARTICLE_TRAMPOLINE5(ret, name, t1, t2, t3, t4, t5) \
    ret Particle_##name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5)\
    {\
        return name(a1, a2, a3, a4, a5);\
    }\

#define DM_PARTICLE_TRAMPOLINE6(ret, name, t1, t2, t3, t4, t5, t6) \
    ret Particle_##name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6)\
    {\
        return name(a1, a2, a3, a4, a5, a6);\
    }\

    DM_PARTICLE_TRAMPOLINE2(HContext, CreateContext, uint32_t, uint32_t);
    DM_PARTICLE_TRAMPOLINE1(void, DestroyContext, HContext);

    DM_PARTICLE_TRAMPOLINE2(HInstance, CreateInstance, HContext, HPrototype);
    DM_PARTICLE_TRAMPOLINE2(void, DestroyInstance, HContext, HInstance);
    DM_PARTICLE_TRAMPOLINE2(void, ReloadInstance, HContext, HInstance);

    DM_PARTICLE_TRAMPOLINE2(void, StartInstance, HContext, HInstance);
    DM_PARTICLE_TRAMPOLINE2(void, StopInstance, HContext, HInstance);
    DM_PARTICLE_TRAMPOLINE2(void, RestartInstance, HContext, HInstance);
    DM_PARTICLE_TRAMPOLINE2(void, ResetInstance, HContext, HInstance);
    DM_PARTICLE_TRAMPOLINE3(void, SetPosition, HContext, HInstance, Point3);
    DM_PARTICLE_TRAMPOLINE3(void, SetRotation, HContext, HInstance, Quat);

    DM_PARTICLE_TRAMPOLINE2(bool, IsSpawning, HContext, HInstance);
    DM_PARTICLE_TRAMPOLINE2(bool, IsSleeping, HContext, HInstance);
    DM_PARTICLE_TRAMPOLINE6(void, Update, HContext, float, float*, uint32_t, uint32_t*, FetchAnimationCallback);
    DM_PARTICLE_TRAMPOLINE3(void, Render, HContext, void*, RenderInstanceCallback);

    DM_PARTICLE_TRAMPOLINE2(HPrototype, NewPrototype, const void*, uint32_t);
    DM_PARTICLE_TRAMPOLINE1(void, DeletePrototype, HPrototype);
    DM_PARTICLE_TRAMPOLINE3(bool, ReloadPrototype, HPrototype, const void*, uint32_t);

    DM_PARTICLE_TRAMPOLINE1(uint32_t, GetEmitterCount, HPrototype);
    DM_PARTICLE_TRAMPOLINE2(const char*, GetMaterialPath, HPrototype, uint32_t);
    DM_PARTICLE_TRAMPOLINE2(const char*, GetTileSourcePath, HPrototype, uint32_t);
    DM_PARTICLE_TRAMPOLINE2(void*, GetMaterial, HPrototype, uint32_t);
    DM_PARTICLE_TRAMPOLINE2(void*, GetTileSource, HPrototype, uint32_t);
    DM_PARTICLE_TRAMPOLINE3(void, SetMaterial, HPrototype, uint32_t, void*);
    DM_PARTICLE_TRAMPOLINE3(void, SetTileSource, HPrototype, uint32_t, void*);

    dmhash_t Particle_Hash(const char* value)
    {
        return dmHashString64(value);
    }
}
