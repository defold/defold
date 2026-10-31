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

#ifndef DMSDK_ENGINE_GEN_HPP
#define DMSDK_ENGINE_GEN_HPP

#if !defined(__cplusplus)
   #error "This file is supported in C++ only!"
#endif


/*# Engine API documentation
 *
 * Engine instance handles and access to the engine-owned window.
 *
 * @document
 * @language C++
 * @name Engine
 * @namespace dmEngine
 * @path engine/engine/src/dmsdk/engine/engine_gen.hpp
 */

#include <dmsdk/platform/window.h>
#include <dmsdk/extension/extension.h>

#include <dmsdk/engine/engine.h>

namespace dmEngine
{
    /*# get the source engine from extension parameters
     * Available from engine creation through application finalization, including
     * early events and cleanup after initialization failure. The handle is borrowed;
     * do not destroy it or retain it across engine deletion or reboot.
     * @name GetEngine
     * @language C++
     * @param params [type:dmExtension::Params*] valid current callback parameters
     * @return engine [type:HEngine] borrowed source engine, or null for a host without an engine
     */
    HEngine GetEngine(dmExtension::Params* params);

    /*# get the engine window
     * The engine owns the returned window. Do not destroy it or retain it across
     * engine deletion or reboot. Call only on the engine thread with a live engine.
     * @name GetWindow
     * @language C++
     * @param engine [type:HEngine] live engine instance
     * @return window [type:HWindow] borrowed window, or null if the engine has no window
     */
    HWindow GetWindow(HEngine engine);


} // namespace dmEngine

#endif // #define DMSDK_ENGINE_GEN_HPP
/*# engine instance handle
 * Identifies an engine instance. Event payloads borrow the handle for the
 * callback; do not destroy the engine or retain the handle after engine deletion.
 * Engine initialization may be incomplete when handling events during startup
 * or cleanup after an initialization failure. Reboot creates a new instance.
 * @typedef
 * @name HEngine
 * @language C
 */

/*# get the source engine from extension parameters
 * Available from engine creation through application finalization, including
 * early events and cleanup after initialization failure. The handle is borrowed;
 * do not destroy it or retain it across engine deletion or reboot.
 * @name EngineGetFromExtensionParams
 * @language C
 * @param params [type:ExtensionParams*] valid current callback parameters
 * @return engine [type:HEngine] borrowed source engine, or null for a host without an engine
 */

/*# get the engine window
 * The engine owns the returned window. Do not destroy it or retain it across
 * engine deletion or reboot. Call only on the engine thread with a live engine.
 * @name EngineGetWindow
 * @language C
 * @param engine [type:HEngine] live engine instance
 * @return window [type:HWindow] borrowed window, or null if the engine has no window
 */
