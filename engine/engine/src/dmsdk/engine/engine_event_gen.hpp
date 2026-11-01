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
 * | `EVENT_STATE` | `_dm_event_engine_state` | StateEvent. Application focus/iconify changes, successful initialization and engine deletion. |
 * | `EVENT_WINDOW_RESIZED` | `_dm_event_window_resized` | WindowResizedEvent, after the existing window state updates and notifications. One event per native resize callback; no initial-size replay. May precede engine Initialize. |
 * | `EVENT_INPUT_ACTIONS` | `_dm_event_input_actions` | InputActionsEvent. One action view per completed input update, after binding resolution and text-last ordering, before focused collection dispatch. Includes empty updates. |
 *
 * Payloads and their handles are borrowed for the synchronous callback. Engine
 * events identify the source instance. Use dmEngine::GetWindow to obtain its current
 * window; window-specific payloads also identify the window directly. A window
 * handle may be null during early startup or failure cleanup.
 * App state notifications retain their window focus/iconify semantics; they do not aggregate
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
    /*# engine state notification
     * Hash of `_dm_event_engine_state` for dmExtension::BroadcastEvent and dmExtension::FEventCallback.
     * Precomputed dmHashString64 value; no runtime hashing.
     * @constant
     * @name EVENT_STATE
     * @language C++
     */
    static const dmhash_t EVENT_STATE = ENGINE_STATE_EVENT;

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

    /*# engine state notification kind
     * Identifies the notification carried by StateEvent. Focus and iconify
     * transitions are independent of engine initialization and deletion.
     * @enum
     * @name State
     * @language C++
     * @member  STATE_APP_ACTIVATE The engine window gains focus.
     * @member  STATE_APP_DEACTIVATE The engine window loses focus.
     * @member  STATE_APP_ICONIFY The engine window is iconified.
     * @member  STATE_APP_DEICONIFY The engine window is restored.
     * @member  STATE_INITIALIZED Successful engine initialization has completed.
     * @member  STATE_DELETE Engine deletion begins, before collections and systems are torn down. Also delivered after initialization failure if AppInitialize succeeded.
     */
    typedef ::EngineState State;
    static const State STATE_APP_ACTIVATE = ::ENGINE_STATE_APP_ACTIVATE;
    static const State STATE_APP_DEACTIVATE = ::ENGINE_STATE_APP_DEACTIVATE;
    static const State STATE_APP_ICONIFY = ::ENGINE_STATE_APP_ICONIFY;
    static const State STATE_APP_DEICONIFY = ::ENGINE_STATE_APP_DEICONIFY;
    static const State STATE_INITIALIZED = ::ENGINE_STATE_INITIALIZED;
    static const State STATE_DELETE = ::ENGINE_STATE_DELETE;

    /*# resolved input actions
     * Published once per completed input update after application bindings are
     * resolved, coordinates converted and text/marked-text sorted last, before
     * focused game-object dispatch. Empty updates are published; skipped input
     * updates are not. This is independent of extension Update and rendered-frame
     * count. Input events use the generic handler's application lifetime.
     *
     * Iterate the current resolved view through dmInput::IterateActions and the iterator
     * getters. Iteration is immutable and borrowed only for the callback; it cannot
     * consume input or change focused dispatch. Obtain the source engine through
     * dmEngine::GetEngine. Focused gameplay should use native component
     * input/update callbacks.
     * @struct
     * @name InputActionsEvent
     * @language C++
     * @member m_Input [type:dmInput::HContext] valid borrowed input context, including empty updates
     */
    typedef EngineInputActionsEvent InputActionsEvent;

    /*# engine state event payload
     * Payload for EVENT_STATE with data_size equal to sizeof(StateEvent).
     * Use dmEngine::GetWindow to obtain the source engine's current window.
     * @struct
     * @name StateEvent
     * @language C++
     * @member m_Engine [type:HEngine] borrowed source engine; initialization may be incomplete
     * @member m_State [type:State] notification being delivered
     */
    typedef EngineStateEvent StateEvent;

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
/*# engine state notification
 * Hash of `_dm_event_engine_state` for ExtensionBroadcastEvent and FExtensionEventCallback.
 * Precomputed dmHashString64 value; no runtime hashing.
 * @constant
 * @name ENGINE_STATE_EVENT
 * @language C
 */

/*# engine state notification kind
 * Identifies the notification carried by EngineStateEvent. Focus and iconify
 * transitions are independent of engine initialization and deletion.
 * @enum
 * @name EngineState
 * @language C
 * @member  ENGINE_STATE_APP_ACTIVATE The engine window gains focus.
 * @member  ENGINE_STATE_APP_DEACTIVATE The engine window loses focus.
 * @member  ENGINE_STATE_APP_ICONIFY The engine window is iconified.
 * @member  ENGINE_STATE_APP_DEICONIFY The engine window is restored.
 * @member  ENGINE_STATE_INITIALIZED Successful engine initialization has completed.
 * @member  ENGINE_STATE_DELETE Engine deletion begins, before collections and systems are torn down. Also delivered after initialization failure if AppInitialize succeeded.
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
 * @language C
 * @member m_Input [type:HInputContext] valid borrowed input context, including empty updates
 */

/*# engine state event payload
 * Payload for ENGINE_STATE_EVENT with data_size equal to sizeof(EngineStateEvent).
 * Use EngineGetWindow to obtain the source engine's current window.
 * @struct
 * @name EngineStateEvent
 * @language C
 * @member m_Engine [type:HEngine] borrowed source engine; initialization may be incomplete
 * @member m_State [type:EngineState] notification being delivered
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
