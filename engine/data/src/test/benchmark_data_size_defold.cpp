// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
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

#include <stdio.h>
#include <stdlib.h>

#include <stdint.h>

#include <dmsdk/data/data.h>

static void Check(DataResult result)
{
    if (result != DATA_RESULT_OK)
        abort();
}

int main(int argc, char** argv)
{
    uint32_t      count = argc > 1 ? (uint32_t)strtoul(argv[1], 0, 10) : 100;
    HDataStore    store = DataCreateStore();
    DataFieldDesc fields[] = {
        { .m_Field = 1, .m_Type = DATA_TYPE_VECTOR3, .m_Offset = 0 },
        { .m_Field = 2, .m_Type = DATA_TYPE_NUMBER, .m_Offset = 16 }
    };
    DataTableDesc table = {
        .m_Type = 1,
        .m_Fields = fields,
        .m_FieldCount = 2,
        .m_RowStride = 24
    };
    Check(DataRegisterTable(store, &table));
    struct InitialRow
    {
        DataVector3 m_Position;
        double      m_Health;
    };
    const InitialRow    values = { .m_Position = {}, .m_Health = 100 };
    DataId*             ids = new DataId[count];
    for (uint32_t row = 0; row < count; ++row)
    {
        Check(DataCreateRows(store, 1, row, 1, &values, &ids[row]));
    }
    DataQueryField query_fields[] = {
        { .m_Field = 1, .m_Type = DATA_TYPE_VECTOR3 },
        { .m_Field = 2, .m_Type = DATA_TYPE_NUMBER, .m_Access = DATA_ACCESS_READ_WRITE }
    };
    DataQueryDesc desc = { .m_Fields = query_fields, .m_FieldCount = 2 };
    HDataQuery    query;
    Check(DataCreateQuery(store, &desc, &query));
    uint32_t position_field = DataQueryFindField(query, &query_fields[0]);
    uint32_t health_field = DataQueryFindField(query, &query_fields[1]);
    Check(DataQueryTryBegin(query));
    DataIterator it = DataQueryIterRange(query, 0, DataQueryGetRowCount(query));
    while (DataIterNext(&it) == DATA_RESULT_OK)
    {
        DataRowIterator rows = DataIterRows(&it);
        while (DataRowIterNext(&rows) == DATA_RESULT_OK)
        {
            const DataVector3* position = DataRowIterGetVector3(&rows, position_field);
            if (position->m_Values[0] < 50)
                *DataRowIterGetNumberMut(&rows, health_field) -= 25;
        }
    }
    DataQueryEnd(query);
    double sum = 0;
    for (uint32_t row = 0; row < count; ++row)
    {
        double health;
        Check(DataFieldGetNumber(store, ids[row], 2, &health));
        sum += health;
        Check(DataRemoveRow(store, ids[row]));
    }
    DataDestroyQuery(query);
    DataDestroyStore(store);
    delete[] ids;
    printf("%.0f\n", sum);
    return sum != (double)count * 75;
}
