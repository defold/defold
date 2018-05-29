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

    struct UIScope {
        const char* m_Name;
        uint64_t m_LastSeenTick;
        TNameHash m_NameHash;    // m_Name
        uint32_t m_Elapsed;
//        uint32_t m_PeakElapsed;
        uint32_t m_FilteredElapsed; // Used for sorting, highest peak since last sort
        uint32_t m_Count;
    };

    struct UISampleSum {
        const char* m_Name;
        uint64_t m_LastSeenTick;
        TNameHash m_NameHash;   // m_Name + m_Scope->m_Name
        TNameHash m_ScopeHash;  // m_Scope->m_Name
        uint32_t m_Elapsed;
//        uint32_t m_PeakElapsed;
        uint32_t m_FilteredElapsed; // Used for sorting, highest peak since last sort
        uint32_t m_Count;
        TIndex m_LastSampleIndex;
        uint16_t m_ColorIndex;
    };

    struct UICounter {
        const char* m_Name;
        uint64_t m_LastSeenTick;
        TNameHash m_NameHash;    // m_Name
        uint32_t m_Count;
    };

    struct UISample {
        uint32_t m_StartTick;
        uint32_t m_Elapsed;
        TIndex m_PreviousSampleIndex;
    };

    #define HASHTABLE_BUFFER_SIZE(c, table_size, capacity) ((table_size) * sizeof(uint32_t) + (capacity) * sizeof(c))

    typedef dmHashTable<TNameHash, TIndex> TIndexLookupTable;

    struct UIProfile {
        static UIProfile* New(float fps, TIndex max_scope_count, TIndex max_sample_sum_count, TIndex max_counter_count, TIndex max_sample_count);
        static void Delete(UIProfile* ui_profile);

        float m_FPS;
        uint64_t m_NowTick;
        uint64_t m_LastSortTick;
        uint32_t m_LifeTime;
        uint32_t m_SortInterval;
        uint16_t m_ColorIndex;

        TIndexLookupTable m_ScopeLookup;
        TIndexLookupTable m_SampleSumLookup;
        TIndexLookupTable m_CounterLookup;

        UIScope* m_Scopes;
        UISampleSum* m_SampleSums;
        UICounter* m_Counters;
        UISample* m_Samples;

        const TIndex m_MaxScopeCount;
        const TIndex m_MaxSampleSumCount;
        const TIndex m_MaxCounterCount;

        TIndex m_SampleCount;
        const TIndex m_MaxSampleCount;

        uint32_t m_ScopeOverflow : 1;
        uint32_t m_SampleSumOverflow : 1;
        uint32_t m_CounterOverflow : 1;
        uint32_t m_SampleOverflow : 1;
    private:
        UIProfile(float fps, TIndex max_scope_count, TIndex max_sample_sum_count, TIndex max_counter_count, TIndex max_sample_count);
    };

    static size_t ScopeTableDataOffset(TIndex max_scope_count, TIndex max_sample_sum_count, TIndex max_counter_count, TIndex max_sample_count)
    {
        return sizeof(UIProfile);
    }

    static size_t SampleSumTableDataOffset(TIndex max_scope_count, TIndex max_sample_sum_count, TIndex max_counter_count, TIndex max_sample_count)
    {
        return ScopeTableDataOffset(max_scope_count, max_sample_sum_count, max_counter_count, max_sample_count)
            + HASHTABLE_BUFFER_SIZE(TIndexLookupTable::Entry, (max_scope_count * 2) / 3, max_scope_count * 2);
    }

    static size_t CounterTableDataOffset(TIndex max_scope_count, TIndex max_sample_sum_count, TIndex max_counter_count, TIndex max_sample_count)
    {
        return SampleSumTableDataOffset(max_scope_count, max_sample_sum_count, max_counter_count, max_sample_count)
            + HASHTABLE_BUFFER_SIZE(TIndexLookupTable::Entry, (max_sample_sum_count * 2) / 3, max_sample_sum_count * 2);
    }

    static size_t ScopesOffset(TIndex max_scope_count, TIndex max_sample_sum_count, TIndex max_counter_count, TIndex max_sample_count)
    {
        return CounterTableDataOffset(max_scope_count, max_sample_sum_count, max_counter_count, max_sample_count)
            + HASHTABLE_BUFFER_SIZE(TIndexLookupTable::Entry, (max_counter_count * 2) / 3, (max_counter_count * 2));
    }

    static size_t SampleSumsOffset(TIndex max_scope_count, TIndex max_sample_sum_count, TIndex max_counter_count, TIndex max_sample_count)
    {
        return ScopesOffset(max_scope_count, max_sample_sum_count, max_counter_count, max_sample_count)
            + sizeof(UIScope) * max_scope_count;
    }

    static size_t CountersOffset(TIndex max_scope_count, TIndex max_sample_sum_count, TIndex max_counter_count, TIndex max_sample_count)
    {
        return SampleSumsOffset(max_scope_count, max_sample_sum_count, max_counter_count, max_sample_count)
            + sizeof(UISampleSum) * max_sample_sum_count;
    }

    static size_t SamplesOffset(TIndex max_scope_count, TIndex max_sample_sum_count, TIndex max_counter_count, TIndex max_sample_count)
    {
        return CountersOffset(max_scope_count,max_sample_sum_count, max_counter_count, max_sample_count)
            + sizeof(UICounter) * max_counter_count;
    }

    static size_t UIProfileSize(TIndex max_scope_count, TIndex max_sample_sum_count, TIndex max_counter_count, TIndex max_sample_count)
    {
        return SamplesOffset(max_scope_count, max_sample_sum_count, max_counter_count, max_sample_count)
            + sizeof(UISample) * max_sample_count;
    }

    static uint8_t* GetBufferAddress(void* base, size_t offset)
    {
        return &(((uint8_t*)(base))[offset]);
    }

    UIProfile::UIProfile(float fps, TIndex max_scope_count, TIndex max_sample_sum_count, TIndex max_counter_count, TIndex max_sample_count)
        : m_FPS(fps)
        , m_NowTick(0u)
        , m_LastSortTick(0u)
        , m_ColorIndex(0u)
        , m_ScopeLookup(GetBufferAddress(this, ScopeTableDataOffset(max_scope_count, max_sample_sum_count, max_counter_count, max_sample_count)), (max_scope_count * 2) / 3, max_scope_count * 2)
        , m_SampleSumLookup(GetBufferAddress(this, SampleSumTableDataOffset(max_scope_count, max_sample_sum_count, max_counter_count, max_sample_count)), (max_sample_sum_count * 2) / 3, max_sample_sum_count * 2)
        , m_CounterLookup(GetBufferAddress(this, CounterTableDataOffset(max_scope_count, max_sample_sum_count, max_counter_count, max_sample_count)), (max_counter_count * 2) / 3, max_counter_count * 2)
        , m_Scopes((UIScope*)GetBufferAddress(this, ScopesOffset(max_scope_count, max_sample_sum_count, max_counter_count, max_sample_count)))
        , m_SampleSums((UISampleSum*)GetBufferAddress(this, SampleSumsOffset(max_scope_count, max_sample_sum_count, max_counter_count, max_sample_count)))
        , m_Counters((UICounter*)GetBufferAddress(this, CountersOffset(max_scope_count, max_sample_sum_count, max_counter_count, max_sample_count)))
        , m_Samples((UISample*)GetBufferAddress(this, SamplesOffset(max_scope_count, max_sample_sum_count, max_counter_count, max_sample_count)))
        , m_MaxScopeCount(max_scope_count)
        , m_MaxSampleSumCount(max_sample_sum_count)
        , m_MaxCounterCount(max_counter_count)
        , m_SampleCount(0)
        , m_MaxSampleCount(max_sample_count)
        , m_ScopeOverflow(0)
        , m_SampleSumOverflow(0)
        , m_CounterOverflow(0)
        , m_SampleOverflow(0)
    {}

    UIProfile* UIProfile::New(float fps, TIndex max_scope_count, TIndex max_sample_sum_count, TIndex max_counter_count, TIndex max_sample_count)
    {
        size_t size = UIProfileSize(max_scope_count, max_sample_sum_count, max_counter_count, max_sample_count);
        void* mem = malloc(size);
        return new (mem) UIProfile(fps, max_scope_count, max_sample_sum_count, max_counter_count, max_sample_count);
    }

    void UIProfile::Delete(UIProfile* ui_profile)
    {
        if (ui_profile == 0x0)
            return;
        ui_profile->~UIProfile();
        free(ui_profile);
    }

    static void AddScope(UIProfile* ui_profile, const char* name, TNameHash name_hash, uint32_t elapsed, uint32_t count)
    {
        TIndex* index_ptr = ui_profile->m_ScopeLookup.Get(name_hash);
        if (index_ptr == 0x0)
        {
            if (ui_profile->m_ScopeLookup.Full())
            {
                ui_profile->m_ScopeOverflow = 1;
                return;
            }
            TIndex new_index = ui_profile->m_ScopeLookup.Size();
            if (new_index == ui_profile->m_MaxScopeCount)
            {
                ui_profile->m_ScopeOverflow = 1;
                return;
            }
            ui_profile->m_ScopeLookup.Put(name_hash, new_index);
            index_ptr = ui_profile->m_ScopeLookup.Get(name_hash);
            UIScope* ui_scope = &ui_profile->m_Scopes[new_index];
            ui_scope->m_Name = name;
            ui_scope->m_NameHash = name_hash;
            ui_scope->m_Count = 0u;
            ui_scope->m_Elapsed = 0u;
//            ui_scope->m_PeakElapsed = 0u;
            ui_scope->m_FilteredElapsed = 0u;
            ui_scope->m_LastSeenTick = ui_profile->m_NowTick - ui_profile->m_LifeTime;
        }
        uint32_t index = *index_ptr;
        UIScope* ui_scope = &ui_profile->m_Scopes[index];
        ui_scope->m_Elapsed += elapsed;
        ui_scope->m_Count += count;
    }

    static void FreeScope(UIProfile* ui_profile, TIndex index)
    {
        UIScope* scope = &ui_profile->m_Scopes[index];
        ui_profile->m_ScopeLookup.Erase(scope->m_NameHash);
        uint32_t new_count = ui_profile->m_ScopeLookup.Size();
        if (index != new_count)
        {
            UIScope* source = &ui_profile->m_Scopes[new_count];
            TIndex* source_index_ptr = ui_profile->m_ScopeLookup.Get(source->m_NameHash);
            assert(source_index_ptr != 0x0);
            *scope = *source;
            *source_index_ptr = index;
        }
    }

    static void AddSample(UIProfile* ui_profile, const char* name, TNameHash name_hash, TNameHash scope_name_hash, uint32_t start_tick, uint32_t elapsed)
    {
        TIndex* index_ptr = ui_profile->m_SampleSumLookup.Get(name_hash);
        if (index_ptr == 0x0)
        {
            if (ui_profile->m_SampleSumLookup.Full())
            {
                ui_profile->m_SampleSumOverflow = 1;
                return;
            }
            TIndex new_index = ui_profile->m_SampleSumLookup.Size();
            if (new_index == ui_profile->m_MaxSampleSumCount)
            {
                ui_profile->m_SampleSumOverflow = 1;
                return;
            }
            ui_profile->m_SampleSumLookup.Put(name_hash, new_index);
            index_ptr = ui_profile->m_SampleSumLookup.Get(name_hash);
            UISampleSum* ui_sample_sum = &ui_profile->m_SampleSums[new_index];
            ui_sample_sum->m_Name = name;
            ui_sample_sum->m_NameHash = name_hash;
            ui_sample_sum->m_ScopeHash = scope_name_hash;
            ui_sample_sum->m_Elapsed = 0u;
//            ui_sample_sum->m_PeakElapsed = 0u;
            ui_sample_sum->m_FilteredElapsed = 0u;
            ui_sample_sum->m_Count = 0u;
            ui_sample_sum->m_LastSampleIndex = ui_profile->m_MaxSampleCount;
            ui_sample_sum->m_LastSeenTick = ui_profile->m_NowTick - ui_profile->m_LifeTime;
            ui_sample_sum->m_ColorIndex = ui_profile->m_ColorIndex++;
        }
        uint32_t index = *index_ptr;
        UISampleSum* ui_sample_sum = &ui_profile->m_SampleSums[index];

        if (ui_sample_sum->m_LastSampleIndex != ui_profile->m_MaxSampleCount)
        {
            UISample* last_sample = &ui_profile->m_Samples[ui_sample_sum->m_LastSampleIndex];
            uint32_t end_last = last_sample->m_StartTick + last_sample->m_Elapsed;
            if (start_tick >= last_sample->m_StartTick && start_tick < end_last)
            {
                // Probably recursion. The sample is overlapping the previous.
                // Ignore this sample.
                return;
            }
        }

        ui_sample_sum->m_Elapsed += elapsed;
        ui_sample_sum->m_Count += 1u;

        if (ui_profile->m_SampleCount == ui_profile->m_MaxSampleCount)
        {
            ui_profile->m_SampleOverflow = 1;
            return;
        }

        TIndex sample_index = ui_profile->m_SampleCount++;
        UISample* sample = &ui_profile->m_Samples[sample_index];
        sample->m_PreviousSampleIndex = ui_sample_sum->m_LastSampleIndex;
        sample->m_StartTick = start_tick;
        sample->m_Elapsed = elapsed;

        ui_sample_sum->m_LastSampleIndex = sample_index;
    }

    static void FreeSampleSum(UIProfile* ui_profile, TIndex index)
    {
        UISampleSum* sample_sum = &ui_profile->m_SampleSums[index];
        ui_profile->m_SampleSumLookup.Erase(sample_sum->m_NameHash);
        uint32_t new_count = ui_profile->m_SampleSumLookup.Size();
        if (index != new_count)
        {
            UISampleSum* source = &ui_profile->m_SampleSums[new_count];
            TIndex* source_index_ptr = ui_profile->m_SampleSumLookup.Get(source->m_NameHash);
            assert(source_index_ptr != 0x0);
            *sample_sum = *source;
            *source_index_ptr = index;
        }
    }

    static void AddCounter(UIProfile* ui_profile, const char* name, TNameHash name_hash, uint32_t count)
    {
        TIndex* index_ptr = ui_profile->m_CounterLookup.Get(name_hash);
        if (index_ptr == 0x0)
        {
            if (ui_profile->m_CounterLookup.Full())
            {
                ui_profile->m_CounterOverflow = 1;
                return;
            }
            TIndex new_index = ui_profile->m_CounterLookup.Size();
            if (new_index == ui_profile->m_MaxCounterCount)
            {
                ui_profile->m_CounterOverflow = 1;
                return;
            }
            ui_profile->m_CounterLookup.Put(name_hash, new_index);
            index_ptr = ui_profile->m_CounterLookup.Get(name_hash);
            UICounter* ui_counter = &ui_profile->m_Counters[new_index];
            ui_counter->m_Name = name;
            ui_counter->m_NameHash = name_hash;
            ui_counter->m_Count = 0u;
            ui_counter->m_LastSeenTick = ui_profile->m_NowTick;
        }
        uint32_t index = *index_ptr;
        UICounter* ui_counter = &ui_profile->m_Counters[index];
        ui_counter->m_Count += count;
    }

    static void FreeCounter(UIProfile* ui_profile, TIndex index)
    {
        UICounter* counter = &ui_profile->m_Counters[index];
        ui_profile->m_CounterLookup.Erase(counter->m_NameHash);
        uint32_t new_count = ui_profile->m_CounterLookup.Size();
        if (index != new_count)
        {
            UICounter* source = &ui_profile->m_Counters[new_count];
            TIndex* source_index_ptr = ui_profile->m_CounterLookup.Get(source->m_NameHash);
            assert(source_index_ptr != 0x0);
            *counter = *source;
            *source_index_ptr = index;
        }
    }

    static void BuildUIScope(void* context, const dmProfile::ScopeData* scope_data)
    {
        UIProfile* ui_profile = (UIProfile*)context;
        dmProfile::Scope* scope = scope_data->m_Scope;
        if (scope_data->m_Count == 0)
        {
            return;
        }
        TNameHash name_hash = GetScopeHash(scope);
        AddScope(ui_profile, scope->m_Name, name_hash, scope_data->m_Elapsed, scope_data->m_Count);
    }

    static void BuildUISampleSum(void* context, const dmProfile::Sample* sample)
    {
        UIProfile* ui_profile = (UIProfile*)context;
        TNameHash scope_name_hash = GetScopeHash(sample->m_Scope);
        if (sample->m_Elapsed == 0u)
        {
            return;
        }
        TNameHash name_hash = GetSampleHash(sample);
        AddSample(ui_profile, sample->m_Name, name_hash, scope_name_hash, sample->m_Start, sample->m_Elapsed);
    }

    static void BuildUICounter(void* context, const dmProfile::CounterData* counter_data)
    {
        UIProfile* ui_profile = (UIProfile*)context;
        dmProfile::Counter* counter = counter_data->m_Counter;
        if (counter_data->m_Value == 0)
        {
            return;
        }
        TNameHash name_hash = GetCounterHash(counter);
        AddCounter(ui_profile, counter->m_Name, name_hash, counter_data->m_Value);
    }

    static void ResetUIStructure(UIProfile* out_ui_profile)
    {
        for (uint32_t i = 0; i < out_ui_profile->m_ScopeLookup.Size(); ++i)
        {
            UIScope* ui_scope = &out_ui_profile->m_Scopes[i];
            ui_scope->m_Elapsed = 0u;
            ui_scope->m_Count = 0u;
        }
        for (uint32_t i = 0; i < out_ui_profile->m_SampleSumLookup.Size(); ++i)
        {
            UISampleSum* sample_sum = &out_ui_profile->m_SampleSums[i];
            sample_sum->m_LastSampleIndex = out_ui_profile->m_MaxSampleCount;
            sample_sum->m_Elapsed = 0u;
            sample_sum->m_Count = 0u;
        }
        for (uint32_t i = 0; i < out_ui_profile->m_CounterLookup.Size(); ++i)
        {
            UICounter* ui_counter = &out_ui_profile->m_Counters[i];
            ui_counter->m_Count = 0u;
        }
        out_ui_profile->m_SampleCount = 0;

        out_ui_profile->m_ScopeOverflow = 0;
        out_ui_profile->m_SampleSumOverflow = 0;
        out_ui_profile->m_CounterOverflow = 0;
        out_ui_profile->m_SampleOverflow = 0;
    }

    static void BuildUIStructure(dmProfile::HProfile profile, UIProfile* out_ui_profile)
    {
        const uint32_t LIFETIME_IN_SECONDS = 6u;
        const uint32_t SORT_INTERVALL_IN_SECONDS = 3u;

        out_ui_profile->m_NowTick = dmProfile::GetNowTicks();
        out_ui_profile->m_LifeTime = (uint32_t)(dmProfile::GetTicksPerSecond() * LIFETIME_IN_SECONDS);
        out_ui_profile->m_SortInterval = (uint32_t)(dmProfile::GetTicksPerSecond() * SORT_INTERVALL_IN_SECONDS);

        dmProfile::IterateScopeData(profile, out_ui_profile, BuildUIScope);
        dmProfile::IterateSamples(profile, out_ui_profile, BuildUISampleSum);
        dmProfile::IterateCounterData(profile, out_ui_profile, BuildUICounter);
    }

    static void PurgeUIStructure(UIProfile* out_ui_profile)
    {
        uint64_t ticks_per_second = dmProfile::GetTicksPerSecond();
        uint64_t now_tick = out_ui_profile->m_NowTick;
        uint64_t life_time = out_ui_profile->m_LifeTime;

        uint32_t scope_count = out_ui_profile->m_ScopeLookup.Size();
        uint32_t sample_sum_count = out_ui_profile->m_SampleSumLookup.Size();
        uint32_t counter_count = out_ui_profile->m_CounterLookup.Size();

        for (uint32_t i = 0; i < scope_count; ++i)
        {
            UIScope* ui_scope = &out_ui_profile->m_Scopes[i];

            ui_scope->m_FilteredElapsed = (ui_scope->m_FilteredElapsed * 31 + ui_scope->m_Elapsed) / 32u;

            if (ui_scope->m_Count == 0)
            {
                continue;
            }

            double e = ((double)(ui_scope->m_Elapsed)) / ticks_per_second;
            if (e < 0.00005)   // Don't show measurements that are less than a 50 microseconds...
                continue;

//            ui_scope->m_PeakElapsed = ui_scope->m_Elapsed > ui_scope->m_PeakElapsed ? ui_scope->m_Elapsed : ui_scope->m_PeakElapsed;

            ui_scope->m_LastSeenTick = now_tick;
        }

        for (uint32_t i = 0; i < sample_sum_count; ++i)
        {
            UISampleSum* ui_sample_sum = &out_ui_profile->m_SampleSums[i];

            ui_sample_sum->m_FilteredElapsed = (ui_sample_sum->m_FilteredElapsed * 31 + ui_sample_sum->m_Elapsed) / 32u;

            if (ui_sample_sum->m_Count == 0u)
            {
                continue;
            }

            double e = ((double)(ui_sample_sum->m_Elapsed)) / ticks_per_second;
            if (e < 0.00005)   // Don't show measurements that are less than a 50 microseconds...
                continue;

//            ui_sample_sum->m_PeakElapsed = ui_sample_sum->m_Elapsed > ui_sample_sum->m_PeakElapsed ? ui_sample_sum->m_Elapsed : ui_sample_sum->m_PeakElapsed;

            ui_sample_sum->m_LastSeenTick = now_tick;
        }

        for (uint32_t i = 0; i < counter_count; ++i)
        {
            UICounter* ui_counter = &out_ui_profile->m_Counters[i];

            if (ui_counter->m_Count == 0)
            {
                continue;
            }

            ui_counter->m_LastSeenTick = now_tick;
        }

        {
            uint32_t i = 0;
            while (i < scope_count)
            {
                UIScope* ui_scope = &out_ui_profile->m_Scopes[i];

                bool purge = (ui_scope->m_LastSeenTick + life_time) <= now_tick;

                if (purge)
                {
                    FreeScope(out_ui_profile, i);
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
                UISampleSum* ui_sample_sum = &out_ui_profile->m_SampleSums[i];
                TIndex* scope_index_ptr = out_ui_profile->m_ScopeLookup.Get(ui_sample_sum->m_ScopeHash);
                
                bool purge = scope_index_ptr == 0x0 || (ui_sample_sum->m_LastSeenTick + life_time) <= now_tick;

                if (purge)
                {
                    FreeSampleSum(out_ui_profile, i);
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
                UICounter* ui_counter = &out_ui_profile->m_Counters[i];

                bool purge = (ui_counter->m_LastSeenTick + life_time) <= now_tick;

                if (purge)
                {
                    FreeCounter(out_ui_profile, i);
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
        bool operator ()(const UIScope& a, const UIScope& b) const
        {
            return a.m_FilteredElapsed > b.m_FilteredElapsed;
        }
    };

    struct SampleSumSortPred
    {
        bool operator ()(const UISampleSum& a, const UISampleSum& b) const
        {
            return a.m_FilteredElapsed > b.m_FilteredElapsed;
        }
    };

    static void SortUIStructure(UIProfile* out_ui_profile)
    {
        if ((out_ui_profile->m_NowTick - out_ui_profile->m_LastSortTick) < out_ui_profile->m_SortInterval)
        {
            return;
        }
        {
            uint32_t scope_count = out_ui_profile->m_ScopeLookup.Size();

            std::sort(&out_ui_profile->m_Scopes[0], &out_ui_profile->m_Scopes[scope_count], ScopeSortPred());
            out_ui_profile->m_ScopeLookup.Clear();
            for (uint32_t i = 0; i < scope_count; ++i)
            {
                UIScope* ui_scope = &out_ui_profile->m_Scopes[i];
                out_ui_profile->m_ScopeLookup.Put(ui_scope->m_NameHash, i);
//                ui_scope->m_PeakElapsed = 0u;
            }
        }
        {
            uint32_t sample_sum_count = out_ui_profile->m_SampleSumLookup.Size();

            std::sort(&out_ui_profile->m_SampleSums[0], &out_ui_profile->m_SampleSums[sample_sum_count], SampleSumSortPred());
            out_ui_profile->m_SampleSumLookup.Clear();
            for (uint32_t i = 0; i < sample_sum_count; ++i)
            {
                UISampleSum* ui_sample_sum = &out_ui_profile->m_SampleSums[i];
                out_ui_profile->m_SampleSumLookup.Put(ui_sample_sum->m_NameHash, i);
//                ui_sample_sum->m_PeakElapsed = 0u;
            }
        }
        out_ui_profile->m_LastSortTick = out_ui_profile->m_NowTick;
    }

    HRenderProfile NewRenderProfile(float fps)
    {
        UIProfile* ui_profile = UIProfile::New(fps, 64, 128, 64, 16384); // Just shy of 208 Kb
        return (HRenderProfile)ui_profile;
    }

    void UpdateRenderProfile(HRenderProfile render_profile, dmProfile::HProfile profile)
    {
        UIProfile* ui_profile = (UIProfile*)render_profile;
        ResetUIStructure(ui_profile);
        BuildUIStructure(profile, ui_profile);
        PurgeUIStructure(ui_profile);
        SortUIStructure(ui_profile);
    }

    void DeleteRenderProfile(HRenderProfile render_profile)
    {
        UIProfile* ui_profile = (UIProfile*)render_profile;
        UIProfile::Delete(ui_profile);
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

        UIProfile* ui_profile = (UIProfile*)render_profile;
        assert(ui_profile != 0x0);

        dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(render_context);
        uint32_t display_width = dmGraphics::GetWindowWidth(graphics_context);
        uint32_t display_height = dmGraphics::GetWindowHeight(graphics_context);
        dmRender::Square2d(render_context, 0.0f, 0.0f, display_width, display_height, Vector4(0.1f, 0.1f, 0.1f, 0.5f));

        int y0 = display_height - g_Border_Spacing;

        char buffer[256];
        float col[3];

        dmRender::DrawTextParams params;
        params.m_Text = buffer;
        params.m_WorldTransform.setElem(3, 1, y0);
        params.m_FaceColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
        params.m_ShadowColor = Vector4(0.0f, 0.0f, 0.0f, 1.0f);

        {
            uint64_t ticks_per_second = dmProfile::GetTicksPerSecond();

            DM_SNPRINTF(buffer, 256, "Frame: %6.3f Max: %6.3f", dmProfile::GetFrameTime(), dmProfile::GetMaxFrameTime());
            params.m_WorldTransform.setElem(3, 0, g_Scope_x0);
            dmRender::DrawText(render_context, font_map, 0, 0, params);

            y0 -= g_Spacing;
            dmRender::Square2d(render_context, g_Sample_Frame_x0, y0 - g_Spacing, display_width - g_Border_Spacing, 0, Vector4(0.15f, 0.15f, 0.15f, 0.2f));

            params.m_WorldTransform.setElem(3, 1, y0);

            params.m_Text = ui_profile->m_ScopeOverflow ? "*Scopes:" : "Scopes:";
            params.m_WorldTransform.setElem(3, 0, g_Scope_x0);
            dmRender::DrawText(render_context, font_map, 0, 0, params);
            params.m_Text = "    ms";
            params.m_WorldTransform.setElem(3, 0, g_Scope_Time_x0);
            dmRender::DrawText(render_context, font_map, 0, 0, params);
            params.m_Text = "  #";
            params.m_WorldTransform.setElem(3, 0, g_Scope_Count_x0);
            dmRender::DrawText(render_context, font_map, 0, 0, params);
            params.m_Text = ui_profile->m_SampleSumOverflow ? "*Samples:" : "Samples:";
            params.m_WorldTransform.setElem(3, 0, g_Sample_x0);
            dmRender::DrawText(render_context, font_map, 0, 0, params);
            params.m_Text = "    ms";
            params.m_WorldTransform.setElem(3, 0, g_Sample_Time_x0);
            dmRender::DrawText(render_context, font_map, 0, 0, params);
            params.m_Text = "  #";
            params.m_WorldTransform.setElem(3, 0, g_Sample_Count_x0);
            dmRender::DrawText(render_context, font_map, 0, 0, params);
            params.m_Text = ui_profile->m_SampleOverflow ? "*Frame:" : "Frame:";
            params.m_WorldTransform.setElem(3, 0, g_Sample_Frame_x0);
            dmRender::DrawText(render_context, font_map, 0, 0, params);

            TIndex counter_count = (TIndex)ui_profile->m_CounterLookup.Size();
            int y = (counter_count + 1) * g_Spacing;

            params.m_WorldTransform.setElem(3, 1, y);

            params.m_Text = ui_profile->m_CounterOverflow ? "*Counters:" : "Counters:";
            params.m_WorldTransform.setElem(3, 0, g_Counter_x0);
            dmRender::DrawText(render_context, font_map, 0, 0, params);
            params.m_Text = "           #";
            params.m_WorldTransform.setElem(3, 0, g_Counter_Amount_x0);
            dmRender::DrawText(render_context, font_map, 0, 0, params);

            int start_y = y0;

            bool profile_valid = true;
            if (dmProfile::IsOutOfScopes())
            {
                profile_valid = false;
                params.m_WorldTransform.setElem(3, 0, g_Scope_x0);
                params.m_WorldTransform.setElem(3, 1, y0 - g_Spacing);
                params.m_FaceColor = Vector4(1.0f, 0.2f, 0.1f, 1.0f);
                params.m_Text = "Out of scopes!";
                dmRender::DrawText(render_context, font_map, 0, 0, params);
            }
            if (dmProfile::IsOutOfSamples())
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
            const bool draw_counters = true;

            if (draw_scopes)
            {
                start_y = y0 - g_Spacing;
                TIndex scope_count = (TIndex)ui_profile->m_ScopeLookup.Size();
                for (TIndex index = 0; index < scope_count; ++index)
                {
                    UIScope* scope = &ui_profile->m_Scopes[index];
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
                TIndex sample_sum_count = (TIndex)ui_profile->m_SampleSumLookup.Size();
                for (TIndex index = 0; index < sample_sum_count; ++index)
                {
                    UISampleSum* sample_sum = &ui_profile->m_SampleSums[index];

                    HslToRgb2( (sample_sum->m_ColorIndex % 16) / 15.0f, 1.0f, 0.6f, col);

                    double e = ((double)(sample_sum->m_Elapsed)) / ticks_per_second;

                    float y = start_y - (index * g_Spacing);

                    params.m_WorldTransform.setElem(3, 1, y);
                    params.m_FaceColor = Vector4(col[0], col[1], col[2], 1.0f);

                    params.m_Text = buffer;

                    UIScope* scope = &ui_profile->m_Scopes[*ui_profile->m_ScopeLookup.Get(sample_sum->m_ScopeHash)];
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
                    while (sample_index != ui_profile->m_MaxSampleCount)
                    {
                        UISample* sample = &ui_profile->m_Samples[sample_index];
                        
                        float frames_per_tick = ui_profile->m_FPS / ticks_per_second;
                        float x = g_Sample_Frame_x0 + sample_frame_width * sample->m_StartTick * frames_per_tick;
                        float w = sample_frame_width * sample->m_Elapsed * frames_per_tick;

                        dmRender::Square2d(render_context, x, y - g_Sample_Bar_Height, x + w, y, Vector4(col[0], col[1], col[2], 1));

                        sample_index = sample->m_PreviousSampleIndex;
                    }
                }
            }

            if (draw_counters)
            {
                TIndex counter_count = (TIndex)ui_profile->m_CounterLookup.Size();
                start_y = counter_count * g_Spacing;

                for (TIndex index = 0; index < counter_count; ++index)
                {
                    UICounter* counter = &ui_profile->m_Counters[index];

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
