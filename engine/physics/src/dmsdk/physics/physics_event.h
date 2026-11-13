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

#ifndef DMSDK_PHYSICS_EVENT_H
#define DMSDK_PHYSICS_EVENT_H

#include <dmsdk/dlib/hash.h>

/*# Physics events
 * Native physics-world notifications delivered through the extension broadcast.
 * Defold owns simulation, collision objects and transform synchronization.
 * @document
 * @name Physics events
 * @language C
 */

/*# physics world step completed
 * Event identifier for PhysicsEvent notifications after each Defold physics-world
 * update. Delivery is synchronous on the engine thread after native stepping and
 * game-object world-transform propagation, before the collection's queued message
 * dispatch. Fixed updates publish once per completed world update; paused or skipped
 * world updates do not publish. A backend can perform zero or multiple internal
 * simulation substeps in one world update, as Bullet does with fixed substeps.
 * These substeps do not publish separate events. m_Dt carries the supplied world
 * update interval; the amount of native simulation time advanced can differ.
 * @constant
 * @name PHYSICS_EVENT_POST_STEP
 */
#define PHYSICS_EVENT_POST_STEP ((dmhash_t)0x314bbdd060083848ULL) // DM_HASH: dmHashString64("_dm_event_physics_post_step")

/*# native physics world event
 * Payload for PHYSICS_EVENT_POST_STEP with data_size equal to sizeof(PhysicsEvent).
 * Interpret m_World using the physics backend configured for the project and build.
 * Legacy Defold Box2D supplies a b2World pointer, Box2D 3 a pointer to a b2WorldId
 * value, and Bullet a btDiscreteDynamicsWorld pointer. For Box2D 3, copy the
 * b2WorldId value from the supplied pointer. Native headers and libraries must
 * match the backend bundled with this SDK.
 *
 * The payload and native world access are borrowed only during the callback.
 * Read native event buffers and perform queries using that backend's own API.
 * Do not step or destroy the world, destroy Defold-managed bodies, replace reserved
 * user data or callbacks, or invalidate event buffers for other listeners. Defer
 * destructive changes through Defold's normal lifecycle operations. Delivery order
 * between extensions is unspecified. This event cannot change contact response
 * for the completed step; that requires the backend's own pre-solve mechanism.
 * @struct
 * @name PhysicsEvent
 * @member m_World [type:const void*] borrowed native world access for the configured physics backend
 * @member m_Dt [type:float] time-scaled world update interval in seconds; native simulated time can differ
 */
typedef struct PhysicsEvent
{
    const void* m_World;
    float       m_Dt;
} PhysicsEvent;

#endif // DMSDK_PHYSICS_EVENT_H
