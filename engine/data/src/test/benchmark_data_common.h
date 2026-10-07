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

#ifndef DM_BENCHMARK_DATA_COMMON_H
#define DM_BENCHMARK_DATA_COMMON_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlib/hash.h>
#include <dlib/time.h>
#include <flecs.h>
#include <dmsdk/data/data.h>
#include "benchmark_memory.h"

static const uint32_t TYPE_COUNT = 6;
static const uint32_t MAX_FIELDS = 6;
static const uint32_t READ_PASSES = 5;

enum FieldId
{
    POSITION,
    HEALTH,
    VELOCITY,
    LIGHT,
    RANGE,
    INNER_ANGLE,
    OUTER_ANGLE,
    AMOUNT,
    COLOR,
    INTENSITY,
    FIELD_COUNT
};

struct Vector3
{
    float m_Values[3];
};
struct Light
{
    Vector3 color;
    double  intensity;
};

struct SpotLight
{
    Vector3 position;
    Light   light;
    double  range, inner_cone_angle, outer_cone_angle;
};
struct PointLight
{
    Light   light;
    Vector3 position;
    double  range;
};
struct Player
{
    Vector3 position;
    double  health;
    Vector3 velocity;
};
struct Enemy
{
    double  health;
    Vector3 velocity, position;
};
struct Pickup
{
    Vector3 position;
    double  amount;
};
struct Breakable
{
    double  health;
    Vector3 position;
};

struct Field
{
    FieldId       m_Field;
    DataValueType m_Kind;
    uint32_t      m_NativeOffset;
    uint32_t      m_NativeSize;
};

struct TypeInput
{
    uint64_t       m_Type;
    uint64_t       m_Tags[3];
    uint32_t       m_TagCount;
    Field          m_Fields[MAX_FIELDS];
    DataFieldDesc  m_Metadata[MAX_FIELDS];
    uint32_t       m_FieldCount;
    uint32_t       m_Stride;
    uint32_t       m_NativeStride;
    uint32_t       m_Offset;
    uint32_t       m_Count;
    uint32_t       m_Extra;
    uint8_t*       m_Native;
    uint8_t*       m_Columns[MAX_FIELDS];
    uint64_t*      m_Groups;
    uint64_t*      m_ComponentIds;
};

struct RowKey
{
    uint32_t m_Type, m_Row;
};
struct Fixture
{
    TypeInput m_Types[TYPE_COUNT];
    uint32_t  m_Count, m_Total;
    RowKey*   m_Order;
    uint32_t  m_GroupSize;
    HDataBlob m_Blobs[TYPE_COUNT];
    uint8_t*  m_BlobBytes[TYPE_COUNT];
};

struct Backend
{
    uint32_t     m_Kind;
    HDataStore   m_Data;
    ecs_world_t* m_World;
    ecs_entity_t m_Types[TYPE_COUNT];
    ecs_entity_t m_Fields[FIELD_COUNT];
    ecs_entity_t m_Tags[TYPE_COUNT][3];
    uint32_t     m_Offsets[TYPE_COUNT][FIELD_COUNT];
    ecs_entity_t m_Group, m_Component;
    uint64_t*    m_Ids;
};

struct Query
{
    HDataQuery   m_Data;
    ecs_query_t* m_Flecs[TYPE_COUNT];
    uint32_t     m_Types[TYPE_COUNT];
    uint32_t     m_Count;
    uint64_t     m_Tag;
    uint32_t     m_PositionField, m_HealthField, m_ColorField, m_VelocityField, m_IntensityField;
};

struct Stats
{
    double   m_Sum;
    uint64_t m_Rows, m_Hits, m_Batches;
    int      m_Error;
};

void                 InitFixtureMetadata();
void                 InitFixture(Fixture* input, uint32_t count);
void                 DeleteFixture(Fixture* input);
const void*          FixtureField(const TypeInput* type, uint32_t row, uint32_t field);
uint8_t*             ReadFixtureBlob(const char* name, uint32_t* out_size);

extern uint64_t      g_Fields[FIELD_COUNT];
extern uint64_t      g_LightTag;
extern uint64_t      g_EnemyTag;
extern const char*   BACKENDS[];

void                 Check(bool ok, const char* message);
uint64_t             BeginOperation();
uint64_t             EndOperation();
void                 RecordMemory(const Backend* store, const Fixture* input, uint32_t sample, const char* operation, uint64_t operations, const Stats& stats);
uint32_t             FindField(const TypeInput* type, FieldId field);
bool                 HasTag(const TypeInput* type, uint64_t tag);
ecs_entity_t         Tag(ecs_world_t* world, uint64_t hash);
void                 Validate(const Stats& actual, const Stats& expected);
void                 Record(const Backend* store, const Fixture* input, uint32_t sample, const char* operation, uint64_t start, uint64_t end, uint64_t operations, const Stats& stats);
void                 ResetValues(Backend* store, const Fixture* input);
void                 ValidateHealth(Backend* store, const Fixture* input, int radius, uint32_t writes);

static inline double SumVector(const float* v)
{
    return (double)v[0] + v[1] + v[2];
}

static inline bool Hit(const float* p, int radius)
{
    return radius >= 0 && p[0] * p[0] + p[1] * p[1] + p[2] * p[2] <= (float)(radius * radius);
}

static inline double Damaged(double health, uint32_t writes)
{
    double result = health - 25 * writes;
    return result < 0 ? 0 : result;
}

static inline void* FlecsField(Backend* store, const TypeInput* t, uint32_t ti, uint32_t r, FieldId field)
{
    uint64_t id = store->m_Ids[t->m_Offset + r];
    uint8_t* value = (uint8_t*)ecs_get_mut_id(store->m_World, id, store->m_Kind == 1 ? store->m_Types[ti] : store->m_Fields[field]);
    return value ? value + (store->m_Kind == 1 ? store->m_Offsets[ti][field] : 0) : 0;
}

typedef Stats (*QueryBenchmark)(Backend* store, const Fixture* input, Query* query);
void    MeasureQuery(Backend* store, const Fixture* input, Query* query, uint32_t sample, const char* name, QueryBenchmark benchmark, Stats expected, uint32_t passes);
Stats   LightColor_Defold(Backend* store, const Fixture* input, Query* query);
Stats   LightColor_Flecs(Backend* store, const Fixture* input, Query* query);
Stats   HealthPosition_Defold(Backend* store, const Fixture* input, Query* query);
Stats   HealthPosition_Flecs(Backend* store, const Fixture* input, Query* query);
Stats   Explosion_Defold(Backend* store, const Fixture* input, Query* query);
Stats   Explosion_Flecs(Backend* store, const Fixture* input, Query* query);

void    MeasureLightColor(Backend* store, const Fixture* input, Query* query, uint32_t sample, const char* name, bool extra = false, uint32_t passes = READ_PASSES);
void    MeasureHealthPosition(Backend* store, const Fixture* input, Query* query, uint32_t sample, const char* name, bool extra = false, double extra_sum = 0);
void    MeasureExplosion(Backend* store, const Fixture* input, Query* query, uint32_t sample);
void    MeasureShuffledPosition(Backend* store, const Fixture* input, uint32_t sample);
void    MeasurePackedAccess(Backend* store, const Fixture* input, uint32_t sample, uint32_t percent, uint32_t write);
FieldId ScalarField(uint32_t type);
int     CreateBulk_Defold(Backend* store, const Fixture* input, uint32_t ti, uint32_t start, uint32_t count);
void    ValidateCreatedRows_Defold(Backend* store, const Fixture* input);
void    MeasureCreatePopulationSoA_Defold(const Fixture* input, uint32_t sample);
int     CreateBulk_Flecs(Backend* store, const Fixture* input, uint32_t ti, uint32_t start, uint32_t count);
int     CreateBulk(Backend* store, const Fixture* input, uint32_t ti, uint32_t start, uint32_t count);
int     CreateIndividual_Defold(Backend* store, const Fixture* input, uint32_t ti, uint32_t start, uint32_t count);
int     CreateIndividual_Flecs(Backend* store, const Fixture* input, uint32_t ti, uint32_t start, uint32_t count);
int     CreateIndividual(Backend* store, const Fixture* input, uint32_t ti, uint32_t start, uint32_t count);
Stats   AddInstances_Defold(Backend* store, const Fixture* input);
Stats   AddInstances_Flecs(Backend* store, const Fixture* input);
Stats   RemoveInstances_Defold(Backend* store, const Fixture* input);
Stats   RemoveInstances_Flecs(Backend* store, const Fixture* input);
Stats   ReplaceInstances_Defold(Backend* store, const Fixture* input);
Stats   ReplaceInstances_Flecs(Backend* store, const Fixture* input);
Stats   ShuffledPosition_Defold(Backend* store, const Fixture* input);
Stats   ShuffledPosition_Flecs(Backend* store, const Fixture* input);

void    RunPacked(const Fixture* input, uint32_t kind, uint32_t sample);
void    RunCore(const Fixture* input, uint32_t kind, uint32_t sample);
void    ProfileSpawnWave(const Fixture* input, uint32_t kind, uint32_t samples, uint32_t passes);
void    ProfileCreatePopulation(const Fixture* input, uint32_t kind, uint32_t samples, uint32_t passes);
void    ProfilePositionLookup(const Fixture* input, uint32_t kind, uint32_t samples, uint32_t passes);
Query   CreateNearbyLightsQuery(Backend* store);
void    MeasureNearbyLights(Backend* store, const Fixture* input, Query* query, uint32_t sample);
void    MeasurePositionLookup(Backend* store, const Fixture* input, uint32_t sample);
Backend CreateBackend(const Fixture* input, uint32_t kind, uint32_t sample, const char* phase);
void    DestroyBackend(Backend* store, const Fixture* input, uint32_t sample, const char* phase);

// Caller owns each case query and releases it with DestroyQuery after traversal.
Query CreateLightColorQuery(Backend* store, const Fixture* input, uint64_t tag);
Query CreateHealthPositionQuery(Backend* store, const Fixture* input, uint64_t tag);
Query CreateExplosionQuery(Backend* store, const Fixture* input);
void  DestroyQuery(Query* query);

extern uint64_t g_LightColor, g_LightIntensity;

#endif
