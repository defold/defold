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
 * | `EVENT_STATE` | `_dm_event_engine_state` | StateEvent. Successful initialization and engine deletion. |
 * | `EVENT_WINDOW` | `_dm_event_engine_window` | WindowEvent. Resize, minimize/restore and focus notifications from the window callbacks. May precede engine Initialize. |
 * | `EVENT_INPUT_ACTIONS` | `_dm_event_input_actions` | InputActionsEvent. One action batch per completed input update, after binding resolution and text-last ordering, before focused collection dispatch. Includes empty updates. |
 *
 * Payloads and their handles are borrowed for the synchronous callback. Obtain
 * the source engine from the callback registry using ENGINE_CONTEXT_NAME.
 * Get the registry with dmExtension::ParamsGetContextRegistry, then use ContextRegistryGet
 * and cast the result to HEngine. Use dmEngine::GetWindow for its current window.
 * Window payloads identify the source window directly. A window
 * handle may be null during early startup or failure cleanup.
 * Window notifications retain the backend focus/minimize semantics. Minimization
 * is distinct from hide/show visibility; platform callback availability is unchanged.
 *
 * @document
 * @language C++
 * @name EngineEvent
 * @namespace dmEngine
 * @path engine/engine/src/dmsdk/engine/engine_event_gen.hpp
 */

extern "C" {
#include <dmsdk/dlib/hash.h>
#include <dmsdk/engine/engine.h>
#include <dmsdk/platform/window.h>
#include <dmsdk/input/input.h>

#include <dmsdk/engine/engine_event.h>
}

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

    /*# engine window notification
     * Hash of `_dm_event_engine_window` for dmExtension::BroadcastEvent and dmExtension::FEventCallback.
     * Precomputed dmHashString64 value; no runtime hashing.
     * @constant
     * @name EVENT_WINDOW
     * @language C++
     */
    static const dmhash_t EVENT_WINDOW = ENGINE_WINDOW_EVENT;

    /*# input actions have been resolved
     * Hash of `_dm_event_input_actions` for dmExtension::BroadcastEvent and dmExtension::FEventCallback.
     * Precomputed dmHashString64 value; no runtime hashing.
     * @constant
     * @name EVENT_INPUT_ACTIONS
     * @language C++
     */
    static const dmhash_t EVENT_INPUT_ACTIONS = ENGINE_EVENT_INPUT_ACTIONS;

    /*# engine state notification kind
     * Identifies the initialization or deletion notification in StateEvent.
     * @enum
     * @name State
     * @language C++
     * @member  STATE_INITIALIZED Successful engine initialization has completed.
     * @member  STATE_DELETE Engine deletion begins, before collections and systems are torn down. Also delivered after initialization failure if AppInitialize succeeded.
     */
    typedef ::EngineState State;
    static const State STATE_INITIALIZED = ::ENGINE_STATE_INITIALIZED;
    static const State STATE_DELETE = ::ENGINE_STATE_DELETE;

    /*# window notification kind
     * Identifies the active payload in WindowEvent.
     * @enum
     * @name WindowEventType
     * @language C++
     * @member  WINDOW_EVENT_RESIZED Resize notification. Read physical dimensions from m_Data.m_Size.
     * @member  WINDOW_EVENT_MINIMIZED The window was minimized; not a hide notification.
     * @member  WINDOW_EVENT_RESTORED The window was restored from minimization; not a show notification.
     * @member  WINDOW_EVENT_FOCUS_GAINED The window or app screen gained focus.
     * @member  WINDOW_EVENT_FOCUS_LOST The window or app screen lost focus.
     */
    typedef ::EngineWindowEventType WindowEventType;
    static const WindowEventType WINDOW_EVENT_RESIZED = ::ENGINE_WINDOW_EVENT_RESIZED;
    static const WindowEventType WINDOW_EVENT_MINIMIZED = ::ENGINE_WINDOW_EVENT_MINIMIZED;
    static const WindowEventType WINDOW_EVENT_RESTORED = ::ENGINE_WINDOW_EVENT_RESTORED;
    static const WindowEventType WINDOW_EVENT_FOCUS_GAINED = ::ENGINE_WINDOW_EVENT_FOCUS_GAINED;
    static const WindowEventType WINDOW_EVENT_FOCUS_LOST = ::ENGINE_WINDOW_EVENT_FOCUS_LOST;

    /*# resolved input actions
     * Published once per completed input update after application bindings are
     * resolved, coordinates converted and text/marked-text ordered last, before
     * focused game-object dispatch. Empty updates are published; skipped input
     * updates are not. This is independent of extension Update and rendered-frame
     * count. Input events use the generic handler's application lifetime.
     *
     * Iterate the current resolved batch through dmInput::IterateActions and the iterator
     * getters. The batch belongs to input and remains valid until the next binding
     * update starts or the context is destroyed. Iteration cannot consume input or
     * change focused dispatch. Obtain the source engine from the extension context
     * registry using ENGINE_CONTEXT_NAME. Focused gameplay should use native
     * component input/update callbacks.
     * @struct
     * @name InputActionsEvent
     * @language C++
     * @member m_Input [type:dmInput::HContext] valid borrowed input context, including empty updates
     */
    typedef EngineInputActionsEvent InputActionsEvent;

    /*# engine state event payload
     * Payload for EVENT_STATE with data_size equal to sizeof(StateEvent).
     * Obtain the source engine from the extension context registry using ENGINE_CONTEXT_NAME.
     * @struct
     * @name StateEvent
     * @language C++
     * @member m_State [type:State] notification being delivered
     */
    typedef EngineStateEvent StateEvent;

    /*# engine window event payload
     * Payload for EVENT_WINDOW with data_size equal to sizeof(WindowEvent).
     * m_Type selects the valid union member. Only WINDOW_EVENT_RESIZED has
     * event-specific data: m_Data.m_Size contains the physical framebuffer dimensions.
     * Do not read that member for other notification types.
     *
     * Resize delivery follows the existing window state updates and notifications,
     * once per native resize callback with no initial-size replay. Window events may
     * arrive before engine Initialize; m_Window can be null during early startup.
     * Obtain the source engine from the extension context registry using ENGINE_CONTEXT_NAME.
     * @struct
     * @name WindowEvent
     * @language C++
     * @member m_Window [type:HWindow] borrowed source window, or null if not yet available
     * @member m_Type [type:WindowEventType] notification selecting the valid payload
     * @member  m_Data.m_Size.m_Width [type:uint32_t] physical framebuffer width; valid only for WINDOW_EVENT_RESIZED
     * @member  m_Data.m_Size.m_Height [type:uint32_t] physical framebuffer height; valid only for WINDOW_EVENT_RESIZED
     */
    typedef EngineWindowEvent WindowEvent;


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
 * Identifies the initialization or deletion notification in EngineStateEvent.
 * @enum
 * @name EngineState
 * @language C
 * @member  ENGINE_STATE_INITIALIZED Successful engine initialization has completed.
 * @member  ENGINE_STATE_DELETE Engine deletion begins, before collections and systems are torn down. Also delivered after initialization failure if AppInitialize succeeded.
 */

/*# engine window notification
 * Hash of `_dm_event_engine_window` for ExtensionBroadcastEvent and FExtensionEventCallback.
 * Precomputed dmHashString64 value; no runtime hashing.
 * @constant
 * @name ENGINE_WINDOW_EVENT
 * @language C
 */

/*# window notification kind
 * Identifies the active payload in EngineWindowEvent.
 * @enum
 * @name EngineWindowEventType
 * @language C
 * @member  ENGINE_WINDOW_EVENT_RESIZED Resize notification. Read physical dimensions from m_Data.m_Size.
 * @member  ENGINE_WINDOW_EVENT_MINIMIZED The window was minimized; not a hide notification.
 * @member  ENGINE_WINDOW_EVENT_RESTORED The window was restored from minimization; not a show notification.
 * @member  ENGINE_WINDOW_EVENT_FOCUS_GAINED The window or app screen gained focus.
 * @member  ENGINE_WINDOW_EVENT_FOCUS_LOST The window or app screen lost focus.
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
 * resolved, coordinates converted and text/marked-text ordered last, before
 * focused game-object dispatch. Empty updates are published; skipped input
 * updates are not. This is independent of extension Update and rendered-frame
 * count. Input events use the generic handler's application lifetime.
 *
 * Iterate the current resolved batch through InputIterateActions and the iterator
 * getters. The batch belongs to input and remains valid until the next binding
 * update starts or the context is destroyed. Iteration cannot consume input or
 * change focused dispatch. Obtain the source engine from the extension context
 * registry using ENGINE_CONTEXT_NAME. Focused gameplay should use native
 * component input/update callbacks.
 * @struct
 * @name EngineInputActionsEvent
 * @language C
 * @member m_Input [type:HInputContext] valid borrowed input context, including empty updates
 */

/*# engine state event payload
 * Payload for ENGINE_STATE_EVENT with data_size equal to sizeof(EngineStateEvent).
 * Obtain the source engine from the extension context registry using ENGINE_CONTEXT_NAME.
 * @struct
 * @name EngineStateEvent
 * @language C
 * @member m_State [type:EngineState] notification being delivered
 */

/*# engine window event payload
 * Payload for ENGINE_WINDOW_EVENT with data_size equal to sizeof(EngineWindowEvent).
 * m_Type selects the valid union member. Only ENGINE_WINDOW_EVENT_RESIZED has
 * event-specific data: m_Data.m_Size contains the physical framebuffer dimensions.
 * Do not read that member for other notification types.
 *
 * Resize delivery follows the existing window state updates and notifications,
 * once per native resize callback with no initial-size replay. Window events may
 * arrive before engine Initialize; m_Window can be null during early startup.
 * Obtain the source engine from the extension context registry using ENGINE_CONTEXT_NAME.
 * @struct
 * @name EngineWindowEvent
 * @language C
 * @member m_Window [type:HWindow] borrowed source window, or null if not yet available
 * @member m_Type [type:EngineWindowEventType] notification selecting the valid payload
 * @member  m_Data.m_Size.m_Width [type:uint32_t] physical framebuffer width; valid only for ENGINE_WINDOW_EVENT_RESIZED
 * @member  m_Data.m_Size.m_Height [type:uint32_t] physical framebuffer height; valid only for ENGINE_WINDOW_EVENT_RESIZED
 */
