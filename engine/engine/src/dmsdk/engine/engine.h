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

#ifndef DMSDK_ENGINE_H
#define DMSDK_ENGINE_H

#include <dmsdk/platform/window.h>

/*# Engine API documentation
 * Engine instance handles and access to the engine-owned window.
 * @document
 * @name Engine
 * @language C
 */

/*# engine instance handle
 * Identifies an engine instance. Event payloads borrow the handle for the
 * callback; do not destroy the engine or retain the handle after engine deletion.
 * Engine initialization may be incomplete when handling events during startup
 * or cleanup after an initialization failure. Reboot creates a new instance.
 * @typedef
 * @name HEngine
 */
#if defined(__cplusplus)
// Preserve the existing C++ engine type without exposing its definition.
namespace dmEngine
{
    struct Engine;
    typedef Engine* HEngine;
}
typedef dmEngine::HEngine HEngine;
#else
typedef struct Engine* HEngine;
#endif

#if defined(__cplusplus)
extern "C" {
#endif

/*# get the engine window
 * The engine owns the returned window. Do not destroy it or retain it across
 * engine deletion or reboot. Call only on the engine thread with a live engine.
 * @name EngineGetWindow
 * @param engine [type:HEngine] live engine instance
 * @return window [type:HWindow] borrowed window, or null if the engine has no window
 */
HWindow EngineGetWindow(HEngine engine);

#if defined(__cplusplus)
}
#include "engine.hpp"
#endif

#endif // DMSDK_ENGINE_H
