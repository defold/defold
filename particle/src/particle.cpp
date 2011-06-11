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

    /// Config key to use for tweaking maximum number of emitters in a context.
    const char* MAX_EMITTER_COUNT_KEY        = "particle_system.max_emitter_count";
    /// Config key to use for tweaking the total maximum number of particles in a context.
    const char* MAX_PARTICLE_COUNT_KEY       = "particle_system.max_particle_count";

    uint32_t PARTICLE_PROPERTY_INDICES[dmParticleDDF::PARTICLE_KEY_COUNT] =
    {
        0, // PARTICLE_KEY_VELOCITY
        3, // PARTICLE_KEY_SIZE
        4 // PARTICLE_KEY_ALPHA
    };

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

    HEmitter CreateEmitter(HContext context, Prototype* prototype)
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

        e->m_Prototype = prototype;
        e->m_Particles.SetCapacity(prototype->m_DDF->m_MaxParticleCount);
        e->m_Particles.SetSize(prototype->m_DDF->m_MaxParticleCount);
        memset(&e->m_Particles.Front(), 0, prototype->m_DDF->m_MaxParticleCount * sizeof(Particle));
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
    static void ApplyParticleModifiers(Emitter* emitter, float* particle_states);
    static uint32_t UpdateRenderData(HContext context, Emitter* emitter, uint32_t vertex_index, float* particle_state, float* vertex_buffer, uint32_t vertex_buffer_size);

    void Update(HContext context, float dt, float* vertex_buffer, uint32_t vertex_buffer_size)
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

            // init states
            for (uint32_t j = 0; j < particle_count; ++j)
            {
                Particle* p = &emitter->m_Particles[j];
                memcpy(&context->m_ParticleStates[j * PARTICLE_PROPERTY_FIELD_COUNT], p->m_Properties, sizeof(float) * PARTICLE_PROPERTY_FIELD_COUNT);
            }

            // Modifiers
            ApplyParticleModifiers(emitter, context->m_ParticleStates);

            // Simulate
            {
                DM_PROFILE(Particle, "Simulate");
                for (uint32_t j = 0; j < particle_count; j++)
                {
                    Particle* p = &emitter->m_Particles[j];
                    float* v = &context->m_ParticleStates[j * PARTICLE_PROPERTY_FIELD_COUNT + PARTICLE_PROPERTY_INDICES[PARTICLE_KEY_VELOCITY]];
                    p->m_Position += Vector3(v[0], v[1], v[2]) * dt;
                    p->m_TimeLeft -= dt;
                }
            }

            // Render data
            if (vertex_buffer != 0x0)
                vertex_index += UpdateRenderData(context, emitter, vertex_index, context->m_ParticleStates, vertex_buffer, vertex_buffer_size);

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
    }

    static float CalcModifierWeight(ModifierType type, float t, float* data)
    {
        // value at t = 0
        float min = data[MODIFIER_KEY_MIN];
        // value at t = t_mid
        float mid = data[MODIFIER_KEY_MID];
        // value at t = 1
        float max = data[MODIFIER_KEY_MAX];
        float t_mid = data[MODIFIER_KEY_T];
        switch (type)
        {
        case MODIFIER_TYPE_LINEAR:
            return dmMath::LinearBezier(t, min, max);
        case MODIFIER_TYPE_EASE_IN:
            return dmMath::CubicBezier(t, min, min, min, max);
        case MODIFIER_TYPE_EASE_OUT:
            return dmMath::CubicBezier(t, min, max, max, max);
        case MODIFIER_TYPE_EASE_IN_OUT:
            return dmMath::CubicBezier(t, min, min, max, max);
        case MODIFIER_TYPE_TWO_SEG_SPLINE:
            {
                float t0 = t / t_mid;
                float t1 = (t - t_mid) / (1.0f - t_mid);
                float w0 = dmMath::CubicBezier(t0, min, min, mid, mid);
                float w1 = dmMath::CubicBezier(t1, mid, mid, max, max);
                return dmMath::Select(t - t_mid, w1, w0);
            }
        default:
            dmLogFatal("Unknown modifier type (%d) when calculating weight.", type);
            return -1.0f;
        }
    }

    static void SetParticleProperty(float* properties, ParticleKey key, const float* in_values);

    static void SpawnParticle(HContext context, Particle* particle, Emitter* emitter)
    {
        dmParticleDDF::Emitter* ddf = emitter->m_Prototype->m_DDF;

        memset(emitter->m_Properties, 0, sizeof(emitter->m_Properties));
        emitter->m_Properties[EMITTER_KEY_PARTICLE_ALPHA] = 1.0f;

        float key_weights[EMITTER_KEY_COUNT];
        for (uint32_t i = 0; i < EMITTER_KEY_COUNT; ++i)
            key_weights[i] = 1.0f;
        for (uint32_t i = 0; i < ddf->m_Properties.m_Count; ++i)
        {
            const dmParticleDDF::Emitter::Property& property = ddf->m_Properties[i];
            float weight = 1.0f;
            for (uint32_t j = 0; j < property.m_Modifiers.m_Count; ++j)
            {
                const dmParticleDDF::Emitter::Modifier& modifier = property.m_Modifiers[j];
                float t = 0.0f;
                switch (modifier.m_Input)
                {
                case dmParticleDDF::Emitter::Modifier::INPUT_TYPE_GAIN:
                    t = emitter->m_Gain;
                    break;
                case dmParticleDDF::Emitter::Modifier::INPUT_TYPE_RAND:
                    t = dmMath::Rand01();
                    break;
                default:
                    dmLogError("Unknown input type (%d).", modifier.m_Input);
                }
                float modifier_values[MODIFIER_KEY_COUNT];
                memcpy(modifier_values, context->m_DefaultModifierProperties, sizeof(modifier_values));
                for (uint32_t k = 0; k < modifier.m_Properties.m_Count; ++k)
                {
                    const dmParticleDDF::ModifierProperty& p = modifier.m_Properties[k];
                    modifier_values[p.m_Key] = p.m_Value;
                }
                weight *= CalcModifierWeight(modifier.m_Type, t, modifier_values);
            }
            emitter->m_Properties[property.m_Key] = property.m_Value;
            key_weights[property.m_Key] = weight;
        }
        emitter->m_SpawnDelay = emitter->m_Properties[EMITTER_KEY_SPAWN_DELAY] * key_weights[EMITTER_KEY_SPAWN_DELAY];
        particle->m_MaxLifeTime = emitter->m_Properties[EMITTER_KEY_PARTICLE_LIFE_TIME] * key_weights[EMITTER_KEY_PARTICLE_LIFE_TIME];
        particle->m_ooMaxLifeTime = 1.0f / particle->m_MaxLifeTime;
        particle->m_TimeLeft = particle->m_MaxLifeTime;

        if (emitter->m_ParticleTimeLeft < particle->m_TimeLeft)
            emitter->m_ParticleTimeLeft = particle->m_TimeLeft;

        Vector3 local_position;

        switch (ddf->m_Type)
        {
            case EMITTER_TYPE_SPHERE:
            {
                Vector3 p = Vector3::zAxis();
                Vector3 v(dmMath::Rand11(), dmMath::Rand11(), dmMath::Rand11());
                if (lengthSqr(v) > 0.0f)
                    p = normalize(v);

                local_position = p * emitter->m_Properties[EMITTER_KEY_SPHERE_RADIUS] * key_weights[EMITTER_KEY_SPHERE_RADIUS];

                break;
            }

            case EMITTER_TYPE_CONE:
            {
                float height = emitter->m_Properties[EMITTER_KEY_CONE_HEIGHT] * key_weights[EMITTER_KEY_CONE_HEIGHT];
                float radius = emitter->m_Properties[EMITTER_KEY_CONE_RADIUS] * key_weights[EMITTER_KEY_CONE_RADIUS] * key_weights[EMITTER_KEY_CONE_HEIGHT];
                float angle = 2.0f * ((float) M_PI) * dmMath::RandOpen01();

                local_position = Vector3(cosf(angle) * radius, sinf(angle) * radius, height);

                break;
            }

            case EMITTER_TYPE_BOX:
            {
                float width = emitter->m_Properties[EMITTER_KEY_BOX_WIDTH] * key_weights[EMITTER_KEY_BOX_WIDTH];
                float height = emitter->m_Properties[EMITTER_KEY_BOX_HEIGHT] * key_weights[EMITTER_KEY_BOX_HEIGHT];
                float depth = emitter->m_Properties[EMITTER_KEY_BOX_DEPTH] * key_weights[EMITTER_KEY_BOX_DEPTH];
                local_position = Vector3(dmMath::Rand11() * width, dmMath::Rand11() * height, dmMath::Rand11() * depth);

                break;
            }

            default:
                dmLogWarning("Unknown emitter type (%d), particle is spawned at emitter.", ddf->m_Type);
                local_position = Vector3(0.0f, 0.0f, 0.0f);
                break;
        }

        float speed = emitter->m_Properties[EMITTER_KEY_PARTICLE_SPEED] * key_weights[EMITTER_KEY_PARTICLE_SPEED];
        Vector3 velocity = Vector3::zAxis();
        if (lengthSqr(local_position) > 0.0f)
            velocity = normalize(local_position);

        switch (emitter->m_Prototype->m_DDF->m_Space)
        {
        case EMISSION_SPACE_WORLD:
            particle->m_Position = emitter->m_Position + rotate(emitter->m_Rotation, local_position);
            particle->m_Rotation = Quat::identity();
            velocity = speed * rotate(emitter->m_Rotation, velocity);
            break;
        case EMISSION_SPACE_EMITTER:
            particle->m_Position = Point3(local_position);
            particle->m_Rotation = emitter->m_Prototype->m_DDF->m_Rotation;
            velocity *= speed;
            break;
        }
        // Store properties
        float particle_velocity[] = {velocity.getX(), velocity.getY(), velocity.getZ()};
        SetParticleProperty(particle->m_Properties, PARTICLE_KEY_VELOCITY, particle_velocity);
        float particle_size = emitter->m_Properties[EMITTER_KEY_PARTICLE_SIZE] * key_weights[EMITTER_KEY_PARTICLE_SIZE];
        SetParticleProperty(particle->m_Properties, PARTICLE_KEY_SIZE, &particle_size);
        float particle_alpha = emitter->m_Properties[EMITTER_KEY_PARTICLE_ALPHA] * key_weights[EMITTER_KEY_PARTICLE_ALPHA];
        SetParticleProperty(particle->m_Properties, PARTICLE_KEY_ALPHA, &particle_alpha);
    }

    static uint32_t UpdateRenderData(HContext context, Emitter* emitter, uint32_t vertex_index, float* particle_state, float* vertex_buffer, uint32_t vertex_buffer_size)
    {
        DM_PROFILE(Particle, "UpdateRenderData");

        uint32_t particle_count = emitter->m_Particles.Size();
        if (particle_count * 4 <= vertex_buffer_size - vertex_index)
        {
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

            for (uint32_t j = 0; j < particle_count; j++)
            {
                Particle* particle = &emitter->m_Particles[j];

                float size = particle_state[j * PARTICLE_PROPERTY_FIELD_COUNT + PARTICLE_PROPERTY_INDICES[PARTICLE_KEY_SIZE]];
                float alpha = particle_state[j * PARTICLE_PROPERTY_FIELD_COUNT + PARTICLE_PROPERTY_INDICES[PARTICLE_KEY_ALPHA]];

                // shrink dead particles
                // TODO: Better to not render them?
                size = dmMath::Select(particle->m_TimeLeft, size, 0.0f);

                Vector3 particle_position = rotate(emission_rotation, Vector3(particle->m_Position)) + emission_position;
                Quat particle_rotation = emission_rotation * particle->m_Rotation;

                Vector3 x = rotate(particle_rotation, Vector3(size, 0.0f, 0.0f));
                Vector3 y = rotate(particle_rotation, Vector3(0.0f, size, 0.0f));

                Vector3 p0 = -x - y + particle_position;
                Vector3 p1 = -x + y + particle_position;
                Vector3 p2 = x + y + particle_position;
                Vector3 p3 = x - y + particle_position;

                // avoid wrapping for dead particles
                float time_left = dmMath::Select(particle->m_TimeLeft, particle->m_TimeLeft, 0.0f);
                uint32_t frame = (uint32_t)((float)total_frames * (1.0f - time_left * particle->m_ooMaxLifeTime) - 0.5f );
                uint32_t uframe = frame % emitter->m_Prototype->m_DDF->m_Texture.m_TX;
                uint32_t vframe = frame / emitter->m_Prototype->m_DDF->m_Texture.m_TX;

                assert(uframe >= 0 && uframe < emitter->m_Prototype->m_DDF->m_Texture.m_TX);
                assert(vframe >= 0 && vframe < emitter->m_Prototype->m_DDF->m_Texture.m_TY);

                float u = uframe * dU;
                float v = vframe * dV;
                float u0 = u + 0.0f;
                float v0 = v + 0.0f;
                float u1 = u + 0.0f;
                float v1 = v + dV;
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
                ++vertex_index;
                field_index += VERTEX_FIELD_COUNT;
                vertex_buffer[field_index + 0] = p1.getX();
                vertex_buffer[field_index + 1] = p1.getY();
                vertex_buffer[field_index + 2] = p1.getZ();
                vertex_buffer[field_index + 3] = u1;
                vertex_buffer[field_index + 4] = v1;
                vertex_buffer[field_index + 5] = alpha;
                ++vertex_index;
                field_index += VERTEX_FIELD_COUNT;
                vertex_buffer[field_index + 0] = p2.getX();
                vertex_buffer[field_index + 1] = p2.getY();
                vertex_buffer[field_index + 2] = p2.getZ();
                vertex_buffer[field_index + 3] = u2;
                vertex_buffer[field_index + 4] = v2;
                vertex_buffer[field_index + 5] = alpha;
                ++vertex_index;
                field_index += VERTEX_FIELD_COUNT;
                vertex_buffer[field_index + 0] = p3.getX();
                vertex_buffer[field_index + 1] = p3.getY();
                vertex_buffer[field_index + 2] = p3.getZ();
                vertex_buffer[field_index + 3] = u3;
                vertex_buffer[field_index + 4] = v3;
                vertex_buffer[field_index + 5] = alpha;
                ++vertex_index;
            }
        }
        else
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

            Vectormath::Aos::Vector4 color(0.0f, 1.0f, 0.0f, 1.0f);
            if (IsSleeping(e))
            {
                color.setY(0.0f);
                color.setZ(1.0f);
            }
            else if (e->m_Prototype->m_DDF->m_Mode == dmParticle::PLAY_MODE_ONCE)
            {
                float t = e->m_Timer / e->m_Prototype->m_DDF->m_Duration;
                color.setY(1.0f - t);
                color.setZ(t);
            }

            switch (e->m_Prototype->m_DDF->m_Type)
            {
            case EMITTER_TYPE_SPHERE:
            {
                const float radius = e->m_Properties[EMITTER_KEY_SPHERE_RADIUS];

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
                const float radius = e->m_Properties[EMITTER_KEY_CONE_RADIUS];
                const float height = e->m_Properties[EMITTER_KEY_CONE_HEIGHT];

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
                const float x_ext = 0.5f * e->m_Properties[EMITTER_KEY_BOX_WIDTH];
                const float y_ext = 0.5f * e->m_Properties[EMITTER_KEY_BOX_HEIGHT];
                const float z_ext = 0.5f * e->m_Properties[EMITTER_KEY_BOX_DEPTH];

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

    static void ApplyEaseIn1ParticleModifier(Emitter* emitter, float min, float max, ParticleKey key, float* output_states);
    static void ApplyTwoSegSpline3ParticleModifier(Emitter* emitter, float t_mid, float min, float mid, float max, ParticleKey key, float* output_states);
    static void ApplyTwoSegSpline1ParticleModifier(Emitter* emitter, float t_mid, float min, float mid, float max, ParticleKey key, float* output_states);
    static void ApplyEaseInOut3ParticleModifier(Emitter* emitter, float min, float max, ParticleKey key, float* output_states);
    static void ApplyEaseInOut1ParticleModifier(Emitter* emitter, float min, float max, ParticleKey key, float* output_states);
    static void ApplyEaseOut3ParticleModifier(Emitter* emitter, float min, float max, ParticleKey key, float* output_states);
    static void ApplyEaseOut1ParticleModifier(Emitter* emitter, float min, float max, ParticleKey key, float* output_states);
    static void ApplyEaseIn3ParticleModifier(Emitter* emitter, float min, float max, ParticleKey key, float* output_states);
    static void ApplyLinear3ParticleModifier(Emitter* emitter, float min, float max, ParticleKey key, float* output_states);
    static void ApplyLinear1ParticleModifier(Emitter* emitter, float min, float max, ParticleKey key, float* output_states);

    static void ApplyParticleModifiers(Emitter* emitter, float* particle_states)
    {
        DM_PROFILE(Particle, "ApplyParticleModifiers");
        for (uint32_t j = 0; j < emitter->m_Prototype->m_DDF->m_ParticleModifiers.m_Count; ++j)
        {
            const dmParticleDDF::Emitter::ParticleModifier& modifier = emitter->m_Prototype->m_DDF->m_ParticleModifiers[j];
            float t_mid = 0.5f;
            float min = 0.0f;
            float mid = 0.5f;
            float max = 1.0f;
            for (uint32_t k = 0; k < modifier.m_Properties.m_Count; ++k)
            {
                float value = modifier.m_Properties[k].m_Value;
                switch (modifier.m_Properties[k].m_Key)
                {
                case MODIFIER_KEY_T:
                    t_mid = value;
                    break;
                case MODIFIER_KEY_MIN:
                    min = value;
                    break;
                case MODIFIER_KEY_MID:
                    mid = value;
                    break;
                case MODIFIER_KEY_MAX:
                    max = value;
                    break;
                default:
                    // quietly ignore
                    break;
                }
            }
            switch (modifier.m_Type)
            {
            case MODIFIER_TYPE_LINEAR:
                switch (modifier.m_Modify)
                {
                case PARTICLE_KEY_VELOCITY:
                    ApplyLinear3ParticleModifier(emitter, min, max, modifier.m_Modify, particle_states);
                    break;
                default:
                    ApplyLinear1ParticleModifier(emitter, min, max, modifier.m_Modify, particle_states);
                    break;
                }
                break;
            case MODIFIER_TYPE_EASE_IN:
                switch (modifier.m_Modify)
                {
                case PARTICLE_KEY_VELOCITY:
                    ApplyEaseIn3ParticleModifier(emitter, min, max, modifier.m_Modify, particle_states);
                    break;
                default:
                    ApplyEaseIn1ParticleModifier(emitter, min, max, modifier.m_Modify, particle_states);
                    break;
                }
                break;
            case MODIFIER_TYPE_EASE_OUT:
                switch (modifier.m_Modify)
                {
                case PARTICLE_KEY_VELOCITY:
                    ApplyEaseOut3ParticleModifier(emitter, min, max, modifier.m_Modify, particle_states);
                    break;
                default:
                    ApplyEaseOut1ParticleModifier(emitter, min, max, modifier.m_Modify, particle_states);
                    break;
                }
                break;
            case MODIFIER_TYPE_EASE_IN_OUT:
                switch (modifier.m_Modify)
                {
                case PARTICLE_KEY_VELOCITY:
                    ApplyEaseInOut3ParticleModifier(emitter, min, max, modifier.m_Modify, particle_states);
                    break;
                default:
                    ApplyEaseInOut1ParticleModifier(emitter, min, max, modifier.m_Modify, particle_states);
                    break;
                }
                break;
            case MODIFIER_TYPE_TWO_SEG_SPLINE:
                switch (modifier.m_Modify)
                {
                case PARTICLE_KEY_VELOCITY:
                    ApplyTwoSegSpline3ParticleModifier(emitter, t_mid, min, mid, max, modifier.m_Modify, particle_states);
                    break;
                default:
                    ApplyTwoSegSpline1ParticleModifier(emitter, t_mid, min, mid, max, modifier.m_Modify, particle_states);
                    break;
                }
                break;
            default:
                // quietly ignore
                break;
            }
        }
    }

    static void ApplyLinear1ParticleModifier(Emitter* emitter, float min, float max, ParticleKey key, float* output_states)
    {
        uint32_t index = PARTICLE_PROPERTY_INDICES[key];
        uint32_t particle_count = emitter->m_Particles.Size();
        for (uint32_t j = 0; j < particle_count; j++)
        {
            Particle* p = &emitter->m_Particles[j];
            output_states[j * PARTICLE_PROPERTY_FIELD_COUNT + index] = p->m_Properties[index] * dmMath::LinearBezier(1.0f - p->m_TimeLeft * p->m_ooMaxLifeTime, min, max);
        }
    }

    static void ApplyLinear3ParticleModifier(Emitter* emitter, float min, float max, ParticleKey key, float* output_states)
    {
        uint32_t index = PARTICLE_PROPERTY_INDICES[key];
        uint32_t particle_count = emitter->m_Particles.Size();
        for (uint32_t j = 0; j < particle_count; j++)
        {
            Particle* p = &emitter->m_Particles[j];
            output_states[j * PARTICLE_PROPERTY_FIELD_COUNT + index+0] = p->m_Properties[index+0] * dmMath::LinearBezier(1.0f - p->m_TimeLeft * p->m_ooMaxLifeTime, min, max);
            output_states[j * PARTICLE_PROPERTY_FIELD_COUNT + index+1] = p->m_Properties[index+1] * dmMath::LinearBezier(1.0f - p->m_TimeLeft * p->m_ooMaxLifeTime, min, max);
            output_states[j * PARTICLE_PROPERTY_FIELD_COUNT + index+2] = p->m_Properties[index+2] * dmMath::LinearBezier(1.0f - p->m_TimeLeft * p->m_ooMaxLifeTime, min, max);
        }
    }

    static void ApplyEaseIn1ParticleModifier(Emitter* emitter, float min, float max, ParticleKey key, float* output_states)
    {
        uint32_t index = PARTICLE_PROPERTY_INDICES[key];
        uint32_t particle_count = emitter->m_Particles.Size();
        for (uint32_t j = 0; j < particle_count; j++)
        {
            Particle* p = &emitter->m_Particles[j];
            output_states[j * PARTICLE_PROPERTY_FIELD_COUNT + index] = p->m_Properties[index] * dmMath::CubicBezier(1.0f - p->m_TimeLeft * p->m_ooMaxLifeTime, min, min, min, max);
        }
    }

    static void ApplyEaseIn3ParticleModifier(Emitter* emitter, float min, float max, ParticleKey key, float* output_states)
    {
        uint32_t index = PARTICLE_PROPERTY_INDICES[key];
        uint32_t particle_count = emitter->m_Particles.Size();
        for (uint32_t j = 0; j < particle_count; j++)
        {
            Particle* p = &emitter->m_Particles[j];
            output_states[j * PARTICLE_PROPERTY_FIELD_COUNT + index+0] = p->m_Properties[index+0] * dmMath::CubicBezier(1.0f - p->m_TimeLeft * p->m_ooMaxLifeTime, min, min, min, max);
            output_states[j * PARTICLE_PROPERTY_FIELD_COUNT + index+1] = p->m_Properties[index+1] * dmMath::CubicBezier(1.0f - p->m_TimeLeft * p->m_ooMaxLifeTime, min, min, min, max);
            output_states[j * PARTICLE_PROPERTY_FIELD_COUNT + index+2] = p->m_Properties[index+2] * dmMath::CubicBezier(1.0f - p->m_TimeLeft * p->m_ooMaxLifeTime, min, min, min, max);
        }
    }

    static void ApplyEaseOut1ParticleModifier(Emitter* emitter, float min, float max, ParticleKey key, float* output_states)
    {
        uint32_t index = PARTICLE_PROPERTY_INDICES[key];
        uint32_t particle_count = emitter->m_Particles.Size();
        for (uint32_t j = 0; j < particle_count; j++)
        {
            Particle* p = &emitter->m_Particles[j];
            output_states[j * PARTICLE_PROPERTY_FIELD_COUNT + index] = p->m_Properties[index] * dmMath::CubicBezier(1.0f - p->m_TimeLeft * p->m_ooMaxLifeTime, min, max, max, max);
        }
    }

    static void ApplyEaseOut3ParticleModifier(Emitter* emitter, float min, float max, ParticleKey key, float* output_states)
    {
        uint32_t index = PARTICLE_PROPERTY_INDICES[key];
        uint32_t particle_count = emitter->m_Particles.Size();
        for (uint32_t j = 0; j < particle_count; j++)
        {
            Particle* p = &emitter->m_Particles[j];
            output_states[j * PARTICLE_PROPERTY_FIELD_COUNT + index+0] = p->m_Properties[index+0] * dmMath::CubicBezier(1.0f - p->m_TimeLeft * p->m_ooMaxLifeTime, min, max, max, max);
            output_states[j * PARTICLE_PROPERTY_FIELD_COUNT + index+1] = p->m_Properties[index+1] * dmMath::CubicBezier(1.0f - p->m_TimeLeft * p->m_ooMaxLifeTime, min, max, max, max);
            output_states[j * PARTICLE_PROPERTY_FIELD_COUNT + index+2] = p->m_Properties[index+2] * dmMath::CubicBezier(1.0f - p->m_TimeLeft * p->m_ooMaxLifeTime, min, max, max, max);
        }
    }

    static void ApplyEaseInOut1ParticleModifier(Emitter* emitter, float min, float max, ParticleKey key, float* output_states)
    {
        uint32_t index = PARTICLE_PROPERTY_INDICES[key];
        uint32_t particle_count = emitter->m_Particles.Size();
        for (uint32_t j = 0; j < particle_count; j++)
        {
            Particle* p = &emitter->m_Particles[j];
            output_states[j * PARTICLE_PROPERTY_FIELD_COUNT + index] = p->m_Properties[index] * dmMath::CubicBezier(1.0f - p->m_TimeLeft * p->m_ooMaxLifeTime, min, min, max, max);
        }
    }

    static void ApplyEaseInOut3ParticleModifier(Emitter* emitter, float min, float max, ParticleKey key, float* output_states)
    {
        uint32_t index = PARTICLE_PROPERTY_INDICES[key];
        uint32_t particle_count = emitter->m_Particles.Size();
        for (uint32_t j = 0; j < particle_count; j++)
        {
            Particle* p = &emitter->m_Particles[j];
            output_states[j * PARTICLE_PROPERTY_FIELD_COUNT + index+0] = p->m_Properties[index+0] * dmMath::CubicBezier(1.0f - p->m_TimeLeft * p->m_ooMaxLifeTime, min, min, max, max);
            output_states[j * PARTICLE_PROPERTY_FIELD_COUNT + index+1] = p->m_Properties[index+1] * dmMath::CubicBezier(1.0f - p->m_TimeLeft * p->m_ooMaxLifeTime, min, min, max, max);
            output_states[j * PARTICLE_PROPERTY_FIELD_COUNT + index+2] = p->m_Properties[index+2] * dmMath::CubicBezier(1.0f - p->m_TimeLeft * p->m_ooMaxLifeTime, min, min, max, max);
        }
    }

    static void ApplyTwoSegSpline1ParticleModifier(Emitter* emitter, float t_mid, float min, float mid, float max, ParticleKey key, float* output_states)
    {
        uint32_t index = PARTICLE_PROPERTY_INDICES[key];
        float t_mid_recip = 1.0f / t_mid;
        float s_mid_recip = 1.0f / (1.0f - t_mid);
        uint32_t particle_count = emitter->m_Particles.Size();
        for (uint32_t j = 0; j < particle_count; j++)
        {
            Particle* p = &emitter->m_Particles[j];
            float t = (1.0f - p->m_TimeLeft * p->m_ooMaxLifeTime);
            float t0 = t * t_mid_recip;
            float t1 = (t - t_mid) * s_mid_recip;
            float w0 = dmMath::CubicBezier(t0, min, min, mid, mid);
            float w1 = dmMath::CubicBezier(t1, mid, mid, max, max);
            output_states[j * PARTICLE_PROPERTY_FIELD_COUNT + index] = p->m_Properties[index] * dmMath::Select(t - t_mid, w1, w0);
        }
    }

    static void ApplyTwoSegSpline3ParticleModifier(Emitter* emitter, float t_mid, float min, float mid, float max, ParticleKey key, float* output_states)
    {
        uint32_t index = PARTICLE_PROPERTY_INDICES[key];
        float t_mid_recip = 1.0f / t_mid;
        float s_mid_recip = 1.0f / (1.0f - t_mid);
        uint32_t particle_count = emitter->m_Particles.Size();
        for (uint32_t j = 0; j < particle_count; j++)
        {
            Particle* p = &emitter->m_Particles[j];
            float t = (1.0f - p->m_TimeLeft * p->m_ooMaxLifeTime);
            float t0 = t * t_mid_recip;
            float t1 = (t - t_mid) * s_mid_recip;
            float w0 = dmMath::CubicBezier(t0, min, min, mid, mid);
            float w1 = dmMath::CubicBezier(t1, mid, mid, max, max);
            output_states[j * PARTICLE_PROPERTY_FIELD_COUNT + index+0] = p->m_Properties[index+0] * dmMath::Select(t - t_mid, w1, w0);
            output_states[j * PARTICLE_PROPERTY_FIELD_COUNT + index+1] = p->m_Properties[index+1] * dmMath::Select(t - t_mid, w1, w0);
            output_states[j * PARTICLE_PROPERTY_FIELD_COUNT + index+2] = p->m_Properties[index+2] * dmMath::Select(t - t_mid, w1, w0);
        }
    }

    static void SetParticleProperty(float* properties, ParticleKey key, const float* in_values)
    {
        uint32_t index = PARTICLE_PROPERTY_INDICES[key];
        switch (key)
        {
        case PARTICLE_KEY_VELOCITY:
            memcpy(&properties[index], in_values, sizeof(float) * 3);
            break;
        case PARTICLE_KEY_SIZE:
            memcpy(&properties[index], in_values, sizeof(float));
            break;
        case PARTICLE_KEY_ALPHA:
            memcpy(&properties[index], in_values, sizeof(float));
            break;
        default:
            dmLogError("Unknown particle property: %d.", key);
            break;
        }
    }
}
