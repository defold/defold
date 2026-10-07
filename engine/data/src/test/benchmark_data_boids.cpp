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

#include "benchmark_data_boids.h"
#include <math.h>

#ifdef DATA_BENCHMARK_ENTT
#include "benchmark_data_entt.h"
#endif

static const Vector3 TARGETS[] = { { { 38, 27, 35 } }, { { -42, 29, -32 } } };

static Vector3       Add(Vector3 a, Vector3 b)
{
    return Vector3 { { a.m_Values[0] + b.m_Values[0], a.m_Values[1] + b.m_Values[1], a.m_Values[2] + b.m_Values[2] } };
}

static Vector3 Mul(Vector3 a, float scale)
{
    return Vector3 { { a.m_Values[0] * scale, a.m_Values[1] * scale, a.m_Values[2] * scale } };
}

static Vector3 Sub(Vector3 a, Vector3 b)
{
    return Add(a, Mul(b, -1));
}

static float LengthSquared(Vector3 a)
{
    return a.m_Values[0] * a.m_Values[0] + a.m_Values[1] * a.m_Values[1] + a.m_Values[2] * a.m_Values[2];
}

static Vector3 Normalize(Vector3 a, Vector3 fallback)
{
    float length = LengthSquared(a);
    return length > 1e-12f ? Mul(a, 1.0f / sqrtf(length)) : fallback;
}

BoidsScratch CreateBoidsScratch(uint32_t num_rows)
{
    uint32_t slots = 1;
    while (slots < num_rows * 2)
        slots *= 2;
    return BoidsScratch { .m_Snapshot = new Boid[num_rows](), .m_Cells = new BoidCell[num_rows](), .m_RowCells = new uint32_t[num_rows](), .m_Slots = new uint32_t[slots](), .m_Count = num_rows, .m_SlotCount = slots };
}

void DestroyBoidsScratch(BoidsScratch* scratch)
{
    delete[] scratch->m_Snapshot;
    delete[] scratch->m_Cells;
    delete[] scratch->m_RowCells;
    delete[] scratch->m_Slots;
}

void BuildBoidCells(BoidsScratch* scratch)
{
    memset(scratch->m_Slots, 0, scratch->m_SlotCount * sizeof(uint32_t));
    scratch->m_CellCount = 0;
    uint32_t mask = scratch->m_SlotCount - 1;
    for (uint32_t i = 0; i < scratch->m_Count; ++i)
    {
        const Boid& row = scratch->m_Snapshot[i];
        int32_t     x = (int32_t)floorf(row.m_Position.m_Values[0] * 0.125f);
        int32_t     y = (int32_t)floorf(row.m_Position.m_Values[1] * 0.125f);
        int32_t     z = (int32_t)floorf(row.m_Position.m_Values[2] * 0.125f);
        uint32_t    slot = ((uint32_t)x * 73856093u ^ (uint32_t)y * 19349663u ^ (uint32_t)z * 83492791u) & mask;
        while (scratch->m_Slots[slot])
        {
            const BoidCell& cell = scratch->m_Cells[scratch->m_Slots[slot] - 1];
            if (cell.m_X == x && cell.m_Y == y && cell.m_Z == z)
                break;
            slot = (slot + 1) & mask;
        }
        if (!scratch->m_Slots[slot])
        {
            BoidCell cell = { .m_X = x, .m_Y = y, .m_Z = z };
            scratch->m_Cells[scratch->m_CellCount++] = cell;
            scratch->m_Slots[slot] = scratch->m_CellCount;
        }
        uint32_t  index = scratch->m_Slots[slot] - 1;
        BoidCell* cell = &scratch->m_Cells[index];
        ++cell->m_Count;
        cell->m_PositionSum = Add(cell->m_PositionSum, row.m_Position);
        cell->m_VelocitySum = Add(cell->m_VelocitySum, row.m_Velocity);
        scratch->m_RowCells[i] = index;
    }
    for (uint32_t i = 0; i < scratch->m_CellCount; ++i)
    {
        BoidCell* cell = &scratch->m_Cells[i];
        Vector3   center = Mul(cell->m_PositionSum, 1.0f / cell->m_Count);
        cell->m_Target = LengthSquared(Sub(TARGETS[0], center)) <= LengthSquared(Sub(TARGETS[1], center)) ? 0 : 1;
        cell->m_Avoid = LengthSquared(center) < 900.0f;
    }
}

Boid SteerBoid(const Boid& row, const BoidCell& cell)
{
    const Vector3 zero = {}, forward = { { 0, 0, 1 } };
    Vector3       heading = Normalize(row.m_Velocity, forward);
    Vector3       alignment = Normalize(Sub(Mul(cell.m_VelocitySum, 1.0f / cell.m_Count), row.m_Velocity), zero);
    Vector3       separation = Normalize(Sub(Mul(row.m_Position, (float)cell.m_Count), cell.m_PositionSum), zero);
    Vector3       attraction = Mul(Normalize(Sub(TARGETS[cell.m_Target], row.m_Position), zero), 2.0f);
    Vector3       desired = Normalize(Add(Add(alignment, separation), attraction), heading);
    if (cell.m_Avoid)
    {
        Vector3 away = Normalize(row.m_Position, heading);
        desired = Normalize(Sub(Mul(away, 30.0f), row.m_Position), away);
    }
    heading = Normalize(Add(heading, Mul(Sub(desired, heading), 0.015625f)), heading);
    Vector3 velocity = Mul(heading, 25.0f);
    return Boid { .m_Position = Add(row.m_Position, Mul(velocity, 0.015625f)), .m_Velocity = velocity };
}

double BoidChecksum(const Boid& row)
{
    double sum = 0;
    for (uint32_t axis = 0; axis < 3; ++axis)
        sum += (double)row.m_Position.m_Values[axis] * row.m_Position.m_Values[axis] + (double)row.m_Velocity.m_Values[axis] * row.m_Velocity.m_Values[axis];
    return sum;
}

BoidsScratch CreateBoidsReference(const TypeInput* type)
{
    BoidsScratch   reference = CreateBoidsScratch(type->m_Count);
    const Vector3* positions = (const Vector3*)type->m_Columns[FindField(type, POSITION)];
    const Vector3* velocities = (const Vector3*)type->m_Columns[FindField(type, VELOCITY)];
    for (uint32_t r = 0; r < type->m_Count; ++r)
        reference.m_Snapshot[r] = Boid { .m_Position = positions[r], .m_Velocity = velocities[r] };
    BuildBoidCells(&reference);
    return reference;
}

void ValidateBoid(const Boid& actual, const Boid& expected)
{
    for (uint32_t axis = 0; axis < 3; ++axis)
    {
        Check(isfinite(actual.m_Position.m_Values[axis]) && isfinite(actual.m_Velocity.m_Values[axis]), "finite boid");
        Check(fabsf(actual.m_Position.m_Values[axis] - expected.m_Position.m_Values[axis]) < 1e-4f && fabsf(actual.m_Velocity.m_Values[axis] - expected.m_Velocity.m_Values[axis]) < 1e-4f, "persisted boid position/velocity");
    }
    Check(fabsf(LengthSquared(actual.m_Velocity) - 625.0f) < 0.001f, "boid speed");
}

// Check cell grouping against an independent all-pairs scan, including negative
// coordinates, deliberate hash collisions, empty input and coincident agents.
void ValidateBoidsCells()
{
    BoidsScratch scratch = CreateBoidsScratch(0);
    BuildBoidCells(&scratch);
    Check(!scratch.m_CellCount, "empty flock");
    DestroyBoidsScratch(&scratch);
    scratch = CreateBoidsScratch(8);
    const float positions[8] = { 0, 0, -1, -8, 8, 128, 256, 384 };
    for (uint32_t i = 0; i < 8; ++i)
        scratch.m_Snapshot[i] = Boid { .m_Position = { { positions[i], 0, 0 } }, .m_Velocity = { { (float)(i % 3), 0, 1 } } };
    BuildBoidCells(&scratch);
    for (uint32_t i = 0; i < 8; ++i)
    {
        BoidCell expected = {};
        for (uint32_t j = 0; j < 8; ++j)
        {
            if (floorf(positions[i] / 8) != floorf(positions[j] / 8))
                continue;
            ++expected.m_Count;
            expected.m_PositionSum = Add(expected.m_PositionSum, scratch.m_Snapshot[j].m_Position);
            expected.m_VelocitySum = Add(expected.m_VelocitySum, scratch.m_Snapshot[j].m_Velocity);
        }
        const BoidCell& cell = scratch.m_Cells[scratch.m_RowCells[i]];
        Check(cell.m_Count == expected.m_Count && !memcmp(&cell.m_PositionSum, &expected.m_PositionSum, sizeof(Vector3)) && !memcmp(&cell.m_VelocitySum, &expected.m_VelocitySum, sizeof(Vector3)), "flock cell aggregation");
        Boid result = SteerBoid(scratch.m_Snapshot[i], cell);
        ValidateBoid(result, result);
    }
    Check(scratch.m_Cells[scratch.m_RowCells[0]].m_Avoid, "obstacle avoidance selected");
    Boid     stationary = {};
    BoidCell obstacle = { .m_Count = 1, .m_Avoid = true };
    Boid     forward = { .m_Position = { { 0, 0, 25.0f / 64.0f } }, .m_Velocity = { { 0, 0, 25 } } };
    ValidateBoid(SteerBoid(stationary, obstacle), forward);
    DestroyBoidsScratch(&scratch);
}

Query CreateBoidsQuery(Backend* store, uint32_t flock)
{
    Query    out = {};
    uint64_t tag = dmHashString64(flock ? "enemy" : "player");
    if (!store->m_Kind)
    {
        DataQueryField fields[] = {
            { .m_Field = g_Fields[POSITION], .m_Type = DATA_TYPE_VECTOR3, .m_Access = DATA_ACCESS_READ_WRITE },
            { .m_Field = g_Fields[VELOCITY], .m_Type = DATA_TYPE_VECTOR3, .m_Access = DATA_ACCESS_READ_WRITE }
        };
        DataQueryDesc desc = { .m_AllTags = &tag, .m_AllTagCount = 1, .m_Fields = fields, .m_FieldCount = 2 };
        Check(DataCreateQuery(store->m_Data, &desc, &out.m_Data) == DATA_RESULT_OK, "boids query");
        out.m_PositionField = DataQueryFindField(out.m_Data, &fields[0]);
        out.m_VelocityField = DataQueryFindField(out.m_Data, &fields[1]);
    }
    else
    {
        ecs_query_desc_t desc = {
            .terms = { { .id = store->m_Fields[POSITION], .inout = EcsInOut }, { .id = store->m_Fields[VELOCITY], .inout = EcsInOut }, { .id = Tag(store->m_World, tag), .inout = EcsInOutNone } },
            .cache_kind = EcsQueryCacheAuto
        };
        out.m_Flecs[0] = ecs_query_init(store->m_World, &desc);
        out.m_Count = 1;
        Check(out.m_Flecs[0] != 0, "Flecs boids query");
    }
    return out;
}

static Stats Boids_Defold(Backend* store, Query* query, BoidsScratch* scratch)
{
    Stats stats = {};
    DataStoreLock(store->m_Data);
    uint32_t     index = 0;
    DataIterator it = DataQueryIter(query->m_Data);
    while (DataIterNext(&it) == DATA_RESULT_OK)
    {
        DataRowIterator rows = DataIterRows(&it);
        while (DataRowIterNext(&rows) == DATA_RESULT_OK)
        {
            Boid* row = &scratch->m_Snapshot[index++];
            memcpy(&row->m_Position, DataRowIterGetVector3(&rows, query->m_PositionField), sizeof(Vector3));
            memcpy(&row->m_Velocity, DataRowIterGetVector3(&rows, query->m_VelocityField), sizeof(Vector3));
        }
    }
    BuildBoidCells(scratch);
    index = 0;
    it = DataQueryIter(query->m_Data);
    while (DataIterNext(&it) == DATA_RESULT_OK)
    {
        ++stats.m_Batches;
        DataRowIterator rows = DataIterRows(&it);
        while (DataRowIterNext(&rows) == DATA_RESULT_OK)
        {
            const BoidCell& cell = scratch->m_Cells[scratch->m_RowCells[index]];
            Boid            result = SteerBoid(scratch->m_Snapshot[index++], cell);
            memcpy(DataRowIterGetVector3Mut(&rows, query->m_PositionField), &result.m_Position, sizeof(Vector3));
            memcpy(DataRowIterGetVector3Mut(&rows, query->m_VelocityField), &result.m_Velocity, sizeof(Vector3));
            stats.m_Sum += BoidChecksum(result);
            stats.m_Hits += cell.m_Avoid;
            ++stats.m_Rows;
        }
    }
    DataStoreUnlock(store->m_Data);
    return stats;
}

static Stats Boids_Flecs(Backend* store, Query* query, BoidsScratch* scratch)
{
    Stats      stats = {};
    uint32_t   index = 0;
    ecs_iter_t it = ecs_query_iter(store->m_World, query->m_Flecs[0]);
    while (ecs_query_next(&it))
    {
        const Vector3* positions = (const Vector3*)ecs_field_w_size(&it, sizeof(Vector3), 0);
        const Vector3* velocities = (const Vector3*)ecs_field_w_size(&it, sizeof(Vector3), 1);
        for (int32_t r = 0; r < it.count; ++r)
            scratch->m_Snapshot[index++] = Boid { .m_Position = positions[r], .m_Velocity = velocities[r] };
    }
    BuildBoidCells(scratch);
    index = 0;
    it = ecs_query_iter(store->m_World, query->m_Flecs[0]);
    while (ecs_query_next(&it))
    {
        ++stats.m_Batches;
        Vector3* positions = (Vector3*)ecs_field_w_size(&it, sizeof(Vector3), 0);
        Vector3* velocities = (Vector3*)ecs_field_w_size(&it, sizeof(Vector3), 1);
        for (int32_t r = 0; r < it.count; ++r)
        {
            const BoidCell& cell = scratch->m_Cells[scratch->m_RowCells[index]];
            Boid            result = SteerBoid(scratch->m_Snapshot[index++], cell);
            positions[r] = result.m_Position;
            velocities[r] = result.m_Velocity;
            stats.m_Sum += BoidChecksum(result);
            stats.m_Hits += cell.m_Avoid;
            ++stats.m_Rows;
        }
    }
    return stats;
}

void MeasureBoids(Backend* store, const Fixture* input, Query* queries, uint32_t sample)
{
    BoidsScratch scratch[2] = { CreateBoidsScratch(input->m_Types[2].m_Count), CreateBoidsScratch(input->m_Types[3].m_Count) };
    Stats        total = {};
    uint64_t     start = BeginOperation();
    for (uint32_t s = 0; s < 2; ++s)
    {
        Stats stats = store->m_Kind ? Boids_Flecs(store, &queries[s], &scratch[s]) : Boids_Defold(store, &queries[s], &scratch[s]);
        total.m_Rows += stats.m_Rows;
        total.m_Hits += stats.m_Hits;
        total.m_Batches += stats.m_Batches;
        total.m_Sum += stats.m_Sum;
    }
    Record(store, input, sample, "boids", start, EndOperation(), total.m_Rows, total);
    Check(total.m_Rows == input->m_Types[2].m_Count + input->m_Types[3].m_Count, "boid count");
    for (uint32_t s = 0; s < 2; ++s)
    {
        const TypeInput* type = &input->m_Types[s + 2];
        BoidsScratch     reference = CreateBoidsReference(type);
        for (uint32_t r = 0; r < type->m_Count; ++r)
        {
            Boid     actual;
            uint64_t id = store->m_Ids[type->m_Offset + r];
            if (!store->m_Kind)
            {
                DataVector3 position, velocity;
                Check(DataFieldGetVector3(store->m_Data, id, g_Fields[POSITION], &position) == DATA_RESULT_OK && DataFieldGetVector3(store->m_Data, id, g_Fields[VELOCITY], &velocity) == DATA_RESULT_OK, "boids stored values");
                memcpy(&actual.m_Position, &position, sizeof(Vector3));
                memcpy(&actual.m_Velocity, &velocity, sizeof(Vector3));
            }
            else
            {
                actual.m_Position = *(const Vector3*)ecs_get_id(store->m_World, id, store->m_Fields[POSITION]);
                actual.m_Velocity = *(const Vector3*)ecs_get_id(store->m_World, id, store->m_Fields[VELOCITY]);
            }
            ValidateBoid(actual, SteerBoid(reference.m_Snapshot[r], reference.m_Cells[reference.m_RowCells[r]]));
        }
        DestroyBoidsScratch(&reference);
        DestroyBoidsScratch(&scratch[s]);
    }
}

void ProfileBoids(const Fixture* input, uint32_t kind, uint32_t samples, uint32_t passes)
{
    Backend store = CreateBackend(input, kind, 0, "setup");
    for (uint32_t t = 0; t < TYPE_COUNT; ++t)
        Check(CreateBulk(&store, input, t, 0, input->m_Types[t].m_Count) == 0, "profile bulk insertion");
    Query queries[] = { CreateBoidsQuery(&store, 0), CreateBoidsQuery(&store, 1) };
    fprintf(stderr, "Profile ready: %s, Boids, %u passes per sample\n", BACKENDS[kind], passes);
    for (uint32_t sample = 0; sample <= samples; ++sample)
        for (uint32_t pass = 0; pass < passes; ++pass)
        {
            ResetValues(&store, input);
            MeasureBoids(&store, input, queries, sample);
        }
    DestroyQuery(&queries[0]);
    DestroyQuery(&queries[1]);
    DestroyBackend(&store, input, 0, "destroy");
}

#ifdef DATA_BENCHMARK_ENTT
CoreEnttBoids CreateBoidsQuery_EnTT(CoreEnttStore* store, uint32_t flock)
{
    CoreEnttRegistry& registry = store->m_Registry;
    uint64_t          tag = dmHashString64(flock ? "enemy" : "player");
    return CoreEnttBoids(registry.storage<CoreEnttPosition>(), registry.storage<CoreEnttVelocity>(), registry.storage<CoreEnttTag>((entt::id_type)tag));
}

Stats Boids_EnTT(CoreEnttBoids* query, BoidsScratch* scratch)
{
    uint32_t index = 0;
    query->each([&](const CoreEnttPosition& position, const CoreEnttVelocity& velocity) {
        scratch->m_Snapshot[index++] = Boid { .m_Position = position.m_Value, .m_Velocity = velocity.m_Value };
    });
    BuildBoidCells(scratch);
    Stats stats = {};
    index = 0;
    query->each([&](CoreEnttPosition& position, CoreEnttVelocity& velocity) {
        const BoidCell& cell = scratch->m_Cells[scratch->m_RowCells[index]];
        Boid            result = SteerBoid(scratch->m_Snapshot[index++], cell);
        position.m_Value = result.m_Position;
        velocity.m_Value = result.m_Velocity;
        stats.m_Sum += BoidChecksum(result);
        stats.m_Hits += cell.m_Avoid;
        ++stats.m_Rows;
    });
    return stats;
}
#endif
