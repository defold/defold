#include "profile_render.h"

#include <string.h>

#include <algorithm>

#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/hashtable.h>
#include <dlib/math.h>
#include <dlib/profile.h>
#include <render/font_renderer.h>
#include <render/font_ddf.h>

namespace dmProfileRender
{
    using namespace Vectormath::Aos;

    typedef uint16_t TIndex;
    typedef uint32_t TNameHash;

    static TNameHash GetScopeHash(const dmProfile::Scope* scope)
    {
        return scope->m_NameHash;
    }

    static TNameHash GetSampleHash(const dmProfile::Sample* sample)
    {
        HashState32 hash_state;
        dmHashInit32(&hash_state, false);
        dmHashUpdateBuffer32(&hash_state, sample->m_Name, strlen(sample->m_Name));
        TNameHash sample_hash = dmHashFinal32(&hash_state);
        TNameHash scope_hash = GetScopeHash(sample->m_Scope);

        TNameHash hash = scope_hash ^ (sample_hash + 0x9e3779b9 + (scope_hash << 6) + (scope_hash >> 2));
        return hash;
    }

    static TNameHash GetCounterHash(const dmProfile::Counter* counter)
    {
        return counter->m_NameHash;
    }

    //  float *r, *g, *b; /* red, green, blue in [0,1] */
    //  float h, s, l;    /* hue in [0,360]; saturation, light in [0,1] */
    static void hsl_to_rgb(float* r, float* g, float* b, float h, float s, float l)
    {
        float c = (1.0f - dmMath::Abs(2.0f * l - 1.0f)) * s;
        float hp = h / 60.0f;
        int hpi = (int)hp;
        float hpmod2 = (hpi % 2) + (hp - hpi);
        float x = c * (1.0f - dmMath::Abs(hpmod2 - 1.0f));
        switch (hpi)
        {
            case 0: *r = c; *g = x; *b = 0; break;
            case 1: *r = x; *g = c; *b = 0; break;
            case 2: *r = 0; *g = c; *b = x; break;
            case 3: *r = 0; *g = x; *b = c; break;
            case 4: *r = x; *g = 0; *b = c; break;
            case 5: *r = c; *g = 0; *b = x; break;
        }
        float m = l - 0.5f * c;
        *r += m;
        *g += m;
        *b += m;
    }

    static void HslToRgb2(float h, float s, float l, float* c)
    {
        hsl_to_rgb(&c[0], &c[1], &c[2], h * 360, s, l);
    }

    struct Scope {
        const char* m_Name;
        TNameHash m_NameHash;    // m_Name
        uint32_t m_Elapsed;
        uint32_t m_Count;
    };

    struct SampleSum {
        const char* m_Name;
        TNameHash m_NameHash;   // m_Name + Scope::m_Name
        TNameHash m_ScopeHash;  // Scope::m_Name
        uint32_t m_Elapsed;
        uint32_t m_Count;
        TIndex m_LastSampleIndex;
    };

    struct Counter {
        const char* m_Name;
        TNameHash m_NameHash;    // m_Name
        uint32_t m_Count;
    };

    struct Sample {
        uint32_t m_StartTick;
        uint32_t m_Elapsed;
        TIndex m_PreviousSampleIndex;
    };

    #define HASHTABLE_BUFFER_SIZE(c, table_size, capacity) ((table_size) * sizeof(uint32_t) + (capacity) * sizeof(c))

    typedef dmHashTable<TNameHash, TIndex> TIndexLookupTable;

    struct ProfileFrame
    {
        ProfileFrame(
                Scope* scopes,
                SampleSum* sample_sums,
                Counter* counters,
                Sample* samples)
            : m_Scopes(scopes)
            , m_SampleSums(sample_sums)
            , m_Counters(counters)
            , m_Samples(samples)
            , m_FrameTime(0.f)
            , m_MaxFrameTime(0.f)
        {
        }

        Scope* m_Scopes;
        SampleSum* m_SampleSums;
        Counter* m_Counters;
        Sample* m_Samples;
        float m_FrameTime;
        float m_WaitTime;
        float m_MaxFrameTime;
    };

    static size_t ProfileFrameSize(TIndex max_scope_count, TIndex max_sample_sum_count, TIndex max_counter_count, TIndex max_sample_count)
    {
        const size_t scopes_size = sizeof(Scope) * max_scope_count;
        const size_t sample_sums_size = sizeof(SampleSum) * max_sample_sum_count;
        const size_t counters_size = sizeof(Counter) * max_counter_count;
        const size_t samples_size = sizeof(Sample) * max_sample_count;

        size_t size = sizeof(ProfileFrame) +
            scopes_size +
            sample_sums_size +
            counters_size +
            samples_size;

        return size;
    }

    static ProfileFrame* CreateProfileFrame(void* mem, TIndex max_scope_count, TIndex max_sample_sum_count, TIndex max_counter_count, TIndex max_sample_count)
    {
        const size_t scopes_size = sizeof(Scope) * max_scope_count;
        const size_t sample_sums_size = sizeof(SampleSum) * max_sample_sum_count;
        const size_t counters_size = sizeof(Counter) * max_counter_count;
        const size_t samples_size = sizeof(Sample) * max_sample_count;

        uint8_t* p = &((uint8_t*)mem)[sizeof(ProfileFrame)];

        Scope* scopes = (Scope*)p;
        p += scopes_size;

        SampleSum* sample_sums = (SampleSum*)p;
        p += sample_sums_size;

        Counter* counters = (Counter*)p;
        p += counters_size;

        Sample* samples = (Sample*)p;
        p += samples_size;

        ProfileFrame* s = new (mem) ProfileFrame(
            scopes,
            sample_sums,
            counters,
            samples);
        return s;
    }

    struct ProfileSnapshot
    {
        ProfileSnapshot(
                uint64_t frame_tick,
                Scope* scopes,
                SampleSum* sample_sums,
                Counter* counters,
                Sample* samples,
                TIndex scope_count,
                TIndex sample_sum_count,
                TIndex counter_count,
                TIndex sample_count)
            : m_FrameTick(frame_tick)
            , m_Frame(
                scopes,
                sample_sums,
                counters,
                samples)
            , m_ScopeCount(scope_count)
            , m_SampleSumCount(sample_sum_count)
            , m_CounterCount(counter_count)
            , m_SampleCount(sample_count)
        {
        }

        uint64_t m_FrameTick;
        ProfileFrame m_Frame;
        TIndex m_ScopeCount;
        TIndex m_SampleSumCount;
        TIndex m_CounterCount;
        TIndex m_SampleCount;
    };

    static size_t ProfileSnapshotSize(TIndex scope_count, TIndex sample_sum_count, TIndex counter_count, TIndex sample_count)
    {
        const size_t scopes_size = sizeof(Scope) * scope_count;
        const size_t sample_sums_size = sizeof(SampleSum) * sample_sum_count;
        const size_t counters_size = sizeof(Counter) * counter_count;
        const size_t samples_size = sizeof(Sample) * sample_count;

        size_t size = sizeof(ProfileSnapshot) +
            scopes_size +
            sample_sums_size +
            counters_size +
            samples_size;

        return size;
    }

    static ProfileSnapshot* CreateProfileSnapshot(void* mem, uint64_t frame_tick, TIndex scope_count, TIndex sample_sum_count, TIndex counter_count, TIndex sample_count)
    {
        const size_t scopes_size = sizeof(Scope) * scope_count;
        const size_t sample_sums_size = sizeof(SampleSum) * sample_sum_count;
        const size_t counters_size = sizeof(Counter) * counter_count;
        const size_t samples_size = sizeof(Sample) * sample_count;

        uint8_t* p = &((uint8_t*)mem)[sizeof(ProfileSnapshot)];

        Scope* scopes = (Scope*)p;
        p += scopes_size;

        SampleSum* sample_sums = (SampleSum*)p;
        p += sample_sums_size;

        Counter* counters = (Counter*)p;
        p += counters_size;

        Sample* samples = (Sample*)p;
        p += samples_size;

        ProfileSnapshot* s = new (mem) ProfileSnapshot(
            frame_tick,
            scopes,
            sample_sums,
            counters,
            samples,
            scope_count,
            sample_sum_count,
            counter_count,
            sample_count);
        return s;
    }

    static ProfileSnapshot* MakeProfileSnapshot(uint64_t frame_tick, const ProfileFrame* frame, TIndex scope_count, TIndex sample_sum_count, TIndex counter_count, TIndex sample_count)
    {
        void* mem = malloc(ProfileSnapshotSize(scope_count, sample_sum_count, counter_count, sample_count));
        ProfileSnapshot* snapshot = CreateProfileSnapshot(mem, frame_tick, scope_count, sample_sum_count, counter_count, sample_count);
        memcpy(snapshot->m_Frame.m_Scopes, frame->m_Scopes, sizeof(Scope) * scope_count);
        memcpy(snapshot->m_Frame.m_SampleSums, frame->m_SampleSums, sizeof(SampleSum) * sample_sum_count);
        memcpy(snapshot->m_Frame.m_Counters, frame->m_Counters, sizeof(Counter) * counter_count);
        memcpy(snapshot->m_Frame.m_Samples, frame->m_Samples, sizeof(Sample) * sample_count);
        snapshot->m_Frame.m_FrameTime = frame->m_FrameTime;
        snapshot->m_Frame.m_WaitTime = frame->m_WaitTime;
        snapshot->m_Frame.m_MaxFrameTime = frame->m_MaxFrameTime;

        return snapshot;
    }

    struct ScopeStats
    {
        uint64_t m_LastSeenTick;
        uint32_t m_FilteredElapsed; // Used for sorting, highest peak since last sort
    };

    struct SampleSumStats
    {
        uint64_t m_LastSeenTick;
        uint32_t m_FilteredElapsed; // Used for sorting, highest peak since last sort
    };

    struct CounterStats
    {
        uint64_t m_LastSeenTick;
    };

    struct RenderProfile {
        static RenderProfile* New(
            float fps,
            uint32_t ticks_per_second,
            uint32_t lifetime_in_seconds,
            uint32_t sort_intervall_in_seconds,
            TIndex max_scope_count,
            TIndex max_sample_sum_count,
            TIndex max_counter_count,
            TIndex max_sample_count);
        static void Delete(RenderProfile* render_profile);

        const float m_FPS;
        const uint32_t m_TicksPerSecond;
        const uint32_t m_LifeTime;
        const uint32_t m_SortInterval;

        ProfileFrame* m_BuildFrame;
        const ProfileFrame* m_ActiveFrame;
        dmArray<ProfileSnapshot*> m_RecordBuffer;

        ProfilerMode m_Mode;
        ProfilerViewMode m_ViewMode;

        TIndexLookupTable m_ScopeLookup;
        TIndexLookupTable m_SampleSumLookup;
        TIndexLookupTable m_CounterLookup;

        ScopeStats* m_ScopeStats;
        SampleSumStats* m_SampleSumStats;
        CounterStats* m_CounterStats;

        uint64_t m_NowTick;
        uint64_t m_LastSortTick;

        TIndex m_SampleCount;

        const TIndex m_MaxScopeCount;
        const TIndex m_MaxSampleSumCount;
        const TIndex m_MaxCounterCount;

        const TIndex m_MaxSampleCount;

        int32_t m_PlaybackFrame;

        uint32_t m_IncludeFrameWait : 1;
        uint32_t m_ScopeOverflow : 1;
        uint32_t m_SampleSumOverflow : 1;
        uint32_t m_CounterOverflow : 1;
        uint32_t m_SampleOverflow : 1;
        uint32_t m_OutOfScopes : 1;
        uint32_t m_OutOfSamples : 1;

    private:
        RenderProfile(
            float fps,
            uint32_t ticks_per_second,
            uint32_t lifetime_in_seconds,
            uint32_t sort_intervall_in_seconds,
            TIndex max_scope_count,
            TIndex max_sample_sum_count,
            TIndex max_counter_count,
            TIndex max_sample_count,
            void* scope_lookup_data,
            void* sample_sum_lookup_data,
            void* counter_lookup_data,
            ScopeStats* scope_stats,
            SampleSumStats* sample_sum_stats,
            CounterStats* counter_stats,
            ProfileFrame* build_frame);
        ~RenderProfile();
    };

    RenderProfile::RenderProfile(
            float fps,
            uint32_t ticks_per_second,
            uint32_t lifetime_in_seconds,
            uint32_t sort_intervall_in_seconds,
            TIndex max_scope_count,
            TIndex max_sample_sum_count,
            TIndex max_counter_count,
            TIndex max_sample_count,
            void* scope_lookup_data,
            void* sample_sum_lookup_data,
            void* counter_lookup_data,
            ScopeStats* scope_stats,
            SampleSumStats* sample_sum_stats,
            CounterStats* counter_stats,
            ProfileFrame* build_frame)
        : m_FPS(fps)
        , m_TicksPerSecond(ticks_per_second)
        , m_LifeTime((uint32_t)(lifetime_in_seconds * ticks_per_second))
        , m_SortInterval((uint32_t)(sort_intervall_in_seconds * ticks_per_second))
        , m_BuildFrame(build_frame)
        , m_ActiveFrame(m_BuildFrame)
        , m_Mode(PROFILER_MODE_RUN)
        , m_ViewMode(PROFILER_VIEW_MODE_FULL)
        , m_ScopeLookup(scope_lookup_data, (max_scope_count * 2) / 3, max_scope_count * 2)
        , m_SampleSumLookup(sample_sum_lookup_data, (max_sample_sum_count * 2) / 3, max_sample_sum_count * 2)
        , m_CounterLookup(counter_lookup_data, (max_counter_count * 2) / 3, max_counter_count * 2)
        , m_ScopeStats(scope_stats)
        , m_SampleSumStats(sample_sum_stats)
        , m_CounterStats(counter_stats)
        , m_NowTick(0u)
        , m_LastSortTick(0u)
        , m_MaxScopeCount(max_scope_count)
        , m_MaxSampleSumCount(max_sample_sum_count)
        , m_MaxCounterCount(max_counter_count)
        , m_MaxSampleCount(max_sample_count)
        , m_PlaybackFrame(-1)
        , m_IncludeFrameWait(1)
        , m_ScopeOverflow(0)
        , m_SampleSumOverflow(0)
        , m_CounterOverflow(0)
        , m_SampleOverflow(0)
        , m_OutOfScopes(0)
        , m_OutOfSamples(0)
    {}

    RenderProfile::~RenderProfile()
    {
    }

    static void FlushRecording(RenderProfile* render_profile, uint32_t capacity)
    {
        uint32_t c = render_profile->m_RecordBuffer.Size();
        while (c-- > 0)
        {
            delete render_profile->m_RecordBuffer[c];
        }
        render_profile->m_RecordBuffer.SetCapacity(capacity);
        render_profile->m_RecordBuffer.SetSize(0);
        render_profile->m_PlaybackFrame = -1;
    }

    RenderProfile* RenderProfile::New(
        float fps,
        uint32_t ticks_per_second,
        uint32_t lifetime_in_seconds,
        uint32_t sort_intervall_in_seconds,
        TIndex max_scope_count,
        TIndex max_sample_sum_count,
        TIndex max_counter_count,
        TIndex max_sample_count)
    {
        const size_t scope_lookup_data_size = HASHTABLE_BUFFER_SIZE(TIndexLookupTable::Entry, (max_scope_count * 2) / 3, max_scope_count * 2);
        const size_t sample_sum_lookup_data_size = HASHTABLE_BUFFER_SIZE(TIndexLookupTable::Entry, (max_sample_sum_count * 2) / 3, max_sample_sum_count * 2);
        const size_t counter_lookup_data_size = HASHTABLE_BUFFER_SIZE(TIndexLookupTable::Entry, (max_counter_count * 2) / 3, max_counter_count * 2);
        const size_t scope_stats_size = sizeof(ScopeStats) * max_scope_count;
        const size_t sample_sum_stats_size = sizeof(SampleSumStats) * max_sample_sum_count;
        const size_t counter_stats_size = sizeof(CounterStats) * max_counter_count;

        size_t size = sizeof(RenderProfile) +
            scope_lookup_data_size +
            sample_sum_lookup_data_size +
            counter_lookup_data_size +
            scope_stats_size +
            sample_sum_stats_size +
            counter_stats_size +
            ProfileFrameSize(max_scope_count, max_sample_sum_count, max_counter_count, max_sample_count);

        uint8_t* mem = (uint8_t*)malloc(size);
        if (mem == 0x0)
        {
            return 0x0;
        }
        uint8_t* p = &mem[sizeof(RenderProfile)];
        void* scope_lookup_data = p;
        p += scope_lookup_data_size;

        void* sample_sum_lookup_data = p;
        p += sample_sum_lookup_data_size;

        void* counter_lookup_data = p;
        p += counter_lookup_data_size;

        ScopeStats* scope_stats_data = (ScopeStats*)p;
        p += scope_stats_size;

        SampleSumStats* sample_sum_stats_data = (SampleSumStats*)p;
        p += sample_sum_stats_size;

        CounterStats* counter_stats_data = (CounterStats*)p;
        p += counter_stats_size;

        ProfileFrame* frame = CreateProfileFrame(p, max_scope_count, max_sample_sum_count, max_counter_count, max_sample_count);

        return new (mem) RenderProfile(
            fps,
            ticks_per_second,
            lifetime_in_seconds,
            sort_intervall_in_seconds,
            max_scope_count,
            max_sample_sum_count,
            max_counter_count,
            max_sample_count,
            scope_lookup_data,
            sample_sum_lookup_data,
            counter_lookup_data,
            scope_stats_data,
            sample_sum_stats_data,
            counter_stats_data,
            frame);
    }

    void RenderProfile::Delete(RenderProfile* render_profile)
    {
        FlushRecording(render_profile, 0);
        delete render_profile;
    }

    static void AddScope(RenderProfile* render_profile, const char* name, TNameHash name_hash, uint32_t elapsed, uint32_t count)
    {
        ProfileFrame* frame = render_profile->m_BuildFrame;
        TIndex* index_ptr = render_profile->m_ScopeLookup.Get(name_hash);
        if (index_ptr == 0x0)
        {
            if (render_profile->m_ScopeLookup.Full())
            {
                render_profile->m_ScopeOverflow = 1;
                return;
            }
            TIndex new_index = render_profile->m_ScopeLookup.Size();
            if (new_index == render_profile->m_MaxScopeCount)
            {
                render_profile->m_ScopeOverflow = 1;
                return;
            }
            index_ptr = render_profile->m_ScopeLookup.Put(name_hash, new_index);
            Scope* scope = &frame->m_Scopes[new_index];
            scope->m_Name = name;
            scope->m_NameHash = name_hash;
            scope->m_Count = 0u;
            scope->m_Elapsed = 0u;

            ScopeStats* scope_stats = &render_profile->m_ScopeStats[new_index];
            scope_stats->m_FilteredElapsed = 0u;
            scope_stats->m_LastSeenTick = render_profile->m_NowTick - render_profile->m_LifeTime;
        }
        uint32_t index = *index_ptr;
        Scope* scope = &frame->m_Scopes[index];
        scope->m_Elapsed += elapsed;
        scope->m_Count += count;
    }

    static void FreeScope(RenderProfile* render_profile, TIndex index)
    {
        ProfileFrame* frame = render_profile->m_BuildFrame;
        Scope* scope = &frame->m_Scopes[index];
        ScopeStats* stats = &render_profile->m_ScopeStats[index];
        render_profile->m_ScopeLookup.Erase(scope->m_NameHash);
        uint32_t new_count = render_profile->m_ScopeLookup.Size();
        if (index != new_count)
        {
            Scope* source = &frame->m_Scopes[new_count];
            TIndex* source_index_ptr = render_profile->m_ScopeLookup.Get(source->m_NameHash);
            assert(source_index_ptr != 0x0);

            ScopeStats* source_stats = &render_profile->m_ScopeStats[new_count];
            *scope = *source;
            *stats = *source_stats;

            *source_index_ptr = index;
        }
    }

    static void AddSample(RenderProfile* render_profile, const char* name, TNameHash name_hash, TNameHash scope_name_hash, uint32_t start_tick, uint32_t elapsed)
    {
        ProfileFrame* frame = render_profile->m_BuildFrame;
        TIndex* index_ptr = render_profile->m_SampleSumLookup.Get(name_hash);
        if (index_ptr == 0x0)
        {
            if (render_profile->m_SampleSumLookup.Full())
            {
                render_profile->m_SampleSumOverflow = 1;
                return;
            }
            TIndex new_index = render_profile->m_SampleSumLookup.Size();
            if (new_index == render_profile->m_MaxSampleSumCount)
            {
                render_profile->m_SampleSumOverflow = 1;
                return;
            }
            index_ptr = render_profile->m_SampleSumLookup.Put(name_hash, new_index);
            SampleSum* sample_sum = &frame->m_SampleSums[new_index];
            sample_sum->m_Name = name;
            sample_sum->m_NameHash = name_hash;
            sample_sum->m_ScopeHash = scope_name_hash;
            sample_sum->m_Elapsed = 0u;
            sample_sum->m_Count = 0u;
            sample_sum->m_LastSampleIndex = render_profile->m_MaxSampleCount;

            SampleSumStats* sample_sum_stats = &render_profile->m_SampleSumStats[new_index];
            sample_sum_stats->m_FilteredElapsed = 0u;
            sample_sum_stats->m_LastSeenTick = render_profile->m_NowTick - render_profile->m_LifeTime;
        }
        uint32_t index = *index_ptr;
        SampleSum* sample_sum = &frame->m_SampleSums[index];

        if (sample_sum->m_LastSampleIndex != render_profile->m_MaxSampleCount)
        {
            Sample* last_sample = &frame->m_Samples[sample_sum->m_LastSampleIndex];
            uint32_t end_last = last_sample->m_StartTick + last_sample->m_Elapsed;
            if (start_tick >= last_sample->m_StartTick && start_tick < end_last)
            {
                // Probably recursion. The sample is overlapping the previous.
                // Ignore this sample.
                return;
            }
        }

        sample_sum->m_Elapsed += elapsed;
        sample_sum->m_Count += 1u;

        if (render_profile->m_SampleCount == render_profile->m_MaxSampleCount)
        {
            render_profile->m_SampleOverflow = 1;
            return;
        }

        TIndex sample_index = render_profile->m_SampleCount++;
        Sample* sample = &frame->m_Samples[sample_index];
        sample->m_PreviousSampleIndex = sample_sum->m_LastSampleIndex;
        sample->m_StartTick = start_tick;
        sample->m_Elapsed = elapsed;

        sample_sum->m_LastSampleIndex = sample_index;
    }

    static void FreeSampleSum(RenderProfile* render_profile, TIndex index)
    {
        ProfileFrame* frame = render_profile->m_BuildFrame;
        SampleSum* sample_sum = &frame->m_SampleSums[index];
        SampleSumStats* sample_sum_stats = &render_profile->m_SampleSumStats[index];
        render_profile->m_SampleSumLookup.Erase(sample_sum->m_NameHash);
        uint32_t new_count = render_profile->m_SampleSumLookup.Size();
        if (index != new_count)
        {
            SampleSum* source = &frame->m_SampleSums[new_count];
            TIndex* source_index_ptr = render_profile->m_SampleSumLookup.Get(source->m_NameHash);
            assert(source_index_ptr != 0x0);

            SampleSumStats* source_stats = &render_profile->m_SampleSumStats[new_count];
            *sample_sum = *source;
            *sample_sum_stats = *source_stats;

            *source_index_ptr = index;
        }
    }

    static void AddCounter(RenderProfile* render_profile, const char* name, TNameHash name_hash, uint32_t count)
    {
        ProfileFrame* frame = render_profile->m_BuildFrame;
        TIndex* index_ptr = render_profile->m_CounterLookup.Get(name_hash);
        if (index_ptr == 0x0)
        {
            if (render_profile->m_CounterLookup.Full())
            {
                render_profile->m_CounterOverflow = 1;
                return;
            }
            TIndex new_index = render_profile->m_CounterLookup.Size();
            if (new_index == render_profile->m_MaxCounterCount)
            {
                render_profile->m_CounterOverflow = 1;
                return;
            }
            index_ptr = render_profile->m_CounterLookup.Put(name_hash, new_index);
            Counter* counter = &frame->m_Counters[new_index];
            counter->m_Name = name;
            counter->m_NameHash = name_hash;
            counter->m_Count = 0u;
            CounterStats* counter_stats = &render_profile->m_CounterStats[new_index];
            counter_stats->m_LastSeenTick = render_profile->m_NowTick;
        }
        uint32_t index = *index_ptr;
        Counter* counter = &frame->m_Counters[index];
        counter->m_Count += count;
    }

    static void FreeCounter(RenderProfile* render_profile, TIndex index)
    {
        ProfileFrame* frame = render_profile->m_BuildFrame;
        Counter* counter = &frame->m_Counters[index];
        CounterStats* counter_stats = &render_profile->m_CounterStats[index];
        render_profile->m_CounterLookup.Erase(counter->m_NameHash);
        uint32_t new_count = render_profile->m_CounterLookup.Size();
        if (index != new_count)
        {
            Counter* source = &frame->m_Counters[new_count];
            TIndex* source_index_ptr = render_profile->m_CounterLookup.Get(source->m_NameHash);
            assert(source_index_ptr != 0x0);

            CounterStats* source_stats = &render_profile->m_CounterStats[new_count];
            *counter = *source;
            *counter_stats = *source_stats;

            *source_index_ptr = index;
        }
    }

    static void BuildScope(void* context, const dmProfile::ScopeData* scope_data)
    {
        if (scope_data->m_Count == 0)
        {
            return;
        }
        RenderProfile* render_profile = (RenderProfile*)context;
        dmProfile::Scope* scope = scope_data->m_Scope;
        TNameHash name_hash = GetScopeHash(scope);
        AddScope(render_profile, scope->m_Name, name_hash, scope_data->m_Elapsed, scope_data->m_Count);
    }

    static void BuildSampleSum(void* context, const dmProfile::Sample* sample)
    {
        if (sample->m_Elapsed == 0u)
        {
            return;
        }
        RenderProfile* render_profile = (RenderProfile*)context;
        TNameHash scope_name_hash = GetScopeHash(sample->m_Scope);
        TNameHash name_hash = GetSampleHash(sample);
        AddSample(render_profile, sample->m_Name, name_hash, scope_name_hash, sample->m_Start, sample->m_Elapsed);
    }

    static void BuildCounter(void* context, const dmProfile::CounterData* counter_data)
    {
        if (counter_data->m_Value == 0)
        {
            return;
        }
        RenderProfile* render_profile = (RenderProfile*)context;
        dmProfile::Counter* counter = counter_data->m_Counter;
        TNameHash name_hash = GetCounterHash(counter);
        AddCounter(render_profile, counter->m_Name, name_hash, counter_data->m_Value);
    }

    static void ResetStructure(RenderProfile* render_profile)
    {
        ProfileFrame* frame = render_profile->m_BuildFrame;
        if (render_profile->m_ActiveFrame != frame)
        {
            render_profile->m_ScopeLookup.Clear();
            render_profile->m_SampleSumLookup.Clear();
            render_profile->m_CounterLookup.Clear();
            render_profile->m_ActiveFrame = frame;
        }
        else
        {
            uint32_t scope_count = render_profile->m_ScopeLookup.Size();
            for (uint32_t i = 0; i < scope_count; ++i)
            {
                Scope* scope = &frame->m_Scopes[i];
                scope->m_Elapsed = 0u;
                scope->m_Count = 0u;
            }
            uint32_t sample_sum_count = render_profile->m_SampleSumLookup.Size();
            for (uint32_t i = 0; i < sample_sum_count; ++i)
            {
                SampleSum* sample_sum = &frame->m_SampleSums[i];
                sample_sum->m_LastSampleIndex = render_profile->m_MaxSampleCount;
                sample_sum->m_Elapsed = 0u;
                sample_sum->m_Count = 0u;
            }
            uint32_t counter_count = render_profile->m_CounterLookup.Size();
            for (uint32_t i = 0; i < counter_count; ++i)
            {
                Counter* counter = &frame->m_Counters[i];
                counter->m_Count = 0u;
            }
        }
        render_profile->m_SampleCount = 0;

        render_profile->m_ScopeOverflow = 0;
        render_profile->m_SampleSumOverflow = 0;
        render_profile->m_CounterOverflow = 0;
        render_profile->m_SampleOverflow = 0;
    }

    static const TNameHash FRAME_WAIT_SCOPE_NAME_HASH = 3155412768u;
    static const TNameHash FRAME_SCOPE_NAME_HASH = 3881933696u;

    static uint32_t GetWaitTicks(HRenderProfile render_profile)
    {
        TIndex* wait_time_ptr = render_profile->m_SampleSumLookup.Get(FRAME_WAIT_SCOPE_NAME_HASH);
        if (wait_time_ptr != 0x0)
        {
            SampleSum* sample_sum = &render_profile->m_ActiveFrame->m_SampleSums[*wait_time_ptr];
            return sample_sum->m_Elapsed;
        }
        return 0;
    }

    static float GetWaitTime(HRenderProfile render_profile)
    {
        uint32_t wait_ticks = GetWaitTicks(render_profile);
        uint64_t ticks_per_second = render_profile->m_TicksPerSecond;
        double elapsed_s = (double)(wait_ticks) / ticks_per_second;
        float elapsed_ms = (float)(elapsed_s * 1000.0);
        return elapsed_ms;
    }

    static uint32_t GetFrameTicks(HRenderProfile render_profile)
    {
        TIndex* frame_time_ptr = render_profile->m_SampleSumLookup.Get(FRAME_SCOPE_NAME_HASH);
        if (frame_time_ptr != 0x0)
        {
            SampleSum* sample_sum = &render_profile->m_ActiveFrame->m_SampleSums[*frame_time_ptr];
            return sample_sum->m_Elapsed;
        }
        return 0;
    }

    static uint32_t GetActiveFrameTicks(HRenderProfile render_profile, uint32_t frame_ticks)
    {
        return frame_ticks - GetWaitTicks(render_profile);
    }

    static void BuildStructure(dmProfile::HProfile profile, RenderProfile* render_profile)
    {
        render_profile->m_NowTick = dmProfile::GetNowTicks();
        render_profile->m_BuildFrame->m_FrameTime = dmProfile::GetFrameTime();
        render_profile->m_BuildFrame->m_MaxFrameTime = dmProfile::GetMaxFrameTime();
        render_profile->m_OutOfScopes = dmProfile::IsOutOfScopes();
        render_profile->m_OutOfSamples = dmProfile::IsOutOfSamples();

        dmProfile::IterateScopeData(profile, render_profile, BuildScope);
        dmProfile::IterateSamples(profile, render_profile, BuildSampleSum);
        dmProfile::IterateCounterData(profile, render_profile, BuildCounter);

        render_profile->m_BuildFrame->m_WaitTime = GetWaitTime(render_profile);
    }

    static void PurgeStructure(RenderProfile* render_profile)
    {
        ProfileFrame* frame = render_profile->m_BuildFrame;

        uint64_t ticks_per_second = render_profile->m_TicksPerSecond;
        uint64_t now_tick = render_profile->m_NowTick;
        uint64_t life_time = render_profile->m_LifeTime;

        uint32_t scope_count = render_profile->m_ScopeLookup.Size();
        uint32_t sample_sum_count = render_profile->m_SampleSumLookup.Size();
        uint32_t counter_count = render_profile->m_CounterLookup.Size();

        const double MIN_ELAPSED_TIME = 0.00005;    // Don't show measurements that are less than a 50 microseconds...

        for (uint32_t i = 0; i < scope_count; ++i)
        {
            Scope* scope = &frame->m_Scopes[i];
            ScopeStats* scope_stats = &render_profile->m_ScopeStats[i];

            scope_stats->m_FilteredElapsed = (scope_stats->m_FilteredElapsed * 31 + scope->m_Elapsed) / 32u;

            if (scope->m_Count == 0)
            {
                continue;
            }

            double e = ((double)(scope->m_Elapsed)) / ticks_per_second;
            if (e < MIN_ELAPSED_TIME)
                continue;

            scope_stats->m_LastSeenTick = now_tick;
        }

        for (uint32_t i = 0; i < sample_sum_count; ++i)
        {
            SampleSum* sample_sum = &frame->m_SampleSums[i];
            SampleSumStats* sample_sum_stats = &render_profile->m_SampleSumStats[i];

            sample_sum_stats->m_FilteredElapsed = (sample_sum_stats->m_FilteredElapsed * 31 + sample_sum->m_Elapsed) / 32u;

            if (sample_sum->m_Count == 0u)
            {
                continue;
            }

            double e = ((double)(sample_sum->m_Elapsed)) / ticks_per_second;
            if (e < MIN_ELAPSED_TIME)
                continue;

            sample_sum_stats->m_LastSeenTick = now_tick;
        }

        for (uint32_t i = 0; i < counter_count; ++i)
        {
            Counter* counter = &frame->m_Counters[i];

            if (counter->m_Count == 0)
            {
                continue;
            }
            CounterStats* counter_stats = &render_profile->m_CounterStats[i];

            counter_stats->m_LastSeenTick = now_tick;
        }

        {
            uint32_t i = 0;
            while (i < scope_count)
            {
                ScopeStats* scope_stats = &render_profile->m_ScopeStats[i];

                bool purge = (scope_stats->m_LastSeenTick + life_time) <= now_tick;

                if (purge)
                {
                    FreeScope(render_profile, i);
                    --scope_count;
                }
                else
                {
                    ++i;
                }
            }
        }

        {
            uint32_t i = 0;
            while (i < sample_sum_count)
            {
                SampleSum* sample_sum = &frame->m_SampleSums[i];
                TIndex* scope_index_ptr = render_profile->m_ScopeLookup.Get(sample_sum->m_ScopeHash);
                
                SampleSumStats* sample_sum_stats = &render_profile->m_SampleSumStats[i];
                bool purge = scope_index_ptr == 0x0 || (sample_sum_stats->m_LastSeenTick + life_time) <= now_tick;

                if (purge)
                {
                    FreeSampleSum(render_profile, i);
                    --sample_sum_count;
                }
                else
                {
                    ++i;
                }
            }
        }

        {
            uint32_t i = 0;
            while (i < counter_count)
            {
                CounterStats* counter_stats = &render_profile->m_CounterStats[i];

                bool purge = (counter_stats->m_LastSeenTick + life_time) <= now_tick;

                if (purge)
                {
                    FreeCounter(render_profile, i);
                    --counter_count;
                }
                else
                {
                    ++i;
                }
            }
        }
    }

    struct ScopeSortPred
    {
        ScopeSortPred(RenderProfile* render_profile)
            : m_RenderProfile(render_profile)
        {}
        const RenderProfile* m_RenderProfile;
        bool operator ()(const Scope& a, const Scope& b) const
        {
            const TIndex a_index = *m_RenderProfile->m_ScopeLookup.Get(a.m_NameHash);
            const TIndex b_index = *m_RenderProfile->m_ScopeLookup.Get(b.m_NameHash);
            const ScopeStats& stats_a = m_RenderProfile->m_ScopeStats[a_index];
            const ScopeStats& stats_b = m_RenderProfile->m_ScopeStats[b_index];
            return stats_a.m_FilteredElapsed > stats_b.m_FilteredElapsed;
        }
    };
    struct SampleSumSortPred
    {
        SampleSumSortPred(RenderProfile* render_profile)
            : m_RenderProfile(render_profile)
        {}
        const RenderProfile* m_RenderProfile;
        bool operator ()(const SampleSum& a, const SampleSum& b) const
        {
            const TIndex a_index = *m_RenderProfile->m_SampleSumLookup.Get(a.m_NameHash);
            const TIndex b_index = *m_RenderProfile->m_SampleSumLookup.Get(b.m_NameHash);
            const SampleSumStats& stats_a = m_RenderProfile->m_SampleSumStats[a_index];
            const SampleSumStats& stats_b = m_RenderProfile->m_SampleSumStats[b_index];
            return stats_a.m_FilteredElapsed > stats_b.m_FilteredElapsed;
        }
    };

    static void SortStructure(RenderProfile* render_profile)
    {
        const ProfileFrame* frame = render_profile->m_ActiveFrame;

        {
            uint32_t scope_count = render_profile->m_ScopeLookup.Size();

            std::sort(&frame->m_Scopes[0], &frame->m_Scopes[scope_count], ScopeSortPred(render_profile));
            for (uint32_t i = 0; i < scope_count; ++i)
            {
                Scope* scope = &frame->m_Scopes[i];
                TNameHash name_hash = scope->m_NameHash;

                TIndex* old_i_ptr = render_profile->m_ScopeLookup.Get(name_hash);
                assert(old_i_ptr != 0x0);

                TIndex old_i = *old_i_ptr;

                if (old_i == i)
                {
                    continue;
                }

                TNameHash old_name_hash = frame->m_Scopes[old_i].m_NameHash;

                ScopeStats tmp = render_profile->m_ScopeStats[i];
                render_profile->m_ScopeStats[i] = render_profile->m_ScopeStats[old_i];
                render_profile->m_ScopeStats[old_i] = tmp;

                *old_i_ptr = i;
                render_profile->m_ScopeLookup.Put(old_name_hash, old_i);
            }
        }
        {
            uint32_t sample_sum_count = render_profile->m_SampleSumLookup.Size();

            std::stable_sort(&frame->m_SampleSums[0], &frame->m_SampleSums[sample_sum_count], SampleSumSortPred(render_profile));
            for (uint32_t i = 0; i < sample_sum_count; ++i)
            {
                SampleSum* sample_sum = &frame->m_SampleSums[i];
                TNameHash name_hash = sample_sum->m_NameHash;

                TIndex* old_i_ptr = render_profile->m_SampleSumLookup.Get(name_hash);
                assert(old_i_ptr != 0x0);

                TIndex old_i = *old_i_ptr;

                if (old_i == i)
                {
                    continue;
                }

                TNameHash old_name_hash = frame->m_SampleSums[old_i].m_NameHash;

                SampleSumStats tmp = render_profile->m_SampleSumStats[i];
                render_profile->m_SampleSumStats[i] = render_profile->m_SampleSumStats[old_i];
                render_profile->m_SampleSumStats[old_i] = tmp;

                *old_i_ptr = i;
                render_profile->m_SampleSumLookup.Put(old_name_hash, old_i);

                render_profile->m_SampleSumLookup.Put(sample_sum->m_NameHash, i);
            }
        }
        render_profile->m_LastSortTick = render_profile->m_NowTick;
    }

    static void GotoRecordedFrame(HRenderProfile render_profile, int recorded_frame_index)
    {
        if (render_profile->m_RecordBuffer.Size() == 0)
        {
            ResetStructure(render_profile);
            return;
        }
        if (recorded_frame_index >= render_profile->m_RecordBuffer.Size())
        {
            recorded_frame_index = render_profile->m_RecordBuffer.Size() - 1;
        }
        else if (recorded_frame_index < 0)
        {
            recorded_frame_index = 0;
        }

        ProfileSnapshot* snapshot = render_profile->m_RecordBuffer[recorded_frame_index];
        if (&snapshot->m_Frame == render_profile->m_ActiveFrame)
        {
            return;
        }

        render_profile->m_ScopeLookup.Clear();
        for (TIndex i = 0; i < snapshot->m_ScopeCount; ++i)
        {
            Scope* scope = &snapshot->m_Frame.m_Scopes[i];
            render_profile->m_ScopeLookup.Put(scope->m_NameHash, i);
            ScopeStats* scope_stats = &render_profile->m_ScopeStats[i];
            scope_stats->m_LastSeenTick = render_profile->m_NowTick;
            scope_stats->m_FilteredElapsed = scope->m_Elapsed;
        }

        render_profile->m_SampleSumLookup.Clear();
        for (TIndex i = 0; i < snapshot->m_SampleSumCount; ++i)
        {
            SampleSum* sample_sum = &snapshot->m_Frame.m_SampleSums[i];
            render_profile->m_SampleSumLookup.Put(sample_sum->m_NameHash, i);
            SampleSumStats* sample_stats = &render_profile->m_SampleSumStats[i];
            sample_stats->m_LastSeenTick = render_profile->m_NowTick;
            sample_stats->m_FilteredElapsed = sample_sum->m_Elapsed;
        }

        render_profile->m_CounterLookup.Clear();
        for (TIndex i = 0; i < snapshot->m_CounterCount; ++i)
        {
            Counter* counter = &snapshot->m_Frame.m_Counters[i];
            render_profile->m_CounterLookup.Put(counter->m_NameHash, i);
            CounterStats* counter_stats = &render_profile->m_CounterStats[i];
            counter_stats->m_LastSeenTick = render_profile->m_NowTick;
        }
        render_profile->m_SampleCount = snapshot->m_SampleCount;
        render_profile->m_PlaybackFrame = recorded_frame_index;

        render_profile->m_ActiveFrame = &snapshot->m_Frame;
    }

    static const int CHAR_HEIGHT = 16;
    static const int CHAR_BORDER = 1;
    static const int LINE_SPACING = CHAR_HEIGHT + CHAR_BORDER * 2;
    static const int BORDER_SIZE = 8;

    enum DisplayMode
    {
        DISPLAYMODE_MINIMIZED,
        DISPLAYMODE_LANDSCAPE,
        DISPLAYMODE_PROTRAIT
    };

    struct Position
    {
        Position(int x, int y)
            : x(x)
            , y(y)
        {}
        int x;
        int y;
    };

    struct Size
    {
        Size(int w, int h)
            : w(w)
            , h(h)
        {}
        int w;
        int h;
    };

    struct Area
    {
        Area(Position p, Size s)
            : p(p)
            , s(s)
        {}
        Position p;
        Size s;
    };

    static Position GetHeaderTitlePosition(DisplayMode display_mode, const Size& display_size)
    {
        return Position(BORDER_SIZE, display_size.h - BORDER_SIZE);
    }

    static Area GetProfilerArea(DisplayMode display_mode, const Size& display_size)
    {
        if (display_mode == DISPLAYMODE_MINIMIZED)
        {
            Position p(BORDER_SIZE, display_size.h - (BORDER_SIZE + CHAR_HEIGHT + CHAR_BORDER * 2));
            Size s(display_size.w - (BORDER_SIZE * 2), CHAR_HEIGHT + CHAR_BORDER * 2);
            return Area(p, s);
        }
        Position p(BORDER_SIZE, BORDER_SIZE);
        Size s(display_size.w - (BORDER_SIZE * 2), display_size.h - (BORDER_SIZE * 2));
        return Area(p, s);
    }

    static Area GetHeaderArea(DisplayMode display_mode, const Area& profiler_area)
    {
        Size s(profiler_area.s.w, CHAR_HEIGHT + CHAR_BORDER * 2);
        Position p(profiler_area.p.x, profiler_area.p.y + profiler_area.s.h - s.h);
        return Area(p, s);
    }

    static Area GetDetailsArea(DisplayMode display_mode, const Area& profiler_area, const Area& header_area)
    {
        Size s(profiler_area.s.w, profiler_area.s.h - header_area.s.h);
        Position p(profiler_area.p.x, profiler_area.p.y);
        return Area(p, s);
    }

    static const int SCOPES_NAME_WIDTH(int font_width) { return (15 * font_width); }
    static const int SCOPES_TIME_WIDTH(int font_width) { return (6 * font_width); }
    static const int SCOPES_COUNT_WIDTH(int font_width) { return (3 * font_width); }

    static Area GetScopesArea(DisplayMode display_mode, int font_width, const Area& details_area, int scopes_count, int counters_count)
    {
        const int count = dmMath::Max(scopes_count, counters_count);

        Size s(SCOPES_NAME_WIDTH(font_width) + font_width + SCOPES_TIME_WIDTH(font_width) + font_width + SCOPES_COUNT_WIDTH(font_width), LINE_SPACING * (1 + count));
        if (display_mode == DISPLAYMODE_LANDSCAPE)
        {
            Position p(details_area.p.x, details_area.p.y + details_area.s.h - s.h);
            return Area(p, s);
        }
        else if (display_mode == DISPLAYMODE_PROTRAIT)
        {
            Position p(details_area.p.x + details_area.s.w - s.w, details_area.p.y);
            return Area(p, s);
        }
        return Area(Position(0,0), Size(0,0));
    }

    static const int COUNTERS_NAME_WIDTH(int font_width) { return (12 * font_width); }
    static const int COUNTERS_COUNT_WIDTH(int font_width) { return (12 * font_width); }

    static Area GetCountersArea(DisplayMode display_mode, int font_width, const Area& details_area, int scopes_count, int counters_count)
    {
        if (display_mode == DISPLAYMODE_LANDSCAPE || display_mode == DISPLAYMODE_PROTRAIT)
        {
            const int count = dmMath::Max(scopes_count, counters_count);
            Size s(COUNTERS_NAME_WIDTH(font_width) + font_width + COUNTERS_COUNT_WIDTH(font_width), LINE_SPACING * (1 + count));
            Position p(details_area.p.x, details_area.p.y);
            return Area(p, s);
        }
        return Area(Position(0,0), Size(0,0));
    }

    static Area GetSamplesArea(DisplayMode display_mode, int font_width, const Area& details_area, const Area& scopes_area, const Area& counters_area)
    {
        if (display_mode == DISPLAYMODE_LANDSCAPE)
        {
            Size s(details_area.s.w - (scopes_area.s.w + font_width), details_area.s.h);
            Position p(scopes_area.p.x + scopes_area.s.w + font_width, details_area.p.y + details_area.s.h - s.h);
            return Area(p, s);
        }
        else if (display_mode == DISPLAYMODE_PROTRAIT)
        {
            int lower_clip = dmMath::Max(scopes_area.p.y + scopes_area.s.h, counters_area.p.y + counters_area.s.h);
            int max_height = details_area.p.y + details_area.s.h - lower_clip;
            Size s(details_area.s.w, max_height);
            Position p(details_area.p.x, details_area.p.y + details_area.s.h - s.h);
            return Area(p, s);
        }
        return Area(Position(0,0), Size(0,0));
    }

    static const int SAMPLE_FRAMES_NAME_LENGTH = 32;
    static const int SAMPLE_FRAMES_NAME_WIDTH(int font_width) { return (SAMPLE_FRAMES_NAME_LENGTH * font_width); }
    static const int SAMPLE_FRAMES_TIME_WIDTH(int font_width) { return (6 * font_width); }
    static const int SAMPLE_FRAMES_COUNT_WIDTH(int font_width) { return (3 * font_width); }

    static Area GetSampleFramesArea(DisplayMode display_mode, int font_width, const Area& samples_area)
    {
        const int offset_y = LINE_SPACING;
        const int offset_x = SAMPLE_FRAMES_NAME_WIDTH(font_width) + font_width + SAMPLE_FRAMES_TIME_WIDTH(font_width) + font_width + SAMPLE_FRAMES_COUNT_WIDTH(font_width) + font_width;

        Position p(samples_area.p.x + offset_x, samples_area.p.y);
        Size s(samples_area.s.w - offset_x, samples_area.s.h - offset_y);

        return Area(p, s);
    }

    static void FillArea(dmRender::HRenderContext render_context, const Area& a, const Vector4& col)
    {
        dmRender::Square2d(render_context, a.p.x, a.p.y, a.p.x + a.s.w, a.p.y + a.s.h, col);
    }
    
    static void DrawText(dmRender::HRenderContext render_context, dmRender::DrawTextParams& params, const Position& pos, dmRender::HFontMap font_map, const char* text)
    {
        params.m_Text = text;
        params.m_WorldTransform.setElem(3, 0, pos.x);
        params.m_WorldTransform.setElem(3, 1, pos.y + CHAR_HEIGHT);
        dmRender::DrawText(render_context, font_map, 0, 0, params);
    }

    HRenderProfile NewRenderProfile(float fps)
    {
        const uint32_t LIFETIME_IN_SECONDS = 6u;
        const uint32_t SORT_INTERVALL_IN_SECONDS = 3u;

        RenderProfile* profile = RenderProfile::New(
            fps,
            dmProfile::GetTicksPerSecond(),
            LIFETIME_IN_SECONDS,
            SORT_INTERVALL_IN_SECONDS,
            64,
            128,
            64,
            16384); // Just shy of 208 Kb

        return profile;
    }

    void UpdateRenderProfile(HRenderProfile render_profile, dmProfile::HProfile profile)
    {
        float last_frame_time = render_profile->m_ActiveFrame->m_FrameTime;
        last_frame_time -= render_profile->m_ActiveFrame->m_WaitTime;

        if (render_profile->m_Mode == PROFILER_MODE_PAUSE)
        {
            return;
        }

        ResetStructure(render_profile);
        BuildStructure(profile, render_profile);

        if (render_profile->m_Mode == PROFILER_MODE_PAUSE_ON_PEAK)
        {
            float this_frame_time = render_profile->m_BuildFrame->m_FrameTime;
            this_frame_time -= render_profile->m_BuildFrame->m_WaitTime;

            if (this_frame_time > last_frame_time)
            {
                PurgeStructure(render_profile);

                ProfileSnapshot* snapshot = MakeProfileSnapshot(
                    render_profile->m_NowTick,
                    render_profile->m_BuildFrame,
                    render_profile->m_ScopeLookup.Size(),
                    render_profile->m_SampleSumLookup.Size(),
                    render_profile->m_CounterLookup.Size(),
                    render_profile->m_SampleCount);

                FlushRecording(render_profile, 1);
                render_profile->m_RecordBuffer.SetSize(1);
                render_profile->m_RecordBuffer[0] = snapshot;
            }

            GotoRecordedFrame(render_profile, 0);
            SortStructure(render_profile);

            return;
        }

        PurgeStructure(render_profile);
        if ((render_profile->m_NowTick - render_profile->m_LastSortTick) >= render_profile->m_SortInterval)
        {
            SortStructure(render_profile);
        }

        if (render_profile->m_Mode == PROFILER_MODE_RECORD)
        {
            ProfileSnapshot* snapshot = MakeProfileSnapshot(
                render_profile->m_NowTick,
                render_profile->m_BuildFrame,
                render_profile->m_ScopeLookup.Size(),
                render_profile->m_SampleSumLookup.Size(),
                render_profile->m_CounterLookup.Size(),
                render_profile->m_SampleCount);
            if (render_profile->m_RecordBuffer.Remaining() == 0)
            {
                render_profile->m_RecordBuffer.SetCapacity(render_profile->m_RecordBuffer.Size() + (uint32_t)render_profile->m_FPS);
            }
            render_profile->m_RecordBuffer.Push(snapshot);
            render_profile->m_PlaybackFrame = (int32_t)render_profile->m_RecordBuffer.Size();

            return;
        }
    }

    void SetMode(HRenderProfile render_profile, ProfilerMode mode)
    {
        if ((render_profile->m_Mode != PROFILER_MODE_RECORD) && (mode == PROFILER_MODE_RECORD))
        {
            FlushRecording(render_profile, (uint32_t)render_profile->m_FPS);
        }
        render_profile->m_Mode = mode;
    }

    void SetViewMode(HRenderProfile render_profile, ProfilerViewMode view_mode)
    {
        render_profile->m_ViewMode = view_mode;
    }

    void SetWaitTime(HRenderProfile render_profile, bool include_wait_time)
    {
        render_profile->m_IncludeFrameWait = include_wait_time ? 1 : 0;
    }

    void AdjustShownFrame(HRenderProfile render_profile, int distance)
    {
        render_profile->m_Mode = PROFILER_MODE_PAUSE;
        if (render_profile->m_PlaybackFrame + distance > 0)
        {
            GotoRecordedFrame(render_profile, render_profile->m_PlaybackFrame + distance);
            SortStructure(render_profile);
        }
    }

    void DeleteRenderProfile(HRenderProfile render_profile)
    {
        RenderProfile::Delete(render_profile);
    }

    void Draw(HRenderProfile render_profile, dmRender::HRenderContext render_context, dmRender::HFontMap font_map)
    {
        dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(render_context);
        const Size display_size(dmGraphics::GetWindowWidth(graphics_context), dmGraphics::GetWindowHeight(graphics_context));

        dmRender::TextMetrics font_metrics;
        dmRender::GetTextMetrics(font_map, "Counters: .ygY,Scopes:012345678!", 0, false, 0, 0, &font_metrics);
        int font_width = font_metrics.m_Width / 32;

        const DisplayMode display_mode = render_profile->m_ViewMode == PROFILER_VIEW_MODE_MINIMIZED ?
            DISPLAYMODE_MINIMIZED :
            (display_size.w > display_size.h ? DISPLAYMODE_LANDSCAPE : DISPLAYMODE_PROTRAIT);
 
        const Area profiler_area = GetProfilerArea(display_mode, display_size);

        const Area header_area = GetHeaderArea(display_mode, profiler_area);
        const Area details_area = GetDetailsArea(display_mode, profiler_area, header_area);

        const TIndex scope_count = (TIndex)render_profile->m_ScopeLookup.Size();
        const TIndex counter_count = (TIndex)render_profile->m_CounterLookup.Size();
        const TIndex sample_sum_count = (TIndex)render_profile->m_SampleSumLookup.Size();

        const Area scopes_area = GetScopesArea(display_mode, font_width, details_area, scope_count, counter_count);
        const Area counters_area = GetCountersArea(display_mode, font_width, details_area, scope_count, counter_count);
        const Area samples_area = GetSamplesArea(display_mode, font_width, details_area, scopes_area, counters_area);
        const Area sample_frames_area = GetSampleFramesArea(display_mode, font_width, samples_area);

        FillArea(render_context, profiler_area, Vector4(0.1f, 0.1f, 0.1f, 0.5f));
        FillArea(render_context, sample_frames_area, Vector4(0.15f, 0.15f, 0.15f, 0.2f));

        //FillArea(render_context, profiler_area, Vector4(0.5f, 0.5f, 0.5f, 0.5f));
        //FillArea(render_context, header_area, Vector4(0.8f, 0.0f, 0.0f, 0.8f));
        //FillArea(render_context, scopes_area, Vector4(0.0f, 0.8f, 0.0f, 0.8f));
        //FillArea(render_context, counters_area, Vector4(0.0f, 0.0f, 0.8f, 0.8f));
        //FillArea(render_context, samples_area, Vector4(0.8f, 0.8f, 0.0f, 0.8f));
        //FillArea(render_context, sample_frames_area, Vector4(0.8f, 0.0f, 0.8f, 0.8f));

        char buffer[256];
        float col[3];

        dmRender::DrawTextParams params;
        params.m_FaceColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
        params.m_ShadowColor = Vector4(0.0f, 0.0f, 0.0f, 1.0f);

        uint64_t ticks_per_second = render_profile->m_TicksPerSecond;
        const ProfileFrame* frame = render_profile->m_ActiveFrame;

        float frame_time = frame->m_FrameTime;
        float max_frame_time = frame->m_MaxFrameTime;

        if (render_profile->m_IncludeFrameWait == 0)
        {
            frame_time -= frame->m_WaitTime;
            max_frame_time -= frame->m_WaitTime;
        }

        int l = DM_SNPRINTF(buffer, 256, "Frame: %6.3f Max: %6.3f", frame_time, max_frame_time);

        switch (render_profile->m_Mode)
        {
            case PROFILER_MODE_RUN:
                break;
            case PROFILER_MODE_PAUSE:
                if (render_profile->m_PlaybackFrame < 0)
                {
                    DM_SNPRINTF(&buffer[l], 256 -l, " (Paused)");
                }
                else
                {
                    DM_SNPRINTF(&buffer[l], 256 -l, " (Show: %d)", render_profile->m_PlaybackFrame);
                }
                break;
            case PROFILER_MODE_PAUSE_ON_PEAK:
                DM_SNPRINTF(&buffer[l], 256 -l, " (Peak)");
                break;
            case PROFILER_MODE_RECORD:
                DM_SNPRINTF(&buffer[l], 256 -l, " (Rec: %d)", render_profile->m_PlaybackFrame);
                break;
        }
        DrawText(render_context, params, header_area.p, font_map, buffer);

        if (render_profile->m_ViewMode == PROFILER_VIEW_MODE_MINIMIZED)
        {
            return;
        }

        {
            // Scopes
            int y = scopes_area.p.y + scopes_area.s.h;

            params.m_WorldTransform.setElem(3, 1, y);

            int name_x = scopes_area.p.x;
            int time_x = name_x + SCOPES_NAME_WIDTH(font_width) + font_width;
            int count_x = time_x + SCOPES_TIME_WIDTH(font_width) + font_width;

            params.m_FaceColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
            params.m_Text = render_profile->m_ScopeOverflow ? "*Scopes:" : "Scopes:";
            params.m_WorldTransform.setElem(3, 0, name_x);
            dmRender::DrawText(render_context, font_map, 0, 0, params);
            params.m_Text = "    ms";
            params.m_WorldTransform.setElem(3, 0, time_x);
            dmRender::DrawText(render_context, font_map, 0, 0, params);
            params.m_Text = "  #";
            params.m_WorldTransform.setElem(3, 0, count_x);
            dmRender::DrawText(render_context, font_map, 0, 0, params);

            HslToRgb2( 4/ 16.0f, 1.0f, 0.65f, col);
            params.m_FaceColor = Vector4(col[0], col[1], col[2], 1.0f);
            for (TIndex index = 0; index < scope_count; ++index)
            {
                Scope* scope = &frame->m_Scopes[index];

                y -= LINE_SPACING;

                double e = (double)(scope->m_Elapsed) / ticks_per_second;

                params.m_WorldTransform.setElem(3, 1, y);

                params.m_Text = scope->m_Name;
                params.m_WorldTransform.setElem(3, 0, name_x);
                dmRender::DrawText(render_context, font_map, 0, 0, params);

                params.m_Text = buffer;
                DM_SNPRINTF(buffer, sizeof(buffer), "%6.3f", (float)(e * 1000.0));
                params.m_WorldTransform.setElem(3, 0, time_x);
                dmRender::DrawText(render_context, font_map, 0, 0, params);

                params.m_Text = buffer;
                DM_SNPRINTF(buffer, sizeof(buffer), "%3u", scope->m_Count);
                params.m_WorldTransform.setElem(3, 0, count_x);
                dmRender::DrawText(render_context, font_map, 0, 0, params);
            }
        }
        {
            // Counters
            int y = counters_area.p.y + counters_area.s.h;

            params.m_WorldTransform.setElem(3, 1, y);

            int name_x = counters_area.p.x;
            int count_x = name_x + COUNTERS_NAME_WIDTH(font_width) + font_width;

            params.m_FaceColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
            params.m_Text = render_profile->m_CounterOverflow ? "*Counters:" : "Counters:";
            params.m_WorldTransform.setElem(3, 0, name_x);
            dmRender::DrawText(render_context, font_map, 0, 0, params);
            params.m_Text = "           #";
            params.m_WorldTransform.setElem(3, 0, count_x);
            dmRender::DrawText(render_context, font_map, 0, 0, params);

            HslToRgb2( 4/ 16.0f, 1.0f, 0.65f, col);
            params.m_FaceColor = Vector4(col[0], col[1], col[2], 1.0f);
            for (TIndex index = 0; index < counter_count; ++index)
            {
                Counter* counter = &frame->m_Counters[index];

                y -= LINE_SPACING;

                params.m_WorldTransform.setElem(3, 1, y);

                params.m_Text = counter->m_Name;
                params.m_WorldTransform.setElem(3, 0, name_x);
                dmRender::DrawText(render_context, font_map, 0, 0, params);

                params.m_Text = buffer;
                DM_SNPRINTF(buffer, sizeof(buffer), "%12u", counter->m_Count);
                params.m_WorldTransform.setElem(3, 0, count_x);
                dmRender::DrawText(render_context, font_map, 0, 0, params);
            }
        }
        {
            // Samples
            int y = samples_area.p.y + samples_area.s.h;

            params.m_WorldTransform.setElem(3, 1, y);

            int name_x = samples_area.p.x;
            int time_x = name_x + SAMPLE_FRAMES_NAME_WIDTH(font_width) + font_width;
            int count_x = time_x + SAMPLE_FRAMES_TIME_WIDTH(font_width) + font_width;
            int frames_x = sample_frames_area.p.x;

            params.m_FaceColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
            params.m_Text = render_profile->m_SampleSumOverflow ? "*Samples:" : "Samples:";
            params.m_WorldTransform.setElem(3, 0, name_x);
            dmRender::DrawText(render_context, font_map, 0, 0, params);
            params.m_Text = "    ms";
            params.m_WorldTransform.setElem(3, 0, time_x);
            dmRender::DrawText(render_context, font_map, 0, 0, params);
            params.m_Text = "  #";
            params.m_WorldTransform.setElem(3, 0, count_x);
            dmRender::DrawText(render_context, font_map, 0, 0, params);
            params.m_Text = render_profile->m_SampleOverflow ? "*Frame:" : "Frame:";
            params.m_WorldTransform.setElem(3, 0, frames_x);
            dmRender::DrawText(render_context, font_map, 0, 0, params);

            uint32_t sample_frame_width = sample_frames_area.s.w;
            const uint32_t frame_ticks = GetFrameTicks(render_profile);
            const uint32_t active_frame_ticks = GetActiveFrameTicks(render_profile, frame_ticks);
            const uint32_t frame_time = (frame_ticks == 0) ? (uint32_t)(ticks_per_second / render_profile->m_FPS) : render_profile->m_IncludeFrameWait ? frame_ticks : active_frame_ticks;
            const float tick_length = (float)(sample_frame_width) / (float)(frame_time);
            const TIndex max_sample_count = render_profile->m_MaxSampleCount;

            for (TIndex index = 0; index < sample_sum_count; ++index)
            {
                SampleSum* sample_sum = &frame->m_SampleSums[index];
                if (render_profile->m_IncludeFrameWait == 0)
                {
                    if (sample_sum->m_NameHash == FRAME_WAIT_SCOPE_NAME_HASH ||
                        sample_sum->m_NameHash == FRAME_SCOPE_NAME_HASH)
                    {
                        continue;
                    }
                }

                y -= LINE_SPACING;

                if (y < (samples_area.p.y + LINE_SPACING))
                {
                    break;
                }

                uint32_t color_index = (sample_sum->m_NameHash >> 6) & 0x1f;
                HslToRgb2( color_index / 31.0f, 1.0f, 0.65f, col);

                double e = ((double)(sample_sum->m_Elapsed)) / ticks_per_second;

                params.m_WorldTransform.setElem(3, 1, y);
                params.m_FaceColor = Vector4(col[0], col[1], col[2], 1.0f);

                params.m_Text = buffer;

                Scope* scope = &frame->m_Scopes[*render_profile->m_ScopeLookup.Get(sample_sum->m_ScopeHash)];
                DM_SNPRINTF(buffer, SAMPLE_FRAMES_NAME_LENGTH, "%s.%s", scope->m_Name, sample_sum->m_Name);
                params.m_WorldTransform.setElem(3, 0, name_x);
                dmRender::DrawText(render_context, font_map, 0, 0, params);

                DM_SNPRINTF(buffer, sizeof(buffer), "%6.3f", (float)(e * 1000.0));
                params.m_WorldTransform.setElem(3, 0, time_x);
                dmRender::DrawText(render_context, font_map, 0, 0, params);

                DM_SNPRINTF(buffer, sizeof(buffer), "%3u", sample_sum->m_Count);
                params.m_WorldTransform.setElem(3, 0, count_x);
                dmRender::DrawText(render_context, font_map, 0, 0, params);

                TIndex sample_index = sample_sum->m_LastSampleIndex;
                while (sample_index != max_sample_count)
                {
                    Sample* sample = &frame->m_Samples[sample_index];
                    
                    float x = frames_x + sample->m_StartTick * tick_length;
                    float w = sample->m_Elapsed * tick_length;

                    dmRender::Square2d(render_context, x, y - CHAR_HEIGHT, x + w, y, Vector4(col[0], col[1], col[2], 1));

                    sample_index = sample->m_PreviousSampleIndex;
                }
            }
        }
    }


}
