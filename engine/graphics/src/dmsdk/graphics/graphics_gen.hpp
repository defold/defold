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
#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/jobsystem.h>
#include <dmsdk/platform/window.h>

#include <dmsdk/graphics/graphics.h>

namespace dmGraphics
{
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

    /*# pixel formats supported by textures
     * pixel formats supported by textures
     * @enum
     * @name TextureFormat
     * @language C++
     * @member  TEXTURE_FORMAT_LUMINANCE
     * @member  TEXTURE_FORMAT_LUMINANCE_ALPHA
     * @member  TEXTURE_FORMAT_RGB
     * @member  TEXTURE_FORMAT_RGBA
     * @member  TEXTURE_FORMAT_RGB_16BPP
     * @member  TEXTURE_FORMAT_RGBA_16BPP
     * @member  TEXTURE_FORMAT_DEPTH
     * @member  TEXTURE_FORMAT_STENCIL
     * @member  TEXTURE_FORMAT_RGB_PVRTC_2BPPV1
     * @member  TEXTURE_FORMAT_RGB_PVRTC_4BPPV1
     * @member  TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1
     * @member  TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1
     * @member  TEXTURE_FORMAT_RGB_ETC1
     * @member  TEXTURE_FORMAT_R_ETC2
     * @member  TEXTURE_FORMAT_RG_ETC2
     * @member  TEXTURE_FORMAT_RGBA_ETC2
     * @member  TEXTURE_FORMAT_RGBA_ASTC_4X4
     * @member  TEXTURE_FORMAT_RGB_BC1
     * @member  TEXTURE_FORMAT_RGBA_BC3
     * @member  TEXTURE_FORMAT_R_BC4
     * @member  TEXTURE_FORMAT_RG_BC5
     * @member  TEXTURE_FORMAT_RGBA_BC7
     * @member  TEXTURE_FORMAT_RGB16F
     * @member  TEXTURE_FORMAT_RGB32F
     * @member  TEXTURE_FORMAT_RGBA16F
     * @member  TEXTURE_FORMAT_RGBA32F
     * @member  TEXTURE_FORMAT_R16F
     * @member  TEXTURE_FORMAT_RG16F
     * @member  TEXTURE_FORMAT_R32F
     * @member  TEXTURE_FORMAT_RG32F
     * @member  TEXTURE_FORMAT_RGBA32UI
     * @member  TEXTURE_FORMAT_BGRA8U
     * @member  TEXTURE_FORMAT_R32UI
     * @member  TEXTURE_FORMAT_RGBA_ASTC_5X4
     * @member  TEXTURE_FORMAT_RGBA_ASTC_5X5
     * @member  TEXTURE_FORMAT_RGBA_ASTC_6X5
     * @member  TEXTURE_FORMAT_RGBA_ASTC_6X6
     * @member  TEXTURE_FORMAT_RGBA_ASTC_8X5
     * @member  TEXTURE_FORMAT_RGBA_ASTC_8X6
     * @member  TEXTURE_FORMAT_RGBA_ASTC_8X8
     * @member  TEXTURE_FORMAT_RGBA_ASTC_10X5
     * @member  TEXTURE_FORMAT_RGBA_ASTC_10X6
     * @member  TEXTURE_FORMAT_RGBA_ASTC_10X8
     * @member  TEXTURE_FORMAT_RGBA_ASTC_10X10
     * @member  TEXTURE_FORMAT_RGBA_ASTC_12X10
     * @member  TEXTURE_FORMAT_RGBA_ASTC_12X12
     * @member  TEXTURE_FORMAT_COUNT
     */
    enum TextureFormat {
        TEXTURE_FORMAT_LUMINANCE = 0,
        TEXTURE_FORMAT_LUMINANCE_ALPHA = 1,
        TEXTURE_FORMAT_RGB = 2,
        TEXTURE_FORMAT_RGBA = 3,
        TEXTURE_FORMAT_RGB_16BPP = 4,
        TEXTURE_FORMAT_RGBA_16BPP = 5,
        TEXTURE_FORMAT_DEPTH = 6,
        TEXTURE_FORMAT_STENCIL = 7,
        TEXTURE_FORMAT_RGB_PVRTC_2BPPV1 = 8,
        TEXTURE_FORMAT_RGB_PVRTC_4BPPV1 = 9,
        TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1 = 10,
        TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1 = 11,
        TEXTURE_FORMAT_RGB_ETC1 = 12,
        TEXTURE_FORMAT_R_ETC2 = 13,
        TEXTURE_FORMAT_RG_ETC2 = 14,
        TEXTURE_FORMAT_RGBA_ETC2 = 15,
        TEXTURE_FORMAT_RGBA_ASTC_4X4 = 16,
        TEXTURE_FORMAT_RGB_BC1 = 17,
        TEXTURE_FORMAT_RGBA_BC3 = 18,
        TEXTURE_FORMAT_R_BC4 = 19,
        TEXTURE_FORMAT_RG_BC5 = 20,
        TEXTURE_FORMAT_RGBA_BC7 = 21,
        TEXTURE_FORMAT_RGB16F = 22,
        TEXTURE_FORMAT_RGB32F = 23,
        TEXTURE_FORMAT_RGBA16F = 24,
        TEXTURE_FORMAT_RGBA32F = 25,
        TEXTURE_FORMAT_R16F = 26,
        TEXTURE_FORMAT_RG16F = 27,
        TEXTURE_FORMAT_R32F = 28,
        TEXTURE_FORMAT_RG32F = 29,
        TEXTURE_FORMAT_RGBA32UI = 30,
        TEXTURE_FORMAT_BGRA8U = 31,
        TEXTURE_FORMAT_R32UI = 32,
        TEXTURE_FORMAT_RGBA_ASTC_5X4 = 33,
        TEXTURE_FORMAT_RGBA_ASTC_5X5 = 34,
        TEXTURE_FORMAT_RGBA_ASTC_6X5 = 35,
        TEXTURE_FORMAT_RGBA_ASTC_6X6 = 36,
        TEXTURE_FORMAT_RGBA_ASTC_8X5 = 37,
        TEXTURE_FORMAT_RGBA_ASTC_8X6 = 38,
        TEXTURE_FORMAT_RGBA_ASTC_8X8 = 39,
        TEXTURE_FORMAT_RGBA_ASTC_10X5 = 40,
        TEXTURE_FORMAT_RGBA_ASTC_10X6 = 41,
        TEXTURE_FORMAT_RGBA_ASTC_10X8 = 42,
        TEXTURE_FORMAT_RGBA_ASTC_10X10 = 43,
        TEXTURE_FORMAT_RGBA_ASTC_12X10 = 44,
        TEXTURE_FORMAT_RGBA_ASTC_12X12 = 45,
        TEXTURE_FORMAT_COUNT,
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

    /*# create a vertex stream declaration
     * create a vertex stream declaration
     * @name NewVertexStreamDeclaration
     * @language C++
     * @param context [type:HGraphicsContext] graphics context handle
     * @return declaration [type:HVertexStreamDeclaration] vertex stream declaration handle
     */
    HVertexStreamDeclaration NewVertexStreamDeclaration(HContext context);

    /*# create a vertex stream declaration with explicit step function
     * create a vertex stream declaration with explicit step function
     * @name NewVertexStreamDeclaration
     * @language C++
     * @param context [type:HGraphicsContext] graphics context handle
     * @param step_function [type:VertexAttributeStepFunction] vertex step function value
     * @return declaration [type:HVertexStreamDeclaration] vertex stream declaration handle
     */
    HVertexStreamDeclaration NewVertexStreamDeclaration(HContext context, VertexStepFunction step_function);

    /*# delete a vertex stream declaration
     * delete a vertex stream declaration
     * @name DeleteVertexStreamDeclaration
     * @language C++
     * @param stream_declaration [type:HVertexStreamDeclaration] vertex stream declaration handle
     */
    void DeleteVertexStreamDeclaration(HVertexStreamDeclaration stream_declaration);

    /*# add a stream to a vertex stream declaration
     * add a stream to a vertex stream declaration
     * @name AddVertexStream
     * @language C++
     * @param stream_declaration [type:HVertexStreamDeclaration] vertex stream declaration handle
     * @param name [type:const char*] stream name
     * @param size [type:uint32_t] number of components in stream
     * @param type [type:VertexAttributeDataType] stream data type value
     * @param normalize [type:bool] normalize stream values
     */
    void AddVertexStream(HVertexStreamDeclaration stream_declaration, const char * name, uint32_t size, Type type, bool normalize);

    /*# add a stream (hashed name) to a vertex stream declaration
     * add a stream (hashed name) to a vertex stream declaration
     * @name AddVertexStream
     * @language C++
     * @param stream_declaration [type:HVertexStreamDeclaration] vertex stream declaration handle
     * @param name_hash [type:dmhash_t] stream name hash
     * @param size [type:uint32_t] number of components in stream
     * @param type [type:VertexAttributeDataType] stream data type value
     * @param normalize [type:bool] normalize stream values
     */
    void AddVertexStream(HVertexStreamDeclaration stream_declaration, dmhash_t name_hash, uint32_t size, Type type, bool normalize);

    /*# create a vertex declaration
     * create a vertex declaration
     * @name NewVertexDeclaration
     * @language C++
     * @param context [type:HGraphicsContext] graphics context handle
     * @param stream_declaration [type:HVertexStreamDeclaration] vertex stream declaration handle
     * @return declaration [type:HVertexDeclaration] vertex declaration handle
     */
    HVertexDeclaration NewVertexDeclaration(HContext context, HVertexStreamDeclaration stream_declaration);

    /*# create a vertex declaration with explicit stride
     * create a vertex declaration with explicit stride
     * @name NewVertexDeclaration
     * @language C++
     * @param context [type:HGraphicsContext] graphics context handle
     * @param stream_declaration [type:HVertexStreamDeclaration] vertex stream declaration handle
     * @param stride [type:uint32_t] vertex stride in bytes
     * @return declaration [type:HVertexDeclaration] vertex declaration handle
     */
    HVertexDeclaration NewVertexDeclaration(HContext context, HVertexStreamDeclaration stream_declaration, uint32_t stride);

    /*# delete a vertex declaration
     * delete a vertex declaration
     * @name DeleteVertexDeclaration
     * @language C++
     * @param vertex_declaration [type:HVertexDeclaration] vertex declaration handle
     */
    void DeleteVertexDeclaration(HVertexDeclaration vertex_declaration);

    /*# enable a vertex declaration
     * enable a vertex declaration
     * @name EnableVertexDeclaration
     * @language C++
     * @param context [type:HGraphicsContext] graphics context handle
     * @param vertex_declaration [type:HVertexDeclaration] vertex declaration handle
     * @param binding_index [type:uint32_t] vertex buffer binding index
     * @param base_offset [type:uint32_t] base offset in bytes
     * @param program [type:uintptr_t] program handle
     */
    void EnableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration, uint32_t binding_index, uint32_t base_offset, uintptr_t program);

    /*# disable a vertex declaration
     * disable a vertex declaration
     * @name DisableVertexDeclaration
     * @language C++
     * @param context [type:HGraphicsContext] graphics context handle
     * @param vertex_declaration [type:HVertexDeclaration] vertex declaration handle
     */
    void DisableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration);

    /*# get stream offset from a vertex declaration
     * get stream offset from a vertex declaration
     * @name GetVertexStreamOffset
     * @language C++
     * @param vertex_declaration [type:HVertexDeclaration] vertex declaration handle
     * @param name_hash [type:dmhash_t] stream name hash
     * @return offset [type:uint32_t] stream offset or 0xFFFFFFFF if not found
     */
    uint32_t GetVertexStreamOffset(HVertexDeclaration vertex_declaration, dmhash_t name_hash);


} // namespace dmGraphics

#endif // #define DMSDK_GRAPHICS_GEN_HPP

