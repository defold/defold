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

#include "benchmark_data_common.h"

static uint32_t FlecsColumns(Backend* store, const TypeInput* t, uint32_t ti, uint32_t start, ecs_id_t* ids, void** data)
{
    uint32_t count = 0;
    if (store->m_Kind == 1)
    {
        ids[count] = store->m_Types[ti];
        data[count++] = t->m_Native + (size_t)start * t->m_NativeStride;
    }
    else
    {
        ids[count] = store->m_Types[ti];
        data[count++] = 0;
        for (uint32_t f = 0; f < t->m_FieldCount; ++f)
        {
            ids[count] = store->m_Fields[t->m_Fields[f].m_Field];
            data[count++] = t->m_Columns[f] + (size_t)start * t->m_Fields[f].m_NativeSize;
        }
    }
    ids[count] = store->m_Group;
    data[count++] = t->m_Groups + start;
    ids[count] = store->m_Component;
    data[count++] = t->m_ComponentIds + start;
    for (uint32_t i = 0; i < t->m_TagCount; ++i)
    {
        ids[count] = store->m_Tags[ti][i];
        data[count++] = 0;
    }
    return count;
}

int CreateBulk_Defold(Backend* store, const Fixture* input, uint32_t ti, uint32_t start, uint32_t count)
{
    const TypeInput* t = &input->m_Types[ti];
    // The caller prepares complete native rows, as Flecs receives prepared components.
    return DataCreateRows(store->m_Data, t->m_Type, t->m_Groups[start], count, t->m_Native + (size_t)start * t->m_NativeStride, store->m_Ids + t->m_Offset + start);
}

void ValidateCreatedRows_Defold(Backend* store, const Fixture* input)
{
    for (uint32_t ti = 0; ti < TYPE_COUNT; ++ti)
    {
        const TypeInput* type = &input->m_Types[ti];
        for (uint32_t r = 0; r < type->m_Count; ++r)
        {
            DataId id = store->m_Ids[type->m_Offset + r];
            for (uint32_t f = 0; f < type->m_FieldCount; ++f)
            {
                const Field& field = type->m_Fields[f];
                const void*  expected = FixtureField(type, r, f);
                if (field.m_Kind == DATA_TYPE_NUMBER)
                {
                    double value;
                    Check(DataFieldGetNumber(store->m_Data, id, g_Fields[field.m_Field], &value) == DATA_RESULT_OK && value == *(const double*)expected, "created number matches input");
                }
                else if (field.m_Kind == DATA_TYPE_VECTOR3)
                {
                    DataVector3 value;
                    Check(DataFieldGetVector3(store->m_Data, id, g_Fields[field.m_Field], &value) == DATA_RESULT_OK && !memcmp(&value, expected, sizeof(value)), "created vector matches input");
                }
                else
                {
                    Check(field.m_Field == LIGHT, "created inline struct is Light");
                    const Light* light = (const Light*)expected;
                    DataVector3  color;
                    double       intensity;
                    Check(DataFieldGetVector3(store->m_Data, id, g_LightColor, &color) == DATA_RESULT_OK && !memcmp(&color, &light->color, sizeof(color)), "created light color matches input");
                    Check(DataFieldGetNumber(store->m_Data, id, g_LightIntensity, &intensity) == DATA_RESULT_OK && intensity == light->intensity, "created light intensity matches input");
                }
            }
        }
    }
}

void MeasureCreatePopulationSoA_Defold(const Fixture* input, uint32_t sample)
{
    DataFieldArray fields[TYPE_COUNT][MAX_FIELDS] = {};
    for (uint32_t ti = 0; ti < TYPE_COUNT; ++ti)
    {
        const TypeInput* type = &input->m_Types[ti];
        for (uint32_t f = 0; f < type->m_FieldCount; ++f)
            fields[ti][f] = { .m_Field = g_Fields[type->m_Fields[f].m_Field], .m_Values = type->m_Columns[f] };
    }
    Backend  store = CreateBackend(input, 0, 0, "setup");
    Stats    stats = {};
    uint64_t start = BeginOperation();
    for (uint32_t ti = 0; ti < TYPE_COUNT; ++ti)
    {
        const TypeInput* type = &input->m_Types[ti];
        stats.m_Error |= DataCreateRowsSoA(store.m_Data, type->m_Type, type->m_Groups[0], type->m_Count, type->m_FieldCount, fields[ti], store.m_Ids + type->m_Offset);
    }
    Record(&store, input, sample, "create_population_soa", start, EndOperation(), input->m_Count, stats);
    ValidateCreatedRows_Defold(&store, input);
    DestroyBackend(&store, input, 0, "destroy");
}

int CreateIndividual_Defold(Backend* store, const Fixture* input, uint32_t ti, uint32_t start, uint32_t count)
{
    const TypeInput* t = &input->m_Types[ti];
    uint64_t*        output = store->m_Ids + t->m_Offset + start;
    int              error = 0;
    for (uint32_t r = 0; r < count; ++r)
    {
        error |= DataCreateRows(store->m_Data, t->m_Type, t->m_Groups[start + r], 1, t->m_Native + (size_t)(start + r) * t->m_NativeStride, &output[r]);
    }
    return error;
}

int CreateBulk_Flecs(Backend* store, const Fixture* input, uint32_t ti, uint32_t start, uint32_t count)
{
    const TypeInput* t = &input->m_Types[ti];
    uint64_t*        output = store->m_Ids + t->m_Offset + start;
    ecs_id_t         ids[16] = {};
    void*            data[16] = {};
    uint32_t         fields = FlecsColumns(store, t, ti, start, ids, data);
    ecs_bulk_desc_t  desc = { .count = (int32_t)count, .data = data };
    memcpy(desc.ids, ids, fields * sizeof(ecs_id_t));
    const ecs_entity_t* added = ecs_bulk_init(store->m_World, &desc);
    if (!added)
        return 1;
    memcpy(output, added, count * sizeof(ecs_entity_t));
    return 0;
}

int CreateBulk(Backend* store, const Fixture* input, uint32_t ti, uint32_t start, uint32_t count)
{
    return store->m_Kind ? CreateBulk_Flecs(store, input, ti, start, count) : CreateBulk_Defold(store, input, ti, start, count);
}

// Every pass starts with empty registered tables, matching Create population.
// Filter sampled stacks to CreateBulk/DataCreateRows: setup and teardown also repeat.
void ProfileCreatePopulation(const Fixture* input, uint32_t kind, uint32_t samples, uint32_t passes)
{
    fprintf(stderr, "Profile ready: %s, Create population, %u passes per sample\n", BACKENDS[kind], passes);
    for (uint32_t sample = 0; sample <= samples; ++sample)
    {
        for (uint32_t pass = 0; pass < passes; ++pass)
        {
            Backend  store = CreateBackend(input, kind, 0, "setup");
            Stats    stats = {};
            uint64_t start = BeginOperation();
            for (uint32_t t = 0; t < TYPE_COUNT; ++t)
                stats.m_Error |= CreateBulk(&store, input, t, 0, input->m_Types[t].m_Count);
            Record(&store, input, sample, "create_population", start, EndOperation(), input->m_Count, stats);
            DestroyBackend(&store, input, 0, "destroy");
        }
    }
}

int CreateIndividual_Flecs(Backend* store, const Fixture* input, uint32_t ti, uint32_t start, uint32_t count)
{
    const TypeInput* t = &input->m_Types[ti];
    uint64_t*        output = store->m_Ids + t->m_Offset + start;
    ecs_id_t         ids[16] = {};
    void*            data[16] = {};
    uint32_t         fields = FlecsColumns(store, t, ti, start, ids, data);
    uint32_t         sizes[16] = {};
    for (uint32_t f = 0; f < fields; ++f)
    {
        const ecs_type_info_t* info = ecs_get_type_info(store->m_World, ids[f]);
        sizes[f] = info ? info->size : 0;
    }
    for (uint32_t r = 0; r < count; ++r)
    {
        ecs_value_t values[17] = {};
        for (uint32_t f = 0; f < fields; ++f)
        {
            values[f].type = ids[f];
            values[f].ptr = data[f] ? (uint8_t*)data[f] + (size_t)r * sizes[f] : 0;
        }
        output[r] = ecs_insert_w_values(store->m_World, values);
        if (!output[r])
            return 1;
    }
    return 0;
}

int CreateIndividual(Backend* store, const Fixture* input, uint32_t ti, uint32_t start, uint32_t count)
{
    return store->m_Kind ? CreateIndividual_Flecs(store, input, ti, start, count) : CreateIndividual_Defold(store, input, ti, start, count);
}

#ifdef DATA_BENCHMARK_ENTT
#include "benchmark_data_entt.h"
#include <ranges>

// Transform prepared native columns lazily into EnTT component values. These
// standard input iterators feed registry.insert without an intermediate buffer.
template <typename Component, typename Value>
static void InsertEnttColumn(CoreEnttRegistry* registry, CoreEnttEntity* first, uint32_t count, const Value* source)
{
    auto values = std::ranges::subrange(source, source + count) | std::views::transform([](const Value& value) { return Component { .m_Value = value }; });
    registry->insert<Component>(first, first + count, values.begin());
}

void Setup_EnTT(CoreEnttStore* store, const Fixture* input)
{
    CoreEnttRegistry& registry = store->m_Registry;
    registry.storage<CoreEnttPosition>();
    registry.storage<CoreEnttVelocity>();
    registry.storage<CoreEnttHealth>();
    registry.storage<CoreEnttLight>();
    registry.storage<CoreEnttRange>();
    registry.storage<CoreEnttInnerAngle>();
    registry.storage<CoreEnttOuterAngle>();
    registry.storage<CoreEnttAmount>();
    registry.storage<CoreEnttOwner>();
    registry.storage<CoreEnttIdentity>();
    for (uint32_t t = 0; t < TYPE_COUNT; ++t)
    {
        const TypeInput* type = &input->m_Types[t];
        registry.storage<CoreEnttTag>((entt::id_type)type->m_Type);
        for (uint32_t tag = 0; tag < type->m_TagCount; ++tag)
            registry.storage<CoreEnttTag>((entt::id_type)type->m_Tags[tag]);
    }
}

void CreateBulk_EnTT(CoreEnttStore* store, const Fixture* input, uint32_t ti, uint32_t start, uint32_t count)
{
    const TypeInput*  type = &input->m_Types[ti];
    CoreEnttRegistry& registry = store->m_Registry;
    CoreEnttEntity*   first = store->m_Ids + type->m_Offset + start;
    registry.create(first, first + count);
    for (uint32_t f = 0; f < type->m_FieldCount; ++f)
    {
        const void* source = type->m_Columns[f] + (size_t)start * type->m_Fields[f].m_NativeSize;
        switch (type->m_Fields[f].m_Field)
        {
            case POSITION:
                InsertEnttColumn<CoreEnttPosition>(&registry, first, count, (const Vector3*)source);
                break;
            case VELOCITY:
                InsertEnttColumn<CoreEnttVelocity>(&registry, first, count, (const Vector3*)source);
                break;
            case HEALTH:
                InsertEnttColumn<CoreEnttHealth>(&registry, first, count, (const double*)source);
                break;
            case LIGHT:
                InsertEnttColumn<CoreEnttLight>(&registry, first, count, (const Light*)source);
                break;
            case RANGE:
                InsertEnttColumn<CoreEnttRange>(&registry, first, count, (const double*)source);
                break;
            case INNER_ANGLE:
                InsertEnttColumn<CoreEnttInnerAngle>(&registry, first, count, (const double*)source);
                break;
            case OUTER_ANGLE:
                InsertEnttColumn<CoreEnttOuterAngle>(&registry, first, count, (const double*)source);
                break;
            case AMOUNT:
                InsertEnttColumn<CoreEnttAmount>(&registry, first, count, (const double*)source);
                break;
            default:
                Check(false, "EnTT input field");
        }
    }
    InsertEnttColumn<CoreEnttOwner>(&registry, first, count, type->m_Groups + start);
    InsertEnttColumn<CoreEnttIdentity>(&registry, first, count, type->m_ComponentIds + start);
    registry.storage<CoreEnttTag>((entt::id_type)type->m_Type).insert(first, first + count);
    for (uint32_t tag = 0; tag < type->m_TagCount; ++tag)
        registry.storage<CoreEnttTag>((entt::id_type)type->m_Tags[tag]).insert(first, first + count);
}

Stats CreatePopulation_EnTT(CoreEnttStore* store, const Fixture* input)
{
    for (uint32_t t = 0; t < TYPE_COUNT; ++t)
        CreateBulk_EnTT(store, input, t, 0, input->m_Types[t].m_Count);
    return Stats();
}

#endif
