// Copyright 2026 The Defold Foundation
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include "benchmark_data_threaded.h"

static double Contribution(const DataVector3* position, const DataVector3* color, double intensity)
{
    const float* p = position->m_Values;
    float        distance = p[0] * p[0] + p[1] * p[1] + p[2] * p[2];
    return ((double)color->m_Values[0] + color->m_Values[1] + color->m_Values[2]) * intensity * (1.0 - distance / 2500.0);
}

// Nearby lights: position is outside the inline Light struct; color/intensity inside.
static int32_t NearbyLights_Defold(HJobContext, HJob, void*, void* data)
{
    ThreadedRange*        range = (ThreadedRange*)data;
    const ThreadedUpdate* update = range->m_Update;
    DataIterator          it = DataQueryIterRange(update->m_Query, range->m_First, range->m_Count);
    while (DataIterNext(&it) == DATA_RESULT_OK)
    {
        DataRowIterator rows = DataIterRows(&it);
        while (DataRowIterNext(&rows) == DATA_RESULT_OK)
        {
            const DataVector3* position = DataFieldGetVector3(&rows, update->m_PositionField);
            if (ThreadedInRadius(position))
            {
                const DataVector3* color = DataFieldGetVector3(&rows, update->m_ColorField);
                double             intensity = *DataFieldGetNumber(&rows, update->m_IntensityField);
                range->m_Stats.m_Sum += Contribution(position, color, intensity);
                ++range->m_Stats.m_Hits;
            }
            ++range->m_Stats.m_Rows;
        }
    }
    return 0;
}

void NearbyLights_Reference(ThreadedReferenceRow* row, ThreadedStats* stats)
{
    if (row->m_Type > 1)
        return;
    if (ThreadedInRadius(&row->m_Position))
    {
        stats->m_Sum += Contribution(&row->m_Position, &row->m_Color, row->m_Intensity);
        ++stats->m_Hits;
    }
    ++stats->m_Rows;
}

void CreateThreadedLights(HDataStore store, ThreadedUpdate* update)
{
    uint64_t       color = THREAD_COLOR, intensity = THREAD_INTENSITY;
    DataQueryField fields[] = {
        { .m_Field = THREAD_POSITION, .m_Type = DATA_VALUE_TYPE_VECTOR3 },
        { .m_Field = THREAD_LIGHT, .m_Type = DATA_VALUE_TYPE_VECTOR3, .m_Path = &color, .m_PathCount = 1 },
        { .m_Field = THREAD_LIGHT, .m_Type = DATA_VALUE_TYPE_NUMBER, .m_Path = &intensity, .m_PathCount = 1 }
    };
    DataQueryDesc desc = { .m_Fields = fields, .m_FieldCount = 3 };
    ThreadedCheck(DataCreateQuery(store, &desc, &update->m_Query) == DATA_RESULT_OK, "nearby lights query");
    update->m_PositionField = DataQueryFindField(update->m_Query, &fields[0]);
    update->m_ColorField = DataQueryFindField(update->m_Query, &fields[1]);
    update->m_IntensityField = DataQueryFindField(update->m_Query, &fields[2]);
    update->m_Process = NearbyLights_Defold;
    update->m_Reference = NearbyLights_Reference;
}
