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
    DataStoreReference m_Faction;
    double             m_AlertRadius;
};

struct ReferenceEnemy
{
    DataStoreReference                m_Name;
    DataStoreReference                m_Attributes;
    DataStoreReference                m_Loot;
    struct ReferenceAttributes        m_State;
};

static void CountScheduledRowsFromC(HDataStoreQuery query, uint32_t first, uint32_t num_rows, void* context)
{
    (void)query;
    (void)first;
    *(uint32_t*)context += num_rows;
}

// Engine events must support static C descriptors and match their registered names.
int TestDataSchedulerFromC(void)
{
    static const DataStoreEventDesc events[] = {
        {
            .m_Name = DATASTORE_EVENT_GAMEOBJECT_PRE_UPDATE
        },
        {
            .m_Name = DATASTORE_EVENT_GAMEOBJECT_POST_UPDATE
        }
    };
    if (events[0].m_Name != dmHashString64("engine.gameobject.pre_update") ||
        events[1].m_Name != dmHashString64("engine.gameobject.post_update"))
    {
        return 1;
    }
    HDataStoreContext  store = DataStoreCreate(0);
    DataStoreQueryDesc query_desc = {
        0
    };
    HDataStoreQuery query;
    if (DataStoreQueryCreate(store, &query_desc, &query) != DATASTORE_RESULT_OK)
    {
        DataStoreDestroy(store);
        return 1;
    }
    uint32_t            count = 0;
    DataStoreSystemDesc desc  = {
        .m_Name = dmHashString64("game.count"),
        .m_DependsOn = {
            dmHashString64("engine.update.begin")
        },
        .m_Query = query,
        .m_Process = CountScheduledRowsFromC,
        .m_Context = &count,
        .m_RowsPerJob = 64
    };
    DataStoreEventDesc begin = {
        .m_Name = dmHashString64("engine.update.begin")
    };
    DataStoreEventDesc end = {
        .m_Name = dmHashString64("engine.update.end"), .m_DependsOn = {
            dmHashString64("game.count")
        }
    };
    struct JobSystemCreateParams params = {
        .m_ThreadNamePrefix = "ds-c", .m_ThreadCount = 0
    };
    HJobContext                  jobs = JobSystemCreate(&params);
    int                          ok = DataStoreFunctionAdd(store, &desc) == DATASTORE_RESULT_OK && count == 0;
    ok = DataStoreEventAdd(store, &begin) == DATASTORE_RESULT_OK && ok;
    ok = DataStoreEventAdd(store, &end) == DATASTORE_RESULT_OK && ok;
    ok = DataStoreSchedulerBegin(store, jobs) == DATASTORE_RESULT_OK && ok;
    ok = DataStoreSchedulerSync(store) == DATASTORE_RESULT_OK && ok;
    ok = DataStoreSchedulerEvent(store, end.m_Name) == DATASTORE_RESULT_BUSY && ok;
    ok = DataStoreSchedulerEvent(store, begin.m_Name) == DATASTORE_RESULT_OK && ok;
    ok = DataStoreSchedulerEnd(store) == DATASTORE_RESULT_BUSY && ok;
    ok = DataStoreSchedulerSync(store) == DATASTORE_RESULT_OK && ok;
    ok = DataStoreSchedulerEvent(store, end.m_Name) == DATASTORE_RESULT_OK && ok;
    ok = DataStoreSchedulerEnd(store) == DATASTORE_RESULT_OK && ok;
    JobSystemDestroy(jobs);
    ok = DataStoreEventRemove(store, end.m_Name) == DATASTORE_RESULT_OK && ok;
    ok = DataStoreEventRemove(store, begin.m_Name) == DATASTORE_RESULT_OK && ok;
    ok = DataStoreFunctionRemove(store, desc.m_Name) == DATASTORE_RESULT_OK && ok;
    ok = DataStoreQueryDestroy(query) == DATASTORE_RESULT_OK && ok;
    return DataStoreDestroy(store) == DATASTORE_RESULT_OK && ok ? 0 : 1;
}

int TestDataStoreLifecycleFromC(void)
{
    DataStoreCreateParams params = {
        .m_PageSize = 512 * 1024, .m_RowChunkSize = 64 * 1024
    };
    HDataStoreContext     store = DataStoreCreate(&params);
    DataStoreQueryDesc    desc = {
        0
    };
    HDataStoreQuery query;
    if (DataStoreQueryCreate(store, &desc, &query) != DATASTORE_RESULT_OK)
    {
        DataStoreDestroy(store);
        return 1;
    }
    if (DataStoreQueryTryBegin(query) != DATASTORE_RESULT_OK)
    {
        DataStoreQueryDestroy(query);
        DataStoreDestroy(store);
        return 1;
    }
    DataStoreResult locked = DataStoreDestroy(store);
    DataStoreQueryEnd(query);
    DataStoreQueryDestroy(query);
    DataStoreResult destroyed = DataStoreDestroy(store);
    return locked == DATASTORE_RESULT_LOCKED && destroyed == DATASTORE_RESULT_OK ? 0 : 1;
}

int TestDataQuerySetupFromC(void)
{
    HDataStoreContext  store = DataStoreCreate(0);
    DataStoreQueryDesc desc = {
        .m_Items = {
            DataStoreTag("boid"),
            DataStoreField("position", DATASTORE_TYPE_VECTOR3, DATASTORE_FIELD_MUTABLE),
            DataStoreField("velocity", DATASTORE_TYPE_VECTOR3, 0),
            DataStoreTag("school")
        }
    };
    HDataStoreQuery query;
    DataStoreResult result = DataStoreQueryCreate(store, &desc, &query);
    int             ok = result == DATASTORE_RESULT_OK;
    if (ok)
    {
        ok = DataStoreQueryFindField(query, &desc.m_Items[1]) != UINT32_MAX &&
             DataStoreQueryFindField(query, &desc.m_Items[2]) != UINT32_MAX &&
             DataStoreQueryFindField(query, &desc.m_Items[0]) == UINT32_MAX;
        ok = DataStoreQueryDestroy(query) == DATASTORE_RESULT_OK && ok;
    }
    return DataStoreDestroy(store) == DATASTORE_RESULT_OK && ok ? 0 : 1;
}

// Exercise every batch type through C, including aggregate errors and callbacks.
static int TestDataBatchFromC(HDataStoreContext store, DataStoreId id)
{
    DataStoreId ids[] = {
        id, id, id
    };
    double           numbers[3];
    uint8_t          booleans[3];
    const char*      strings[3];
    DataStoreVector3 vectors3[3];
    DataStoreVector4 vectors4[3];
    DataStoreMatrix4 matrices[3];
    DataStoreResult (*read_numbers)(HDataStoreContext, uint32_t, const DataStoreId*, uint64_t, double*) = DataStoreFieldGetNumberBatch;
    if (read_numbers(store, 3, ids, 42, numbers) != DATASTORE_RESULT_OK ||
        DataStoreFieldGetBooleanBatch(store, 3, ids, 47, booleans) != DATASTORE_RESULT_OK ||
        DataStoreFieldGetStringBatch(store, 3, ids, 48, strings) != DATASTORE_RESULT_OK ||
        DataStoreFieldGetVector3Batch(store, 3, ids, 43, vectors3) != DATASTORE_RESULT_OK ||
        DataStoreFieldGetVector4Batch(store, 3, ids, 44, vectors4) != DATASTORE_RESULT_OK ||
        DataStoreFieldGetMatrix4Batch(store, 3, ids, 45, matrices) != DATASTORE_RESULT_OK)
    {
        return 0;
    }
    for (uint32_t i = 0; i < 3; ++i)
    {
        if (numbers[i] != 29 || booleans[i] != 0 || strings[i][0] != 'o' || strings[i] != strings[0] ||
            vectors3[i].m_Values[2] != 0.25f || vectors4[i].m_Values[3] != 0.75f || matrices[i].m_Values[14] != 44)
        {
            return 0;
        }
    }

    ids[1] = 0;
    if (read_numbers(store, 3, ids, 42, numbers) != DATASTORE_RESULT_NOT_FOUND ||
        DataStoreFieldGetBooleanBatch(store, 3, ids, 47, booleans) != DATASTORE_RESULT_NOT_FOUND ||
        DataStoreFieldGetStringBatch(store, 3, ids, 48, strings) != DATASTORE_RESULT_NOT_FOUND ||
        DataStoreFieldGetVector3Batch(store, 3, ids, 43, vectors3) != DATASTORE_RESULT_NOT_FOUND ||
        DataStoreFieldGetVector4Batch(store, 3, ids, 44, vectors4) != DATASTORE_RESULT_NOT_FOUND ||
        DataStoreFieldGetMatrix4Batch(store, 3, ids, 45, matrices) != DATASTORE_RESULT_NOT_FOUND)
    {
        return 0;
    }

    return read_numbers(0, 0, 0, 42, 0) == DATASTORE_RESULT_OK &&
           DataStoreFieldGetBooleanBatch(0, 0, 0, 47, 0) == DATASTORE_RESULT_OK &&
           DataStoreFieldGetStringBatch(0, 0, 0, 48, 0) == DATASTORE_RESULT_OK &&
           DataStoreFieldGetVector3Batch(0, 0, 0, 43, 0) == DATASTORE_RESULT_OK &&
           DataStoreFieldGetVector4Batch(0, 0, 0, 44, 0) == DATASTORE_RESULT_OK &&
           DataStoreFieldGetMatrix4Batch(0, 0, 0, 45, 0) == DATASTORE_RESULT_OK;
}

int TestDataFromC(HDataStoreContext store, DataStoreId group)
{
    uint64_t           tag = 7;
    DataStoreQueryItem fields[] = {
        {
            .m_Hash = 42, .m_Type = DATASTORE_TYPE_NUMBER, .m_Access = DATASTORE_FIELD_MUTABLE, .m_Kind = DATASTORE_QUERY_ITEM_FIELD
        },
        {
            .m_Hash = 43, .m_Type = DATASTORE_TYPE_VECTOR3, .m_Kind = DATASTORE_QUERY_ITEM_FIELD
        },
        {
            .m_Hash = 44, .m_Type = DATASTORE_TYPE_VECTOR4, .m_Kind = DATASTORE_QUERY_ITEM_FIELD
        },
        {
            .m_Hash = 45, .m_Type = DATASTORE_TYPE_MATRIX4, .m_Kind = DATASTORE_QUERY_ITEM_FIELD
        },
        {
            .m_Hash = 47, .m_Type = DATASTORE_TYPE_BOOLEAN, .m_Kind = DATASTORE_QUERY_ITEM_FIELD
        },
        {
            .m_Hash = 48, .m_Type = DATASTORE_TYPE_STRING, .m_Kind = DATASTORE_QUERY_ITEM_FIELD
        },
        {
            .m_Hash = 46, .m_Type = DATASTORE_TYPE_STRUCT, .m_Kind = DATASTORE_QUERY_ITEM_FIELD
        }
    };
    DataStoreQueryDesc desc = {
        .m_Items = {
            fields[0],
            fields[1],
            fields[2],
            fields[3],
            fields[4],
            fields[5],
            fields[6],
            {
                .m_Hash = tag, .m_Kind = DATASTORE_QUERY_ITEM_TAG
            }
        },
        .m_GroupIds = &group,
        .m_GroupIdCount = 1
    };
    HDataStoreQuery query;
    if (DataStoreQueryCreate(store, &desc, &query) != DATASTORE_RESULT_OK)
    {
        return 1;
    }
    DataStoreFieldId number_field = DataStoreQueryFindField(query, &fields[0]);
    if (DataStoreQueryTryBegin(query) != DATASTORE_RESULT_OK)
    {
        DataStoreQueryDestroy(query);
        return 1;
    }
    DataStoreIterator    it   = DataStoreQueryIter(query);
    int                  ok   = DataStoreIterNext(&it);
    DataStoreRowIterator rows = DataStoreIterRows(&it);
    ok = ok && DataStoreRowIterNext(&rows) && DataStoreRowIterGetGroupId(&rows) == group;
    DataStoreId id     = DataStoreRowIterGetId(&rows);
    double      number = 0;
    ok = ok && *DataStoreRowIterGetNumber(&rows, number_field) == 17.0;
    *DataStoreRowIterGetNumberMut(&rows, number_field) = 29.0;
    ok = ok && *DataStoreRowIterGetNumber(&rows, number_field) == 29.0;
    ok = ok && !DataStoreRowIterNext(&rows) && !DataStoreIterNext(&it);
    DataStoreQueryEnd(query);
    ok = ok && DataStoreFieldSetNumber(store, id, 42, 23.0) == DATASTORE_RESULT_OK &&
         DataStoreFieldGetNumber(store, id, 42, &number) == DATASTORE_RESULT_OK && number == 23.0;
    ok = ok && DataStoreFieldSetNumber(store, id, 42, 29.0) == DATASTORE_RESULT_OK;

    DataStoreVector3 vector3 = {
        .m_Values = {
            1.0f, 0.5f, 0.25f
        }
    };
    DataStoreVector3 out3;
    ok = ok && DataStoreFieldSetVector3(store, id, 43, &vector3) == DATASTORE_RESULT_OK &&
         DataStoreFieldGetVector3(store, id, 43, &out3) == DATASTORE_RESULT_OK && out3.m_Values[1] == 0.5f;

    DataStoreVector4 vector4 = {
        .m_Values = {
            1.0f, 0.5f, 0.25f, 0.75f
        }
    };
    DataStoreVector4 out4;
    ok = ok && DataStoreFieldSetVector4(store, id, 44, &vector4) == DATASTORE_RESULT_OK &&
         DataStoreFieldGetVector4(store, id, 44, &out4) == DATASTORE_RESULT_OK && out4.m_Values[3] == 0.75f;

    DataStoreMatrix4 matrix;
    DataStoreMatrix4 out_matrix;
    for (uint32_t i = 0; i < 16; ++i)
    {
        matrix.m_Values[i] = (float)i;
    }
    matrix.m_Values[3 * 4 + 2] = 44.0f;
    ok = ok && DataStoreFieldSetMatrix4(store, id, 45, &matrix) == DATASTORE_RESULT_OK &&
         DataStoreFieldGetMatrix4(store, id, 45, &out_matrix) == DATASTORE_RESULT_OK && out_matrix.m_Values[14] == 44.0f;

    uint8_t boolean = 0;
    ok = ok && DataStoreFieldSetBoolean(store, id, 47, 1) == DATASTORE_RESULT_OK &&
         DataStoreFieldGetBoolean(store, id, 47, &boolean) == DATASTORE_RESULT_OK && boolean == 1 &&
         DataStoreFieldSetBoolean(store, id, 47, 0) == DATASTORE_RESULT_OK &&
         DataStoreFieldGetBoolean(store, id, 47, &boolean) == DATASTORE_RESULT_OK && boolean == 0;

    const char* string;
    ok = ok && DataStoreFieldSetString(store, id, 48, "copied") == DATASTORE_RESULT_OK &&
         DataStoreFieldGetString(store, id, 48, &string) == DATASTORE_RESULT_OK && string[0] == 'c' &&
         DataStoreFieldSetString(store, id, 48, "override") == DATASTORE_RESULT_OK &&
         DataStoreFieldGetString(store, id, 48, &string) == DATASTORE_RESULT_OK && string[0] == 'o';

    ok = ok && TestDataBatchFromC(store, id);
    DataStoreQueryDestroy(query);
    return ok ? 0 : 1;
}

int TestInlineDataFromC(HDataStoreContext store)
{
    DataStoreQueryItem requested_field = {
        .m_Hash = dmHashString64("2.40.10"), .m_Type = DATASTORE_TYPE_VECTOR3, .m_Kind = DATASTORE_QUERY_ITEM_FIELD
    };
    DataStoreQueryDesc desc = {
        .m_Items = {
            requested_field
        }
    };
    HDataStoreQuery query;
    if (DataStoreQueryCreate(store, &desc, &query) != DATASTORE_RESULT_OK)
    {
        return 1;
    }
    if (DataStoreQueryTryBegin(query) != DATASTORE_RESULT_OK)
    {
        DataStoreQueryDestroy(query);
        return 1;
    }
    DataStoreIterator    it   = DataStoreQueryIter(query);
    int                  ok   = DataStoreIterNext(&it);
    DataStoreRowIterator rows = DataStoreIterRows(&it);
    ok = ok && DataStoreRowIterNext(&rows);
    DataStoreFieldId color_field = DataStoreQueryFindField(query, &requested_field);
    ok = ok && DataStoreRowIterGetVector3(&rows, color_field)->m_Values[2] == 3.0f;
    DataStoreQueryEnd(query);
    DataStoreQueryDestroy(query);
    return ok ? 0 : 1;
}

// Bind once per query, then borrow native values through the public C API.
int TestDataPointersFromC(HDataStoreContext store, int write)
{
    DataStoreQueryItem query_fields[] = {
        {
            .m_Hash = 5, .m_Type = DATASTORE_TYPE_MATRIX4, .m_Kind = DATASTORE_QUERY_ITEM_FIELD
        },
        {
            .m_Hash = 3, .m_Type = DATASTORE_TYPE_VECTOR3, .m_Kind = DATASTORE_QUERY_ITEM_FIELD
        },
        {
            .m_Hash = 1, .m_Type = DATASTORE_TYPE_NUMBER, .m_Kind = DATASTORE_QUERY_ITEM_FIELD
        },
        {
            .m_Hash = 4, .m_Type = DATASTORE_TYPE_VECTOR4, .m_Kind = DATASTORE_QUERY_ITEM_FIELD
        },
        {
            .m_Hash = 2, .m_Type = DATASTORE_TYPE_BOOLEAN, .m_Kind = DATASTORE_QUERY_ITEM_FIELD
        }
    };
    for (uint32_t i = 0; i < 5; ++i)
    {
        query_fields[i].m_Access = write ? DATASTORE_FIELD_MUTABLE : 0;
    }
    DataStoreQueryDesc desc = {
        .m_Items = {
            query_fields[0], query_fields[1], query_fields[2], query_fields[3], query_fields[4]
        }
    };
    HDataStoreQuery query;
    if (DataStoreQueryCreate(store, &desc, &query) != DATASTORE_RESULT_OK)
    {
        return 1;
    }
    int              ok = 1;
    DataStoreFieldId fields[5];
    for (uint32_t i = 0; i < 5; ++i)
    {
        fields[i] = DataStoreQueryFindField(query, &query_fields[i]);
        ok = ok && fields[i] != UINT32_MAX;
    }
    DataStoreQueryItem wrong = {
        .m_Hash = 1, .m_Type = DATASTORE_TYPE_VECTOR3, .m_Kind = DATASTORE_QUERY_ITEM_FIELD
    };
    ok = ok && DataStoreQueryFindField(query, &wrong) == UINT32_MAX;
    wrong.m_Hash = 999;
    ok = ok && DataStoreQueryFindField(query, &wrong) == UINT32_MAX;
    wrong.m_Type = DATASTORE_TYPE_STRING;
    ok = ok && DataStoreQueryFindField(query, &wrong) == UINT32_MAX;
    if (DataStoreQueryTryBegin(query) != DATASTORE_RESULT_OK)
    {
        DataStoreQueryDestroy(query);
        return 1;
    }
    DataStoreIterator it    = DataStoreQueryIter(query);
    uint32_t          count = 0;
    while (ok && DataStoreIterNext(&it))
    {
        const DataStoreVector3* (*read_vector)(const DataStoreRowIterator*, uint32_t) = DataStoreRowIterGetVector3;
        DataStoreRowIterator rows = DataStoreIterRows(&it);
        while (DataStoreRowIterNext(&rows))
        {
            ok = ok && *DataStoreRowIterGetNumber(&rows, fields[2]) == 17.0 &&
                 *DataStoreRowIterGetBoolean(&rows, fields[4]) == 0 &&
                 read_vector(&rows, fields[1])->m_Values[2] == 3.0f &&
                 DataStoreRowIterGetVector4(&rows, fields[3])->m_Values[3] == 4.0f &&
                 DataStoreRowIterGetMatrix4(&rows, fields[0])->m_Values[15] == 15.0f;
            if (write)
            {
                *DataStoreRowIterGetNumberMut(&rows, fields[2]) = 29.0;
                *DataStoreRowIterGetBooleanMut(&rows, fields[4]) = 1;
                DataStoreRowIterGetVector3Mut(&rows, fields[1])->m_Values[2] = 30.0f;
                DataStoreRowIterGetVector4Mut(&rows, fields[3])->m_Values[3] = 40.0f;
                DataStoreRowIterGetMatrix4Mut(&rows, fields[0])->m_Values[15] = 150.0f;
                ok = ok && *DataStoreRowIterGetNumber(&rows, fields[2]) == 29.0 &&
                     *DataStoreRowIterGetBoolean(&rows, fields[4]) == 1 &&
                     read_vector(&rows, fields[1])->m_Values[2] == 30.0f &&
                     DataStoreRowIterGetVector4(&rows, fields[3])->m_Values[3] == 40.0f &&
                     DataStoreRowIterGetMatrix4(&rows, fields[0])->m_Values[15] == 150.0f;
            }
            ++count;
        }
    }
    ok = ok && DataStoreQueryFindField(query, &query_fields[0]) == fields[0] && count == 3;
    DataStoreQueryEnd(query);
    if (DataStoreQueryTryBegin(query) == DATASTORE_RESULT_OK)
    {
        DataStoreIterator range = DataStoreQueryIterRange(query, 0, DataStoreQueryGetRowCount(query));
        ok = ok && DataStoreQueryGetRowCount(query) == 3 && DataStoreIterNext(&range);
        DataStoreQueryEnd(query);
    }
    else
    {
        ok = 0;
    }
    DataStoreQueryDestroy(query);
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
        struct State        m_State;
        DataStoreVector3    m_Position;
    };
    DataStoreFieldDesc state_fields[] = {
        {
            .m_Field = 1, .m_Type = DATASTORE_TYPE_NUMBER, .m_Offset = offsetof(struct State, m_Health), .m_Name = "health"
        },
        {
            .m_Field = 2, .m_Type = DATASTORE_TYPE_BOOLEAN, .m_Offset = offsetof(struct State, m_Enabled), .m_Name = "enabled"
        }
    };
    DataStoreStructDesc state = {
        .m_Fields = state_fields, .m_FieldCount = 2, .m_Size = sizeof(struct State)
    };
    DataStoreFieldDesc fields[] = {
        {
            .m_Field = 3, .m_Type = DATASTORE_TYPE_STRUCT, .m_Offset = offsetof(struct Enemy, m_State), .m_Struct = &state, .m_Name = "state"
        },
        {
            .m_Field = 4, .m_Type = DATASTORE_TYPE_VECTOR3, .m_Offset = offsetof(struct Enemy, m_Position)
        }
    };
    uint64_t           tag   = 7;
    DataStoreTableDesc table = {
        .m_Type = 10, .m_Tags = &tag, .m_TagCount = 1, .m_Fields = fields, .m_FieldCount = 2, .m_RowStride = sizeof(struct Enemy), .m_ResetPolicy = DATASTORE_RESET_RETAIN
    };
    struct Enemy input[] = {
        {
            .m_State = {
                .m_Health = 50, .m_Enabled = 1
            }, .m_Position = {
                .m_Values = {
                    1, 2, 3
                }
            }
        },
        {
            .m_State = {
                .m_Health = 75
            }, .m_Position = {
                .m_Values = {
                    4, 5, 6
                }
            }
        }
    };
    DataStoreId        group = UINT64_C(0x1234567887654321);
    DataStoreId        ids[2], added[2];
    DataStoreQueryItem health = {
        .m_Hash = dmHashString64("state.health"), .m_Type = DATASTORE_TYPE_NUMBER, .m_Access = DATASTORE_FIELD_MUTABLE, .m_Kind = DATASTORE_QUERY_ITEM_FIELD
    };
    DataStoreQueryDesc query_desc = {
        .m_Items = {
            health, {
                .m_Hash = tag, .m_Kind = DATASTORE_QUERY_ITEM_TAG
            }
        },
        .m_GroupIds = &group,
        .m_GroupIdCount = 1,
        .m_Flags = DATASTORE_ROW_MUTABLE
    };
    HDataStoreContext store = DataStoreCreate(0);
    HDataStoreQuery   query = NULL;
    int               active = 0;
    int               error = 0;
    double            number = 0;
    DataStoreVector3  position;
#define CHECK_NATIVE(condition) \
    do \
    { \
        if (!(condition)) \
        { \
            error = __LINE__; \
            goto cleanup; \
        } \
    } while (0)
    CHECK_NATIVE(DataStoreTableAdd(store, &table) == DATASTORE_RESULT_OK);
    CHECK_NATIVE(DataStoreQueryCreate(store, &query_desc, &query) == DATASTORE_RESULT_OK);
    CHECK_NATIVE(DataStoreRowCreate(store, 10, group, 2, input, ids) == DATASTORE_RESULT_OK);
    CHECK_NATIVE(DataStoreFieldGetNumber(store, ids[1], health.m_Hash, &number) == DATASTORE_RESULT_OK && number == 75);
    CHECK_NATIVE(DataStoreFieldGetVector3(store, ids[1], 4, &position) == DATASTORE_RESULT_OK && position.m_Values[2] == 6);
    CHECK_NATIVE(DataStoreRowGetComponentId(store, ids[1]) == 0);

    // Prepare complete rows on the caller side, including nested fields.
    struct Enemy enemies[] = {
        input[0], input[0]
    };
    enemies[0].m_State.m_Health = 80;
    enemies[1].m_State.m_Health = 90;
    CHECK_NATIVE(DataStoreRowCreate(store, 10, 0, 2, enemies, added) == DATASTORE_RESULT_OK);
    CHECK_NATIVE(DataStoreFieldSetNumber(store, added[1], health.m_Hash, 5) == DATASTORE_RESULT_OK);
    CHECK_NATIVE(DataStoreRowReset(store, added[1]) == DATASTORE_RESULT_OK);
    CHECK_NATIVE(DataStoreFieldGetNumber(store, added[1], health.m_Hash, &number) == DATASTORE_RESULT_OK && number == 90);
    CHECK_NATIVE(DataStoreFieldGetVector3(store, added[1], 4, &position) == DATASTORE_RESULT_OK && position.m_Values[2] == 3);

    CHECK_NATIVE(DataStoreQueryTryBegin(query) == DATASTORE_RESULT_OK);
    active = 1;
    CHECK_NATIVE(DataStoreQueryGetRowCount(query) == 2);
    DataStoreIterator it = DataStoreQueryIterRange(query, 0, 2);
    CHECK_NATIVE(DataStoreIterNext(&it));
    DataStoreRowIterator rows = DataStoreIterRows(&it);
    CHECK_NATIVE(DataStoreRowIterNext(&rows) && DataStoreRowIterGetGroupId(&rows) == group);
    const struct Enemy* read_row = (const struct Enemy*)DataStoreRowIterGetData(&rows);
    CHECK_NATIVE(read_row->m_State.m_Health == 50);
    struct Enemy* write_row = (struct Enemy*)DataStoreRowIterGetDataMut(&rows);
    write_row->m_State.m_Health = 45;
    CHECK_NATIVE(DataStoreRowCreate(store, 10, 0, 2, enemies, added) == DATASTORE_RESULT_LOCKED);
    DataStoreQueryEnd(query);
    active = 0;

    CHECK_NATIVE(DataStoreFieldGetNumber(store, ids[0], health.m_Hash, &number) == DATASTORE_RESULT_OK && number == 45);
    CHECK_NATIVE(DataStoreRowReset(store, ids[0]) == DATASTORE_RESULT_OK);

    // Failed input leaves IDs and query membership intact.
    DataStoreId unchanged[] = {
        123, 456
    };
    enemies[1].m_State.m_Enabled = 2;
    CHECK_NATIVE(DataStoreRowCreate(store, 10, 0, 2, enemies, unchanged) == DATASTORE_RESULT_INVALID_ARGUMENT);
    enemies[1].m_State.m_Enabled = 1;
    CHECK_NATIVE(DataStoreRowCreate(store, 10, 0, 2, NULL, unchanged) == DATASTORE_RESULT_INVALID_ARGUMENT);
    CHECK_NATIVE(unchanged[0] == 123 && unchanged[1] == 456);
    CHECK_NATIVE(DataStoreRowCreate(store, 10, 0, 0, NULL, NULL) == DATASTORE_RESULT_OK);
    CHECK_NATIVE(DataStoreRowCreate(NULL, 999, 200, 0, NULL, NULL) == DATASTORE_RESULT_OK);

    CHECK_NATIVE(DataStoreRowRemove(store, ids[0]) == DATASTORE_RESULT_OK);
    CHECK_NATIVE(DataStoreFieldGetNumber(store, ids[0], health.m_Hash, &number) == DATASTORE_RESULT_NOT_FOUND);
    CHECK_NATIVE(DataStoreFieldGetNumber(store, added[1], health.m_Hash, &number) == DATASTORE_RESULT_OK && number == 90);
    // Caller-prepared defaults create independent rows, including group zero.
    enemies[1] = enemies[0];
    CHECK_NATIVE(DataStoreRowCreate(store, 10, 0, 2, enemies, added) == DATASTORE_RESULT_OK);
    CHECK_NATIVE(added[0] != ids[0]);
    CHECK_NATIVE(DataStoreFieldGetNumber(store, added[1], health.m_Hash, &number) == DATASTORE_RESULT_OK && number == 80);
    enemies[0].m_State.m_Health = -1;
    CHECK_NATIVE(DataStoreFieldSetNumber(store, added[0], health.m_Hash, 5) == DATASTORE_RESULT_OK);
    CHECK_NATIVE(DataStoreFieldGetNumber(store, added[1], health.m_Hash, &number) == DATASTORE_RESULT_OK && number == 80);
    CHECK_NATIVE(DataStoreRowReset(store, added[0]) == DATASTORE_RESULT_OK);
    CHECK_NATIVE(DataStoreFieldGetNumber(store, added[0], health.m_Hash, &number) == DATASTORE_RESULT_OK && number == 80);
    CHECK_NATIVE(DataStoreQueryTryBegin(query) == DATASTORE_RESULT_OK);
    active = 1;
    CHECK_NATIVE(DataStoreQueryGetRowCount(query) == 1);
    DataStoreQueryEnd(query);
    active = 0;
    CHECK_NATIVE(DataStoreGroupRemove(store, group) == DATASTORE_RESULT_OK);
    CHECK_NATIVE(DataStoreFieldGetNumber(store, ids[1], health.m_Hash, &number) == DATASTORE_RESULT_NOT_FOUND);
    CHECK_NATIVE(DataStoreGroupRemove(store, group) == DATASTORE_RESULT_NOT_FOUND);
    CHECK_NATIVE(DataStoreTableRemove(store, 10) == DATASTORE_RESULT_OK);
    CHECK_NATIVE(DataStoreFieldGetNumber(store, ids[1], health.m_Hash, &number) == DATASTORE_RESULT_NOT_FOUND);

    // A NULL native string is invalid; valid references are copied on insertion.
    DataStoreFieldDesc string = {
        .m_Field = 1, .m_Type = DATASTORE_TYPE_STRING
    };
    table.m_Fields = &string;
    table.m_FieldCount = 1;
    table.m_RowStride = 8;
    CHECK_NATIVE(DataStoreTableAdd(store, &table) == DATASTORE_RESULT_OK);
    DataStoreReference missing_strings[2] = {
        0
    };
    CHECK_NATIVE(DataStoreRowCreate(store, 10, 0, 2, missing_strings, unchanged) == DATASTORE_RESULT_INVALID_ARGUMENT);
cleanup:
    if (active)
    {
        DataStoreQueryEnd(query);
    }
    if (query)
    {
        DataStoreQueryDestroy(query);
    }
    DataStoreDestroy(store);
#undef CHECK_NATIVE
    return error;
}

// Field arrays use the public C API, including inline struct arrays and reset.
int TestDataSoARowsFromC(void)
{
    struct State
    {
        double           m_Health;
        uint8_t          m_Enabled;
        DataStoreVector3 m_Direction;
    };
    struct Enemy
    {
        uint8_t             m_Active;
        DataStoreVector3    m_Position;
        struct State        m_State;
        DataStoreVector4    m_Color;
        DataStoreMatrix4    m_Transform;
    };
    DataStoreFieldDesc state_fields[] = {
        {
            .m_Field = 1, .m_Type = DATASTORE_TYPE_NUMBER, .m_Offset = offsetof(struct State, m_Health), .m_Name = "health"
        },
        {
            .m_Field = 2, .m_Type = DATASTORE_TYPE_BOOLEAN, .m_Offset = offsetof(struct State, m_Enabled), .m_Name = "enabled"
        },
        {
            .m_Field = 3, .m_Type = DATASTORE_TYPE_VECTOR3, .m_Offset = offsetof(struct State, m_Direction), .m_Name = "direction"
        }
    };
    DataStoreStructDesc state = {
        .m_Fields = state_fields, .m_FieldCount = 3, .m_Size = sizeof(struct State)
    };
    DataStoreFieldDesc layout[] = {
        {
            .m_Field = 10, .m_Type = DATASTORE_TYPE_BOOLEAN, .m_Offset = offsetof(struct Enemy, m_Active)
        },
        {
            .m_Field = 20, .m_Type = DATASTORE_TYPE_VECTOR3, .m_Offset = offsetof(struct Enemy, m_Position)
        },
        {
            .m_Field = 30, .m_Type = DATASTORE_TYPE_STRUCT, .m_Offset = offsetof(struct Enemy, m_State), .m_Struct = &state, .m_Name = "state"
        },
        {
            .m_Field = 40, .m_Type = DATASTORE_TYPE_VECTOR4, .m_Offset = offsetof(struct Enemy, m_Color)
        },
        {
            .m_Field = 50, .m_Type = DATASTORE_TYPE_MATRIX4, .m_Offset = offsetof(struct Enemy, m_Transform)
        }
    };
    DataStoreTableDesc table = {
        .m_Type = 10, .m_Fields = layout, .m_FieldCount = 5, .m_RowStride = sizeof(struct Enemy), .m_ResetPolicy = DATASTORE_RESET_RETAIN
    };
    uint8_t active[] = {
        1, 0
    };
    DataStoreVector3 positions[] = { {
                                         .m_Values = {
                                             1, 2, 3
                                         }
                                     }, {
                                         .m_Values = {
                                             4, 5, 6
                                         }
                                     } };
    struct State states[] = {
        {
            .m_Health = 50, .m_Enabled = 1, .m_Direction = {
                .m_Values = {
                    1, 0, 0
                }
            }
        },
        {
            .m_Health = 75, .m_Enabled = 0, .m_Direction = {
                .m_Values = {
                    0, 1, 0
                }
            }
        }
    };
    DataStoreVector4 colors[] = { {
                                      .m_Values = {
                                          7, 8, 9, 10
                                      }
                                  }, {
                                      .m_Values = {
                                          11, 12, 13, 14
                                      }
                                  } };
    DataStoreMatrix4 transforms[2] = {
        0
    };
    for (uint32_t r = 0; r < 2; ++r)
    {
        for (uint32_t i = 0; i < 16; ++i)
        {
            transforms[r].m_Values[i] = (float)(r * 16 + i);
        }
    }
    // Input order is independent of layout and byte order.
    DataStoreFieldArray fields[] = {
        {
            .m_Field = 50, .m_Values = transforms
        },
        {
            .m_Field = 30, .m_Values = states
        },
        {
            .m_Field = 10, .m_Values = active
        },
        {
            .m_Field = 40, .m_Values = colors
        },
        {
            .m_Field = 20, .m_Values = positions
        }
    };
    DataStoreId ids[2] = {
        123, 456
    };
    DataStoreId        group = UINT64_C(0x1234567887654321);
    DataStoreQueryItem health = {
        .m_Hash = dmHashString64("state.health"), .m_Type = DATASTORE_TYPE_NUMBER, .m_Kind = DATASTORE_QUERY_ITEM_FIELD
    };
    DataStoreQueryDesc query_desc = {
        .m_Items = {
            health
        },
        .m_GroupIds = &group,
        .m_GroupIdCount = 1
    };
    HDataStoreContext store    = DataStoreCreate(0);
    HDataStoreQuery   query    = NULL;
    int               error    = 0;
    int               reserved = 0;
#define CHECK_SOA(condition) \
    do \
    { \
        if (!(condition)) \
        { \
            error = __LINE__; \
            goto cleanup; \
        } \
    } while (0)
    CHECK_SOA(DataStoreTableAdd(store, &table) == DATASTORE_RESULT_OK);
    CHECK_SOA(DataStoreQueryCreate(store, &query_desc, &query) == DATASTORE_RESULT_OK);
    CHECK_SOA(DataStoreRowCreateSoA(store, 999, group, 2, 5, fields, ids) == DATASTORE_RESULT_NOT_FOUND);
    CHECK_SOA(DataStoreRowCreateSoA(store, 10, group, 0, 0, NULL, NULL) == DATASTORE_RESULT_OK);
    CHECK_SOA(DataStoreRowCreateSoA(NULL, 999, group, 0, 5, NULL, NULL) == DATASTORE_RESULT_OK);
    CHECK_SOA(DataStoreRowCreateSoA(store, 10, group, UINT32_MAX, 5, fields, ids) == DATASTORE_RESULT_INVALID_ARGUMENT);
    CHECK_SOA(DataStoreRowCreateSoA(store, 10, group, 2, 4, fields, ids) == DATASTORE_RESULT_INVALID_ARGUMENT);
    CHECK_SOA(DataStoreRowCreateSoA(store, 10, group, 2, 5, NULL, ids) == DATASTORE_RESULT_INVALID_ARGUMENT);
    fields[4].m_Field = 40;
    CHECK_SOA(DataStoreRowCreateSoA(store, 10, group, 2, 5, fields, ids) == DATASTORE_RESULT_INVALID_ARGUMENT);
    fields[4].m_Field = 999;
    CHECK_SOA(DataStoreRowCreateSoA(store, 10, group, 2, 5, fields, ids) == DATASTORE_RESULT_INVALID_ARGUMENT);
    fields[4].m_Field = health.m_Hash;
    CHECK_SOA(DataStoreRowCreateSoA(store, 10, group, 2, 5, fields, ids) == DATASTORE_RESULT_INVALID_ARGUMENT);
    fields[4].m_Field = 20;
    fields[4].m_Values = NULL;
    CHECK_SOA(DataStoreRowCreateSoA(store, 10, group, 2, 5, fields, ids) == DATASTORE_RESULT_INVALID_ARGUMENT);
    fields[4].m_Values = positions;
    active[1] = 2;
    CHECK_SOA(DataStoreRowCreateSoA(store, 10, group, 2, 5, fields, ids) == DATASTORE_RESULT_INVALID_ARGUMENT);
    active[1] = 0;
    states[1].m_Enabled = 2;
    CHECK_SOA(DataStoreRowCreateSoA(store, 10, group, 2, 5, fields, ids) == DATASTORE_RESULT_INVALID_ARGUMENT);
    states[1].m_Enabled = 0;
    CHECK_SOA(ids[0] == 123 && ids[1] == 456);

    CHECK_SOA(DataStoreRowCreateSoA(store, 10, group, 2, 5, fields, ids) == DATASTORE_RESULT_OK);
    for (uint32_t i = 0; i < 2; ++i)
    {
        double           number;
        uint8_t          boolean;
        DataStoreVector3 position;
        DataStoreVector4 color;
        DataStoreMatrix4 transform;
        CHECK_SOA(DataStoreFieldGetNumber(store, ids[i], health.m_Hash, &number) == DATASTORE_RESULT_OK && number == states[i].m_Health);
        CHECK_SOA(DataStoreFieldGetBoolean(store, ids[i], 10, &boolean) == DATASTORE_RESULT_OK && boolean == active[i]);
        CHECK_SOA(DataStoreFieldGetBoolean(store, ids[i], dmHashString64("state.enabled"), &boolean) == DATASTORE_RESULT_OK && boolean == states[i].m_Enabled);
        CHECK_SOA(DataStoreFieldGetVector3(store, ids[i], 20, &position) == DATASTORE_RESULT_OK && position.m_Values[2] == positions[i].m_Values[2]);
        CHECK_SOA(DataStoreFieldGetVector4(store, ids[i], 40, &color) == DATASTORE_RESULT_OK && color.m_Values[3] == colors[i].m_Values[3]);
        CHECK_SOA(DataStoreFieldGetMatrix4(store, ids[i], 50, &transform) == DATASTORE_RESULT_OK && transform.m_Values[15] == transforms[i].m_Values[15]);
        CHECK_SOA(DataStoreFieldGetVector3(store, ids[i], dmHashString64("state.direction"), &position) == DATASTORE_RESULT_OK && position.m_Values[i] == 1);
        CHECK_SOA(DataStoreRowGetComponentId(store, ids[i]) == 0);
    }
    states[1].m_Health = -1;
    positions[1].m_Values[2] = -1;
    CHECK_SOA(DataStoreFieldSetNumber(store, ids[1], health.m_Hash, 5) == DATASTORE_RESULT_OK);
    CHECK_SOA(DataStoreFieldSetVector3(store, ids[1], 20, &positions[1]) == DATASTORE_RESULT_OK);
    CHECK_SOA(DataStoreRowReset(store, ids[1]) == DATASTORE_RESULT_OK);
    double           number;
    DataStoreVector3 position;
    CHECK_SOA(DataStoreFieldGetNumber(store, ids[1], health.m_Hash, &number) == DATASTORE_RESULT_OK && number == 75);
    CHECK_SOA(DataStoreFieldGetVector3(store, ids[1], 20, &position) == DATASTORE_RESULT_OK && position.m_Values[2] == 6);

    CHECK_SOA(DataStoreQueryTryBegin(query) == DATASTORE_RESULT_OK);
    reserved = 1;
    CHECK_SOA(DataStoreQueryGetRowCount(query) == 2);
    DataStoreIterator it = DataStoreQueryIterRange(query, 0, 2);
    CHECK_SOA(DataStoreIterNext(&it));
    DataStoreRowIterator rows = DataStoreIterRows(&it);
    CHECK_SOA(DataStoreRowIterNext(&rows) && DataStoreRowIterGetGroupId(&rows) == group);
    CHECK_SOA(DataStoreRowCreateSoA(store, 10, group, 2, 5, fields, ids) == DATASTORE_RESULT_LOCKED);
    DataStoreQueryEnd(query);
    reserved = 0;
    DataStoreId removed = ids[0];
    CHECK_SOA(DataStoreRowRemove(store, removed) == DATASTORE_RESULT_OK);
    CHECK_SOA(DataStoreRowCreateSoA(store, 10, 0, 2, 5, fields, ids) == DATASTORE_RESULT_OK);
    CHECK_SOA(ids[0] != removed);
    CHECK_SOA(DataStoreFieldGetNumber(store, removed, health.m_Hash, &number) == DATASTORE_RESULT_NOT_FOUND);
    CHECK_SOA(DataStoreQueryTryBegin(query) == DATASTORE_RESULT_OK);
    reserved = 1;
    CHECK_SOA(DataStoreQueryGetRowCount(query) == 1);
    DataStoreQueryEnd(query);
    reserved = 0;

    DataStoreTableDesc empty = {
        .m_Type = 11
    };
    CHECK_SOA(DataStoreTableAdd(store, &empty) == DATASTORE_RESULT_OK);
    CHECK_SOA(DataStoreRowCreateSoA(store, 11, group, 2, 0, NULL, ids) == DATASTORE_RESULT_OK);
    CHECK_SOA(DataStoreRowRemove(store, ids[1]) == DATASTORE_RESULT_OK);
    DataStoreStructDesc zero_struct = {
        0
    };
    DataStoreFieldDesc zero_field = {
        .m_Field = 1, .m_Type = DATASTORE_TYPE_STRUCT, .m_Struct = &zero_struct, .m_Name = "empty"
    };
    DataStoreTableDesc zero_table = {
        .m_Type = 13, .m_Fields = &zero_field, .m_FieldCount = 1
    };
    DataStoreFieldArray zero_values = {
        .m_Field = 1
    };
    CHECK_SOA(DataStoreTableAdd(store, &zero_table) == DATASTORE_RESULT_OK);
    CHECK_SOA(DataStoreRowCreateSoA(store, 13, 0, 2, 1, &zero_values, ids) == DATASTORE_RESULT_OK);
    CHECK_SOA(DataStoreRowRemove(store, ids[0]) == DATASTORE_RESULT_OK);
    DataStoreFieldDesc string = {
        .m_Field = 1, .m_Type = DATASTORE_TYPE_STRING
    };
    DataStoreTableDesc references = {
        .m_Type = 12, .m_Fields = &string, .m_FieldCount = 1, .m_RowStride = 8
    };
    CHECK_SOA(DataStoreTableAdd(store, &references) == DATASTORE_RESULT_OK);
    CHECK_SOA(DataStoreRowCreateSoA(store, 12, group, 2, 1, fields, ids) == DATASTORE_RESULT_INVALID_ARGUMENT);
cleanup:
    if (reserved)
    {
        DataStoreQueryEnd(query);
    }
    if (query)
    {
        DataStoreQueryDestroy(query);
    }
    DataStoreDestroy(store);
#undef CHECK_SOA
    return error;
}

// References are constructed through the public API from stack-owned input.
int CreateReferenceRowsFromC(HDataStoreContext store, int soa, DataStoreId ids[2])
{
    char               name[] = "Guard";
    char               faction[] = "guards";
    char               loot_name[] = "coin";
    DataStoreFieldDesc members[] = {
        {
            .m_Field = 1, .m_Type = DATASTORE_TYPE_STRING, .m_Offset = offsetof(struct ReferenceAttributes, m_Faction), .m_Name = "faction"
        },
        {
            .m_Field = 2, .m_Type = DATASTORE_TYPE_NUMBER, .m_Offset = offsetof(struct ReferenceAttributes, m_AlertRadius), .m_Name = "alert_radius"
        }
    };
    DataStoreStructDesc layout = {
        .m_Fields = members, .m_FieldCount = 2, .m_Size = sizeof(struct ReferenceAttributes)
    };
    struct ReferenceAttributes attributes[] = {
        {
            .m_Faction = {
                .m_String = faction
            }, .m_AlertRadius = 15.0
        },
        {
            .m_Faction = {
                .m_String = "bandits"
            }, .m_AlertRadius = 25.0
        }
    };
    DataStoreStructInput objects[] = {
        {
            .m_Layout = &layout, .m_Values = &attributes[0]
        },
        {
            .m_Layout = &layout, .m_Values = &attributes[1]
        }
    };
    DataStoreReference strings[] = { {
                                         .m_String = loot_name
                                     }, {
                                         .m_String = "health_pickup"
                                     } };
    DataStoreReference structures[] = { {
                                            .m_Struct = &objects[0]
                                        }, {
                                            .m_Struct = &objects[1]
                                        } };
    DataStoreListInput lists[] = {
        {
            .m_Type = DATASTORE_TYPE_STRING, .m_Count = 2, .m_Values = strings
        },
        {
            .m_Type = DATASTORE_TYPE_STRUCT, .m_Count = 2, .m_Values = structures
        }
    };
    DataStoreReference names[] = { {
                                       .m_String = name
                                   }, {
                                       .m_String = "Bandit"
                                   } };
    DataStoreReference loot[] = { {
                                      .m_List = &lists[0]
                                  }, {
                                      .m_List = &lists[1]
                                  } };
    DataStoreFieldDesc fields[] = {
        {
            .m_Field = 10, .m_Type = DATASTORE_TYPE_STRING, .m_Offset = offsetof(struct ReferenceEnemy, m_Name)
        },
        {
            .m_Field = 11, .m_Type = DATASTORE_TYPE_STRUCT, .m_Offset = offsetof(struct ReferenceEnemy, m_Attributes)
        },
        {
            .m_Field = 12, .m_Type = DATASTORE_TYPE_LIST, .m_Offset = offsetof(struct ReferenceEnemy, m_Loot)
        },
        {
            .m_Field = 13, .m_Type = DATASTORE_TYPE_STRUCT, .m_Offset = offsetof(struct ReferenceEnemy, m_State), .m_Struct = &layout, .m_Name = "state"
        }
    };
    DataStoreTableDesc table = {
        .m_Type = 93, .m_Fields = fields, .m_FieldCount = 4, .m_RowStride = sizeof(struct ReferenceEnemy), .m_ResetPolicy = DATASTORE_RESET_RETAIN
    };
    if (DataStoreTableAdd(store, &table) != DATASTORE_RESULT_OK)
    {
        return 1;
    }

    DataStoreResult result;
    if (soa)
    {
        // Supply columns out of metadata order, including complete inline structs.
        DataStoreFieldArray columns[] = {
            {
                .m_Field = 12, .m_Values = loot
            },
            {
                .m_Field = 13, .m_Values = attributes
            },
            {
                .m_Field = 10, .m_Values = names
            },
            {
                .m_Field = 11, .m_Values = structures
            }
        };
        result = DataStoreRowCreateSoA(store, 93, 0, 2, 4, columns, ids);
    }
    else
    {
        struct ReferenceEnemy enemies[] = {
            {
                .m_Name = names[0], .m_Attributes = structures[0], .m_Loot = loot[0], .m_State = attributes[0]
            },
            {
                .m_Name = names[1], .m_Attributes = structures[1], .m_Loot = loot[1], .m_State = attributes[1]
            }
        };
        result = DataStoreRowCreate(store, 93, 0, 2, enemies, ids);
    }
    if (result != DATASTORE_RESULT_OK)
    {
        return 2;
    }

    // Neither payloads nor descriptors may be retained from caller-owned storage.
    name[0] = faction[0] = loot_name[0] = 'X';
    attributes[0].m_AlertRadius = -1;
    const char* stored_name;
    if (DataStoreFieldGetString(store, ids[0], 10, &stored_name) != DATASTORE_RESULT_OK || strcmp(stored_name, "Guard"))
    {
        return 3;
    }
    if (DataStoreFieldGetString(store, ids[0], dmHashString64("state.faction"), &stored_name) != DATASTORE_RESULT_OK || strcmp(stored_name, "guards"))
    {
        return 4;
    }
    if (DataStoreFieldSetString(store, ids[0], 10, "Captain") != DATASTORE_RESULT_OK || DataStoreRowReset(store, ids[0]) != DATASTORE_RESULT_OK)
    {
        return 5;
    }
    return DataStoreFieldGetString(store, ids[0], 10, &stored_name) == DATASTORE_RESULT_OK && strcmp(stored_name, "Guard") == 0 ? 0 : 6;
}

int CreateMixedListRowsFromC(HDataStoreContext store, int soa, DataStoreId ids[2])
{
    double           number = 100.0;
    char             string[] = "coin";
    uint8_t          boolean = 1;
    DataStoreVector3 vector3 = {
        .m_Values = {
            1, 2, 3
        }
    };
    DataStoreVector4 vector4 = {
        .m_Values = {
            4, 5, 6, 7
        }
    };
    DataStoreMatrix4 matrix = {
        .m_Values = {
            1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1
        }
    };
    DataStoreFieldDesc member = {
        .m_Field = 42, .m_Type = DATASTORE_TYPE_NUMBER
    };
    DataStoreStructDesc layout = {
        .m_Fields = &member, .m_FieldCount = 1, .m_Size = sizeof(number)
    };
    DataStoreStructInput object = {
        .m_Layout = &layout, .m_Values = &number
    };
    double numbers[] = {
        1, 2
    };
    DataStoreListInput nested = {
        .m_Type = DATASTORE_TYPE_NUMBER, .m_Count = 2, .m_Values = numbers
    };
    DataStoreValueType types[] = {
        DATASTORE_TYPE_NUMBER, DATASTORE_TYPE_STRING, DATASTORE_TYPE_BOOLEAN, DATASTORE_TYPE_STRUCT, DATASTORE_TYPE_LIST, DATASTORE_TYPE_NULL, DATASTORE_TYPE_VECTOR3, DATASTORE_TYPE_VECTOR4, DATASTORE_TYPE_MATRIX4
    };
    const void* values[] = {
        &number, string, &boolean, &object, &nested, NULL, &vector3, &vector4, &matrix
    };
    // m_Type is ignored when per-element kinds are supplied.
    DataStoreListInput mixed = {
        .m_Type = (DataStoreValueType)UINT32_MAX, .m_Count = 9, .m_Values = values, .m_Types = types
    };
    DataStoreReference child = {
        .m_List = &mixed
    };
    DataStoreListInput parent = {
        .m_Type = DATASTORE_TYPE_LIST, .m_Count = 1, .m_Values = &child
    };
    DataStoreReference rows[] = { {
                                      .m_List = &mixed
                                  }, {
                                      .m_List = &parent
                                  } };
    DataStoreFieldDesc field = {
        .m_Field = 1, .m_Type = DATASTORE_TYPE_LIST
    };
    DataStoreTableDesc table = {
        .m_Type = 94, .m_Fields = &field, .m_FieldCount = 1, .m_RowStride = sizeof(DataStoreReference), .m_ResetPolicy = DATASTORE_RESET_RETAIN
    };
    if (DataStoreTableAdd(store, &table) != DATASTORE_RESULT_OK)
    {
        return 1;
    }
    DataStoreResult result;
    if (soa)
    {
        DataStoreFieldArray column = {
            .m_Field = 1, .m_Values = rows
        };
        result = DataStoreRowCreateSoA(store, 94, 0, 2, 1, &column, ids);
    }
    else
    {
        result = DataStoreRowCreate(store, 94, 0, 2, rows, ids);
    }

    number = -1;
    string[0] = 'X';
    boolean = 0;
    numbers[0] = -1;
    vector3.m_Values[2] = -1;
    vector4.m_Values[3] = -1;
    matrix.m_Values[15] = -1;
    types[0] = DATASTORE_TYPE_NULL;
    values[1] = NULL;
    member.m_Field = 0;
    if (result != DATASTORE_RESULT_OK)
    {
        return 2;
    }
    DataStoreValueView list, item;
    uint32_t           count;
    if (DataStoreFieldGetView(store, ids[0], 1, &list) != DATASTORE_RESULT_OK ||
        DataStoreViewGetCount(&list, &count) != DATASTORE_RESULT_OK || count != 9)
    {
        return 3;
    }
    if (DataStoreViewGetElement(&list, 6, &item) != DATASTORE_RESULT_OK ||
        DataStoreViewGetVector3(&item, &vector3) != DATASTORE_RESULT_OK || vector3.m_Values[2] != 3)
    {
        return 4;
    }
    if (DataStoreViewGetElement(&list, 7, &item) != DATASTORE_RESULT_OK ||
        DataStoreViewGetVector4(&item, &vector4) != DATASTORE_RESULT_OK || vector4.m_Values[3] != 7)
    {
        return 5;
    }
    if (DataStoreViewGetElement(&list, 8, &item) != DATASTORE_RESULT_OK ||
        DataStoreViewGetMatrix4(&item, &matrix) != DATASTORE_RESULT_OK || matrix.m_Values[15] != 1)
    {
        return 6;
    }
    if (DataStoreViewGetElement(&list, 2, &item) != DATASTORE_RESULT_OK ||
        DataStoreViewGetBoolean(&item, &boolean) != DATASTORE_RESULT_OK || boolean != 1)
    {
        return 7;
    }
    types[0] = DATASTORE_TYPE_NUMBER;
    DataStoreViewIterator iterator = DataStoreViewIter(&list);
    uint32_t              seen     = 0;
    while (DataStoreViewIterNext(&iterator, &item))
    {
        if (seen >= 9 || DataStoreViewGetType(&item) != types[seen])
        {
            return 8;
        }
        ++seen;
    }
    if (seen != 9 || DataStoreViewIterNext(&iterator, &item) || DataStoreViewGetType(&item) != DATASTORE_TYPE_MATRIX4)
    {
        return 9;
    }
    return 0;
}
