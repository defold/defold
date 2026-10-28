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

#ifndef DM_COMPONENT_RENDER_H
#define DM_COMPONENT_RENDER_H

#include "gameobject.h"

namespace dmGameObject
{
    struct ComponentType;

    /**
     * Game-owner inputs borrowed during capture. The bridge copies render inputs
     * into m_Frame; renderer callbacks must not retain these mutable game pointers.
     * This interface is engine-internal, not part of the extension SDK.
     */
    struct ComponentRenderCaptureParams
    {
        void*       m_World;
        void*       m_Context;
        void*       m_Frame;
        HCollection m_Collection;
    };

    /**
     * Create a game-side lifetime handle after the component's game world is created.
     * This does not initialize renderer-owned mutable state.
     * @param context Registered bridge context
     * @param component_index Actual component registration index
     * @param max_instances Collection instance capacity
     * @param max_components Capacity for this component type
     * @param render_world Output lifetime handle; leave null and clean up partial work on failure
     * @return True on success with a non-null lifetime handle, false on failure
     */
    typedef bool (*FComponentRenderCreateWorld)(void* context, uint32_t component_index, uint32_t max_instances, uint32_t max_components, void** render_world);

    /**
     * Release the game's ownership without waiting for accepted frames to finish.
     * The bridge keeps renderer state alive until all outstanding references are released.
     * @param context Registered bridge context
     * @param render_world Lifetime handle returned by FComponentRenderCreateWorld
     */
    typedef void (*FComponentRenderDeleteWorld)(void* context, void* render_world);

    /**
     * Capture owned render inputs on the game owner.
     * @param context Registered bridge context
     * @param render_world Lifetime handle associated with this component world
     * @param params Borrowed game inputs and the current capture frame
     * @return True on success; false rejects the frame capture
     */
    typedef bool (*FComponentRenderCapture)(void* context, void* render_world, const ComponentRenderCaptureParams& params);

    /**
     * Game-owner bridge callbacks, separate from the component's existing world hooks.
     * All callbacks are required. The context must outlive all worlds using it.
     */
    struct ComponentRenderCallbacks
    {
        void*                       m_Context;
        FComponentRenderCreateWorld m_CreateWorld;
        FComponentRenderDeleteWorld m_DeleteWorld;
        FComponentRenderCapture     m_Capture;
    };

    /**
     * Register bridge callbacks before any collection starts creating worlds.
     * Does not change ComponentType or the public extension registration API.
     * @param regist Component register
     * @param component_index Actual registration-array index, not the resource type index
     * @param callbacks Required bridge callbacks and their context
     * @return RESULT_OK, RESULT_ALREADY_REGISTERED for duplicates, or RESULT_INVALID_OPERATION
     *         for invalid callbacks/index or registration after world creation has begun
     */
    Result RegisterComponentRenderCallbacks(HRegister regist, uint32_t component_index, const ComponentRenderCallbacks& callbacks);

    /**
     * Register a component type and its bridge callbacks together, before world creation.
     * Failure adds neither a component nor callbacks; existing registrations are unchanged.
     * @param regist Component register
     * @param type Game-side component type
     * @param callbacks Required bridge callbacks and their context
     * @return RESULT_OK, or a component/bridge registration error
     */
    Result RegisterComponentType(HRegister regist, const ComponentType& type, const ComponentRenderCallbacks& callbacks);

    /**
     * Capture render inputs on the game owner after final transforms and before destructive PostUpdate.
     * Uses Render's existing ordering and enabled proxy traversal. Recursive Render calls
     * inherit the frame; nested CaptureRenderData calls are rejected.
     * @param collection Collection to render/capture
     * @param frame Non-null capture frame, valid throughout this synchronous call
     * @return True if the traversal succeeds
     */
    bool CaptureRenderData(HCollection collection, void* frame);
}

#endif // DM_COMPONENT_RENDER_H
