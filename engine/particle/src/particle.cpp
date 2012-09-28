#include <string.h>
#include <stdint.h>
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
        float m_Alpha;
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
            if (context->m_Instances[i] != 0x0 && context->m_Instances[i]->m_Dangling == 0)
                ++lingering;
            delete context->m_Instances[i];
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
        if (emitter_count > 0)
        {
            memset(&instance->m_Emitters.Front(), 0, emitter_count * sizeof(Emitter));
            for (uint32_t i = 0; i < emitter_count; ++i)
            {
                Emitter* emitter = &instance->m_Emitters[i];
                EmitterPrototype* emitter_prototype = &prototype->m_Emitters[i];
                dmParticleDDF::Emitter* emitter_ddf = &ddf->m_Emitters[i];
                emitter->m_Prototype = emitter_prototype;
                uint32_t particle_count = emitter_ddf->m_MaxParticleCount;
                emitter->m_Particles.SetCapacity(particle_count);
                emitter->m_Particles.SetSize(particle_count);
                if (particle_count > 0)
                {
                    memset(&emitter->m_Particles.Front(), 0, particle_count * sizeof(Particle));
                }
                emitter->m_Timer = 0.0f;
            }
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

    void StartInstance(HContext context, HInstance instance)
    {
        if (instance == INVALID_INSTANCE) return;
        Instance* i = GetInstance(context, instance);
        if (!i) return;
        uint32_t emitter_count = i->m_Emitters.Size();
        for (uint32_t emitter_i = 0; emitter_i < emitter_count; ++emitter_i)
        {
            // TODO: Fix auto-start
            i->m_Emitters[emitter_i].m_IsSpawning = 1;
        }
    }

    void StopInstance(HContext context, HInstance instance)
    {
        if (instance == INVALID_INSTANCE) return;
        Instance* i = GetInstance(context, instance);
        if (!i) return;
        uint32_t emitter_count = i->m_Emitters.Size();
        for (uint32_t emitter_i = 0; emitter_i < emitter_count; ++emitter_i)
        {
            i->m_Emitters[emitter_i].m_IsSpawning = 0;
        }
    }

    void RestartInstance(HContext context, HInstance instance)
    {
        if (instance == INVALID_INSTANCE) return;
        Instance* i = GetInstance(context, instance);
        if (!i) return;
        uint32_t emitter_count = i->m_Emitters.Size();
        for (uint32_t emitter_i = 0; emitter_i < emitter_count; ++emitter_i)
        {
            Emitter* emitter = &i->m_Emitters[emitter_i];
            // TODO: Fix auto-start
            emitter->m_IsSpawning = 1;
            emitter->m_Timer = 0.0f;
            emitter->m_SpawnTimer = 0.0f;
            emitter->m_SpawnDelay = 0.0f;
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
            // A looping emitter would never be removed, destroy it and report error
            uint32_t emitter_count = prototype->m_Emitters.Size();
            for (uint32_t emitter_i = 0; emitter_i < emitter_count; ++emitter_i)
            {
                Emitter* emitter = &instance->m_Emitters[emitter_i];
                if (prototype->m_DDF->m_Emitters[emitter_i].m_Mode == PLAY_MODE_LOOP)
                {
                    dmLogError("An emitter is set to loop and can't be created in this manner, it would never be destroyed.");
                    DestroyInstance(context, i);
                    return;
                }
                emitter->m_IsSpawning = 1;
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
        uint32_t emitter_count = i->m_Emitters.Size();
        for (uint32_t emitter_i = 0; emitter_i < emitter_count; ++emitter_i)
        {
            if (IsSpawning(&i->m_Emitters[emitter_i]))
            {
                is_spawning = true;
                break;
            }
        }
        return is_spawning;
    }

    bool IsSleeping(Emitter* emitter)
    {
        if (emitter->m_Prototype->m_DDF->m_Mode == PLAY_MODE_LOOP)
            return false;
        if (emitter->m_IsSpawning || emitter->m_ParticleTimeLeft > 0.0f)
            return false;
        return true;
    }

    bool IsSleeping(Instance* instance)
    {
        // Consider 0x0 instances as sleeping
        if (!instance) return true;
        bool is_sleeping = true;
        uint32_t emitter_count = instance->m_Emitters.Size();
        for (uint32_t emitter_i = 0; emitter_i < emitter_count; ++emitter_i)
        {
            if (!IsSleeping(&instance->m_Emitters[emitter_i]))
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
    static void SpawnParticle(HContext context, Particle* particle, Instance* instance, Emitter* emitter);
    static uint32_t UpdateRenderData(HContext context, Instance* instance, Emitter* emitter, uint32_t vertex_index, float* vertex_buffer, uint32_t vertex_buffer_size);

    static void EvaluateParticleProperties(Emitter* emitter)
    {
        float properties[PARTICLE_KEY_COUNT];
        uint32_t count = emitter->m_Particles.Size();
        EmitterPrototype* prototype = emitter->m_Prototype;
        // TODO Optimize this
        for (uint32_t i = 0; i < count; ++i)
        {
            Particle* particle = &emitter->m_Particles[i];
            float x = dmMath::Select(-particle->m_MaxLifeTime, 0.0f, 1.0f - particle->m_TimeLeft * particle->m_ooMaxLifeTime);
            for (uint32_t i = 0; i < PARTICLE_KEY_COUNT; ++i)
            {
                Property* property = &prototype->m_ParticleProperties[i];
                uint32_t segment_index = (uint32_t)(x * PROPERTY_SAMPLE_COUNT);
                LinearSegment* segment = &property->m_Segments[segment_index];
                float y = (x - segment->m_X) * segment->m_K + segment->m_Y;
                properties[i] = y;
            }
            particle->m_Size = particle->m_SourceSize * properties[PARTICLE_KEY_SCALE];
            particle->m_Alpha = particle->m_SourceAlpha * properties[PARTICLE_KEY_ALPHA];
        }
    }

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
            // don't update sleeping emitters
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

            uint32_t emitter_count = instance->m_Emitters.Size();
            for (uint32_t emitter_i = 0; emitter_i < emitter_count; ++emitter_i)
            {
                Emitter* emitter = &instance->m_Emitters[emitter_i];
                EmitterPrototype* prototype = emitter->m_Prototype;

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
                        emitter->m_FetchAnimWarning = 0;
                    }
                }

                // Resize particle buffer if the resource has been reloaded, wipe particles
                uint32_t max_particle_count = prototype->m_DDF->m_MaxParticleCount;
                if (emitter->m_Particles.Size() != max_particle_count)
                {
                    if (max_particle_count <= context->m_MaxParticleCount)
                    {
                        emitter->m_Particles.SetCapacity(max_particle_count);
                        emitter->m_Particles.SetSize(max_particle_count);
                        if (max_particle_count > 0)
                        {
                            memset(&emitter->m_Particles.Front(), 0, max_particle_count * sizeof(Particle));
                        }
                        emitter->m_ResizeWarning = 0;
                        emitter->m_RenderWarning = 0;
                    }
                    else
                    {
                        if (emitter->m_ResizeWarning == 0)
                        {
                            dmLogWarning("Maximum number of particles (%d) exceeded, emitter will not be resized. Change the max particle count for the emitter or \"%s\" in the config file.", context->m_MaxParticleCount, MAX_PARTICLE_COUNT_KEY);
                            emitter->m_ResizeWarning = 1;
                        }
                    }
                }

                const uint32_t particle_count = emitter->m_Particles.Size();

                // Spawn
                if (emitter->m_IsSpawning)
                {
                    DM_PROFILE(Particle, "Spawn");
                    for (uint32_t j = 0; j < particle_count && emitter->m_SpawnTimer >= emitter->m_SpawnDelay; j++)
                    {
                        Particle* p = &emitter->m_Particles[j];
                        if (p->m_TimeLeft <= 0.0f)
                        {
                            emitter->m_SpawnTimer -= emitter->m_SpawnDelay;
                            SpawnParticle(context, p, instance, emitter);
                        }
                    }
                }

                // Simulate
                {
                    DM_PROFILE(Particle, "Simulate");
                    EvaluateParticleProperties(emitter);
                    // TODO Apply modifiers
                    for (uint32_t j = 0; j < particle_count; j++)
                    {
                        Particle* p = &emitter->m_Particles[j];
                        const Vector3& v = p->m_Velocity;
                        // NOTE This velocity integration has a larger error than normal since we don't use the velocity at the
                        // beginning of the frame, but it's ok since particle movement does not need to be very exact
                        p->m_Position += v * dt;
                        p->m_TimeLeft -= dt;
                        // TODO Handle death-action
                    }
                }

                // Render data
                if (vertex_buffer != 0x0)
                    vertex_index += UpdateRenderData(context, instance, emitter, vertex_index, vertex_buffer, vertex_buffer_size);

                // Update emitter
                if (emitter->m_IsSpawning)
                {
                    emitter->m_Timer += dt;
                    // never let the spawn timer accumulate, it would result in a burst of spawned particles
                    if (emitter->m_SpawnTimer < emitter->m_SpawnDelay)
                        emitter->m_SpawnTimer += dt;
                    // stop once-emitters that have lived their life
                    if (prototype->m_DDF->m_Mode == PLAY_MODE_ONCE && emitter->m_Timer >= prototype->m_DDF->m_Duration)
                        emitter->m_IsSpawning = 0;
                }
                if (emitter->m_ParticleTimeLeft > 0.0f)
                    emitter->m_ParticleTimeLeft -= dt;
            }
        }
        if (out_vertex_buffer_size != 0x0)
        {
            *out_vertex_buffer_size = vertex_index * sizeof(Vertex);
        }
    }

    static void EvaluateEmitterProperties(Emitter* emitter, float properties[EMITTER_KEY_COUNT])
    {
        EmitterPrototype* prototype = emitter->m_Prototype;
        dmParticleDDF::Emitter* ddf = prototype->m_DDF;
        float x = dmMath::Select(-ddf->m_Duration, 0.0f, emitter->m_Timer / ddf->m_Duration);
        for (uint32_t i = 0; i < EMITTER_KEY_COUNT; ++i)
        {
            Property* property = &prototype->m_Properties[i];
            uint32_t segment_index = (uint32_t)(x * PROPERTY_SAMPLE_COUNT);
            LinearSegment* segment = &property->m_Segments[segment_index];
            float y = (x - segment->m_X) * segment->m_K + segment->m_Y;
            properties[i] = y;
        }
    }

    static void SpawnParticle(HContext context, Particle* particle, Instance* instance, Emitter* emitter)
    {
        // TODO Handle birth-action

        dmParticleDDF::Emitter* ddf = emitter->m_Prototype->m_DDF;
        float emitter_properties[EMITTER_KEY_COUNT];
        memset(emitter_properties, 0, sizeof(emitter_properties));
        EvaluateEmitterProperties(emitter, emitter_properties);

        emitter->m_SpawnDelay = emitter_properties[EMITTER_KEY_SPAWN_DELAY];
        particle->m_MaxLifeTime = emitter_properties[EMITTER_KEY_PARTICLE_LIFE_TIME];
        particle->m_ooMaxLifeTime = 1.0f / particle->m_MaxLifeTime;
        particle->m_TimeLeft = particle->m_MaxLifeTime;
        particle->m_SourceSize = emitter_properties[EMITTER_KEY_PARTICLE_SIZE];
        particle->m_SourceAlpha = emitter_properties[EMITTER_KEY_PARTICLE_ALPHA];

        if (emitter->m_ParticleTimeLeft < particle->m_TimeLeft)
            emitter->m_ParticleTimeLeft = particle->m_TimeLeft;

        Vector3 local_position;

        switch (ddf->m_Type)
        {
            case EMITTER_TYPE_SPHERE:
            {
                Vectormath::Aos::Vector3 p = Vectormath::Aos::Vector3::zAxis();
                Vectormath::Aos::Vector3 v(dmMath::Rand11(), dmMath::Rand11(), dmMath::Rand11());
                if (lengthSqr(v) > 0.0f)
                    p = normalize(v);

                local_position = p * emitter_properties[EMITTER_KEY_SPHERE_RADIUS];

                break;
            }

            case EMITTER_TYPE_CONE:
            {
                float height = emitter_properties[EMITTER_KEY_CONE_HEIGHT];
                float radius = emitter_properties[EMITTER_KEY_CONE_RADIUS];
                float angle = 2.0f * ((float) M_PI) * dmMath::RandOpen01();

                local_position = Vector3(cosf(angle) * radius, sinf(angle) * radius, height);

                break;
            }

            case EMITTER_TYPE_BOX:
            {
                float width = emitter_properties[EMITTER_KEY_BOX_WIDTH];
                float height = emitter_properties[EMITTER_KEY_BOX_HEIGHT];
                float depth = emitter_properties[EMITTER_KEY_BOX_DEPTH];
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

        particle->m_Position = ddf->m_Position + rotate(ddf->m_Rotation, local_position);
        particle->m_Rotation = ddf->m_Rotation;
        particle->m_Velocity = rotate(ddf->m_Rotation, velocity);

        if (emitter->m_Prototype->m_DDF->m_Space == EMISSION_SPACE_WORLD)
        {
            particle->m_Position = instance->m_Position + rotate(instance->m_Rotation, Vector3(particle->m_Position));
            particle->m_Rotation = instance->m_Rotation * particle->m_Rotation;
            particle->m_Velocity = rotate(instance->m_Rotation, particle->m_Velocity);
        }
    }

    static float unit_tex_coords[] =
    {
            0.0f, 0.0f, 1.0f, 1.0f
    };

    static uint32_t UpdateRenderData(HContext context, Instance* instance, Emitter* emitter, uint32_t vertex_index, float* vertex_buffer, uint32_t vertex_buffer_size)
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
        if (emitter->m_Prototype->m_DDF->m_Space == EMISSION_SPACE_EMITTER)
        {
            emission_rotation = instance->m_Rotation;
            emission_position = Vector3(instance->m_Position);
        }

        uint32_t max_vertex_count = vertex_buffer_size / sizeof(Vertex);
        uint32_t particle_count = emitter->m_Particles.Size();
        uint32_t j;
        for (j = 0; j < particle_count && vertex_index + 6 <= max_vertex_count; j++)
        {
            Particle* particle = &emitter->m_Particles[j];

            float size = particle->m_Size;
            float alpha = particle->m_Alpha;

            bool alive = (bool)dmMath::Select(particle->m_TimeLeft, 1.0f, 0.0f);
            if (!alive)
            {
                continue;
            }

            Vector3 particle_position = rotate(emission_rotation, Vector3(particle->m_Position)) + emission_position;
            Quat particle_rotation = emission_rotation * particle->m_Rotation;

            Vector3 x = rotate(particle_rotation, Vector3(size, 0.0f, 0.0f));
            Vector3 y = rotate(particle_rotation, Vector3(0.0f, size, 0.0f));

            Vector3 p0 = -x - y + particle_position;
            Vector3 p1 = -x + y + particle_position;
            Vector3 p2 = x - y + particle_position;
            Vector3 p3 = x + y + particle_position;

            // avoid wrapping for dead particles
            float time_left = dmMath::Select(particle->m_TimeLeft, particle->m_TimeLeft, 0.0f);
            float t = (1.0f - time_left * particle->m_ooMaxLifeTime);
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
            uint32_t field_index = vertex_index * VERTEX_FIELD_COUNT;

            vertex_buffer[field_index + 0] = p0.getX();
            vertex_buffer[field_index + 1] = p0.getY();
            vertex_buffer[field_index + 2] = p0.getZ();
            vertex_buffer[field_index + 3] = u0;
            vertex_buffer[field_index + 4] = v1;
            vertex_buffer[field_index + 5] = alpha;

            field_index += VERTEX_FIELD_COUNT;
            vertex_buffer[field_index + 0] = p1.getX();
            vertex_buffer[field_index + 1] = p1.getY();
            vertex_buffer[field_index + 2] = p1.getZ();
            vertex_buffer[field_index + 3] = u0;
            vertex_buffer[field_index + 4] = v0;
            vertex_buffer[field_index + 5] = alpha;

            field_index += VERTEX_FIELD_COUNT;
            vertex_buffer[field_index + 0] = p2.getX();
            vertex_buffer[field_index + 1] = p2.getY();
            vertex_buffer[field_index + 2] = p2.getZ();
            vertex_buffer[field_index + 3] = u1;
            vertex_buffer[field_index + 4] = v1;
            vertex_buffer[field_index + 5] = alpha;

            field_index += VERTEX_FIELD_COUNT;
            vertex_buffer[field_index + 0] = p2.getX();
            vertex_buffer[field_index + 1] = p2.getY();
            vertex_buffer[field_index + 2] = p2.getZ();
            vertex_buffer[field_index + 3] = u1;
            vertex_buffer[field_index + 4] = v1;
            vertex_buffer[field_index + 5] = alpha;

            field_index += VERTEX_FIELD_COUNT;
            vertex_buffer[field_index + 0] = p1.getX();
            vertex_buffer[field_index + 1] = p1.getY();
            vertex_buffer[field_index + 2] = p1.getZ();
            vertex_buffer[field_index + 3] = u0;
            vertex_buffer[field_index + 4] = v0;
            vertex_buffer[field_index + 5] = alpha;

            field_index += VERTEX_FIELD_COUNT;
            vertex_buffer[field_index + 0] = p3.getX();
            vertex_buffer[field_index + 1] = p3.getY();
            vertex_buffer[field_index + 2] = p3.getZ();
            vertex_buffer[field_index + 3] = u1;
            vertex_buffer[field_index + 4] = v0;
            vertex_buffer[field_index + 5] = alpha;

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

                    render_emitter_callback(usercontext, emitter->m_Prototype->m_Material, emitter->m_AnimationData.m_Texture, emitter->m_VertexIndex, emitter->m_VertexCount);
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

            uint32_t emitter_count = instance->m_Emitters.Size();
            for (uint32_t j = 0; j < emitter_count; j++)
            {
                Emitter* e = &instance->m_Emitters[j];

                dmParticleDDF::Emitter* ddf = e->m_Prototype->m_DDF;
                Vectormath::Aos::Vector4 color(0.0f, 1.0f, 0.0f, 1.0f);
                if (IsSleeping(e))
                {
                    color.setY(0.0f);
                    color.setZ(1.0f);
                }
                else if (ddf->m_Mode == dmParticle::PLAY_MODE_ONCE)
                {
                    float t = e->m_Timer / ddf->m_Duration;
                    color.setY(1.0f - t);
                    color.setZ(t);
                }

                Point3 position = instance->m_Position + rotate(instance->m_Rotation, Vector3(ddf->m_Position));
                Quat rotation = instance->m_Rotation * ddf->m_Rotation;
                switch (ddf->m_Type)
                {
                case EMITTER_TYPE_SPHERE:
                {
                    const float radius = ddf->m_Properties[EMITTER_KEY_SPHERE_RADIUS].m_Points[0].m_Y;

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
                    const float radius = ddf->m_Properties[EMITTER_KEY_CONE_RADIUS].m_Points[0].m_Y;
                    const float height = ddf->m_Properties[EMITTER_KEY_CONE_HEIGHT].m_Points[0].m_Y;

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
                    const float x_ext = 0.5f * ddf->m_Properties[EMITTER_KEY_BOX_WIDTH].m_Points[0].m_Y;
                    const float y_ext = 0.5f * ddf->m_Properties[EMITTER_KEY_BOX_HEIGHT].m_Points[0].m_Y;
                    const float z_ext = 0.5f * ddf->m_Properties[EMITTER_KEY_BOX_DEPTH].m_Points[0].m_Y;

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
            if (emitter_count > 0)
            {
                memset(&prototype->m_Emitters.Front(), 0, emitter_count * sizeof(EmitterPrototype));
                for (uint32_t i = 0; i < emitter_count; ++i)
                {
                    dmParticleDDF::Emitter* emitter_ddf = &ddf->m_Emitters[i];
                    EmitterPrototype* emitter = &prototype->m_Emitters[i];
                    emitter->m_DDF = emitter_ddf;
                    emitter->m_Animation = dmHashString64(emitter_ddf->m_Animation);
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

    DM_PARTICLE_TRAMPOLINE2(void, StartInstance, HContext, HInstance);
    DM_PARTICLE_TRAMPOLINE2(void, StopInstance, HContext, HInstance);
    DM_PARTICLE_TRAMPOLINE2(void, RestartInstance, HContext, HInstance);
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

}
