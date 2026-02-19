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

#include <dmsdk/dlib/hash.h>
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

/*# vertex declaration handle
 * @typedef
 * @name HVertexDeclaration
 */
typedef struct VertexDeclaration* HVertexDeclaration;

/*# vertex stream declaration handle
 * @typedef
 * @name HVertexStreamDeclaration
 */
typedef struct VertexStreamDeclaration* HVertexStreamDeclaration;

/*# vertex attribute
 * @typedef
 * @name VertexAttribute
 */
typedef struct VertexAttribute VertexAttribute;

/*# vertex step function
 * @enum
 * @name VertexAttributeStepFunction
 * @member VERTEX_STEP_FUNCTION_VERTEX
 * @member VERTEX_STEP_FUNCTION_INSTANCE
 */
typedef enum VertexAttributeStepFunction
{
    VERTEX_STEP_FUNCTION_VERTEX   = 0,
    VERTEX_STEP_FUNCTION_INSTANCE = 1,
} VertexAttributeStepFunction;

/*# vertex attribute data type
 * @enum
 * @name VertexAttributeDataType
 * @member VERTEX_ATTRIBUTE_DATA_TYPE_BYTE
 * @member VERTEX_ATTRIBUTE_DATA_TYPE_UNSIGNED_BYTE
 * @member VERTEX_ATTRIBUTE_DATA_TYPE_SHORT
 * @member VERTEX_ATTRIBUTE_DATA_TYPE_UNSIGNED_SHORT
 * @member VERTEX_ATTRIBUTE_DATA_TYPE_INT
 * @member VERTEX_ATTRIBUTE_DATA_TYPE_UNSIGNED_INT
 * @member VERTEX_ATTRIBUTE_DATA_TYPE_FLOAT
 */
typedef enum VertexAttributeDataType
{
    VERTEX_ATTRIBUTE_DATA_TYPE_BYTE           = 1,
    VERTEX_ATTRIBUTE_DATA_TYPE_UNSIGNED_BYTE  = 2,
    VERTEX_ATTRIBUTE_DATA_TYPE_SHORT          = 3,
    VERTEX_ATTRIBUTE_DATA_TYPE_UNSIGNED_SHORT = 4,
    VERTEX_ATTRIBUTE_DATA_TYPE_INT            = 5,
    VERTEX_ATTRIBUTE_DATA_TYPE_UNSIGNED_INT   = 6,
    VERTEX_ATTRIBUTE_DATA_TYPE_FLOAT          = 7,
} VertexAttributeDataType;

/*# vertex attribute vector type
 * @enum
 * @name VertexAttributeVectorType
 * @member VERTEX_ATTRIBUTE_VECTOR_TYPE_SCALAR
 * @member VERTEX_ATTRIBUTE_VECTOR_TYPE_VEC2
 * @member VERTEX_ATTRIBUTE_VECTOR_TYPE_VEC3
 * @member VERTEX_ATTRIBUTE_VECTOR_TYPE_VEC4
 * @member VERTEX_ATTRIBUTE_VECTOR_TYPE_MAT2
 * @member VERTEX_ATTRIBUTE_VECTOR_TYPE_MAT3
 * @member VERTEX_ATTRIBUTE_VECTOR_TYPE_MAT4
 */
typedef enum VertexAttributeVectorType
{
    VERTEX_ATTRIBUTE_VECTOR_TYPE_SCALAR = 1,
    VERTEX_ATTRIBUTE_VECTOR_TYPE_VEC2   = 2,
    VERTEX_ATTRIBUTE_VECTOR_TYPE_VEC3   = 3,
    VERTEX_ATTRIBUTE_VECTOR_TYPE_VEC4   = 4,
    VERTEX_ATTRIBUTE_VECTOR_TYPE_MAT2   = 5,
    VERTEX_ATTRIBUTE_VECTOR_TYPE_MAT3   = 6,
    VERTEX_ATTRIBUTE_VECTOR_TYPE_MAT4   = 7,
} VertexAttributeVectorType;

/*# vertex attribute semantic type
 * @enum
 * @name VertexAttributeSemanticType
 * @member VERTEX_ATTRIBUTE_SEMANTIC_TYPE_NONE
 * @member VERTEX_ATTRIBUTE_SEMANTIC_TYPE_POSITION
 * @member VERTEX_ATTRIBUTE_SEMANTIC_TYPE_TEXCOORD
 * @member VERTEX_ATTRIBUTE_SEMANTIC_TYPE_PAGE_INDEX
 * @member VERTEX_ATTRIBUTE_SEMANTIC_TYPE_COLOR
 * @member VERTEX_ATTRIBUTE_SEMANTIC_TYPE_NORMAL
 * @member VERTEX_ATTRIBUTE_SEMANTIC_TYPE_TANGENT
 * @member VERTEX_ATTRIBUTE_SEMANTIC_TYPE_WORLD_MATRIX
 * @member VERTEX_ATTRIBUTE_SEMANTIC_TYPE_NORMAL_MATRIX
 * @member VERTEX_ATTRIBUTE_SEMANTIC_TYPE_BONE_WEIGHTS
 * @member VERTEX_ATTRIBUTE_SEMANTIC_TYPE_BONE_INDICES
 */
typedef enum VertexAttributeSemanticType
{
    VERTEX_ATTRIBUTE_SEMANTIC_TYPE_NONE          = 1,
    VERTEX_ATTRIBUTE_SEMANTIC_TYPE_POSITION      = 2,
    VERTEX_ATTRIBUTE_SEMANTIC_TYPE_TEXCOORD      = 3,
    VERTEX_ATTRIBUTE_SEMANTIC_TYPE_PAGE_INDEX    = 4,
    VERTEX_ATTRIBUTE_SEMANTIC_TYPE_COLOR         = 5,
    VERTEX_ATTRIBUTE_SEMANTIC_TYPE_NORMAL        = 6,
    VERTEX_ATTRIBUTE_SEMANTIC_TYPE_TANGENT       = 7,
    VERTEX_ATTRIBUTE_SEMANTIC_TYPE_WORLD_MATRIX  = 8,
    VERTEX_ATTRIBUTE_SEMANTIC_TYPE_NORMAL_MATRIX = 9,
    VERTEX_ATTRIBUTE_SEMANTIC_TYPE_BONE_WEIGHTS  = 10,
    VERTEX_ATTRIBUTE_SEMANTIC_TYPE_BONE_INDICES  = 11,
} VertexAttributeSemanticType;

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

/*# pixel formats supported by textures
 * @enum
 * @name TextureFormat
 * @member TEXTURE_FORMAT_LUMINANCE
 * @member TEXTURE_FORMAT_LUMINANCE_ALPHA
 * @member TEXTURE_FORMAT_RGB
 * @member TEXTURE_FORMAT_RGBA
 * @member TEXTURE_FORMAT_RGB_16BPP
 * @member TEXTURE_FORMAT_RGBA_16BPP
 * @member TEXTURE_FORMAT_DEPTH
 * @member TEXTURE_FORMAT_STENCIL
 * @member TEXTURE_FORMAT_RGB_PVRTC_2BPPV1
 * @member TEXTURE_FORMAT_RGB_PVRTC_4BPPV1
 * @member TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1
 * @member TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1
 * @member TEXTURE_FORMAT_RGB_ETC1
 * @member TEXTURE_FORMAT_R_ETC2
 * @member TEXTURE_FORMAT_RG_ETC2
 * @member TEXTURE_FORMAT_RGBA_ETC2
 * @member TEXTURE_FORMAT_RGBA_ASTC_4X4
 * @member TEXTURE_FORMAT_RGB_BC1
 * @member TEXTURE_FORMAT_RGBA_BC3
 * @member TEXTURE_FORMAT_R_BC4
 * @member TEXTURE_FORMAT_RG_BC5
 * @member TEXTURE_FORMAT_RGBA_BC7
 * @member TEXTURE_FORMAT_RGB16F
 * @member TEXTURE_FORMAT_RGB32F
 * @member TEXTURE_FORMAT_RGBA16F
 * @member TEXTURE_FORMAT_RGBA32F
 * @member TEXTURE_FORMAT_R16F
 * @member TEXTURE_FORMAT_RG16F
 * @member TEXTURE_FORMAT_R32F
 * @member TEXTURE_FORMAT_RG32F
 * @member TEXTURE_FORMAT_RGBA32UI
 * @member TEXTURE_FORMAT_BGRA8U
 * @member TEXTURE_FORMAT_R32UI
 * @member TEXTURE_FORMAT_RGBA_ASTC_5X4
 * @member TEXTURE_FORMAT_RGBA_ASTC_5X5
 * @member TEXTURE_FORMAT_RGBA_ASTC_6X5
 * @member TEXTURE_FORMAT_RGBA_ASTC_6X6
 * @member TEXTURE_FORMAT_RGBA_ASTC_8X5
 * @member TEXTURE_FORMAT_RGBA_ASTC_8X6
 * @member TEXTURE_FORMAT_RGBA_ASTC_8X8
 * @member TEXTURE_FORMAT_RGBA_ASTC_10X5
 * @member TEXTURE_FORMAT_RGBA_ASTC_10X6
 * @member TEXTURE_FORMAT_RGBA_ASTC_10X8
 * @member TEXTURE_FORMAT_RGBA_ASTC_10X10
 * @member TEXTURE_FORMAT_RGBA_ASTC_12X10
 * @member TEXTURE_FORMAT_RGBA_ASTC_12X12
 * @member TEXTURE_FORMAT_COUNT
 */
typedef enum TextureFormat
{
    TEXTURE_FORMAT_LUMINANCE            = 0,
    TEXTURE_FORMAT_LUMINANCE_ALPHA      = 1,
    TEXTURE_FORMAT_RGB                  = 2,
    TEXTURE_FORMAT_RGBA                 = 3,
    TEXTURE_FORMAT_RGB_16BPP            = 4,
    TEXTURE_FORMAT_RGBA_16BPP           = 5,
    TEXTURE_FORMAT_DEPTH                = 6,
    TEXTURE_FORMAT_STENCIL              = 7,
    TEXTURE_FORMAT_RGB_PVRTC_2BPPV1     = 8,
    TEXTURE_FORMAT_RGB_PVRTC_4BPPV1     = 9,
    TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1    = 10,
    TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1    = 11,
    TEXTURE_FORMAT_RGB_ETC1             = 12,
    TEXTURE_FORMAT_R_ETC2               = 13,
    TEXTURE_FORMAT_RG_ETC2              = 14,
    TEXTURE_FORMAT_RGBA_ETC2            = 15,
    TEXTURE_FORMAT_RGBA_ASTC_4X4        = 16,
    TEXTURE_FORMAT_RGB_BC1              = 17,
    TEXTURE_FORMAT_RGBA_BC3             = 18,
    TEXTURE_FORMAT_R_BC4                = 19,
    TEXTURE_FORMAT_RG_BC5               = 20,
    TEXTURE_FORMAT_RGBA_BC7             = 21,
    TEXTURE_FORMAT_RGB16F               = 22,
    TEXTURE_FORMAT_RGB32F               = 23,
    TEXTURE_FORMAT_RGBA16F              = 24,
    TEXTURE_FORMAT_RGBA32F              = 25,
    TEXTURE_FORMAT_R16F                 = 26,
    TEXTURE_FORMAT_RG16F                = 27,
    TEXTURE_FORMAT_R32F                 = 28,
    TEXTURE_FORMAT_RG32F                = 29,
    TEXTURE_FORMAT_RGBA32UI             = 30,
    TEXTURE_FORMAT_BGRA8U               = 31,
    TEXTURE_FORMAT_R32UI                = 32,
    TEXTURE_FORMAT_RGBA_ASTC_5X4        = 33,
    TEXTURE_FORMAT_RGBA_ASTC_5X5        = 34,
    TEXTURE_FORMAT_RGBA_ASTC_6X5        = 35,
    TEXTURE_FORMAT_RGBA_ASTC_6X6        = 36,
    TEXTURE_FORMAT_RGBA_ASTC_8X5        = 37,
    TEXTURE_FORMAT_RGBA_ASTC_8X6        = 38,
    TEXTURE_FORMAT_RGBA_ASTC_8X8        = 39,
    TEXTURE_FORMAT_RGBA_ASTC_10X5       = 40,
    TEXTURE_FORMAT_RGBA_ASTC_10X6       = 41,
    TEXTURE_FORMAT_RGBA_ASTC_10X8       = 42,
    TEXTURE_FORMAT_RGBA_ASTC_10X10      = 43,
    TEXTURE_FORMAT_RGBA_ASTC_12X10      = 44,
    TEXTURE_FORMAT_RGBA_ASTC_12X12      = 45,

    TEXTURE_FORMAT_COUNT
} TextureFormat;

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

/*# create a vertex stream declaration
 * @name VertexStreamDeclarationNew
 * @param context [type:HGraphicsContext] graphics context handle
 * @return declaration [type:HVertexStreamDeclaration] vertex stream declaration handle
 */
HVertexStreamDeclaration VertexStreamDeclarationNew(HGraphicsContext context);

/*# create a vertex stream declaration with explicit step function
 * @name VertexStreamDeclarationNewStep
 * @param context [type:HGraphicsContext] graphics context handle
 * @param step_function [type:VertexAttributeStepFunction] vertex step function value
 * @return declaration [type:HVertexStreamDeclaration] vertex stream declaration handle
 */
HVertexStreamDeclaration VertexStreamDeclarationNewStep(HGraphicsContext context, VertexAttributeStepFunction step_function);

/*# delete a vertex stream declaration
 * @name VertexStreamDeclarationDelete
 * @param stream_declaration [type:HVertexStreamDeclaration] vertex stream declaration handle
 */
void VertexStreamDeclarationDelete(HVertexStreamDeclaration stream_declaration);

/*# add a stream to a vertex stream declaration
 * @name VertexStreamDeclarationAdd
 * @param stream_declaration [type:HVertexStreamDeclaration] vertex stream declaration handle
 * @param name [type:const char*] stream name
 * @param size [type:uint32_t] number of components in stream
 * @param type [type:VertexAttributeDataType] stream data type value
 * @param normalize [type:bool] normalize stream values
 */
void VertexStreamDeclarationAdd(HVertexStreamDeclaration stream_declaration, const char* name, uint32_t size, VertexAttributeDataType type, bool normalize);

/*# add a stream (hashed name) to a vertex stream declaration
 * @name VertexStreamDeclarationAddHash
 * @param stream_declaration [type:HVertexStreamDeclaration] vertex stream declaration handle
 * @param name_hash [type:dmhash_t] stream name hash
 * @param size [type:uint32_t] number of components in stream
 * @param type [type:VertexAttributeDataType] stream data type value
 * @param normalize [type:bool] normalize stream values
 */
void VertexStreamDeclarationAddHash(HVertexStreamDeclaration stream_declaration, dmhash_t name_hash, uint32_t size, VertexAttributeDataType type, bool normalize);

/*# create a vertex declaration
 * @name VertexDeclarationNew
 * @param context [type:HGraphicsContext] graphics context handle
 * @param stream_declaration [type:HVertexStreamDeclaration] vertex stream declaration handle
 * @return declaration [type:HVertexDeclaration] vertex declaration handle
 */
HVertexDeclaration VertexDeclarationNew(HGraphicsContext context, HVertexStreamDeclaration stream_declaration);

/*# create a vertex declaration with explicit stride
 * @name VertexDeclarationNewStride
 * @param context [type:HGraphicsContext] graphics context handle
 * @param stream_declaration [type:HVertexStreamDeclaration] vertex stream declaration handle
 * @param stride [type:uint32_t] vertex stride in bytes
 * @return declaration [type:HVertexDeclaration] vertex declaration handle
 */
HVertexDeclaration VertexDeclarationNewStride(HGraphicsContext context, HVertexStreamDeclaration stream_declaration, uint32_t stride);

/*# delete a vertex declaration
 * @name VertexDeclarationDelete
 * @param vertex_declaration [type:HVertexDeclaration] vertex declaration handle
 */
void VertexDeclarationDelete(HVertexDeclaration vertex_declaration);

/*# enable a vertex declaration
 * @name VertexDeclarationEnable
 * @param context [type:HGraphicsContext] graphics context handle
 * @param vertex_declaration [type:HVertexDeclaration] vertex declaration handle
 * @param binding_index [type:uint32_t] vertex buffer binding index
 * @param base_offset [type:uint32_t] base offset in bytes
 * @param program [type:uintptr_t] program handle
 */
void VertexDeclarationEnable(HGraphicsContext context, HVertexDeclaration vertex_declaration, uint32_t binding_index, uint32_t base_offset, uintptr_t program);

/*# disable a vertex declaration
 * @name VertexDeclarationDisable
 * @param context [type:HGraphicsContext] graphics context handle
 * @param vertex_declaration [type:HVertexDeclaration] vertex declaration handle
 */
void VertexDeclarationDisable(HGraphicsContext context, HVertexDeclaration vertex_declaration);

/*# get stream offset from a vertex declaration
 * @name VertexDeclarationGetStreamOffset
 * @param vertex_declaration [type:HVertexDeclaration] vertex declaration handle
 * @param name_hash [type:dmhash_t] stream name hash
 * @return offset [type:uint32_t] stream offset or 0xFFFFFFFF if not found
 */
uint32_t VertexDeclarationGetStreamOffset(HVertexDeclaration vertex_declaration, dmhash_t name_hash);

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // DMSDK_GRAPHICS_H
