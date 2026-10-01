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

#ifndef DMSDK_GAMESYS_LIGHT_H
#define DMSDK_GAMESYS_LIGHT_H

#include <dmsdk/render/render.h>

/*# Light component functions
 *
 * API for interacting with light components.
 *
 * @document
 * @name Light
 * @namespace dmGameSystem
 * @language C++
 */

namespace dmGameSystem
{
    /*#
     * Borrowed light component pointer. Valid only while the component exists.
     * @typedef
     * @name HLightComponent
     */
    typedef struct LightComponent* HLightComponent;

    /*#
     * Get the render light instance owned by a light component.
     * Resolve the component using dmGameSystem::GetComponentFromLua with type "lightc",
     * or dmGameObject::GetComponent, and verify the component type before casting.
     * The returned handle can be retained while the component exists, but must be queried
     * with dmRender::GetLightInstanceRenderData after light submission to obtain its current buffer index.
     * Do not delete the returned instance; it is owned by the component.
     * @name CompLightGetLightInstance
     * @param component [type: HLightComponent] non-null, live light component
     * @return instance [type: dmRender::HLightInstance] render light instance handle
     */
    dmRender::HLightInstance CompLightGetLightInstance(HLightComponent component);
}

#endif // DMSDK_GAMESYS_LIGHT_H
