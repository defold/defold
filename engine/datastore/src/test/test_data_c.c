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

#include <stddef.h>
#include <string.h>
#include <dmsdk/datastore/datastore.h>
#include <dmsdk/dlib/hash.h>

struct ReferenceAttributes
{
    DSReference   m_Faction;
    double        m_AlertRadius;
};

struct ReferenceEnemy
{
    DSReference                m_Name;
    DSReference                m_Attributes;
    DSReference                m_Loot;
    struct ReferenceAttributes m_State;
};

static void CountScheduledRowsFromC(HDSQuery query, uint32_t first, uint32_t num_rows, void* context)
{
    (void)query;
    (void)first;
    *(uint32_t*)context += num_rows;
}

int TestDataSchedulerFromC(void)
{
    HDSContext  store = DSCreateStore(0);
    DSQueryDesc query_desc = { 0 };
    HDSQuery    query;
    if (DSCreateQuery(store, &query_desc, &query) != DS_RESULT_OK)
    {
        DSDestroyStore(store);
        return 1;
    }
    uint32_t     count = 0;
    DSSystemDesc desc = {
        .m_Name = dmHashString64("game.count"),
        .m_DependsOn = { dmHashString64("engine.update.begin") },
        .m_Query = query,
        .m_Process = CountScheduledRowsFromC,
        .m_Context = &count
    };
    DSEventDesc                  begin = { .m_Name = dmHashString64("engine.update.begin") };
    DSEventDesc                  end = { .m_Name = dmHashString64("engine.update.end"), .m_DependsOn = { dmHashString64("game.count") } };
    struct JobSystemCreateParams params = { .m_ThreadNamePrefix = "ds-c", .m_ThreadCount = 0 };
    HJobContext                  jobs = JobSystemCreate(&params);
    int                          ok = DSRegisterFunction(store, &desc) == DS_RESULT_OK && count == 0;
    ok = DSRegisterEvent(store, &begin) == DS_RESULT_OK && ok;
    ok = DSRegisterEvent(store, &end) == DS_RESULT_OK && ok;
    ok = DSBeginSchedule(store, jobs) == DS_RESULT_OK && ok;
    ok = DSSchedulerEvent(store, end.m_Name) == DS_RESULT_BUSY && ok;
    ok = DSSchedulerEvent(store, begin.m_Name) == DS_RESULT_OK && ok;
    ok = DSEndSchedule(store) == DS_RESULT_BUSY && ok;
    ok = DSSchedulerUpdate(store) == DS_RESULT_OK && ok;
    ok = DSSchedulerEvent(store, end.m_Name) == DS_RESULT_OK && ok;
    ok = DSEndSchedule(store) == DS_RESULT_OK && ok;
    JobSystemDestroy(jobs);
    ok = DSUnregisterEvent(store, end.m_Name) == DS_RESULT_OK && ok;
    ok = DSUnregisterEvent(store, begin.m_Name) == DS_RESULT_OK && ok;
    ok = DSUnregisterFunction(store, desc.m_Name) == DS_RESULT_OK && ok;
    ok = DSDestroyQuery(query) == DS_RESULT_OK && ok;
    return DSDestroyStore(store) == DS_RESULT_OK && ok ? 0 : 1;
}

int TestDataStoreLifecycleFromC(void)
{
    DSCreateParams params = { .m_PageSize = 512 * 1024, .m_RowChunkSize = 64 * 1024 };
    HDSContext     store = DSCreateStore(&params);
    DSQueryDesc    desc = { 0 };
    HDSQuery       query;
    if (DSCreateQuery(store, &desc, &query) != DS_RESULT_OK)
    {
        DSDestroyStore(store);
        return 1;
    }
    if (DSQueryTryBegin(query) != DS_RESULT_OK)
    {
        DSDestroyQuery(query);
        DSDestroyStore(store);
        return 1;
    }
    DSResult locked = DSDestroyStore(store);
    DSQueryEnd(query);
    DSDestroyQuery(query);
    DSResult destroyed = DSDestroyStore(store);
    return locked == DS_RESULT_LOCKED && destroyed == DS_RESULT_OK ? 0 : 1;
}

int TestDataQuerySetupFromC(void)
{
    HDSContext  store = DSCreateStore(0);
    DSQueryDesc desc = {
        .m_Items = { DSTag("boid"),
                     DSField("position", DS_TYPE_VECTOR3, DS_FIELD_MUTABLE),
                     DSField("velocity", DS_TYPE_VECTOR3, 0),
                     DSTag("school") }
    };
    HDSQuery query;
    DSResult result = DSCreateQuery(store, &desc, &query);
    int      ok = result == DS_RESULT_OK;
    if (ok)
    {
        ok = DSQueryFindField(query, &desc.m_Items[1]) != UINT32_MAX &&
        DSQueryFindField(query, &desc.m_Items[2]) != UINT32_MAX &&
        DSQueryFindField(query, &desc.m_Items[0]) == UINT32_MAX;
        ok = DSDestroyQuery(query) == DS_RESULT_OK && ok;
    }
    return DSDestroyStore(store) == DS_RESULT_OK && ok ? 0 : 1;
}

// Exercise every batch type through C, including aggregate errors and callbacks.
static int TestDataBatchFromC(HDSContext store, DSId id)
{
    DSId        ids[] = { id, id, id };
    double      numbers[3];
    uint8_t     booleans[3];
    const char* strings[3];
    DSVector3   vectors3[3];
    DSVector4   vectors4[3];
    DSMatrix4   matrices[3];
    DSResult (*read_numbers)(HDSContext, uint32_t, const DSId*, uint64_t, double*) = DSFieldGetNumberBatch;
    if (read_numbers(store, 3, ids, 42, numbers) != DS_RESULT_OK ||
        DSFieldGetBooleanBatch(store, 3, ids, 47, booleans) != DS_RESULT_OK ||
        DSFieldGetStringBatch(store, 3, ids, 48, strings) != DS_RESULT_OK ||
        DSFieldGetVector3Batch(store, 3, ids, 43, vectors3) != DS_RESULT_OK ||
        DSFieldGetVector4Batch(store, 3, ids, 44, vectors4) != DS_RESULT_OK ||
        DSFieldGetMatrix4Batch(store, 3, ids, 45, matrices) != DS_RESULT_OK)
        return 0;
    for (uint32_t i = 0; i < 3; ++i)
        if (numbers[i] != 29 || booleans[i] != 0 || strings[i][0] != 'o' || strings[i] != strings[0] ||
            vectors3[i].m_Values[2] != 0.25f || vectors4[i].m_Values[3] != 0.75f || matrices[i].m_Values[14] != 44)
            return 0;

    ids[1] = 0;
    if (read_numbers(store, 3, ids, 42, numbers) != DS_RESULT_NOT_FOUND ||
        DSFieldGetBooleanBatch(store, 3, ids, 47, booleans) != DS_RESULT_NOT_FOUND ||
        DSFieldGetStringBatch(store, 3, ids, 48, strings) != DS_RESULT_NOT_FOUND ||
        DSFieldGetVector3Batch(store, 3, ids, 43, vectors3) != DS_RESULT_NOT_FOUND ||
        DSFieldGetVector4Batch(store, 3, ids, 44, vectors4) != DS_RESULT_NOT_FOUND ||
        DSFieldGetMatrix4Batch(store, 3, ids, 45, matrices) != DS_RESULT_NOT_FOUND)
        return 0;

    return read_numbers(0, 0, 0, 42, 0) == DS_RESULT_OK &&
    DSFieldGetBooleanBatch(0, 0, 0, 47, 0) == DS_RESULT_OK &&
    DSFieldGetStringBatch(0, 0, 0, 48, 0) == DS_RESULT_OK &&
    DSFieldGetVector3Batch(0, 0, 0, 43, 0) == DS_RESULT_OK &&
    DSFieldGetVector4Batch(0, 0, 0, 44, 0) == DS_RESULT_OK &&
    DSFieldGetMatrix4Batch(0, 0, 0, 45, 0) == DS_RESULT_OK;
}

int TestDataFromC(HDSContext store)
{
    DSGroupId      group = 123;
    uint64_t       tag = 7;
    DSQueryItem    fields[] = {
        { .m_Hash = 42, .m_Type = DS_TYPE_NUMBER, .m_Access = DS_FIELD_MUTABLE, .m_Kind = DS_QUERY_ITEM_FIELD },
        { .m_Hash = 43, .m_Type = DS_TYPE_VECTOR3, .m_Kind = DS_QUERY_ITEM_FIELD },
        { .m_Hash = 44, .m_Type = DS_TYPE_VECTOR4, .m_Kind = DS_QUERY_ITEM_FIELD },
        { .m_Hash = 45, .m_Type = DS_TYPE_MATRIX4, .m_Kind = DS_QUERY_ITEM_FIELD },
        { .m_Hash = 47, .m_Type = DS_TYPE_BOOLEAN, .m_Kind = DS_QUERY_ITEM_FIELD },
        { .m_Hash = 48, .m_Type = DS_TYPE_STRING, .m_Kind = DS_QUERY_ITEM_FIELD },
        { .m_Hash = 46, .m_Type = DS_TYPE_STRUCT, .m_Kind = DS_QUERY_ITEM_FIELD }
    };
    DSQueryDesc desc = {
        .m_GroupIds = &group,
        .m_GroupIdCount = 1,
        .m_Items = { fields[0],
                     fields[1],
                     fields[2],
                     fields[3],
                     fields[4],
                     fields[5],
                     fields[6],
                     { .m_Hash = tag, .m_Kind = DS_QUERY_ITEM_TAG } }
    };
    HDSQuery query;
    if (DSCreateQuery(store, &desc, &query) != DS_RESULT_OK)
        return 1;
    uint32_t number_field = DSQueryFindField(query, &fields[0]);
    if (DSQueryTryBegin(query) != DS_RESULT_OK)
    {
        DSDestroyQuery(query);
        return 1;
    }
    DSIterator    it = DSQueryIter(query);
    int           ok = DSIterNext(&it);
    DSRowIterator rows = DSIterRows(&it);
    ok = ok && DSRowIterNext(&rows) && DSRowIterGetGroupId(&rows) == group;
    DSId   id = DSRowIterGetId(&rows);
    double number = 0;
    ok = ok && *DSRowIterGetNumber(&rows, number_field) == 17.0;
    *DSRowIterGetNumberMut(&rows, number_field) = 29.0;
    ok = ok && *DSRowIterGetNumber(&rows, number_field) == 29.0;
    ok = ok && !DSRowIterNext(&rows) && !DSIterNext(&it);
    DSQueryEnd(query);
    ok = ok && DSSetFieldNumber(store, id, 42, 23.0) == DS_RESULT_OK &&
    DSFieldGetNumber(store, id, 42, &number) == DS_RESULT_OK && number == 23.0;
    ok = ok && DSSetFieldNumber(store, id, 42, 29.0) == DS_RESULT_OK;

    DSVector3 vector3 = { .m_Values = { 1.0f, 0.5f, 0.25f } };
    DSVector3 out3;
    ok = ok && DSSetFieldVector3(store, id, 43, &vector3) == DS_RESULT_OK &&
    DSFieldGetVector3(store, id, 43, &out3) == DS_RESULT_OK && out3.m_Values[1] == 0.5f;

    DSVector4 vector4 = { .m_Values = { 1.0f, 0.5f, 0.25f, 0.75f } };
    DSVector4 out4;
    ok = ok && DSSetFieldVector4(store, id, 44, &vector4) == DS_RESULT_OK &&
    DSFieldGetVector4(store, id, 44, &out4) == DS_RESULT_OK && out4.m_Values[3] == 0.75f;

    DSMatrix4 matrix;
    DSMatrix4 out_matrix;
    for (uint32_t i = 0; i < 16; ++i)
        matrix.m_Values[i] = (float)i;
    matrix.m_Values[3 * 4 + 2] = 44.0f;
    ok = ok && DSSetFieldMatrix4(store, id, 45, &matrix) == DS_RESULT_OK &&
    DSFieldGetMatrix4(store, id, 45, &out_matrix) == DS_RESULT_OK && out_matrix.m_Values[14] == 44.0f;

    uint8_t boolean = 0;
    ok = ok && DSSetFieldBoolean(store, id, 47, 1) == DS_RESULT_OK &&
    DSFieldGetBoolean(store, id, 47, &boolean) == DS_RESULT_OK && boolean == 1 &&
    DSSetFieldBoolean(store, id, 47, 0) == DS_RESULT_OK &&
    DSFieldGetBoolean(store, id, 47, &boolean) == DS_RESULT_OK && boolean == 0;

    const char* string;
    ok = ok && DSSetFieldString(store, id, 48, "copied") == DS_RESULT_OK &&
    DSFieldGetString(store, id, 48, &string) == DS_RESULT_OK && string[0] == 'c' &&
    DSSetFieldString(store, id, 48, "override") == DS_RESULT_OK &&
    DSFieldGetString(store, id, 48, &string) == DS_RESULT_OK && string[0] == 'o';

    ok = ok && TestDataBatchFromC(store, id);
    DSDestroyQuery(query);
    return ok ? 0 : 1;
}

int TestInlineDataFromC(HDSContext store)
{
    DSQueryItem requested_field = { .m_Hash = dmHashString64("2.40.10"), .m_Type = DS_TYPE_VECTOR3, .m_Kind = DS_QUERY_ITEM_FIELD };
    DSQueryDesc desc = { .m_Items = { requested_field } };
    HDSQuery    query;
    if (DSCreateQuery(store, &desc, &query) != DS_RESULT_OK)
        return 1;
    if (DSQueryTryBegin(query) != DS_RESULT_OK)
    {
        DSDestroyQuery(query);
        return 1;
    }
    DSIterator    it = DSQueryIter(query);
    int           ok = DSIterNext(&it);
    DSRowIterator rows = DSIterRows(&it);
    ok = ok && DSRowIterNext(&rows);
    uint32_t color_field = DSQueryFindField(query, &requested_field);
    ok = ok && DSRowIterGetVector3(&rows, color_field)->m_Values[2] == 3.0f;
    DSQueryEnd(query);
    DSDestroyQuery(query);
    return ok ? 0 : 1;
}

// Bind once per query, then borrow native values through the public C API.
int TestDataPointersFromC(HDSContext store, int write)
{
    DSQueryItem query_fields[] = {
        { .m_Hash = 5, .m_Type = DS_TYPE_MATRIX4, .m_Kind = DS_QUERY_ITEM_FIELD },
        { .m_Hash = 3, .m_Type = DS_TYPE_VECTOR3, .m_Kind = DS_QUERY_ITEM_FIELD },
        { .m_Hash = 1, .m_Type = DS_TYPE_NUMBER, .m_Kind = DS_QUERY_ITEM_FIELD },
        { .m_Hash = 4, .m_Type = DS_TYPE_VECTOR4, .m_Kind = DS_QUERY_ITEM_FIELD },
        { .m_Hash = 2, .m_Type = DS_TYPE_BOOLEAN, .m_Kind = DS_QUERY_ITEM_FIELD }
    };
    for (uint32_t i = 0; i < 5; ++i)
        query_fields[i].m_Access = write ? DS_FIELD_MUTABLE : 0;
    DSQueryDesc desc = { .m_Items = { query_fields[0], query_fields[1], query_fields[2], query_fields[3], query_fields[4] } };
    HDSQuery    query;
    if (DSCreateQuery(store, &desc, &query) != DS_RESULT_OK)
        return 1;
    int      ok = 1;
    uint32_t fields[5];
    for (uint32_t i = 0; i < 5; ++i)
    {
        fields[i] = DSQueryFindField(query, &query_fields[i]);
        ok = ok && fields[i] != UINT32_MAX;
    }
    DSQueryItem wrong = { .m_Hash = 1, .m_Type = DS_TYPE_VECTOR3, .m_Kind = DS_QUERY_ITEM_FIELD };
    ok = ok && DSQueryFindField(query, &wrong) == UINT32_MAX;
    wrong.m_Hash = 999;
    ok = ok && DSQueryFindField(query, &wrong) == UINT32_MAX;
    wrong.m_Type = DS_TYPE_STRING;
    ok = ok && DSQueryFindField(query, &wrong) == UINT32_MAX;
    if (DSQueryTryBegin(query) != DS_RESULT_OK)
    {
        DSDestroyQuery(query);
        return 1;
    }
    DSIterator   it = DSQueryIter(query);
    uint32_t     count = 0;
    while (ok && DSIterNext(&it))
    {
        const DSVector3* (*read_vector)(const DSRowIterator*, uint32_t) = DSRowIterGetVector3;
        DSRowIterator rows = DSIterRows(&it);
        while (DSRowIterNext(&rows))
        {
            ok = ok && *DSRowIterGetNumber(&rows, fields[2]) == 17.0 &&
            *DSRowIterGetBoolean(&rows, fields[4]) == 0 &&
            read_vector(&rows, fields[1])->m_Values[2] == 3.0f &&
            DSRowIterGetVector4(&rows, fields[3])->m_Values[3] == 4.0f &&
            DSRowIterGetMatrix4(&rows, fields[0])->m_Values[15] == 15.0f;
            if (write)
            {
                *DSRowIterGetNumberMut(&rows, fields[2]) = 29.0;
                *DSRowIterGetBooleanMut(&rows, fields[4]) = 1;
                DSRowIterGetVector3Mut(&rows, fields[1])->m_Values[2] = 30.0f;
                DSRowIterGetVector4Mut(&rows, fields[3])->m_Values[3] = 40.0f;
                DSRowIterGetMatrix4Mut(&rows, fields[0])->m_Values[15] = 150.0f;
                ok = ok && *DSRowIterGetNumber(&rows, fields[2]) == 29.0 &&
                *DSRowIterGetBoolean(&rows, fields[4]) == 1 &&
                read_vector(&rows, fields[1])->m_Values[2] == 30.0f &&
                DSRowIterGetVector4(&rows, fields[3])->m_Values[3] == 40.0f &&
                DSRowIterGetMatrix4(&rows, fields[0])->m_Values[15] == 150.0f;
            }
            ++count;
        }
    }
    ok = ok && DSQueryFindField(query, &query_fields[0]) == fields[0] && count == 3;
    DSQueryEnd(query);
    if (DSQueryTryBegin(query) == DS_RESULT_OK)
    {
        DSIterator range = DSQueryIterRange(query, 0, DSQueryGetRowCount(query));
        ok = ok && DSQueryGetRowCount(query) == 3 && DSIterNext(&range);
        DSQueryEnd(query);
    }
    else
        ok = 0;
    DSDestroyQuery(query);
    return ok ? 0 : 1;
}

// A complete native-row lifecycle using only installed SDK headers and C types.
int TestDataNativeRowsFromC(void)
{
    struct State
    {
        double  m_Health;
        uint8_t m_Enabled;
    };
    struct Enemy
    {
        struct State m_State;
        DSVector3    m_Position;
    };
    DSFieldDesc state_fields[] = {
        { .m_Field = 1, .m_Type = DS_TYPE_NUMBER, .m_Offset = offsetof(struct State, m_Health), .m_Name = "health" },
        { .m_Field = 2, .m_Type = DS_TYPE_BOOLEAN, .m_Offset = offsetof(struct State, m_Enabled), .m_Name = "enabled" }
    };
    DSStructDesc state = { .m_Fields = state_fields, .m_FieldCount = 2, .m_Size = sizeof(struct State) };
    DSFieldDesc  fields[] = {
        { .m_Field = 3, .m_Type = DS_TYPE_STRUCT, .m_Offset = offsetof(struct Enemy, m_State), .m_Struct = &state, .m_Name = "state" },
        { .m_Field = 4, .m_Type = DS_TYPE_VECTOR3, .m_Offset = offsetof(struct Enemy, m_Position) }
    };
    uint64_t      tag = 7;
    DSTableDesc   table = { .m_Type = 10, .m_Tags = &tag, .m_TagCount = 1, .m_Fields = fields, .m_FieldCount = 2, .m_RowStride = sizeof(struct Enemy) };
    struct Enemy  input[] = {
        { .m_State = { .m_Health = 50, .m_Enabled = 1 }, .m_Position = { .m_Values = { 1, 2, 3 } } },
        { .m_State = { .m_Health = 75 }, .m_Position = { .m_Values = { 4, 5, 6 } } }
    };
    DSGroupId       group = 100;
    DSId            ids[2], added[2];
    DSQueryItem     health = { .m_Hash = dmHashString64("state.health"), .m_Type = DS_TYPE_NUMBER, .m_Access = DS_FIELD_MUTABLE, .m_Kind = DS_QUERY_ITEM_FIELD };
    DSQueryDesc     query_desc = { .m_GroupIds = &group, .m_GroupIdCount = 1, .m_Flags = DS_ROW_MUTABLE, .m_Items = { health, { .m_Hash = tag, .m_Kind = DS_QUERY_ITEM_TAG } } };
    HDSContext      store = DSCreateStore(0);
    HDSQuery        query = NULL;
    int             active = 0;
    int             error = 0;
    double          number = 0;
    DSVector3       position;
#define CHECK_NATIVE(condition) \
    do \
    { \
        if (!(condition)) \
        { \
            error = __LINE__; \
            goto cleanup; \
        } \
    } while (0)
    CHECK_NATIVE(DSRegisterTable(store, &table) == DS_RESULT_OK);
    CHECK_NATIVE(DSCreateQuery(store, &query_desc, &query) == DS_RESULT_OK);
    CHECK_NATIVE(DSCreateRows(store, 10, group, 2, input, ids) == DS_RESULT_OK);
    CHECK_NATIVE(DSFieldGetNumber(store, ids[1], health.m_Hash, &number) == DS_RESULT_OK && number == 75);
    CHECK_NATIVE(DSFieldGetVector3(store, ids[1], 4, &position) == DS_RESULT_OK && position.m_Values[2] == 6);
    CHECK_NATIVE(DSGetComponentId(store, ids[1]) == 0);

    // Prepare complete rows on the caller side, including nested fields.
    struct Enemy enemies[] = { input[0], input[0] };
    enemies[0].m_State.m_Health = 80;
    enemies[1].m_State.m_Health = 90;
    CHECK_NATIVE(DSCreateRows(store, 10, 200, 2, enemies, added) == DS_RESULT_OK);
    CHECK_NATIVE(DSSetFieldNumber(store, added[1], health.m_Hash, 5) == DS_RESULT_OK);
    CHECK_NATIVE(DSResetRow(store, added[1]) == DS_RESULT_OK);
    CHECK_NATIVE(DSFieldGetNumber(store, added[1], health.m_Hash, &number) == DS_RESULT_OK && number == 90);
    CHECK_NATIVE(DSFieldGetVector3(store, added[1], 4, &position) == DS_RESULT_OK && position.m_Values[2] == 3);

    CHECK_NATIVE(DSQueryTryBegin(query) == DS_RESULT_OK);
    active = 1;
    CHECK_NATIVE(DSQueryGetRowCount(query) == 2);
    DSIterator it = DSQueryIterRange(query, 0, 2);
    CHECK_NATIVE(DSIterNext(&it));
    DSRowIterator rows = DSIterRows(&it);
    CHECK_NATIVE(DSRowIterNext(&rows) && DSRowIterGetGroupId(&rows) == 100);
    const struct Enemy* read_row = (const struct Enemy*)DSRowIterGetData(&rows);
    CHECK_NATIVE(read_row->m_State.m_Health == 50);
    struct Enemy* write_row = (struct Enemy*)DSRowIterGetDataMut(&rows);
    write_row->m_State.m_Health = 45;
    CHECK_NATIVE(DSCreateRows(store, 10, 200, 2, enemies, added) == DS_RESULT_LOCKED);
    DSQueryEnd(query);
    active = 0;

    CHECK_NATIVE(DSFieldGetNumber(store, ids[0], health.m_Hash, &number) == DS_RESULT_OK && number == 45);
    CHECK_NATIVE(DSResetRow(store, ids[0]) == DS_RESULT_OK);

    // Failed input leaves IDs and query membership intact.
    DSId unchanged[] = { 123, 456 };
    enemies[1].m_State.m_Enabled = 2;
    CHECK_NATIVE(DSCreateRows(store, 10, 200, 2, enemies, unchanged) == DS_RESULT_INVALID_ARGUMENT);
    enemies[1].m_State.m_Enabled = 1;
    CHECK_NATIVE(DSCreateRows(store, 10, 200, 2, NULL, unchanged) == DS_RESULT_INVALID_ARGUMENT);
    CHECK_NATIVE(unchanged[0] == 123 && unchanged[1] == 456);
    CHECK_NATIVE(DSCreateRows(store, 10, 200, 0, NULL, NULL) == DS_RESULT_OK);
    CHECK_NATIVE(DSCreateRows(NULL, 999, 200, 0, NULL, NULL) == DS_RESULT_OK);

    CHECK_NATIVE(DSRemoveRow(store, ids[0]) == DS_RESULT_OK);
    CHECK_NATIVE(DSFieldGetNumber(store, ids[0], health.m_Hash, &number) == DS_RESULT_NOT_FOUND);
    CHECK_NATIVE(DSFieldGetNumber(store, added[1], health.m_Hash, &number) == DS_RESULT_OK && number == 90);
    // Caller-prepared defaults create independent rows, including group zero.
    enemies[1] = enemies[0];
    CHECK_NATIVE(DSCreateRows(store, 10, 0, 2, enemies, added) == DS_RESULT_OK);
    CHECK_NATIVE(added[0] != ids[0]);
    CHECK_NATIVE(DSFieldGetNumber(store, added[1], health.m_Hash, &number) == DS_RESULT_OK && number == 80);
    enemies[0].m_State.m_Health = -1;
    CHECK_NATIVE(DSSetFieldNumber(store, added[0], health.m_Hash, 5) == DS_RESULT_OK);
    CHECK_NATIVE(DSFieldGetNumber(store, added[1], health.m_Hash, &number) == DS_RESULT_OK && number == 80);
    CHECK_NATIVE(DSResetRow(store, added[0]) == DS_RESULT_OK);
    CHECK_NATIVE(DSFieldGetNumber(store, added[0], health.m_Hash, &number) == DS_RESULT_OK && number == 80);
    CHECK_NATIVE(DSQueryTryBegin(query) == DS_RESULT_OK);
    active = 1;
    CHECK_NATIVE(DSQueryGetRowCount(query) == 1);
    DSQueryEnd(query);
    active = 0;
    CHECK_NATIVE(DSUnregisterTable(store, 10) == DS_RESULT_OK);
    CHECK_NATIVE(DSFieldGetNumber(store, ids[1], health.m_Hash, &number) == DS_RESULT_NOT_FOUND);

    // A NULL native string is invalid; valid references are copied on insertion.
    DSFieldDesc string = { .m_Field = 1, .m_Type = DS_TYPE_STRING };
    table.m_Fields = &string;
    table.m_FieldCount = 1;
    table.m_RowStride = 8;
    CHECK_NATIVE(DSRegisterTable(store, &table) == DS_RESULT_OK);
    DSReference missing_strings[2] = { 0 };
    CHECK_NATIVE(DSCreateRows(store, 10, 200, 2, missing_strings, unchanged) == DS_RESULT_INVALID_ARGUMENT);
cleanup:
    if (active)
        DSQueryEnd(query);
    if (query)
        DSDestroyQuery(query);
    DSDestroyStore(store);
#undef CHECK_NATIVE
    return error;
}

// Field arrays use the public C API, including inline struct arrays and reset.
int TestDataSoARowsFromC(void)
{
    struct State
    {
        double      m_Health;
        uint8_t     m_Enabled;
        DSVector3   m_Direction;
    };
    struct Enemy
    {
        uint8_t      m_Active;
        DSVector3    m_Position;
        struct State m_State;
        DSVector4    m_Color;
        DSMatrix4    m_Transform;
    };
    DSFieldDesc state_fields[] = {
        { .m_Field = 1, .m_Type = DS_TYPE_NUMBER, .m_Offset = offsetof(struct State, m_Health), .m_Name = "health" },
        { .m_Field = 2, .m_Type = DS_TYPE_BOOLEAN, .m_Offset = offsetof(struct State, m_Enabled), .m_Name = "enabled" },
        { .m_Field = 3, .m_Type = DS_TYPE_VECTOR3, .m_Offset = offsetof(struct State, m_Direction), .m_Name = "direction" }
    };
    DSStructDesc state = { .m_Fields = state_fields, .m_FieldCount = 3, .m_Size = sizeof(struct State) };
    DSFieldDesc  layout[] = {
        { .m_Field = 10, .m_Type = DS_TYPE_BOOLEAN, .m_Offset = offsetof(struct Enemy, m_Active) },
        { .m_Field = 20, .m_Type = DS_TYPE_VECTOR3, .m_Offset = offsetof(struct Enemy, m_Position) },
        { .m_Field = 30, .m_Type = DS_TYPE_STRUCT, .m_Offset = offsetof(struct Enemy, m_State), .m_Struct = &state, .m_Name = "state" },
        { .m_Field = 40, .m_Type = DS_TYPE_VECTOR4, .m_Offset = offsetof(struct Enemy, m_Color) },
        { .m_Field = 50, .m_Type = DS_TYPE_MATRIX4, .m_Offset = offsetof(struct Enemy, m_Transform) }
    };
    DSTableDesc   table = { .m_Type = 10, .m_Fields = layout, .m_FieldCount = 5, .m_RowStride = sizeof(struct Enemy) };
    uint8_t       active[] = { 1, 0 };
    DSVector3     positions[] = { { .m_Values = { 1, 2, 3 } }, { .m_Values = { 4, 5, 6 } } };
    struct State  states[] = {
        { .m_Health = 50, .m_Enabled = 1, .m_Direction = { .m_Values = { 1, 0, 0 } } },
        { .m_Health = 75, .m_Enabled = 0, .m_Direction = { .m_Values = { 0, 1, 0 } } }
    };
    DSVector4 colors[] = { { .m_Values = { 7, 8, 9, 10 } }, { .m_Values = { 11, 12, 13, 14 } } };
    DSMatrix4 transforms[2] = { 0 };
    for (uint32_t r = 0; r < 2; ++r)
        for (uint32_t i = 0; i < 16; ++i)
            transforms[r].m_Values[i] = (float)(r * 16 + i);
    // Input order is independent of layout and byte order.
    DSFieldArray fields[] = {
        { .m_Field = 50, .m_Values = transforms },
        { .m_Field = 30, .m_Values = states },
        { .m_Field = 10, .m_Values = active },
        { .m_Field = 40, .m_Values = colors },
        { .m_Field = 20, .m_Values = positions }
    };
    DSId           ids[2] = { 123, 456 };
    DSGroupId      group = 77;
    DSQueryItem    health = { .m_Hash = dmHashString64("state.health"), .m_Type = DS_TYPE_NUMBER, .m_Kind = DS_QUERY_ITEM_FIELD };
    DSQueryDesc    query_desc = { .m_GroupIds = &group, .m_GroupIdCount = 1, .m_Items = { health } };
    HDSContext     store = DSCreateStore(0);
    HDSQuery       query = NULL;
    int            error = 0;
    int            reserved = 0;
#define CHECK_SOA(condition) \
    do \
    { \
        if (!(condition)) \
        { \
            error = __LINE__; \
            goto cleanup; \
        } \
    } while (0)
    CHECK_SOA(DSRegisterTable(store, &table) == DS_RESULT_OK);
    CHECK_SOA(DSCreateQuery(store, &query_desc, &query) == DS_RESULT_OK);
    CHECK_SOA(DSCreateRowsSoA(store, 999, group, 2, 5, fields, ids) == DS_RESULT_NOT_FOUND);
    CHECK_SOA(DSCreateRowsSoA(store, 10, group, 0, 0, NULL, NULL) == DS_RESULT_OK);
    CHECK_SOA(DSCreateRowsSoA(NULL, 999, group, 0, 5, NULL, NULL) == DS_RESULT_OK);
    CHECK_SOA(DSCreateRowsSoA(store, 10, group, UINT32_MAX, 5, fields, ids) == DS_RESULT_INVALID_ARGUMENT);
    CHECK_SOA(DSCreateRowsSoA(store, 10, group, 2, 4, fields, ids) == DS_RESULT_INVALID_ARGUMENT);
    CHECK_SOA(DSCreateRowsSoA(store, 10, group, 2, 5, NULL, ids) == DS_RESULT_INVALID_ARGUMENT);
    fields[4].m_Field = 40;
    CHECK_SOA(DSCreateRowsSoA(store, 10, group, 2, 5, fields, ids) == DS_RESULT_INVALID_ARGUMENT);
    fields[4].m_Field = 999;
    CHECK_SOA(DSCreateRowsSoA(store, 10, group, 2, 5, fields, ids) == DS_RESULT_INVALID_ARGUMENT);
    fields[4].m_Field = health.m_Hash;
    CHECK_SOA(DSCreateRowsSoA(store, 10, group, 2, 5, fields, ids) == DS_RESULT_INVALID_ARGUMENT);
    fields[4].m_Field = 20;
    fields[4].m_Values = NULL;
    CHECK_SOA(DSCreateRowsSoA(store, 10, group, 2, 5, fields, ids) == DS_RESULT_INVALID_ARGUMENT);
    fields[4].m_Values = positions;
    active[1] = 2;
    CHECK_SOA(DSCreateRowsSoA(store, 10, group, 2, 5, fields, ids) == DS_RESULT_INVALID_ARGUMENT);
    active[1] = 0;
    states[1].m_Enabled = 2;
    CHECK_SOA(DSCreateRowsSoA(store, 10, group, 2, 5, fields, ids) == DS_RESULT_INVALID_ARGUMENT);
    states[1].m_Enabled = 0;
    CHECK_SOA(ids[0] == 123 && ids[1] == 456);

    CHECK_SOA(DSCreateRowsSoA(store, 10, group, 2, 5, fields, ids) == DS_RESULT_OK);
    for (uint32_t i = 0; i < 2; ++i)
    {
        double      number;
        uint8_t     boolean;
        DSVector3   position;
        DSVector4   color;
        DSMatrix4   transform;
        CHECK_SOA(DSFieldGetNumber(store, ids[i], health.m_Hash, &number) == DS_RESULT_OK && number == states[i].m_Health);
        CHECK_SOA(DSFieldGetBoolean(store, ids[i], 10, &boolean) == DS_RESULT_OK && boolean == active[i]);
        CHECK_SOA(DSFieldGetBoolean(store, ids[i], dmHashString64("state.enabled"), &boolean) == DS_RESULT_OK && boolean == states[i].m_Enabled);
        CHECK_SOA(DSFieldGetVector3(store, ids[i], 20, &position) == DS_RESULT_OK && position.m_Values[2] == positions[i].m_Values[2]);
        CHECK_SOA(DSFieldGetVector4(store, ids[i], 40, &color) == DS_RESULT_OK && color.m_Values[3] == colors[i].m_Values[3]);
        CHECK_SOA(DSFieldGetMatrix4(store, ids[i], 50, &transform) == DS_RESULT_OK && transform.m_Values[15] == transforms[i].m_Values[15]);
        CHECK_SOA(DSFieldGetVector3(store, ids[i], dmHashString64("state.direction"), &position) == DS_RESULT_OK && position.m_Values[i] == 1);
        CHECK_SOA(DSGetComponentId(store, ids[i]) == 0);
    }
    states[1].m_Health = -1;
    positions[1].m_Values[2] = -1;
    CHECK_SOA(DSSetFieldNumber(store, ids[1], health.m_Hash, 5) == DS_RESULT_OK);
    CHECK_SOA(DSSetFieldVector3(store, ids[1], 20, &positions[1]) == DS_RESULT_OK);
    CHECK_SOA(DSResetRow(store, ids[1]) == DS_RESULT_OK);
    double      number;
    DSVector3   position;
    CHECK_SOA(DSFieldGetNumber(store, ids[1], health.m_Hash, &number) == DS_RESULT_OK && number == 75);
    CHECK_SOA(DSFieldGetVector3(store, ids[1], 20, &position) == DS_RESULT_OK && position.m_Values[2] == 6);

    CHECK_SOA(DSQueryTryBegin(query) == DS_RESULT_OK);
    reserved = 1;
    CHECK_SOA(DSQueryGetRowCount(query) == 2);
    DSIterator it = DSQueryIterRange(query, 0, 2);
    CHECK_SOA(DSIterNext(&it));
    DSRowIterator rows = DSIterRows(&it);
    CHECK_SOA(DSRowIterNext(&rows) && DSRowIterGetGroupId(&rows) == group);
    CHECK_SOA(DSCreateRowsSoA(store, 10, group, 2, 5, fields, ids) == DS_RESULT_LOCKED);
    DSQueryEnd(query);
    reserved = 0;
    DSId removed = ids[0];
    CHECK_SOA(DSRemoveRow(store, removed) == DS_RESULT_OK);
    CHECK_SOA(DSCreateRowsSoA(store, 10, 0, 2, 5, fields, ids) == DS_RESULT_OK);
    CHECK_SOA(ids[0] != removed);
    CHECK_SOA(DSFieldGetNumber(store, removed, health.m_Hash, &number) == DS_RESULT_NOT_FOUND);
    CHECK_SOA(DSQueryTryBegin(query) == DS_RESULT_OK);
    reserved = 1;
    CHECK_SOA(DSQueryGetRowCount(query) == 1);
    DSQueryEnd(query);
    reserved = 0;

    DSTableDesc empty = { .m_Type = 11 };
    CHECK_SOA(DSRegisterTable(store, &empty) == DS_RESULT_OK);
    CHECK_SOA(DSCreateRowsSoA(store, 11, group, 2, 0, NULL, ids) == DS_RESULT_OK);
    CHECK_SOA(DSRemoveRow(store, ids[1]) == DS_RESULT_OK);
    DSStructDesc zero_struct = { 0 };
    DSFieldDesc  zero_field = { .m_Field = 1, .m_Type = DS_TYPE_STRUCT, .m_Struct = &zero_struct, .m_Name = "empty" };
    DSTableDesc  zero_table = { .m_Type = 13, .m_Fields = &zero_field, .m_FieldCount = 1 };
    DSFieldArray zero_values = { .m_Field = 1 };
    CHECK_SOA(DSRegisterTable(store, &zero_table) == DS_RESULT_OK);
    CHECK_SOA(DSCreateRowsSoA(store, 13, 0, 2, 1, &zero_values, ids) == DS_RESULT_OK);
    CHECK_SOA(DSRemoveRow(store, ids[0]) == DS_RESULT_OK);
    DSFieldDesc string = { .m_Field = 1, .m_Type = DS_TYPE_STRING };
    DSTableDesc references = { .m_Type = 12, .m_Fields = &string, .m_FieldCount = 1, .m_RowStride = 8 };
    CHECK_SOA(DSRegisterTable(store, &references) == DS_RESULT_OK);
    CHECK_SOA(DSCreateRowsSoA(store, 12, group, 2, 1, fields, ids) == DS_RESULT_INVALID_ARGUMENT);
cleanup:
    if (reserved)
        DSQueryEnd(query);
    if (query)
        DSDestroyQuery(query);
    DSDestroyStore(store);
#undef CHECK_SOA
    return error;
}

// References are constructed through the public API from stack-owned input.
int CreateReferenceRowsFromC(HDSContext store, int soa, DSId ids[2])
{
    char          name[] = "Guard";
    char          faction[] = "guards";
    char          loot_name[] = "coin";
    DSFieldDesc   members[] = {
        { .m_Field = 1, .m_Type = DS_TYPE_STRING, .m_Offset = offsetof(struct ReferenceAttributes, m_Faction), .m_Name = "faction" },
        { .m_Field = 2, .m_Type = DS_TYPE_NUMBER, .m_Offset = offsetof(struct ReferenceAttributes, m_AlertRadius), .m_Name = "alert_radius" }
    };
    DSStructDesc               layout = { .m_Fields = members, .m_FieldCount = 2, .m_Size = sizeof(struct ReferenceAttributes) };
    struct ReferenceAttributes attributes[] = {
        { .m_Faction = { .m_String = faction }, .m_AlertRadius = 15.0 },
        { .m_Faction = { .m_String = "bandits" }, .m_AlertRadius = 25.0 }
    };
    DSStructInput objects[] = {
        { .m_Layout = &layout, .m_Values = &attributes[0] },
        { .m_Layout = &layout, .m_Values = &attributes[1] }
    };
    DSReference strings[] = { { .m_String = loot_name }, { .m_String = "health_pickup" } };
    DSReference structures[] = { { .m_Struct = &objects[0] }, { .m_Struct = &objects[1] } };
    DSListInput lists[] = {
        { .m_Type = DS_TYPE_STRING, .m_Count = 2, .m_Values = strings },
        { .m_Type = DS_TYPE_STRUCT, .m_Count = 2, .m_Values = structures }
    };
    DSReference names[] = { { .m_String = name }, { .m_String = "Bandit" } };
    DSReference loot[] = { { .m_List = &lists[0] }, { .m_List = &lists[1] } };
    DSFieldDesc fields[] = {
        { .m_Field = 10, .m_Type = DS_TYPE_STRING, .m_Offset = offsetof(struct ReferenceEnemy, m_Name) },
        { .m_Field = 11, .m_Type = DS_TYPE_STRUCT, .m_Offset = offsetof(struct ReferenceEnemy, m_Attributes) },
        { .m_Field = 12, .m_Type = DS_TYPE_LIST, .m_Offset = offsetof(struct ReferenceEnemy, m_Loot) },
        { .m_Field = 13, .m_Type = DS_TYPE_STRUCT, .m_Offset = offsetof(struct ReferenceEnemy, m_State), .m_Struct = &layout, .m_Name = "state" }
    };
    DSTableDesc table = { .m_Type = 93, .m_Fields = fields, .m_FieldCount = 4, .m_RowStride = sizeof(struct ReferenceEnemy) };
    if (DSRegisterTable(store, &table) != DS_RESULT_OK)
        return 1;

    DSResult result;
    if (soa)
    {
        // Supply columns out of metadata order, including complete inline structs.
        DSFieldArray columns[] = {
            { .m_Field = 12, .m_Values = loot },
            { .m_Field = 13, .m_Values = attributes },
            { .m_Field = 10, .m_Values = names },
            { .m_Field = 11, .m_Values = structures }
        };
        result = DSCreateRowsSoA(store, 93, 42, 2, 4, columns, ids);
    }
    else
    {
        struct ReferenceEnemy enemies[] = {
            { .m_Name = names[0], .m_Attributes = structures[0], .m_Loot = loot[0], .m_State = attributes[0] },
            { .m_Name = names[1], .m_Attributes = structures[1], .m_Loot = loot[1], .m_State = attributes[1] }
        };
        result = DSCreateRows(store, 93, 42, 2, enemies, ids);
    }
    if (result != DS_RESULT_OK)
        return 2;

    // Neither payloads nor descriptors may be retained from caller-owned storage.
    name[0] = faction[0] = loot_name[0] = 'X';
    attributes[0].m_AlertRadius = -1;
    const char* stored_name;
    if (DSFieldGetString(store, ids[0], 10, &stored_name) != DS_RESULT_OK || strcmp(stored_name, "Guard"))
        return 3;
    if (DSFieldGetString(store, ids[0], dmHashString64("state.faction"), &stored_name) != DS_RESULT_OK || strcmp(stored_name, "guards"))
        return 4;
    if (DSSetFieldString(store, ids[0], 10, "Captain") != DS_RESULT_OK || DSResetRow(store, ids[0]) != DS_RESULT_OK)
        return 5;
    return DSFieldGetString(store, ids[0], 10, &stored_name) == DS_RESULT_OK && strcmp(stored_name, "Guard") == 0 ? 0 : 6;
}

int CreateMixedListRowsFromC(HDSContext store, int soa, DSId ids[2])
{
    double          number = 100.0;
    char            string[] = "coin";
    uint8_t         boolean = 1;
    DSVector3       vector3 = { .m_Values = { 1, 2, 3 } };
    DSVector4       vector4 = { .m_Values = { 4, 5, 6, 7 } };
    DSMatrix4       matrix = { .m_Values = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 } };
    DSFieldDesc     member = { .m_Field = 42, .m_Type = DS_TYPE_NUMBER };
    DSStructDesc    layout = { .m_Fields = &member, .m_FieldCount = 1, .m_Size = sizeof(number) };
    DSStructInput   object = { .m_Layout = &layout, .m_Values = &number };
    double          numbers[] = { 1, 2 };
    DSListInput     nested = { .m_Type = DS_TYPE_NUMBER, .m_Count = 2, .m_Values = numbers };
    DSValueType     types[] = {
        DS_TYPE_NUMBER, DS_TYPE_STRING, DS_TYPE_BOOLEAN, DS_TYPE_STRUCT, DS_TYPE_LIST, DS_TYPE_NULL, DS_TYPE_VECTOR3, DS_TYPE_VECTOR4, DS_TYPE_MATRIX4
    };
    const void* values[] = { &number, string, &boolean, &object, &nested, NULL, &vector3, &vector4, &matrix };
    // m_Type is ignored when per-element kinds are supplied.
    DSListInput mixed = { .m_Type = (DSValueType)UINT32_MAX, .m_Count = 9, .m_Values = values, .m_Types = types };
    DSReference child = { .m_List = &mixed };
    DSListInput parent = { .m_Type = DS_TYPE_LIST, .m_Count = 1, .m_Values = &child };
    DSReference rows[] = { { .m_List = &mixed }, { .m_List = &parent } };
    DSFieldDesc field = { .m_Field = 1, .m_Type = DS_TYPE_LIST };
    DSTableDesc table = { .m_Type = 94, .m_Fields = &field, .m_FieldCount = 1, .m_RowStride = sizeof(DSReference) };
    if (DSRegisterTable(store, &table) != DS_RESULT_OK)
        return 1;
    DSResult result;
    if (soa)
    {
        DSFieldArray column = { .m_Field = 1, .m_Values = rows };
        result = DSCreateRowsSoA(store, 94, 42, 2, 1, &column, ids);
    }
    else
        result = DSCreateRows(store, 94, 42, 2, rows, ids);

    number = -1;
    string[0] = 'X';
    boolean = 0;
    numbers[0] = -1;
    vector3.m_Values[2] = -1;
    vector4.m_Values[3] = -1;
    matrix.m_Values[15] = -1;
    types[0] = DS_TYPE_NULL;
    values[1] = NULL;
    member.m_Field = 0;
    if (result != DS_RESULT_OK)
        return 2;
    DSValueView   list, item;
    uint32_t      count;
    if (DSFieldGetView(store, ids[0], 1, &list) != DS_RESULT_OK ||
        DSViewGetCount(&list, &count) != DS_RESULT_OK || count != 9)
        return 3;
    if (DSViewGetElement(&list, 6, &item) != DS_RESULT_OK ||
        DSViewGetVector3(&item, &vector3) != DS_RESULT_OK || vector3.m_Values[2] != 3)
        return 4;
    if (DSViewGetElement(&list, 7, &item) != DS_RESULT_OK ||
        DSViewGetVector4(&item, &vector4) != DS_RESULT_OK || vector4.m_Values[3] != 7)
        return 5;
    if (DSViewGetElement(&list, 8, &item) != DS_RESULT_OK ||
        DSViewGetMatrix4(&item, &matrix) != DS_RESULT_OK || matrix.m_Values[15] != 1)
        return 6;
    if (DSViewGetElement(&list, 2, &item) != DS_RESULT_OK ||
        DSViewGetBoolean(&item, &boolean) != DS_RESULT_OK || boolean != 1)
        return 7;
    return 0;
}
