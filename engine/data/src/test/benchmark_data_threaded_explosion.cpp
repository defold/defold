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

// Radius 50 at the origin; subtract 25 health, clamped to zero.
static int32_t Explosion_Defold(HJobContext, HJob, void*, void* data)
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
            double*            health = DataFieldGetNumberMut(&rows, update->m_HealthField);
            if (ThreadedInRadius(position))
            {
                *health = *health > 25.0 ? *health - 25.0 : 0.0;
                ++range->m_Stats.m_Hits;
            }
            range->m_Stats.m_Sum += *health;
            ++range->m_Stats.m_Rows;
        }
    }
    return 0;
}

void Explosion_Reference(ThreadedReferenceRow* row, ThreadedStats* stats)
{
    if (!ThreadedHasHealth(row->m_Type))
        return;
    if (ThreadedInRadius(&row->m_Position))
    {
        row->m_Health = row->m_Health > 25.0 ? row->m_Health - 25.0 : 0.0;
        ++stats->m_Hits;
    }
    stats->m_Sum += row->m_Health;
    ++stats->m_Rows;
}

void CreateThreadedExplosion(HDataStore store, ThreadedUpdate* update)
{
    DataQueryField fields[] = {
        { .m_Field = THREAD_POSITION, .m_Type = DATA_VALUE_TYPE_VECTOR3 },
        { .m_Field = THREAD_HEALTH, .m_Type = DATA_VALUE_TYPE_NUMBER, .m_Access = DATA_ACCESS_READ_WRITE }
    };
    DataQueryDesc desc = { .m_Fields = fields, .m_FieldCount = 2 };
    ThreadedCheck(DataCreateQuery(store, &desc, &update->m_Query) == DATA_RESULT_OK, "explosion query");
    update->m_PositionField = DataQueryFindField(update->m_Query, &fields[0]);
    update->m_HealthField = DataQueryFindField(update->m_Query, &fields[1]);
    update->m_Process = Explosion_Defold;
    update->m_Reference = Explosion_Reference;
}
