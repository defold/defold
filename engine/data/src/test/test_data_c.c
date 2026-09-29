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

#include <dmsdk/data/data.h>

int TestDataStoreLifecycleFromC(void)
{
    HDataStore store = DataCreateStore();
    DataStoreLock(store);
    DataResult locked = DataDestroyStore(store);
    DataStoreUnlock(store);
    DataResult destroyed = DataDestroyStore(store);
    return locked == DATA_RESULT_LOCKED && destroyed == DATA_RESULT_OK ? 0 : 1;
}

// Exercise the public inline getters from a C-only consumer, including use as a callback.
int TestDataEmptyFieldFromC(const DataFieldIterator* field)
{
    double      number = 123;
    uint8_t     boolean = 7;
    const char* original = "unchanged";
    const char* string = original;
    DataVector3 vector3 = { { 1, 2, 3 } };
    DataVector4 vector4 = { { 4, 5, 6, 7 } };
    DataMatrix4 matrix = { { 8 } };
    DataResult (*read_vector3)(const DataFieldIterator*, DataVector3*) = DataFieldIterGetVector3;
    int ok = DataFieldIterGetNumber(field, &number) == DATA_RESULT_INVALID_ARGUMENT && number == 123;
    ok = ok && DataFieldIterGetBoolean(field, &boolean) == DATA_RESULT_INVALID_ARGUMENT && boolean == 7;
    ok = ok && DataFieldIterGetString(field, &string) == DATA_RESULT_INVALID_ARGUMENT && string == original;
    ok = ok && read_vector3(field, &vector3) == DATA_RESULT_INVALID_ARGUMENT && vector3.m_Values[0] == 1 && vector3.m_Values[2] == 3;
    ok = ok && DataFieldIterGetVector4(field, &vector4) == DATA_RESULT_INVALID_ARGUMENT && vector4.m_Values[0] == 4 && vector4.m_Values[3] == 7;
    ok = ok && DataFieldIterGetMatrix4(field, &matrix) == DATA_RESULT_INVALID_ARGUMENT && matrix.m_Values[0] == 8 && matrix.m_Values[15] == 0;
    return ok ? 0 : 1;
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
    DataOwnerId    owner = 123;
    uint64_t       tag = 7;
    DataQueryField fields[] = { { 42, DATA_VALUE_TYPE_NUMBER }, { 43, DATA_VALUE_TYPE_VECTOR3 }, { 44, DATA_VALUE_TYPE_VECTOR4 }, { 45, DATA_VALUE_TYPE_MATRIX4 }, { 47, DATA_VALUE_TYPE_BOOLEAN }, { 48, DATA_VALUE_TYPE_STRING }, { 46, DATA_VALUE_TYPE_STRUCT } };
    DataQueryDesc  desc = { &owner, 1, &tag, 1, fields, 7 };
    HDataQuery     query;
    if (DataCreateQuery(store, &desc, &query) != DATA_RESULT_OK)
        return 1;
    DataStoreLock(store);
    DataIterator    it = DataQueryIter(query);
    int             ok = DataIterNext(&it) == DATA_RESULT_OK;
    DataRowIterator rows = DataIterRows(&it);
    ok = ok && DataRowIterNext(&rows) == DATA_RESULT_OK && DataRowIterGetOwnerId(&rows) == owner;
    DataId            id = DataRowIterGetId(&rows);
    DataFieldIterator field = DataRowIterFields(&rows);
    double            number = 0;
    ok = ok && DataFieldIterNext(&field) == DATA_RESULT_OK && field.m_Index == 0 &&
    DataFieldIterGetNameHash(&field) == 42 && DataFieldIterGetType(&field) == DATA_VALUE_TYPE_NUMBER &&
    DataFieldIterGetNumber(&field, &number) == DATA_RESULT_OK && number == 17.0;
    ok = ok && DataSetFieldNumber(store, id, 42, 23.0) == DATA_RESULT_OK &&
    DataFieldGetNumber(store, id, 42, &number) == DATA_RESULT_OK && number == 23.0 &&
    DataFieldIterSetNumber(&field, 29.0) == DATA_RESULT_OK &&
    DataFieldIterGetNumber(&field, &number) == DATA_RESULT_OK && number == 29.0;

    ok = ok && DataFieldIterNext(&field) == DATA_RESULT_OK;

    DataVector3 vector3 = { { 1.0f, 0.5f, 0.25f } };
    DataVector3 out3;
    ok = ok && DataSetFieldVector3(store, id, 43, &vector3) == DATA_RESULT_OK &&
    DataFieldGetVector3(store, id, 43, &out3) == DATA_RESULT_OK && out3.m_Values[1] == 0.5f &&
    DataFieldIterSetVector3(&field, &vector3) == DATA_RESULT_OK &&
    DataFieldIterGetVector3(&field, &out3) == DATA_RESULT_OK && out3.m_Values[2] == 0.25f;

    ok = ok && DataFieldIterNext(&field) == DATA_RESULT_OK;

    DataVector4 vector4 = { { 1.0f, 0.5f, 0.25f, 0.75f } };
    DataVector4 out4;
    ok = ok && DataSetFieldVector4(store, id, 44, &vector4) == DATA_RESULT_OK &&
    DataFieldGetVector4(store, id, 44, &out4) == DATA_RESULT_OK && out4.m_Values[3] == 0.75f &&
    DataFieldIterSetVector4(&field, &vector4) == DATA_RESULT_OK &&
    DataFieldIterGetVector4(&field, &out4) == DATA_RESULT_OK && out4.m_Values[0] == 1.0f;

    ok = ok && DataFieldIterNext(&field) == DATA_RESULT_OK;

    DataMatrix4 matrix;
    DataMatrix4 out_matrix;
    for (uint32_t i = 0; i < 16; ++i)
        matrix.m_Values[i] = (float)i;
    matrix.m_Values[3 * 4 + 2] = 44.0f;
    ok = ok && DataSetFieldMatrix4(store, id, 45, &matrix) == DATA_RESULT_OK &&
    DataFieldGetMatrix4(store, id, 45, &out_matrix) == DATA_RESULT_OK && out_matrix.m_Values[14] == 44.0f &&
    DataFieldIterSetMatrix4(&field, &matrix) == DATA_RESULT_OK &&
    DataFieldIterGetMatrix4(&field, &out_matrix) == DATA_RESULT_OK && out_matrix.m_Values[0] == 0.0f;

    ok = ok && DataFieldIterNext(&field) == DATA_RESULT_OK;

    uint8_t boolean = 0;
    ok = ok && DataSetFieldBoolean(store, id, 47, 1) == DATA_RESULT_OK &&
    DataFieldGetBoolean(store, id, 47, &boolean) == DATA_RESULT_OK && boolean == 1 &&
    DataFieldIterSetBoolean(&field, 0) == DATA_RESULT_OK &&
    DataFieldIterGetBoolean(&field, &boolean) == DATA_RESULT_OK && boolean == 0;

    ok = ok && DataFieldIterNext(&field) == DATA_RESULT_OK;

    const char* string;
    ok = ok && DataSetFieldString(store, id, 48, "copied") == DATA_RESULT_OK &&
    DataFieldGetString(store, id, 48, &string) == DATA_RESULT_OK && string[0] == 'c' &&
    DataFieldIterSetString(&field, "override") == DATA_RESULT_OK &&
    DataFieldIterGetString(&field, &string) == DATA_RESULT_OK && string[0] == 'o';

    ok = ok && DataFieldIterNext(&field) == DATA_RESULT_OK;

    ok = ok && DataFieldIterGetNameHash(&field) == 46 && DataFieldIterGetType(&field) == DATA_VALUE_TYPE_STRUCT &&
    DataFieldIterNext(&field) == DATA_RESULT_END &&
    DataRowIterNext(&rows) == DATA_RESULT_END && DataIterNext(&it) == DATA_RESULT_END;
    ok = ok && TestDataBatchFromC(store, id);
    DataStoreUnlock(store);
    DataDestroyQuery(query);
    return ok ? 0 : 1;
}

int TestInlineDataFromC(HDataStore store)
{
    uint64_t       path[] = { 40, 10 };
    DataQueryField requested_field = { 2, DATA_VALUE_TYPE_VECTOR3, path, 2 };
    DataQueryDesc  desc = { .m_Fields = &requested_field, .m_FieldCount = 1 };
    HDataQuery     query;
    if (DataCreateQuery(store, &desc, &query) != DATA_RESULT_OK)
        return 1;
    DataStoreLock(store);
    DataIterator    it = DataQueryIter(query);
    int             ok = DataIterNext(&it) == DATA_RESULT_OK;
    DataRowIterator rows = DataIterRows(&it);
    ok = ok && DataRowIterNext(&rows) == DATA_RESULT_OK;
    DataFieldIterator field = DataRowIterFields(&rows);
    DataVector3       color;
    ok = ok && DataFieldIterNext(&field) == DATA_RESULT_OK && DataFieldIterGetNameHash(&field) == 10 &&
    DataFieldIterGetType(&field) == DATA_VALUE_TYPE_VECTOR3 && DataFieldIterGetVector3(&field, &color) == DATA_RESULT_OK && color.m_Values[2] == 3.0f;
    DataStoreUnlock(store);
    DataDestroyQuery(query);
    return ok ? 0 : 1;
}

// Bind once per query, then borrow native values through the public C API.
int TestDataPointersFromC(HDataStore store, int write)
{
    DataQueryField query_fields[] = {
        { 5, DATA_VALUE_TYPE_MATRIX4 }, { 3, DATA_VALUE_TYPE_VECTOR3 }, { 1, DATA_VALUE_TYPE_NUMBER }, { 4, DATA_VALUE_TYPE_VECTOR4 }, { 2, DATA_VALUE_TYPE_BOOLEAN }
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
    DataQueryField wrong = { 1, DATA_VALUE_TYPE_VECTOR3 };
    ok = ok && DataQueryFindField(query, &wrong) == UINT32_MAX;
    wrong.m_Field = 999;
    ok = ok && DataQueryFindField(query, &wrong) == UINT32_MAX;
    wrong.m_Type = DATA_VALUE_TYPE_STRING;
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
