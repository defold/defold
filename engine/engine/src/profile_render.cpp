#include "profile_render.h"

#include <string.h>

#include <algorithm>

#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/hashtable.h>
#include <dlib/math.h>
#include <dlib/profile.h>
#include <render/font_renderer.h>

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
        TNameHash m_NameHash;   // m_Name + m_Scope->m_Name
        TNameHash m_ScopeHash;  // m_Scope->m_Name
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
        uint16_t m_ColorIndex;
    };

    struct CounterStats
    {
        uint64_t m_LastSeenTick;
    };

    struct RenderProfile {
        static RenderProfile* New(
            float fps,
            TIndex max_scope_count,
            TIndex max_sample_sum_count,
            TIndex max_counter_count,
            TIndex max_sample_count);
        static void Delete(RenderProfile* render_profile);

        ProfileFrame* m_BuildFrame;
        const ProfileFrame* m_ActiveFrame;
        dmArray<ProfileSnapshot*> m_RecordBuffer;

        ProfilerMode m_Mode;
        ProfilerViewMode m_ViewMode;
        float m_FPS;
        TIndexLookupTable m_ScopeLookup;
        TIndexLookupTable m_SampleSumLookup;
        TIndexLookupTable m_CounterLookup;

        ScopeStats* m_ScopeStats;
        SampleSumStats* m_SampleSumStats;
        CounterStats* m_CounterStats;

        uint64_t m_NowTick;
        uint64_t m_LastSortTick;
        uint32_t m_TicksPerSecond;
        uint32_t m_LifeTime;
        uint32_t m_SortInterval;
        uint16_t m_ColorIndex;

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
        : m_BuildFrame(build_frame)
        , m_ActiveFrame(m_BuildFrame)
        , m_Mode(PROFILER_MODE_RUN)
        , m_ViewMode(PROFILER_VIEW_MODE_FULL)
        , m_FPS(fps)
        , m_ScopeLookup(scope_lookup_data, (max_scope_count * 2) / 3, max_scope_count * 2)
        , m_SampleSumLookup(sample_sum_lookup_data, (max_sample_sum_count * 2) / 3, max_sample_sum_count * 2)
        , m_CounterLookup(counter_lookup_data, (max_counter_count * 2) / 3, max_counter_count * 2)
        , m_ScopeStats(scope_stats)
        , m_SampleSumStats(sample_sum_stats)
        , m_CounterStats(counter_stats)
        , m_NowTick(0u)
        , m_LastSortTick(0u)
        , m_TicksPerSecond(0u)
        , m_LifeTime(0u)
        , m_SortInterval(0u)
        , m_ColorIndex(0u)
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

    RenderProfile* RenderProfile::New(float fps, TIndex max_scope_count, TIndex max_sample_sum_count, TIndex max_counter_count, TIndex max_sample_count)
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
            render_profile->m_ScopeLookup.Put(name_hash, new_index);
            index_ptr = render_profile->m_ScopeLookup.Get(name_hash);
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
            render_profile->m_SampleSumLookup.Put(name_hash, new_index);
            index_ptr = render_profile->m_SampleSumLookup.Get(name_hash);
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
            sample_sum_stats->m_ColorIndex = render_profile->m_ColorIndex++;
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
            render_profile->m_CounterLookup.Put(name_hash, new_index);
            index_ptr = render_profile->m_CounterLookup.Get(name_hash);
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

    static float GetWaitTime(HRenderProfile render_profile)
    {
        const TNameHash FRAME_WAIT_SCOPE_NAME_HASH = 3155412768u;
        TIndex* wait_time_ptr = render_profile->m_SampleSumLookup.Get(FRAME_WAIT_SCOPE_NAME_HASH);
        if (wait_time_ptr != 0x0)
        {
            SampleSum* sample_sum = &render_profile->m_ActiveFrame->m_SampleSums[*wait_time_ptr];
            uint64_t ticks_per_second = render_profile->m_TicksPerSecond;
            double elapsed_s = (double)(sample_sum->m_Elapsed) / ticks_per_second;
            float elapsed_ms = (float)(elapsed_s * 1000.0);
            return elapsed_ms;
        }
        return 0.0f;
    }

    static void BuildStructure(dmProfile::HProfile profile, RenderProfile* render_profile)
    {
        const uint32_t LIFETIME_IN_SECONDS = 6u;
        const uint32_t SORT_INTERVALL_IN_SECONDS = 3u;

        render_profile->m_NowTick = dmProfile::GetNowTicks();
        render_profile->m_TicksPerSecond = dmProfile::GetTicksPerSecond();
        render_profile->m_LifeTime = (uint32_t)(render_profile->m_TicksPerSecond * LIFETIME_IN_SECONDS);
        render_profile->m_SortInterval = (uint32_t)(render_profile->m_TicksPerSecond * SORT_INTERVALL_IN_SECONDS);
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
            if (e < 0.00005)   // Don't show measurements that are less than a 50 microseconds...
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
            if (e < 0.00005)   // Don't show measurements that are less than a 50 microseconds...
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
        if ((render_profile->m_NowTick - render_profile->m_LastSortTick) < render_profile->m_SortInterval)
        {
            return;
        }

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

            std::sort(&frame->m_SampleSums[0], &frame->m_SampleSums[sample_sum_count], SampleSumSortPred(render_profile));
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
            sample_stats->m_ColorIndex = i;
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

    HRenderProfile NewRenderProfile(float fps)
    {
        RenderProfile* profile = RenderProfile::New(fps, 64, 128, 64, 16384); // Just shy of 208 Kb
        return profile;
    }

    void UpdateRenderProfile(HRenderProfile render_profile, dmProfile::HProfile profile)
    {
        float last_frame_time = render_profile->m_ActiveFrame->m_FrameTime;
        if (render_profile->m_IncludeFrameWait == 0)
        {
            last_frame_time -= render_profile->m_ActiveFrame->m_WaitTime;
        }

        if (render_profile->m_Mode == PROFILER_MODE_PAUSE)
        {
            return;
        }

        ResetStructure(render_profile);
        BuildStructure(profile, render_profile);

        if (render_profile->m_Mode == PROFILER_MODE_PAUSE_ON_PEAK)
        {
            float this_frame_time = render_profile->m_BuildFrame->m_FrameTime;
            if (render_profile->m_IncludeFrameWait == 0)
            {
                this_frame_time -= render_profile->m_BuildFrame->m_WaitTime;
            }

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
        SortStructure(render_profile);

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
        GotoRecordedFrame(render_profile, render_profile->m_PlaybackFrame + distance);
    }

    void DeleteRenderProfile(HRenderProfile render_profile)
    {
        RenderProfile::Delete(render_profile);
    }

    void Draw(HRenderProfile render_profile, dmRender::HRenderContext render_context, dmRender::HFontMap font_map)
    {
        const int g_Border_Spacing = 8;
        const int g_Scope_x0 = g_Border_Spacing;
        const int g_Scope_Time_x0 = g_Scope_x0 + 120;
        const int g_Scope_Count_x0 = g_Scope_Time_x0 + 64;
        const int g_Sample_x0 = g_Scope_Count_x0 + 40;
        const int g_Sample_Time_x0 = g_Sample_x0 + 252;
        const int g_Sample_Count_x0 = g_Sample_Time_x0 + 64;
        const int g_Sample_Bar_Height = 14;
        const int g_Sample_Frame_x0 = g_Sample_Count_x0 + 40;
        const int g_Spacing = 18;
        const int g_Counter_x0 = g_Border_Spacing;
        const int g_Counter_Amount_x0 = g_Counter_x0 + 112;

        dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(render_context);
        uint32_t display_width = dmGraphics::GetWindowWidth(graphics_context);
        uint32_t display_height = dmGraphics::GetWindowHeight(graphics_context);
        float bkg_start_y = (render_profile->m_ViewMode == PROFILER_VIEW_MODE_MINIMIZED) ? (display_height - g_Spacing * 1.5) : 0.0f;
        dmRender::Square2d(render_context, 0.0f, bkg_start_y, display_width, display_height, Vector4(0.1f, 0.1f, 0.1f, 0.5f));

        int y0 = display_height - g_Border_Spacing;

        char buffer[256];
        float col[3];

        dmRender::DrawTextParams params;
        params.m_Text = buffer;
        params.m_WorldTransform.setElem(3, 1, y0);
        params.m_FaceColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
        params.m_ShadowColor = Vector4(0.0f, 0.0f, 0.0f, 1.0f);

        {
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

            params.m_WorldTransform.setElem(3, 0, g_Scope_x0);
            dmRender::DrawText(render_context, font_map, 0, 0, params);

            if (render_profile->m_ViewMode == PROFILER_VIEW_MODE_MINIMIZED)
            {
                return;
            }

            y0 -= g_Spacing;
            dmRender::Square2d(render_context, g_Sample_Frame_x0, y0 - g_Spacing, display_width - g_Border_Spacing, 0, Vector4(0.15f, 0.15f, 0.15f, 0.2f));

            params.m_WorldTransform.setElem(3, 1, y0);

            params.m_Text = render_profile->m_ScopeOverflow ? "*Scopes:" : "Scopes:";
            params.m_WorldTransform.setElem(3, 0, g_Scope_x0);
            dmRender::DrawText(render_context, font_map, 0, 0, params);
            params.m_Text = "    ms";
            params.m_WorldTransform.setElem(3, 0, g_Scope_Time_x0);
            dmRender::DrawText(render_context, font_map, 0, 0, params);
            params.m_Text = "  #";
            params.m_WorldTransform.setElem(3, 0, g_Scope_Count_x0);
            dmRender::DrawText(render_context, font_map, 0, 0, params);
            params.m_Text = render_profile->m_SampleSumOverflow ? "*Samples:" : "Samples:";
            params.m_WorldTransform.setElem(3, 0, g_Sample_x0);
            dmRender::DrawText(render_context, font_map, 0, 0, params);
            params.m_Text = "    ms";
            params.m_WorldTransform.setElem(3, 0, g_Sample_Time_x0);
            dmRender::DrawText(render_context, font_map, 0, 0, params);
            params.m_Text = "  #";
            params.m_WorldTransform.setElem(3, 0, g_Sample_Count_x0);
            dmRender::DrawText(render_context, font_map, 0, 0, params);
            params.m_Text = render_profile->m_SampleOverflow ? "*Frame:" : "Frame:";
            params.m_WorldTransform.setElem(3, 0, g_Sample_Frame_x0);
            dmRender::DrawText(render_context, font_map, 0, 0, params);

            TIndex counter_count = (TIndex)render_profile->m_CounterLookup.Size();
            int y = (counter_count + 1) * g_Spacing;

            params.m_WorldTransform.setElem(3, 1, y);

            params.m_Text = render_profile->m_CounterOverflow ? "*Counters:" : "Counters:";
            params.m_WorldTransform.setElem(3, 0, g_Counter_x0);
            dmRender::DrawText(render_context, font_map, 0, 0, params);
            params.m_Text = "           #";
            params.m_WorldTransform.setElem(3, 0, g_Counter_Amount_x0);
            dmRender::DrawText(render_context, font_map, 0, 0, params);

            int start_y = y0;

            bool profile_valid = true;
            if (render_profile->m_OutOfScopes)
            {
                profile_valid = false;
                params.m_WorldTransform.setElem(3, 0, g_Scope_x0);
                params.m_WorldTransform.setElem(3, 1, y0 - g_Spacing);
                params.m_FaceColor = Vector4(1.0f, 0.2f, 0.1f, 1.0f);
                params.m_Text = "Out of scopes!";
                dmRender::DrawText(render_context, font_map, 0, 0, params);
            }
            if (render_profile->m_OutOfSamples)
            {
                profile_valid = false;
                params.m_WorldTransform.setElem(3, 0, g_Sample_x0);
                params.m_WorldTransform.setElem(3, 1, y0 - g_Spacing);
                params.m_FaceColor = Vector4(1.0f, 0.2f, 0.1f, 1.0f);
                params.m_Text = "Out of samples!";
                dmRender::DrawText(render_context, font_map, 0, 0, params);
            }

            const bool draw_scopes = profile_valid;
            const bool draw_samples = profile_valid;
            const bool draw_counters = profile_valid;

            if (draw_scopes)
            {
                start_y = y0 - g_Spacing;
                TIndex scope_count = (TIndex)render_profile->m_ScopeLookup.Size();
                for (TIndex index = 0; index < scope_count; ++index)
                {
                    Scope* scope = &frame->m_Scopes[index];
                    int y = start_y - index * g_Spacing;

                    HslToRgb2( 4/ 16.0f, 1.0f, 0.65f, col);

                    double e = (double)(scope->m_Elapsed) / ticks_per_second;

                    params.m_WorldTransform.setElem(3, 1, y);
                    params.m_FaceColor = Vector4(col[0], col[1], col[2], 1.0f);

                    params.m_Text = scope->m_Name;
                    params.m_WorldTransform.setElem(3, 0, g_Scope_x0);
                    dmRender::DrawText(render_context, font_map, 0, 0, params);

                    params.m_Text = buffer;
                    DM_SNPRINTF(buffer, sizeof(buffer), "%6.3f", (float)(e * 1000.0));
                    params.m_WorldTransform.setElem(3, 0, g_Scope_Time_x0);
                    dmRender::DrawText(render_context, font_map, 0, 0, params);

                    params.m_Text = buffer;
                    DM_SNPRINTF(buffer, sizeof(buffer), "%3u", scope->m_Count);
                    params.m_WorldTransform.setElem(3, 0, g_Scope_Count_x0);
                    dmRender::DrawText(render_context, font_map, 0, 0, params);
                }
            }

            if (draw_samples)
            {
                start_y = y0 - g_Spacing;
                uint32_t sample_frame_width = display_width - g_Sample_Frame_x0 - g_Border_Spacing;
                TIndex sample_sum_count = (TIndex)render_profile->m_SampleSumLookup.Size();
                for (TIndex index = 0; index < sample_sum_count; ++index)
                {
                    SampleSum* sample_sum = &frame->m_SampleSums[index];
                    SampleSumStats* sample_sum_stats = &render_profile->m_SampleSumStats[index];

                    HslToRgb2( (sample_sum_stats->m_ColorIndex % 16) / 15.0f, 1.0f, 0.6f, col);

                    double e = ((double)(sample_sum->m_Elapsed)) / ticks_per_second;

                    float y = start_y - (index * g_Spacing);

                    params.m_WorldTransform.setElem(3, 1, y);
                    params.m_FaceColor = Vector4(col[0], col[1], col[2], 1.0f);

                    params.m_Text = buffer;

                    Scope* scope = &frame->m_Scopes[*render_profile->m_ScopeLookup.Get(sample_sum->m_ScopeHash)];
                    DM_SNPRINTF(buffer, sizeof(buffer), "%s.%s", scope->m_Name, sample_sum->m_Name);
                    params.m_WorldTransform.setElem(3, 0, g_Sample_x0);
                    dmRender::DrawText(render_context, font_map, 0, 0, params);

                    DM_SNPRINTF(buffer, sizeof(buffer), "%6.3f", (float)(e * 1000.0));
                    params.m_WorldTransform.setElem(3, 0, g_Sample_Time_x0);
                    dmRender::DrawText(render_context, font_map, 0, 0, params);

                    DM_SNPRINTF(buffer, sizeof(buffer), "%3u", sample_sum->m_Count);
                    params.m_WorldTransform.setElem(3, 0, g_Sample_Count_x0);
                    dmRender::DrawText(render_context, font_map, 0, 0, params);

                    TIndex sample_index = sample_sum->m_LastSampleIndex;
                    while (sample_index != render_profile->m_MaxSampleCount)
                    {
                        Sample* sample = &frame->m_Samples[sample_index];
                        
                        float frames_per_tick = render_profile->m_FPS / ticks_per_second;
                        float x = g_Sample_Frame_x0 + sample_frame_width * sample->m_StartTick * frames_per_tick;
                        float w = sample_frame_width * sample->m_Elapsed * frames_per_tick;

                        dmRender::Square2d(render_context, x, y - g_Sample_Bar_Height, x + w, y, Vector4(col[0], col[1], col[2], 1));

                        sample_index = sample->m_PreviousSampleIndex;
                    }
                }
            }

            if (draw_counters)
            {
                TIndex counter_count = (TIndex)render_profile->m_CounterLookup.Size();
                start_y = counter_count * g_Spacing;

                for (TIndex index = 0; index < counter_count; ++index)
                {
                    Counter* counter = &frame->m_Counters[index];

                    HslToRgb2( 4/ 16.0f, 1.0f, 0.65f, col);

                    float y = start_y - (index * g_Spacing);

                    params.m_WorldTransform.setElem(3, 1, y);
                    params.m_FaceColor = Vector4(col[0], col[1], col[2], 1.0f);

                    params.m_Text = counter->m_Name;
                    params.m_WorldTransform.setElem(3, 0, g_Counter_x0);
                    dmRender::DrawText(render_context, font_map, 0, 0, params);

                    params.m_Text = buffer;
                    DM_SNPRINTF(buffer, sizeof(buffer), "%12u", counter->m_Count);
                    params.m_WorldTransform.setElem(3, 0, g_Counter_Amount_x0);
                    dmRender::DrawText(render_context, font_map, 0, 0, params);
                }
            }        
        }
    }
}
