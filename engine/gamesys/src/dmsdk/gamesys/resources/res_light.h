// Copyright 2020-2026 The Defold Foundation
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

#ifndef DMSDK_GAMESYS_RES_LIGHT_H
#define DMSDK_GAMESYS_RES_LIGHT_H

#include <gamesys/data_ddf.h>

#include <dmsdk/render/render.h>

namespace dmGameSystem
{
    /*# Light resource access
     *
     * Helper types and accessors for the light resource type (`.lightc`).
     *
     * @document
     * @namespace dmGameSystem
     * @name Light Resource
     * @language C++
     */

    /*# Opaque handle to a loaded light resource
     *
     * This struct is an opaque handle managed by the engine.
     *
     * @struct
     * @name LightResource
     */
    struct LightResource;

    /*# Get the render light prototype for a loaded light resource
     *
     * Returns the `dmRender::LightPrototype` created from the `.lightc` data.
     * The pointer remains valid until the resource is released.
     *
     * @name GetLightPrototype
     * @param res [type: LightResource*] Light resource handle
     * @return prototype [type: dmRender::LightPrototype*] Prototype pointer, or
     *         `0` if `res` is `0` or the prototype failed to create.
     */
    dmRender::HLightPrototype GetLightPrototype(LightResource* res);
}

#endif // DMSDK_GAMESYS_RES_LIGHT_H

