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

#include <dlib/align.h>
#include <jc_test/jc_test.h>
#include "test_data_memory.h"

struct PageRow
{
    double           m_Health;
    DataStoreVector3 m_Position;
    uint32_t         m_Color;
};

struct PageReferenceRow
{
    double             m_Health;
    DataStoreReference m_Name;
};

static DataStoreResult RegisterPageRows(HDataStoreContext store, uint64_t type, DataStoreResetPolicy reset)
{
    DataStoreFieldDesc fields[] = {
        { 1, DATASTORE_TYPE_NUMBER, offsetof(PageRow, m_Health) },
        { 2, DATASTORE_TYPE_VECTOR3, offsetof(PageRow, m_Position) },
        { 3, DATASTORE_TYPE_UINT32, offsetof(PageRow, m_Color) }
    };
    DataStoreTableDesc desc = { type, 0, 0, fields, 3, sizeof(PageRow), reset };
    return DataStoreTableAdd(store, &desc);
}

static uint32_t PageCount(HDataStoreContext store)
{
    uint32_t count = 0;
    for (uint32_t i = 0; i < store->m_Pages.Size(); ++i)
    {
        count += store->m_Pages[i].m_Data != 0;
    }
    return count;
}

// Allocation settings are validated once; zero sizes select defaults and do not
// allocate backing pages before values are inserted.
TEST(DataPages, Parameters)
{
    DataStoreCreateParams invalid[] = {
        { 128, 256 },
        { 768, 64 },
        { 256, 96 },
        { 256, 32 }
    };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(invalid); ++i)
    {
        ASSERT_EQ((HDataStoreContext)0, DataStoreCreate(&invalid[i]));
    }
    DataStoreCreateParams defaults = {};
    HDataStoreContext     store = DataStoreCreate(&defaults);
    ASSERT_NE((HDataStoreContext)0, store);
    ASSERT_EQ(0u, PageCount(store));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Bulk input, SoA input, row-pointer writes, irregular query ranges and swaps
// must all cross chunk boundaries without moving values to the wrong ID.
TEST(DataPages, GrowthRangesSwapAndReset)
{
    DataStoreCreateParams params = { 2048, 256, 1 };
    HDataStoreContext     store = DataStoreCreate(&params);
    DataStoreId           group = NextTestGroupId();
    ASSERT_EQ(DATASTORE_RESULT_OK, RegisterPageRows(store, 10, DATASTORE_RESET_RETAIN));
    PageRow     input[259];
    DataStoreId ids[259];
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(input); ++i)
    {
        input[i] = PageRow { (double)i, { { (float)i, 2, 3 } }, i };
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 10, 0, 1, input, ids));
    DataStoreTable* table = FindTable(store, 10);
    const uint8_t*  first = GetRowBytes(table, 0);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 10, group, 256, input + 1, ids + 1));
    ASSERT_EQ(first, GetRowBytes(table, 0));
    double              health[] = { 257, 258 };
    DataStoreVector3    positions[] = { input[257].m_Position, input[258].m_Position };
    uint32_t            colors[] = { 257, 258 };
    DataStoreFieldArray columns[] = {
        { 3, colors },
        { 1, health },
        { 2, positions }
    };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreateSoA(store, 10, group, 2, 3, columns, ids + 257));
    DataStoreQueryDesc desc = { { 0 }, &group, 1, DATASTORE_ROW_MUTABLE };
    HDataStoreQuery    query;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &desc, &query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    ASSERT_EQ(258u, DataStoreQueryGetRowCount(query));
    uint32_t visited = 0;
    for (uint32_t start = 0; start < 258; start += 7)
    {
        uint32_t          count = 258 - start < 7 ? 258 - start : 7;
        DataStoreIterator it    = DataStoreQueryIterRange(query, start, count);
        while (DataStoreIterNext(&it))
        {
            DataStoreRowIterator rows = DataStoreIterRows(&it);
            while (DataStoreRowIterNext(&rows))
            {
                ++visited;
                PageRow* value = (PageRow*)DataStoreRowIterGetDataMut(&rows);
                ASSERT_EQ(ids[visited], DataStoreRowIterGetId(&rows));
                ASSERT_EQ((double)visited, value->m_Health);
                ASSERT_EQ(visited, value->m_Color);
                value->m_Health = -1;
            }
        }
    }
    ASSERT_EQ(258u, visited);
    DataStoreQueryEnd(query);
    for (uint32_t i = 0; i < 259; ++i)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowReset(store, ids[i]));
        double value;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetNumber(store, ids[i], 1, &value));
        ASSERT_EQ((double)i, value);
    }
    // 97 is coprime to 259; each removal targets a distinct original ID.
    for (uint32_t i = 0; i < 259; ++i)
    {
        uint32_t index = (i * 97) % 259;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, ids[index]));
        double value;
        ASSERT_EQ(DATASTORE_RESULT_NOT_FOUND, DataStoreFieldGetNumber(store, ids[index], 1, &value));
        ASSERT_EQ((table->m_Rows.Size() + table->m_ChunkMask) >> table->m_ChunkShift, table->m_Chunks.Size());
        if (!table->m_Rows.Empty())
        {
            DataStoreId live = MakeId(store, table->m_Rows.Back().m_Slot);
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSetNumber(store, live, 1, -2));
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowReset(store, live));
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetNumber(store, live, 1, &value));
            uint32_t color;
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetUInt32(store, live, 3, &color));
            ASSERT_EQ((double)color, value);
        }
    }
    ASSERT_TRUE(table->m_Chunks.Empty());
    ASSERT_EQ(1u, PageCount(store));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Whole pages are reclaimed when chunks empty, one spare is retained, and page
// descriptor slots are reused across tables without invalidating other chunks.
TEST(DataPages, SharedPagesAndReuse)
{
    DataStoreCreateParams params = { 1024, 256, 1 };
    HDataStoreContext     store = DataStoreCreate(&params);
    PageRow               input[129] = {};
    for (uint32_t i = 0; i < 129; ++i)
    {
        input[i].m_Health = i;
    }
    uint32_t slots = 0;
    for (uint32_t wave = 0; wave < 8; ++wave)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, RegisterPageRows(store, 10, DATASTORE_RESET_NONE));
        ASSERT_EQ(DATASTORE_RESULT_OK, RegisterPageRows(store, 20, DATASTORE_RESET_RETAIN));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 10, 0, 129, input, 0));
        DataStoreId live;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 20, 0, 1, input + 128, &live));
        if (!wave)
        {
            slots = store->m_Pages.Size();
        }
        ASSERT_EQ(slots, store->m_Pages.Size());
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableRemove(store, 10));
        double value;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetNumber(store, live, 1, &value));
        ASSERT_EQ(128.0, value);
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowReset(store, live));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableRemove(store, 20));
        ASSERT_EQ(1u, PageCount(store));
        ASSERT_EQ(1u, store->m_SparePageCount);
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Rows larger than a configured chunk use dedicated storage and preserve reset;
// this must not overflow page offsets or consume a partially occupied page.
TEST(DataPages, OversizedRows)
{
    DataStoreCreateParams params = { 256, 64 };
    HDataStoreContext     store = DataStoreCreate(&params);
    double                input[3][32] = {};
    input[0][31] = 11;
    input[1][31] = 22;
    input[2][31] = 33;
    DataStoreFieldDesc field = { 1, DATASTORE_TYPE_NUMBER, 31 * sizeof(double) };
    DataStoreTableDesc desc = { 10, 0, 0, &field, 1, sizeof(input[0]), DATASTORE_RESET_RETAIN };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(store, &desc));
    DataStoreId ids[3];
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 10, 0, 3, input, ids));
    ASSERT_EQ(0u, PageCount(store));
    ASSERT_EQ(3u, FindTable(store, 10)->m_Chunks.Size());
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowRemove(store, ids[1]));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSetNumber(store, ids[2], 1, -1));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowReset(store, ids[2]));
    double value;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetNumber(store, ids[2], 1, &value));
    ASSERT_EQ(33.0, value);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
}

// Blob instantiation copies across chunks while resource offsets stay immutable.
// Replacement strings, reset and removing registrations must preserve ownership.
TEST(DataPages, BlobReferencesAcrossChunks)
{
    DataStoreCreateParams params = { 512, 128, 1 };
    HDataStoreContext     source = DataStoreCreate(&params);
    DataStoreFieldDesc    fields[] = {
        { 1, DATASTORE_TYPE_NUMBER, offsetof(PageReferenceRow, m_Health) },
        { 2, DATASTORE_TYPE_STRING, offsetof(PageReferenceRow, m_Name) }
    };
    DataStoreTableDesc table = { 10, 0, 0, fields, 2, sizeof(PageReferenceRow) };
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableAdd(source, &table));
    PageReferenceRow input[41];
    for (uint32_t i = 0; i < 41; ++i)
    {
        input[i] = PageReferenceRow { (double)i, { "enemy" } };
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(source, 10, 0, 41, input, 0));
    uint8_t DM_ALIGNED(8) bytes[4096];
    uint32_t size;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobWrite(source, bytes, sizeof(bytes), &size));
    HDataStoreBlob blob;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobLoad(bytes, size, &blob));
    HDataStoreContext      store = DataStoreCreate(&params);
    HDataStoreBlobInstance instances[3];
    for (uint32_t i = 0; i < 3; ++i)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobAdd(store, blob, 0, &instances[i]));
    }
    DataStoreQueryDesc desc = { { { 1, DATASTORE_TYPE_NUMBER, 0, DATASTORE_QUERY_ITEM_FIELD } }, 0, 0, 0 };
    HDataStoreQuery    query;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryCreate(store, &desc, &query));
    DataStoreFieldId health = DataStoreQueryFindField(query, &desc.m_Items[0]);
    DataStoreId      ids[123];
    uint32_t         count = 0;
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    DataStoreIterator it = DataStoreQueryIter(query);
    while (DataStoreIterNext(&it))
    {
        DataStoreRowIterator rows = DataStoreIterRows(&it);
        while (DataStoreRowIterNext(&rows))
        {
            ids[count] = DataStoreRowIterGetId(&rows);
            ASSERT_EQ((double)(count % 41), *DataStoreRowIterGetNumber(&rows, health));
            ++count;
        }
    }
    DataStoreQueryEnd(query);
    ASSERT_EQ(123u, count);
    for (uint32_t i = 0; i < count; ++i)
    {
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSetString(store, ids[i], 2, "changed"));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldSetNumber(store, ids[i], 1, -1));
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowReset(store, ids[i]));
        const char* name;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetString(store, ids[i], 2, &name));
        ASSERT_STREQ("enemy", name);
        double value;
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreFieldGetNumber(store, ids[i], 1, &value));
        ASSERT_EQ((double)(i % 41), value);
    }
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobRemove(instances[1]));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryTryBegin(query));
    ASSERT_EQ(82u, DataStoreQueryGetRowCount(query));
    DataStoreQueryEnd(query);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobRemove(instances[0]));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreBlobRemove(instances[2]));
    ASSERT_EQ(1u, PageCount(store));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreQueryDestroy(query));
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
    DataStoreBlobDestroy(blob);
    ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(source));
}

// The configured spare budget must remove allocation churn without retaining
// more empty pages than requested; zero releases every backing allocation.
TEST(DataPages, SparePageBudget)
{
    PageRow values[129] = {};
    for (uint32_t budget = 0; budget <= 4; budget += 4)
    {
        DataStoreCreateParams params = { 1024, 256, budget };
        HDataStoreContext     store = DataStoreCreate(&params);
        for (uint32_t wave = 0; wave < 3; ++wave)
        {
            ASSERT_EQ(DATASTORE_RESULT_OK, RegisterPageRows(store, 10, DATASTORE_RESET_NONE));
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreRowCreate(store, 10, 0, 129, values, 0));
            ASSERT_GT(PageCount(store), budget);
            ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreTableRemove(store, 10));
            ASSERT_EQ(budget, PageCount(store));
            ASSERT_EQ(budget, store->m_SparePageCount);
        }
        ASSERT_EQ(DATASTORE_RESULT_OK, DataStoreDestroy(store));
    }
}
