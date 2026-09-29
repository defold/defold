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

static int32_t Movement_Defold(HJobContext, HJob, void*, void* data)
{
    ThreadedRange*        range = (ThreadedRange*)data;
    const ThreadedUpdate* update = range->m_Update;
    DataIterator          it = DataQueryIterRange(update->m_Query, range->m_First, range->m_Count);
    while (DataIterNext(&it) == DATA_RESULT_OK)
    {
        DataRowIterator rows = DataIterRows(&it);
        while (DataRowIterNext(&rows) == DATA_RESULT_OK)
        {
            DataVector3*       position = DataRowIterGetVector3Mut(&rows, update->m_PositionField);
            const DataVector3* velocity = DataRowIterGetVector3(&rows, update->m_VelocityField);
            for (uint32_t axis = 0; axis < 3; ++axis)
                position->m_Values[axis] += velocity->m_Values[axis] * 0.015625f;
            range->m_Stats.m_Sum += position->m_Values[0];
            ++range->m_Stats.m_Rows;
        }
    }
    return 0;
}

void Movement_Reference(ThreadedReferenceRow* row, ThreadedStats* stats)
{
    if (row->m_Type != 2 && row->m_Type != 3)
        return;
    for (uint32_t axis = 0; axis < 3; ++axis)
        row->m_Position.m_Values[axis] += row->m_Velocity.m_Values[axis] * 0.015625f;
    stats->m_Sum += row->m_Position.m_Values[0];
    ++stats->m_Rows;
}

void CreateThreadedMovement(HDataStore store, ThreadedUpdate* update)
{
    DataQueryField fields[] = {
        { .m_Field = THREAD_POSITION, .m_Type = DATA_VALUE_TYPE_VECTOR3, .m_Access = DATA_ACCESS_READ_WRITE },
        { .m_Field = THREAD_VELOCITY, .m_Type = DATA_VALUE_TYPE_VECTOR3 }
    };
    DataQueryDesc desc = { .m_Fields = fields, .m_FieldCount = 2 };
    ThreadedCheck(DataCreateQuery(store, &desc, &update->m_Query) == DATA_RESULT_OK, "movement query");
    update->m_PositionField = DataQueryFindField(update->m_Query, &fields[0]);
    update->m_VelocityField = DataQueryFindField(update->m_Query, &fields[1]);
    update->m_Process = Movement_Defold;
    update->m_Reference = Movement_Reference;
}
