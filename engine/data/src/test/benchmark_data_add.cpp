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

Stats AddInstances_Defold(Backend* store, const Fixture* input)
{
    Stats stats = {};
    for (uint32_t t = 0; t < TYPE_COUNT; ++t)
    {
        const TypeInput* type = &input->m_Types[t];
        for (uint32_t r = 0; r < type->m_Extra; r += 100)
        {
            uint32_t count = type->m_Extra - r < 100 ? type->m_Extra - r : 100;
            stats.m_Error |= CreateBulk_Defold(store, input, t, type->m_Count + r, count);
        }
    }
    return stats;
}

Stats AddInstances_Flecs(Backend* store, const Fixture* input)
{
    Stats stats = {};
    for (uint32_t t = 0; t < TYPE_COUNT; ++t)
    {
        const TypeInput* type = &input->m_Types[t];
        for (uint32_t r = 0; r < type->m_Extra; r += 100)
        {
            uint32_t count = type->m_Extra - r < 100 ? type->m_Extra - r : 100;
            stats.m_Error |= CreateBulk_Flecs(store, input, t, type->m_Count + r, count);
        }
    }
    return stats;
}
