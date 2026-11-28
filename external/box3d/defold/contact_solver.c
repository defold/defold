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

#include "../Box3D/src/solver.h"
static void WarmStartContactRange(b3SolverBlock block, b3StepContext* context);
#define b3WarmStartContacts_Convex WarmStartContactRange
#include "../Box3D/src/contact_solver.c"
#undef b3WarmStartContacts_Convex

static bool HasWarmStartImpulse(const b3ContactConstraintWide* c)
{
    if (!b3AllZeroW(c->points[0].normalImpulses))
    {
        return true;
    }

    b3FloatW impulses = b3OrW(c->frictionImpulse.x, c->frictionImpulse.y);
    impulses = b3OrW(impulses, c->twistImpulse);
    impulses = b3OrW(impulses, c->rollingImpulse.X);
    impulses = b3OrW(impulses, c->rollingImpulse.Y);
    impulses = b3OrW(impulses, c->rollingImpulse.Z);
    for (int i = 1; i < B3_MAX_MANIFOLD_POINTS; ++i)
    {
        impulses = b3OrW(impulses, c->points[i].normalImpulses);
    }

    return !b3AllZeroW(impulses);
}

void b3WarmStartContacts_Convex(b3SolverBlock block, b3StepContext* context)
{
    const b3ContactConstraintWide* constraints = context->graph->colors[block.colorIndex].wideConstraints;
    int end = block.startIndex + block.count;
    int first = block.startIndex;
    for (int i = first; i < end; ++i)
    {
        if (HasWarmStartImpulse(&constraints[i]))
        {
            continue;
        }

        block.startIndex = first;
        block.count = (uint16_t)(i - first);
        if (block.count)
        {
            WarmStartContactRange(block, context);
        }

        first = i + 1;
    }

    block.startIndex = first;
    block.count = (uint16_t)(end - first);
    if (block.count)
    {
        WarmStartContactRange(block, context);
    }
}
