#include "profile_render.h"

#include <string.h>

#include <algorithm>

#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/hashtable.h>
#include <dlib/index_pool.h>
#include <dlib/math.h>
#include <dlib/profile.h>
#include <render/font_renderer.h>
#include <render/font_ddf.h>

namespace dmProfileRender
{
/**
 * The on-screen profiler stores a copy of a profiled frame locally and tries
 * to be as efficitent as possible by only storing the necessary data.
 * 
 * It also has a recording buffer where recorded frames are stored.
 * 
 * A base frame (ProfileFrame) is a struct containing all the data related to
 * a sampled frame. It tries to be as minimal as possible, ie it does not
 * contain the actual number of scopes and samples etc in this basic struct.
 * 
 * The base frame (ProfileFrame) extended by either a ProfileSnapshot which
 * is the data structure used for recorded profile frames and includes the
 * number of scopes and counters etc in the frame.
 * 
 * The profiler (RenderProfile) also has a ProfileFrame but instead of direct
 * numbers for scopes and counters count it derives the counts from its lookup tables.
 * 
 * Extra data such as filtered values for elapsed times and "last seen" information
 * is separated and only present in the RenderProfile, not in the actual frames.
 * 
 * The lookup-by-hash and allocation information for data in the RenderProfile is
 * built on demand as a new frame is built from a frame sample on the go or when
 * picking up and displaying a frame from the recording buffer.
 * 
 * The RenderProfile is created with a pre-defined number of sample/scopes/counters
 * available and no memory allocation is done when processing a frame.
 * 
 * When recording profile frames only one malloc() call is done per recorded frame
 * to reduce overhead and keep the data compact.
 * 
 * The profiler sorts the data on filtered values and keeps the coloring of sample
 * names consistent in an effort to reduce flickering and samples moving around to fast.
 * 
 * It also automatically adjust the layout depending on the aspect ratio of the
 * profiler display area (which is directly derived from the window/screen size).
 * 
 * Some terms:
 *  -   Scope           High level scope such as "Engine" and "Script", the first parameter to DM_PROFILE(Physics, ...)
 *  -   Counter         A counter - ie DM_COUNTER("DrawCalls", 1)
 *  -   Sample          A single sample of a ProfilerScope, ie DM_PROFILE(Physics, "UpdateKinematic")
 *  -   SampleSum       The summation of all the samples for a ProfilerScope, ie DM_PROFILE(Physics, "UpdateKinematic")
 *                      it also has a list of all the Sample instances for the ProfileScope
 *  -   ProfileFrame    Information (Scopes, Counters, Samples, SampleSums etc) of a single profile frame
 *  -   ProfileSnapshot A recorded profile frame, embedds a ProfileFrame
 *  -   RenderProfile   The on-screen profiler instance, embeds a ProfileFrame and holds lookup tables and allocation
 *                      buffers for sampled data. It also holds information of filtered values, sorting and a
 *                      recording buffer (array of ProfileSnapshot)
 */
    static const int CHAR_HEIGHT  = 16;
    static const int CHAR_WIDTH   = 8;
    static const int CHAR_BORDER  = 1;
    static const int LINE_SPACING = CHAR_HEIGHT + CHAR_BORDER * 2;
    static const int BORDER_SIZE  = 8;

    static const int SCOPES_NAME_WIDTH  = 17 * CHAR_WIDTH;
    static const int SCOPES_TIME_WIDTH  = 6 * CHAR_WIDTH;
    static const int SCOPES_COUNT_WIDTH = 3 * CHAR_WIDTH;

    static const int PORTRAIT_SAMPLE_FRAMES_NAME_LENGTH = 40;
    static const int LANDSCAPE_SAMPLE_FRAMES_NAME_LENGTH = 60;
    static const int PORTRAIT_SAMPLE_FRAMES_NAME_WIDTH  = PORTRAIT_SAMPLE_FRAMES_NAME_LENGTH * CHAR_WIDTH;
    static const int LANDSCAPE_SAMPLE_FRAMES_NAME_WIDTH  = LANDSCAPE_SAMPLE_FRAMES_NAME_LENGTH * CHAR_WIDTH;
    static const int SAMPLE_FRAMES_TIME_WIDTH  = 6 * CHAR_WIDTH;
    static const int SAMPLE_FRAMES_COUNT_WIDTH = 3 * CHAR_WIDTH;

    static const int COUNTERS_NAME_WIDTH  = 15 * CHAR_WIDTH;
    static const int COUNTERS_COUNT_WIDTH = 12 * CHAR_WIDTH;

    enum DisplayMode
    {
        DISPLAYMODE_MINIMIZED,
        DISPLAYMODE_LANDSCAPE,
        DISPLAYMODE_PORTRAIT
    };

    using namespace Vectormath::Aos;

    static const Vector4 PROFILER_BG_COLOR  = Vector4(0.1f, 0.1f, 0.1f, 0.6f);
    static const Vector4 TITLE_FACE_COLOR   = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    static const Vector4 TITLE_SHADOW_COLOR = Vector4(0.0f, 0.0f, 0.0f, 1.0f);
    static const Vector4 SAMPLES_BG_COLOR   = Vector4(0.15f, 0.15f, 0.15f, 0.2f);

    typedef uint32_t TIndex;
    typedef uint32_t TNameHash;

    static TNameHash GetCombinedHash(TNameHash scope_hash, TNameHash sample_hash)
    {
        TNameHash hash = scope_hash ^ (sample_hash + 0x9e3779b9 + (scope_hash << 6) + (scope_hash >> 2));
        return hash;
    }

    // These hashes are used so we can filter out the time the engine is waiting for frame buffer flip
    static const TNameHash VSYNC_WAIT_NAME_HASH   = GetCombinedHash(dmProfile::GetNameHash("VSync"), dmProfile::GetNameHash("Wait"));
    static const TNameHash ENGINE_FRAME_NAME_HASH = GetCombinedHash(dmProfile::GetNameHash("Engine"), dmProfile::GetNameHash("Frame"));

    //  float *r, *g, *b; /* red, green, blue in [0,1] */
    //  float h, s, l;    /* hue in [0,360]; saturation, light in [0,1] */
    static void hsl_to_rgb(float* r, float* g, float* b, float h, float s, float l)
    {
        float c      = (1.0f - dmMath::Abs(2.0f * l - 1.0f)) * s;
        float hp     = h / 60.0f;
        int hpi      = (int)hp;
        float hpmod2 = (hpi % 2) + (hp - hpi);
        float x      = c * (1.0f - dmMath::Abs(hpmod2 - 1.0f));
        switch (hpi)
        {
            case 0:
                *r = c;
                *g = x;
                *b = 0;
                break;
            case 1:
                *r = x;
                *g = c;
                *b = 0;
                break;
            case 2:
                *r = 0;
                *g = c;
                *b = x;
                break;
            case 3:
                *r = 0;
                *g = x;
                *b = c;
                break;
            case 4:
                *r = x;
                *g = 0;
                *b = c;
                break;
            case 5:
                *r = c;
                *g = 0;
                *b = x;
                break;
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

    struct Scope
    {
        uint32_t m_Elapsed;
        uint32_t m_Count;
        TNameHash m_NameHash;
    };

    struct SampleSum
    {
        uint32_t m_Elapsed;
        uint32_t m_Count;
        TNameHash m_SampleNameHash;
        TNameHash m_ScopeNameHash;
        TIndex m_LastSampleIndex;
    };

    struct Counter
    {
        uint32_t m_Count;
        TNameHash m_NameHash;
    };

    struct Sample
    {
        uint32_t m_StartTick;
        uint32_t m_Elapsed;
        TIndex m_PreviousSampleIndex;
    };

    typedef dmHashTable<TNameHash, TIndex> TIndexLookupTable;
    typedef dmHashTable<TNameHash, const char*> TNameLookupTable;

    // The raw data of a frame, length of arrays are omitted since we use this block
    // either by building a ProfileSnapshot where we have the lengths explicitly or
    // we use it in RenderProfile which has the length of the arrays implicitly
    // via hash tables for name-hash -> item
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

    // The complicated allocation of a frame is to make sure we only do one allocation per frame
    static size_t ProfileFrameSize(TIndex max_scope_count, TIndex max_sample_sum_count, TIndex max_counter_count, TIndex max_sample_count)
    {
        const size_t scopes_size      = sizeof(Scope) * max_scope_count;
        const size_t sample_sums_size = sizeof(SampleSum) * max_sample_sum_count;
        const size_t counters_size    = sizeof(Counter) * max_counter_count;
        const size_t samples_size     = sizeof(Sample) * max_sample_count;

        size_t size = sizeof(ProfileFrame) +
        scopes_size +
        sample_sums_size +
        counters_size +
        samples_size;

        return size;
    }

    static ProfileFrame* CreateProfileFrame(void* mem, TIndex max_scope_count, TIndex max_sample_sum_count, TIndex max_counter_count, TIndex max_sample_count)
    {
        const size_t scopes_size      = sizeof(Scope) * max_scope_count;
        const size_t sample_sums_size = sizeof(SampleSum) * max_sample_sum_count;
        const size_t counters_size    = sizeof(Counter) * max_counter_count;
        const size_t samples_size     = sizeof(Sample) * max_sample_count;

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

    // A snapshot of a frame without efficient lookups, focuses on keeping it
    // small. When presenting a frame the lookup tables are rebuilt from the raw data
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

    // The complicated allocation of a snapshot is to make sure we only do one allocation per frame
    static size_t ProfileSnapshotSize(TIndex scope_count, TIndex sample_sum_count, TIndex counter_count, TIndex sample_count)
    {
        const size_t scopes_size      = sizeof(Scope) * scope_count;
        const size_t sample_sums_size = sizeof(SampleSum) * sample_sum_count;
        const size_t counters_size    = sizeof(Counter) * counter_count;
        const size_t samples_size     = sizeof(Sample) * sample_count;

        size_t size = sizeof(ProfileSnapshot) +
        scopes_size +
        sample_sums_size +
        counters_size +
        samples_size;

        return size;
    }

    static ProfileSnapshot* CreateProfileSnapshot(void* mem, uint64_t frame_tick, TIndex scope_count, TIndex sample_sum_count, TIndex counter_count, TIndex sample_count)
    {
        const size_t scopes_size      = sizeof(Scope) * scope_count;
        const size_t sample_sums_size = sizeof(SampleSum) * sample_sum_count;
        const size_t counters_size    = sizeof(Counter) * counter_count;
        const size_t samples_size     = sizeof(Sample) * sample_count;

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

    static ProfileSnapshot* MakeProfileSnapshot(uint64_t frame_tick,
                                                const ProfileFrame* frame,
                                                TIndex scope_count,
                                                const TIndex* scope_indexes,
                                                TIndex sample_sum_count,
                                                const TIndex* sample_sum_indexes,
                                                TIndex counter_count,
                                                const TIndex* counter_indexes,
                                                TIndex sample_count)
    {
        void* mem                 = malloc(ProfileSnapshotSize(scope_count, sample_sum_count, counter_count, sample_count));
        ProfileSnapshot* snapshot = CreateProfileSnapshot(mem, frame_tick, scope_count, sample_sum_count, counter_count, sample_count);
        for (TIndex i = 0; i < scope_count; ++i)
        {
            snapshot->m_Frame.m_Scopes[i] = frame->m_Scopes[scope_indexes[i]];
        }
        for (TIndex i = 0; i < sample_sum_count; ++i)
        {
            snapshot->m_Frame.m_SampleSums[i] = frame->m_SampleSums[sample_sum_indexes[i]];
        }
        for (TIndex i = 0; i < counter_count; ++i)
        {
            snapshot->m_Frame.m_Counters[i] = frame->m_Counters[counter_indexes[i]];
        }
        memcpy(snapshot->m_Frame.m_Samples, frame->m_Samples, sizeof(Sample) * sample_count);
        snapshot->m_Frame.m_FrameTime    = frame->m_FrameTime;
        snapshot->m_Frame.m_WaitTime     = frame->m_WaitTime;
        snapshot->m_Frame.m_MaxFrameTime = frame->m_MaxFrameTime;

        return snapshot;
    }

    // Stats from each data type is kept in separate data containers so they
    // can be easily stripped from the frame when storing them in the recording buffer
    // using the ProfileSnapshot structure
    struct ScopeStats
    {
        uint64_t m_LastSeenTick;
        uint32_t m_FilteredElapsed;
    };

    struct SampleSumStats
    {
        uint64_t m_LastSeenTick;
        uint32_t m_FilteredElapsed;
    };

    struct CounterStats
    {
        uint64_t m_LastSeenTick;
    };

    // Allocation, lookup and sorting is stored here and is used for
    // Scope, SampleSum and Counter
    struct DataLookup
    {
        DataLookup(TIndex max_count, TIndex* free_indexes_data, TIndex* sort_order_data)
            : m_HashLookup()
            , m_FreeIndexes(free_indexes_data, max_count)
            , m_SortOrder(sort_order_data)
        {
            m_HashLookup.SetCapacity((max_count * 2) / 3, max_count * 2);
        }
        TIndexLookupTable m_HashLookup;
        dmIndexPool<TIndex> m_FreeIndexes;
        TIndex* m_SortOrder;
    };

    // The RenderProfile contains the current "live" frame and
    // stats used to sort/purge sample items
    struct RenderProfile
    {
        static RenderProfile* New(
        float fps,
        uint32_t ticks_per_second,
        uint32_t lifetime_in_milliseconds,
        TIndex max_scope_count,
        TIndex max_sample_sum_count,
        TIndex max_counter_count,
        TIndex max_sample_count);
        static void Delete(RenderProfile* render_profile);

        const float m_FPS;
        const uint32_t m_TicksPerSecond;
        const uint32_t m_LifeTime;

        ProfileFrame* m_BuildFrame;
        const ProfileFrame* m_ActiveFrame;
        dmArray<ProfileSnapshot*> m_RecordBuffer;

        ProfilerMode m_Mode;
        ProfilerViewMode m_ViewMode;

        DataLookup m_ScopeLookup;
        DataLookup m_SampleSumLookup;
        DataLookup m_CounterLookup;

        TNameLookupTable m_NameLookupTable;

        ScopeStats* m_ScopeStats;
        SampleSumStats* m_SampleSumStats;
        CounterStats* m_CounterStats;

        uint64_t m_NowTick;

        TIndex m_SampleCount;

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
        uint32_t lifetime_in_milliseconds,
        TIndex max_scope_count,
        TIndex max_sample_sum_count,
        TIndex max_counter_count,
        TIndex max_sample_count,
        TIndex max_name_count,
        ScopeStats* scope_stats,
        SampleSumStats* sample_sum_stats,
        CounterStats* counter_stats,
        TIndex* scope_sort_order,
        TIndex* sample_sum_sort_order,
        TIndex* counter_sort_order_data,
        TIndex* scope_free_index_data,
        TIndex* sample_sum_free_index_data,
        TIndex* counter_free_index_data,
        ProfileFrame* build_frame);
        ~RenderProfile();
    };

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

    static Scope* GetOrCreateScope(RenderProfile* render_profile, TNameHash name_hash)
    {
        ProfileFrame* frame = render_profile->m_BuildFrame;
        TIndex* index_ptr   = render_profile->m_ScopeLookup.m_HashLookup.Get(name_hash);
        if (index_ptr != 0x0)
        {
            return &frame->m_Scopes[*index_ptr];
        }
        if (render_profile->m_ScopeLookup.m_HashLookup.Full())
        {
            return 0x0;
        }
        if (render_profile->m_ScopeLookup.m_FreeIndexes.Remaining() == 0)
        {
            return 0x0;
        }
        uint32_t scope_count = render_profile->m_ScopeLookup.m_HashLookup.Size();
        TIndex new_index     = render_profile->m_ScopeLookup.m_FreeIndexes.Pop();
        render_profile->m_ScopeLookup.m_HashLookup.Put(name_hash, new_index);
        Scope* scope      = &frame->m_Scopes[new_index];
        scope->m_NameHash = name_hash;
        scope->m_Count    = 0u;
        scope->m_Elapsed  = 0u;

        ScopeStats* scope_stats        = &render_profile->m_ScopeStats[new_index];
        scope_stats->m_FilteredElapsed = 0u;
        scope_stats->m_LastSeenTick    = render_profile->m_NowTick - render_profile->m_LifeTime;

        render_profile->m_ScopeLookup.m_SortOrder[scope_count] = new_index;
        return scope;
    }

    static void AddScope(RenderProfile* render_profile, TNameHash name_hash, uint32_t elapsed, uint32_t count)
    {
        Scope* scope = GetOrCreateScope(render_profile, name_hash);
        if (scope == 0x0)
        {
            render_profile->m_ScopeOverflow = 1;
            return;
        }
        scope->m_Elapsed += elapsed;
        scope->m_Count += count;
    }

    static void FreeScope(RenderProfile* render_profile, TIndex index)
    {
        ProfileFrame* frame = render_profile->m_BuildFrame;
        TIndex scope_index  = render_profile->m_ScopeLookup.m_SortOrder[index];
        Scope* scope        = &frame->m_Scopes[scope_index];
        render_profile->m_ScopeLookup.m_HashLookup.Erase(scope->m_NameHash);
        uint32_t new_count = render_profile->m_ScopeLookup.m_HashLookup.Size();
        render_profile->m_ScopeLookup.m_FreeIndexes.Push(scope_index);
        if (index != new_count)
        {
            // A SwapErase would be simpler but would mess with the established sort order (causing jumping entries in the profile UI)
            memmove(&render_profile->m_ScopeLookup.m_SortOrder[index], &render_profile->m_ScopeLookup.m_SortOrder[index + 1], sizeof(TIndex) * (new_count - index));
        }
    }

    static SampleSum* GetOrCreateSampleSum(RenderProfile* render_profile, TNameHash sample_name_hash, TNameHash scope_name_hash)
    {
        ProfileFrame* frame       = render_profile->m_BuildFrame;
        TNameHash sample_sum_hash = GetCombinedHash(scope_name_hash, sample_name_hash);
        TIndex* index_ptr         = render_profile->m_SampleSumLookup.m_HashLookup.Get(sample_sum_hash);
        if (index_ptr != 0x0)
        {
            return &frame->m_SampleSums[*index_ptr];
        }
        if (render_profile->m_SampleSumLookup.m_HashLookup.Full())
        {
            return 0x0;
        }
        if (render_profile->m_SampleSumLookup.m_FreeIndexes.Remaining() == 0)
        {
            return 0x0;
        }
        uint32_t sample_sum_count = render_profile->m_SampleSumLookup.m_HashLookup.Size();
        TIndex new_index          = render_profile->m_SampleSumLookup.m_FreeIndexes.Pop();
        render_profile->m_SampleSumLookup.m_HashLookup.Put(sample_sum_hash, new_index);
        SampleSum* sample_sum         = &frame->m_SampleSums[new_index];
        sample_sum->m_SampleNameHash  = sample_name_hash;
        sample_sum->m_ScopeNameHash   = scope_name_hash;
        sample_sum->m_Elapsed         = 0u;
        sample_sum->m_Count           = 0u;
        sample_sum->m_LastSampleIndex = render_profile->m_MaxSampleCount;

        SampleSumStats* sample_sum_stats    = &render_profile->m_SampleSumStats[new_index];
        sample_sum_stats->m_FilteredElapsed = 0u;
        sample_sum_stats->m_LastSeenTick    = render_profile->m_NowTick - render_profile->m_LifeTime;

        render_profile->m_SampleSumLookup.m_SortOrder[sample_sum_count] = new_index;

        return sample_sum;
    }

    static void AddSample(RenderProfile* render_profile, TNameHash sample_name_hash, TNameHash scope_name_hash, uint32_t start_tick, uint32_t elapsed)
    {
        SampleSum* sample_sum = GetOrCreateSampleSum(render_profile, sample_name_hash, scope_name_hash);
        if (sample_sum == 0x0)
        {
            render_profile->m_SampleSumOverflow = 1;
            return;
        }

        ProfileFrame* frame = render_profile->m_BuildFrame;
        if (sample_sum->m_LastSampleIndex != render_profile->m_MaxSampleCount)
        {
            Sample* last_sample = &frame->m_Samples[sample_sum->m_LastSampleIndex];
            uint32_t end_last   = last_sample->m_StartTick + last_sample->m_Elapsed;
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

        TIndex sample_index           = render_profile->m_SampleCount++;
        Sample* sample                = &frame->m_Samples[sample_index];
        sample->m_PreviousSampleIndex = sample_sum->m_LastSampleIndex;
        sample->m_StartTick           = start_tick;
        sample->m_Elapsed             = elapsed;

        sample_sum->m_LastSampleIndex = sample_index;
    }

    static void FreeSampleSum(RenderProfile* render_profile, TIndex index)
    {
        ProfileFrame* frame       = render_profile->m_BuildFrame;
        TIndex sample_sum_index   = render_profile->m_SampleSumLookup.m_SortOrder[index];
        SampleSum* sample_sum     = &frame->m_SampleSums[sample_sum_index];
        TNameHash sample_sum_hash = GetCombinedHash(sample_sum->m_ScopeNameHash, sample_sum->m_SampleNameHash);
        render_profile->m_SampleSumLookup.m_HashLookup.Erase(sample_sum_hash);
        uint32_t new_count = render_profile->m_SampleSumLookup.m_HashLookup.Size();
        render_profile->m_SampleSumLookup.m_FreeIndexes.Push(sample_sum_index);
        if (index != new_count)
        {
            // A SwapErase would be simpler but would mess with the established sort order (causing jumping entries in the profile UI)
            memmove(&render_profile->m_SampleSumLookup.m_SortOrder[index], &render_profile->m_SampleSumLookup.m_SortOrder[index + 1], sizeof(TIndex) * (new_count - index));
        }
    }

    static Counter* GetOrCreateCounter(RenderProfile* render_profile, TNameHash name_hash)
    {
        ProfileFrame* frame = render_profile->m_BuildFrame;
        TIndex* index_ptr   = render_profile->m_CounterLookup.m_HashLookup.Get(name_hash);
        if (index_ptr != 0x0)
        {
            return &frame->m_Counters[*index_ptr];
        }
        if (render_profile->m_CounterLookup.m_HashLookup.Full())
        {
            return 0x0;
        }
        if (render_profile->m_CounterLookup.m_FreeIndexes.Remaining() == 0)
        {
            return 0x0;
        }
        uint32_t counter_count = render_profile->m_CounterLookup.m_HashLookup.Size();
        TIndex new_index       = render_profile->m_CounterLookup.m_FreeIndexes.Pop();
        render_profile->m_CounterLookup.m_HashLookup.Put(name_hash, new_index);
        Counter* counter    = &frame->m_Counters[new_index];
        counter->m_NameHash = name_hash;
        counter->m_Count    = 0u;

        CounterStats* counter_stats   = &render_profile->m_CounterStats[new_index];
        counter_stats->m_LastSeenTick = render_profile->m_NowTick - render_profile->m_LifeTime;

        render_profile->m_CounterLookup.m_SortOrder[counter_count] = new_index;
        return counter;
    }

    static void AddCounter(RenderProfile* render_profile, TNameHash name_hash, uint32_t count)
    {
        Counter* counter = GetOrCreateCounter(render_profile, name_hash);
        if (counter == 0x0)
        {
            render_profile->m_CounterOverflow = 1;
            return;
        }
        counter->m_Count += count;
    }

    static void FreeCounter(RenderProfile* render_profile, TIndex index)
    {
        ProfileFrame* frame  = render_profile->m_BuildFrame;
        TIndex counter_index = render_profile->m_CounterLookup.m_SortOrder[index];
        Counter* counter     = &frame->m_Counters[counter_index];
        render_profile->m_CounterLookup.m_HashLookup.Erase(counter->m_NameHash);
        uint32_t new_count = render_profile->m_CounterLookup.m_HashLookup.Size();
        render_profile->m_CounterLookup.m_FreeIndexes.Push(counter_index);
        if (index != new_count)
        {
            // A SwapErase would be simpler but would mess with the established sort order (causing jumping entries in the profile UI)
            memmove(&render_profile->m_CounterLookup.m_SortOrder[index], &render_profile->m_CounterLookup.m_SortOrder[index + 1], sizeof(TIndex) * (new_count - index));
        }
    }

    static bool AddName(RenderProfile* render_profile, TNameHash name_hash, const char* name)
    {
        if (0x0 == render_profile->m_NameLookupTable.Get(name_hash))
        {
            if (render_profile->m_NameLookupTable.Full())
            {
                return false;
            }
            render_profile->m_NameLookupTable.Put(name_hash, name);
        }
        return true;
    }

    static void BuildScope(void* context, const dmProfile::ScopeData* scope_data)
    {
        if (scope_data->m_Count == 0)
        {
            return;
        }
        RenderProfile* render_profile = (RenderProfile*)context;
        dmProfile::Scope* scope       = scope_data->m_Scope;
        if (!AddName(render_profile, scope->m_NameHash, scope->m_Name))
        {
            render_profile->m_ScopeOverflow = 1;
            return;
        }
        AddScope(render_profile, scope->m_NameHash, scope_data->m_Elapsed, scope_data->m_Count);
    }

    static void BuildSampleSum(void* context, const dmProfile::Sample* sample)
    {
        if (sample->m_Elapsed == 0u)
        {
            return;
        }
        RenderProfile* render_profile = (RenderProfile*)context;
        if (!AddName(render_profile, sample->m_NameHash, sample->m_Name))
        {
            render_profile->m_SampleSumOverflow = 1;
            return;
        }
        AddSample(render_profile, sample->m_NameHash, sample->m_Scope->m_NameHash, sample->m_Start, sample->m_Elapsed);
    }

    static void BuildCounter(void* context, const dmProfile::CounterData* counter_data)
    {
        if (counter_data->m_Value == 0)
        {
            return;
        }
        RenderProfile* render_profile = (RenderProfile*)context;
        dmProfile::Counter* counter   = counter_data->m_Counter;
        if (!AddName(render_profile, counter->m_NameHash, counter->m_Name))
        {
            render_profile->m_CounterOverflow = 1;
            return;
        }
        AddCounter(render_profile, counter->m_NameHash, counter_data->m_Value);
    }

    static void ResetStructure(RenderProfile* render_profile)
    {
        ProfileFrame* frame = render_profile->m_BuildFrame;
        if (render_profile->m_ActiveFrame != frame)
        {
            render_profile->m_ScopeLookup.m_FreeIndexes.Clear();
            render_profile->m_SampleSumLookup.m_FreeIndexes.Clear();
            render_profile->m_CounterLookup.m_FreeIndexes.Clear();
            render_profile->m_ScopeLookup.m_HashLookup.Clear();
            render_profile->m_SampleSumLookup.m_HashLookup.Clear();
            render_profile->m_CounterLookup.m_HashLookup.Clear();
            render_profile->m_ActiveFrame = frame;
        }
        else
        {
            uint32_t scope_count = render_profile->m_ScopeLookup.m_HashLookup.Size();
            for (uint32_t i = 0; i < scope_count; ++i)
            {
                TIndex scope_index = render_profile->m_ScopeLookup.m_SortOrder[i];
                Scope* scope       = &frame->m_Scopes[scope_index];
                scope->m_Elapsed   = 0u;
                scope->m_Count     = 0u;
            }
            uint32_t sample_sum_count = render_profile->m_SampleSumLookup.m_HashLookup.Size();
            for (uint32_t i = 0; i < sample_sum_count; ++i)
            {
                TIndex sample_sum_index       = render_profile->m_SampleSumLookup.m_SortOrder[i];
                SampleSum* sample_sum         = &frame->m_SampleSums[sample_sum_index];
                sample_sum->m_LastSampleIndex = render_profile->m_MaxSampleCount;
                sample_sum->m_Elapsed         = 0u;
                sample_sum->m_Count           = 0u;
            }
            uint32_t counter_count = render_profile->m_CounterLookup.m_HashLookup.Size();
            for (uint32_t i = 0; i < counter_count; ++i)
            {
                TIndex counter_index = render_profile->m_CounterLookup.m_SortOrder[i];
                Counter* counter     = &frame->m_Counters[counter_index];
                counter->m_Count     = 0u;
            }
        }
        render_profile->m_SampleCount = 0;

        render_profile->m_ScopeOverflow     = 0;
        render_profile->m_SampleSumOverflow = 0;
        render_profile->m_CounterOverflow   = 0;
        render_profile->m_SampleOverflow    = 0;
    }

    static uint32_t GetWaitTicks(HRenderProfile render_profile)
    {
        TIndex* wait_time_ptr = render_profile->m_SampleSumLookup.m_HashLookup.Get(VSYNC_WAIT_NAME_HASH);
        if (wait_time_ptr != 0x0)
        {
            SampleSum* sample_sum = &render_profile->m_ActiveFrame->m_SampleSums[*wait_time_ptr];
            return sample_sum->m_Elapsed;
        }
        return 0;
    }

    static float GetWaitTime(HRenderProfile render_profile)
    {
        uint32_t wait_ticks       = GetWaitTicks(render_profile);
        uint64_t ticks_per_second = render_profile->m_TicksPerSecond;
        double elapsed_s          = (double)(wait_ticks) / ticks_per_second;
        float elapsed_ms          = (float)(elapsed_s * 1000.0);
        return elapsed_ms;
    }

    static uint32_t GetFrameTicks(HRenderProfile render_profile)
    {
        TIndex* frame_time_ptr = render_profile->m_SampleSumLookup.m_HashLookup.Get(ENGINE_FRAME_NAME_HASH);
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
        render_profile->m_NowTick                    = dmProfile::GetNowTicks();
        render_profile->m_BuildFrame->m_FrameTime    = dmProfile::GetFrameTime();
        render_profile->m_BuildFrame->m_MaxFrameTime = dmProfile::GetMaxFrameTime();
        render_profile->m_OutOfScopes                = dmProfile::IsOutOfScopes();
        render_profile->m_OutOfSamples               = dmProfile::IsOutOfSamples();

        dmProfile::IterateScopeData(profile, render_profile, false, BuildScope);
        dmProfile::IterateSamples(profile, render_profile, false, BuildSampleSum);
        dmProfile::IterateCounterData(profile, render_profile, BuildCounter);

        render_profile->m_BuildFrame->m_WaitTime = GetWaitTime(render_profile);
    }

    static void PurgeStructure(RenderProfile* render_profile)
    {
        ProfileFrame* frame = render_profile->m_BuildFrame;

        uint64_t ticks_per_second = render_profile->m_TicksPerSecond;
        uint64_t now_tick         = render_profile->m_NowTick;
        uint64_t life_time        = render_profile->m_LifeTime;

        uint32_t scope_count      = render_profile->m_ScopeLookup.m_HashLookup.Size();
        uint32_t sample_sum_count = render_profile->m_SampleSumLookup.m_HashLookup.Size();
        uint32_t counter_count    = render_profile->m_CounterLookup.m_HashLookup.Size();

        const double MIN_ELAPSED_TIME = 0.00005; // Don't show measurements that are less than a 50 microseconds...

        for (uint32_t i = 0; i < scope_count; ++i)
        {
            TIndex scope_index      = render_profile->m_ScopeLookup.m_SortOrder[i];
            Scope* scope            = &frame->m_Scopes[scope_index];
            ScopeStats* scope_stats = &render_profile->m_ScopeStats[scope_index];

            scope_stats->m_FilteredElapsed = (scope_stats->m_FilteredElapsed * 127 + scope->m_Elapsed) / 128u;

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
            TIndex sample_sum_index          = render_profile->m_SampleSumLookup.m_SortOrder[i];
            SampleSum* sample_sum            = &frame->m_SampleSums[sample_sum_index];
            SampleSumStats* sample_sum_stats = &render_profile->m_SampleSumStats[sample_sum_index];

            sample_sum_stats->m_FilteredElapsed = (sample_sum_stats->m_FilteredElapsed * 127 + sample_sum->m_Elapsed) / 128u;

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
            TIndex counter_index = render_profile->m_CounterLookup.m_SortOrder[i];
            Counter* counter     = &frame->m_Counters[counter_index];

            if (counter->m_Count == 0)
            {
                continue;
            }
            CounterStats* counter_stats = &render_profile->m_CounterStats[counter_index];

            counter_stats->m_LastSeenTick = now_tick;
        }

        {
            uint32_t i = 0;
            while (i < scope_count)
            {
                TIndex scope_index      = render_profile->m_ScopeLookup.m_SortOrder[i];
                ScopeStats* scope_stats = &render_profile->m_ScopeStats[scope_index];

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
                TIndex sample_sum_index = render_profile->m_SampleSumLookup.m_SortOrder[i];
                SampleSum* sample_sum   = &frame->m_SampleSums[sample_sum_index];
                TIndex* scope_index_ptr = render_profile->m_ScopeLookup.m_HashLookup.Get(sample_sum->m_ScopeNameHash);

                SampleSumStats* sample_sum_stats = &render_profile->m_SampleSumStats[sample_sum_index];
                bool purge                       = scope_index_ptr == 0x0 || (sample_sum_stats->m_LastSeenTick + life_time) <= now_tick;

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
                TIndex counter_index        = render_profile->m_CounterLookup.m_SortOrder[i];
                CounterStats* counter_stats = &render_profile->m_CounterStats[counter_index];

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
        {
        }
        const RenderProfile* m_RenderProfile;
        bool operator()(TIndex a_index, TIndex b_index) const
        {
            const ScopeStats& stats_a = m_RenderProfile->m_ScopeStats[a_index];
            const ScopeStats& stats_b = m_RenderProfile->m_ScopeStats[b_index];
            return stats_a.m_FilteredElapsed > stats_b.m_FilteredElapsed;
        }
    };

    struct SampleSumSortPred
    {
        SampleSumSortPred(RenderProfile* render_profile)
            : m_RenderProfile(render_profile)
        {
        }
        const RenderProfile* m_RenderProfile;
        bool operator()(TIndex a_index, TIndex b_index) const
        {
            const SampleSumStats& stats_a = m_RenderProfile->m_SampleSumStats[a_index];
            const SampleSumStats& stats_b = m_RenderProfile->m_SampleSumStats[b_index];
            return stats_a.m_FilteredElapsed > stats_b.m_FilteredElapsed;
        }
    };

    struct CounterSortPred
    {
        CounterSortPred(RenderProfile* render_profile)
            : m_RenderProfile(render_profile)
        {
        }
        const RenderProfile* m_RenderProfile;
        bool operator()(TIndex a_index, TIndex b_index) const
        {
            const ProfileFrame* frame         = m_RenderProfile->m_ActiveFrame;
            const Counter& counter_a          = frame->m_Counters[a_index];
            const char* const* counter_a_name = m_RenderProfile->m_NameLookupTable.Get(counter_a.m_NameHash);
            const Counter& counter_b          = frame->m_Counters[b_index];
            const char* const* counter_b_name = m_RenderProfile->m_NameLookupTable.Get(counter_b.m_NameHash);
            return dmStrCaseCmp(*counter_a_name, *counter_b_name) < 0;
        }
    };

    static void SortStructure(RenderProfile* render_profile)
    {
        uint32_t scope_count = render_profile->m_ScopeLookup.m_HashLookup.Size();
        std::stable_sort(&render_profile->m_ScopeLookup.m_SortOrder[0], &render_profile->m_ScopeLookup.m_SortOrder[scope_count], ScopeSortPred(render_profile));

        uint32_t sample_sum_count = render_profile->m_SampleSumLookup.m_HashLookup.Size();
        std::stable_sort(&render_profile->m_SampleSumLookup.m_SortOrder[0], &render_profile->m_SampleSumLookup.m_SortOrder[sample_sum_count], SampleSumSortPred(render_profile));

        uint32_t counter_count = render_profile->m_CounterLookup.m_HashLookup.Size();
        std::sort(&render_profile->m_CounterLookup.m_SortOrder[0], &render_profile->m_CounterLookup.m_SortOrder[counter_count], CounterSortPred(render_profile));
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

        render_profile->m_ScopeLookup.m_FreeIndexes.Clear();
        render_profile->m_SampleSumLookup.m_FreeIndexes.Clear();
        render_profile->m_CounterLookup.m_FreeIndexes.Clear();

        render_profile->m_ScopeLookup.m_HashLookup.Clear();
        for (TIndex i = 0; i < snapshot->m_ScopeCount; ++i)
        {
            TIndex scope_index = render_profile->m_ScopeLookup.m_FreeIndexes.Pop();
            Scope* scope       = &snapshot->m_Frame.m_Scopes[scope_index];
            render_profile->m_ScopeLookup.m_HashLookup.Put(scope->m_NameHash, scope_index);
            render_profile->m_ScopeLookup.m_SortOrder[i] = scope_index;
            ScopeStats* scope_stats                      = &render_profile->m_ScopeStats[i];
            scope_stats->m_LastSeenTick                  = render_profile->m_NowTick;
            scope_stats->m_FilteredElapsed               = scope->m_Elapsed;
        }

        render_profile->m_SampleSumLookup.m_HashLookup.Clear();
        for (TIndex i = 0; i < snapshot->m_SampleSumCount; ++i)
        {
            TIndex sample_sum_index = render_profile->m_SampleSumLookup.m_FreeIndexes.Pop();
            SampleSum* sample_sum   = &snapshot->m_Frame.m_SampleSums[sample_sum_index];
            TNameHash name_hash     = GetCombinedHash(sample_sum->m_ScopeNameHash, sample_sum->m_SampleNameHash);
            render_profile->m_SampleSumLookup.m_HashLookup.Put(name_hash, sample_sum_index);
            render_profile->m_SampleSumLookup.m_SortOrder[i] = sample_sum_index;
            SampleSumStats* sample_stats                     = &render_profile->m_SampleSumStats[i];
            sample_stats->m_LastSeenTick                     = render_profile->m_NowTick;
            sample_stats->m_FilteredElapsed                  = sample_sum->m_Elapsed;
        }

        render_profile->m_CounterLookup.m_HashLookup.Clear();
        for (TIndex i = 0; i < snapshot->m_CounterCount; ++i)
        {
            TIndex counter_index = render_profile->m_CounterLookup.m_FreeIndexes.Pop();
            Counter* counter     = &snapshot->m_Frame.m_Counters[counter_index];
            render_profile->m_CounterLookup.m_HashLookup.Put(counter->m_NameHash, counter_index);
            render_profile->m_CounterLookup.m_SortOrder[i] = counter_index;
            CounterStats* counter_stats                    = &render_profile->m_CounterStats[i];
            counter_stats->m_LastSeenTick                  = render_profile->m_NowTick;
        }

        render_profile->m_SampleCount   = snapshot->m_SampleCount;
        render_profile->m_PlaybackFrame = recorded_frame_index;

        render_profile->m_ActiveFrame = &snapshot->m_Frame;
    }

    struct Position
    {
        Position(int x, int y)
            : x(x)
            , y(y)
        {
        }
        int x;
        int y;
    };

    struct Size
    {
        Size(int w, int h)
            : w(w)
            , h(h)
        {
        }
        int w;
        int h;
    };

    struct Area
    {
        Area(Position p, Size s)
            : p(p)
            , s(s)
        {
        }
        Position p;
        Size s;
    };

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

    static Area GetScopesArea(DisplayMode display_mode, const Area& details_area, int scopes_count, int counters_count)
    {
        const int count = dmMath::Max(scopes_count, counters_count);

        Size s(SCOPES_NAME_WIDTH + CHAR_WIDTH + SCOPES_TIME_WIDTH + CHAR_WIDTH + SCOPES_COUNT_WIDTH, LINE_SPACING * (1 + count));
        if (display_mode == DISPLAYMODE_LANDSCAPE)
        {
            Position p(details_area.p.x, details_area.p.y + details_area.s.h - s.h);
            return Area(p, s);
        }
        else if (display_mode == DISPLAYMODE_PORTRAIT)
        {
            Position p(details_area.p.x + details_area.s.w - s.w, details_area.p.y);
            return Area(p, s);
        }
        return Area(Position(0, 0), Size(0, 0));
    }

    static Area GetCountersArea(DisplayMode display_mode, const Area& details_area, int scopes_count, int counters_count)
    {
        if (display_mode == DISPLAYMODE_LANDSCAPE || display_mode == DISPLAYMODE_PORTRAIT)
        {
            const int count = dmMath::Max(scopes_count, counters_count);
            Size s(COUNTERS_NAME_WIDTH + CHAR_WIDTH + COUNTERS_COUNT_WIDTH, LINE_SPACING * (1 + count));
            Position p(details_area.p.x, details_area.p.y);
            return Area(p, s);
        }
        return Area(Position(0, 0), Size(0, 0));
    }

    static Area GetSamplesArea(DisplayMode display_mode, const Area& details_area, const Area& scopes_area, const Area& counters_area)
    {
        if (display_mode == DISPLAYMODE_LANDSCAPE)
        {
            Size s(details_area.s.w - (scopes_area.s.w + CHAR_WIDTH), details_area.s.h);
            Position p(scopes_area.p.x + scopes_area.s.w + CHAR_WIDTH, details_area.p.y + details_area.s.h - s.h);
            return Area(p, s);
        }
        else if (display_mode == DISPLAYMODE_PORTRAIT)
        {
            int lower_clip = dmMath::Max(scopes_area.p.y + scopes_area.s.h, counters_area.p.y + counters_area.s.h);
            int max_height = details_area.p.y + details_area.s.h - lower_clip;
            Size s(details_area.s.w, max_height);
            Position p(details_area.p.x, details_area.p.y + details_area.s.h - s.h);
            return Area(p, s);
        }
        return Area(Position(0, 0), Size(0, 0));
    }

    static Area GetSampleFramesArea(DisplayMode display_mode, int sample_frames_name_width, const Area& samples_area)
    {
        const int offset_y = LINE_SPACING;
        const int offset_x = sample_frames_name_width + CHAR_WIDTH + SAMPLE_FRAMES_TIME_WIDTH + CHAR_WIDTH + SAMPLE_FRAMES_COUNT_WIDTH + CHAR_WIDTH;

        Position p(samples_area.p.x + offset_x, samples_area.p.y);
        Size s(samples_area.s.w - offset_x, samples_area.s.h - offset_y);

        return Area(p, s);
    }

    static void FillArea(dmRender::HRenderContext render_context, const Area& a, const Vector4& col)
    {
        dmRender::Square2d(render_context, a.p.x, a.p.y, a.p.x + a.s.w, a.p.y + a.s.h, col);
    }

    static void Draw(HRenderProfile render_profile, dmRender::HRenderContext render_context, dmRender::HFontMap font_map, const Size display_size, DisplayMode display_mode)
    {
        uint32_t batch_key = 0;
        {
            HashState64 key_state;
            dmHashInit64(&key_state, false);
            dmHashUpdateBuffer64(&key_state, &font_map, sizeof(font_map));
            uint16_t render_order = 0;
            dmHashUpdateBuffer64(&key_state, &render_order, sizeof(uint16_t));
            batch_key = dmHashFinal64(&key_state);
        }

        const int SAMPLE_FRAMES_NAME_LENGTH = display_mode == DISPLAYMODE_LANDSCAPE ? LANDSCAPE_SAMPLE_FRAMES_NAME_LENGTH : PORTRAIT_SAMPLE_FRAMES_NAME_LENGTH;
        const int SAMPLE_FRAMES_NAME_WIDTH = display_mode == DISPLAYMODE_LANDSCAPE ? LANDSCAPE_SAMPLE_FRAMES_NAME_WIDTH : PORTRAIT_SAMPLE_FRAMES_NAME_WIDTH;

        const Area profiler_area = GetProfilerArea(display_mode, display_size);
        FillArea(render_context, profiler_area, PROFILER_BG_COLOR);

        const Area header_area  = GetHeaderArea(display_mode, profiler_area);
        const Area details_area = GetDetailsArea(display_mode, profiler_area, header_area);

        char buffer[256];
        float col[3];

        dmRender::DrawTextParams params;
        params.m_FaceColor   = TITLE_FACE_COLOR;
        params.m_ShadowColor = TITLE_SHADOW_COLOR;

        uint64_t ticks_per_second = render_profile->m_TicksPerSecond;
        const ProfileFrame* frame = render_profile->m_ActiveFrame;

        float frame_time     = frame->m_FrameTime;
        float max_frame_time = frame->m_MaxFrameTime;

        if (render_profile->m_IncludeFrameWait == 0)
        {
            frame_time -= frame->m_WaitTime;
            max_frame_time -= frame->m_WaitTime;
        }

        int l = DM_SNPRINTF(buffer, sizeof(buffer), "Frame: %6.3f Max: %6.3f", frame_time, max_frame_time);

        switch (render_profile->m_Mode)
        {
            case PROFILER_MODE_RUN:
                break;
            case PROFILER_MODE_PAUSE:
                if (render_profile->m_PlaybackFrame < 0 || render_profile->m_PlaybackFrame == (int32_t)render_profile->m_RecordBuffer.Size())
                {
                    DM_SNPRINTF(&buffer[l], sizeof(buffer) - l, " (Paused)");
                }
                else
                {
                    DM_SNPRINTF(&buffer[l], sizeof(buffer) - l, " (Show: %d)", render_profile->m_PlaybackFrame);
                }
                break;
            case PROFILER_MODE_PAUSE_ON_PEAK:
                DM_SNPRINTF(&buffer[l], sizeof(buffer) - l, " (Peak)");
                break;
            case PROFILER_MODE_RECORD:
                DM_SNPRINTF(&buffer[l], sizeof(buffer) - l, " (Rec: %d)", render_profile->m_PlaybackFrame);
                break;
        }

        params.m_Text = buffer;
        params.m_WorldTransform.setElem(3, 0, header_area.p.x);
        params.m_WorldTransform.setElem(3, 1, header_area.p.y + CHAR_HEIGHT);
        dmRender::DrawText(render_context, font_map, 0, batch_key, params);

        if (render_profile->m_ViewMode == PROFILER_VIEW_MODE_MINIMIZED)
        {
            return;
        }

        const TIndex scope_count      = (TIndex)render_profile->m_ScopeLookup.m_HashLookup.Size();
        const TIndex counter_count    = (TIndex)render_profile->m_CounterLookup.m_HashLookup.Size();
        const TIndex sample_sum_count = (TIndex)render_profile->m_SampleSumLookup.m_HashLookup.Size();

        const Area scopes_area        = GetScopesArea(display_mode, details_area, scope_count, counter_count);
        const Area counters_area      = GetCountersArea(display_mode, details_area, scope_count, counter_count);
        const Area samples_area       = GetSamplesArea(display_mode, details_area, scopes_area, counters_area);
        const Area sample_frames_area = GetSampleFramesArea(display_mode, SAMPLE_FRAMES_NAME_WIDTH, samples_area);

        FillArea(render_context, sample_frames_area, SAMPLES_BG_COLOR);

        {
            // Scopes
            int y = scopes_area.p.y + scopes_area.s.h;

            params.m_WorldTransform.setElem(3, 1, y);

            int name_x  = scopes_area.p.x;
            int time_x  = name_x + SCOPES_NAME_WIDTH + CHAR_WIDTH;
            int count_x = time_x + SCOPES_TIME_WIDTH + CHAR_WIDTH;

            params.m_FaceColor = TITLE_FACE_COLOR;
            params.m_Text      = render_profile->m_ScopeOverflow ? "*Scopes:" : "Scopes:";
            params.m_WorldTransform.setElem(3, 0, name_x);
            dmRender::DrawText(render_context, font_map, 0, batch_key, params);
            params.m_Text = "    ms";
            params.m_WorldTransform.setElem(3, 0, time_x);
            dmRender::DrawText(render_context, font_map, 0, batch_key, params);
            params.m_Text = "  #";
            params.m_WorldTransform.setElem(3, 0, count_x);
            dmRender::DrawText(render_context, font_map, 0, batch_key, params);

            HslToRgb2(4 / 16.0f, 1.0f, 0.65f, col);
            params.m_FaceColor = Vector4(col[0], col[1], col[2], 1.0f);

            for (TIndex sort_index = 0; sort_index < scope_count; ++sort_index)
            {
                TIndex index = render_profile->m_ScopeLookup.m_SortOrder[sort_index];
                Scope* scope = &frame->m_Scopes[index];

                y -= LINE_SPACING;

                double e = (double)(scope->m_Elapsed) / ticks_per_second;

                params.m_WorldTransform.setElem(3, 1, y);

                params.m_Text = *render_profile->m_NameLookupTable.Get(scope->m_NameHash);
                params.m_WorldTransform.setElem(3, 0, name_x);
                dmRender::DrawText(render_context, font_map, 0, batch_key, params);

                params.m_Text = buffer;
                DM_SNPRINTF(buffer, sizeof(buffer), "%6.3f", (float)(e * 1000.0));
                params.m_WorldTransform.setElem(3, 0, time_x);
                dmRender::DrawText(render_context, font_map, 0, batch_key, params);

                params.m_Text = buffer;
                DM_SNPRINTF(buffer, sizeof(buffer), "%3u", scope->m_Count);
                params.m_WorldTransform.setElem(3, 0, count_x);
                dmRender::DrawText(render_context, font_map, 0, batch_key, params);
            }
        }
        {
            // Counters
            int y = counters_area.p.y + counters_area.s.h;

            params.m_WorldTransform.setElem(3, 1, y);

            int name_x  = counters_area.p.x;
            int count_x = name_x + COUNTERS_NAME_WIDTH + CHAR_WIDTH;

            params.m_FaceColor = TITLE_FACE_COLOR;
            params.m_Text      = render_profile->m_CounterOverflow ? "*Counters:" : "Counters:";
            params.m_WorldTransform.setElem(3, 0, name_x);
            dmRender::DrawText(render_context, font_map, 0, batch_key, params);
            params.m_Text = "           #";
            params.m_WorldTransform.setElem(3, 0, count_x);
            dmRender::DrawText(render_context, font_map, 0, batch_key, params);

            HslToRgb2(4 / 16.0f, 1.0f, 0.65f, col);
            params.m_FaceColor = Vector4(col[0], col[1], col[2], 1.0f);
            for (TIndex sort_index = 0; sort_index < counter_count; ++sort_index)
            {
                TIndex index     = render_profile->m_CounterLookup.m_SortOrder[sort_index];
                Counter* counter = &frame->m_Counters[index];

                y -= LINE_SPACING;

                params.m_WorldTransform.setElem(3, 1, y);
                params.m_Text = *render_profile->m_NameLookupTable.Get(counter->m_NameHash);
                params.m_WorldTransform.setElem(3, 0, name_x);
                dmRender::DrawText(render_context, font_map, 0, batch_key, params);

                params.m_Text = buffer;
                DM_SNPRINTF(buffer, sizeof(buffer), "%12u", counter->m_Count);
                params.m_WorldTransform.setElem(3, 0, count_x);
                dmRender::DrawText(render_context, font_map, 0, batch_key, params);
            }
        }
        {
            // Samples
            int y = samples_area.p.y + samples_area.s.h;

            params.m_WorldTransform.setElem(3, 1, y);

            int name_x   = samples_area.p.x;
            int time_x   = name_x + SAMPLE_FRAMES_NAME_WIDTH + CHAR_WIDTH;
            int count_x  = time_x + SAMPLE_FRAMES_TIME_WIDTH + CHAR_WIDTH;
            int frames_x = sample_frames_area.p.x;

            params.m_FaceColor = TITLE_FACE_COLOR;
            params.m_Text      = render_profile->m_SampleSumOverflow ? "*Samples:" : "Samples:";
            params.m_WorldTransform.setElem(3, 0, name_x);
            dmRender::DrawText(render_context, font_map, 0, batch_key, params);
            params.m_Text = "    ms";
            params.m_WorldTransform.setElem(3, 0, time_x);
            dmRender::DrawText(render_context, font_map, 0, batch_key, params);
            params.m_Text = "  #";
            params.m_WorldTransform.setElem(3, 0, count_x);
            dmRender::DrawText(render_context, font_map, 0, batch_key, params);
            params.m_Text = render_profile->m_SampleOverflow ? "*Frame:" : "Frame:";
            params.m_WorldTransform.setElem(3, 0, frames_x);
            dmRender::DrawText(render_context, font_map, 0, batch_key, params);

            uint32_t sample_frame_width       = sample_frames_area.s.w;
            const uint32_t frame_ticks        = GetFrameTicks(render_profile);
            const uint32_t active_frame_ticks = GetActiveFrameTicks(render_profile, frame_ticks);
            const uint32_t frame_time         = (frame_ticks == 0) ? (uint32_t)(ticks_per_second / render_profile->m_FPS) : render_profile->m_IncludeFrameWait ? frame_ticks : active_frame_ticks;
            const float tick_length           = (float)(sample_frame_width) / (float)(frame_time);
            const TIndex max_sample_count     = render_profile->m_MaxSampleCount;

            for (TIndex sort_index = 0; sort_index < sample_sum_count; ++sort_index)
            {
                TIndex index          = render_profile->m_SampleSumLookup.m_SortOrder[sort_index];
                SampleSum* sample_sum = &frame->m_SampleSums[index];
                TNameHash name_hash   = GetCombinedHash(sample_sum->m_ScopeNameHash, sample_sum->m_SampleNameHash);
                if (render_profile->m_IncludeFrameWait == 0)
                {
                    if (name_hash == VSYNC_WAIT_NAME_HASH ||
                        name_hash == ENGINE_FRAME_NAME_HASH)
                    {
                        continue;
                    }
                }

                y -= LINE_SPACING;

                if (y < (samples_area.p.y + LINE_SPACING))
                {
                    break;
                }

                double e = ((double)(sample_sum->m_Elapsed)) / ticks_per_second;

                uint32_t color_index = (name_hash >> 6) & 0x1f;
                HslToRgb2(color_index / 31.0f, 1.0f, 0.65f, col);

                params.m_WorldTransform.setElem(3, 1, y);
                params.m_FaceColor = Vector4(col[0], col[1], col[2], 1.0f);

                params.m_Text = buffer;

                TIndex* scope_index_ptr = render_profile->m_ScopeLookup.m_HashLookup.Get(sample_sum->m_ScopeNameHash);
                assert(scope_index_ptr);
                Scope* scope                     = &frame->m_Scopes[*scope_index_ptr];
                const char** scope_name_hash_ptr = render_profile->m_NameLookupTable.Get(scope->m_NameHash);
                assert(scope_name_hash_ptr);
                const char* scope_name            = *scope_name_hash_ptr;
                const char** sample_name_hash_ptr = render_profile->m_NameLookupTable.Get(sample_sum->m_SampleNameHash);
                const char* sample_name           = *sample_name_hash_ptr;

                int buffer_offset = DM_SNPRINTF(buffer, SAMPLE_FRAMES_NAME_LENGTH, "%s.", scope_name);

                while (buffer_offset < SAMPLE_FRAMES_NAME_LENGTH)
                {
                    if (*sample_name == 0)
                    {
                        break;
                    }
                    else if (*sample_name == '@')
                    {
                        buffer[buffer_offset++] = *sample_name++;
                        if (buffer_offset == SAMPLE_FRAMES_NAME_LENGTH)
                        {
                            break;
                        }
                        const uint32_t remaining_sample_name_length = (uint32_t)strlen(sample_name);
                        const uint32_t remaining_buffer_length = SAMPLE_FRAMES_NAME_LENGTH - buffer_offset;
                        if (remaining_sample_name_length > remaining_buffer_length)
                        {
                            sample_name += (remaining_sample_name_length - remaining_buffer_length);
                        }
                        continue;
                    }
                    buffer[buffer_offset++] = *sample_name++;
                }
                buffer[buffer_offset] = 0;

                params.m_WorldTransform.setElem(3, 0, name_x);
                dmRender::DrawText(render_context, font_map, 0, batch_key, params);

                DM_SNPRINTF(buffer, sizeof(buffer), "%6.3f", (float)(e * 1000.0));
                params.m_WorldTransform.setElem(3, 0, time_x);
                dmRender::DrawText(render_context, font_map, 0, batch_key, params);

                DM_SNPRINTF(buffer, sizeof(buffer), "%3u", sample_sum->m_Count);
                params.m_WorldTransform.setElem(3, 0, count_x);
                dmRender::DrawText(render_context, font_map, 0, batch_key, params);

                TIndex sample_index = sample_sum->m_LastSampleIndex;
                while (sample_index != max_sample_count)
                {
                    Sample* sample = &frame->m_Samples[sample_index];

                    float x = frames_x + sample->m_StartTick * tick_length;
                    float w = sample->m_Elapsed * tick_length;
                    if (w < 0.5f)
                    {
                        w = 0.5f;
                    }

                    dmRender::Square2d(render_context, x, y - CHAR_HEIGHT, x + w, y, Vector4(col[0], col[1], col[2], 1.0f));

                    sample_index = sample->m_PreviousSampleIndex;
                }
            }
        }
    }

    RenderProfile::RenderProfile(
    float fps,
    uint32_t ticks_per_second,
    uint32_t lifetime_in_milliseconds,
    TIndex max_scope_count,
    TIndex max_sample_sum_count,
    TIndex max_counter_count,
    TIndex max_sample_count,
    TIndex max_name_count,
    ScopeStats* scope_stats,
    SampleSumStats* sample_sum_stats,
    CounterStats* counter_stats,
    TIndex* scope_sort_order,
    TIndex* sample_sum_sort_order,
    TIndex* counter_sort_order_data,
    TIndex* scope_free_index_data,
    TIndex* sample_sum_free_index_data,
    TIndex* counter_free_index_data,
    ProfileFrame* build_frame)
        : m_FPS(fps)
        , m_TicksPerSecond(ticks_per_second)
        , m_LifeTime((uint32_t)((lifetime_in_milliseconds * ticks_per_second) / 1000))
        , m_BuildFrame(build_frame)
        , m_ActiveFrame(m_BuildFrame)
        , m_Mode(PROFILER_MODE_RUN)
        , m_ViewMode(PROFILER_VIEW_MODE_FULL)
        , m_ScopeLookup(max_scope_count, scope_free_index_data, scope_sort_order)
        , m_SampleSumLookup(max_sample_sum_count, sample_sum_free_index_data, sample_sum_sort_order)
        , m_CounterLookup(max_counter_count, counter_free_index_data, counter_sort_order_data)
        , m_NameLookupTable()
        , m_ScopeStats(scope_stats)
        , m_SampleSumStats(sample_sum_stats)
        , m_CounterStats(counter_stats)
        , m_NowTick(0u)
        , m_MaxSampleCount(max_sample_count)
        , m_PlaybackFrame(-1)
        , m_IncludeFrameWait(1)
        , m_ScopeOverflow(0)
        , m_SampleSumOverflow(0)
        , m_CounterOverflow(0)
        , m_SampleOverflow(0)
        , m_OutOfScopes(0)
        , m_OutOfSamples(0)
    {
        m_NameLookupTable.SetCapacity((max_name_count * 2) / 3, max_name_count * 2);
    }

    RenderProfile::~RenderProfile()
    {
        free(m_BuildFrame);
    }

    RenderProfile* RenderProfile::New(
    float fps,
    uint32_t ticks_per_second,
    uint32_t lifetime_in_milliseconds,
    TIndex max_scope_count,
    TIndex max_sample_sum_count,
    TIndex max_counter_count,
    TIndex max_sample_count)
    {
        // The compicated allocation of a render profile is to make sure we allocated all the data needed in a single allocation
        const TIndex max_name_count             = max_scope_count + max_sample_sum_count + max_counter_count;
        const size_t scope_stats_size           = sizeof(ScopeStats) * max_scope_count;
        const size_t sample_sum_stats_size      = sizeof(SampleSumStats) * max_sample_sum_count;
        const size_t counter_stats_size         = sizeof(CounterStats) * max_counter_count;
        const size_t scope_sort_order_size      = sizeof(TIndex) * max_scope_count;
        const size_t sample_sum_sort_order_size = sizeof(TIndex) * max_sample_sum_count;
        const size_t counter_sort_order_size    = sizeof(TIndex) * max_counter_count;
        const size_t scope_free_index_size      = sizeof(TIndex) * max_scope_count;
        const size_t sample_sum_free_index_size = sizeof(TIndex) * max_sample_sum_count;
        const size_t counters_free_index_size   = sizeof(TIndex) * max_counter_count;

        size_t size = sizeof(RenderProfile) +
        scope_stats_size +
        sample_sum_stats_size +
        counter_stats_size +
        scope_sort_order_size +
        sample_sum_sort_order_size +
        counter_sort_order_size +
        scope_free_index_size +
        sample_sum_free_index_size +
        counters_free_index_size;

        uint8_t* mem = (uint8_t*)malloc(size);
        if (mem == 0x0)
        {
            return 0x0;
        }
        uint8_t* p = &mem[sizeof(RenderProfile)];

        ScopeStats* scope_stats_data = (ScopeStats*)p;
        p += scope_stats_size;

        SampleSumStats* sample_sum_stats_data = (SampleSumStats*)p;
        p += sample_sum_stats_size;

        CounterStats* counter_stats_data = (CounterStats*)p;
        p += counter_stats_size;

        TIndex* scope_sort_order_data = (TIndex*)p;
        p += scope_sort_order_size;

        TIndex* sample_sum_sort_order_data = (TIndex*)p;
        p += sample_sum_sort_order_size;

        TIndex* counter_sort_order_data = (TIndex*)p;
        p += counter_sort_order_size;

        TIndex* scope_free_index_data = (TIndex*)p;
        p += scope_free_index_size;

        TIndex* sample_sum_free_index_data = (TIndex*)p;
        p += sample_sum_free_index_size;

        TIndex* counter_free_index_data = (TIndex*)p;
        p += counters_free_index_size;

        size_t frame_size   = ProfileFrameSize(max_scope_count, max_sample_sum_count, max_counter_count, max_sample_count);
        void* frame_mem     = malloc(frame_size);
        ProfileFrame* frame = CreateProfileFrame(frame_mem, max_scope_count, max_sample_sum_count, max_counter_count, max_sample_count);

        return new (mem) RenderProfile(
        fps,
        ticks_per_second,
        lifetime_in_milliseconds,
        max_scope_count,
        max_sample_sum_count,
        max_counter_count,
        max_sample_count,
        max_name_count,
        scope_stats_data,
        sample_sum_stats_data,
        counter_stats_data,
        scope_sort_order_data,
        sample_sum_sort_order_data,
        counter_sort_order_data,
        scope_free_index_data,
        sample_sum_free_index_data,
        counter_free_index_data,
        frame);
    }

    void RenderProfile::Delete(RenderProfile* render_profile)
    {
        FlushRecording(render_profile, 0);
        delete render_profile;
    }

    HRenderProfile NewRenderProfile(float fps)
    {
        const uint32_t LIFETIME_IN_MILLISECONDS = 6000u;
        const uint32_t MAX_SCOPE_COUNT          = 256;
        const uint32_t MAX_SAMPLE_SUM_COUNT     = 1024;
        const uint32_t MAX_COUNTER_COUNT        = 128;
        const uint32_t MAX_SAMPLE_COUNT         = 8192;

        RenderProfile* profile = RenderProfile::New(
        fps,
        dmProfile::GetTicksPerSecond(),
        LIFETIME_IN_MILLISECONDS,
        MAX_SCOPE_COUNT,
        MAX_SAMPLE_SUM_COUNT,
        MAX_COUNTER_COUNT,
        MAX_SAMPLE_COUNT);

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
                ProfileSnapshot* snapshot = MakeProfileSnapshot(
                render_profile->m_NowTick,
                render_profile->m_BuildFrame,
                render_profile->m_ScopeLookup.m_HashLookup.Size(),
                render_profile->m_ScopeLookup.m_SortOrder,
                render_profile->m_SampleSumLookup.m_HashLookup.Size(),
                render_profile->m_SampleSumLookup.m_SortOrder,
                render_profile->m_CounterLookup.m_HashLookup.Size(),
                render_profile->m_CounterLookup.m_SortOrder,
                render_profile->m_SampleCount);

                FlushRecording(render_profile, 1);
                render_profile->m_RecordBuffer.SetSize(1);
                render_profile->m_RecordBuffer[0] = snapshot;
            }

            GotoRecordedFrame(render_profile, 0);
            SortStructure(render_profile);

            return;
        }

        if (render_profile->m_Mode == PROFILER_MODE_RECORD)
        {
            ProfileSnapshot* snapshot = MakeProfileSnapshot(
            render_profile->m_NowTick,
            render_profile->m_BuildFrame,
            render_profile->m_ScopeLookup.m_HashLookup.Size(),
            render_profile->m_ScopeLookup.m_SortOrder,
            render_profile->m_SampleSumLookup.m_HashLookup.Size(),
            render_profile->m_SampleSumLookup.m_SortOrder,
            render_profile->m_CounterLookup.m_HashLookup.Size(),
            render_profile->m_CounterLookup.m_SortOrder,
            render_profile->m_SampleCount);
            if (render_profile->m_RecordBuffer.Remaining() == 0)
            {
                render_profile->m_RecordBuffer.SetCapacity(render_profile->m_RecordBuffer.Size() + (uint32_t)render_profile->m_FPS);
            }
            render_profile->m_RecordBuffer.Push(snapshot);
            render_profile->m_PlaybackFrame = (int32_t)render_profile->m_RecordBuffer.Size();
        }

        if (render_profile->m_ViewMode != PROFILER_VIEW_MODE_MINIMIZED)
        {
            PurgeStructure(render_profile);
            SortStructure(render_profile);
        }
    }

    void SetMode(HRenderProfile render_profile, ProfilerMode mode)
    {
        if ((render_profile->m_Mode != PROFILER_MODE_RECORD) && (mode == PROFILER_MODE_RECORD))
        {
            FlushRecording(render_profile, (uint32_t)render_profile->m_FPS);
        }
        if ((render_profile->m_Mode == PROFILER_MODE_PAUSE) && (mode == PROFILER_MODE_RUN))
        {
            render_profile->m_PlaybackFrame = (int32_t)render_profile->m_RecordBuffer.Size();
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

        const DisplayMode display_mode = render_profile->m_ViewMode == PROFILER_VIEW_MODE_MINIMIZED ?
        DISPLAYMODE_MINIMIZED :
        (display_size.w > display_size.h ? DISPLAYMODE_LANDSCAPE : DISPLAYMODE_PORTRAIT);

        Draw(render_profile, render_context, font_map, display_size, display_mode);
    }

} // namespace dmProfileRender
