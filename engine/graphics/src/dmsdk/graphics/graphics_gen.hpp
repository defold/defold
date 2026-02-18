// Generated, do not edit!
// Generated with cwd=/Users/mathiaswesterdahl/work/defold/engine/graphics and cmd=../../scripts/dmsdk/gen_sdk.py -i sdk_gen.json

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

#ifndef DMSDK_GRAPHICS_GEN_HPP
#define DMSDK_GRAPHICS_GEN_HPP

#if !defined(__cplusplus)
   #error "This file is supported in C++ only!"
#endif


/*# API for graphics context lifecycle.
 *
 * API for graphics context lifecycle.
 *
 * @document
 * @language C++
 * @name Graphics
 * @namespace dmGraphics
 * @path engine/graphics/src/dmsdk/graphics/graphics_gen.hpp
 */

#include <stdint.h>
#include <dmsdk/dlib/jobsystem.h>
#include <dmsdk/platform/window.h>

#include <dmsdk/graphics/graphics.h>

namespace dmGraphics
{
    /*# graphics context handle
     * graphics context handle
     * @typedef
     * @name HContext
     * @language C++
     */
    typedef HGraphicsContext HContext;

    /*# graphics adapter family
     * graphics adapter family
     * @enum
     * @name AdapterFamily
     * @language C++
     * @member  ADAPTER_FAMILY_NONE
     * @member  ADAPTER_FAMILY_NULL
     * @member  ADAPTER_FAMILY_OPENGL
     * @member  ADAPTER_FAMILY_OPENGLES
     * @member  ADAPTER_FAMILY_VULKAN
     * @member  ADAPTER_FAMILY_VENDOR
     * @member  ADAPTER_FAMILY_WEBGPU
     * @member  ADAPTER_FAMILY_DIRECTX
     */
    enum AdapterFamily {
        ADAPTER_FAMILY_NONE = -1,
        ADAPTER_FAMILY_NULL = 1,
        ADAPTER_FAMILY_OPENGL = 2,
        ADAPTER_FAMILY_OPENGLES = 3,
        ADAPTER_FAMILY_VULKAN = 4,
        ADAPTER_FAMILY_VENDOR = 5,
        ADAPTER_FAMILY_WEBGPU = 6,
        ADAPTER_FAMILY_DIRECTX = 7,
    };

    /*# texture filter mode
     * texture filter mode
     * @enum
     * @name TextureFilter
     * @language C++
     * @member  TEXTURE_FILTER_DEFAULT
     * @member  TEXTURE_FILTER_NEAREST
     * @member  TEXTURE_FILTER_LINEAR
     * @member  TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST
     * @member  TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR
     * @member  TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST
     * @member  TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR
     */
    enum TextureFilter {
        TEXTURE_FILTER_DEFAULT = 0,
        TEXTURE_FILTER_NEAREST = 1,
        TEXTURE_FILTER_LINEAR = 2,
        TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST = 3,
        TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR = 4,
        TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST = 5,
        TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR = 6,
    };

    /*# context creation parameters
     * context creation parameters
     * @struct
     * @name ContextParams
     * @language C++
     * @member m_Window [type:HWindow] Window handle
     * @member m_JobContext [type:HJobContext] Job system context
     * @member m_DefaultTextureMinFilter [type:TextureFilter] Default minification filter
     * @member m_DefaultTextureMagFilter [type:TextureFilter] Default magnification filter
     * @member m_Width [type:uint32_t] Context width
     * @member m_Height [type:uint32_t] Context height
     * @member m_GraphicsMemorySize [type:uint32_t] Maximum graphics memory budget (0 = backend default)
     * @member m_SwapInterval [type:uint32_t] Swap interval (1 = vsync)
     * @member m_VerifyGraphicsCalls [type:uint8_t:1] Enable graphics call verification
     * @member m_PrintDeviceInfo [type:uint8_t:1] Print backend/device information
     * @member m_RenderDocSupport [type:uint8_t:1] Enable RenderDoc integration where supported
     * @member m_UseValidationLayers [type:uint8_t:1] Enable validation layers where supported
     */
    typedef GraphicsCreateParams ContextParams;

    /*# initialize context creation parameters
     * initialize context creation parameters
     * @name ContextParamsInitialize
     * @language C++
     * @param params [type:GraphicsCreateParams*] parameter struct to initialize
     */
    void ContextParamsInitialize(ContextParams * params);

    /*# create a graphics context
     * create a graphics context
     * @name NewContext
     * @language C++
     * @param params [type:GraphicsCreateParams*] context creation parameters
     * @return context [type:HGraphicsContext] graphics context handle
     */
    HContext NewContext(const ContextParams * params);

    /*# delete a graphics context
     * delete a graphics context
     * @name DeleteContext
     * @language C++
     * @param context [type:HGraphicsContext] graphics context handle
     */
    void DeleteContext(HContext context);

    /*# install graphics adapter
     * install graphics adapter
     * @name InstallAdapter
     * @language C++
     * @param family [type:AdapterFamily] graphics adapter family
     * @return ok [type:bool] true if adapter was installed, false otherwise
     */
    bool InstallAdapter(AdapterFamily family);

    /*# begin frame
     * begin frame
     * @name BeginFrame
     * @language C++
     * @param context [type:HGraphicsContext] graphics context handle
     */
    void BeginFrame(HContext context);

    /*# present frame
     * present frame
     * @name Flip
     * @language C++
     * @param context [type:HGraphicsContext] graphics context handle
     */
    void Flip(HContext context);

    /*# close graphics window
     * close graphics window
     * @name CloseWindow
     * @language C++
     * @param context [type:HGraphicsContext] graphics context handle
     */
    void CloseWindow(HContext context);

    /*# finalize graphics subsystem
     * finalize graphics subsystem
     * @name Finalize
     * @language C++
     */
    void Finalize();


} // namespace dmGraphics

#endif // #define DMSDK_GRAPHICS_GEN_HPP

