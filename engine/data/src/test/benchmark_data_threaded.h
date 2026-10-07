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

#ifndef DM_BENCHMARK_DATA_THREADED_H
#define DM_BENCHMARK_DATA_THREADED_H

#include <dmsdk/data/data.h>
#include <dlib/jobsystem.h>

uint8_t*              ReadFixtureBlob(const char* name, uint32_t* out_size);

static const uint64_t THREAD_POSITION = 1, THREAD_HEALTH = 2, THREAD_VELOCITY = 3;
static const uint64_t THREAD_LIGHT = 4, THREAD_COLOR = 5, THREAD_INTENSITY = 6;
static const uint32_t THREAD_TYPE_COUNT = 6, THREAD_BUNDLE_ROWS = 100;
static const uint32_t THREAD_TYPE_ROWS[] = { 10, 15, 1, 24, 25, 25 };

enum ThreadedTask
{
    THREAD_MOVEMENT,
    THREAD_EXPLOSION,
    THREAD_REGENERATE,
    THREAD_LIGHTS,
    THREAD_TASK_COUNT
};
enum ThreadedState
{
    THREAD_WAITING,
    THREAD_RUNNING,
    THREAD_DONE
};

struct ThreadedStats
{
    double   m_Sum;
    uint32_t m_Rows, m_Hits;
};

// Sequential reference and validation-only IDs; never read by worker callbacks.
struct ThreadedReferenceRow
{
    DataVector3 m_Position, m_Velocity, m_Color;
    double      m_Health, m_Intensity;
    DataId      m_Id;
    uint32_t    m_Type;
    bool        m_Live;
};

struct ThreadedUpdate;

// Counts are 100-row groups. Mixed content is replaced; enemy-only waves grow
// and shrink independently. A frame seed keeps the plan independent of workers.
struct ThreadedWave
{
    uint32_t m_Content;
    uint32_t m_EnemyAdd;
    uint32_t m_EnemyRemove;
};

struct ThreadedRange
{
    const ThreadedUpdate* m_Update;
    uint32_t              m_First, m_Count;
    ThreadedStats         m_Stats;
};

// Caller-owned job group. Completion counters/state are touched only by the main
// thread. Workers borrow query/bindings and write only their range's stats.
struct ThreadedUpdate
{
    HDataQuery  m_Query;
    FJobProcess m_Process;
    void (*m_Reference)(ThreadedReferenceRow*, ThreadedStats*);
    ThreadedRange* m_Ranges;
    uint32_t       m_RangeCapacity, m_RangeCount, m_Remaining;
    uint32_t       m_PositionField, m_HealthField, m_VelocityField, m_ColorField, m_IntensityField;
    ThreadedState  m_State;
    ThreadedStats  m_Stats;
    uint32_t       m_Busy;
    uint64_t       m_WaitMicros, m_FirstAttempt;
};

uint32_t           ThreadedRandom(uint32_t* state);
void               ThreadedDefaults(ThreadedReferenceRow* defaults, bool enemies = false);
ThreadedWave       ThreadedPlanWave(uint32_t initial_groups, uint32_t frame, uint32_t live_enemies);
uint32_t           ThreadedGroupCapacity(uint32_t initial_groups, uint32_t frames);
void               Movement_Reference(ThreadedReferenceRow*, ThreadedStats*);
void               Explosion_Reference(ThreadedReferenceRow*, ThreadedStats*);
void               Regenerate_Reference(ThreadedReferenceRow*, ThreadedStats*);
void               NearbyLights_Reference(ThreadedReferenceRow*, ThreadedStats*);

void               ThreadedCheck(bool ok, const char* message);
void               CreateThreadedMovement(HDataStore store, ThreadedUpdate* update);
void               CreateThreadedExplosion(HDataStore store, ThreadedUpdate* update);
void               CreateThreadedRegenerate(HDataStore store, ThreadedUpdate* update);
void               CreateThreadedLights(HDataStore store, ThreadedUpdate* update);

static inline bool ThreadedHasHealth(uint32_t type)
{
    return type == 2 || type == 3 || type == 5;
}
static inline uint32_t ThreadedTypeRows(uint32_t type, bool enemies)
{
    return enemies ? (type == 3 ? THREAD_BUNDLE_ROWS : 0) : THREAD_TYPE_ROWS[type];
}
static inline bool ThreadedInRadius(const DataVector3* position)
{
    const float* p = position->m_Values;
    return p[0] * p[0] + p[1] * p[1] + p[2] * p[2] <= 2500.0f;
}
#endif
