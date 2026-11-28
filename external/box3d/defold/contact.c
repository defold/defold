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

#include <box3d/defold_contacts.h>
#include "../Box3D/src/contact.h"
#include "../Box3D/src/physics_world.h"
#include "../Box3D/src/body.h"
#include "../Box3D/src/shape.h"
#include <box3d/collision.h>
#include "hull_support.h"
#include <stdlib.h>
#include <string.h>

enum
{
    QUERY_SLOTS = 128, PREPARE_BATCH = 64
};

typedef struct HullResult
{
    b3Vec3               m_Normal;
    b3LocalManifoldPoint m_Points[B3_MAX_MANIFOLD_POINTS];
    b3SATCache           m_Axis;
    int                  m_Count;
} HullResult;

typedef struct HullQuery
{
    const b3HullData* m_HullA;
    const b3HullData* m_HullB;
    b3Transform       m_Pose;
    int               m_Capacity;
    HullResult        m_Result;
} HullQuery;

typedef struct PreparedContact
{
    HullQuery          m_Query;
    dmBox3DHullSupport m_Support;
    uint32_t           m_ContactGeneration;
} PreparedContact;

typedef struct WorkerQueries
{
    HullQuery m_Queries[QUERY_SLOTS];
    // A slot is valid only in the step that wrote it. Workers never share slots.
    uint64_t  m_Steps[QUERY_SLOTS];
    bool      m_UsedPreparation;
} WorkerQueries;

typedef struct ContactWork
{
    PreparedContact* m_Prepared;
    int              m_Count;
    int              m_Cursor;
    WorkerQueries    m_Workers[];
} ContactWork;

// World creation and destruction are synchronized with the world's jobs by Box3D's owner.
static ContactWork* g_ContactWork[B3_MAX_WORLDS];

void dmBox3DCreateContactWork(b3WorldId id)
{
    b3World* world = b3GetWorldFromId(id);
    ContactWork* work = calloc(1, sizeof(*work) + world->workerCount * sizeof(WorkerQueries));
    if (work)
    {
        work->m_Cursor = -1;
    }

    g_ContactWork[world->worldId] = work;
}

void dmBox3DDestroyContactWork(b3WorldId id)
{
    ContactWork* work = g_ContactWork[id.index1 - 1];
    if (work)
    {
        free(work->m_Prepared);
    }

    free(work);
    g_ContactWork[id.index1 - 1] = NULL;
}

void dmBox3DPrepareSleepingContacts(b3WorldId id)
{
    ContactWork* work = g_ContactWork[id.index1 - 1];
    if (work)
    {
        free(work->m_Prepared);
        work->m_Prepared = NULL;
        work->m_Count = 0;
        work->m_Cursor = 0;
    }
}

static bool SameQuery(const HullQuery* query, const b3HullData* a, const b3HullData* b, b3Transform pose, int capacity)
{
    return query->m_HullA == a && query->m_HullB == b && query->m_Capacity == capacity &&
           memcmp(&query->m_Pose, &pose, sizeof(pose)) == 0;
}

static void StoreQuery(HullQuery* query, const b3HullData* a, const b3HullData* b, b3Transform pose, int capacity,
                       const b3LocalManifold* manifold, b3SATCache axis)
{
    query->m_HullA = a;
    query->m_HullB = b;
    query->m_Pose = pose;
    query->m_Capacity = capacity;
    query->m_Result.m_Axis = axis;
    query->m_Result.m_Normal = manifold->normal;
    query->m_Result.m_Count = manifold->pointCount;
    memcpy(query->m_Result.m_Points, manifold->points, sizeof(*manifold->points) * manifold->pointCount);
}

static void LoadResult(const HullResult* result, b3LocalManifold* manifold, b3SATCache* axis)
{
    *axis = result->m_Axis;
    manifold->normal = result->m_Normal;
    manifold->pointCount = result->m_Count;
    memcpy(manifold->points, result->m_Points, sizeof(*manifold->points) * result->m_Count);
}

static unsigned QuerySlot(const b3HullData* a, const b3HullData* b, const b3Transform* pose)
{
    uint32_t words[7];
    _Static_assert(sizeof(words) == sizeof(*pose), "hash all transform components");
    memcpy(words, pose, sizeof(words));
    uint32_t hash = (uint32_t)((uintptr_t)a >> 4) ^ ((uint32_t)((uintptr_t)b >> 4) * 16777619u);
    for (int i = 0; i < 7; ++i)
    {
        hash = (hash ^ words[i]) * 16777619u;
    }

    hash ^= hash >> 16;
    hash *= 0x85ebca6bu;
    return (hash ^ (hash >> 13)) & (QUERY_SLOTS - 1);
}

static void CollideContactHulls(b3World* world, int worker_index, const b3Contact* contact, b3LocalManifold* manifold,
                                int capacity, const b3HullData* a, const b3HullData* b, b3Transform pose, b3SATCache* axis)
{
    static const b3SATCache empty = {
        0
    };
    ContactWork* work = g_ContactWork[world->worldId];
    if (!work || memcmp(axis, &empty, sizeof(empty)) != 0)
    {
        b3CollideHulls(manifold, capacity, a, b, pose, axis);
        return;
    }

    WorkerQueries* worker = &work->m_Workers[worker_index];
    const PreparedContact* candidate = NULL;
    if (contact->contactId < work->m_Count)
    {
        const PreparedContact* prepared = &work->m_Prepared[contact->contactId];
        if (prepared->m_ContactGeneration == contact->generation && SameQuery(&prepared->m_Query, a, b, pose, capacity))
        {
            LoadResult(&prepared->m_Query.m_Result, manifold, axis);
            worker->m_UsedPreparation = true;
            return;
        }

        const HullQuery* query = &prepared->m_Query;
        if (prepared->m_ContactGeneration == contact->generation && query->m_HullA == a && query->m_HullB == b &&
            query->m_Capacity == capacity)
        {
            candidate = prepared;
            worker->m_UsedPreparation = true;
        }
    }

    unsigned slot = QuerySlot(a, b, &pose);
    HullQuery* query = &worker->m_Queries[slot];
    if (worker->m_Steps[slot] == world->stepIndex && SameQuery(query, a, b, pose, capacity))
    {
        LoadResult(&query->m_Result, manifold, axis);
        return;
    }

    if (!candidate || !dmBox3DCollidePreparedHulls(&candidate->m_Support, candidate->m_Query.m_Pose.q, manifold, capacity, a, b, pose, axis))
    {
        b3CollideHulls(manifold, capacity, a, b, pose, axis);
    }

    if (manifold->pointCount <= B3_MAX_MANIFOLD_POINTS)
    {
        StoreQuery(query, a, b, pose, capacity, manifold, *axis);
        worker->m_Steps[slot] = world->stepIndex;
    }
}

void dmBox3DAdvanceContactPreparation(b3WorldId id)
{
    b3World* world = b3GetWorldFromId(id);
    ContactWork* work = g_ContactWork[world->worldId];
    if (!work || (work->m_Cursor < 0 && !work->m_Prepared))
    {
        return;
    }

    bool consumed = false;
    for (int i = 0; i < world->workerCount; ++i)
    {
        if (work->m_Workers[i].m_UsedPreparation)
        {
            work->m_Workers[i].m_UsedPreparation = false;
            consumed = true;
        }
    }

    if (consumed || b3GetIdCount(&world->contactIdPool) == 0)
    {
        free(work->m_Prepared);
        work->m_Prepared = NULL;
        work->m_Count = 0;
        work->m_Cursor = -1;
    }

    if (work->m_Cursor < 0)
    {
        return;
    }

    uint64_t start = b3GetTicks();
    if (!work->m_Prepared)
    {
        work->m_Count = world->contacts.count;
        if (!work->m_Count)
        {
            return;
        }

        work->m_Prepared = calloc(work->m_Count, sizeof(*work->m_Prepared));
        if (!work->m_Prepared)
        {
            work->m_Count = 0;
            work->m_Cursor = -1;
            return;
        }
    }

    int end = b3MinInt(work->m_Count, work->m_Cursor + PREPARE_BATCH);
    for (; work->m_Cursor < end; ++work->m_Cursor)
    {
        if (work->m_Cursor >= world->contacts.count)
        {
            break;
        }

        b3Contact* contact = world->contacts.data + work->m_Cursor;
        if (contact->contactId != work->m_Cursor || (contact->flags & b3_contactTouchingFlag))
        {
            continue;
        }

        b3Shape* a = world->shapes.data + contact->shapeIdA;
        b3Shape* b = world->shapes.data + contact->shapeIdB;
        if (a->type != b3_hullShape || b->type != b3_hullShape)
        {
            continue;
        }

        b3Body* body_a = world->bodies.data + a->bodyId;
        b3Body* body_b = world->bodies.data + b->bodyId;
        if (body_a->setIndex == b3_awakeSet || body_b->setIndex == b3_awakeSet)
        {
            continue;
        }

        b3Transform pose = b3InvMulWorldTransforms(b3GetBodyTransformQuick(world, body_a), b3GetBodyTransformQuick(world, body_b));
        // Match b3ComputeConvexManifold's input and capacity, including the zero-initialized incoming SAT cache.
        b3LocalManifoldPoint points[32];
        b3LocalManifold manifold = {
            0
        };
        manifold.points = points;
        b3SATCache axis = {
            0
        };
        b3CollideHulls(&manifold, 32, a->hull, b->hull, pose, &axis);
        if (manifold.pointCount <= B3_MAX_MANIFOLD_POINTS)
        {
            PreparedContact* result = &work->m_Prepared[work->m_Cursor];
            StoreQuery(&result->m_Query, a->hull, b->hull, pose, 32, &manifold, axis);
            dmBox3DPrepareHullSupport(&result->m_Support, a->hull, b->hull, pose.q);
            result->m_ContactGeneration = contact->generation;
        }

        if (b3GetMilliseconds(start) >= 1.0f)
        {
            ++work->m_Cursor;
            break;
        }
    }

    if (work->m_Cursor >= b3MinInt(work->m_Count, world->contacts.count))
    {
        work->m_Cursor = -1;
    }
}

// The contact owns its impulses, material, event flags and manifold allocation. Only the local geometry is reusable.
#define b3CollideHulls(manifold, capacity, a, b, pose, axis) CollideContactHulls(world, workerIndex, contact, manifold, capacity, a, b, pose, axis)
#include "../Box3D/src/contact.c"
