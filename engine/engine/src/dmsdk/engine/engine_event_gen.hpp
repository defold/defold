// Generated, do not edit!
// Generated from sdk_gen.json (C++ API).

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

#ifndef DMSDK_ENGINE_EVENT_GEN_HPP
#define DMSDK_ENGINE_EVENT_GEN_HPP

#if !defined(__cplusplus)
   #error "This file is supported in C++ only!"
#endif


/*# Engine event definitions
 *
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
 * | `EVENT_APP_ACTIVATE` | `_dm_event_app_activate` | AppEvent. The engine window gains focus. |
 * | `EVENT_APP_DEACTIVATE` | `_dm_event_app_deactivate` | AppEvent. The engine window loses focus. |
 * | `EVENT_APP_ICONIFY` | `_dm_event_app_iconify` | AppEvent. The engine window is iconified. |
 * | `EVENT_APP_DEICONIFY` | `_dm_event_app_deiconify` | AppEvent. The engine window is restored. |
 * | `EVENT_ENGINE_INITIALIZED` | `_dm_event_engine_initialized` | LifecycleEvent. Successful engine initialization has completed. |
 * | `EVENT_ENGINE_DELETE` | `_dm_event_engine_delete` | LifecycleEvent. Engine deletion begins, before collections and systems are torn down. Also delivered after initialization failure if AppInitialize succeeded. |
 * | `EVENT_WINDOW_RESIZED` | `_dm_event_window_resized` | WindowResizedEvent, after the existing window state updates and notifications. One event per native resize callback; no initial-size replay. May precede engine Initialize. |
 * | `EVENT_INPUT_ACTIONS` | `_dm_event_input_actions` | InputActionsEvent. One batch per completed input update, after binding resolution and text-last ordering, before focused collection dispatch. Includes empty updates. |
 *
 * Payloads and their handles are borrowed for the synchronous callback. Engine
 * events identify the source instance. Use dmEngine::GetWindow to obtain its current
 * window; window-specific payloads also identify the window directly. A window
 * handle may be null during early startup or failure cleanup.
 * APP events retain their window focus/iconify semantics; they do not aggregate
 * activity across multiple windows or represent separate process lifecycle events.
 *
 * @document
 * @language C++
 * @name EngineEvent
 * @namespace dmEngine
 * @path engine/engine/src/dmsdk/engine/engine_event_gen.hpp
 */

#include <dmsdk/dlib/hash.h>
#include <dmsdk/engine/engine.h>
#include <dmsdk/platform/window.h>
#include <dmsdk/input/input.h>

#include <dmsdk/engine/engine_event.h>

namespace dmEngine
{
    /*# The engine window gains focus.
     * Hash of `_dm_event_app_activate` for dmExtension::BroadcastEvent and dmExtension::FEventCallback.
     * Precomputed dmHashString64 value; no runtime hashing.
     * @constant
     * @name EVENT_APP_ACTIVATE
     * @language C++
     */
    static const dmhash_t EVENT_APP_ACTIVATE = ENGINE_EVENT_APP_ACTIVATE;

    /*# The engine window loses focus.
     * Hash of `_dm_event_app_deactivate` for dmExtension::BroadcastEvent and dmExtension::FEventCallback.
     * Precomputed dmHashString64 value; no runtime hashing.
     * @constant
     * @name EVENT_APP_DEACTIVATE
     * @language C++
     */
    static const dmhash_t EVENT_APP_DEACTIVATE = ENGINE_EVENT_APP_DEACTIVATE;

    /*# The engine window is iconified.
     * Hash of `_dm_event_app_iconify` for dmExtension::BroadcastEvent and dmExtension::FEventCallback.
     * Precomputed dmHashString64 value; no runtime hashing.
     * @constant
     * @name EVENT_APP_ICONIFY
     * @language C++
     */
    static const dmhash_t EVENT_APP_ICONIFY = ENGINE_EVENT_APP_ICONIFY;

    /*# The engine window is restored.
     * Hash of `_dm_event_app_deiconify` for dmExtension::BroadcastEvent and dmExtension::FEventCallback.
     * Precomputed dmHashString64 value; no runtime hashing.
     * @constant
     * @name EVENT_APP_DEICONIFY
     * @language C++
     */
    static const dmhash_t EVENT_APP_DEICONIFY = ENGINE_EVENT_APP_DEICONIFY;

    /*# Successful engine initialization has completed.
     * Hash of `_dm_event_engine_initialized` for dmExtension::BroadcastEvent and dmExtension::FEventCallback.
     * Precomputed dmHashString64 value; no runtime hashing.
     * @constant
     * @name EVENT_ENGINE_INITIALIZED
     * @language C++
     */
    static const dmhash_t EVENT_ENGINE_INITIALIZED = ENGINE_EVENT_ENGINE_INITIALIZED;

    /*# Engine deletion begins, before collections and systems are torn down.
     * Hash of `_dm_event_engine_delete` for dmExtension::BroadcastEvent and dmExtension::FEventCallback.
     * Precomputed dmHashString64 value; no runtime hashing.
     * @constant
     * @name EVENT_ENGINE_DELETE
     * @language C++
     */
    static const dmhash_t EVENT_ENGINE_DELETE = ENGINE_EVENT_ENGINE_DELETE;

    /*# Window size changed; the payload is WindowResizedEvent.
     * Hash of `_dm_event_window_resized` for dmExtension::BroadcastEvent and dmExtension::FEventCallback.
     * Precomputed dmHashString64 value; no runtime hashing.
     * @constant
     * @name EVENT_WINDOW_RESIZED
     * @language C++
     */
    static const dmhash_t EVENT_WINDOW_RESIZED = ENGINE_EVENT_WINDOW_RESIZED;

    /*# input actions have been resolved
     * Hash of `_dm_event_input_actions` for dmExtension::BroadcastEvent and dmExtension::FEventCallback.
     * Precomputed dmHashString64 value; no runtime hashing.
     * @constant
     * @name EVENT_INPUT_ACTIONS
     * @language C++
     */
    static const dmhash_t EVENT_INPUT_ACTIONS = ENGINE_EVENT_INPUT_ACTIONS;

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
     * @name InputActionsEvent
     * @language C++
     * @member m_Engine [type:HEngine] borrowed source engine
     * @member m_Window [type:HWindow] borrowed engine window, or null if unavailable
     * @member m_Actions [type:dmInput::HActionBatch] valid borrowed batch, including empty updates
     */
    typedef EngineInputActionsEvent InputActionsEvent;

    /*# engine lifecycle event payload
     * Payload for EVENT_ENGINE_INITIALIZED and EVENT_ENGINE_DELETE
     * with data_size equal to sizeof(LifecycleEvent).
     * @struct
     * @name LifecycleEvent
     * @language C++
     * @member m_Engine [type:HEngine] borrowed source engine; initialization may be incomplete
     */
    typedef EngineLifecycleEvent LifecycleEvent;

    /*# application event payload
     * Payload for all four EVENT_APP_* events with data_size equal to
     * sizeof(AppEvent). The event id specifies the focus or iconify transition.
     * Use dmEngine::GetWindow to obtain the source engine's current window.
     * @struct
     * @name AppEvent
     * @language C++
     * @member m_Engine [type:HEngine] borrowed source engine
     */
    typedef EngineAppEvent AppEvent;

    /*# window resized event payload
     * Payload for `EVENT_WINDOW_RESIZED` with data_size equal to
     * `sizeof(WindowResizedEvent)`.
     * @struct
     * @name WindowResizedEvent
     * @language C++
     * @member m_Engine [type:HEngine] borrowed source engine
     * @member m_Window [type:HWindow] borrowed source window, or null if not yet available
     * @member m_Width [type:uint32_t] physical framebuffer width in pixels
     * @member m_Height [type:uint32_t] physical framebuffer height in pixels
     */
    typedef EngineWindowResizedEvent WindowResizedEvent;


} // namespace dmEngine

#endif // #define DMSDK_ENGINE_EVENT_GEN_HPP
/*# The engine window gains focus.
 * Hash of `_dm_event_app_activate` for ExtensionBroadcastEvent and FExtensionEventCallback.
 * Precomputed dmHashString64 value; no runtime hashing.
 * @constant
 * @name ENGINE_EVENT_APP_ACTIVATE
 * @language C
 */

/*# The engine window loses focus.
 * Hash of `_dm_event_app_deactivate` for ExtensionBroadcastEvent and FExtensionEventCallback.
 * Precomputed dmHashString64 value; no runtime hashing.
 * @constant
 * @name ENGINE_EVENT_APP_DEACTIVATE
 * @language C
 */

/*# The engine window is iconified.
 * Hash of `_dm_event_app_iconify` for ExtensionBroadcastEvent and FExtensionEventCallback.
 * Precomputed dmHashString64 value; no runtime hashing.
 * @constant
 * @name ENGINE_EVENT_APP_ICONIFY
 * @language C
 */

/*# The engine window is restored.
 * Hash of `_dm_event_app_deiconify` for ExtensionBroadcastEvent and FExtensionEventCallback.
 * Precomputed dmHashString64 value; no runtime hashing.
 * @constant
 * @name ENGINE_EVENT_APP_DEICONIFY
 * @language C
 */

/*# Successful engine initialization has completed.
 * Hash of `_dm_event_engine_initialized` for ExtensionBroadcastEvent and FExtensionEventCallback.
 * Precomputed dmHashString64 value; no runtime hashing.
 * @constant
 * @name ENGINE_EVENT_ENGINE_INITIALIZED
 * @language C
 */

/*# Engine deletion begins, before collections and systems are torn down.
 * Hash of `_dm_event_engine_delete` for ExtensionBroadcastEvent and FExtensionEventCallback.
 * Precomputed dmHashString64 value; no runtime hashing.
 * @constant
 * @name ENGINE_EVENT_ENGINE_DELETE
 * @language C
 */

/*# Window size changed; the payload is EngineWindowResizedEvent.
 * Hash of `_dm_event_window_resized` for ExtensionBroadcastEvent and FExtensionEventCallback.
 * Precomputed dmHashString64 value; no runtime hashing.
 * @constant
 * @name ENGINE_EVENT_WINDOW_RESIZED
 * @language C
 */

/*# input actions have been resolved
 * Hash of `_dm_event_input_actions` for ExtensionBroadcastEvent and FExtensionEventCallback.
 * Precomputed dmHashString64 value; no runtime hashing.
 * @constant
 * @name ENGINE_EVENT_INPUT_ACTIONS
 * @language C
 */

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
 * @language C
 * @member m_Engine [type:HEngine] borrowed source engine
 * @member m_Window [type:HWindow] borrowed engine window, or null if unavailable
 * @member m_Actions [type:HInputActionBatch] valid borrowed batch, including empty updates
 */

/*# engine lifecycle event payload
 * Payload for ENGINE_EVENT_ENGINE_INITIALIZED and ENGINE_EVENT_ENGINE_DELETE
 * with data_size equal to sizeof(EngineLifecycleEvent).
 * @struct
 * @name EngineLifecycleEvent
 * @language C
 * @member m_Engine [type:HEngine] borrowed source engine; initialization may be incomplete
 */

/*# application event payload
 * Payload for all four ENGINE_EVENT_APP_* events with data_size equal to
 * sizeof(EngineAppEvent). The event id specifies the focus or iconify transition.
 * Use EngineGetWindow to obtain the source engine's current window.
 * @struct
 * @name EngineAppEvent
 * @language C
 * @member m_Engine [type:HEngine] borrowed source engine
 */

/*# window resized event payload
 * Payload for `ENGINE_EVENT_WINDOW_RESIZED` with data_size equal to
 * `sizeof(EngineWindowResizedEvent)`.
 * @struct
 * @name EngineWindowResizedEvent
 * @language C
 * @member m_Engine [type:HEngine] borrowed source engine
 * @member m_Window [type:HWindow] borrowed source window, or null if not yet available
 * @member m_Width [type:uint32_t] physical framebuffer width in pixels
 * @member m_Height [type:uint32_t] physical framebuffer height in pixels
 */
