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
 *
 * Payloads and their handles are borrowed for the synchronous callback. Engine
 * events identify the source instance, and window-driven events identify its
 * window too. A window handle may be null during early startup or failure cleanup.
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

#include <dmsdk/engine/engine_event.h>

namespace dmEngine
{
    /*# The engine window gains focus.
     * Hash of `_dm_event_app_activate` for dmExtension::BroadcastEvent and dmExtension::FEventCallback.
     * Evaluates dmHashString64; this is a hash value, not a legacy ExtensionEventID.
     * @constant
     * @name EVENT_APP_ACTIVATE
     * @language C++
     */
    static const dmhash_t EVENT_APP_ACTIVATE = ENGINE_EVENT_APP_ACTIVATE;

    /*# The engine window loses focus.
     * Hash of `_dm_event_app_deactivate` for dmExtension::BroadcastEvent and dmExtension::FEventCallback.
     * Evaluates dmHashString64; this is a hash value, not a legacy ExtensionEventID.
     * @constant
     * @name EVENT_APP_DEACTIVATE
     * @language C++
     */
    static const dmhash_t EVENT_APP_DEACTIVATE = ENGINE_EVENT_APP_DEACTIVATE;

    /*# The engine window is iconified.
     * Hash of `_dm_event_app_iconify` for dmExtension::BroadcastEvent and dmExtension::FEventCallback.
     * Evaluates dmHashString64; this is a hash value, not a legacy ExtensionEventID.
     * @constant
     * @name EVENT_APP_ICONIFY
     * @language C++
     */
    static const dmhash_t EVENT_APP_ICONIFY = ENGINE_EVENT_APP_ICONIFY;

    /*# The engine window is restored.
     * Hash of `_dm_event_app_deiconify` for dmExtension::BroadcastEvent and dmExtension::FEventCallback.
     * Evaluates dmHashString64; this is a hash value, not a legacy ExtensionEventID.
     * @constant
     * @name EVENT_APP_DEICONIFY
     * @language C++
     */
    static const dmhash_t EVENT_APP_DEICONIFY = ENGINE_EVENT_APP_DEICONIFY;

    /*# Successful engine initialization has completed.
     * Hash of `_dm_event_engine_initialized` for dmExtension::BroadcastEvent and dmExtension::FEventCallback.
     * Evaluates dmHashString64; this is a hash value, not a legacy ExtensionEventID.
     * @constant
     * @name EVENT_ENGINE_INITIALIZED
     * @language C++
     */
    static const dmhash_t EVENT_ENGINE_INITIALIZED = ENGINE_EVENT_ENGINE_INITIALIZED;

    /*# Engine deletion begins, before collections and systems are torn down.
     * Hash of `_dm_event_engine_delete` for dmExtension::BroadcastEvent and dmExtension::FEventCallback.
     * Evaluates dmHashString64; this is a hash value, not a legacy ExtensionEventID.
     * @constant
     * @name EVENT_ENGINE_DELETE
     * @language C++
     */
    static const dmhash_t EVENT_ENGINE_DELETE = ENGINE_EVENT_ENGINE_DELETE;

    /*# Window size changed; the payload is WindowResizedEvent.
     * Hash of `_dm_event_window_resized` for dmExtension::BroadcastEvent and dmExtension::FEventCallback.
     * Evaluates dmHashString64; this is a hash value, not a legacy ExtensionEventID.
     * @constant
     * @name EVENT_WINDOW_RESIZED
     * @language C++
     */
    static const dmhash_t EVENT_WINDOW_RESIZED = ENGINE_EVENT_WINDOW_RESIZED;

    /*# engine lifecycle event payload
     * Payload for EVENT_ENGINE_INITIALIZED and EVENT_ENGINE_DELETE
     * with data_size equal to sizeof(LifecycleEvent).
     * @struct
     * @name LifecycleEvent
     * @language C++
     * @member m_Engine [type:HEngine] borrowed source engine; initialization may be incomplete
     */
    typedef EngineLifecycleEvent LifecycleEvent;

    /*# application window event payload
     * Payload for all four EVENT_APP_* events with data_size equal to
     * sizeof(AppEvent). The event id specifies the focus or iconify transition.
     * @struct
     * @name AppEvent
     * @language C++
     * @member m_Engine [type:HEngine] borrowed source engine
     * @member m_Window [type:HWindow] borrowed source window, or null if not yet available
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
 * Evaluates dmHashString64; this is a hash value, not a legacy ExtensionEventID.
 * @constant
 * @name ENGINE_EVENT_APP_ACTIVATE
 * @language C
 */

/*# The engine window loses focus.
 * Hash of `_dm_event_app_deactivate` for ExtensionBroadcastEvent and FExtensionEventCallback.
 * Evaluates dmHashString64; this is a hash value, not a legacy ExtensionEventID.
 * @constant
 * @name ENGINE_EVENT_APP_DEACTIVATE
 * @language C
 */

/*# The engine window is iconified.
 * Hash of `_dm_event_app_iconify` for ExtensionBroadcastEvent and FExtensionEventCallback.
 * Evaluates dmHashString64; this is a hash value, not a legacy ExtensionEventID.
 * @constant
 * @name ENGINE_EVENT_APP_ICONIFY
 * @language C
 */

/*# The engine window is restored.
 * Hash of `_dm_event_app_deiconify` for ExtensionBroadcastEvent and FExtensionEventCallback.
 * Evaluates dmHashString64; this is a hash value, not a legacy ExtensionEventID.
 * @constant
 * @name ENGINE_EVENT_APP_DEICONIFY
 * @language C
 */

/*# Successful engine initialization has completed.
 * Hash of `_dm_event_engine_initialized` for ExtensionBroadcastEvent and FExtensionEventCallback.
 * Evaluates dmHashString64; this is a hash value, not a legacy ExtensionEventID.
 * @constant
 * @name ENGINE_EVENT_ENGINE_INITIALIZED
 * @language C
 */

/*# Engine deletion begins, before collections and systems are torn down.
 * Hash of `_dm_event_engine_delete` for ExtensionBroadcastEvent and FExtensionEventCallback.
 * Evaluates dmHashString64; this is a hash value, not a legacy ExtensionEventID.
 * @constant
 * @name ENGINE_EVENT_ENGINE_DELETE
 * @language C
 */

/*# Window size changed; the payload is EngineWindowResizedEvent.
 * Hash of `_dm_event_window_resized` for ExtensionBroadcastEvent and FExtensionEventCallback.
 * Evaluates dmHashString64; this is a hash value, not a legacy ExtensionEventID.
 * @constant
 * @name ENGINE_EVENT_WINDOW_RESIZED
 * @language C
 */

/*# engine lifecycle event payload
 * Payload for ENGINE_EVENT_ENGINE_INITIALIZED and ENGINE_EVENT_ENGINE_DELETE
 * with data_size equal to sizeof(EngineLifecycleEvent).
 * @struct
 * @name EngineLifecycleEvent
 * @language C
 * @member m_Engine [type:HEngine] borrowed source engine; initialization may be incomplete
 */

/*# application window event payload
 * Payload for all four ENGINE_EVENT_APP_* events with data_size equal to
 * sizeof(EngineAppEvent). The event id specifies the focus or iconify transition.
 * @struct
 * @name EngineAppEvent
 * @language C
 * @member m_Engine [type:HEngine] borrowed source engine
 * @member m_Window [type:HWindow] borrowed source window, or null if not yet available
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
