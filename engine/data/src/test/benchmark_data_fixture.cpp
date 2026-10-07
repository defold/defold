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

static const uint32_t SEED = 0x12345678;
static const char*    TYPE_NAMES[] = { "SpotLight", "PointLight", "Player", "Enemy", "Pickup", "Breakable" };
static const uint32_t TYPE_PERCENT[] = { 10, 15, 1, 24, 25, 25 };
const char*           BACKENDS[] = { "data", "flecs_rows", "flecs_columns", "entt" };

static const char*    FIELD_NAMES[] = { "position", "health", "velocity", "light", "range", "inner_cone_angle", "outer_cone_angle", "amount", "color", "intensity" };
uint64_t              g_Fields[FIELD_COUNT];
uint64_t              g_LightTag;
uint64_t              g_EnemyTag;
uint64_t              g_LightColor, g_LightIntensity;
static DataFieldDesc  g_LightFields[2];
static DataStructDesc g_LightLayout = { .m_Fields = g_LightFields, .m_FieldCount = 2, .m_Size = sizeof(Light) };

static uint32_t       Random(uint32_t* state)
{
    *state = *state * 1664525u + 1013904223u;
    return *state;
}

static void AddField(TypeInput* type, FieldId field_id, DataValueType kind, uint32_t offset, uint32_t size)
{
    uint32_t i = type->m_FieldCount++;
    Field    field = { .m_Field = field_id, .m_Kind = kind, .m_NativeOffset = offset, .m_NativeSize = size };
    type->m_Fields[i] = field;
    DataFieldDesc meta = { .m_Field = g_Fields[field_id], .m_Type = kind, .m_Offset = offset, .m_Struct = field_id == LIGHT ? &g_LightLayout : 0, .m_Name = FIELD_NAMES[field_id] };
    type->m_Metadata[i] = meta;
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
            FIELD(SpotLight, position, POSITION, DATA_TYPE_VECTOR3);
            FIELD(SpotLight, light, LIGHT, DATA_TYPE_STRUCT);
            FIELD(SpotLight, range, RANGE, DATA_TYPE_NUMBER);
            FIELD(SpotLight, inner_cone_angle, INNER_ANGLE, DATA_TYPE_NUMBER);
            FIELD(SpotLight, outer_cone_angle, OUTER_ANGLE, DATA_TYPE_NUMBER);
            break;
        case 1:
            t->m_NativeStride = sizeof(PointLight);
            FIELD(PointLight, light, LIGHT, DATA_TYPE_STRUCT);
            FIELD(PointLight, position, POSITION, DATA_TYPE_VECTOR3);
            FIELD(PointLight, range, RANGE, DATA_TYPE_NUMBER);
            break;
        case 2:
            t->m_NativeStride = sizeof(Player);
            FIELD(Player, position, POSITION, DATA_TYPE_VECTOR3);
            FIELD(Player, health, HEALTH, DATA_TYPE_NUMBER);
            FIELD(Player, velocity, VELOCITY, DATA_TYPE_VECTOR3);
            break;
        case 3:
            t->m_NativeStride = sizeof(Enemy);
            FIELD(Enemy, health, HEALTH, DATA_TYPE_NUMBER);
            FIELD(Enemy, velocity, VELOCITY, DATA_TYPE_VECTOR3);
            FIELD(Enemy, position, POSITION, DATA_TYPE_VECTOR3);
            break;
        case 4:
            t->m_NativeStride = sizeof(Pickup);
            FIELD(Pickup, position, POSITION, DATA_TYPE_VECTOR3);
            FIELD(Pickup, amount, AMOUNT, DATA_TYPE_NUMBER);
            break;
        case 5:
            t->m_NativeStride = sizeof(Breakable);
            FIELD(Breakable, health, HEALTH, DATA_TYPE_NUMBER);
            FIELD(Breakable, position, POSITION, DATA_TYPE_VECTOR3);
            break;
    }
}

#undef FIELD

void InitFixtureMetadata()
{
    for (uint32_t p = 0; p < FIELD_COUNT; ++p)
        g_Fields[p] = dmHashString64(FIELD_NAMES[p]);
    DataFieldDesc color = { .m_Field = g_Fields[COLOR], .m_Type = DATA_TYPE_VECTOR3, .m_Offset = offsetof(Light, color), .m_Name = "color" };
    DataFieldDesc intensity = { .m_Field = g_Fields[INTENSITY], .m_Type = DATA_TYPE_NUMBER, .m_Offset = offsetof(Light, intensity), .m_Name = "intensity" };
    g_LightColor = dmHashString64("light.color");
    g_LightIntensity = dmHashString64("light.intensity");
    g_LightFields[0] = color;
    g_LightFields[1] = intensity;
    g_LightTag = dmHashString64("light");
    g_EnemyTag = dmHashString64("enemy");
}

void InitFixture(Fixture* input, uint32_t count)
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
        t->m_Native = new uint8_t[(size_t)total * t->m_NativeStride];
        memset(t->m_Native, 0, (size_t)total * t->m_NativeStride);
        t->m_Groups = new uint64_t[total];
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
                Field*      field = &t->m_Fields[f];
                double      scalar = field->m_Field == HEALTH ? health : field->m_Field == INNER_ANGLE ? 0 :
                     field->m_Field == OUTER_ANGLE                                                     ? 45 :
                                                                                                         10;
                const void* source = &scalar;
                if (field->m_Field == POSITION)
                    source = &position;
                else if (field->m_Field == VELOCITY)
                    source = &velocity;
                else if (field->m_Field == LIGHT)
                    source = &light;
                void* native = t->m_Native + (size_t)r * t->m_NativeStride + field->m_NativeOffset;
                memcpy(native, source, field->m_NativeSize);
                memcpy(t->m_Columns[f] + (size_t)r * field->m_NativeSize, native, field->m_NativeSize);
            }
            t->m_Groups[r] = input->m_GroupSize ? t->m_Offset + r / input->m_GroupSize * input->m_GroupSize + 1 : 1;
            t->m_ComponentIds[r] = input->m_GroupSize ? (ti + 1) * 100 + r % input->m_GroupSize : 0;
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

void DeleteFixture(Fixture* input)
{
    for (uint32_t i = 0; i < TYPE_COUNT; ++i)
    {
        TypeInput* t = &input->m_Types[i];
        for (uint32_t f = 0; f < t->m_FieldCount; ++f)
            delete[] t->m_Columns[f];
        if (input->m_Blobs[i])
            DataDestroyBlob(input->m_Blobs[i]);
        delete[] input->m_BlobBytes[i];
        delete[] t->m_Native;
        delete[] t->m_Groups;
        delete[] t->m_ComponentIds;
    }
    delete[] input->m_Order;
}

const void* FixtureField(const TypeInput* type, uint32_t row, uint32_t field)
{
    return type->m_Columns[field] + (size_t)row * type->m_Fields[field].m_NativeSize;
}
