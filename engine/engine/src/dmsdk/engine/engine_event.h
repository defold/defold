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

/*# Engine event definitions
 * Built-in event hashes and payloads published through the generic extension
 * event transport in `<dmsdk/extension/extension.h>`. Custom events may use
 * arbitrary name hashes and payloads without central registration.
 *
 * Built-in event hashes and their names:
 *
 * | Constant | Name | Payload and delivery |
 * | -------- | ---- | -------------------- |
 * | `ENGINE_EVENT_ACTIVATEAPP` | `_dm_event_activate_app` | No payload. Application gains focus. |
 * | `ENGINE_EVENT_DEACTIVATEAPP` | `_dm_event_deactivate_app` | No payload. Application loses focus. |
 * | `ENGINE_EVENT_ICONIFYAPP` | `_dm_event_iconify_app` | No payload. Application is iconified. |
 * | `ENGINE_EVENT_DEICONIFYAPP` | `_dm_event_deiconify_app` | No payload. Application is restored. |
 * | `ENGINE_EVENT_ENGINE_INITIALIZED` | `_dm_event_engine_initialized` | No payload. Successful engine initialization has completed. |
 * | `ENGINE_EVENT_ENGINE_DELETE` | `_dm_event_engine_delete` | No payload. Engine deletion begins, before collections and systems are torn down. Also delivered after initialization failure if AppInitialize succeeded. |
 * | `ENGINE_EVENT_WINDOW_RESIZED` | `_dm_event_window_resized` | EngineWindowResizedEvent, after the existing window state updates and notifications. One event per native resize callback; no initial-size replay. May precede engine Initialize. |
 *
 * @document
 * @name EngineEvent
 * @language C
 */

/*# Application gains focus.
 * Hash of `_dm_event_activate_app` for ExtensionBroadcastEvent and FExtensionEventCallback.
 * Evaluates dmHashString64; this is a hash value, not a legacy ExtensionEventID.
 * @constant
 * @name ENGINE_EVENT_ACTIVATEAPP
 */
#define ENGINE_EVENT_ACTIVATEAPP (dmHashString64("_dm_event_activate_app"))

/*# Application loses focus.
 * Hash of `_dm_event_deactivate_app` for ExtensionBroadcastEvent and FExtensionEventCallback.
 * Evaluates dmHashString64; this is a hash value, not a legacy ExtensionEventID.
 * @constant
 * @name ENGINE_EVENT_DEACTIVATEAPP
 */
#define ENGINE_EVENT_DEACTIVATEAPP (dmHashString64("_dm_event_deactivate_app"))

/*# Application is iconified.
 * Hash of `_dm_event_iconify_app` for ExtensionBroadcastEvent and FExtensionEventCallback.
 * Evaluates dmHashString64; this is a hash value, not a legacy ExtensionEventID.
 * @constant
 * @name ENGINE_EVENT_ICONIFYAPP
 */
#define ENGINE_EVENT_ICONIFYAPP (dmHashString64("_dm_event_iconify_app"))

/*# Application is restored.
 * Hash of `_dm_event_deiconify_app` for ExtensionBroadcastEvent and FExtensionEventCallback.
 * Evaluates dmHashString64; this is a hash value, not a legacy ExtensionEventID.
 * @constant
 * @name ENGINE_EVENT_DEICONIFYAPP
 */
#define ENGINE_EVENT_DEICONIFYAPP (dmHashString64("_dm_event_deiconify_app"))

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

/*# window resized event payload
 * Payload for `ENGINE_EVENT_WINDOW_RESIZED` with data_size equal to
 * `sizeof(EngineWindowResizedEvent)`.
 * @struct
 * @name EngineWindowResizedEvent
 * @member m_Width [type:uint32_t] physical framebuffer width in pixels
 * @member m_Height [type:uint32_t] physical framebuffer height in pixels
 */
typedef struct EngineWindowResizedEvent
{
    uint32_t m_Width;
    uint32_t m_Height;
} EngineWindowResizedEvent;

#if defined(__cplusplus)
#include "engine_event.hpp"
#endif

#endif // DMSDK_ENGINE_EVENT_H
