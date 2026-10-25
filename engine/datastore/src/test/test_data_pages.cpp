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
    double    m_Health;
    DSVector3 m_Position;
    uint32_t  m_Color;
};

struct PageReferenceRow
{
    double      m_Health;
    DSReference m_Name;
};

static DSResult RegisterPageRows(HDSContext store, uint64_t type, DSResetPolicy reset)
{
    DSFieldDesc fields[] = {
        { .m_Field = 1, .m_Type = DS_TYPE_NUMBER, .m_Offset = offsetof(PageRow, m_Health) },
        { .m_Field = 2, .m_Type = DS_TYPE_VECTOR3, .m_Offset = offsetof(PageRow, m_Position) },
        { .m_Field = 3, .m_Type = DS_TYPE_UINT32, .m_Offset = offsetof(PageRow, m_Color) }
    };
    DSTableDesc desc = { .m_Type = type, .m_Fields = fields, .m_FieldCount = 3, .m_RowStride = sizeof(PageRow), .m_ResetPolicy = reset };
    return DSTableAdd(store, &desc);
}

static uint32_t PageCount(HDSContext store)
{
    uint32_t count = 0;
    for (uint32_t i = 0; i < store->m_Pages.Size(); ++i)
        count += store->m_Pages[i].m_Data != 0;
    return count;
}

// Allocation settings are validated once; zero sizes select defaults and do not
// allocate backing pages before values are inserted.
TEST(DataPages, Parameters)
{
    DSCreateParams invalid[] = {
        { .m_PageSize = 128, .m_RowChunkSize = 256 },
        { .m_PageSize = 768, .m_RowChunkSize = 64 },
        { .m_PageSize = 256, .m_RowChunkSize = 96 },
        { .m_PageSize = 256, .m_RowChunkSize = 32 }
    };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(invalid); ++i)
        ASSERT_EQ((HDSContext)0, DSStoreCreate(&invalid[i]));
    DSCreateParams defaults = {};
    HDSContext     store = DSStoreCreate(&defaults);
    ASSERT_NE((HDSContext)0, store);
    ASSERT_EQ(0u, PageCount(store));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
}

// Bulk input, SoA input, row-pointer writes, irregular query ranges and swaps
// must all cross chunk boundaries without moving values to the wrong ID.
TEST(DataPages, GrowthRangesSwapAndReset)
{
    DSCreateParams params = { .m_PageSize = 2048, .m_RowChunkSize = 256, .m_MaxSparePages = 1 };
    HDSContext     store = DSStoreCreate(&params);
    DSId           group = NextTestGroupId();
    ASSERT_EQ(DS_RESULT_OK, RegisterPageRows(store, 10, DS_RESET_RETAIN));
    PageRow input[259];
    DSId    ids[259];
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(input); ++i)
        input[i] = PageRow { .m_Health = (double)i, .m_Position = { { (float)i, 2, 3 } }, .m_Color = i };
    ASSERT_EQ(DS_RESULT_OK, DSRowCreate(store, 10, 0, 1, input, ids));
    DSTable*       table = FindTable(store, 10);
    const uint8_t* first = GetRowBytes(table, 0);
    ASSERT_EQ(DS_RESULT_OK, DSRowCreate(store, 10, group, 256, input + 1, ids + 1));
    ASSERT_EQ(first, GetRowBytes(table, 0));
    double       health[] = { 257, 258 };
    DSVector3    positions[] = { input[257].m_Position, input[258].m_Position };
    uint32_t     colors[] = { 257, 258 };
    DSFieldArray columns[] = {
        { .m_Field = 3, .m_Values = colors },
        { .m_Field = 1, .m_Values = health },
        { .m_Field = 2, .m_Values = positions }
    };
    ASSERT_EQ(DS_RESULT_OK, DSRowCreateSoA(store, 10, group, 2, 3, columns, ids + 257));
    DSQueryDesc desc = { .m_GroupIds = &group, .m_GroupIdCount = 1, .m_Flags = DS_ROW_MUTABLE };
    HDSQuery    query;
    ASSERT_EQ(DS_RESULT_OK, DSQueryCreate(store, &desc, &query));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    ASSERT_EQ(258u, DSQueryGetRowCount(query));
    uint32_t visited = 0;
    for (uint32_t start = 0; start < 258; start += 7)
    {
        uint32_t   count = 258 - start < 7 ? 258 - start : 7;
        DSIterator it = DSQueryIterRange(query, start, count);
        while (DSIterNext(&it))
        {
            DSRowIterator rows = DSIterRows(&it);
            while (DSRowIterNext(&rows))
            {
                ++visited;
                PageRow* value = (PageRow*)DSRowIterGetDataMut(&rows);
                ASSERT_EQ(ids[visited], DSRowIterGetId(&rows));
                ASSERT_EQ((double)visited, value->m_Health);
                ASSERT_EQ(visited, value->m_Color);
                value->m_Health = -1;
            }
        }
    }
    ASSERT_EQ(258u, visited);
    DSQueryEnd(query);
    for (uint32_t i = 0; i < 259; ++i)
    {
        ASSERT_EQ(DS_RESULT_OK, DSRowReset(store, ids[i]));
        double value;
        ASSERT_EQ(DS_RESULT_OK, DSFieldGetNumber(store, ids[i], 1, &value));
        ASSERT_EQ((double)i, value);
    }
    // 97 is coprime to 259; each removal targets a distinct original ID.
    for (uint32_t i = 0; i < 259; ++i)
    {
        uint32_t index = (i * 97) % 259;
        ASSERT_EQ(DS_RESULT_OK, DSRowRemove(store, ids[index]));
        double value;
        ASSERT_EQ(DS_RESULT_NOT_FOUND, DSFieldGetNumber(store, ids[index], 1, &value));
        ASSERT_EQ((table->m_Rows.Size() + table->m_ChunkMask) >> table->m_ChunkShift, table->m_Chunks.Size());
        if (!table->m_Rows.Empty())
        {
            DSId live = MakeId(store, table->m_Rows.Back().m_Slot);
            ASSERT_EQ(DS_RESULT_OK, DSFieldSetNumber(store, live, 1, -2));
            ASSERT_EQ(DS_RESULT_OK, DSRowReset(store, live));
            ASSERT_EQ(DS_RESULT_OK, DSFieldGetNumber(store, live, 1, &value));
            uint32_t color;
            ASSERT_EQ(DS_RESULT_OK, DSFieldGetUInt32(store, live, 3, &color));
            ASSERT_EQ((double)color, value);
        }
    }
    ASSERT_TRUE(table->m_Chunks.Empty());
    ASSERT_EQ(1u, PageCount(store));
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(query));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
}

// Whole pages are reclaimed when chunks empty, one spare is retained, and page
// descriptor slots are reused across tables without invalidating other chunks.
TEST(DataPages, SharedPagesAndReuse)
{
    DSCreateParams params = { .m_PageSize = 1024, .m_RowChunkSize = 256, .m_MaxSparePages = 1 };
    HDSContext     store = DSStoreCreate(&params);
    PageRow        input[129] = {};
    for (uint32_t i = 0; i < 129; ++i)
        input[i].m_Health = i;
    uint32_t slots = 0;
    for (uint32_t wave = 0; wave < 8; ++wave)
    {
        ASSERT_EQ(DS_RESULT_OK, RegisterPageRows(store, 10, DS_RESET_NONE));
        ASSERT_EQ(DS_RESULT_OK, RegisterPageRows(store, 20, DS_RESET_RETAIN));
        ASSERT_EQ(DS_RESULT_OK, DSRowCreate(store, 10, 0, 129, input, 0));
        DSId live;
        ASSERT_EQ(DS_RESULT_OK, DSRowCreate(store, 20, 0, 1, input + 128, &live));
        if (!wave)
            slots = store->m_Pages.Size();
        ASSERT_EQ(slots, store->m_Pages.Size());
        ASSERT_EQ(DS_RESULT_OK, DSTableRemove(store, 10));
        double value;
        ASSERT_EQ(DS_RESULT_OK, DSFieldGetNumber(store, live, 1, &value));
        ASSERT_EQ(128.0, value);
        ASSERT_EQ(DS_RESULT_OK, DSRowReset(store, live));
        ASSERT_EQ(DS_RESULT_OK, DSTableRemove(store, 20));
        ASSERT_EQ(1u, PageCount(store));
        ASSERT_EQ(1u, store->m_SparePageCount);
    }
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
}

// Rows larger than a configured chunk use dedicated storage and preserve reset;
// this must not overflow page offsets or consume a partially occupied page.
TEST(DataPages, OversizedRows)
{
    DSCreateParams params = { .m_PageSize = 256, .m_RowChunkSize = 64 };
    HDSContext     store = DSStoreCreate(&params);
    double         input[3][32] = {};
    input[0][31] = 11;
    input[1][31] = 22;
    input[2][31] = 33;
    DSFieldDesc field = { .m_Field = 1, .m_Type = DS_TYPE_NUMBER, .m_Offset = 31 * sizeof(double) };
    DSTableDesc desc = { .m_Type = 10, .m_Fields = &field, .m_FieldCount = 1, .m_RowStride = sizeof(input[0]), .m_ResetPolicy = DS_RESET_RETAIN };
    ASSERT_EQ(DS_RESULT_OK, DSTableAdd(store, &desc));
    DSId ids[3];
    ASSERT_EQ(DS_RESULT_OK, DSRowCreate(store, 10, 0, 3, input, ids));
    ASSERT_EQ(0u, PageCount(store));
    ASSERT_EQ(3u, FindTable(store, 10)->m_Chunks.Size());
    ASSERT_EQ(DS_RESULT_OK, DSRowRemove(store, ids[1]));
    ASSERT_EQ(DS_RESULT_OK, DSFieldSetNumber(store, ids[2], 1, -1));
    ASSERT_EQ(DS_RESULT_OK, DSRowReset(store, ids[2]));
    double value;
    ASSERT_EQ(DS_RESULT_OK, DSFieldGetNumber(store, ids[2], 1, &value));
    ASSERT_EQ(33.0, value);
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
}

// Blob instantiation copies across chunks while resource offsets stay immutable.
// Replacement strings, reset and removing registrations must preserve ownership.
TEST(DataPages, BlobReferencesAcrossChunks)
{
    DSCreateParams params = { .m_PageSize = 512, .m_RowChunkSize = 128, .m_MaxSparePages = 1 };
    HDSContext     source = DSStoreCreate(&params);
    DSFieldDesc    fields[] = {
        { .m_Field = 1, .m_Type = DS_TYPE_NUMBER, .m_Offset = offsetof(PageReferenceRow, m_Health) },
        { .m_Field = 2, .m_Type = DS_TYPE_STRING, .m_Offset = offsetof(PageReferenceRow, m_Name) }
    };
    DSTableDesc table = { .m_Type = 10, .m_Fields = fields, .m_FieldCount = 2, .m_RowStride = sizeof(PageReferenceRow) };
    ASSERT_EQ(DS_RESULT_OK, DSTableAdd(source, &table));
    PageReferenceRow input[41];
    for (uint32_t i = 0; i < 41; ++i)
        input[i] = PageReferenceRow { .m_Health = (double)i, .m_Name = { .m_String = "enemy" } };
    ASSERT_EQ(DS_RESULT_OK, DSRowCreate(source, 10, 0, 41, input, 0));
    uint8_t  DM_ALIGNED(8) bytes[4096];
    uint32_t size;
    ASSERT_EQ(DS_RESULT_OK, DSBlobWrite(source, bytes, sizeof(bytes), &size));
    HDSBlob blob;
    ASSERT_EQ(DS_RESULT_OK, DSBlobLoad(bytes, size, &blob));
    HDSContext      store = DSStoreCreate(&params);
    HDSBlobInstance instances[3];
    for (uint32_t i = 0; i < 3; ++i)
        ASSERT_EQ(DS_RESULT_OK, DSBlobAdd(store, blob, 0, &instances[i]));
    DSQueryDesc desc = { .m_Items = { { .m_Hash = 1, .m_Type = DS_TYPE_NUMBER, .m_Kind = DS_QUERY_ITEM_FIELD } } };
    HDSQuery    query;
    ASSERT_EQ(DS_RESULT_OK, DSQueryCreate(store, &desc, &query));
    DSFieldId health = DSQueryFindField(query, &desc.m_Items[0]);
    DSId     ids[123];
    uint32_t count = 0;
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    DSIterator it = DSQueryIter(query);
    while (DSIterNext(&it))
    {
        DSRowIterator rows = DSIterRows(&it);
        while (DSRowIterNext(&rows))
        {
            ids[count] = DSRowIterGetId(&rows);
            ASSERT_EQ((double)(count % 41), *DSRowIterGetNumber(&rows, health));
            ++count;
        }
    }
    DSQueryEnd(query);
    ASSERT_EQ(123u, count);
    for (uint32_t i = 0; i < count; ++i)
    {
        ASSERT_EQ(DS_RESULT_OK, DSFieldSetString(store, ids[i], 2, "changed"));
        ASSERT_EQ(DS_RESULT_OK, DSFieldSetNumber(store, ids[i], 1, -1));
        ASSERT_EQ(DS_RESULT_OK, DSRowReset(store, ids[i]));
        const char* name;
        ASSERT_EQ(DS_RESULT_OK, DSFieldGetString(store, ids[i], 2, &name));
        ASSERT_STREQ("enemy", name);
        double value;
        ASSERT_EQ(DS_RESULT_OK, DSFieldGetNumber(store, ids[i], 1, &value));
        ASSERT_EQ((double)(i % 41), value);
    }
    ASSERT_EQ(DS_RESULT_OK, DSBlobRemove(instances[1]));
    ASSERT_EQ(DS_RESULT_OK, DSQueryTryBegin(query));
    ASSERT_EQ(82u, DSQueryGetRowCount(query));
    DSQueryEnd(query);
    ASSERT_EQ(DS_RESULT_OK, DSBlobRemove(instances[0]));
    ASSERT_EQ(DS_RESULT_OK, DSBlobRemove(instances[2]));
    ASSERT_EQ(1u, PageCount(store));
    ASSERT_EQ(DS_RESULT_OK, DSQueryDestroy(query));
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
    DSBlobDestroy(blob);
    ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(source));
}

// The configured spare budget must remove allocation churn without retaining
// more empty pages than requested; zero releases every backing allocation.
TEST(DataPages, SparePageBudget)
{
    PageRow values[129] = {};
    for (uint32_t budget = 0; budget <= 4; budget += 4)
    {
        DSCreateParams params = { .m_PageSize = 1024, .m_RowChunkSize = 256, .m_MaxSparePages = budget };
        HDSContext     store = DSStoreCreate(&params);
        for (uint32_t wave = 0; wave < 3; ++wave)
        {
            ASSERT_EQ(DS_RESULT_OK, RegisterPageRows(store, 10, DS_RESET_NONE));
            ASSERT_EQ(DS_RESULT_OK, DSRowCreate(store, 10, 0, 129, values, 0));
            ASSERT_GT(PageCount(store), budget);
            ASSERT_EQ(DS_RESULT_OK, DSTableRemove(store, 10));
            ASSERT_EQ(budget, PageCount(store));
            ASSERT_EQ(budget, store->m_SparePageCount);
        }
        ASSERT_EQ(DS_RESULT_OK, DSStoreDestroy(store));
    }
}
