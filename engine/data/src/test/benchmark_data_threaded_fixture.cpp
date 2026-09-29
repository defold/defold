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

#include <stdio.h>
#include <stdlib.h>
#include "benchmark_data_threaded.h"

void ThreadedCheck(bool ok, const char* message)
{
    if (!ok)
    {
        fprintf(stderr, "Threaded benchmark: %s\n", message);
        abort();
    }
}

uint32_t ThreadedRandom(uint32_t* state)
{
    *state = *state * 1664525u + 1013904223u;
    return *state;
}

ThreadedWave ThreadedPlanWave(uint32_t initial_groups, uint32_t frame, uint32_t live_enemies)
{
    uint32_t     random = 0x91e10da5u ^ frame;
    uint32_t     content_limit = initial_groups / 100 + 1;
    uint32_t     enemy_limit = initial_groups / 10;
    ThreadedWave wave = { .m_Content = 1 + (ThreadedRandom(&random) >> 16) % content_limit };
    switch (frame % 8)
    {
        case 0:
            wave.m_EnemyAdd = enemy_limit + (ThreadedRandom(&random) >> 16) % enemy_limit;
            break;
        case 1:
        case 3:
            wave.m_EnemyAdd = (ThreadedRandom(&random) >> 16) % (enemy_limit / 2 + 1);
            wave.m_EnemyRemove = (ThreadedRandom(&random) >> 16) % (enemy_limit / 2 + 1);
            break;
        case 2:
            wave.m_EnemyRemove = enemy_limit / 2 + (ThreadedRandom(&random) >> 16) % enemy_limit;
            break;
        case 4:
        case 7:
            wave.m_EnemyRemove = live_enemies;
            break;
        case 5:
            break; // A quiet frame for enemy spawning/despawning.
        case 6:
            wave.m_EnemyAdd = 1 + (ThreadedRandom(&random) >> 16) % enemy_limit;
            break;
    }
    if (wave.m_EnemyRemove > live_enemies)
        wave.m_EnemyRemove = live_enemies;
    return wave;
}

uint32_t ThreadedGroupCapacity(uint32_t initial_groups, uint32_t frames)
{
    uint32_t capacity = initial_groups, enemies = 0;
    for (uint32_t frame = 0; frame < frames; ++frame)
    {
        ThreadedWave wave = ThreadedPlanWave(initial_groups, frame, enemies);
        capacity += wave.m_Content + wave.m_EnemyAdd;
        enemies = enemies - wave.m_EnemyRemove + wave.m_EnemyAdd;
    }
    return capacity;
}

void ThreadedDefaults(ThreadedReferenceRow* defaults, bool enemies)
{
    uint32_t component = 0, random = enemies ? 0x9abc : 0x1234;
    for (uint32_t t = 0; t < THREAD_TYPE_COUNT; ++t)
        for (uint32_t r = 0; r < ThreadedTypeRows(t, enemies); ++r, ++component)
        {
            ThreadedReferenceRow* row = &defaults[component];
            row->m_Type = t;
            row->m_Live = true;
            for (uint32_t axis = 0; axis < 3; ++axis)
            {
                row->m_Position.m_Values[axis] = (float)((int)(ThreadedRandom(&random) >> 25) - 64);
                row->m_Color.m_Values[axis] = (ThreadedRandom(&random) >> 24) / 256.0f;
            }
            row->m_Velocity.m_Values[0] = 1;
            row->m_Velocity.m_Values[2] = -1;
            row->m_Health = r % 4 == 0 ? 100 : r % 4 == 1 ? 99.9 :
                                                            (double)((ThreadedRandom(&random) >> 16) % 101);
            row->m_Intensity = 1;
        }
}
