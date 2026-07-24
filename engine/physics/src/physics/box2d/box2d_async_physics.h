// Copyright 2020-2026 The Defold Foundation
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

#ifndef DM_BOX2D_ASYNC_PHYSICS_H
#define DM_BOX2D_ASYNC_PHYSICS_H

#include "box2d_physics.h"

// Orchestration for the double-buffered async path. box2d_physics.cpp stays the synchronous
// backend; when World2D::m_UseDoubleBufferedWorlds is set it mirrors structural mutations into a
// per-world operation queue that this module drains against the physics world (m_PhysicsWorldId).
namespace dmPhysics
{
    // Queue an OP_CREATE_BODY that reproduces a just-created game body (def + collision data +
    // the body's already-transformed shapes) in the physics world. The owning Body receives its
    // physics-world twin id when the queue is drained. Grid shapes are not yet implemented (skipped by the
    // operation queue).
    void EnqueueCreateBody(World2D* world, const b2BodyDef& def, const CollisionObjectData& data, Body* owner);

    // Queue an OP_DESTROY_BODY for a body's physics-world twin. If the twin has not been created
    // (same-frame create then destroy, before a drain), the pending create is cancelled instead
    // so no orphan body is left in the physics world.
    void EnqueueDestroyBody(World2D* world, Body* owner);

    // Apply all queued operations to the physics world, writing each created twin id back onto its
    // owning Body, then clear the queue. Runs on the main thread at the frame-start safe point.
    void DrainPendingOps(World2D* world);
}

#endif // DM_BOX2D_ASYNC_PHYSICS_H
