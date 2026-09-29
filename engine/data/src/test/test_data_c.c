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

// Scalar and batch getters have identical value and failure semantics in C.
static int TestDataBatchFromC(HDataStore store, DataId id)
{
    DataId      ids[] = { id, 0, id };
    DataResult  results[3];
    double      numbers[] = { 0, 123, 0 };
    uint8_t     booleans[] = { 0, 7, 0 };
    const char* strings[] = { 0, "unchanged", 0 };
    DataVector3 vectors3[3] = { { .m_Values = { 0 } }, { .m_Values = { 123, 123, 123 } }, { .m_Values = { 0 } } };
    DataVector4 vectors4[3] = { { .m_Values = { 0 } }, { .m_Values = { 123, 123, 123, 123 } }, { .m_Values = { 0 } } };
    DataMatrix4 matrices[3] = { { .m_Values = { 0 } }, { .m_Values = { 123 } }, { .m_Values = { 0 } } };
    DataGetFieldNumberBatch(store, ids, 42, 3, numbers, results);
    int ok = results[0] == DATA_RESULT_OK && results[1] == DATA_RESULT_NOT_FOUND && results[2] == DATA_RESULT_OK && numbers[0] == 29 && numbers[1] == 123 && numbers[2] == 29;
    DataGetFieldBooleanBatch(store, ids, 47, 3, booleans, results);
    ok = ok && results[0] == DATA_RESULT_OK && results[1] == DATA_RESULT_NOT_FOUND && results[2] == DATA_RESULT_OK && booleans[0] == 0 && booleans[1] == 7 && booleans[2] == 0;
    DataGetFieldStringBatch(store, ids, 48, 3, strings, results);
    ok = ok && results[0] == DATA_RESULT_OK && results[1] == DATA_RESULT_NOT_FOUND && results[2] == DATA_RESULT_OK && strings[0][0] == 'o' && strings[1][0] == 'u' && strings[2] == strings[0];
    DataGetFieldVector3Batch(store, ids, 43, 3, vectors3, results);
    ok = ok && results[0] == DATA_RESULT_OK && results[1] == DATA_RESULT_NOT_FOUND && results[2] == DATA_RESULT_OK && vectors3[0].m_Values[2] == 0.25f && vectors3[1].m_Values[2] == 123 && vectors3[2].m_Values[2] == 0.25f;
    DataGetFieldVector4Batch(store, ids, 44, 3, vectors4, results);
    ok = ok && results[0] == DATA_RESULT_OK && results[1] == DATA_RESULT_NOT_FOUND && results[2] == DATA_RESULT_OK && vectors4[0].m_Values[3] == 0.75f && vectors4[1].m_Values[3] == 123 && vectors4[2].m_Values[3] == 0.75f;
    DataGetFieldMatrix4Batch(store, ids, 45, 3, matrices, results);
    ok = ok && results[0] == DATA_RESULT_OK && results[1] == DATA_RESULT_NOT_FOUND && results[2] == DATA_RESULT_OK && matrices[0].m_Values[14] == 44 && matrices[1].m_Values[0] == 123 && matrices[2].m_Values[14] == 44;
    return ok;
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
    DataGetFieldNumber(store, id, 42, &number) == DATA_RESULT_OK && number == 23.0 &&
    DataFieldIterSetNumber(&field, 29.0) == DATA_RESULT_OK &&
    DataFieldIterGetNumber(&field, &number) == DATA_RESULT_OK && number == 29.0;

    ok = ok && DataFieldIterNext(&field) == DATA_RESULT_OK;

    DataVector3 vector3 = { { 1.0f, 0.5f, 0.25f } };
    DataVector3 out3;
    ok = ok && DataSetFieldVector3(store, id, 43, &vector3) == DATA_RESULT_OK &&
    DataGetFieldVector3(store, id, 43, &out3) == DATA_RESULT_OK && out3.m_Values[1] == 0.5f &&
    DataFieldIterSetVector3(&field, &vector3) == DATA_RESULT_OK &&
    DataFieldIterGetVector3(&field, &out3) == DATA_RESULT_OK && out3.m_Values[2] == 0.25f;

    ok = ok && DataFieldIterNext(&field) == DATA_RESULT_OK;

    DataVector4 vector4 = { { 1.0f, 0.5f, 0.25f, 0.75f } };
    DataVector4 out4;
    ok = ok && DataSetFieldVector4(store, id, 44, &vector4) == DATA_RESULT_OK &&
    DataGetFieldVector4(store, id, 44, &out4) == DATA_RESULT_OK && out4.m_Values[3] == 0.75f &&
    DataFieldIterSetVector4(&field, &vector4) == DATA_RESULT_OK &&
    DataFieldIterGetVector4(&field, &out4) == DATA_RESULT_OK && out4.m_Values[0] == 1.0f;

    ok = ok && DataFieldIterNext(&field) == DATA_RESULT_OK;

    DataMatrix4 matrix;
    DataMatrix4 out_matrix;
    for (uint32_t i = 0; i < 16; ++i)
        matrix.m_Values[i] = (float)i;
    matrix.m_Values[3 * 4 + 2] = 44.0f;
    ok = ok && DataSetFieldMatrix4(store, id, 45, &matrix) == DATA_RESULT_OK &&
    DataGetFieldMatrix4(store, id, 45, &out_matrix) == DATA_RESULT_OK && out_matrix.m_Values[14] == 44.0f &&
    DataFieldIterSetMatrix4(&field, &matrix) == DATA_RESULT_OK &&
    DataFieldIterGetMatrix4(&field, &out_matrix) == DATA_RESULT_OK && out_matrix.m_Values[0] == 0.0f;

    ok = ok && DataFieldIterNext(&field) == DATA_RESULT_OK;

    uint8_t boolean = 0;
    ok = ok && DataSetFieldBoolean(store, id, 47, 1) == DATA_RESULT_OK &&
    DataGetFieldBoolean(store, id, 47, &boolean) == DATA_RESULT_OK && boolean == 1 &&
    DataFieldIterSetBoolean(&field, 0) == DATA_RESULT_OK &&
    DataFieldIterGetBoolean(&field, &boolean) == DATA_RESULT_OK && boolean == 0;

    ok = ok && DataFieldIterNext(&field) == DATA_RESULT_OK;

    const char* string;
    ok = ok && DataSetFieldString(store, id, 48, "copied") == DATA_RESULT_OK &&
    DataGetFieldString(store, id, 48, &string) == DATA_RESULT_OK && string[0] == 'c' &&
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
        const DataVector3* (*read_vector)(const DataRowIterator*, uint32_t) = DataFieldGetVector3;
        DataRowIterator rows = DataIterRows(&it);
        while (DataRowIterNext(&rows) == DATA_RESULT_OK)
        {
            ok = ok && *DataFieldGetNumber(&rows, fields[2]) == 17.0 &&
            *DataFieldGetBoolean(&rows, fields[4]) == 0 &&
            read_vector(&rows, fields[1])->m_Values[2] == 3.0f &&
            DataFieldGetVector4(&rows, fields[3])->m_Values[3] == 4.0f &&
            DataFieldGetMatrix4(&rows, fields[0])->m_Values[15] == 15.0f;
            if (write)
            {
                *DataFieldGetNumberMut(&rows, fields[2]) = 29.0;
                *DataFieldGetBooleanMut(&rows, fields[4]) = 1;
                DataFieldGetVector3Mut(&rows, fields[1])->m_Values[2] = 30.0f;
                DataFieldGetVector4Mut(&rows, fields[3])->m_Values[3] = 40.0f;
                DataFieldGetMatrix4Mut(&rows, fields[0])->m_Values[15] = 150.0f;
                ok = ok && *DataFieldGetNumber(&rows, fields[2]) == 29.0 &&
                *DataFieldGetBoolean(&rows, fields[4]) == 1 &&
                read_vector(&rows, fields[1])->m_Values[2] == 30.0f &&
                DataFieldGetVector4(&rows, fields[3])->m_Values[3] == 40.0f &&
                DataFieldGetMatrix4(&rows, fields[0])->m_Values[15] == 150.0f;
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
