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

#ifndef DMSDK_GRAPHICS_OPENGL_H
#define DMSDK_GRAPHICS_OPENGL_H

#include <dmsdk/graphics/graphics.h>

/*# Graphics API documentation
 *
 * Graphics OpenGL API
 *
 * @document
 * @name Graphics OpenGL
 * @namespace dmGraphics
 * @language C++
 */

namespace dmGraphics
{
	/*#
     * Get the OpenGL render target id from a render target
     * @name OpenGLGetRenderTargetId
     * @param context [type: dmGraphics::HContext] the OpenGL context
     * @param render_target [type: dmGraphics::HRenderTarget] the render target to get the ID from
     * @return id [type: uint32_t] the OpenGL render target id
     */
	uint32_t OpenGLGetRenderTargetId(HContext context, HRenderTarget render_target);

    /*#
     * Get the default framebuffer ID
     * @name OpenGLGetDefaultFramebufferId
     * @param context [type: dmGraphics::HContext] the OpenGL context
     * @return framebuffer [type: uint32_t] the framebuffer id
     */
    uint32_t OpenGLGetDefaultFramebufferId(HContext context);
}

#endif
