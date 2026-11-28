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

// Hull clipping and face separation calculations are adapted from Box3D.
// Copyright 2026 Erin Catto. See ../Box3D/LICENSE for the MIT license.
#include "../Box3D/src/manifold.h"
#include "../Box3D/src/math_internal.h"
#include <string.h>

static int ClipHullFace(b3ClipVertex** output, b3ClipVertex** input, int count, b3Plane plane, int edge, b3Plane reference)
{
    B3_ASSERT(count >= 3);
    b3ClipVertex* source = *input;
    b3ClipVertex* dest = *output;
    int first = 0;
    float previous_distance = b3PlaneSeparation(plane, source[count - 1].position);
    float distance = b3PlaneSeparation(plane, source[0].position);
    if (previous_distance <= 0.0f)
    {
        while (distance <= 0.0f)
        {
            if (++first == count)
            {
                // The caller swaps buffers after each plane. Leave an unchanged polygon in its original buffer.
                *input = dest;
                *output = source;
                B3_VALIDATE(b3ValidatePolygon(source, count));
                return count;
            }

            previous_distance = distance;
            distance = b3PlaneSeparation(plane, source[first].position);
        }
    }

    // Edges up to the first crossing survived unchanged. Finish clipping using the distances already computed.
    int written = first;
    if (first)
    {
        memcpy(dest, source, first * sizeof(*dest));
    }

    const b3ClipVertex* previous = &source[first ? first - 1 : count - 1];
    for (int i = first; i < count; ++i)
    {
        const b3ClipVertex* vertex = &source[i];
        bool leaving = previous_distance <= 0.0f && distance > 0.0f;
        bool entering = previous_distance > 0.0f && distance <= 0.0f;
        if (leaving || entering)
        {
            b3ClipVertex* cross = &dest[written++];
            float fraction = previous_distance / (previous_distance - distance);
            cross->position = b3MulAdd(previous->position, fraction, b3Sub(vertex->position, previous->position));
            cross->separation = b3PlaneSeparation(reference, cross->position);
            cross->pair = leaving ? vertex->pair : previous->pair;
            if (leaving)
            {
                cross->pair.owner2 = b3_featureShapeA;
                cross->pair.index2 = (uint8_t)edge;
            }
            else
            {
                cross->pair.owner1 = b3_featureShapeA;
                cross->pair.index1 = (uint8_t)edge;
            }
        }

        if (distance <= 0.0f && (previous_distance <= 0.0f || entering))
        {
            dest[written++] = *vertex;
        }

        previous = vertex;
        previous_distance = distance;
        if (i + 1 < count)
        {
            distance = b3PlaneSeparation(plane, source[i + 1].position);
        }
    }

    B3_VALIDATE(b3ValidatePolygon(dest, written));
    return written;
}

#define b3ClipPolygon(output, input, count, plane, edge, reference) ClipHullFace(&(output), &(input), count, plane, edge, reference)
#include "../Box3D/src/convex_manifold.c"

#include "hull_support.h"

static void GetSupportingVertices(uint8_t* vertices, const b3HullData* faces, const b3HullData* hull, b3Matrix3 rotation)
{
    int stride = (hull->vertexCount + 3) & ~3;
    const float* x = b3GetHullSoaVertices(hull);
    b3Vec3 center = b3AABB_Center(hull->aabb);
    b3Vec3 extents = b3AABB_Extents(hull->aabb);
    const b3Plane* planes = b3GetHullPlanes(faces);
    for (int i = 0; i < faces->faceCount; ++i)
    {
        b3Vec3 direction = b3Neg(b3MulMV(rotation, planes[i].normal));
        float bias = b3Dot(direction, center) + 1.0625f * b3Dot(b3Abs(direction), extents);
        float support;
        int index;
        b3GetSupportWide(direction, x, x + stride, x + 2 * stride, stride, bias, &support, &index);
        vertices[i] = (uint8_t)index;
    }
}

static bool SharesPrismAxis(b3Quat rotation, int axis)
{
    const float v[3] = {
        rotation.v.x, rotation.v.y, rotation.v.z
    };
    return v[(axis + 1) % 3] == 0.0f && v[(axis + 2) % 3] == 0.0f;
}

void dmBox3DPrepareHullSupport(dmBox3DHullSupport* support, const b3HullData* a, const b3HullData* b, b3Quat rotation)
{
    support->m_PrismAxisPlusOne = 0;
    if (a->faceCount > DM_PREPARED_FACE_LIMIT || b->faceCount > DM_PREPARED_FACE_LIMIT)
    {
        return;
    }

    for (int i = 0; i < 3; ++i)
    {
        int axis = (i + 1) % 3;
        if (!SharesPrismAxis(rotation, axis) || !b3IsRightPrism(a, axis) || (a != b && !b3IsRightPrism(b, axis)))
        {
            continue;
        }

        b3Matrix3 r = b3MakeMatrixFromQuat(rotation);
        GetSupportingVertices(support->m_OnB, a, b, b3Transpose(r));
        if (a == b && rotation.v.x == 0 && rotation.v.y == 0 && rotation.v.z == 0)
        {
            memcpy(support->m_OnA, support->m_OnB, a->faceCount);
        }
        else
        {
            GetSupportingVertices(support->m_OnA, b, a, r);
        }

        support->m_PrismAxisPlusOne = (uint8_t)(axis + 1);
        return;
    }
}

bool dmBox3DCollidePreparedHulls(const dmBox3DHullSupport* support, b3Quat prepared_rotation, b3LocalManifold* manifold,
                                 int capacity, const b3HullData* a, const b3HullData* b, b3Transform pose, b3SATCache* axis)
{
    if (capacity < B3_MAX_MANIFOLD_POINTS || !support->m_PrismAxisPlusOne || !SharesPrismAxis(pose.q, support->m_PrismAxisPlusOne - 1))
    {
        return false;
    }

    b3Matrix3 r = b3MakeMatrixFromQuat(pose.q);
    b3Matrix3 previous = b3MakeMatrixFromQuat(prepared_rotation);
    bool was_identity = prepared_rotation.v.x == 0 && prepared_rotation.v.y == 0 && prepared_rotation.v.z == 0;
    bool is_identity = pose.q.v.x == 0 && pose.q.v.y == 0 && pose.q.v.z == 0;
    if (memcmp(&r, &previous, sizeof(r)) != 0 || (a == b && was_identity != is_identity))
    {
        return false;
    }

    // With a shared prism axis, face normals cover all separating axes. Retain Box3D's face order,
    // support-vertex tie breaks and float operation order. Translation changes only the plane separations.
    const b3HullData* hulls[2] = {
        a, b
    };
    const uint8_t* indices[2] = {
        support->m_OnB, support->m_OnA
    };
    b3SeparatingAxis best[2] = {
        0
    };
    b3Matrix3 inverse = b3Transpose(r);
    manifold->pointCount = 0;
    float distance = B3_SPECULATIVE_DISTANCE;
    for (int side = 0; side < 2; ++side)
    {
        const b3Plane* planes = b3GetHullPlanes(hulls[side]);
        const b3Vec3* points = b3GetHullPoints(hulls[1 - side]);
        b3SeparatingAxis* face = best + side;
        face->type = side ? b3_faceAxisB : b3_faceAxisA;
        face->separation = -INFINITY;
        for (int i = 0; i < hulls[side]->faceCount; ++i)
        {
            b3Vec3 direction = b3Neg(b3MulMV(side ? r : inverse, planes[i].normal));
            b3Vec3 normal = side ? direction : planes[i].normal;
            int vertex = indices[side][i];
            float separation = (b3Dot(normal, pose.p) - planes[i].offset) - b3Dot(direction, points[vertex]);
            if (separation > face->separation)
            {
                face->normal = normal;
                face->separation = separation;
                face->indexA = side ? vertex : i;
                face->indexB = side ? i : vertex;
                if (separation > distance)
                {
                    axis->type = (uint8_t)face->type;
                    axis->separation = separation;
                    axis->indexA = (uint8_t)face->indexA;
                    axis->indexB = (uint8_t)face->indexB;
                    manifold->pointCount = 0;
                    return true;
                }
            }
        }
    }

    if (best[0].separation > best[1].separation)
    {
        b3BuildFaceAContact(manifold, capacity, a, b, pose, best[0], axis);
    }
    else
    {
        b3BuildFaceBContact(manifold, capacity, a, b, pose, best[1], axis);
    }

    return true;
}
