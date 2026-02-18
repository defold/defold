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

#ifndef DMSDK_GRAPHICS_H
#define DMSDK_GRAPHICS_H

#include <stdbool.h>
#include <stdint.h>

#include <dmsdk/dlib/jobsystem.h>
#include <dmsdk/platform/window.h>

#if defined(__cplusplus)
extern "C" {
#endif

/*# API for graphics context lifecycle.
 *
 * API for graphics context lifecycle.
 *
 * @document
 * @name Graphics
 * @language C
 */

/*# graphics context handle
 * @typedef
 * @name HGraphicsContext
 */
typedef void* HGraphicsContext;

/*# graphics adapter family
 * @enum
 * @name AdapterFamily
 * @member ADAPTER_FAMILY_NONE
 * @member ADAPTER_FAMILY_NULL
 * @member ADAPTER_FAMILY_OPENGL
 * @member ADAPTER_FAMILY_OPENGLES
 * @member ADAPTER_FAMILY_VULKAN
 * @member ADAPTER_FAMILY_VENDOR
 * @member ADAPTER_FAMILY_WEBGPU
 * @member ADAPTER_FAMILY_DIRECTX
 */
typedef enum AdapterFamily
{
    ADAPTER_FAMILY_NONE     = -1,
    ADAPTER_FAMILY_NULL     = 1,
    ADAPTER_FAMILY_OPENGL   = 2,
    ADAPTER_FAMILY_OPENGLES = 3,
    ADAPTER_FAMILY_VULKAN   = 4,
    ADAPTER_FAMILY_VENDOR   = 5,
    ADAPTER_FAMILY_WEBGPU   = 6,
    ADAPTER_FAMILY_DIRECTX  = 7,
} AdapterFamily;

/*# texture filter mode
 * @enum
 * @name TextureFilter
 * @member TEXTURE_FILTER_DEFAULT
 * @member TEXTURE_FILTER_NEAREST
 * @member TEXTURE_FILTER_LINEAR
 * @member TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST
 * @member TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR
 * @member TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST
 * @member TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR
 */
typedef enum TextureFilter
{
    TEXTURE_FILTER_DEFAULT                = 0,
    TEXTURE_FILTER_NEAREST                = 1,
    TEXTURE_FILTER_LINEAR                 = 2,
    TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST = 3,
    TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR  = 4,
    TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST  = 5,
    TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR   = 6,
} TextureFilter;

/*# context creation parameters
 * @struct
 * @name GraphicsCreateParams
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
typedef struct GraphicsCreateParams
{
    HWindow                 m_Window;
    HJobContext             m_JobContext;
    TextureFilter           m_DefaultTextureMinFilter;
    TextureFilter           m_DefaultTextureMagFilter;
    uint32_t                m_Width;
    uint32_t                m_Height;
    uint32_t                m_GraphicsMemorySize;
    uint32_t                m_SwapInterval;
    uint8_t                 m_VerifyGraphicsCalls : 1;
    uint8_t                 m_PrintDeviceInfo : 1;
    uint8_t                 m_RenderDocSupport : 1;
    uint8_t                 m_UseValidationLayers : 1;
    uint8_t                                       : 4;
} GraphicsCreateParams;

/*# initialize context creation parameters
 * @name GraphicsContextParamsInitialize
 * @param params [type:GraphicsCreateParams*] parameter struct to initialize
 */
void GraphicsContextParamsInitialize(GraphicsCreateParams* params);

/*# create a graphics context
 * @name GraphicsNewContext
 * @param params [type:GraphicsCreateParams*] context creation parameters
 * @return context [type:HGraphicsContext] graphics context handle
 */
HGraphicsContext GraphicsNewContext(const GraphicsCreateParams* params);

/*# delete a graphics context
 * @name GraphicsDeleteContext
 * @param context [type:HGraphicsContext] graphics context handle
 */
void GraphicsDeleteContext(HGraphicsContext context);

/*# install graphics adapter
 * @name GraphicsInstallAdapter
 * @param family [type:AdapterFamily] graphics adapter family
 * @return ok [type:bool] true if adapter was installed, false otherwise
 */
bool GraphicsInstallAdapter(AdapterFamily family);

/*# begin frame
 * @name GraphicsBeginFrame
 * @param context [type:HGraphicsContext] graphics context handle
 */
void GraphicsBeginFrame(HGraphicsContext context);

/*# present frame
 * @name GraphicsFlip
 * @param context [type:HGraphicsContext] graphics context handle
 */
void GraphicsFlip(HGraphicsContext context);

/*# close graphics window
 * @name GraphicsCloseWindow
 * @param context [type:HGraphicsContext] graphics context handle
 */
void GraphicsCloseWindow(HGraphicsContext context);

/*# finalize graphics subsystem
 * @name GraphicsFinalize
 */
void GraphicsFinalize(void);

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // DMSDK_GRAPHICS_H
