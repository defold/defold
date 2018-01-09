#include <string.h>
#include <stdint.h>
#include <float.h>
#include <algorithm>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/vmath.h>
#include <dlib/profile.h>
#include <dlib/time.h>

#include "particle.h"
#include "particle_private.h"

namespace dmParticle
{
    using namespace dmParticleDDF;
    using namespace Vectormath::Aos;

    const static float EPSILON = 0.0001f;
    const static Vector3 PARTICLE_LOCAL_BASE_DIR = Vector3::yAxis();
    const static Vector3 ACCELERATION_LOCAL_DIR = Vector3::yAxis();
    const static Vector3 DRAG_LOCAL_DIR = Vector3::xAxis();
    const static Vector3 VORTEX_LOCAL_AXIS = Vector3::zAxis();
    // Should be set to positive rotation around VORTEX_LOCAL_AXIS
    const static Vector3 VORTEX_LOCAL_START_DIR = -Vector3::xAxis();

    /// Config key to use for tweaking maximum number of instances in a context.
    const char* MAX_INSTANCE_COUNT_KEY          = "particle_fx.max_count";
    /// Config key to use for tweaking the total maximum number of particles in a context.
    const char* MAX_PARTICLE_COUNT_KEY          = "particle_fx.max_particle_count";

    /// Used for degree to radian conversion
    const float DEG_RAD = (float) (M_PI / 180.0);

    /// Simulate motion blur at 60 fps with a 180 deg shutter
    const static float STRETCH_SCALING = (1.0f/60.0f) * 0.5f;

    AnimationData::AnimationData()
    {
        memset(this, 0, sizeof(*this));
    }

    void ResetEmitterStateChangedData(Instance* instance)
    {
        // Deallocate callback data if it is present
        if(instance->m_EmitterStateChangedData.m_UserData != 0x0)
        {
            free(instance->m_EmitterStateChangedData.m_UserData);
            instance->m_EmitterStateChangedData.m_UserData = 0x0;
            instance->m_NumAwakeEmitters = 0;
        }
    }

    HParticleContext CreateContext(uint32_t max_instance_count, uint32_t max_particle_count)
    {
        return new Context(max_instance_count, max_particle_count);
    }

    void DestroyContext(HParticleContext context)
    {
        uint32_t lingering = 0;
        for (uint32_t i=0; i < context->m_Instances.Size(); ++i)
        {
            Instance* instance = context->m_Instances[i];
            if (instance != 0x0)
            {
                ++lingering;
                ResetEmitterStateChangedData(instance);
            }
            delete instance;
        }
        if (lingering > 0)
            dmLogWarning("Destroyed %d instances (this might indicate leakage).", lingering);
        delete context;
    }

    uint32_t GetContextMaxParticleCount(HParticleContext context)
    {
        return context->m_MaxParticleCount;
    }

    void SetContextMaxParticleCount(HParticleContext context, uint32_t max_particle_count)
    {
        context->m_MaxParticleCount = max_particle_count;
    }

    static Instance* GetInstance(HParticleContext context, HInstance instance)
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

    float Hermite(float x0, float x1, float t0, float t1, float t) {
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
            // break when we found the appropriate segemnt, or the last one
            if ((x >= p0.m_X && x < p1.m_X) || s == segment_count - 2) {
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

    static void InitEmitter(Emitter* emitter, dmParticleDDF::Emitter* emitter_ddf, uint32_t original_seed)
    {
        emitter->m_Id = dmHashString64(emitter_ddf->m_Id);
        uint32_t particle_count = emitter_ddf->m_MaxParticleCount;
        emitter->m_Particles.SetCapacity(particle_count);
        emitter->m_OriginalSeed = original_seed;

        uint32_t seed = original_seed;
        emitter->m_Duration = emitter_ddf->m_Duration + dmMath::Rand11(&seed) * emitter_ddf->m_DurationSpread;
        emitter->m_StartDelay = emitter_ddf->m_StartDelay + dmMath::Rand11(&seed) * emitter_ddf->m_StartDelaySpread;
        emitter->m_SpawnRateSpread = dmMath::Rand11(&seed) * ((dmParticleDDF::Emitter::Property&)emitter_ddf->m_Properties[EMITTER_KEY_SPAWN_RATE]).m_Spread;
    }

    static void ResetEmitter(Emitter* emitter);
    void UpdateEmitterRenderData(HInstance instance, uint32_t emitter_index, Instance* inst, Emitter* emitter, dmParticleDDF::Emitter* ddf);
    void ReHashEmitter(Emitter* e);

    HInstance CreateInstance(HParticleContext context, HPrototype prototype, EmitterStateChangedData* emitter_state_changed_data)
    {
        if (context->m_InstanceIndexPool.Remaining() == 0)
        {
            dmLogError("Instance could not be created since the buffer is full (%d). Tweak \"%s\" in the config file.", context->m_Instances.Capacity(), MAX_INSTANCE_COUNT_KEY);
            return 0;
        }
        dmParticleDDF::ParticleFX* ddf = prototype->m_DDF;
        uint32_t emitter_count = ddf->m_Emitters.m_Count;
        Instance* instance = new Instance;
        uint16_t index = context->m_InstanceIndexPool.Pop();

        // Avoid zero in order to ensure that HInstance != INVALID_INSTANCE for valid handles.
        if (context->m_NextVersionNumber == INVALID_INSTANCE) context->m_NextVersionNumber++;
        instance->m_VersionNumber = context->m_NextVersionNumber++;

        context->m_Instances[index] = instance;

        instance->m_Prototype = prototype;

        if(emitter_state_changed_data != 0x0 && emitter_state_changed_data->m_UserData != 0x0)
        {
            instance->m_EmitterStateChangedData = *emitter_state_changed_data;
        }

        instance->m_Emitters.SetCapacity(emitter_count);
        instance->m_Emitters.SetSize(emitter_count);
        uint32_t instance_handle = instance->m_VersionNumber << 16 | index;

        uint32_t seed_base = (uint32_t)dmTime::GetTime();
        memset(instance->m_Emitters.Begin(), 0, emitter_count * sizeof(Emitter));
        for (uint32_t i = 0; i < emitter_count; ++i)
        {
            Emitter* emitter = &instance->m_Emitters[i];
            uint32_t original_seed = seed_base + i;

            // Add the context seed (and increase it) to avoid
            // instances spawned the same frame to be identical.
            original_seed += context->m_InstanceSeeding++;

            InitEmitter(emitter, &ddf->m_Emitters[i], original_seed);
            emitter->m_Seed = original_seed;

            UpdateEmitterRenderData(instance_handle, i, instance, emitter, &ddf->m_Emitters[i]);
            ReHashEmitter(emitter);
        }

        return instance_handle;
    }

    void DestroyInstance(HParticleContext context, HInstance instance)
    {
        if (instance == INVALID_INSTANCE) return;
        Instance* i = GetInstance(context, instance);
        if (!i) return;
        ResetEmitterStateChangedData(i);
        uint32_t index = instance & 0xffff;
        context->m_InstanceIndexPool.Push(index);
        context->m_Instances[index] = 0;
        uint32_t emitter_count = i->m_Emitters.Size();
        for (uint32_t emitter_i = 0; emitter_i < emitter_count; ++emitter_i)
        {
            Emitter* emitter = &i->m_Emitters[emitter_i];
            emitter->m_Particles.SetCapacity(0);
            emitter->m_RenderConstants.SetCapacity(0);
        }
        delete i;
    }

    void SetEmitterState(Instance* instance, Emitter* emitter, EmitterState state)
    {
        EmitterState old_emitter_state = emitter->m_State;
        emitter->m_State = state;

        if(state != old_emitter_state && instance->m_EmitterStateChangedData.m_UserData != 0x0)
        {
            if(state == EMITTER_STATE_PRESPAWN)
            {
                instance->m_NumAwakeEmitters += 1;
            }
            else if(state == EMITTER_STATE_SLEEPING)
            {
                instance->m_NumAwakeEmitters -= 1;
            }

            instance->m_EmitterStateChangedData.m_StateChangedCallback(
                instance->m_NumAwakeEmitters,
                emitter->m_Id,
                emitter->m_State,
                instance->m_EmitterStateChangedData.m_UserData);
        }
    }

    static bool IsSleeping(Emitter* emitter);
    static void UpdateEmitter(Prototype* prototype, Instance* instance, EmitterPrototype* emitter_prototype, Emitter* emitter, dmParticleDDF::Emitter* emitter_ddf, float dt);

    static void StartEmitter(Instance* instance, Emitter* emitter)
    {
        // TODO: Fix auto-start
        SetEmitterState(instance, emitter, EMITTER_STATE_PRESPAWN);
        emitter->m_Retiring = 0;
    }

    static void StopEmitter(Instance* instance, Emitter* emitter)
    {
        if(emitter->m_State != EMITTER_STATE_SLEEPING)
            SetEmitterState(instance, emitter, EMITTER_STATE_POSTSPAWN);

        emitter->m_Retiring = 0;
    }

    static void RetireEmitter(Emitter* emitter)
    {
        emitter->m_Retiring = 1;
    }

    // Emitters are looping when play mode is looping, except for when retiring
    static bool IsEmitterLooping(Emitter* emitter, dmParticleDDF::Emitter* emitter_ddf)
    {
        return emitter->m_Retiring == 0 && emitter_ddf->m_Mode == PLAY_MODE_LOOP;
    }

    static void FastForwardEmitter(Prototype* prototype, Instance* instance, EmitterPrototype* emitter_prototype, Emitter* emitter, dmParticleDDF::Emitter* emitter_ddf, float time)
    {
        StartEmitter(instance, emitter);
        float timer = 0.0f;
        // Hard coded for now
        float dt = 1.0f / 60.0f;
        while (timer < time)
        {
            UpdateEmitter(prototype, instance, emitter_prototype, emitter, emitter_ddf, dt);
            timer += dt;
        }
    }

    static float CalculateReplayTime(float duration, float start_delay, float max_particle_life_time, float play_time)
    {
        float time = play_time;
        // In case play time is big we need to cut it down, but retain the position relative the duration
        if (play_time > duration + max_particle_life_time + start_delay)
        {
            float inv_duration = 1.0f / duration;
            float emitter_time = (play_time - start_delay) * inv_duration;
            float frac = emitter_time - (uint32_t)emitter_time;
            uint32_t iterations = 1 + (uint32_t)(max_particle_life_time * inv_duration);
            time = start_delay + duration * (iterations + frac);
        }
        return time;
    }

    void ReloadInstance(HParticleContext context, HInstance instance, bool replay)
    {
        if (instance == INVALID_INSTANCE) return;
        Instance* i = GetInstance(context, instance);
        if (!i) return;
        ResetEmitterStateChangedData(i);
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
            emitters.SetCapacity(prototype_emitter_count);
            emitters.SetSize(prototype_emitter_count);
            // memset new emitters if we have grown
            if (emitter_count < prototype_emitter_count)
            {
                memset(&emitters[emitter_count], 0, (prototype_emitter_count - emitter_count) * sizeof(Emitter));
                // Set seeds
                uint32_t seed_base = (uint32_t)dmTime::GetTime();
                for (uint32_t emitter_i = emitter_count; emitter_i < prototype_emitter_count; ++emitter_i)
                {
                    Emitter* emitter = &emitters[emitter_i];
                    uint32_t original_seed = seed_base + emitter_i + context->m_InstanceSeeding++;
                    InitEmitter(emitter, &ddf->m_Emitters[emitter_i], original_seed);
                    emitter->m_Seed = original_seed;
                }
            }
        }
        uint32_t old_emitter_count = dmMath::Min(emitter_count, prototype_emitter_count);
        for (uint32_t emitter_i = 0; emitter_i < old_emitter_count; ++emitter_i)
        {
            Emitter* emitter = &emitters[emitter_i];
            InitEmitter(emitter, &ddf->m_Emitters[emitter_i], emitter->m_OriginalSeed);
        }
        if (replay)
        {
            float max_play_time = 0.0f;
            // Reload id and replay emitters
            uint32_t count = i->m_Emitters.Size();
            for (uint32_t emitter_i = 0; emitter_i < count; ++emitter_i)
            {
                Emitter* emitter = &emitters[emitter_i];
                EmitterPrototype* emitter_prototype = &prototype->m_Emitters[emitter_i];
                float time = CalculateReplayTime(emitter->m_Duration, emitter->m_StartDelay, emitter_prototype->m_MaxParticleLifeTime, i->m_PlayTime);
                max_play_time = dmMath::Max(max_play_time, time);
            }
            i->m_PlayTime = max_play_time;
            for (uint32_t emitter_i = 0; emitter_i < count; ++emitter_i)
            {
                Emitter* emitter = &emitters[emitter_i];
                EmitterPrototype* emitter_prototype = &prototype->m_Emitters[emitter_i];
                dmParticleDDF::Emitter* emitter_ddf = &prototype->m_DDF->m_Emitters[emitter_i];
                ResetEmitter(emitter);
                FastForwardEmitter(prototype, i, emitter_prototype, emitter, emitter_ddf, i->m_PlayTime);
            }
        }

        ReHash(context, instance);
    }

    void StartInstance(HParticleContext context, HInstance instance)
    {
        if (instance == INVALID_INSTANCE) return;
        Instance* i = GetInstance(context, instance);
        if (!i) return;
        dmArray<Emitter>& emitters = i->m_Emitters;
        uint32_t emitter_count = emitters.Size();
        for (uint32_t emitter_i = 0; emitter_i < emitter_count; ++emitter_i)
        {
            StartEmitter(i, &emitters[emitter_i]);
        }
    }

    void StopInstance(HParticleContext context, HInstance instance)
    {
        if (instance == INVALID_INSTANCE) return;
        Instance* i = GetInstance(context, instance);
        if (!i) return;
        dmArray<Emitter>& emitters = i->m_Emitters;
        uint32_t emitter_count = emitters.Size();
        for (uint32_t emitter_i = 0; emitter_i < emitter_count; ++emitter_i)
        {
            Emitter* emitter = &emitters[emitter_i];
            StopEmitter(i, emitter);
        }
    }

    void RetireInstance(HParticleContext context, HInstance instance)
    {
        if (instance == INVALID_INSTANCE) return;
        Instance* i = GetInstance(context, instance);
        if (!i) return;
        dmArray<Emitter>& emitters = i->m_Emitters;
        uint32_t emitter_count = emitters.Size();
        for (uint32_t emitter_i = 0; emitter_i < emitter_count; ++emitter_i)
        {
            Emitter* emitter = &emitters[emitter_i];
            RetireEmitter(emitter);
        }
    }

    static void ResetEmitter(Emitter* emitter)
    {
        // Save particles array and id
        dmArray<Particle> tmp;
        tmp.Swap(emitter->m_Particles);
        dmhash_t id = emitter->m_Id;
        uint32_t original_seed = emitter->m_OriginalSeed;
        float duration = emitter->m_Duration;
        float start_delay = emitter->m_StartDelay;
        float spawn_rate_spread = emitter->m_SpawnRateSpread;

        // Clear emitter
        memset(emitter, 0, sizeof(Emitter));

        // Restore particles and id
        tmp.Swap(emitter->m_Particles);
        emitter->m_Id = id;

        // Remove living particles
        emitter->m_Particles.SetSize(0);

        // Restore values
        emitter->m_OriginalSeed = original_seed;
        emitter->m_Seed = original_seed;
        emitter->m_Duration = duration;
        emitter->m_StartDelay = start_delay;
        emitter->m_SpawnRateSpread = spawn_rate_spread;
    }

    void ResetInstance(HParticleContext context, HInstance instance)
    {
        if (instance == INVALID_INSTANCE) return;
        Instance* i = GetInstance(context, instance);
        if (!i) return;
        i->m_PlayTime = 0.0f;
        dmArray<Emitter>& emitters = i->m_Emitters;
        uint32_t emitter_count = emitters.Size();
        for (uint32_t emitter_i = 0; emitter_i < emitter_count; ++emitter_i)
        {
            ResetEmitter(&emitters[emitter_i]);
        }
    }

    void SetPosition(HParticleContext context, HInstance instance, const Point3& position)
    {
        Instance* i = GetInstance(context, instance);
        if (!i) return;
        i->m_WorldTransform.SetTranslation(Vector3(position));
    }

    void SetRotation(HParticleContext context, HInstance instance, const Quat& rotation)
    {
        Instance* i = GetInstance(context, instance);
        if (!i) return;
        i->m_WorldTransform.SetRotation(rotation);
    }

    void SetScale(HParticleContext context, HInstance instance, float scale)
    {
        Instance* i = GetInstance(context, instance);
        if (!i) return;
        i->m_WorldTransform.SetScale(scale);
    }

    void SetScaleAlongZ(HParticleContext context, HInstance instance, bool scale_along_z)
    {
        Instance* i = GetInstance(context, instance);
        if (!i) return;
        i->m_ScaleAlongZ = scale_along_z;
    }

    Vector3 GetPosition(HParticleContext context, HInstance instance)
    {
        Instance* i = GetInstance(context, instance);
        if (!i) return Vector3(0);
        return i->m_WorldTransform.GetTranslation();
    }

    static bool IsSleeping(Emitter* emitter)
    {
        return emitter->m_State == EMITTER_STATE_SLEEPING;
    }

    bool IsSleeping(Instance* instance)
    {
        // Consider 0x0 instances as sleeping
        if (!instance) return true;
        bool is_sleeping = true;
        dmArray<Emitter>& emitters = instance->m_Emitters;
        uint32_t emitter_count = emitters.Size();
        for (uint32_t emitter_i = 0; emitter_i < emitter_count; ++emitter_i)
        {
            if (!IsSleeping(&emitters[emitter_i]))
            {
                is_sleeping = false;
                break;
            }
        }
        return is_sleeping;
    }

    bool IsSleeping(HParticleContext context, HInstance instance)
    {
        return IsSleeping(GetInstance(context, instance));
    }

    // helper functions in update
    static void FetchAnimation(Emitter* emitter, EmitterPrototype* prototype, FetchAnimationCallback fetch_animation_callback);
    static void UpdateParticles(Instance* instance, Emitter* emitter, dmParticleDDF::Emitter* emitter_ddf, float dt);
    static void UpdateEmitterState(Instance* instance, Emitter* emitter, EmitterPrototype* emitter_prototype, dmParticleDDF::Emitter* emitter_ddf, float dt);
    static void EvaluateEmitterProperties(Emitter* emitter, Property* emitter_properties, float duration, float properties[EMITTER_KEY_COUNT]);
    static void EvaluateParticleProperties(Emitter* emitter, Property* particle_properties);
    static uint32_t UpdateRenderData(HParticleContext context, Instance* instance, Emitter* emitter, dmParticleDDF::Emitter* ddf, const Vector4& color, uint32_t vertex_index, void* vertex_buffer, uint32_t vertex_buffer_size, float dt, ParticleVertexFormat format);
    static void GenerateKeys(Emitter* emitter, float max_particle_life_time);
    static void SortParticles(Emitter* emitter);
    static void Simulate(Instance* instance, Emitter* emitter, EmitterPrototype* prototype, dmParticleDDF::Emitter* ddf, float dt);

    static void UpdateEmitter(Prototype* prototype, Instance* instance, EmitterPrototype* emitter_prototype, Emitter* emitter, dmParticleDDF::Emitter* emitter_ddf, float dt)
    {
        // Don't update emitter if time is standing still
        if (IsSleeping(emitter) || dt <= 0.0f)
            return;

        UpdateParticles(instance, emitter, emitter_ddf, dt);

        UpdateEmitterState(instance, emitter, emitter_prototype, emitter_ddf, dt);

        GenerateKeys(emitter, emitter_prototype->m_MaxParticleLifeTime);
        SortParticles(emitter);

        Simulate(instance, emitter, emitter_prototype, emitter_ddf, dt);
    }

    static void UpdateEmitterVelocity(Instance* instance, Emitter* emitter, dmParticleDDF::Emitter* emitter_ddf, float dt)
    {
        // Update emitter velocity (1-frame estimate)

        Point3 world_position = dmTransform::Apply(instance->m_WorldTransform, emitter_ddf->m_Position);
        if (emitter->m_LastPositionSet)
        {
            if (dt > 0.0f)
            {
                Vector3 diff = world_position - emitter->m_LastPosition;
                emitter->m_Velocity = diff * (1.0f/dt);
            }
        }
        else
        {
            emitter->m_LastPositionSet = 1;
        }
        emitter->m_LastPosition = world_position;
    }

    void GenerateVertexData(HParticleContext context, float dt, HInstance instance, uint32_t emitter_index, const Vector4& color, void* vertex_buffer, uint32_t vertex_buffer_size, uint32_t* out_vertex_buffer_size, ParticleVertexFormat vertex_format)
    {
        DM_PROFILE(Particle, "GenerateVertexData");
        if (instance == INVALID_INSTANCE)
            return;

        Instance* inst = GetInstance(context, instance);
        
        if (IsSleeping(inst))
            return;

        uint32_t vertex_size = sizeof(Vertex);

        if (vertex_format == PARTICLE_GUI)
        {
            vertex_size = sizeof(ParticleGuiVertex);
        }

        // vertex buffer index for each emitter
        uint32_t vertex_index = 0;

        vertex_index = *out_vertex_buffer_size / vertex_size;
        Prototype* prototype = inst->m_Prototype;
        Emitter* emitter = &inst->m_Emitters[emitter_index];
        dmParticleDDF::Emitter* emitter_ddf = &prototype->m_DDF->m_Emitters[emitter_index];
        if (vertex_buffer != 0x0 && vertex_buffer_size > 0)
        {
            vertex_index += UpdateRenderData(context, inst, emitter, emitter_ddf, color, vertex_index, vertex_buffer, vertex_buffer_size, dt, vertex_format);
        }

        *out_vertex_buffer_size = vertex_index * vertex_size;


        context->m_Stats.m_Particles = vertex_index / 6; // Debug data for editor playback
    }

    void Update(HParticleContext context, float dt, FetchAnimationCallback fetch_animation_callback)
    {
        DM_PROFILE(Particle, "Update");

        uint32_t size = context->m_Instances.Size();
        uint32_t TotalAliveParticles = 0;
        for (uint32_t i = 0; i < size; i++)
        {
            Instance* instance = context->m_Instances[i];

            // empty slot
            if (instance == INVALID_INSTANCE) continue;
            // don't update sleeping instances
            if (IsSleeping(instance))
            {
                // update velocity and clear vertex count (don't render)
                uint32_t emitter_count = instance->m_Emitters.Size();
                for (uint32_t emitter_i = 0; emitter_i < emitter_count; ++emitter_i)
                {
                    Emitter* emitter = &instance->m_Emitters[emitter_i];
                    emitter->m_VertexCount = 0;
                    dmParticleDDF::Emitter* emitter_ddf = &instance->m_Prototype->m_DDF->m_Emitters[emitter_i];
                    UpdateEmitterVelocity(instance, emitter, emitter_ddf, dt);
                }
                continue;
            }
            uint32_t instance_handle = instance->m_VersionNumber << 16 | i;
            instance->m_PlayTime += dt;
            Prototype* prototype = instance->m_Prototype;
            uint32_t emitter_count = instance->m_Emitters.Size();
            for (uint32_t emitter_i = 0; emitter_i < emitter_count; ++emitter_i)
            {
                Emitter* emitter = &instance->m_Emitters[emitter_i];
                EmitterPrototype* emitter_prototype = &prototype->m_Emitters[emitter_i];
                dmParticleDDF::Emitter* emitter_ddf = &prototype->m_DDF->m_Emitters[emitter_i];

                UpdateEmitterVelocity(instance, emitter, emitter_ddf, dt);
                UpdateEmitter(prototype, instance, emitter_prototype, emitter, emitter_ddf, dt);
                TotalAliveParticles += (uint32_t)emitter->m_Particles.Size();
                FetchAnimation(emitter, emitter_prototype, fetch_animation_callback);
                UpdateEmitterRenderData(instance_handle, emitter_i, instance, emitter, emitter_ddf);

                if (emitter->m_ReHash)
                    ReHashEmitter(emitter);
            }
        }

        DM_COUNTER("Particles alive", TotalAliveParticles);
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
                    dmLogWarning("The animation '%s' could not be found", dmHashReverseSafe64(prototype->m_Animation));
                }
            } else {
                assert(emitter->m_AnimationData.m_StructSize == sizeof(AnimationData) && "AnimationData::m_StructSize has an invalid size");
                emitter->m_FetchAnimWarning = 0;
            }
        }
    }

    static void UpdateParticles(Instance* instance, Emitter* emitter, dmParticleDDF::Emitter* emitter_ddf, float dt)
    {
        DM_PROFILE(Particle, "UpdateParticles");

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
    }

    static void SpawnParticle(dmArray<Particle>& particles, uint32_t* seed, dmParticleDDF::Emitter* ddf, const dmTransform::TransformS1& emitter_transform, Vector3 emitter_velocity, float emitter_properties[EMITTER_KEY_COUNT], float dt);

    static void UpdateEmitterState(Instance* instance, Emitter* emitter, EmitterPrototype* emitter_prototype, dmParticleDDF::Emitter* emitter_ddf, float dt)
    {
        DM_PROFILE(Particle, "UpdateEmitterState");

        if (emitter->m_State == EMITTER_STATE_PRESPAWN)
        {
            if (emitter->m_Timer >= emitter->m_StartDelay)
            {
                SetEmitterState(instance, emitter, EMITTER_STATE_SPAWNING);
                emitter->m_Timer -= emitter->m_StartDelay;
            }
        }
        // Step emitter life
        emitter->m_Timer += dt;
        if (emitter->m_State != EMITTER_STATE_PRESPAWN) {
            // never go above duration
            emitter->m_Timer = dmMath::Min(emitter->m_Timer, emitter->m_Duration);
        }
        if (emitter->m_State == EMITTER_STATE_SPAWNING)
        {
            // wrap looping emitters when they reach the end
            if (IsEmitterLooping(emitter, emitter_ddf) && emitter->m_Timer >= emitter->m_Duration)
            {
                emitter->m_Timer -= emitter->m_Duration;
            }

            // Evaluate spawn delay every frame while spawning (it might change)
            float original_emitter_properties[EMITTER_KEY_COUNT];
            float emitter_properties[EMITTER_KEY_COUNT];
            EvaluateEmitterProperties(emitter, emitter_prototype->m_Properties, emitter->m_Duration, original_emitter_properties);
            float spawn_rate = dmMath::Max(original_emitter_properties[EMITTER_KEY_SPAWN_RATE] + emitter->m_SpawnRateSpread, 0.0f);
            emitter->m_ParticlesToSpawn += spawn_rate * dt;

            uint32_t spawn_count = (uint32_t)emitter->m_ParticlesToSpawn;
            emitter->m_ParticlesToSpawn -= spawn_count;
            uint32_t count = dmMath::Min(emitter->m_Particles.Remaining(), spawn_count);
            dmTransform::TransformS1 emitter_transform(Vector3(emitter_ddf->m_Position), emitter_ddf->m_Rotation, 1.0f);
            Vector3 emitter_velocity(0.0f);
            if (emitter_ddf->m_Space == EMISSION_SPACE_WORLD)
            {
                if (instance->m_ScaleAlongZ)
                    emitter_transform = dmTransform::Mul(instance->m_WorldTransform, emitter_transform);
                else
                    emitter_transform = dmTransform::MulNoScaleZ(instance->m_WorldTransform, emitter_transform);
                emitter_velocity = emitter->m_Velocity * emitter_ddf->m_InheritVelocity;
            }
            for (uint32_t i = 0; i < count; ++i)
            {
                for (uint32_t i = 0; i < EMITTER_KEY_COUNT; ++i)
                {
                    // Apply spread per particle
                    float r = dmMath::Rand11(&emitter->m_Seed);
                    emitter_properties[i] = original_emitter_properties[i] + r * emitter_prototype->m_Properties[i].m_Spread;
                }
                SpawnParticle(emitter->m_Particles, &emitter->m_Seed, emitter_ddf, emitter_transform, emitter_velocity, emitter_properties, dt);
            }

            if (!IsEmitterLooping(emitter, emitter_ddf) && emitter->m_Timer >= emitter->m_Duration)
                StopEmitter(instance, emitter);
        }
        if (emitter->m_State == EMITTER_STATE_POSTSPAWN)
        {
            if (emitter->m_Particles.Empty())
                SetEmitterState(instance, emitter, EMITTER_STATE_SLEEPING);
        }
    }

    uint32_t GetEmitterVertexCount(HParticleContext context, HInstance instance, uint32_t emitter_index)
    {
        Instance* inst = GetInstance(context, instance);
        Emitter* emitter = &inst->m_Emitters[emitter_index];
        const uint32_t vertices_per_particle = 6;
        uint32_t particle_count = emitter->m_Particles.Size();

        return particle_count * vertices_per_particle;
    }

    static void SpawnParticle(dmArray<Particle>& particles, uint32_t* seed, dmParticleDDF::Emitter* ddf, const dmTransform::TransformS1& emitter_transform, Vector3 emitter_velocity, float emitter_properties[EMITTER_KEY_COUNT], float dt)
    {
        DM_PROFILE(Particle, "Spawn");

        uint32_t particle_count = particles.Size();
        particles.SetSize(particle_count + 1);
        Particle *particle = &particles[particle_count];
        memset(particle, 0, sizeof(Particle));

        // TODO Handle birth-action

        particle->SetMaxLifeTime(emitter_properties[EMITTER_KEY_PARTICLE_LIFE_TIME]);
        particle->SetooMaxLifeTime(1.0f / particle->GetMaxLifeTime());
        // Include dt since already existing particles have already been advanced
        particle->SetTimeLeft(particle->GetMaxLifeTime() - dt);
        particle->SetSpreadFactor(dmMath::Rand11(seed));
        particle->SetSourceSize(emitter_properties[EMITTER_KEY_PARTICLE_SIZE] * emitter_transform.GetScale());
        particle->SetSourceColor(Vector4(
                emitter_properties[EMITTER_KEY_PARTICLE_RED],
                emitter_properties[EMITTER_KEY_PARTICLE_GREEN],
                emitter_properties[EMITTER_KEY_PARTICLE_BLUE],
                emitter_properties[EMITTER_KEY_PARTICLE_ALPHA]));

        dmTransform::TransformS1 transform;
        transform.SetIdentity();
        Vector3 dir(0.0f, 0.0f, 0.0f);

        switch (ddf->m_Type)
        {
            case EMITTER_TYPE_SPHERE:
            {
                // Direction is sampled uniformly over the unit-sphere surface
                // http://www.altdevblogaday.com/2012/05/03/generating-uniformly-distributed-points-on-sphere/
                float z = dmMath::Rand11(seed);
                float angle = 2.0f * ((float) M_PI) * dmMath::RandOpen01(seed);
                float r = sqrtf(1.0f - z * z);
                dir = Vector3(r * cosf(angle), r * sinf(angle), z);
                // Pick radius to give uniform dist. over volume, surface area of sub-spheres grows quadratic wrt radius
                float radius = sqrtf(dmMath::RandOpen01(seed));
                radius *= 0.5f * emitter_properties[EMITTER_KEY_SIZE_X];
                transform.SetTranslation(dir * radius);

                break;
            }

            case EMITTER_TYPE_CIRCLE:
            {
                // Direction is sampled uniformly over the unit-sphere surface
                // http://www.altdevblogaday.com/2012/05/03/generating-uniformly-distributed-points-on-sphere/
                float angle = 2.0f * ((float) M_PI) * dmMath::RandOpen01(seed);
                dir = Vector3(cosf(angle), sinf(angle), 0);
                // Pick radius to give uniform dist. over volume, surface area of sub-spheres grows quadratic wrt radius
                float radius = sqrtf(dmMath::RandOpen01(seed));
                radius *= 0.5f * emitter_properties[EMITTER_KEY_SIZE_X];
                transform.SetTranslation(dir * radius);

                break;
            }

            case EMITTER_TYPE_CONE:
            {
                // Direction is sampled uniformly over the unit-circle (cone-top) surface
                // http://stackoverflow.com/questions/5837572/generate-a-random-point-within-a-circle-uniformly
                float angle = 2.0f * ((float) M_PI) * dmMath::RandOpen01(seed);
                float u = dmMath::Rand01(seed) + dmMath::Rand01(seed);
                float r = dmMath::Select(u - 1.0f, 2.0f - u, u);

                // Pick height to give uniform dist. over volume, surface area of sub-circles grows quadratic wrt height
                float h = sqrtf(dmMath::Rand01(seed));
                float height = h * emitter_properties[EMITTER_KEY_SIZE_Y];
                float radius = h * r * 0.5f * emitter_properties[EMITTER_KEY_SIZE_X];

                Vector3 local_position(radius * cosf(angle), height, radius * sinf(angle));
                transform.SetTranslation(local_position);

                // Finally normalize dir
                if (lengthSqr(local_position) != 0.0f)
                    dir = normalize(local_position);
                else
                    dir = Vector3(0.0f, 1.0f, 0.0f);

                break;
            }

            case EMITTER_TYPE_2DCONE:
            {
                float width = emitter_properties[EMITTER_KEY_SIZE_X];
                float height = emitter_properties[EMITTER_KEY_SIZE_Y];

                float u = dmMath::Rand01(seed);
                float v = dmMath::Rand01(seed);

                /*
                 * Create samples in a parallelogram
                 * See http://mathworld.wolfram.com/TrianglePointPicking.html
                 * for more information
                 * Effectively we get two triangles with total height 2 * height
                 */

                /*
                 * The triangle is comprised of the vertices (0,0), v1 and v2
                 *  p = (x, y) = v1 * u + v2 * v
                 *
                 *  p is a point in the parallelogram
                 */
                float x = -width * 0.5f * u + width * 0.5f * v;
                float y = height * u + height * v;
                // Mirror points outside triangle
                y = dmMath::Select(height - y, y, 2 * height - y);

                Vector3 local_position(x, y, 0.0f);
                transform.SetTranslation(local_position);
                if (lengthSqr(local_position) != 0.0f)
                    dir = normalize(local_position);
                else
                    dir = Vector3(0.0f, 1.0f, 0.0f);

                break;
            }

            case EMITTER_TYPE_BOX:
            {
                Vector3 p(dmMath::Rand11(seed), dmMath::Rand11(seed), dmMath::Rand11(seed));
                while (lengthSqr(p) == 0.0f)
                    p = Vector3(dmMath::Rand11(seed), dmMath::Rand11(seed), dmMath::Rand11(seed));
                dir = Vector3::yAxis();

                Vector3 extent(0.5f * emitter_properties[EMITTER_KEY_SIZE_X],
                        0.5f * emitter_properties[EMITTER_KEY_SIZE_Y],
                        0.5f * emitter_properties[EMITTER_KEY_SIZE_Z]);
                transform.SetTranslation(mulPerElem(p, extent));

                break;
            }

            default:
                dmLogWarning("Unknown emitter type (%d), particle is spawned at emitter.", ddf->m_Type);
                transform.SetTranslation(Vector3(0.0f, 0.0f, 0.0f));
                break;
        }

        Vector3 velocity = dir * emitter_properties[EMITTER_KEY_PARTICLE_SPEED];
        Quat rotation;
        switch (ddf->m_ParticleOrientation)
        {
        case PARTICLE_ORIENTATION_DEFAULT:
        case PARTICLE_ORIENTATION_MOVEMENT_DIRECTION:
            // rotation is already identity
            // or will be defined in simulation
            break;
        case PARTICLE_ORIENTATION_INITIAL_DIRECTION:
            transform.SetRotation(Quat::rotation(Vector3::yAxis(), dir));
            break;
        }

        transform = dmTransform::Mul(emitter_transform, transform);
        particle->SetPosition(Point3(transform.GetTranslation()));
        if (ddf->m_ParticleOrientation == PARTICLE_ORIENTATION_MOVEMENT_DIRECTION)
            particle->SetSourceRotation(dmVMath::QuatFromAngle(2, DEG_RAD * emitter_properties[EMITTER_KEY_PARTICLE_ROTATION]));
        else
            particle->SetSourceRotation(transform.GetRotation() * dmVMath::QuatFromAngle(2, DEG_RAD * emitter_properties[EMITTER_KEY_PARTICLE_ROTATION]));
        particle->SetRotation(particle->GetSourceRotation());
        particle->SetVelocity(dmTransform::Apply(emitter_transform, velocity) + emitter_velocity);
        particle->m_SourceStretchFactorX = emitter_properties[EMITTER_KEY_PARTICLE_STRETCH_FACTOR_X];
        particle->m_StretchFactorX = particle->m_SourceStretchFactorX;
        particle->m_SourceStretchFactorY = emitter_properties[EMITTER_KEY_PARTICLE_STRETCH_FACTOR_Y];
        particle->m_StretchFactorY = particle->m_SourceStretchFactorY;
    }

    static float unit_tex_coords[] =
    {
            0.0f,1.0f, 0.0f,0.0f, 1.0f,0.0f, 1.0f,1.0f
    };

    static uint32_t UpdateRenderData(HParticleContext context, Instance* instance, Emitter* emitter, dmParticleDDF::Emitter* ddf, const Vector4& color, uint32_t vertex_index, void* vertex_buffer, uint32_t vertex_buffer_size, float dt, ParticleVertexFormat format)
    {
        DM_PROFILE(Particle, "UpdateRenderData");
        static int tex_coord_order[] = {
            0,1,2,2,3,0,
            3,2,1,1,0,3,	//h
            1,0,3,3,2,1,	//v
            2,3,0,0,1,2		//hv
        };

        uint32_t vertex_size = sizeof(Vertex);

        if (format == PARTICLE_GUI)
            vertex_size = sizeof(ParticleGuiVertex);

        emitter->m_VertexIndex = vertex_index;
        emitter->m_VertexCount = 0;

        const AnimationData& anim_data = emitter->m_AnimationData;
        // texture animation
        uint32_t start_tile = anim_data.m_StartTile;
        uint32_t end_tile = anim_data.m_EndTile;
        uint32_t interval = end_tile - start_tile;
        uint32_t tile_count = interval;
        AnimPlayback playback = anim_data.m_Playback;
        float* tex_coords = anim_data.m_TexCoords;
        float* tex_dims = anim_data.m_TexDims;
        bool hFlip = anim_data.m_HFlip != 0;
        bool vFlip = anim_data.m_VFlip != 0;
        bool anim_playing = playback != ANIM_PLAYBACK_NONE && tile_count > 1;
        bool anim_auto_size = (ddf->m_SizeMode == SIZE_MODE_AUTO) && (anim_data.m_TexDims != 0x0) && anim_playing;
        bool anim_once = playback == ANIM_PLAYBACK_ONCE_FORWARD || playback == ANIM_PLAYBACK_ONCE_BACKWARD || playback == ANIM_PLAYBACK_ONCE_PINGPONG;
        bool anim_bwd = playback == ANIM_PLAYBACK_ONCE_BACKWARD || playback == ANIM_PLAYBACK_LOOP_BACKWARD;
        bool anim_ping_pong = playback == ANIM_PLAYBACK_ONCE_PINGPONG || playback == ANIM_PLAYBACK_LOOP_PINGPONG;
        if (anim_ping_pong) {
            tile_count = dmMath::Max(1u, tile_count * 2 - 2);
        }
        float inv_anim_length = anim_data.m_FPS / (float)tile_count;
        // Used to sample anim tiles in the "frame center"
        float half_dt = dt * 0.5f;

        if (tex_coords == 0x0)
        {
            tex_coords = unit_tex_coords;
            start_tile = 0;
            end_tile = 1;
            tile_count = 1;
        }

        // calculate emission space
        dmTransform::TransformS1 emission_transform;
        dmTransform::Transform particle_transform;
        emission_transform.SetIdentity();
        if (ddf->m_Space == EMISSION_SPACE_EMITTER)
        {
            emission_transform = instance->m_WorldTransform;
        }

        uint32_t max_vertex_count = vertex_buffer_size / vertex_size;
        uint32_t particle_count = emitter->m_Particles.Size();
        uint32_t j;

        float width_factor = 1.0f;
        float height_factor = 1.0f;
        if(!anim_auto_size)
        {
            if (anim_data.m_TileWidth > anim_data.m_TileHeight)
            {
                height_factor = anim_data.m_TileHeight / (float)anim_data.m_TileWidth;
            }
            else if (anim_data.m_TileHeight > 0)
            {
                width_factor = anim_data.m_TileWidth / (float)anim_data.m_TileHeight;
            }
            // Extent for each vertex, scale by half
            width_factor *= 0.5f;
            height_factor *= 0.5f;
        }

        for (j = 0; j < particle_count && vertex_index + 6 <= max_vertex_count; j++)
        {
            Particle* particle = &emitter->m_Particles[j];
            // Evaluate anim frame
            uint32_t tile = 0;
            Vector3 size;
            if (anim_playing)
            {
                float anim_cursor = particle->GetMaxLifeTime() - particle->GetTimeLeft() - half_dt;
                float anim_t = 0.0f;
                if (anim_once) // stretch over particle life
                {
                    anim_t = anim_cursor * particle->GetooMaxLifeTime();
                }
                else // use anim FPS
                {
                    anim_t = anim_cursor * inv_anim_length;
                }
                tile = (uint32_t)(tile_count * anim_t);
                tile = tile % tile_count;
                if (tile >= interval) {
                    tile = (interval-1) * 2 - tile;
                }
                if (anim_bwd)
                    tile = tile_count - tile - 1;

                size = particle->GetScale();
                if(anim_auto_size)
                {
                    const float* td = &tex_dims[(start_tile + tile) << 1];
                    width_factor = td[0] * 0.5;
                    height_factor = td[1] * 0.5;
                }
                else
                {
                    size *= particle->GetSourceSize();
                }
            }
            else
            {
                size = particle->GetScale() * particle->GetSourceSize();
            }
            tile += start_tile;
            float* tex_coord = &tex_coords[tile << 3];

            particle_transform.SetTranslation(Vector3(particle->GetPosition()));
            particle_transform.SetRotation(particle->GetRotation());
            particle_transform.SetScale(size);
            particle_transform.SetRotation(emission_transform.GetRotation() * particle_transform.GetRotation());
            particle_transform.SetTranslation(Vector3(Apply(emission_transform, Point3(particle_transform.GetTranslation()))));
            particle_transform.SetScale(emission_transform.GetScale() * particle_transform.GetScale());

            Vector3 x = dmTransform::Apply(particle_transform, Vector3(width_factor, 0.0f, 0.0f));
            Vector3 y = dmTransform::Apply(particle_transform, Vector3(0.0f, height_factor, 0.0f));

            Vector3 p0 = -x - y + particle_transform.GetTranslation();
            Vector3 p1 = -x + y + particle_transform.GetTranslation();
            Vector3 p2 = x - y + particle_transform.GetTranslation();
            Vector3 p3 = x + y + particle_transform.GetTranslation();

            uint32_t flip_flag = 0;
            if (hFlip)
            {
                flip_flag = 1;
            }
            if (vFlip)
            {
                flip_flag |= 2;
            }
            const int* tex_lookup = &tex_coord_order[flip_flag * 6];

            Vector4 c = particle->GetColor();
            c = Vector4(mulPerElem(c.getXYZ(), color.getXYZ()), c.getW() * color.getW());

            if (format == PARTICLE_GO)
            {
                Vertex* vertex = &((Vertex*)vertex_buffer)[vertex_index];

#define TO_BYTE(val) (uint8_t)(val * 255.0f)
#define TO_SHORT(val) (uint16_t)(val * 65535.0f)

#define SET_VERTEX_GO(vertex, p, c, u, v)\
    vertex->m_X = p.getX();\
    vertex->m_Y = p.getY();\
    vertex->m_Z = p.getZ();\
    vertex->m_Red = TO_BYTE(c.getX());\
    vertex->m_Green = TO_BYTE(c.getY());\
    vertex->m_Blue = TO_BYTE(c.getZ());\
    vertex->m_Alpha = TO_BYTE(c.getW());\
    vertex->m_U = TO_SHORT(u);\
    vertex->m_V = TO_SHORT(v);

                SET_VERTEX_GO(vertex, p0, c, tex_coord[tex_lookup[0] * 2], tex_coord[tex_lookup[0] * 2 + 1])
                ++vertex;
                SET_VERTEX_GO(vertex, p1, c, tex_coord[tex_lookup[1] * 2], tex_coord[tex_lookup[1] * 2 + 1])
                ++vertex;
                SET_VERTEX_GO(vertex, p3, c, tex_coord[tex_lookup[2] * 2], tex_coord[tex_lookup[2] * 2 + 1])
                ++vertex;
                SET_VERTEX_GO(vertex, p3, c, tex_coord[tex_lookup[3] * 2], tex_coord[tex_lookup[3] * 2 + 1])
                ++vertex;
                SET_VERTEX_GO(vertex, p2, c, tex_coord[tex_lookup[4] * 2], tex_coord[tex_lookup[4] * 2 + 1])
                ++vertex;
                SET_VERTEX_GO(vertex, p0, c, tex_coord[tex_lookup[5] * 2], tex_coord[tex_lookup[5] * 2 + 1])

#undef TO_BYTE
#undef SET_VERTEX_GO
            }
            else if (format == PARTICLE_GUI)
            {
                ParticleGuiVertex* vertex = &((ParticleGuiVertex*)vertex_buffer)[vertex_index];

#define SET_VERTEX_GUI(vertex, p, c, u, v)\
    vertex->m_Position[0] = p.getX();\
    vertex->m_Position[1] = p.getY();\
    vertex->m_Position[2] = p.getZ();\
    vertex->m_Color = (((uint32_t) (c.getW() * 255.0f)) << 24) | (((uint32_t) (c.getZ() * 255.0f)) << 16) | (((uint32_t) (c.getY() * 255.0f)) << 8) | ((uint32_t) (c.getX() * 255.0f));\
    vertex->m_UV[0] = u;\
    vertex->m_UV[1] = v;

                SET_VERTEX_GUI(vertex, p0, c, tex_coord[tex_lookup[0] * 2], tex_coord[tex_lookup[0] * 2 + 1])
                ++vertex;
                SET_VERTEX_GUI(vertex, p1, c, tex_coord[tex_lookup[1] * 2], tex_coord[tex_lookup[1] * 2 + 1])
                ++vertex;
                SET_VERTEX_GUI(vertex, p3, c, tex_coord[tex_lookup[2] * 2], tex_coord[tex_lookup[2] * 2 + 1])
                ++vertex;
                SET_VERTEX_GUI(vertex, p3, c, tex_coord[tex_lookup[3] * 2], tex_coord[tex_lookup[3] * 2 + 1])
                ++vertex;
                SET_VERTEX_GUI(vertex, p2, c, tex_coord[tex_lookup[4] * 2], tex_coord[tex_lookup[4] * 2 + 1])
                ++vertex;
                SET_VERTEX_GUI(vertex, p0, c, tex_coord[tex_lookup[5] * 2], tex_coord[tex_lookup[5] * 2 + 1])
#undef SET_VERTEX_GUI
            }

            vertex_index += 6;
        }
        if (j < particle_count)
        {
            if (emitter->m_RenderWarning == 0)
            {
                const char* config_key = MAX_PARTICLE_COUNT_KEY;

                if (format == PARTICLE_GUI)
                    config_key = "gui.max_particle_count";

                dmLogWarning("Maximum number of particles (%d) exceeded, particles will not be rendered. Change \"%s\" in the config file.", context->m_MaxParticleCount, config_key);
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

    void GenerateKeys(Emitter* emitter, float max_particle_life_time)
    {
        dmArray<Particle>& particles = emitter->m_Particles;
        uint32_t n = particles.Size();

        float range = 1.0f / max_particle_life_time;

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
        const LinearSegment* s = &segment;\
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
        // TODO Optimize this
        dmArray<Particle>& particles = emitter->m_Particles;
        uint32_t count = particles.Size();
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
            SAMPLE_PROP(particle_properties[PARTICLE_KEY_ROTATION].m_Segments[segment_index], x, properties[PARTICLE_KEY_ROTATION])
            SAMPLE_PROP(particle_properties[PARTICLE_KEY_STRETCH_FACTOR_X].m_Segments[segment_index], x, properties[PARTICLE_KEY_STRETCH_FACTOR_X])
            SAMPLE_PROP(particle_properties[PARTICLE_KEY_STRETCH_FACTOR_Y].m_Segments[segment_index], x, properties[PARTICLE_KEY_STRETCH_FACTOR_Y])

            Vector4 c = particle->GetSourceColor();
            particle->SetScale(Vector3(properties[PARTICLE_KEY_SCALE]));
            particle->SetColor(Vector4(dmMath::Clamp(c.getX() * properties[PARTICLE_KEY_RED], 0.0f, 1.0f),
                    dmMath::Clamp(c.getY() * properties[PARTICLE_KEY_GREEN], 0.0f, 1.0f),
                    dmMath::Clamp(c.getZ() * properties[PARTICLE_KEY_BLUE], 0.0f, 1.0f),
                    dmMath::Clamp(c.getW() * properties[PARTICLE_KEY_ALPHA], 0.0f, 1.0f)));
            particle->SetRotation(particle->GetSourceRotation() * dmVMath::QuatFromAngle(2, DEG_RAD * properties[PARTICLE_KEY_ROTATION]));
            particle->m_StretchFactorX = particle->m_SourceStretchFactorX + (properties[PARTICLE_KEY_STRETCH_FACTOR_X]);
            particle->m_StretchFactorY = particle->m_SourceStretchFactorY + (properties[PARTICLE_KEY_STRETCH_FACTOR_Y]);
        }
    }

    void ApplyAcceleration(dmArray<Particle>& particles, Property* modifier_properties, const Quat& rotation, float scale, float emitter_t, float dt)
    {
        uint32_t particle_count = particles.Size();
        Vector3 acc_step = rotate(rotation, ACCELERATION_LOCAL_DIR) * dt * scale;
        const Property& magnitude_property = modifier_properties[MODIFIER_KEY_MAGNITUDE];
        uint32_t segment_index = dmMath::Min((uint32_t)(emitter_t * PROPERTY_SAMPLE_COUNT), PROPERTY_SAMPLE_COUNT - 1);
        float magnitude;
        SAMPLE_PROP(magnitude_property.m_Segments[segment_index], emitter_t, magnitude)
        float mag_spread = magnitude_property.m_Spread;
        for (uint32_t i = 0; i < particle_count; ++i)
        {
            Particle* particle = &particles[i];
            particle->SetVelocity(particle->GetVelocity() + acc_step * (magnitude + mag_spread * particle->GetSpreadFactor()));
        }
    }

    void ApplyDrag(dmArray<Particle>& particles, Property* modifier_properties, dmParticleDDF::Modifier* modifier_ddf, const Quat& rotation, float emitter_t, float dt)
    {
        uint32_t particle_count = particles.Size();
        Vector3 direction = rotate(rotation, DRAG_LOCAL_DIR);
        const Property& magnitude_property = modifier_properties[MODIFIER_KEY_MAGNITUDE];
        uint32_t segment_index = dmMath::Min((uint32_t)(emitter_t * PROPERTY_SAMPLE_COUNT), PROPERTY_SAMPLE_COUNT - 1);
        float magnitude;
        SAMPLE_PROP(magnitude_property.m_Segments[segment_index], emitter_t, magnitude)
        float mag_spread = magnitude_property.m_Spread;
        for (uint32_t i = 0; i < particle_count; ++i)
        {
            Particle* particle = &particles[i];
            Vector3 v = particle->GetVelocity();
            if (modifier_ddf->m_UseDirection)
                v = projection(Point3(particle->GetVelocity()), direction) * direction;
            // Applied drag > 1 means the particle would travel in the reverse direction
            float applied_drag = dmMath::Min((magnitude + mag_spread * particle->GetSpreadFactor()) * dt, 1.0f);
            particle->SetVelocity(particle->GetVelocity() - v * applied_drag);
        }
    }

    static Vector3 GetParticleDir(Particle* particle)
    {
        return rotate(particle->GetRotation(), PARTICLE_LOCAL_BASE_DIR);
    }

    static Vector3 NonZeroVector3(Vector3 v, float sq_length, Vector3 fallback)
    {
        Vector3 result;
        float neg_sq_length = -sq_length;
        result.setX(dmMath::Select(neg_sq_length, fallback.getX(), v.getX()));
        result.setY(dmMath::Select(neg_sq_length, fallback.getY(), v.getY()));
        result.setZ(dmMath::Select(neg_sq_length, fallback.getZ(), v.getZ()));
        return result;
    }

    void ApplyRadial(dmArray<Particle>& particles, Property* modifier_properties, const Point3& position, float scale, float emitter_t, float dt)
    {
        uint32_t particle_count = particles.Size();
        const Property& magnitude_property = modifier_properties[MODIFIER_KEY_MAGNITUDE];
        const Property& max_distance_property = modifier_properties[MODIFIER_KEY_MAX_DISTANCE];
        uint32_t segment_index = dmMath::Min((uint32_t)(emitter_t * PROPERTY_SAMPLE_COUNT), PROPERTY_SAMPLE_COUNT - 1);
        float magnitude;
        SAMPLE_PROP(magnitude_property.m_Segments[segment_index], emitter_t, magnitude)
        float mag_spread = magnitude_property.m_Spread;
        // We temporarily only sample the first frame until we have decided what to animate over
        float max_distance = max_distance_property.m_Segments[0].m_Y * scale;
        float max_sq_distance = max_distance * max_distance;
        float applied_factor = dt * scale;
        for (uint32_t i = 0; i < particle_count; ++i)
        {
            Particle* particle = &particles[i];
            Vector3 delta = particle->GetPosition() - position;
            float delta_sq_len = lengthSqr(delta);
            float applied_magnitude = magnitude + mag_spread * particle->GetSpreadFactor();
            // 0 acc delta lies outside max dist
            float a = dmMath::Select(max_sq_distance - delta_sq_len, applied_magnitude, 0.0f);
            Vector3 dir = normalize(NonZeroVector3(delta, delta_sq_len, GetParticleDir(particle)));
            particle->SetVelocity(particle->GetVelocity() + dir * a * applied_factor);
        }
    }

    void ApplyVortex(dmArray<Particle>& particles, Property* modifier_properties, const Point3& position, const Quat& rotation, float scale, float emitter_t, float dt)
    {
        uint32_t particle_count = particles.Size();
        const Property& magnitude_property = modifier_properties[MODIFIER_KEY_MAGNITUDE];
        const Property& max_distance_property = modifier_properties[MODIFIER_KEY_MAX_DISTANCE];
        uint32_t segment_index = dmMath::Min((uint32_t)(emitter_t * PROPERTY_SAMPLE_COUNT), PROPERTY_SAMPLE_COUNT - 1);
        float magnitude;
        SAMPLE_PROP(magnitude_property.m_Segments[segment_index], emitter_t, magnitude)
        float mag_spread = magnitude_property.m_Spread;
        // We temporarily only sample the first frame until we have decided what to animate over
        float max_distance = max_distance_property.m_Segments[0].m_Y * scale;
        float max_sq_distance = max_distance * max_distance;
        Vector3 axis = rotate(rotation, VORTEX_LOCAL_AXIS);
        Vector3 start = rotate(rotation, VORTEX_LOCAL_START_DIR);
        float applied_factor = dt * scale;
        for (uint32_t i = 0; i < particle_count; ++i)
        {
            Particle* particle = &particles[i];
            // delta from vortex position
            Vector3 delta = particle->GetPosition() - position;
            // normal from vortex axis (non-unit)
            Vector3 normal = delta - projection(Point3(delta), axis) * axis;
            // tangent is the direction of the vortex acceleration
            Vector3 tangent = cross(axis, normal);
            // In case the particle is directed along the axis, give it a guaranteed orthogonal start
            tangent = NonZeroVector3(tangent, lengthSqr(tangent), start);
            // tangent is now guaranteed to be non-zero
            tangent = normalize(tangent);
            // use normal for max distance test
            float normal_sq_len = lengthSqr(normal);
            float acceleration = dmMath::Select(max_sq_distance - normal_sq_len, magnitude + mag_spread * particle->GetSpreadFactor(), 0.0f);
            particle->SetVelocity(particle->GetVelocity() + tangent * acceleration * applied_factor);
        }
    }

#undef SAMPLE_PROP

    static Point3 CalculateModifierPosition(Instance* instance, dmParticleDDF::Emitter* emitter_ddf, dmParticleDDF::Modifier* modifier_ddf)
    {
        Point3 position(modifier_ddf->m_Position);
        position = emitter_ddf->m_Position + rotate(emitter_ddf->m_Rotation, Vector3(position));
        if (emitter_ddf->m_Space == EMISSION_SPACE_WORLD)
        {
            if (instance->m_ScaleAlongZ)
                position = dmTransform::Apply(instance->m_WorldTransform, position);
            else
                position = dmTransform::ApplyNoScaleZ(instance->m_WorldTransform, position);
        }
        return Point3(position);
    }

    static Quat CalculateModifierRotation(Instance* instance, dmParticleDDF::Emitter* emitter_ddf, dmParticleDDF::Modifier* modifier_ddf)
    {
        return emitter_ddf->m_Rotation * modifier_ddf->m_Rotation;
    }

    void Simulate(Instance* instance, Emitter* emitter, EmitterPrototype* prototype, dmParticleDDF::Emitter* ddf, float dt)
    {
        DM_PROFILE(Particle, "Simulate");

        dmArray<Particle>& particles = emitter->m_Particles;
        EvaluateParticleProperties(emitter, prototype->m_ParticleProperties);
        float emitter_t = dmMath::Select(-ddf->m_Duration, 0.0f, emitter->m_Timer / ddf->m_Duration);
        // Apply modifiers
        uint32_t modifier_count = prototype->m_Modifiers.Size();
        for (uint32_t i = 0; i < modifier_count; ++i)
        {
            ModifierPrototype* modifier = &prototype->m_Modifiers[i];
            dmParticleDDF::Modifier* modifier_ddf = &ddf->m_Modifiers[i];
            switch (modifier_ddf->m_Type)
            {
            case dmParticleDDF::MODIFIER_TYPE_ACCELERATION:
                {
                    Quat rotation = CalculateModifierRotation(instance, ddf, modifier_ddf);
                    ApplyAcceleration(particles, modifier->m_Properties, rotation, instance->m_WorldTransform.GetScale(), emitter_t, dt);
                }
                break;
            case dmParticleDDF::MODIFIER_TYPE_DRAG:
                {
                    Quat rotation = CalculateModifierRotation(instance, ddf, modifier_ddf);
                    ApplyDrag(particles, modifier->m_Properties, modifier_ddf, rotation, emitter_t, dt);
                }
                break;
            case dmParticleDDF::MODIFIER_TYPE_RADIAL:
                {
                    Point3 position = CalculateModifierPosition(instance, ddf, modifier_ddf);
                    ApplyRadial(particles, modifier->m_Properties, position, instance->m_WorldTransform.GetScale(), emitter_t, dt);
                }
                break;
            case dmParticleDDF::MODIFIER_TYPE_VORTEX:
                {
                    Point3 position = CalculateModifierPosition(instance, ddf, modifier_ddf);
                    Quat rotation = CalculateModifierRotation(instance, ddf, modifier_ddf);
                    ApplyVortex(particles, modifier->m_Properties, position, rotation, instance->m_WorldTransform.GetScale(), emitter_t, dt);
                }
                break;
            }
        }
        uint32_t particle_count = particles.Size();
        for (uint32_t i = 0; i < particle_count; ++i)
        {
            Particle* p = &particles[i];
            // NOTE This velocity integration has a larger error than normal since we don't use the velocity at the
            // beginning of the frame, but it's ok since particle movement does not need to be very exact
            p->SetPosition(p->GetPosition() + p->m_Velocity * dt);

            p->m_Scale[0] += p->m_Scale[0] * p->m_StretchFactorX;
            if (!ddf->m_StretchWithVelocity)
                p->m_Scale[1] += p->m_Scale[1] * p->m_StretchFactorY;
            else
                p->m_Scale[1] += p->m_Scale[1] * p->m_StretchFactorY * length(p->m_Velocity) * STRETCH_SCALING;
        }

        if (ddf->m_ParticleOrientation == PARTICLE_ORIENTATION_MOVEMENT_DIRECTION)
        {
            for (uint32_t i = 0; i < particle_count; ++i)
            {
                Particle* p = &particles[i];
                if (lengthSqr(p->m_Velocity) > EPSILON)
                {
                    Vector3 vel_norm = normalize(p->m_Velocity);
                    float y_dot = dot(Vector3::yAxis(), vel_norm);
                    // Corner case, https://gamedev.stackexchange.com/questions/61672/align-a-rotation-to-a-direction
                    Quat q_vel = (dmMath::Abs(y_dot + 1.0f) > EPSILON) ? Quat::rotation(Vector3::yAxis(), vel_norm) : Quat(0.0, 0.0, 1.0, 0.0);
                    Quat q = p->GetRotation() * q_vel;
                    p->SetRotation(q);
                }
            }
        }
    }

    void DebugRender(HParticleContext context, void* user_context, RenderLineCallback render_line_callback)
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
                if (IsSleeping(e))
                {
                    color.setY(0.0f);
                    color.setZ(1.0f);
                }
                else if (!IsEmitterLooping(e, ddf))
                {
                    float t = dmMath::Select(-ddf->m_Duration, 0.0f, e->m_Timer / ddf->m_Duration);
                    color.setY(1.0f - t);
                    color.setZ(t);
                }
                dmTransform::TransformS1 transform(Vector3(ddf->m_Position), ddf->m_Rotation, 1.0f);
                if (instance->m_ScaleAlongZ)
                    transform = dmTransform::Mul(instance->m_WorldTransform, transform);
                else
                    transform = dmTransform::MulNoScaleZ(instance->m_WorldTransform, transform);

                switch (ddf->m_Type)
                {
                case EMITTER_TYPE_SPHERE:
                {
                    const float radius = 0.5f * ddf->m_Properties[EMITTER_KEY_SIZE_X].m_Points[0].m_Y;

                    const uint32_t segment_count = 16;
                    Point3 vertices[segment_count + 1][3];
                    for (uint32_t j = 0; j < segment_count + 1; ++j)
                    {
                        float angle = 2.0f * ((float) M_PI) * j / segment_count;
                        vertices[j][0] = Point3(radius * cos(angle), radius * sin(angle), 0.0f);
                        vertices[j][1] = Point3(0.0f, radius * cos(angle), radius * sin(angle));
                        vertices[j][2] = Point3(radius * cos(angle), 0.0f, radius * sin(angle));
                    }
                    for (uint32_t j = 1; j < segment_count + 1; ++j)
                    {
                        for (uint32_t k = 0; k < 3; ++k)
                            render_line_callback(user_context, dmTransform::Apply(transform, vertices[j-1][k]), dmTransform::Apply(transform, vertices[j][k]), color);
                    }
                    break;
                }
                case EMITTER_TYPE_CONE:
                {
                    const float radius = 0.5f * ddf->m_Properties[EMITTER_KEY_SIZE_X].m_Points[0].m_Y;
                    const float height = ddf->m_Properties[EMITTER_KEY_SIZE_Y].m_Points[0].m_Y;

                    // 4 pillars
                    render_line_callback(user_context, Point3(transform.GetTranslation()), dmTransform::Apply(transform, Point3(radius, 0.0f, height)), color);
                    render_line_callback(user_context, Point3(transform.GetTranslation()), dmTransform::Apply(transform, Point3(-radius, 0.0f, height)), color);
                    render_line_callback(user_context, Point3(transform.GetTranslation()), dmTransform::Apply(transform, Point3(0.0f, radius, height)), color);
                    render_line_callback(user_context, Point3(transform.GetTranslation()), dmTransform::Apply(transform, Point3(0.0f, -radius, height)), color);
                    // circle
                    const uint32_t segment_count = 16;
                    Point3 vertices[segment_count];
                    for (uint32_t j = 0; j < segment_count; ++j)
                    {
                        float angle = 2.0f * ((float) M_PI) * j / segment_count;
                        vertices[j] = Point3(radius * cos(angle), radius * sin(angle), height);
                    }
                    for (uint32_t j = 1; j < segment_count; ++j)
                    {
                        render_line_callback(user_context, dmTransform::Apply(transform, vertices[j-1]), dmTransform::Apply(transform, vertices[j]), color);
                    }
                    render_line_callback(user_context, dmTransform::Apply(transform, vertices[segment_count - 1]), dmTransform::Apply(transform, vertices[0]), color);
                    break;
                }
                case EMITTER_TYPE_BOX:
                {
                    const float x_ext = 0.5f * ddf->m_Properties[EMITTER_KEY_SIZE_X].m_Points[0].m_Y;
                    const float y_ext = 0.5f * ddf->m_Properties[EMITTER_KEY_SIZE_Y].m_Points[0].m_Y;
                    const float z_ext = 0.5f * ddf->m_Properties[EMITTER_KEY_SIZE_Z].m_Points[0].m_Y;

                    render_line_callback(user_context, dmTransform::Apply(transform, Point3(-x_ext, -y_ext, -z_ext)), dmTransform::Apply(transform, Point3(x_ext, -y_ext, -z_ext)), color);
                    render_line_callback(user_context, dmTransform::Apply(transform, Point3(x_ext, -y_ext, -z_ext)), dmTransform::Apply(transform, Point3(x_ext, y_ext, -z_ext)), color);
                    render_line_callback(user_context, dmTransform::Apply(transform, Point3(x_ext, y_ext, -z_ext)), dmTransform::Apply(transform, Point3(-x_ext, y_ext, -z_ext)), color);
                    render_line_callback(user_context, dmTransform::Apply(transform, Point3(-x_ext, y_ext, -z_ext)), dmTransform::Apply(transform, Point3(-x_ext, -y_ext, -z_ext)), color);

                    render_line_callback(user_context, dmTransform::Apply(transform, Point3(-x_ext, -y_ext, z_ext)), dmTransform::Apply(transform, Point3(x_ext, -y_ext, z_ext)), color);
                    render_line_callback(user_context, dmTransform::Apply(transform, Point3(x_ext, -y_ext, z_ext)), dmTransform::Apply(transform, Point3(x_ext, y_ext, z_ext)), color);
                    render_line_callback(user_context, dmTransform::Apply(transform, Point3(x_ext, y_ext, z_ext)), dmTransform::Apply(transform, Point3(-x_ext, y_ext, z_ext)), color);
                    render_line_callback(user_context, dmTransform::Apply(transform, Point3(-x_ext, y_ext, z_ext)), dmTransform::Apply(transform, Point3(-x_ext, -y_ext, z_ext)), color);

                    render_line_callback(user_context, dmTransform::Apply(transform, Point3(-x_ext, -y_ext, -z_ext)), dmTransform::Apply(transform, Point3(-x_ext, -y_ext, z_ext)), color);
                    render_line_callback(user_context, dmTransform::Apply(transform, Point3(x_ext, -y_ext, -z_ext)), dmTransform::Apply(transform, Point3(x_ext, -y_ext, z_ext)), color);
                    render_line_callback(user_context, dmTransform::Apply(transform, Point3(x_ext, y_ext, -z_ext)), dmTransform::Apply(transform, Point3(x_ext, y_ext, z_ext)), color);
                    render_line_callback(user_context, dmTransform::Apply(transform, Point3(-x_ext, y_ext, -z_ext)), dmTransform::Apply(transform, Point3(-x_ext, y_ext, z_ext)), color);

                    break;
                }

                    default:
                        break;
                }
            }
        }
    }

    void LoadResources(Prototype* prototype, dmParticleDDF::ParticleFX* ddf)
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
            // Add-alpha is deprecated because of premultiplied alpha and replaced by Add
            if (emitter_ddf->m_BlendMode == dmParticleDDF::BLEND_MODE_ADD_ALPHA)
                emitter_ddf->m_BlendMode = dmParticleDDF::BLEND_MODE_ADD;
            EmitterPrototype* emitter = &prototype->m_Emitters[i];
            emitter->m_Animation = dmHashString64(emitter_ddf->m_Animation);
            emitter->m_BlendMode = emitter_ddf->m_BlendMode;
            // Approximate splines with linear segments
            memset(emitter->m_Properties, 0, sizeof(emitter->m_Properties));
            memset(emitter->m_ParticleProperties, 0, sizeof(emitter->m_ParticleProperties));
            uint32_t prop_count = emitter_ddf->m_Properties.m_Count;
            for (uint32_t j = 0; j < prop_count; ++j)
            {
                const dmParticleDDF::Emitter::Property& p = emitter_ddf->m_Properties[j];
                if (p.m_Key < dmParticleDDF::EMITTER_KEY_COUNT)
                {
                    Property& property = emitter->m_Properties[p.m_Key];
                    SampleProperty(p.m_Points.m_Data, p.m_Points.m_Count, property.m_Segments);
                    property.m_Spread = p.m_Spread;
                }
                else
                {
                    dmLogWarning("The key %d is not a valid emitter key.", p.m_Key);
                }
            }
            // Calculate max life time
            const Property& life_time = emitter->m_Properties[dmParticleDDF::EMITTER_KEY_PARTICLE_LIFE_TIME];
            float max_life_time = 0.0f;
            for (uint32_t j = 0; j < PROPERTY_SAMPLE_COUNT; ++j)
            {
                const LinearSegment& s = life_time.m_Segments[j];
                max_life_time = dmMath::Max(dmMath::Select(s.m_K, s.m_Y + s.m_K, s.m_Y), max_life_time);
            }
            emitter->m_MaxParticleLifeTime = max_life_time;
            // particle properties
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
                    const dmParticleDDF::Modifier::Property& p = modifier_ddf.m_Properties[j];
                    if (p.m_Key < dmParticleDDF::MODIFIER_KEY_COUNT)
                    {
                        Property& property = modifier.m_Properties[p.m_Key];
                        SampleProperty(p.m_Points.m_Data, p.m_Points.m_Count, property.m_Segments);
                        property.m_Spread = p.m_Spread;
                    }
                    else
                    {
                        dmLogWarning("The key %d is not a valid modifier key.", p.m_Key);
                    }
                }
            }
        }
    }

    Prototype* NewPrototype(const void* buffer, uint32_t buffer_size)
    {
        dmParticleDDF::ParticleFX* ddf = 0;
        dmDDF::Result r = dmDDF::LoadMessage<dmParticleDDF::ParticleFX>(buffer, buffer_size, &ddf);
        if (r == dmDDF::RESULT_OK)
        {
            return NewPrototypeFromDDF(ddf);
        }
        dmLogError("Failed to load particle data");
        return 0x0;
    }

    Prototype* NewPrototypeFromDDF(dmParticleDDF::ParticleFX* message)
    {
        Prototype* prototype = new Prototype();
        LoadResources(prototype, message);
        return prototype;
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
        dmParticleDDF::ParticleFX* ddf = 0;
        dmDDF::Result r = dmDDF::LoadMessage<dmParticleDDF::ParticleFX>(buffer, buffer_size, &ddf);
        if (r == dmDDF::RESULT_OK)
        {
            LoadResources(prototype, ddf);
            return true;
        }
        return false;
    }

    uint32_t GetEmitterCount(HPrototype prototype)
    {
        return prototype->m_Emitters.Size();
    }

    uint32_t GetInstanceEmitterCount(HParticleContext context, HInstance instance)
    {
        Instance* inst = GetInstance(context, instance);
        return inst->m_Emitters.Size();
    }

    void RenderEmitter(Instance* instance, uint32_t emitter_index, void* usercontext, RenderEmitterCallback render_emitter_callback);
    void RenderEmitter(HParticleContext context, HInstance instance, uint32_t emitter_index, void* user_context, RenderEmitterCallback render_instance_callback)
    {
        Instance* inst = GetInstance(context, instance);
        if (inst != 0x0 && emitter_index < inst->m_Emitters.Size())
        {
            RenderEmitter(GetInstance(context, instance), emitter_index, user_context, render_instance_callback);
        }
        else if (inst == 0x0)
        {
            dmLogError("The particlefx instance could not be found when rendering.");
        }
        else
        {
            dmLogError("The particlefx emitter could not be found when rendering.");
        }
    }

    void RenderEmitter(Instance* instance, uint32_t emitter_index, void* user_context, RenderEmitterCallback render_emitter_callback)
    {
        Emitter* emitter = &instance->m_Emitters[emitter_index];
        if (!emitter || emitter->m_VertexCount == 0) return;

        dmParticleDDF::Emitter* ddf = &instance->m_Prototype->m_DDF->m_Emitters[emitter_index];
        dmTransform::TransformS1 transform(Vector3(ddf->m_Position), ddf->m_Rotation, 1.0f);
        if (instance->m_ScaleAlongZ)
            transform = dmTransform::Mul(instance->m_WorldTransform, transform);
        else
            transform = dmTransform::MulNoScaleZ(instance->m_WorldTransform, transform);
        Vectormath::Aos::Matrix4 world = dmTransform::ToMatrix4(transform);
        dmParticle::EmitterPrototype* emitter_proto = &instance->m_Prototype->m_Emitters[emitter_index];

        render_emitter_callback(user_context, emitter_proto->m_Material, emitter->m_AnimationData.m_Texture, world, emitter_proto->m_BlendMode, emitter->m_VertexIndex, emitter->m_VertexCount, emitter->m_RenderConstants.Begin(), emitter->m_RenderConstants.Size());
    }

    // Update render data for the emitter at the specified index
    void UpdateEmitterRenderData(HInstance instance, uint32_t emitter_index, Instance* inst, Emitter* emitter, dmParticleDDF::Emitter* ddf)
    {
        EmitterRenderData& render_data = emitter->m_RenderData;

        dmTransform::TransformS1 transform(Vector3(ddf->m_Position), ddf->m_Rotation, 1.0f);
        if (inst->m_ScaleAlongZ)
            transform = dmTransform::Mul(inst->m_WorldTransform, transform);
        else
            transform = dmTransform::MulNoScaleZ(inst->m_WorldTransform, transform);
        Vectormath::Aos::Matrix4 world = dmTransform::ToMatrix4(transform);
        dmParticle::EmitterPrototype* emitter_proto = &inst->m_Prototype->m_Emitters[emitter_index];

        render_data.m_Transform = world;
        render_data.m_Material = emitter_proto->m_Material;
        render_data.m_BlendMode = emitter_proto->m_BlendMode;
        render_data.m_Texture = emitter->m_AnimationData.m_Texture;
        render_data.m_RenderConstants = emitter->m_RenderConstants.Begin();
        render_data.m_RenderConstantsSize = emitter->m_RenderConstants.Size();
        render_data.m_Instance = instance;
        render_data.m_EmitterIndex = emitter_index;
    }

    // Update render data for all emitters on an instance
    void UpdateRenderData(HParticleContext context, HInstance instance, uint32_t emitter_index)
    {
        Instance* inst = GetInstance(context, instance);

        if (inst != 0x0)
        {
            uint32_t emitter_count = inst->m_Emitters.Size();
            for (uint32_t i = 0; i < emitter_count; ++i)
            {
                Emitter* emitter = &inst->m_Emitters[i];
                dmParticleDDF::Emitter* emitter_ddf = &inst->m_Prototype->m_DDF->m_Emitters[i];
                UpdateEmitterRenderData(instance, i, inst, emitter, emitter_ddf);
            }
        }
    }

    void GetEmitterRenderData(HParticleContext context, HInstance instance, uint32_t emitter_index, EmitterRenderData** data)
    {
        Instance* inst = GetInstance(context, instance);

        if (inst != 0x0 && emitter_index < inst->m_Emitters.Size())
        {
            Emitter* emitter = &inst->m_Emitters[emitter_index];

            if (emitter && data != 0x0)
            {
                *data = &emitter->m_RenderData;
                return;
            }
        }

        *data = 0x0;
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

    void SetRenderConstant(HParticleContext context, HInstance instance, dmhash_t emitter_id, dmhash_t name_hash, Vector4 value)
    {
        Instance* inst = GetInstance(context, instance);
        uint32_t count = inst->m_Emitters.Size();
        for (uint32_t i = 0; i < count; ++i)
        {
            Emitter* e = &inst->m_Emitters[i];
            if (e->m_Id == emitter_id)
            {
                RenderConstant* c = 0x0;
                dmArray<RenderConstant>& constants = e->m_RenderConstants;
                uint32_t constant_count = constants.Size();
                for (uint32_t constant_i = 0; constant_i < constant_count; ++constant_i)
                {
                    RenderConstant* constant = &constants[constant_i];
                    if (constant->m_NameHash == name_hash)
                    {
                        c = constant;
                        break;
                    }
                }
                if (c == 0x0)
                {
                    if (constants.Full())
                    {
                        constants.SetCapacity(constants.Capacity() + 4);
                    }
                    constants.SetSize(constant_count + 1);
                    c = &constants[constant_count];
                    c->m_NameHash = name_hash;
                }
                c->m_Value = value;
                e->m_ReHash = 1;
            }
        }
    }

    void ResetRenderConstant(HParticleContext context, HInstance instance, dmhash_t emitter_id, dmhash_t name_hash)
    {
        Instance* inst = GetInstance(context, instance);
        uint32_t count = inst->m_Emitters.Size();
        for (uint32_t i = 0; i < count; ++i)
        {
            Emitter* e = &inst->m_Emitters[i];
            if (e->m_Id == emitter_id)
            {
                dmArray<RenderConstant>& constants = e->m_RenderConstants;
                uint32_t constant_count = constants.Size();
                for (uint32_t constant_i = 0; constant_i < constant_count; ++constant_i)
                {
                    if (constants[constant_i].m_NameHash == name_hash)
                    {
                        constants.EraseSwap(constant_i);
                        e->m_ReHash = 1;
                        break;
                    }
                }
                // Don't break here, look for more
            }
        }
    }

    void GetStats(HParticleContext context, Stats* stats)
    {
        assert(stats->m_StructSize == sizeof(*stats));
        *stats = context->m_Stats;
        stats->m_MaxParticles = context->m_MaxParticleCount;
    }

    void GetInstanceStats(HParticleContext context, HInstance instance, InstanceStats* stats)
    {
        assert(stats->m_StructSize == sizeof(*stats));
        Instance* i = GetInstance(context, instance);
        stats->m_Time = i->m_PlayTime;
    }

    uint32_t GetVertexBufferSize(uint32_t particle_count, ParticleVertexFormat vertex_format)
    {
        uint32_t vertex_size = sizeof(Vertex);

        if (vertex_format == PARTICLE_GUI)
            vertex_size = sizeof(ParticleGuiVertex);

        return particle_count * 6 * vertex_size;
    }

    uint32_t GetMaxVertexBufferSize(HParticleContext context, ParticleVertexFormat vertex_format)
    {
        return GetVertexBufferSize(context->m_MaxParticleCount, vertex_format);
    }

    void ReHashEmitter(Emitter* e)
    {
        EmitterRenderData& data = e->m_RenderData;

        if (data.m_Material == 0x0 || data.m_Texture == 0x0)
        {
            e->m_ReHash = 1;
            return;
        }

        HashState32 state;
        HashState32 state_no_material;
        bool reverse = false;
        dmHashInit32(&state, reverse);
        dmHashUpdateBuffer32(&state, &data.m_Texture, sizeof(data.m_Texture));
        dmHashUpdateBuffer32(&state, &data.m_BlendMode, sizeof(data.m_BlendMode));

        dmParticle::RenderConstant* constants = data.m_RenderConstants;
        uint32_t size = data.m_RenderConstantsSize;
        for (uint32_t j = 0; j < size; ++j)
        {
            dmParticle::RenderConstant& c = constants[j];
            dmHashUpdateBuffer32(&state, &c.m_NameHash, sizeof(uint64_t));
            dmHashUpdateBuffer32(&state, &c.m_Value, sizeof(Vector4));
        }
        memcpy(&state_no_material, &state, sizeof(HashState32));
        data.m_MixedHashNoMaterial = dmHashFinal32(&state_no_material);

        dmHashUpdateBuffer32(&state, &data.m_Material, sizeof(data.m_Material));
        data.m_MixedHash = dmHashFinal32(&state);
        e->m_ReHash = 0;
    }

    void ReHash(HParticleContext context, HInstance instance)
    {
        Instance* inst = GetInstance(context, instance);
        uint32_t count = inst->m_Emitters.Size();
        for (uint32_t i = 0; i < count; ++i)
        {
            Emitter* e = &inst->m_Emitters[i];
            ReHashEmitter(e);
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

#define DM_PARTICLE_TRAMPOLINE6(ret, name, t1, t2, t3, t4, t5, t6) \
    ret Particle_##name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6)\
    {\
        return name(a1, a2, a3, a4, a5, a6);\
    }\

#define DM_PARTICLE_TRAMPOLINE7(ret, name, t1, t2, t3, t4, t5, t6, t7) \
    ret Particle_##name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7)\
    {\
        return name(a1, a2, a3, a4, a5, a6, a7);\
    }\

#define DM_PARTICLE_TRAMPOLINE8(ret, name, t1, t2, t3, t4, t5, t6, t7, t8) \
    ret Particle_##name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7, t8 a8)\
    {\
        return name(a1, a2, a3, a4, a5, a6, a7, a8);\
    }\

#define DM_PARTICLE_TRAMPOLINE9(ret, name, t1, t2, t3, t4, t5, t6, t7, t8, t9) \
    ret Particle_##name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7, t8 a8, t9 a9)\
    {\
        return name(a1, a2, a3, a4, a5, a6, a7, a8, a9);\
    }\

#define DM_PARTICLE_TRAMPOLINE10(ret, name, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10) \
    ret Particle_##name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7, t8 a8, t9 a9, t10 a10)\
    {\
        return name(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10);\
    }\

    DM_PARTICLE_TRAMPOLINE2(HParticleContext, CreateContext, uint32_t, uint32_t);
    DM_PARTICLE_TRAMPOLINE1(void, DestroyContext, HParticleContext);
    DM_PARTICLE_TRAMPOLINE1(uint32_t, GetContextMaxParticleCount, HParticleContext);
    DM_PARTICLE_TRAMPOLINE2(void, SetContextMaxParticleCount, HParticleContext, uint32_t);

    DM_PARTICLE_TRAMPOLINE3(HInstance, CreateInstance, HParticleContext, HPrototype, EmitterStateChangedData*);
    DM_PARTICLE_TRAMPOLINE2(void, DestroyInstance, HParticleContext, HInstance);
    DM_PARTICLE_TRAMPOLINE3(void, ReloadInstance, HParticleContext, HInstance, bool);

    DM_PARTICLE_TRAMPOLINE2(void, StartInstance, HParticleContext, HInstance);
    DM_PARTICLE_TRAMPOLINE2(void, StopInstance, HParticleContext, HInstance);
    DM_PARTICLE_TRAMPOLINE2(void, ResetInstance, HParticleContext, HInstance);
    DM_PARTICLE_TRAMPOLINE3(void, SetPosition, HParticleContext, HInstance, const Point3&);
    DM_PARTICLE_TRAMPOLINE3(void, SetRotation, HParticleContext, HInstance, const Quat&);
    DM_PARTICLE_TRAMPOLINE3(void, SetScale, HParticleContext, HInstance, float);
    DM_PARTICLE_TRAMPOLINE3(void, SetScaleAlongZ, HParticleContext, HInstance, bool);

    DM_PARTICLE_TRAMPOLINE2(bool, IsSleeping, HParticleContext, HInstance);
    DM_PARTICLE_TRAMPOLINE3(void, Update, HParticleContext, float, FetchAnimationCallback);
    DM_PARTICLE_TRAMPOLINE9(void, GenerateVertexData, HParticleContext, float, HInstance, uint32_t, const Vector4&, void*, uint32_t, uint32_t*, ParticleVertexFormat);

    DM_PARTICLE_TRAMPOLINE2(HPrototype, NewPrototype, const void*, uint32_t);
    DM_PARTICLE_TRAMPOLINE1(HPrototype, NewPrototypeFromDDF, dmParticleDDF::ParticleFX*);
    DM_PARTICLE_TRAMPOLINE1(void, DeletePrototype, HPrototype);
    DM_PARTICLE_TRAMPOLINE3(bool, ReloadPrototype, HPrototype, const void*, uint32_t);

    DM_PARTICLE_TRAMPOLINE1(uint32_t, GetEmitterCount, HPrototype);
    DM_PARTICLE_TRAMPOLINE5(void, RenderEmitter, HParticleContext, HInstance, uint32_t, void*, RenderEmitterCallback);
    DM_PARTICLE_TRAMPOLINE2(const char*, GetMaterialPath, HPrototype, uint32_t);
    DM_PARTICLE_TRAMPOLINE2(const char*, GetTileSourcePath, HPrototype, uint32_t);
    DM_PARTICLE_TRAMPOLINE2(void*, GetMaterial, HPrototype, uint32_t);
    DM_PARTICLE_TRAMPOLINE2(void*, GetTileSource, HPrototype, uint32_t);
    DM_PARTICLE_TRAMPOLINE3(void, SetMaterial, HPrototype, uint32_t, void*);
    DM_PARTICLE_TRAMPOLINE3(void, SetTileSource, HPrototype, uint32_t, void*);

    DM_PARTICLE_TRAMPOLINE5(void, SetRenderConstant, HParticleContext, HInstance, dmhash_t, dmhash_t, Vector4);
    DM_PARTICLE_TRAMPOLINE4(void, ResetRenderConstant, HParticleContext, HInstance, dmhash_t, dmhash_t);

    DM_PARTICLE_TRAMPOLINE2(void, GetStats, HParticleContext, Stats*);
    DM_PARTICLE_TRAMPOLINE3(void, GetInstanceStats, HParticleContext, HInstance, InstanceStats*);

    DM_PARTICLE_TRAMPOLINE2(uint32_t, GetVertexBufferSize, uint32_t, ParticleVertexFormat);

    dmhash_t Particle_Hash(const char* value)
    {
        return dmHashString64(value);
    }
}
