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

/*# program handle
 * @typedef
 * @name HProgram
 */
typedef uintptr_t HProgram;

/*# vertex buffer handle
 * @typedef
 * @name HVertexBuffer
 */
typedef uintptr_t HVertexBuffer;

/*# index buffer handle
 * @typedef
 * @name HIndexBuffer
 */
typedef uintptr_t HIndexBuffer;

/*# uniform location handle
 * @typedef
 * @name HUniformLocation
 */
typedef int64_t HUniformLocation;

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

/*# primitive drawing modes
 * @enum
 * @name PrimitiveType
 * @member PRIMITIVE_LINES
 * @member PRIMITIVE_TRIANGLES
 * @member PRIMITIVE_TRIANGLE_STRIP
 */
typedef enum PrimitiveType
{
    PRIMITIVE_LINES          = 0,
    PRIMITIVE_TRIANGLES      = 1,
    PRIMITIVE_TRIANGLE_STRIP = 2,
} PrimitiveType;

/*# buffer usage hint
 * @enum
 * @name BufferUsage
 * @member BUFFER_USAGE_STREAM_DRAW
 * @member BUFFER_USAGE_STATIC_DRAW
 * @member BUFFER_USAGE_DYNAMIC_DRAW
 */
typedef enum BufferUsage
{
    BUFFER_USAGE_STREAM_DRAW  = 0,
    BUFFER_USAGE_STATIC_DRAW  = 1,
    BUFFER_USAGE_DYNAMIC_DRAW = 2,
} BufferUsage;

/*# index buffer format
 * @enum
 * @name IndexBufferFormat
 * @member INDEXBUFFER_FORMAT_16
 * @member INDEXBUFFER_FORMAT_32
 */
typedef enum IndexBufferFormat
{
    INDEXBUFFER_FORMAT_16 = 0,
    INDEXBUFFER_FORMAT_32 = 1,
} IndexBufferFormat;

/*# graphics data type
 * @enum
 * @name Type
 * @member TYPE_BYTE
 * @member TYPE_UNSIGNED_BYTE
 * @member TYPE_SHORT
 * @member TYPE_UNSIGNED_SHORT
 * @member TYPE_INT
 * @member TYPE_UNSIGNED_INT
 * @member TYPE_FLOAT
 * @member TYPE_FLOAT_VEC4
 * @member TYPE_FLOAT_MAT4
 * @member TYPE_SAMPLER_2D
 * @member TYPE_SAMPLER_CUBE
 * @member TYPE_SAMPLER_2D_ARRAY
 * @member TYPE_FLOAT_VEC2
 * @member TYPE_FLOAT_VEC3
 * @member TYPE_FLOAT_MAT2
 * @member TYPE_FLOAT_MAT3
 * @member TYPE_IMAGE_2D
 * @member TYPE_TEXTURE_2D
 * @member TYPE_SAMPLER
 * @member TYPE_TEXTURE_2D_ARRAY
 * @member TYPE_TEXTURE_CUBE
 * @member TYPE_SAMPLER_3D
 * @member TYPE_TEXTURE_3D
 * @member TYPE_IMAGE_3D
 * @member TYPE_SAMPLER_3D_ARRAY
 * @member TYPE_TEXTURE_3D_ARRAY
 */
typedef enum Type
{
    TYPE_BYTE             = 0,
    TYPE_UNSIGNED_BYTE    = 1,
    TYPE_SHORT            = 2,
    TYPE_UNSIGNED_SHORT   = 3,
    TYPE_INT              = 4,
    TYPE_UNSIGNED_INT     = 5,
    TYPE_FLOAT            = 6,
    TYPE_FLOAT_VEC4       = 7,
    TYPE_FLOAT_MAT4       = 8,
    TYPE_SAMPLER_2D       = 9,
    TYPE_SAMPLER_CUBE     = 10,
    TYPE_SAMPLER_2D_ARRAY = 11,
    TYPE_FLOAT_VEC2       = 12,
    TYPE_FLOAT_VEC3       = 13,
    TYPE_FLOAT_MAT2       = 14,
    TYPE_FLOAT_MAT3       = 15,
    TYPE_IMAGE_2D         = 16,
    TYPE_TEXTURE_2D       = 17,
    TYPE_SAMPLER          = 18,
    TYPE_TEXTURE_2D_ARRAY = 19,
    TYPE_TEXTURE_CUBE     = 20,
    TYPE_SAMPLER_3D       = 21,
    TYPE_TEXTURE_3D       = 22,
    TYPE_IMAGE_3D         = 23,
    TYPE_SAMPLER_3D_ARRAY = 24,
    TYPE_TEXTURE_3D_ARRAY = 25,
} Type;

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

/*# install graphics adapter
 * @name GraphicsInstallAdapter
 * @param family [type:AdapterFamily] graphics adapter family
 * @return ok [type:bool] true if adapter was installed, false otherwise
 */
bool GraphicsInstallAdapter(AdapterFamily family);

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

/*# begin frame
 * @name GraphicsBeginFrame
 * @param context [type:HGraphicsContext] graphics context handle
 */
void GraphicsBeginFrame(HGraphicsContext context);

/*# present frame
 * @note Must match each call to GraphicsBeginFrame() with GraphicsFlip()
 * @name GraphicsFlip
 * @param context [type:HGraphicsContext] graphics context handle
 */
void GraphicsFlip(HGraphicsContext context);

/*# set viewport rectangle
 * @name GraphicsSetViewport
 * @param context [type:HGraphicsContext] graphics context handle
 * @param x [type:int32_t] x coordinate
 * @param y [type:int32_t] y coordinate
 * @param width [type:int32_t] width
 * @param height [type:int32_t] height
 */
void GraphicsSetViewport(HGraphicsContext context, int32_t x, int32_t y, int32_t width, int32_t height);

/*# clear buffers in the current render target
 * @name GraphicsClear
 * @param context [type:HGraphicsContext] graphics context handle
 * @param flags [type:uint32_t] buffer clear mask using BUFFER_TYPE_* bits
 * @param red [type:uint8_t] red clear value
 * @param green [type:uint8_t] green clear value
 * @param blue [type:uint8_t] blue clear value
 * @param alpha [type:uint8_t] alpha clear value
 * @param depth [type:float] depth clear value
 * @param stencil [type:uint32_t] stencil clear value
 */
void GraphicsClear(HGraphicsContext context, uint32_t flags, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha, float depth, uint32_t stencil);

/*# draw primitives from currently bound buffers
 * @name GraphicsDraw
 * @param context [type:HGraphicsContext] graphics context handle
 * @param prim_type [type:PrimitiveType] primitive topology
 * @param first [type:uint32_t] first vertex index
 * @param count [type:uint32_t] vertex count
 * @param instance_count [type:uint32_t] instance count
 */
void GraphicsDraw(HGraphicsContext context, PrimitiveType prim_type, uint32_t first, uint32_t count, uint32_t instance_count);

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

/*# create a vertex buffer
 * @name VertexBufferNew
 * @param context [type:HGraphicsContext] graphics context handle
 * @param size [type:uint32_t] buffer size in bytes
 * @param data [type:const void*] initial data
 * @param usage [type:BufferUsage] buffer usage hint
 * @return buffer [type:HVertexBuffer] vertex buffer handle
 */
HVertexBuffer VertexBufferNew(HGraphicsContext context, uint32_t size, const void* data, BufferUsage usage);

/*# delete a vertex buffer
 * @name VertexBufferDelete
 * @param buffer [type:HVertexBuffer] vertex buffer handle
 */
void VertexBufferDelete(HVertexBuffer buffer);

/*# upload vertex buffer data
 * @name VertexBufferSetData
 * @param buffer [type:HVertexBuffer] vertex buffer handle
 * @param size [type:uint32_t] data size in bytes
 * @param data [type:const void*] source data
 * @param usage [type:BufferUsage] buffer usage hint
 */
void VertexBufferSetData(HVertexBuffer buffer, uint32_t size, const void* data, BufferUsage usage);

/*# upload partial vertex buffer data
 * @name VertexBufferSetSubData
 * @param buffer [type:HVertexBuffer] vertex buffer handle
 * @param offset [type:uint32_t] destination offset in bytes
 * @param size [type:uint32_t] data size in bytes
 * @param data [type:const void*] source data
 */
void VertexBufferSetSubData(HVertexBuffer buffer, uint32_t offset, uint32_t size, const void* data);

/*# bind a vertex buffer
 * @name VertexBufferEnable
 * @param context [type:HGraphicsContext] graphics context handle
 * @param buffer [type:HVertexBuffer] vertex buffer handle
 * @param binding_index [type:uint32_t] binding index
 */
void VertexBufferEnable(HGraphicsContext context, HVertexBuffer buffer, uint32_t binding_index);

/*# unbind a vertex buffer
 * @name VertexBufferDisable
 * @param context [type:HGraphicsContext] graphics context handle
 * @param buffer [type:HVertexBuffer] vertex buffer handle
 */
void VertexBufferDisable(HGraphicsContext context, HVertexBuffer buffer);

/*# create an index buffer
 * @name IndexBufferNew
 * @param context [type:HGraphicsContext] graphics context handle
 * @param size [type:uint32_t] buffer size in bytes
 * @param data [type:const void*] initial data
 * @param usage [type:BufferUsage] buffer usage hint
 * @return buffer [type:HIndexBuffer] index buffer handle
 */
HIndexBuffer IndexBufferNew(HGraphicsContext context, uint32_t size, const void* data, BufferUsage usage);

/*# delete an index buffer
 * @name IndexBufferDelete
 * @param buffer [type:HIndexBuffer] index buffer handle
 */
void IndexBufferDelete(HIndexBuffer buffer);

/*# upload index buffer data
 * @name IndexBufferSetData
 * @param buffer [type:HIndexBuffer] index buffer handle
 * @param size [type:uint32_t] data size in bytes
 * @param data [type:const void*] source data
 * @param usage [type:BufferUsage] buffer usage hint
 */
void IndexBufferSetData(HIndexBuffer buffer, uint32_t size, const void* data, BufferUsage usage);

/*# upload partial index buffer data
 * @name IndexBufferSetSubData
 * @param buffer [type:HIndexBuffer] index buffer handle
 * @param offset [type:uint32_t] destination offset in bytes
 * @param size [type:uint32_t] data size in bytes
 * @param data [type:const void*] source data
 */
void IndexBufferSetSubData(HIndexBuffer buffer, uint32_t offset, uint32_t size, const void* data);

/*# check index buffer format support
 * @name IndexBufferIsFormatSupported
 * @param context [type:HGraphicsContext] graphics context handle
 * @param format [type:IndexBufferFormat] index format
 * @return supported [type:bool] true if supported
 */
bool IndexBufferIsFormatSupported(HGraphicsContext context, IndexBufferFormat format);

/*# delete a shader program
 * @name ProgramDelete
 * @param context [type:HGraphicsContext] graphics context handle
 * @param program [type:HProgram] program handle
 */
void ProgramDelete(HGraphicsContext context, HProgram program);

/*# enable a shader program for rendering
 * @name ProgramEnable
 * @param context [type:HGraphicsContext] graphics context handle
 * @param program [type:HProgram] program handle
 */
void ProgramEnable(HGraphicsContext context, HProgram program);

/*# disable the currently active shader program
 * @name ProgramDisable
 * @param context [type:HGraphicsContext] graphics context handle
 */
void ProgramDisable(HGraphicsContext context);

/*# bind a sampler uniform to a texture unit
 * @name ProgramSetSampler
 * @param context [type:HGraphicsContext] graphics context handle
 * @param location [type:HUniformLocation] uniform location
 * @param unit [type:int32_t] texture unit index
 */
void ProgramSetSampler(HGraphicsContext context, HUniformLocation location, int32_t unit);

/*# find uniform location by hashed uniform name
 * @name ProgramFindUniformLocationHash
 * @param program [type:HProgram] program handle
 * @param name_hash [type:dmhash_t] hash of the uniform name
 * @return location [type:HUniformLocation] uniform location, or INVALID_UNIFORM_LOCATION if not found
 */
HUniformLocation ProgramFindUniformLocationHash(HProgram program, dmhash_t name_hash);

/*# find uniform location by uniform name
 * @name ProgramFindUniformLocation
 * @param program [type:HProgram] program handle
 * @param name [type:const char*] uniform name
 * @return location [type:HUniformLocation] uniform location, or INVALID_UNIFORM_LOCATION if not found
 */
HUniformLocation ProgramFindUniformLocation(HProgram program, const char* name);

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // DMSDK_GRAPHICS_H
