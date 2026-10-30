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
 * | `EVENT_ACTIVATEAPP` | `_dm_event_activate_app` | No payload. Application gains focus. |
 * | `EVENT_DEACTIVATEAPP` | `_dm_event_deactivate_app` | No payload. Application loses focus. |
 * | `EVENT_ICONIFYAPP` | `_dm_event_iconify_app` | No payload. Application is iconified. |
 * | `EVENT_DEICONIFYAPP` | `_dm_event_deiconify_app` | No payload. Application is restored. |
 * | `EVENT_ENGINE_INITIALIZED` | `_dm_event_engine_initialized` | No payload. Successful engine initialization has completed. |
 * | `EVENT_ENGINE_DELETE` | `_dm_event_engine_delete` | No payload. Engine deletion begins, before collections and systems are torn down. Also delivered after initialization failure if AppInitialize succeeded. |
 * | `EVENT_WINDOW_RESIZED` | `_dm_event_window_resized` | WindowResizedEvent, after the existing window state updates and notifications. One event per native resize callback; no initial-size replay. May precede engine Initialize. |
 *
 * @document
 * @language C++
 * @name EngineEvent
 * @namespace dmEngine
 * @path engine/engine/src/dmsdk/engine/engine_event_gen.hpp
 */

#include <dmsdk/dlib/hash.h>

#include <dmsdk/engine/engine_event.h>

namespace dmEngine
{
    /*# Application gains focus.
     * Hash of `_dm_event_activate_app` for dmExtension::BroadcastEvent and dmExtension::FEventCallback.
     * Evaluates dmHashString64; this is a hash value, not a legacy ExtensionEventID.
     * @constant
     * @name EVENT_ACTIVATEAPP
     * @language C++
     */
    static const dmhash_t EVENT_ACTIVATEAPP = ENGINE_EVENT_ACTIVATEAPP;

    /*# Application loses focus.
     * Hash of `_dm_event_deactivate_app` for dmExtension::BroadcastEvent and dmExtension::FEventCallback.
     * Evaluates dmHashString64; this is a hash value, not a legacy ExtensionEventID.
     * @constant
     * @name EVENT_DEACTIVATEAPP
     * @language C++
     */
    static const dmhash_t EVENT_DEACTIVATEAPP = ENGINE_EVENT_DEACTIVATEAPP;

    /*# Application is iconified.
     * Hash of `_dm_event_iconify_app` for dmExtension::BroadcastEvent and dmExtension::FEventCallback.
     * Evaluates dmHashString64; this is a hash value, not a legacy ExtensionEventID.
     * @constant
     * @name EVENT_ICONIFYAPP
     * @language C++
     */
    static const dmhash_t EVENT_ICONIFYAPP = ENGINE_EVENT_ICONIFYAPP;

    /*# Application is restored.
     * Hash of `_dm_event_deiconify_app` for dmExtension::BroadcastEvent and dmExtension::FEventCallback.
     * Evaluates dmHashString64; this is a hash value, not a legacy ExtensionEventID.
     * @constant
     * @name EVENT_DEICONIFYAPP
     * @language C++
     */
    static const dmhash_t EVENT_DEICONIFYAPP = ENGINE_EVENT_DEICONIFYAPP;

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

    /*# window resized event payload
     * Payload for `EVENT_WINDOW_RESIZED` with data_size equal to
     * `sizeof(WindowResizedEvent)`.
     * @struct
     * @name WindowResizedEvent
     * @language C++
     * @member m_Width [type:uint32_t] physical framebuffer width in pixels
     * @member m_Height [type:uint32_t] physical framebuffer height in pixels
     */
    typedef EngineWindowResizedEvent WindowResizedEvent;

} // namespace dmEngine

#endif // #define DMSDK_ENGINE_EVENT_GEN_HPP
/*# Application gains focus.
 * Hash of `_dm_event_activate_app` for ExtensionBroadcastEvent and FExtensionEventCallback.
 * Evaluates dmHashString64; this is a hash value, not a legacy ExtensionEventID.
 * @constant
 * @name ENGINE_EVENT_ACTIVATEAPP
 * @language C
 */

/*# Application loses focus.
 * Hash of `_dm_event_deactivate_app` for ExtensionBroadcastEvent and FExtensionEventCallback.
 * Evaluates dmHashString64; this is a hash value, not a legacy ExtensionEventID.
 * @constant
 * @name ENGINE_EVENT_DEACTIVATEAPP
 * @language C
 */

/*# Application is iconified.
 * Hash of `_dm_event_iconify_app` for ExtensionBroadcastEvent and FExtensionEventCallback.
 * Evaluates dmHashString64; this is a hash value, not a legacy ExtensionEventID.
 * @constant
 * @name ENGINE_EVENT_ICONIFYAPP
 * @language C
 */

/*# Application is restored.
 * Hash of `_dm_event_deiconify_app` for ExtensionBroadcastEvent and FExtensionEventCallback.
 * Evaluates dmHashString64; this is a hash value, not a legacy ExtensionEventID.
 * @constant
 * @name ENGINE_EVENT_DEICONIFYAPP
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

/*# window resized event payload
 * Payload for `ENGINE_EVENT_WINDOW_RESIZED` with data_size equal to
 * `sizeof(EngineWindowResizedEvent)`.
 * @struct
 * @name EngineWindowResizedEvent
 * @language C
 * @member m_Width [type:uint32_t] physical framebuffer width in pixels
 * @member m_Height [type:uint32_t] physical framebuffer height in pixels
 */
