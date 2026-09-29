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

#include "benchmark_data_common.h"
#ifdef DATA_BENCHMARK_ENTT
#include "benchmark_data_entt.h"
#endif

static const uint32_t      SEED = 0x12345678;
static const char*         TYPE_NAMES[] = { "SpotLight", "PointLight", "Player", "Enemy", "Pickup", "Breakable" };
static const uint32_t      TYPE_PERCENT[] = { 10, 15, 1, 24, 25, 25 };
const char*                BACKENDS[] = { "data", "flecs_rows", "flecs_columns", "entt" };

static const char*         FIELD_NAMES[] = { "position", "health", "velocity", "light", "range", "inner_cone_angle", "outer_cone_angle", "amount", "color", "intensity" };
uint64_t                   g_Fields[FIELD_COUNT];
static uint64_t            g_LightNames[2];
static const DataValueType g_LightTypes[] = { DATA_VALUE_TYPE_VECTOR3, DATA_VALUE_TYPE_NUMBER };
uint64_t                   g_LightTag;
static uint64_t            g_EnemyTag;
static DataFieldDesc       g_LightFields[2];
static DataStructDesc      g_LightLayout = { .m_Fields = g_LightFields, .m_FieldCount = 2, .m_Size = sizeof(Light) };

static uint32_t            Random(uint32_t* state)
{
    *state = *state * 1664525u + 1013904223u;
    return *state;
}

static DataValue Number(double value)
{
    DataValue out = { .m_Type = DATA_VALUE_TYPE_NUMBER, .m_Value = { .m_Number = value } };
    return out;
}

static DataValue Vector(Vector3 value)
{
    DataValue out = { .m_Type = DATA_VALUE_TYPE_VECTOR3 };
    memcpy(out.m_Value.m_Vector3, &value, sizeof(value));
    return out;
}

static void AddField(TypeInput* type, FieldId field_id, DataValueType kind, uint32_t offset, uint32_t size)
{
    uint32_t i = type->m_FieldCount++;
    Field    field = { .m_Field = field_id, .m_Kind = kind, .m_NativeOffset = offset, .m_NativeSize = size };
    type->m_Fields[i] = field;
    DataFieldDesc meta = { .m_Field = g_Fields[field_id], .m_Type = kind, .m_Offset = offset, .m_Struct = field_id == LIGHT ? &g_LightLayout : 0 };
    type->m_Metadata[i] = meta;
    type->m_ValueTypes[i] = kind;
    type->m_Stride = type->m_NativeStride;
}

#define FIELD(TYPE, MEMBER, FIELD_ID, KIND) AddField(t, FIELD_ID, KIND, offsetof(TYPE, MEMBER), sizeof(((TYPE*)0)->MEMBER))

static void InitLayout(TypeInput* t, uint32_t type)
{
    static const char* tags[][3] = {
        { "light", "spot_light", 0 }, { "light", "point_light", 0 }, { "actor", "player", "damageable" }, { "actor", "enemy", "damageable" }, { "pickup", 0, 0 }, { "breakable", "damageable", 0 }
    };
    t->m_Type = dmHashString64(TYPE_NAMES[type]);
    for (uint32_t i = 0; i < 3 && tags[type][i]; ++i)
        t->m_Tags[t->m_TagCount++] = dmHashString64(tags[type][i]);
    switch (type)
    {
        case 0:
            t->m_NativeStride = sizeof(SpotLight);
            FIELD(SpotLight, position, POSITION, DATA_VALUE_TYPE_VECTOR3);
            FIELD(SpotLight, light, LIGHT, DATA_VALUE_TYPE_STRUCT);
            FIELD(SpotLight, range, RANGE, DATA_VALUE_TYPE_NUMBER);
            FIELD(SpotLight, inner_cone_angle, INNER_ANGLE, DATA_VALUE_TYPE_NUMBER);
            FIELD(SpotLight, outer_cone_angle, OUTER_ANGLE, DATA_VALUE_TYPE_NUMBER);
            break;
        case 1:
            t->m_NativeStride = sizeof(PointLight);
            FIELD(PointLight, light, LIGHT, DATA_VALUE_TYPE_STRUCT);
            FIELD(PointLight, position, POSITION, DATA_VALUE_TYPE_VECTOR3);
            FIELD(PointLight, range, RANGE, DATA_VALUE_TYPE_NUMBER);
            break;
        case 2:
            t->m_NativeStride = sizeof(Player);
            FIELD(Player, position, POSITION, DATA_VALUE_TYPE_VECTOR3);
            FIELD(Player, health, HEALTH, DATA_VALUE_TYPE_NUMBER);
            FIELD(Player, velocity, VELOCITY, DATA_VALUE_TYPE_VECTOR3);
            break;
        case 3:
            t->m_NativeStride = sizeof(Enemy);
            FIELD(Enemy, health, HEALTH, DATA_VALUE_TYPE_NUMBER);
            FIELD(Enemy, velocity, VELOCITY, DATA_VALUE_TYPE_VECTOR3);
            FIELD(Enemy, position, POSITION, DATA_VALUE_TYPE_VECTOR3);
            break;
        case 4:
            t->m_NativeStride = sizeof(Pickup);
            FIELD(Pickup, position, POSITION, DATA_VALUE_TYPE_VECTOR3);
            FIELD(Pickup, amount, AMOUNT, DATA_VALUE_TYPE_NUMBER);
            break;
        case 5:
            t->m_NativeStride = sizeof(Breakable);
            FIELD(Breakable, health, HEALTH, DATA_VALUE_TYPE_NUMBER);
            FIELD(Breakable, position, POSITION, DATA_VALUE_TYPE_VECTOR3);
            break;
    }
}

#undef FIELD

static void InitFixture(Fixture* input, uint32_t count)
{
    input->m_Count = count;
    input->m_Order = new RowKey[count];
    uint32_t order = 0, random = SEED;
    for (uint32_t ti = 0; ti < TYPE_COUNT; ++ti)
    {
        TypeInput* t = &input->m_Types[ti];
        InitLayout(t, ti);
        t->m_Count = count / 100 * TYPE_PERCENT[ti];
        t->m_Extra = t->m_Count / 10;
        if (input->m_GroupSize)
            Check(t->m_Count % input->m_GroupSize == 0, "each type count must divide by packed group size");
        t->m_Offset = input->m_Total;
        uint32_t total = t->m_Count + t->m_Extra;
        input->m_Total += total;
        t->m_Values = new DataValueData[(size_t)total * t->m_FieldCount];
        t->m_LightValues = ti < 2 ? new DataValueData[(size_t)total * 2] : 0;
        t->m_Rows = new DataRowDesc[total];
        t->m_Native = new uint8_t[(size_t)total * t->m_NativeStride];
        memset(t->m_Native, 0, (size_t)total * t->m_NativeStride);
        t->m_Owners = new uint64_t[total];
        t->m_ComponentIds = new uint64_t[total];
        for (uint32_t f = 0; f < t->m_FieldCount; ++f)
            t->m_Columns[f] = new uint8_t[(size_t)total * t->m_Fields[f].m_NativeSize];
        for (uint32_t r = 0; r < total; ++r)
        {
            if (input->m_GroupSize)
                random = SEED + ti * 65537 + r % input->m_GroupSize;
            // Use high bits: the low bits of an LCG have short periods.
            Vector3 position = { .m_Values = { (float)((int)(Random(&random) >> 25) - 64), (float)((int)(Random(&random) >> 25) - 64), (float)((int)(Random(&random) >> 25) - 64) } };
            Vector3 color = { .m_Values = { (Random(&random) >> 24) / 256.0f, (Random(&random) >> 24) / 256.0f, (Random(&random) >> 24) / 256.0f } };
            Vector3 velocity = { .m_Values = { 1, 0, -1 } };
            double  health = 100 + (Random(&random) >> 16) % 101;
            Light   light = { .color = color, .intensity = 1.0 };
            for (uint32_t f = 0; f < t->m_FieldCount; ++f)
            {
                Field*    field = &t->m_Fields[f];
                DataValue value = Number(10);
                if (field->m_Field == POSITION)
                    value = Vector(position);
                else if (field->m_Field == VELOCITY)
                    value = Vector(velocity);
                else if (field->m_Field == HEALTH)
                    value = Number(health);
                else if (field->m_Field == INNER_ANGLE)
                    value = Number(0);
                else if (field->m_Field == OUTER_ANGLE)
                    value = Number(45);
                else if (field->m_Field == LIGHT)
                {
                    t->m_LightValues[r * 2] = Vector(color).m_Value;
                    t->m_LightValues[r * 2 + 1] = Number(light.intensity).m_Value;
                    value.m_Type = DATA_VALUE_TYPE_STRUCT;
                    DataStruct object = { .m_Names = g_LightNames, .m_Types = g_LightTypes, .m_Values = &t->m_LightValues[r * 2], .m_Count = 2 };
                    value.m_Value.m_Struct = object;
                }
                t->m_Values[(size_t)r * t->m_FieldCount + f] = value.m_Value;
                void*       native = t->m_Native + (size_t)r * t->m_NativeStride + field->m_NativeOffset;
                const void* source = field->m_Field == LIGHT ? (const void*)&light : (const void*)&value.m_Value;
                memcpy(native, source, field->m_NativeSize);
                memcpy(t->m_Columns[f] + (size_t)r * field->m_NativeSize, native, field->m_NativeSize);
            }
            t->m_Owners[r] = t->m_Offset + (input->m_GroupSize ? r / input->m_GroupSize * input->m_GroupSize : r) + 1;
            t->m_ComponentIds[r] = input->m_GroupSize ? (ti + 1) * 100 + r % input->m_GroupSize : t->m_Type;
            DataRowDesc row = { .m_Owner = t->m_Owners[r], .m_Types = t->m_ValueTypes, .m_Values = &t->m_Values[(size_t)r * t->m_FieldCount], .m_ValueCount = t->m_FieldCount, .m_ComponentId = t->m_ComponentIds[r] };
            t->m_Rows[r] = row;
            if (r < t->m_Count)
            {
                RowKey key = { .m_Type = ti, .m_Row = r };
                input->m_Order[order++] = key;
            }
        }
    }
    for (uint32_t i = count - 1; i; --i)
    {
        uint32_t j = Random(&random) % (i + 1);
        RowKey   swap = input->m_Order[i];
        input->m_Order[i] = input->m_Order[j];
        input->m_Order[j] = swap;
    }
}

static void DeleteFixture(Fixture* input)
{
    for (uint32_t i = 0; i < TYPE_COUNT; ++i)
    {
        TypeInput* t = &input->m_Types[i];
        for (uint32_t f = 0; f < t->m_FieldCount; ++f)
            delete[] t->m_Columns[f];
        if (input->m_Blobs[i])
            DataDestroyBlob(input->m_Blobs[i]);
        delete[] input->m_BlobBytes[i];
        delete[] t->m_Values;
        delete[] t->m_LightValues;
        delete[] t->m_Rows;
        delete[] t->m_Native;
        delete[] t->m_Owners;
        delete[] t->m_ComponentIds;
    }
    delete[] input->m_Order;
}

static ecs_entity_t RegisterComponent(ecs_world_t* world, uint32_t size, uint32_t alignment)
{
    ecs_component_desc_t desc = { .type = { .size = (ecs_size_t)size, .alignment = (ecs_size_t)alignment } };
    return ecs_component_init(world, &desc);
}

Backend CreateBackend(const Fixture* input, uint32_t kind, uint32_t sample, const char* phase)
{
    Backend out = { .m_Kind = kind };
    SetBenchmarkMemoryDomain(BENCHMARK_MEMORY_FIXTURE);
    out.m_Ids = new uint64_t[input->m_Total];
    BeginBenchmarkMemoryBackend();
    BeginBenchmarkMemoryOperation();
    if (!kind)
        out.m_Data = DataCreateStore();
    else
    {
        out.m_World = ecs_mini();
        out.m_Owner = RegisterComponent(out.m_World, 8, 8);
        out.m_Component = RegisterComponent(out.m_World, 8, 8);
    }
    for (uint32_t i = 0; i < TYPE_COUNT; ++i)
    {
        const TypeInput* t = &input->m_Types[i];
        if (!kind)
        {
            if (input->m_GroupSize)
                continue;
            DataTableDesc desc = {
                .m_Type = t->m_Type,
                .m_Tags = t->m_Tags,
                .m_TagCount = t->m_TagCount,
                .m_Fields = t->m_Metadata,
                .m_FieldCount = t->m_FieldCount,
                .m_RowStride = t->m_Stride
            };
            Check(DataRegisterTable(out.m_Data, &desc) == DATA_RESULT_OK, "register Data layout");
        }
        else
        {
            out.m_Types[i] = kind == 1 ? RegisterComponent(out.m_World, t->m_NativeStride, 8) : ecs_new(out.m_World);
            for (uint32_t f = 0; f < t->m_FieldCount; ++f)
            {
                const Field* field = &t->m_Fields[f];
                if (kind == 2 && !out.m_Fields[field->m_Field])
                    out.m_Fields[field->m_Field] = RegisterComponent(out.m_World, field->m_NativeSize, field->m_Kind == DATA_VALUE_TYPE_VECTOR3 ? 4 : 8);
            }
            for (uint32_t tag = 0; tag < t->m_TagCount; ++tag)
                out.m_Tags[i][tag] = Tag(out.m_World, t->m_Tags[tag]);
            for (uint32_t f = 0; f < t->m_FieldCount; ++f)
                out.m_Offsets[i][t->m_Fields[f].m_Field] = t->m_Fields[f].m_NativeOffset;
        }
    }
    EndBenchmarkMemoryOperation();
    RecordMemory(&out, input, sample, phase, 1, Stats());
    return out;
}

void DestroyBackend(Backend* store, const Fixture* input, uint32_t sample, const char* phase)
{
    BeginBenchmarkMemoryOperation();
    if (store->m_Data)
        DataDestroyStore(store->m_Data);
    else
        ecs_fini(store->m_World);
    delete[] store->m_Ids;
    EndBenchmarkMemoryOperation();
    RecordMemory(store, input, sample, phase, 1, Stats());
    EndBenchmarkMemoryBackend();
}

static void Run(const Fixture* input, uint32_t kind, uint32_t sample)
{
    Backend  store = CreateBackend(input, kind, sample, "setup_individual");
    Stats    stats = {};
    uint64_t start = BeginOperation();
    for (uint32_t t = 0; t < TYPE_COUNT; ++t)
        stats.m_Error |= CreateIndividual(&store, input, t, 0, input->m_Types[t].m_Count);
    Record(&store, input, sample, "create_individual", start, EndOperation(), input->m_Count, stats);
    DestroyBackend(&store, input, sample, "destroy_individual");
    store = CreateBackend(input, kind, sample, "setup_bulk");
    stats = Stats();
    start = BeginOperation();
    for (uint32_t t = 0; t < TYPE_COUNT; ++t)
        stats.m_Error |= CreateBulk(&store, input, t, 0, input->m_Types[t].m_Count);
    Record(&store, input, sample, "create_bulk", start, EndOperation(), input->m_Count, stats);
    start = BeginOperation();
    Query spot = CreateLightColorQuery(&store, input, dmHashString64("spot_light"));
    Query light = CreateLightColorQuery(&store, input, g_LightTag);
    Query health = CreateHealthPositionQuery(&store, input, 0);
    Query enemy = CreateHealthPositionQuery(&store, input, g_EnemyTag);
    Query explosion = CreateExplosionQuery(&store, input);
    Record(&store, input, sample, "create_queries", start, EndOperation(), 5, stats);
    MeasureLightColor(&store, input, &spot, sample, "spot_color");
    MeasureLightColor(&store, input, &light, sample, "all_light_color");
    MeasureHealthPosition(&store, input, &health, sample, "health_position");
    MeasureHealthPosition(&store, input, &enemy, sample, "enemy_health_position");
    ResetValues(&store, input);
    MeasureExplosion(&store, input, &explosion, sample);
    ValidateHealth(&store, input, 50, 1);
    ResetValues(&store, input);
    MeasureShuffledPosition(&store, input, sample);
    ResetValues(&store, input);
    stats = Stats();
    start = BeginOperation();
    stats = kind ? AddInstances_Flecs(&store, input) : AddInstances_Defold(&store, input);
    Record(&store, input, sample, "add_10pct_live_queries_batch100", start, EndOperation(), input->m_Total - input->m_Count, stats);
    MeasureLightColor(&store, input, &light, sample, "all_light_after_add", true);
    MeasureHealthPosition(&store, input, &health, sample, "health_after_add", true);
    uint32_t removed = input->m_Count / 100;
    stats = Stats();
    start = BeginOperation();
    stats = kind ? RemoveInstances_Flecs(&store, input) : RemoveInstances_Defold(&store, input);
    Record(&store, input, sample, "remove_1pct", start, EndOperation(), removed, stats);
    for (uint32_t i = 0; i < removed; ++i)
    {
        RowKey      key = input->m_Order[i];
        uint64_t    id = store.m_Ids[input->m_Types[key.m_Type].m_Offset + key.m_Row];
        DataVector3 value;
        Check(kind ? !ecs_is_alive(store.m_World, id) : DataGetFieldVector3(store.m_Data, id, g_Fields[POSITION], &value) == DATA_RESULT_NOT_FOUND, "removed IDs");
    }
    start = BeginOperation();
    stats = kind ? ReplaceInstances_Flecs(&store, input) : ReplaceInstances_Defold(&store, input);
    Record(&store, input, sample, "replace_1pct", start, EndOperation(), removed, stats);
    MeasureHealthPosition(&store, input, &health, sample, "health_after_churn", true);
    BeginBenchmarkMemoryOperation();
    DestroyQuery(&spot);
    DestroyQuery(&light);
    DestroyQuery(&health);
    DestroyQuery(&enemy);
    DestroyQuery(&explosion);
    EndBenchmarkMemoryOperation();
    RecordMemory(&store, input, sample, "destroy_queries", 5, Stats());
    DestroyBackend(&store, input, sample, "destroy_bulk");
}

static void BuildPackedResources(Fixture* input)
{
    for (uint32_t ti = 0; ti < TYPE_COUNT; ++ti)
    {
        const TypeInput* t = &input->m_Types[ti];
        HDataStore       source = DataCreateStore();
        DataTableDesc    desc = {
               .m_Type = t->m_Type,
               .m_Tags = t->m_Tags,
               .m_TagCount = t->m_TagCount,
               .m_Fields = t->m_Metadata,
               .m_FieldCount = t->m_FieldCount,
               .m_RowStride = t->m_Stride
        };
        Check(DataRegisterTable(source, &desc) == DATA_RESULT_OK, "packed source layout");
        DataId ids[16];
        Check(DataAddRows(source, t->m_Type, t->m_Rows, input->m_GroupSize, ids) == DATA_RESULT_OK, "packed source rows");
        uint32_t size;
        Check(DataWriteBlob(source, 0, 0, &size) == DATA_RESULT_OK, "packed source size");
        input->m_BlobBytes[ti] = new uint8_t[size];
        Check(DataWriteBlob(source, input->m_BlobBytes[ti], size, &size) == DATA_RESULT_OK, "packed source write");
        Check(DataLoadBlob(input->m_BlobBytes[ti], size, &input->m_Blobs[ti]) == DATA_RESULT_OK, "packed source load");
        DataDestroyStore(source);
    }
}

// Repeat the existing hot loop with construction and query creation outside the
// measured samples. This keeps a native sampler on the requested operation.
static void ProfileSpotColor(const Fixture* input, uint32_t kind, uint32_t samples, uint32_t passes)
{
    Backend store = CreateBackend(input, kind, 0, "setup_bulk");
    for (uint32_t t = 0; t < TYPE_COUNT; ++t)
        Check(CreateBulk(&store, input, t, 0, input->m_Types[t].m_Count) == 0, "profile bulk insertion");
    Query spot = CreateLightColorQuery(&store, input, dmHashString64("spot_light"));
    fprintf(stderr, "Profile ready: %s, %u SpotLight rows, %u passes per sample\n", BACKENDS[kind], input->m_Types[0].m_Count, passes);
    for (uint32_t sample = 0; sample <= samples; ++sample)
        MeasureLightColor(&store, input, &spot, sample, "spot_color", false, passes);
    DestroyQuery(&spot);
    DestroyBackend(&store, input, 0, "destroy_bulk");
}

// Each pass starts without overrides, matching the report's first-write case.
// Native profiles must exclude ResetValues and ExpectedExplosion.
static void ProfileExplosion(const Fixture* input, uint32_t kind, uint32_t samples, uint32_t passes)
{
    Backend store = CreateBackend(input, kind, 0, "setup_bulk");
    for (uint32_t t = 0; t < TYPE_COUNT; ++t)
        Check(CreateBulk(&store, input, t, 0, input->m_Types[t].m_Count) == 0, "profile bulk insertion");
    Query explosion = CreateExplosionQuery(&store, input);
    fprintf(stderr, "Profile ready: %s, Explosion radius 50, %u passes per sample\n", BACKENDS[kind], passes);
    for (uint32_t sample = 0; sample <= samples; ++sample)
    {
        for (uint32_t pass = 0; pass < passes; ++pass)
        {
            ResetValues(&store, input);
            MeasureExplosion(&store, input, &explosion, sample);
        }
        ValidateHealth(&store, input, 50, 1);
    }
    DestroyQuery(&explosion);
    DestroyBackend(&store, input, 0, "destroy_bulk");
}

static uint32_t ParseCount(const char* text, uint32_t maximum)
{
    char*         end;
    unsigned long value = strtoul(text, &end, 10);
    Check(*text && !*end && value && value <= maximum, "invalid numeric argument");
    return (uint32_t)value;
}

int main(int argc, char** argv)
{
    InitializeBenchmarkMemory();
#ifdef DATA_BENCHMARK_MEMORY
    if (argc == 2 && !strcmp(argv[1], "--self-test"))
    {
        printf("Memory allocator self-test passed\n");
        return 0;
    }
#endif
    if (argc > 7 || argc == 6)
    {
        fprintf(stderr, "Usage: benchmark_data_mixed [rows=1000000, multiple of 1000] [samples=7] [all|data|flecs_rows|flecs_columns] [packed_rows=0|1|4|16] [spot_color|explosion|create_population|spawn_wave|movement|position_lookup passes]\n");
        return 1;
    }
    uint32_t count = argc > 1 ? ParseCount(argv[1], 10000000) : 1000000;
    uint32_t samples = argc > 2 ? ParseCount(argv[2], 31) : 7;
    Check(count >= 1000 && count % 1000 == 0, "row count must be a multiple of 1000");
    bool entt = false;
#ifdef DATA_BENCHMARK_ENTT
    entt = argc > 3 && !strcmp(argv[3], "entt");
#endif
    uint32_t selected = 3;
    if (!entt && argc > 3 && strcmp(argv[3], "all"))
    {
        for (uint32_t i = 0; i < 3; ++i)
            if (!strcmp(argv[3], BACKENDS[i]))
                selected = i;
        Check(selected < 3, "unknown backend");
    }
    for (uint32_t p = 0; p < FIELD_COUNT; ++p)
        g_Fields[p] = dmHashString64(FIELD_NAMES[p]);
    g_LightNames[0] = g_Fields[COLOR];
    g_LightNames[1] = g_Fields[INTENSITY];
    DataFieldDesc color = { .m_Field = g_Fields[COLOR], .m_Type = DATA_VALUE_TYPE_VECTOR3, .m_Offset = offsetof(Light, color) };
    DataFieldDesc intensity = { .m_Field = g_Fields[INTENSITY], .m_Type = DATA_VALUE_TYPE_NUMBER, .m_Offset = offsetof(Light, intensity) };
    g_LightFields[0] = color;
    g_LightFields[1] = intensity;
    g_LightTag = dmHashString64("light");
    g_EnemyTag = dmHashString64("enemy");
    Fixture input = {};
    bool    core = argc > 4 && !strcmp(argv[4], "core");
    Check(!entt || core, "EnTT uses the core suite");
    Check(!core || entt || selected == 0 || selected == 2, "core suite uses data, flecs_columns or entt");
    input.m_GroupSize = !core && argc > 4 && strcmp(argv[4], "0") ? ParseCount(argv[4], 16) : 0;
    Check(!input.m_GroupSize || input.m_GroupSize == 1 || input.m_GroupSize == 4 || input.m_GroupSize == 16, "packed group size must be 1, 4 or 16");
    uint32_t profile_passes = argc == 7 ? ParseCount(argv[6], 1000000) : 0;
    if (profile_passes)
    {
        Check((!strcmp(argv[5], "spot_color") || !strcmp(argv[5], "explosion") || !strcmp(argv[5], "create_population") || !strcmp(argv[5], "spawn_wave") || !strcmp(argv[5], "movement") || !strcmp(argv[5], "position_lookup")) && !input.m_GroupSize && selected < 3, "profiling requires a supported case, one backend and decoded tables");
        Check((strcmp(argv[5], "movement") && strcmp(argv[5], "position_lookup")) || selected == 0 || selected == 2, "movement/lookup profiling uses data or flecs_columns");
    }
    SetBenchmarkMemoryDomain(BENCHMARK_MEMORY_FIXTURE);
    InitFixture(&input, count);
    SetBenchmarkMemoryDomain(BENCHMARK_MEMORY_RESOURCE);
    if (input.m_GroupSize)
        BuildPackedResources(&input);
    SetBenchmarkMemoryDomain(BENCHMARK_MEMORY_NONE);
    if (core)
        printf("# suite=core; seven standalone cases; one pass; three live queries; no alternate layouts\n");
    printf("# packed_rows=%u; packed mode repeats prototype values and groups owners; sources built/validated outside timing\n", input.m_GroupSize);
    printf("# Flecs %s; seed=%u; samples=%u; one warmup; read_passes=%u; damage=25; explosion_radius=50\n", FLECS_VERSION, SEED, samples, profile_passes ? profile_passes : core ? 1 :
                                                                                                                                                                                   READ_PASSES);
    printf("# Data: inline Light + dense rows per resource table + pooled registrations + shared defaults/reset; Flecs: inline Light, mutable components; no serialization or disk I/O\n");
    printf("# Type percentages: SpotLight=10,PointLight=15,Player=1,Enemy=24,Pickup=25,Breakable=25; dense owners are per row; packed owners are per group\n");
#ifdef DATA_BENCHMARK_MEMORY
    printf("# measurement=memory; requested payload bytes; backend-owned C++ new/delete and Flecs OS allocation hooks; no timing results\n");
    printf("# fixtures and shared resource buffers/handles are separate; allocator/tracker overhead, libc/platform internals and RSS are excluded\n");
    printf("backend,operation,rows,sample,operations,hits,batches,checksum,before_bytes,live_bytes,peak_bytes,live_blocks,peak_blocks,allocations,reallocations,frees,allocated_bytes,total_allocations,total_reallocations,total_frees,total_allocated_bytes,fixture_bytes,fixture_blocks,shared_bytes,shared_blocks\n");
#else
    printf("backend,operation,rows,sample,operations,hits,batches,total_ms,ns_per_operation,checksum\n");
#endif
    if (profile_passes)
    {
        if (!strcmp(argv[5], "create_population"))
            ProfileCreatePopulation(&input, selected, samples, profile_passes);
        else if (!strcmp(argv[5], "spawn_wave"))
            ProfileSpawnWave(&input, selected, samples, profile_passes);
        else if (!strcmp(argv[5], "movement"))
            ProfileMovement(&input, selected, samples, profile_passes);
        else if (!strcmp(argv[5], "position_lookup"))
            ProfilePositionLookup(&input, selected, samples, profile_passes);
        else if (!strcmp(argv[5], "explosion"))
            ProfileExplosion(&input, selected, samples, profile_passes);
        else
            ProfileSpotColor(&input, selected, samples, profile_passes);
    }
#ifdef DATA_BENCHMARK_ENTT
    else if (entt)
        for (uint32_t sample = 0; sample <= samples; ++sample)
        {
            fprintf(stderr, "sample %u/%u: entt, %u rows\n", sample, samples, count);
            RunCoreEnTT(&input, sample);
        }
#endif
    else
        for (uint32_t sample = 0; sample <= samples; ++sample)
        {
            for (uint32_t run = 0; run < (selected == 3 ? 3u : 1u); ++run)
            {
                uint32_t kind = selected == 3 ? (sample + run) % 3 : selected;
                fprintf(stderr, "sample %u/%u: %s, %u rows\n", sample, samples, BACKENDS[kind], count);
                if (core)
                    RunCore(&input, kind, sample);
                else if (input.m_GroupSize)
                    RunPacked(&input, kind, sample);
                else
                    Run(&input, kind, sample);
            }
        }
    DeleteFixture(&input);
    CheckBenchmarkMemoryReleased();
    return 0;
}
