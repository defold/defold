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

#ifndef DMSDK_ENGINE_EVENT_H
#define DMSDK_ENGINE_EVENT_H

#include <dmsdk/dlib/hash.h>
#include <dmsdk/engine/engine.h>
#include <dmsdk/platform/window.h>
#include <dmsdk/input/input.h>

/*# Engine event definitions
 * Built-in event hashes and payloads published through the generic extension
 * event transport in `<dmsdk/extension/extension.h>`. Custom events may use
 * arbitrary name hashes and payloads without central registration.
 *
 * Built-in IDs are precomputed dmHashString64 values and integer constant
 * expressions in C and C++.
 *
 * Built-in event hashes and their names:
 *
 * | Constant | Name | Payload and delivery |
 * | -------- | ---- | -------------------- |
 * | `ENGINE_EVENT_APP_ACTIVATE` | `_dm_event_app_activate` | EngineAppEvent. The engine window gains focus. |
 * | `ENGINE_EVENT_APP_DEACTIVATE` | `_dm_event_app_deactivate` | EngineAppEvent. The engine window loses focus. |
 * | `ENGINE_EVENT_APP_ICONIFY` | `_dm_event_app_iconify` | EngineAppEvent. The engine window is iconified. |
 * | `ENGINE_EVENT_APP_DEICONIFY` | `_dm_event_app_deiconify` | EngineAppEvent. The engine window is restored. |
 * | `ENGINE_EVENT_ENGINE_INITIALIZED` | `_dm_event_engine_initialized` | EngineLifecycleEvent. Successful engine initialization has completed. |
 * | `ENGINE_EVENT_ENGINE_DELETE` | `_dm_event_engine_delete` | EngineLifecycleEvent. Engine deletion begins, before collections and systems are torn down. Also delivered after initialization failure if AppInitialize succeeded. |
 * | `ENGINE_EVENT_WINDOW_RESIZED` | `_dm_event_window_resized` | EngineWindowResizedEvent, after the existing window state updates and notifications. One event per native resize callback; no initial-size replay. May precede engine Initialize. |
 * | `ENGINE_EVENT_INPUT_ACTIONS` | `_dm_event_input_actions` | EngineInputActionsEvent. One action view per completed input update, after binding resolution and text-last ordering, before focused collection dispatch. Includes empty updates. |
 *
 * Payloads and their handles are borrowed for the synchronous callback. Engine
 * events identify the source instance. Use EngineGetWindow to obtain its current
 * window; window-specific payloads also identify the window directly. A window
 * handle may be null during early startup or failure cleanup.
 * APP events retain their window focus/iconify semantics; they do not aggregate
 * activity across multiple windows or represent separate process lifecycle events.
 *
 * @document
 * @name EngineEvent
 * @language C
 */

/*# The engine window gains focus.
 * Hash of `_dm_event_app_activate` for ExtensionBroadcastEvent and FExtensionEventCallback.
 * Precomputed dmHashString64 value; no runtime hashing.
 * @constant
 * @name ENGINE_EVENT_APP_ACTIVATE
 */
#define ENGINE_EVENT_APP_ACTIVATE ((dmhash_t)0xbe4d4370ee0c2abeULL)

/*# The engine window loses focus.
 * Hash of `_dm_event_app_deactivate` for ExtensionBroadcastEvent and FExtensionEventCallback.
 * Precomputed dmHashString64 value; no runtime hashing.
 * @constant
 * @name ENGINE_EVENT_APP_DEACTIVATE
 */
#define ENGINE_EVENT_APP_DEACTIVATE ((dmhash_t)0x695861ad27760b07ULL)

/*# The engine window is iconified.
 * Hash of `_dm_event_app_iconify` for ExtensionBroadcastEvent and FExtensionEventCallback.
 * Precomputed dmHashString64 value; no runtime hashing.
 * @constant
 * @name ENGINE_EVENT_APP_ICONIFY
 */
#define ENGINE_EVENT_APP_ICONIFY ((dmhash_t)0x4adf48e23070b20dULL)

/*# The engine window is restored.
 * Hash of `_dm_event_app_deiconify` for ExtensionBroadcastEvent and FExtensionEventCallback.
 * Precomputed dmHashString64 value; no runtime hashing.
 * @constant
 * @name ENGINE_EVENT_APP_DEICONIFY
 */
#define ENGINE_EVENT_APP_DEICONIFY ((dmhash_t)0x74b60ff5566fd9d6ULL)

/*# Successful engine initialization has completed.
 * Hash of `_dm_event_engine_initialized` for ExtensionBroadcastEvent and FExtensionEventCallback.
 * Precomputed dmHashString64 value; no runtime hashing.
 * @constant
 * @name ENGINE_EVENT_ENGINE_INITIALIZED
 */
#define ENGINE_EVENT_ENGINE_INITIALIZED ((dmhash_t)0x1a23d009fce5c274ULL)

/*# Engine deletion begins, before collections and systems are torn down.
 * Hash of `_dm_event_engine_delete` for ExtensionBroadcastEvent and FExtensionEventCallback.
 * Precomputed dmHashString64 value; no runtime hashing.
 * @constant
 * @name ENGINE_EVENT_ENGINE_DELETE
 */
#define ENGINE_EVENT_ENGINE_DELETE ((dmhash_t)0x18a5362db07c4597ULL)

/*# Window size changed; the payload is EngineWindowResizedEvent.
 * Hash of `_dm_event_window_resized` for ExtensionBroadcastEvent and FExtensionEventCallback.
 * Precomputed dmHashString64 value; no runtime hashing.
 * @constant
 * @name ENGINE_EVENT_WINDOW_RESIZED
 */
#define ENGINE_EVENT_WINDOW_RESIZED ((dmhash_t)0x7dc2351509a8562eULL)

/*# input actions have been resolved
 * Hash of `_dm_event_input_actions` for ExtensionBroadcastEvent and FExtensionEventCallback.
 * Precomputed dmHashString64 value; no runtime hashing.
 * @constant
 * @name ENGINE_EVENT_INPUT_ACTIONS
 */
#define ENGINE_EVENT_INPUT_ACTIONS ((dmhash_t)0x9075043efcdbe0e2ULL)

/*# resolved input actions
 * Published once per completed input update after application bindings are
 * resolved, coordinates converted and text/marked-text sorted last, before
 * focused game-object dispatch. Empty updates are published; skipped input
 * updates are not. This is independent of extension Update and rendered-frame
 * count. Input events use the generic handler's application lifetime.
 *
 * Iterate the current resolved view through InputIterateActions and the iterator
 * getters. Iteration is immutable and borrowed only for the callback; it cannot
 * consume input or change focused dispatch. Obtain the source engine through
 * EngineGetFromExtensionParams. Focused gameplay should use native component
 * input/update callbacks.
 * @struct
 * @name EngineInputActionsEvent
 * @member m_Input [type:HInputContext] valid borrowed input context, including empty updates
 */
typedef struct EngineInputActionsEvent
{
    HInputContext m_Input;
} EngineInputActionsEvent;

/*# engine lifecycle event payload
 * Payload for ENGINE_EVENT_ENGINE_INITIALIZED and ENGINE_EVENT_ENGINE_DELETE
 * with data_size equal to sizeof(EngineLifecycleEvent).
 * @struct
 * @name EngineLifecycleEvent
 * @member m_Engine [type:HEngine] borrowed source engine; initialization may be incomplete
 */
typedef struct EngineLifecycleEvent
{
    HEngine m_Engine;
} EngineLifecycleEvent;

/*# application event payload
 * Payload for all four ENGINE_EVENT_APP_* events with data_size equal to
 * sizeof(EngineAppEvent). The event id specifies the focus or iconify transition.
 * Use EngineGetWindow to obtain the source engine's current window.
 * @struct
 * @name EngineAppEvent
 * @member m_Engine [type:HEngine] borrowed source engine
 */
typedef struct EngineAppEvent
{
    HEngine m_Engine;
} EngineAppEvent;

/*# window resized event payload
 * Payload for `ENGINE_EVENT_WINDOW_RESIZED` with data_size equal to
 * `sizeof(EngineWindowResizedEvent)`.
 * @struct
 * @name EngineWindowResizedEvent
 * @member m_Engine [type:HEngine] borrowed source engine
 * @member m_Window [type:HWindow] borrowed source window, or null if not yet available
 * @member m_Width [type:uint32_t] physical framebuffer width in pixels
 * @member m_Height [type:uint32_t] physical framebuffer height in pixels
 */
typedef struct EngineWindowResizedEvent
{
    HEngine  m_Engine;
    HWindow  m_Window;
    uint32_t m_Width;
    uint32_t m_Height;
} EngineWindowResizedEvent;

#if defined(__cplusplus)
#include "engine_event.hpp"
#endif

#endif // DMSDK_ENGINE_EVENT_H
