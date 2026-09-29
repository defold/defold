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

// Competes with Explosion for health; neither task has a dependency on the other.
static int32_t Regenerate_Defold(HJobContext, HJob, void*, void* data)
{
    ThreadedRange*        range = (ThreadedRange*)data;
    const ThreadedUpdate* update = range->m_Update;
    DataIterator          it = DataQueryIterRange(update->m_Query, range->m_First, range->m_Count);
    while (DataIterNext(&it) == DATA_RESULT_OK)
    {
        DataRowIterator rows = DataIterRows(&it);
        while (DataRowIterNext(&rows) == DATA_RESULT_OK)
        {
            double* health = DataRowIterGetNumberMut(&rows, update->m_HealthField);
            *health = *health < 99.75 ? *health + 0.25 : 100.0;
            range->m_Stats.m_Sum += *health;
            ++range->m_Stats.m_Rows;
        }
    }
    return 0;
}

void Regenerate_Reference(ThreadedReferenceRow* row, ThreadedStats* stats)
{
    if (!ThreadedHasHealth(row->m_Type))
        return;
    row->m_Health = row->m_Health < 99.75 ? row->m_Health + 0.25 : 100.0;
    stats->m_Sum += row->m_Health;
    ++stats->m_Rows;
}

void CreateThreadedRegenerate(HDataStore store, ThreadedUpdate* update)
{
    DataQueryField field = { .m_Field = THREAD_HEALTH, .m_Type = DATA_VALUE_TYPE_NUMBER, .m_Access = DATA_ACCESS_READ_WRITE };
    DataQueryDesc  desc = { .m_Fields = &field, .m_FieldCount = 1 };
    ThreadedCheck(DataCreateQuery(store, &desc, &update->m_Query) == DATA_RESULT_OK, "regenerate query");
    update->m_HealthField = DataQueryFindField(update->m_Query, &field);
    update->m_Process = Regenerate_Defold;
    update->m_Reference = Regenerate_Reference;
}
