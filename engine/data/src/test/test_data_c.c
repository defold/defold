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
#include <dmsdk/data/data.h>
#include <dmsdk/dlib/hash.h>

struct ReferenceAttributes
{
    DataReference m_Faction;
    double        m_AlertRadius;
};

struct ReferenceEnemy
{
    DataReference              m_Name;
    DataReference              m_Attributes;
    DataReference              m_Loot;
    struct ReferenceAttributes m_State;
};

int TestDataStoreLifecycleFromC(void)
{
    HDataStore store = DataCreateStore();
    DataStoreLock(store);
    DataResult locked = DataDestroyStore(store);
    DataStoreUnlock(store);
    DataResult destroyed = DataDestroyStore(store);
    return locked == DATA_RESULT_LOCKED && destroyed == DATA_RESULT_OK ? 0 : 1;
}

// Exercise every batch type through C, including aggregate errors and callbacks.
static int TestDataBatchFromC(HDataStore store, DataId id)
{
    DataId      ids[] = { id, id, id };
    double      numbers[3];
    uint8_t     booleans[3];
    const char* strings[3];
    DataVector3 vectors3[3];
    DataVector4 vectors4[3];
    DataMatrix4 matrices[3];
    DataResult (*read_numbers)(HDataStore, uint32_t, const DataId*, uint64_t, double*) = DataFieldGetNumberBatch;
    if (read_numbers(store, 3, ids, 42, numbers) != DATA_RESULT_OK ||
        DataFieldGetBooleanBatch(store, 3, ids, 47, booleans) != DATA_RESULT_OK ||
        DataFieldGetStringBatch(store, 3, ids, 48, strings) != DATA_RESULT_OK ||
        DataFieldGetVector3Batch(store, 3, ids, 43, vectors3) != DATA_RESULT_OK ||
        DataFieldGetVector4Batch(store, 3, ids, 44, vectors4) != DATA_RESULT_OK ||
        DataFieldGetMatrix4Batch(store, 3, ids, 45, matrices) != DATA_RESULT_OK)
        return 0;
    for (uint32_t i = 0; i < 3; ++i)
        if (numbers[i] != 29 || booleans[i] != 0 || strings[i][0] != 'o' || strings[i] != strings[0] ||
            vectors3[i].m_Values[2] != 0.25f || vectors4[i].m_Values[3] != 0.75f || matrices[i].m_Values[14] != 44)
            return 0;

    ids[1] = 0;
    if (read_numbers(store, 3, ids, 42, numbers) != DATA_RESULT_NOT_FOUND ||
        DataFieldGetBooleanBatch(store, 3, ids, 47, booleans) != DATA_RESULT_NOT_FOUND ||
        DataFieldGetStringBatch(store, 3, ids, 48, strings) != DATA_RESULT_NOT_FOUND ||
        DataFieldGetVector3Batch(store, 3, ids, 43, vectors3) != DATA_RESULT_NOT_FOUND ||
        DataFieldGetVector4Batch(store, 3, ids, 44, vectors4) != DATA_RESULT_NOT_FOUND ||
        DataFieldGetMatrix4Batch(store, 3, ids, 45, matrices) != DATA_RESULT_NOT_FOUND)
        return 0;

    return read_numbers(0, 0, 0, 42, 0) == DATA_RESULT_OK &&
    DataFieldGetBooleanBatch(0, 0, 0, 47, 0) == DATA_RESULT_OK &&
    DataFieldGetStringBatch(0, 0, 0, 48, 0) == DATA_RESULT_OK &&
    DataFieldGetVector3Batch(0, 0, 0, 43, 0) == DATA_RESULT_OK &&
    DataFieldGetVector4Batch(0, 0, 0, 44, 0) == DATA_RESULT_OK &&
    DataFieldGetMatrix4Batch(0, 0, 0, 45, 0) == DATA_RESULT_OK;
}

int TestDataFromC(HDataStore store)
{
    DataGroupId    group = 123;
    uint64_t       tag = 7;
    DataQueryField fields[] = {
        { .m_Field = 42, .m_Type = DATA_TYPE_NUMBER },
        { .m_Field = 43, .m_Type = DATA_TYPE_VECTOR3 },
        { .m_Field = 44, .m_Type = DATA_TYPE_VECTOR4 },
        { .m_Field = 45, .m_Type = DATA_TYPE_MATRIX4 },
        { .m_Field = 47, .m_Type = DATA_TYPE_BOOLEAN },
        { .m_Field = 48, .m_Type = DATA_TYPE_STRING },
        { .m_Field = 46, .m_Type = DATA_TYPE_STRUCT }
    };
    DataQueryDesc desc = { .m_GroupIds = &group, .m_GroupIdCount = 1, .m_AllTags = &tag, .m_AllTagCount = 1, .m_Fields = fields, .m_FieldCount = 7 };
    HDataQuery    query;
    if (DataCreateQuery(store, &desc, &query) != DATA_RESULT_OK)
        return 1;
    uint32_t number_field = DataQueryFindField(query, &fields[0]);
    DataStoreLock(store);
    DataIterator    it = DataQueryIter(query);
    int             ok = DataIterNext(&it) == DATA_RESULT_OK;
    DataRowIterator rows = DataIterRows(&it);
    ok = ok && DataRowIterNext(&rows) == DATA_RESULT_OK && DataRowIterGetGroupId(&rows) == group;
    DataId id = DataRowIterGetId(&rows);
    double number = 0;
    ok = ok && *DataRowIterGetNumber(&rows, number_field) == 17.0;
    ok = ok && DataSetFieldNumber(store, id, 42, 23.0) == DATA_RESULT_OK &&
    DataFieldGetNumber(store, id, 42, &number) == DATA_RESULT_OK && number == 23.0;
    *DataRowIterGetNumberMut(&rows, number_field) = 29.0;
    ok = ok && DataFieldGetNumber(store, id, 42, &number) == DATA_RESULT_OK && number == 29.0;

    DataVector3 vector3 = { .m_Values = { 1.0f, 0.5f, 0.25f } };
    DataVector3 out3;
    ok = ok && DataSetFieldVector3(store, id, 43, &vector3) == DATA_RESULT_OK &&
    DataFieldGetVector3(store, id, 43, &out3) == DATA_RESULT_OK && out3.m_Values[1] == 0.5f;

    DataVector4 vector4 = { .m_Values = { 1.0f, 0.5f, 0.25f, 0.75f } };
    DataVector4 out4;
    ok = ok && DataSetFieldVector4(store, id, 44, &vector4) == DATA_RESULT_OK &&
    DataFieldGetVector4(store, id, 44, &out4) == DATA_RESULT_OK && out4.m_Values[3] == 0.75f;

    DataMatrix4 matrix;
    DataMatrix4 out_matrix;
    for (uint32_t i = 0; i < 16; ++i)
        matrix.m_Values[i] = (float)i;
    matrix.m_Values[3 * 4 + 2] = 44.0f;
    ok = ok && DataSetFieldMatrix4(store, id, 45, &matrix) == DATA_RESULT_OK &&
    DataFieldGetMatrix4(store, id, 45, &out_matrix) == DATA_RESULT_OK && out_matrix.m_Values[14] == 44.0f;

    uint8_t boolean = 0;
    ok = ok && DataSetFieldBoolean(store, id, 47, 1) == DATA_RESULT_OK &&
    DataFieldGetBoolean(store, id, 47, &boolean) == DATA_RESULT_OK && boolean == 1 &&
    DataSetFieldBoolean(store, id, 47, 0) == DATA_RESULT_OK &&
    DataFieldGetBoolean(store, id, 47, &boolean) == DATA_RESULT_OK && boolean == 0;

    const char* string;
    ok = ok && DataSetFieldString(store, id, 48, "copied") == DATA_RESULT_OK &&
    DataFieldGetString(store, id, 48, &string) == DATA_RESULT_OK && string[0] == 'c' &&
    DataSetFieldString(store, id, 48, "override") == DATA_RESULT_OK &&
    DataFieldGetString(store, id, 48, &string) == DATA_RESULT_OK && string[0] == 'o';

    ok = ok && DataRowIterNext(&rows) == DATA_RESULT_END && DataIterNext(&it) == DATA_RESULT_END;
    ok = ok && TestDataBatchFromC(store, id);
    DataStoreUnlock(store);
    DataDestroyQuery(query);
    return ok ? 0 : 1;
}

int TestInlineDataFromC(HDataStore store)
{
    DataQueryField requested_field = { .m_Field = dmHashString64("2.40.10"), .m_Type = DATA_TYPE_VECTOR3 };
    DataQueryDesc  desc = { .m_Fields = &requested_field, .m_FieldCount = 1 };
    HDataQuery     query;
    if (DataCreateQuery(store, &desc, &query) != DATA_RESULT_OK)
        return 1;
    DataStoreLock(store);
    DataIterator    it = DataQueryIter(query);
    int             ok = DataIterNext(&it) == DATA_RESULT_OK;
    DataRowIterator rows = DataIterRows(&it);
    ok = ok && DataRowIterNext(&rows) == DATA_RESULT_OK;
    uint32_t color_field = DataQueryFindField(query, &requested_field);
    ok = ok && DataRowIterGetVector3(&rows, color_field)->m_Values[2] == 3.0f;
    DataStoreUnlock(store);
    DataDestroyQuery(query);
    return ok ? 0 : 1;
}

// Bind once per query, then borrow native values through the public C API.
int TestDataPointersFromC(HDataStore store, int write)
{
    DataQueryField query_fields[] = {
        { 5, DATA_TYPE_MATRIX4 }, { 3, DATA_TYPE_VECTOR3 }, { 1, DATA_TYPE_NUMBER }, { 4, DATA_TYPE_VECTOR4 }, { 2, DATA_TYPE_BOOLEAN }
    };
    DataQueryDesc desc = { 0, 0, 0, 0, query_fields, 5 };
    HDataQuery    query;
    if (DataCreateQuery(store, &desc, &query) != DATA_RESULT_OK)
        return 1;
    int      ok = 1;
    uint32_t fields[5];
    for (uint32_t i = 0; i < 5; ++i)
    {
        fields[i] = DataQueryFindField(query, &query_fields[i]);
        ok = ok && fields[i] != UINT32_MAX;
    }
    DataQueryField wrong = { 1, DATA_TYPE_VECTOR3 };
    ok = ok && DataQueryFindField(query, &wrong) == UINT32_MAX;
    wrong.m_Field = 999;
    ok = ok && DataQueryFindField(query, &wrong) == UINT32_MAX;
    wrong.m_Type = DATA_TYPE_STRING;
    ok = ok && DataQueryFindField(query, &wrong) == UINT32_MAX;
    DataStoreLock(store);
    DataIterator it = DataQueryIter(query);
    uint32_t     count = 0;
    while (ok && DataIterNext(&it) == DATA_RESULT_OK)
    {
        const DataVector3* (*read_vector)(const DataRowIterator*, uint32_t) = DataRowIterGetVector3;
        DataRowIterator rows = DataIterRows(&it);
        while (DataRowIterNext(&rows) == DATA_RESULT_OK)
        {
            ok = ok && *DataRowIterGetNumber(&rows, fields[2]) == 17.0 &&
            *DataRowIterGetBoolean(&rows, fields[4]) == 0 &&
            read_vector(&rows, fields[1])->m_Values[2] == 3.0f &&
            DataRowIterGetVector4(&rows, fields[3])->m_Values[3] == 4.0f &&
            DataRowIterGetMatrix4(&rows, fields[0])->m_Values[15] == 15.0f;
            if (write)
            {
                *DataRowIterGetNumberMut(&rows, fields[2]) = 29.0;
                *DataRowIterGetBooleanMut(&rows, fields[4]) = 1;
                DataRowIterGetVector3Mut(&rows, fields[1])->m_Values[2] = 30.0f;
                DataRowIterGetVector4Mut(&rows, fields[3])->m_Values[3] = 40.0f;
                DataRowIterGetMatrix4Mut(&rows, fields[0])->m_Values[15] = 150.0f;
                ok = ok && *DataRowIterGetNumber(&rows, fields[2]) == 29.0 &&
                *DataRowIterGetBoolean(&rows, fields[4]) == 1 &&
                read_vector(&rows, fields[1])->m_Values[2] == 30.0f &&
                DataRowIterGetVector4(&rows, fields[3])->m_Values[3] == 40.0f &&
                DataRowIterGetMatrix4(&rows, fields[0])->m_Values[15] == 150.0f;
            }
            ++count;
        }
    }
    ok = ok && DataQueryFindField(query, &query_fields[0]) == fields[0] && count == 3;
    DataStoreUnlock(store);
    if (DataQueryTryBegin(query) == DATA_RESULT_OK)
    {
        DataIterator range = DataQueryIterRange(query, 0, DataQueryGetRowCount(query));
        ok = ok && DataQueryGetRowCount(query) == 3 && DataIterNext(&range) == DATA_RESULT_OK;
        DataQueryEnd(query);
    }
    else
        ok = 0;
    DataDestroyQuery(query);
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
        DataVector3  m_Position;
    };
    DataFieldDesc state_fields[] = {
        { .m_Field = 1, .m_Type = DATA_TYPE_NUMBER, .m_Offset = offsetof(struct State, m_Health), .m_Name = "health" },
        { .m_Field = 2, .m_Type = DATA_TYPE_BOOLEAN, .m_Offset = offsetof(struct State, m_Enabled), .m_Name = "enabled" }
    };
    DataStructDesc state = { .m_Fields = state_fields, .m_FieldCount = 2, .m_Size = sizeof(struct State) };
    DataFieldDesc  fields[] = {
        { .m_Field = 3, .m_Type = DATA_TYPE_STRUCT, .m_Offset = offsetof(struct Enemy, m_State), .m_Struct = &state, .m_Name = "state" },
        { .m_Field = 4, .m_Type = DATA_TYPE_VECTOR3, .m_Offset = offsetof(struct Enemy, m_Position) }
    };
    uint64_t      tag = 7;
    DataTableDesc table = { .m_Type = 10, .m_Tags = &tag, .m_TagCount = 1, .m_Fields = fields, .m_FieldCount = 2, .m_RowStride = sizeof(struct Enemy) };
    struct Enemy  input[] = {
        { .m_State = { .m_Health = 50, .m_Enabled = 1 }, .m_Position = { .m_Values = { 1, 2, 3 } } },
        { .m_State = { .m_Health = 75 }, .m_Position = { .m_Values = { 4, 5, 6 } } }
    };
    DataGroupId     group = 100;
    DataId          ids[2], added[2];
    DataQueryField  health = { .m_Field = dmHashString64("state.health"), .m_Type = DATA_TYPE_NUMBER, .m_Access = DATA_ACCESS_READ_WRITE };
    DataQueryDesc   query_desc = { .m_GroupIds = &group, .m_GroupIdCount = 1, .m_AllTags = &tag, .m_AllTagCount = 1, .m_Fields = &health, .m_FieldCount = 1 };
    HDataStore      store = DataCreateStore();
    HDataQuery      query = NULL;
    int             active = 0;
    int             error = 0;
    double          number = 0;
    DataVector3     position;
#define CHECK_NATIVE(condition) \
    do \
    { \
        if (!(condition)) \
        { \
            error = __LINE__; \
            goto cleanup; \
        } \
    } while (0)
    CHECK_NATIVE(DataRegisterTable(store, &table) == DATA_RESULT_OK);
    CHECK_NATIVE(DataCreateQuery(store, &query_desc, &query) == DATA_RESULT_OK);
    CHECK_NATIVE(DataCreateRows(store, 10, group, 2, input, ids) == DATA_RESULT_OK);
    CHECK_NATIVE(DataFieldGetNumber(store, ids[1], health.m_Field, &number) == DATA_RESULT_OK && number == 75);
    CHECK_NATIVE(DataFieldGetVector3(store, ids[1], 4, &position) == DATA_RESULT_OK && position.m_Values[2] == 6);
    CHECK_NATIVE(DataGetComponentId(store, ids[1]) == 0);

    // Prepare complete rows on the caller side, including nested fields.
    struct Enemy enemies[] = { input[0], input[0] };
    enemies[0].m_State.m_Health = 80;
    enemies[1].m_State.m_Health = 90;
    CHECK_NATIVE(DataCreateRows(store, 10, 200, 2, enemies, added) == DATA_RESULT_OK);
    CHECK_NATIVE(DataSetFieldNumber(store, added[1], health.m_Field, 5) == DATA_RESULT_OK);
    CHECK_NATIVE(DataResetRow(store, added[1]) == DATA_RESULT_OK);
    CHECK_NATIVE(DataFieldGetNumber(store, added[1], health.m_Field, &number) == DATA_RESULT_OK && number == 90);
    CHECK_NATIVE(DataFieldGetVector3(store, added[1], 4, &position) == DATA_RESULT_OK && position.m_Values[2] == 3);

    CHECK_NATIVE(DataQueryTryBegin(query) == DATA_RESULT_OK);
    active = 1;
    CHECK_NATIVE(DataQueryGetRowCount(query) == 2);
    DataIterator it = DataQueryIterRange(query, 0, 2);
    CHECK_NATIVE(DataIterNext(&it) == DATA_RESULT_OK);
    DataRowIterator rows = DataIterRows(&it);
    CHECK_NATIVE(DataRowIterNext(&rows) == DATA_RESULT_OK && DataRowIterGetGroupId(&rows) == 100);
    CHECK_NATIVE(DataCreateRows(store, 10, 200, 2, enemies, added) == DATA_RESULT_LOCKED);
    DataQueryEnd(query);
    active = 0;

    // Failed input leaves IDs and query membership intact.
    DataId unchanged[] = { 123, 456 };
    enemies[1].m_State.m_Enabled = 2;
    CHECK_NATIVE(DataCreateRows(store, 10, 200, 2, enemies, unchanged) == DATA_RESULT_INVALID_ARGUMENT);
    enemies[1].m_State.m_Enabled = 1;
    CHECK_NATIVE(DataCreateRows(store, 10, 200, 2, NULL, unchanged) == DATA_RESULT_INVALID_ARGUMENT);
    CHECK_NATIVE(unchanged[0] == 123 && unchanged[1] == 456);
    CHECK_NATIVE(DataCreateRows(store, 10, 200, 0, NULL, NULL) == DATA_RESULT_OK);
    CHECK_NATIVE(DataCreateRows(NULL, 999, 200, 0, NULL, NULL) == DATA_RESULT_OK);

    CHECK_NATIVE(DataRemoveRow(store, ids[0]) == DATA_RESULT_OK);
    CHECK_NATIVE(DataFieldGetNumber(store, ids[0], health.m_Field, &number) == DATA_RESULT_NOT_FOUND);
    CHECK_NATIVE(DataFieldGetNumber(store, added[1], health.m_Field, &number) == DATA_RESULT_OK && number == 90);
    // Caller-prepared defaults create independent rows, including group zero.
    enemies[1] = enemies[0];
    CHECK_NATIVE(DataCreateRows(store, 10, 0, 2, enemies, added) == DATA_RESULT_OK);
    CHECK_NATIVE(added[0] != ids[0]);
    CHECK_NATIVE(DataFieldGetNumber(store, added[1], health.m_Field, &number) == DATA_RESULT_OK && number == 80);
    enemies[0].m_State.m_Health = -1;
    CHECK_NATIVE(DataSetFieldNumber(store, added[0], health.m_Field, 5) == DATA_RESULT_OK);
    CHECK_NATIVE(DataFieldGetNumber(store, added[1], health.m_Field, &number) == DATA_RESULT_OK && number == 80);
    CHECK_NATIVE(DataResetRow(store, added[0]) == DATA_RESULT_OK);
    CHECK_NATIVE(DataFieldGetNumber(store, added[0], health.m_Field, &number) == DATA_RESULT_OK && number == 80);
    CHECK_NATIVE(DataQueryTryBegin(query) == DATA_RESULT_OK);
    active = 1;
    CHECK_NATIVE(DataQueryGetRowCount(query) == 1);
    DataQueryEnd(query);
    active = 0;
    CHECK_NATIVE(DataUnregisterTable(store, 10) == DATA_RESULT_OK);
    CHECK_NATIVE(DataFieldGetNumber(store, ids[1], health.m_Field, &number) == DATA_RESULT_NOT_FOUND);

    // A NULL native string is invalid; valid references are copied on insertion.
    DataFieldDesc string = { .m_Field = 1, .m_Type = DATA_TYPE_STRING };
    table.m_Fields = &string;
    table.m_FieldCount = 1;
    table.m_RowStride = 8;
    CHECK_NATIVE(DataRegisterTable(store, &table) == DATA_RESULT_OK);
    DataReference missing_strings[2] = { 0 };
    CHECK_NATIVE(DataCreateRows(store, 10, 200, 2, missing_strings, unchanged) == DATA_RESULT_INVALID_ARGUMENT);
cleanup:
    if (active)
        DataQueryEnd(query);
    if (query)
        DataDestroyQuery(query);
    DataDestroyStore(store);
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
        DataVector3 m_Direction;
    };
    struct Enemy
    {
        uint8_t      m_Active;
        DataVector3  m_Position;
        struct State m_State;
        DataVector4  m_Color;
        DataMatrix4  m_Transform;
    };
    DataFieldDesc state_fields[] = {
        { .m_Field = 1, .m_Type = DATA_TYPE_NUMBER, .m_Offset = offsetof(struct State, m_Health), .m_Name = "health" },
        { .m_Field = 2, .m_Type = DATA_TYPE_BOOLEAN, .m_Offset = offsetof(struct State, m_Enabled), .m_Name = "enabled" },
        { .m_Field = 3, .m_Type = DATA_TYPE_VECTOR3, .m_Offset = offsetof(struct State, m_Direction), .m_Name = "direction" }
    };
    DataStructDesc state = { .m_Fields = state_fields, .m_FieldCount = 3, .m_Size = sizeof(struct State) };
    DataFieldDesc  layout[] = {
        { .m_Field = 10, .m_Type = DATA_TYPE_BOOLEAN, .m_Offset = offsetof(struct Enemy, m_Active) },
        { .m_Field = 20, .m_Type = DATA_TYPE_VECTOR3, .m_Offset = offsetof(struct Enemy, m_Position) },
        { .m_Field = 30, .m_Type = DATA_TYPE_STRUCT, .m_Offset = offsetof(struct Enemy, m_State), .m_Struct = &state, .m_Name = "state" },
        { .m_Field = 40, .m_Type = DATA_TYPE_VECTOR4, .m_Offset = offsetof(struct Enemy, m_Color) },
        { .m_Field = 50, .m_Type = DATA_TYPE_MATRIX4, .m_Offset = offsetof(struct Enemy, m_Transform) }
    };
    DataTableDesc table = { .m_Type = 10, .m_Fields = layout, .m_FieldCount = 5, .m_RowStride = sizeof(struct Enemy) };
    uint8_t       active[] = { 1, 0 };
    DataVector3   positions[] = { { .m_Values = { 1, 2, 3 } }, { .m_Values = { 4, 5, 6 } } };
    struct State  states[] = {
        { .m_Health = 50, .m_Enabled = 1, .m_Direction = { .m_Values = { 1, 0, 0 } } },
        { .m_Health = 75, .m_Enabled = 0, .m_Direction = { .m_Values = { 0, 1, 0 } } }
    };
    DataVector4 colors[] = { { .m_Values = { 7, 8, 9, 10 } }, { .m_Values = { 11, 12, 13, 14 } } };
    DataMatrix4 transforms[2] = { 0 };
    for (uint32_t r = 0; r < 2; ++r)
        for (uint32_t i = 0; i < 16; ++i)
            transforms[r].m_Values[i] = (float)(r * 16 + i);
    // Input order is independent of layout and byte order.
    DataFieldArray fields[] = {
        { .m_Field = 50, .m_Values = transforms },
        { .m_Field = 30, .m_Values = states },
        { .m_Field = 10, .m_Values = active },
        { .m_Field = 40, .m_Values = colors },
        { .m_Field = 20, .m_Values = positions }
    };
    DataId         ids[2] = { 123, 456 };
    DataGroupId    group = 77;
    DataQueryField health = { .m_Field = dmHashString64("state.health"), .m_Type = DATA_TYPE_NUMBER };
    DataQueryDesc  query_desc = { .m_GroupIds = &group, .m_GroupIdCount = 1, .m_Fields = &health, .m_FieldCount = 1 };
    HDataStore     store = DataCreateStore();
    HDataQuery     query = NULL;
    int            error = 0;
    int            reserved = 0;
    int            locked = 0;
#define CHECK_SOA(condition) \
    do \
    { \
        if (!(condition)) \
        { \
            error = __LINE__; \
            goto cleanup; \
        } \
    } while (0)
    CHECK_SOA(DataRegisterTable(store, &table) == DATA_RESULT_OK);
    CHECK_SOA(DataCreateQuery(store, &query_desc, &query) == DATA_RESULT_OK);
    CHECK_SOA(DataCreateRowsSoA(store, 999, group, 2, 5, fields, ids) == DATA_RESULT_NOT_FOUND);
    CHECK_SOA(DataCreateRowsSoA(store, 10, group, 0, 0, NULL, NULL) == DATA_RESULT_OK);
    CHECK_SOA(DataCreateRowsSoA(NULL, 999, group, 0, 5, NULL, NULL) == DATA_RESULT_OK);
    CHECK_SOA(DataCreateRowsSoA(store, 10, group, UINT32_MAX, 5, fields, ids) == DATA_RESULT_INVALID_ARGUMENT);
    CHECK_SOA(DataCreateRowsSoA(store, 10, group, 2, 4, fields, ids) == DATA_RESULT_INVALID_ARGUMENT);
    CHECK_SOA(DataCreateRowsSoA(store, 10, group, 2, 5, NULL, ids) == DATA_RESULT_INVALID_ARGUMENT);
    fields[4].m_Field = 40;
    CHECK_SOA(DataCreateRowsSoA(store, 10, group, 2, 5, fields, ids) == DATA_RESULT_INVALID_ARGUMENT);
    fields[4].m_Field = 999;
    CHECK_SOA(DataCreateRowsSoA(store, 10, group, 2, 5, fields, ids) == DATA_RESULT_INVALID_ARGUMENT);
    fields[4].m_Field = health.m_Field;
    CHECK_SOA(DataCreateRowsSoA(store, 10, group, 2, 5, fields, ids) == DATA_RESULT_INVALID_ARGUMENT);
    fields[4].m_Field = 20;
    fields[4].m_Values = NULL;
    CHECK_SOA(DataCreateRowsSoA(store, 10, group, 2, 5, fields, ids) == DATA_RESULT_INVALID_ARGUMENT);
    fields[4].m_Values = positions;
    active[1] = 2;
    CHECK_SOA(DataCreateRowsSoA(store, 10, group, 2, 5, fields, ids) == DATA_RESULT_INVALID_ARGUMENT);
    active[1] = 0;
    states[1].m_Enabled = 2;
    CHECK_SOA(DataCreateRowsSoA(store, 10, group, 2, 5, fields, ids) == DATA_RESULT_INVALID_ARGUMENT);
    states[1].m_Enabled = 0;
    CHECK_SOA(ids[0] == 123 && ids[1] == 456);

    CHECK_SOA(DataCreateRowsSoA(store, 10, group, 2, 5, fields, ids) == DATA_RESULT_OK);
    for (uint32_t i = 0; i < 2; ++i)
    {
        double      number;
        uint8_t     boolean;
        DataVector3 position;
        DataVector4 color;
        DataMatrix4 transform;
        CHECK_SOA(DataFieldGetNumber(store, ids[i], health.m_Field, &number) == DATA_RESULT_OK && number == states[i].m_Health);
        CHECK_SOA(DataFieldGetBoolean(store, ids[i], 10, &boolean) == DATA_RESULT_OK && boolean == active[i]);
        CHECK_SOA(DataFieldGetBoolean(store, ids[i], dmHashString64("state.enabled"), &boolean) == DATA_RESULT_OK && boolean == states[i].m_Enabled);
        CHECK_SOA(DataFieldGetVector3(store, ids[i], 20, &position) == DATA_RESULT_OK && position.m_Values[2] == positions[i].m_Values[2]);
        CHECK_SOA(DataFieldGetVector4(store, ids[i], 40, &color) == DATA_RESULT_OK && color.m_Values[3] == colors[i].m_Values[3]);
        CHECK_SOA(DataFieldGetMatrix4(store, ids[i], 50, &transform) == DATA_RESULT_OK && transform.m_Values[15] == transforms[i].m_Values[15]);
        CHECK_SOA(DataFieldGetVector3(store, ids[i], dmHashString64("state.direction"), &position) == DATA_RESULT_OK && position.m_Values[i] == 1);
        CHECK_SOA(DataGetComponentId(store, ids[i]) == 0);
    }
    states[1].m_Health = -1;
    positions[1].m_Values[2] = -1;
    CHECK_SOA(DataSetFieldNumber(store, ids[1], health.m_Field, 5) == DATA_RESULT_OK);
    CHECK_SOA(DataSetFieldVector3(store, ids[1], 20, &positions[1]) == DATA_RESULT_OK);
    CHECK_SOA(DataResetRow(store, ids[1]) == DATA_RESULT_OK);
    double      number;
    DataVector3 position;
    CHECK_SOA(DataFieldGetNumber(store, ids[1], health.m_Field, &number) == DATA_RESULT_OK && number == 75);
    CHECK_SOA(DataFieldGetVector3(store, ids[1], 20, &position) == DATA_RESULT_OK && position.m_Values[2] == 6);

    CHECK_SOA(DataQueryTryBegin(query) == DATA_RESULT_OK);
    reserved = 1;
    CHECK_SOA(DataQueryGetRowCount(query) == 2);
    DataIterator it = DataQueryIterRange(query, 0, 2);
    CHECK_SOA(DataIterNext(&it) == DATA_RESULT_OK);
    DataRowIterator rows = DataIterRows(&it);
    CHECK_SOA(DataRowIterNext(&rows) == DATA_RESULT_OK && DataRowIterGetGroupId(&rows) == group);
    CHECK_SOA(DataCreateRowsSoA(store, 10, group, 2, 5, fields, ids) == DATA_RESULT_LOCKED);
    DataQueryEnd(query);
    reserved = 0;
    DataStoreLock(store);
    locked = 1;
    CHECK_SOA(DataCreateRowsSoA(store, 10, group, 2, 5, fields, ids) == DATA_RESULT_LOCKED);
    DataStoreUnlock(store);
    locked = 0;
    DataId removed = ids[0];
    CHECK_SOA(DataRemoveRow(store, removed) == DATA_RESULT_OK);
    CHECK_SOA(DataCreateRowsSoA(store, 10, 0, 2, 5, fields, ids) == DATA_RESULT_OK);
    CHECK_SOA(ids[0] != removed);
    CHECK_SOA(DataFieldGetNumber(store, removed, health.m_Field, &number) == DATA_RESULT_NOT_FOUND);
    CHECK_SOA(DataQueryTryBegin(query) == DATA_RESULT_OK);
    reserved = 1;
    CHECK_SOA(DataQueryGetRowCount(query) == 1);
    DataQueryEnd(query);
    reserved = 0;

    DataTableDesc empty = { .m_Type = 11 };
    CHECK_SOA(DataRegisterTable(store, &empty) == DATA_RESULT_OK);
    CHECK_SOA(DataCreateRowsSoA(store, 11, group, 2, 0, NULL, ids) == DATA_RESULT_OK);
    CHECK_SOA(DataRemoveRow(store, ids[1]) == DATA_RESULT_OK);
    DataStructDesc zero_struct = { 0 };
    DataFieldDesc  zero_field = { .m_Field = 1, .m_Type = DATA_TYPE_STRUCT, .m_Struct = &zero_struct, .m_Name = "empty" };
    DataTableDesc  zero_table = { .m_Type = 13, .m_Fields = &zero_field, .m_FieldCount = 1 };
    DataFieldArray zero_values = { .m_Field = 1 };
    CHECK_SOA(DataRegisterTable(store, &zero_table) == DATA_RESULT_OK);
    CHECK_SOA(DataCreateRowsSoA(store, 13, 0, 2, 1, &zero_values, ids) == DATA_RESULT_OK);
    CHECK_SOA(DataRemoveRow(store, ids[0]) == DATA_RESULT_OK);
    DataFieldDesc string = { .m_Field = 1, .m_Type = DATA_TYPE_STRING };
    DataTableDesc references = { .m_Type = 12, .m_Fields = &string, .m_FieldCount = 1, .m_RowStride = 8 };
    CHECK_SOA(DataRegisterTable(store, &references) == DATA_RESULT_OK);
    CHECK_SOA(DataCreateRowsSoA(store, 12, group, 2, 1, fields, ids) == DATA_RESULT_INVALID_ARGUMENT);
cleanup:
    if (locked)
        DataStoreUnlock(store);
    if (reserved)
        DataQueryEnd(query);
    if (query)
        DataDestroyQuery(query);
    DataDestroyStore(store);
#undef CHECK_SOA
    return error;
}

// References are constructed through the public API from stack-owned input.
int CreateReferenceRowsFromC(HDataStore store, int soa, DataId ids[2])
{
    char          name[] = "Guard";
    char          faction[] = "guards";
    char          loot_name[] = "coin";
    DataFieldDesc members[] = {
        { .m_Field = 1, .m_Type = DATA_TYPE_STRING, .m_Offset = offsetof(struct ReferenceAttributes, m_Faction), .m_Name = "faction" },
        { .m_Field = 2, .m_Type = DATA_TYPE_NUMBER, .m_Offset = offsetof(struct ReferenceAttributes, m_AlertRadius), .m_Name = "alert_radius" }
    };
    DataStructDesc             layout = { .m_Fields = members, .m_FieldCount = 2, .m_Size = sizeof(struct ReferenceAttributes) };
    struct ReferenceAttributes attributes[] = {
        { .m_Faction = { .m_String = faction }, .m_AlertRadius = 15.0 },
        { .m_Faction = { .m_String = "bandits" }, .m_AlertRadius = 25.0 }
    };
    DataStructInput objects[] = {
        { .m_Layout = &layout, .m_Values = &attributes[0] },
        { .m_Layout = &layout, .m_Values = &attributes[1] }
    };
    DataReference strings[] = { { .m_String = loot_name }, { .m_String = "health_pickup" } };
    DataReference structures[] = { { .m_Struct = &objects[0] }, { .m_Struct = &objects[1] } };
    DataListInput lists[] = {
        { .m_Type = DATA_TYPE_STRING, .m_Count = 2, .m_Values = strings },
        { .m_Type = DATA_TYPE_STRUCT, .m_Count = 2, .m_Values = structures }
    };
    DataReference names[] = { { .m_String = name }, { .m_String = "Bandit" } };
    DataReference loot[] = { { .m_List = &lists[0] }, { .m_List = &lists[1] } };
    DataFieldDesc fields[] = {
        { .m_Field = 10, .m_Type = DATA_TYPE_STRING, .m_Offset = offsetof(struct ReferenceEnemy, m_Name) },
        { .m_Field = 11, .m_Type = DATA_TYPE_STRUCT, .m_Offset = offsetof(struct ReferenceEnemy, m_Attributes) },
        { .m_Field = 12, .m_Type = DATA_TYPE_LIST, .m_Offset = offsetof(struct ReferenceEnemy, m_Loot) },
        { .m_Field = 13, .m_Type = DATA_TYPE_STRUCT, .m_Offset = offsetof(struct ReferenceEnemy, m_State), .m_Struct = &layout, .m_Name = "state" }
    };
    DataTableDesc table = { .m_Type = 93, .m_Fields = fields, .m_FieldCount = 4, .m_RowStride = sizeof(struct ReferenceEnemy) };
    if (DataRegisterTable(store, &table) != DATA_RESULT_OK)
        return 1;

    DataResult result;
    if (soa)
    {
        // Supply columns out of metadata order, including complete inline structs.
        DataFieldArray columns[] = {
            { .m_Field = 12, .m_Values = loot },
            { .m_Field = 13, .m_Values = attributes },
            { .m_Field = 10, .m_Values = names },
            { .m_Field = 11, .m_Values = structures }
        };
        result = DataCreateRowsSoA(store, 93, 42, 2, 4, columns, ids);
    }
    else
    {
        struct ReferenceEnemy enemies[] = {
            { .m_Name = names[0], .m_Attributes = structures[0], .m_Loot = loot[0], .m_State = attributes[0] },
            { .m_Name = names[1], .m_Attributes = structures[1], .m_Loot = loot[1], .m_State = attributes[1] }
        };
        result = DataCreateRows(store, 93, 42, 2, enemies, ids);
    }
    if (result != DATA_RESULT_OK)
        return 2;

    // Neither payloads nor descriptors may be retained from caller-owned storage.
    name[0] = faction[0] = loot_name[0] = 'X';
    attributes[0].m_AlertRadius = -1;
    const char* stored_name;
    if (DataFieldGetString(store, ids[0], 10, &stored_name) != DATA_RESULT_OK || strcmp(stored_name, "Guard"))
        return 3;
    if (DataFieldGetString(store, ids[0], dmHashString64("state.faction"), &stored_name) != DATA_RESULT_OK || strcmp(stored_name, "guards"))
        return 4;
    if (DataSetFieldString(store, ids[0], 10, "Captain") != DATA_RESULT_OK || DataResetRow(store, ids[0]) != DATA_RESULT_OK)
        return 5;
    return DataFieldGetString(store, ids[0], 10, &stored_name) == DATA_RESULT_OK && strcmp(stored_name, "Guard") == 0 ? 0 : 6;
}

int CreateMixedListRowsFromC(HDataStore store, int soa, DataId ids[2])
{
    double          number = 100.0;
    char            string[] = "coin";
    uint8_t         boolean = 1;
    DataVector3     vector3 = { .m_Values = { 1, 2, 3 } };
    DataVector4     vector4 = { .m_Values = { 4, 5, 6, 7 } };
    DataMatrix4     matrix = { .m_Values = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 } };
    DataFieldDesc   member = { .m_Field = 42, .m_Type = DATA_TYPE_NUMBER };
    DataStructDesc  layout = { .m_Fields = &member, .m_FieldCount = 1, .m_Size = sizeof(number) };
    DataStructInput object = { .m_Layout = &layout, .m_Values = &number };
    double          numbers[] = { 1, 2 };
    DataListInput   nested = { .m_Type = DATA_TYPE_NUMBER, .m_Count = 2, .m_Values = numbers };
    DataValueType   types[] = {
        DATA_TYPE_NUMBER, DATA_TYPE_STRING, DATA_TYPE_BOOLEAN, DATA_TYPE_STRUCT, DATA_TYPE_LIST, DATA_TYPE_NULL, DATA_TYPE_VECTOR3, DATA_TYPE_VECTOR4, DATA_TYPE_MATRIX4
    };
    const void* values[] = { &number, string, &boolean, &object, &nested, NULL, &vector3, &vector4, &matrix };
    // m_Type is ignored when per-element kinds are supplied.
    DataListInput mixed = { .m_Type = (DataValueType)UINT32_MAX, .m_Count = 9, .m_Values = values, .m_Types = types };
    DataReference child = { .m_List = &mixed };
    DataListInput parent = { .m_Type = DATA_TYPE_LIST, .m_Count = 1, .m_Values = &child };
    DataReference rows[] = { { .m_List = &mixed }, { .m_List = &parent } };
    DataFieldDesc field = { .m_Field = 1, .m_Type = DATA_TYPE_LIST };
    DataTableDesc table = { .m_Type = 94, .m_Fields = &field, .m_FieldCount = 1, .m_RowStride = sizeof(DataReference) };
    if (DataRegisterTable(store, &table) != DATA_RESULT_OK)
        return 1;
    DataResult result;
    if (soa)
    {
        DataFieldArray column = { .m_Field = 1, .m_Values = rows };
        result = DataCreateRowsSoA(store, 94, 42, 2, 1, &column, ids);
    }
    else
        result = DataCreateRows(store, 94, 42, 2, rows, ids);

    number = -1;
    string[0] = 'X';
    boolean = 0;
    numbers[0] = -1;
    vector3.m_Values[2] = -1;
    vector4.m_Values[3] = -1;
    matrix.m_Values[15] = -1;
    types[0] = DATA_TYPE_NULL;
    values[1] = NULL;
    member.m_Field = 0;
    return result == DATA_RESULT_OK ? 0 : 2;
}
