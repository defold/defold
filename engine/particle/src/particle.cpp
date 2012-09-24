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

    /// Config key to use for tweaking maximum number of emitters in a context.
    const char* MAX_EMITTER_COUNT_KEY        = "particle_system.max_emitter_count";
    /// Config key to use for tweaking the total maximum number of particles in a context.
    const char* MAX_PARTICLE_COUNT_KEY       = "particle_system.max_particle_count";

    HContext CreateContext(uint32_t max_emitter_count, uint32_t max_particle_count)
    {
        return new Context(max_emitter_count, max_particle_count);
    }

    void DestroyContext(HContext context)
    {
        uint32_t lingering = 0;
        for (uint32_t i=0; i < context->m_Emitters.Size(); ++i)
        {
            if (context->m_Emitters[i] != 0x0 && context->m_Emitters[i]->m_Dangling == 0)
                ++lingering;
            delete context->m_Emitters[i];
        }
        if (lingering > 0)
            dmLogWarning("Destroyed %d client-owned emitters (this might indicate leakage).", lingering);
        delete context;
    }

    static Emitter* GetEmitter(HContext context, HEmitter emitter)
    {
        if (emitter == INVALID_EMITTER)
            return 0x0;
        uint16_t version = emitter >> 16;
        Emitter* e = context->m_Emitters[emitter & 0xffff];

        if (version != e->m_VersionNumber)
        {
            dmLogError("Stale emitter handle");
            return 0;
        }
        return e;
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

    HEmitter CreateEmitter(HContext context, HPrototype prototype)
    {
        if (context->m_EmitterIndexPool.Remaining() == 0)
        {
            dmLogWarning("Emitter could not be created since the buffer is full (%d). Tweak \"%s\" in the config file.", context->m_Emitters.Capacity(), MAX_EMITTER_COUNT_KEY);
            return 0;
        }
        if (prototype->m_DDF->m_MaxParticleCount > context->m_MaxParticleCount)
        {
            dmLogWarning("Emitter could not be created since it has a higher particle count than the maximum number allowed (%d). Tweak \"%s\" in the config file.", context->m_MaxParticleCount, MAX_EMITTER_COUNT_KEY);
            return 0;
        }
        Emitter* e = new Emitter;
        uint16_t index = context->m_EmitterIndexPool.Pop();

        // Avoid zero in order to ensure that HEmitter != INVALID_EMITTER for valid handles.
        if (context->m_NextVersionNumber == INVALID_EMITTER) context->m_NextVersionNumber++;
        e->m_VersionNumber = context->m_NextVersionNumber++;

        context->m_Emitters[index] = e;

        dmParticleDDF::Emitter* ddf = prototype->m_DDF;
        e->m_Prototype = prototype;
        e->m_Particles.SetCapacity(ddf->m_MaxParticleCount);
        e->m_Particles.SetSize(ddf->m_MaxParticleCount);
        memset(&e->m_Particles.Front(), 0, ddf->m_MaxParticleCount * sizeof(Particle));
        e->m_Timer = 0.0f;

        return e->m_VersionNumber << 16 | index;
    }

    void DestroyEmitter(HContext context, HEmitter emitter)
    {
        if (emitter == INVALID_EMITTER) return;
        Emitter* e = GetEmitter(context, emitter);
        if (!e) return;
        uint32_t index = emitter & 0xffff;
        context->m_EmitterIndexPool.Push(index);
        context->m_Emitters[index] = 0;
        delete e;
    }

    void StartEmitter(HContext context, HEmitter emitter)
    {
        if (emitter == INVALID_EMITTER) return;
        Emitter* e = GetEmitter(context, emitter);
        if (!e) return;
        e->m_IsSpawning = 1;
    }

    void StopEmitter(HContext context, HEmitter emitter)
    {
        if (emitter == INVALID_EMITTER) return;
        Emitter* e = GetEmitter(context, emitter);
        if (!e) return;
        e->m_IsSpawning = 0;
    }

    void RestartEmitter(HContext context, HEmitter emitter)
    {
        if (emitter == INVALID_EMITTER) return;
        Emitter* e = GetEmitter(context, emitter);
        if (!e) return;
        e->m_IsSpawning = 1;
        e->m_Timer = 0.0f;
        e->m_SpawnTimer = 0.0f;
        e->m_SpawnDelay = 0.0f;
    }

    void FireAndForget(HContext context, Prototype* prototype, Point3 position, Quat rotation)
    {
        if (prototype == 0x0)
            return;
        HEmitter e = CreateEmitter(context, prototype);
        Emitter* emitter = GetEmitter(context, e);
        if (emitter != 0x0)
        {
            // A looping emitter would never be removed, destroy it and report error
            if (emitter->m_Prototype->m_DDF->m_Mode == PLAY_MODE_LOOP)
            {
                dmLogError("The emitter is set to loop and can't be created in this manner, it would never be destroyed.");
                DestroyEmitter(context, e);
                return;
            }
            emitter->m_Dangling = 1;
            emitter->m_IsSpawning = 1;
            emitter->m_Position = position;
            emitter->m_Rotation = rotation;
        }
    }

    void SetPosition(HContext context, HEmitter emitter, Point3 position)
    {
        Emitter* e = GetEmitter(context, emitter);
        if (!e) return;
        e->m_Position = position;
    }

    void SetRotation(HContext context, HEmitter emitter, Quat rotation)
    {
        Emitter* e = GetEmitter(context, emitter);
        if (!e) return;
        e->m_Rotation = rotation;
    }

    void SetGain(HContext context, HEmitter emitter, float gain)
    {
        Emitter* e = GetEmitter(context, emitter);
        if (!e) return;
        e->m_Gain = dmMath::Clamp(gain, 0.0f, 1.0f);
    }

    bool IsSpawning(HContext context, HEmitter emitter)
    {
        Emitter* e = GetEmitter(context, emitter);
        if (!e) return false;

        return e->m_IsSpawning;
    }

    bool IsSleeping(Emitter* emitter)
    {
        // Consider 0x0 emitters as sleeping
        if (!emitter) return true;
        // Looping emitters never sleeps
        if (emitter->m_Prototype->m_DDF->m_Mode == PLAY_MODE_LOOP) return false;
        // Sleeping when not spawning and no particles alive
        return emitter->m_IsSpawning != 1 && emitter->m_ParticleTimeLeft <= 0.0f;
    }

    bool IsSleeping(HContext context, HEmitter emitter)
    {
        return IsSleeping(GetEmitter(context, emitter));
    }

    // helper functions in update
    static void SpawnParticle(HContext context, Particle* particle, Emitter* emitter);
    static uint32_t UpdateRenderData(HContext context, Emitter* emitter, uint32_t vertex_index, float* vertex_buffer, uint32_t vertex_buffer_size);

    static void EvaluateParticleProperties(Emitter* emitter)
    {
        float properties[PARTICLE_KEY_COUNT];
        uint32_t count = emitter->m_Particles.Size();
        Prototype* prototype = emitter->m_Prototype;
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

    void Update(HContext context, float dt, float* vertex_buffer, uint32_t vertex_buffer_size, uint32_t* out_vertex_buffer_size)
    {
        DM_PROFILE(Particle, "Update");

        // vertex buffer index for each emitter
        uint32_t vertex_index = 0;

        uint32_t size = context->m_Emitters.Size();
        for (uint32_t i = 0; i < size; i++)
        {
            Emitter* emitter = context->m_Emitters[i];
            // empty slot
            if (!emitter) continue;
            // don't update sleeping emitters
            if (IsSleeping(emitter))
            {
                // delete fire and forget
                if (emitter->m_Dangling)
                    DestroyEmitter(context, emitter->m_VersionNumber << 16 | i);
                else
                    // don't render
                    emitter->m_VertexCount = 0;
                continue;
            }

            // Resize particle buffer if the resource has been reloaded, wipe particles
            if (emitter->m_Particles.Size() != emitter->m_Prototype->m_DDF->m_MaxParticleCount)
            {
                if (emitter->m_Prototype->m_DDF->m_MaxParticleCount <= context->m_MaxParticleCount)
                {
                    emitter->m_Particles.SetCapacity(emitter->m_Prototype->m_DDF->m_MaxParticleCount);
                    emitter->m_Particles.SetSize(emitter->m_Prototype->m_DDF->m_MaxParticleCount);
                    memset(&emitter->m_Particles.Front(), 0, emitter->m_Prototype->m_DDF->m_MaxParticleCount * sizeof(Particle));
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
                        SpawnParticle(context, p, emitter);
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
                vertex_index += UpdateRenderData(context, emitter, vertex_index, vertex_buffer, vertex_buffer_size);

            // Update emitter
            if (emitter->m_IsSpawning)
            {
                emitter->m_Timer += dt;
                // never let the spawn timer accumulate, it would result in a burst of spawned particles
                if (emitter->m_SpawnTimer < emitter->m_SpawnDelay)
                    emitter->m_SpawnTimer += dt;
                // stop once-emitters that have lived their life
                if (emitter->m_Prototype->m_DDF->m_Mode == PLAY_MODE_ONCE && emitter->m_Timer >= emitter->m_Prototype->m_DDF->m_Duration)
                    emitter->m_IsSpawning = 0;
            }
            if (emitter->m_ParticleTimeLeft > 0.0f)
                emitter->m_ParticleTimeLeft -= dt;
        }

        if (out_vertex_buffer_size != 0x0)
        {
            *out_vertex_buffer_size = vertex_index * sizeof(Vertex);
        }
    }

    static void EvaluateEmitterProperties(Emitter* emitter, float properties[EMITTER_KEY_COUNT])
    {
        Prototype* prototype = emitter->m_Prototype;
        dmParticleDDF::Emitter* ddf = prototype->m_DDF;
        float x = emitter->m_Timer / ddf->m_Duration;
        for (uint32_t i = 0; i < EMITTER_KEY_COUNT; ++i)
        {
            Property* property = &prototype->m_Properties[i];
            uint32_t segment_index = (uint32_t)(x * PROPERTY_SAMPLE_COUNT);
            LinearSegment* segment = &property->m_Segments[segment_index];
            float y = (x - segment->m_X) * segment->m_K + segment->m_Y;
            properties[i] = y;
        }
    }

    static void SpawnParticle(HContext context, Particle* particle, Emitter* emitter)
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

        switch (emitter->m_Prototype->m_DDF->m_Space)
        {
        case EMISSION_SPACE_WORLD:
            particle->m_Position = emitter->m_Position + rotate(emitter->m_Rotation, local_position);
            particle->m_Rotation = Quat::identity();
            particle->m_Velocity = velocity;
            break;
        case EMISSION_SPACE_EMITTER:
            particle->m_Position = Point3(local_position);
            particle->m_Rotation = emitter->m_Prototype->m_DDF->m_Rotation;
            particle->m_Velocity = rotate(particle->m_Rotation, velocity);
            break;
        }
    }

    static uint32_t UpdateRenderData(HContext context, Emitter* emitter, uint32_t vertex_index, float* vertex_buffer, uint32_t vertex_buffer_size)
    {
        DM_PROFILE(Particle, "UpdateRenderData");

        emitter->m_VertexIndex = vertex_index;
        emitter->m_VertexCount = 0;

        // texture animation
        uint32_t total_frames = emitter->m_Prototype->m_DDF->m_Texture.m_TX * emitter->m_Prototype->m_DDF->m_Texture.m_TY;
        float dU = 1.0f / (float)(emitter->m_Prototype->m_DDF->m_Texture.m_TX);
        float dV = 1.0f / (float)(emitter->m_Prototype->m_DDF->m_Texture.m_TY);

        // calculate emission space
        Quat emission_rotation = Quat::identity();
        Vector3 emission_position(0.0f, 0.0f, 0.0f);
        if (emitter->m_Prototype->m_DDF->m_Space == EMISSION_SPACE_EMITTER)
        {
            emission_rotation = emitter->m_Rotation;
            emission_position = Vector3(emitter->m_Position);
        }

        uint32_t tex_tx = emitter->m_Prototype->m_DDF->m_Texture.m_TX;
        uint32_t tex_ty = emitter->m_Prototype->m_DDF->m_Texture.m_TY;
        uint32_t max_vertex_count = vertex_buffer_size / sizeof(Vertex);
        uint32_t j;
        uint32_t particle_count = emitter->m_Particles.Size();
        for (j = 0; j < particle_count && vertex_index + 6 < max_vertex_count; j++)
        {
            Particle* particle = &emitter->m_Particles[j];

            float size = particle->m_Size;
            float alpha = particle->m_Alpha;

            // invisible dead particles
            // TODO: Better to not render them?
            alpha = dmMath::Select(particle->m_TimeLeft, alpha, 0.0f);

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
            uint32_t frame = (uint32_t)((float)total_frames * (1.0f - time_left * particle->m_ooMaxLifeTime) - 0.5f );
            uint32_t uframe = frame % tex_tx;
            uint32_t vframe = frame / tex_ty;

            assert(uframe >= 0 && uframe < tex_tx);
            assert(vframe >= 0 && vframe < tex_ty);

            float u = uframe * dU;
            float v = vframe * dV;
            float u0 = u + 0.0f;
            float v0 = v + dV;
            float u1 = u + 0.0f;
            float v1 = v + 0.0f;
            float u2 = u + dU;
            float v2 = v + dV;
            float u3 = u + dU;
            float v3 = v + 0.0f;

            // store values in the buffer
            uint32_t field_index = vertex_index * VERTEX_FIELD_COUNT;

            vertex_buffer[field_index + 0] = p0.getX();
            vertex_buffer[field_index + 1] = p0.getY();
            vertex_buffer[field_index + 2] = p0.getZ();
            vertex_buffer[field_index + 3] = u0;
            vertex_buffer[field_index + 4] = v0;
            vertex_buffer[field_index + 5] = alpha;

            field_index += VERTEX_FIELD_COUNT;
            vertex_buffer[field_index + 0] = p1.getX();
            vertex_buffer[field_index + 1] = p1.getY();
            vertex_buffer[field_index + 2] = p1.getZ();
            vertex_buffer[field_index + 3] = u1;
            vertex_buffer[field_index + 4] = v1;
            vertex_buffer[field_index + 5] = alpha;

            field_index += VERTEX_FIELD_COUNT;
            vertex_buffer[field_index + 0] = p2.getX();
            vertex_buffer[field_index + 1] = p2.getY();
            vertex_buffer[field_index + 2] = p2.getZ();
            vertex_buffer[field_index + 3] = u2;
            vertex_buffer[field_index + 4] = v2;
            vertex_buffer[field_index + 5] = alpha;

            field_index += VERTEX_FIELD_COUNT;
            vertex_buffer[field_index + 0] = p2.getX();
            vertex_buffer[field_index + 1] = p2.getY();
            vertex_buffer[field_index + 2] = p2.getZ();
            vertex_buffer[field_index + 3] = u2;
            vertex_buffer[field_index + 4] = v2;
            vertex_buffer[field_index + 5] = alpha;

            field_index += VERTEX_FIELD_COUNT;
            vertex_buffer[field_index + 0] = p1.getX();
            vertex_buffer[field_index + 1] = p1.getY();
            vertex_buffer[field_index + 2] = p1.getZ();
            vertex_buffer[field_index + 3] = u1;
            vertex_buffer[field_index + 4] = v1;
            vertex_buffer[field_index + 5] = alpha;

            field_index += VERTEX_FIELD_COUNT;
            vertex_buffer[field_index + 0] = p3.getX();
            vertex_buffer[field_index + 1] = p3.getY();
            vertex_buffer[field_index + 2] = p3.getZ();
            vertex_buffer[field_index + 3] = u3;
            vertex_buffer[field_index + 4] = v3;
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


    void Render(HContext context, void* usercontext, RenderEmitterCallback render_emitter_callback)
    {
        DM_PROFILE(Particle, "Render");

        if (context->m_Emitters.Size() == 0)
            return;

        if (render_emitter_callback != 0x0)
        {
            for (uint32_t i=0; i<context->m_Emitters.Size(); i++)
            {
                Emitter* emitter = context->m_Emitters[i];
                if (!emitter || emitter->m_VertexCount == 0) continue;

                render_emitter_callback(usercontext, emitter->m_Position, emitter->m_Prototype->m_Material, emitter->m_Prototype->m_Texture, emitter->m_VertexIndex, emitter->m_VertexCount);
            }
        }
    }

    void DebugRender(HContext context, void* user_context, RenderLineCallback render_line_callback)
    {
        for (uint32_t i=0; i<context->m_Emitters.Size(); i++)
        {
            Emitter* e = context->m_Emitters[i];
            if (!e || e == INVALID_EMITTER) continue;

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
                        render_line_callback(user_context, e->m_Position + rotate(e->m_Rotation, vertices[j-1][k]), e->m_Position + rotate(e->m_Rotation, vertices[j][k]), color);
                }
                break;
            }
            case EMITTER_TYPE_CONE:
            {
                const float radius = ddf->m_Properties[EMITTER_KEY_CONE_RADIUS].m_Points[0].m_Y;
                const float height = ddf->m_Properties[EMITTER_KEY_CONE_HEIGHT].m_Points[0].m_Y;

                // 4 pillars
                render_line_callback(user_context, e->m_Position, e->m_Position + rotate(e->m_Rotation, Vector3(radius, 0.0f, height)), color);
                render_line_callback(user_context, e->m_Position, e->m_Position + rotate(e->m_Rotation, Vector3(-radius, 0.0f, height)), color);
                render_line_callback(user_context, e->m_Position, e->m_Position + rotate(e->m_Rotation, Vector3(0.0f, radius, height)), color);
                render_line_callback(user_context, e->m_Position, e->m_Position + rotate(e->m_Rotation, Vector3(0.0f, -radius, height)), color);
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
                    render_line_callback(user_context, e->m_Position + rotate(e->m_Rotation, vertices[j-1]), e->m_Position + rotate(e->m_Rotation, vertices[j]), color);
                }
                render_line_callback(user_context, e->m_Position + rotate(e->m_Rotation, vertices[segment_count - 1]), e->m_Position + rotate(e->m_Rotation, vertices[0]), color);
                break;
            }
            case EMITTER_TYPE_BOX:
            {
                const float x_ext = 0.5f * ddf->m_Properties[EMITTER_KEY_BOX_WIDTH].m_Points[0].m_Y;
                const float y_ext = 0.5f * ddf->m_Properties[EMITTER_KEY_BOX_HEIGHT].m_Points[0].m_Y;
                const float z_ext = 0.5f * ddf->m_Properties[EMITTER_KEY_BOX_DEPTH].m_Points[0].m_Y;

                render_line_callback(user_context, e->m_Position + rotate(e->m_Rotation, Vector3(-x_ext, -y_ext, -z_ext)), e->m_Position + rotate(e->m_Rotation, Vector3(x_ext, -y_ext, -z_ext)), color);
                render_line_callback(user_context, e->m_Position + rotate(e->m_Rotation, Vector3(x_ext, -y_ext, -z_ext)), e->m_Position + rotate(e->m_Rotation, Vector3(x_ext, y_ext, -z_ext)), color);
                render_line_callback(user_context, e->m_Position + rotate(e->m_Rotation, Vector3(x_ext, y_ext, -z_ext)), e->m_Position + rotate(e->m_Rotation, Vector3(-x_ext, y_ext, -z_ext)), color);
                render_line_callback(user_context, e->m_Position + rotate(e->m_Rotation, Vector3(-x_ext, y_ext, -z_ext)), e->m_Position + rotate(e->m_Rotation, Vector3(-x_ext, -y_ext, -z_ext)), color);

                render_line_callback(user_context, e->m_Position + rotate(e->m_Rotation, Vector3(-x_ext, -y_ext, z_ext)), e->m_Position + rotate(e->m_Rotation, Vector3(x_ext, -y_ext, z_ext)), color);
                render_line_callback(user_context, e->m_Position + rotate(e->m_Rotation, Vector3(x_ext, -y_ext, z_ext)), e->m_Position + rotate(e->m_Rotation, Vector3(x_ext, y_ext, z_ext)), color);
                render_line_callback(user_context, e->m_Position + rotate(e->m_Rotation, Vector3(x_ext, y_ext, z_ext)), e->m_Position + rotate(e->m_Rotation, Vector3(-x_ext, y_ext, z_ext)), color);
                render_line_callback(user_context, e->m_Position + rotate(e->m_Rotation, Vector3(-x_ext, y_ext, z_ext)), e->m_Position + rotate(e->m_Rotation, Vector3(-x_ext, -y_ext, z_ext)), color);

                render_line_callback(user_context, e->m_Position + rotate(e->m_Rotation, Vector3(-x_ext, -y_ext, -z_ext)), e->m_Position + rotate(e->m_Rotation, Vector3(-x_ext, -y_ext, z_ext)), color);
                render_line_callback(user_context, e->m_Position + rotate(e->m_Rotation, Vector3(x_ext, -y_ext, -z_ext)), e->m_Position + rotate(e->m_Rotation, Vector3(x_ext, -y_ext, z_ext)), color);
                render_line_callback(user_context, e->m_Position + rotate(e->m_Rotation, Vector3(x_ext, y_ext, -z_ext)), e->m_Position + rotate(e->m_Rotation, Vector3(x_ext, y_ext, z_ext)), color);
                render_line_callback(user_context, e->m_Position + rotate(e->m_Rotation, Vector3(-x_ext, y_ext, -z_ext)), e->m_Position + rotate(e->m_Rotation, Vector3(-x_ext, y_ext, z_ext)), color);

                break;
            }

                default:
                    break;
            }
        }
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


    DM_PARTICLE_TRAMPOLINE2(HContext, CreateContext, uint32_t, uint32_t);
    DM_PARTICLE_TRAMPOLINE1(void, DestroyContext, HContext);

    Prototype* Particle_NewPrototype(HContext context, void* emitter_data, uint32_t emitter_data_size)
    {
        dmParticleDDF::Emitter* emitter = 0;
        dmDDF::Result r = dmDDF::LoadMessage<dmParticleDDF::Emitter>(emitter_data, emitter_data_size, &emitter);
        if (r == dmDDF::RESULT_OK)
        {
            if (emitter->m_Material == 0x0 || *emitter->m_Material == '\0'
                        || emitter->m_Texture.m_Name == 0x0 || *emitter->m_Texture.m_Name == '\0')
            {
                dmDDF::FreeMessage(emitter);
                dmLogError("Failed to load emitter data because of unspecified resources");
                return 0;
            }
            Prototype* prototype = new Prototype();
            prototype->m_DDF = emitter;
            // Approximate splines with linear segments
            memset(prototype->m_Properties, 0, sizeof(prototype->m_Properties));
            memset(prototype->m_ParticleProperties, 0, sizeof(prototype->m_ParticleProperties));
            uint32_t prop_count = emitter->m_Properties.m_Count;
            for (uint32_t i = 0; i < prop_count; ++i)
            {
                const dmParticleDDF::Emitter::Property& p = emitter->m_Properties[i];
                SampleProperty(p.m_Points.m_Data, p.m_Points.m_Count, prototype->m_Properties[p.m_Key].m_Segments);
            }
            prop_count = emitter->m_ParticleProperties.m_Count;
            for (uint32_t i = 0; i < prop_count; ++i)
            {
                const dmParticleDDF::Emitter::ParticleProperty& p = emitter->m_ParticleProperties[i];
                SampleProperty(p.m_Points.m_Data, p.m_Points.m_Count, prototype->m_ParticleProperties[p.m_Key].m_Segments);
            }

            return prototype;
        }
        else
        {
            dmLogError("Failed to load emitter data");
            return 0;
        }
    }

    void Particle_DeletePrototype(HContext context, HPrototype prototype)
    {
        dmDDF::FreeMessage(prototype->m_DDF);
        delete prototype;
    }

    DM_PARTICLE_TRAMPOLINE2(HEmitter, CreateEmitter, HContext, HPrototype);
    DM_PARTICLE_TRAMPOLINE2(void, DestroyEmitter, HContext, HEmitter);

    DM_PARTICLE_TRAMPOLINE2(void, StartEmitter, HContext, HEmitter);
    DM_PARTICLE_TRAMPOLINE2(void, StopEmitter, HContext, HEmitter);
    DM_PARTICLE_TRAMPOLINE2(void, RestartEmitter, HContext, HEmitter);
    DM_PARTICLE_TRAMPOLINE3(void, SetPosition, HContext, HEmitter, Point3);
    DM_PARTICLE_TRAMPOLINE3(void, SetRotation, HContext, HEmitter, Quat);

    DM_PARTICLE_TRAMPOLINE2(bool, IsSpawning, HContext, HEmitter);
    DM_PARTICLE_TRAMPOLINE2(bool, IsSleeping, HContext, HEmitter);
    DM_PARTICLE_TRAMPOLINE5(void, Update, HContext, float, float*, uint32_t, uint32_t*);
    DM_PARTICLE_TRAMPOLINE3(void, Render, HContext, void*, RenderEmitterCallback);

}
