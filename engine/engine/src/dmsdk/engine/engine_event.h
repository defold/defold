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
 * | `ENGINE_EVENT_INPUT_ACTIONS` | `_dm_event_input_actions` | EngineInputActionsEvent. One batch per completed input update, after binding resolution and text-last ordering, before focused collection dispatch. Includes empty updates. |
 *
 * Payloads and their handles are borrowed for the synchronous callback. Engine
 * events identify the source instance, and window-driven events identify its
 * window too. A window handle may be null during early startup or failure cleanup.
 * APP events retain their window focus/iconify semantics; they do not aggregate
 * activity across multiple windows or represent separate process lifecycle events.
 *
 * @document
 * @name EngineEvent
 * @language C
 */

/*# The engine window gains focus.
 * Hash of `_dm_event_app_activate` for ExtensionBroadcastEvent and FExtensionEventCallback.
 * Evaluates dmHashString64; this is a hash value, not a legacy ExtensionEventID.
 * @constant
 * @name ENGINE_EVENT_APP_ACTIVATE
 */
#define ENGINE_EVENT_APP_ACTIVATE (dmHashString64("_dm_event_app_activate"))

/*# The engine window loses focus.
 * Hash of `_dm_event_app_deactivate` for ExtensionBroadcastEvent and FExtensionEventCallback.
 * Evaluates dmHashString64; this is a hash value, not a legacy ExtensionEventID.
 * @constant
 * @name ENGINE_EVENT_APP_DEACTIVATE
 */
#define ENGINE_EVENT_APP_DEACTIVATE (dmHashString64("_dm_event_app_deactivate"))

/*# The engine window is iconified.
 * Hash of `_dm_event_app_iconify` for ExtensionBroadcastEvent and FExtensionEventCallback.
 * Evaluates dmHashString64; this is a hash value, not a legacy ExtensionEventID.
 * @constant
 * @name ENGINE_EVENT_APP_ICONIFY
 */
#define ENGINE_EVENT_APP_ICONIFY (dmHashString64("_dm_event_app_iconify"))

/*# The engine window is restored.
 * Hash of `_dm_event_app_deiconify` for ExtensionBroadcastEvent and FExtensionEventCallback.
 * Evaluates dmHashString64; this is a hash value, not a legacy ExtensionEventID.
 * @constant
 * @name ENGINE_EVENT_APP_DEICONIFY
 */
#define ENGINE_EVENT_APP_DEICONIFY (dmHashString64("_dm_event_app_deiconify"))

/*# Successful engine initialization has completed.
 * Hash of `_dm_event_engine_initialized` for ExtensionBroadcastEvent and FExtensionEventCallback.
 * Evaluates dmHashString64; this is a hash value, not a legacy ExtensionEventID.
 * @constant
 * @name ENGINE_EVENT_ENGINE_INITIALIZED
 */
#define ENGINE_EVENT_ENGINE_INITIALIZED (dmHashString64("_dm_event_engine_initialized"))

/*# Engine deletion begins, before collections and systems are torn down.
 * Hash of `_dm_event_engine_delete` for ExtensionBroadcastEvent and FExtensionEventCallback.
 * Evaluates dmHashString64; this is a hash value, not a legacy ExtensionEventID.
 * @constant
 * @name ENGINE_EVENT_ENGINE_DELETE
 */
#define ENGINE_EVENT_ENGINE_DELETE (dmHashString64("_dm_event_engine_delete"))

/*# Window size changed; the payload is EngineWindowResizedEvent.
 * Hash of `_dm_event_window_resized` for ExtensionBroadcastEvent and FExtensionEventCallback.
 * Evaluates dmHashString64; this is a hash value, not a legacy ExtensionEventID.
 * @constant
 * @name ENGINE_EVENT_WINDOW_RESIZED
 */
#define ENGINE_EVENT_WINDOW_RESIZED (dmHashString64("_dm_event_window_resized"))

/*# input actions have been resolved
 * Hash of `_dm_event_input_actions` for ExtensionBroadcastEvent and FExtensionEventCallback.
 * Evaluates dmHashString64; this is a hash value, not a legacy ExtensionEventID.
 * @constant
 * @name ENGINE_EVENT_INPUT_ACTIONS
 */
#define ENGINE_EVENT_INPUT_ACTIONS (dmHashString64("_dm_event_input_actions"))

/*# resolved input action batch
 * Published once per completed input update after application bindings are
 * resolved, coordinates converted and text/marked-text sorted last, before
 * focused game-object dispatch. Empty updates are published; skipped input
 * updates are not. This is independent of extension Update and rendered-frame
 * count. Input events use the generic handler's application lifetime.
 *
 * Read scalar binding state through the input batch/action accessors. Batches
 * and actions are immutable and borrowed only for the callback. Do not retain
 * their handles. Observation cannot consume input or change focused dispatch.
 * Focused gameplay should use native component input/update callbacks.
 * @struct
 * @name EngineInputActionsEvent
 * @member m_Engine [type:HEngine] borrowed source engine
 * @member m_Window [type:HWindow] borrowed engine window, or null if unavailable
 * @member m_Actions [type:HInputActionBatch] valid borrowed batch, including empty updates
 */
typedef struct EngineInputActionsEvent
{
    HEngine           m_Engine;
    HWindow           m_Window;
    HInputActionBatch m_Actions;
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

/*# application window event payload
 * Payload for all four ENGINE_EVENT_APP_* events with data_size equal to
 * sizeof(EngineAppEvent). The event id specifies the focus or iconify transition.
 * @struct
 * @name EngineAppEvent
 * @member m_Engine [type:HEngine] borrowed source engine
 * @member m_Window [type:HWindow] borrowed source window, or null if not yet available
 */
typedef struct EngineAppEvent
{
    HEngine m_Engine;
    HWindow m_Window;
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
