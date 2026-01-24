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

#include <stdint.h>

#include <graphics/graphics_ddf.h>

/*# Graphics API documentation
 *
 * Graphics API
 *
 * @document
 * @name Graphics
 * @namespace dmGraphics
 * @language C++
 */

namespace dmGraphics
{
    /*#
     * Context handle
     * @typedef
     * @name HContext
     */
    typedef void* HContext;

    /*#
     * Texture handle
     * @typedef
     * @name HTexture
     */
    typedef uint64_t HTexture;

    /*#
     * Rendertarget handle
     * @typedef
     * @name HRenderTarget
     */
    typedef uint64_t HRenderTarget;

    /*#
     * Program handle
     * @typedef
     * @name HProgram
     */
    typedef uintptr_t HProgram;

    /*#
     * Vertex buffer handle
     * @typedef
     * @name HVertexBuffer
     */
    typedef uintptr_t HVertexBuffer;

    /*#
     * Index buffer handle
     * @typedef
     * @name HIndexBuffer
     */
    typedef uintptr_t HIndexBuffer;

    /*#
     * Storage buffer handle
     * @typedef
     * @name HStorageBuffer
     */
    typedef uintptr_t HStorageBuffer;

    /*#
     * Uniform location handle
     * @typedef
     * @name HUniformLocation
     */
    typedef int64_t HUniformLocation;

    /*#
     * Vertex declaration handle
     * @typedef
     * @name HVertexDeclaration
     */
    typedef struct VertexDeclaration* HVertexDeclaration;

    /*#
     * Vertex stream declaration handle
     * @typedef
     * @name HVertexStreamDeclaration
     */
    typedef struct VertexStreamDeclaration* HVertexStreamDeclaration;

    /*#
     * PipelineState handle
     * @typedef
     * @name HPipelineState
     */
    typedef struct PipelineState* HPipelineState;

    /*#
     * Invalid stream offset
     * @constant
     * @name INVALID_STREAM_OFFSET
     */
    const uint32_t INVALID_STREAM_OFFSET = 0xFFFFFFFF;

    /*#
     * Max buffer color attachments
     * @constant
     * @name MAX_BUFFER_COLOR_ATTACHMENTS
     */
    const uint8_t  MAX_BUFFER_COLOR_ATTACHMENTS = 4;

    /*#
     * Function's call result code
     * @enum
     * @name HandleResult
     * @member HANDLE_RESULT_OK             The function's call succeeded and returned a valid result
     * @member HANDLE_RESULT_NOT_AVAILABLE  The function is not supported by the current graphics backend
     * @member HANDLE_RESULT_ERROR          An error occurred while function call
     */
    enum HandleResult
    {
        HANDLE_RESULT_OK = 0,
        HANDLE_RESULT_NOT_AVAILABLE = -1,
        HANDLE_RESULT_ERROR = -2
    };

    /*#
     * Attachment points for render targets
     * @enum
     * @name RenderTargetAttachment
     * @member ATTACHMENT_COLOR     A color buffer attachment (used for rendering visible output)
     * @member ATTACHMENT_DEPTH     A depth buffer attachment (used for depth testing)
     * @member ATTACHMENT_STENCIL   A stencil buffer attachment (used for stencil operations)
     */
    enum RenderTargetAttachment
    {
        ATTACHMENT_COLOR     = 0,
        ATTACHMENT_DEPTH     = 1,
        ATTACHMENT_STENCIL   = 2,
        MAX_ATTACHMENT_COUNT = 3
    };

    /*#
     * Defines how an attachment should be treated at the start and end of a render pass
     * @enum
     * @name AttachmentOp
     * @member ATTACHMENT_OP_DONT_CARE Ignore existing content, no guarantees about the result
     * @member ATTACHMENT_OP_LOAD      Preserve the existing contents of the attachment
     * @member ATTACHMENT_OP_STORE     Store the attachment’s results after the pass finishes
     * @member ATTACHMENT_OP_CLEAR     Clear the attachment to a predefined value at the beginning of the pass
     */
    enum AttachmentOp
    {
        ATTACHMENT_OP_DONT_CARE,
        ATTACHMENT_OP_LOAD,
        ATTACHMENT_OP_STORE,
        ATTACHMENT_OP_CLEAR,
    };

    /*#
     * Pixel formats supported by textures.
     * Includes uncompressed, compressed, and floating-point variants
     * @enum
     * @name TextureFormat
     * @member TEXTURE_FORMAT_LUMINANCE            Single-channel grayscale
     * @member TEXTURE_FORMAT_LUMINANCE_ALPHA      Two-channel grayscale + alpha
     * @member TEXTURE_FORMAT_RGB                  Standard 24-bit RGB color
     * @member TEXTURE_FORMAT_RGBA                 Standard 32-bit RGBA color
     * @member TEXTURE_FORMAT_RGB_16BPP            Packed 16-bit RGB (lower precision, saves memory)
     * @member TEXTURE_FORMAT_RGBA_16BPP           Packed 16-bit RGBA
     * @member TEXTURE_FORMAT_DEPTH                Depth buffer texture (used for depth testing)
     * @member TEXTURE_FORMAT_STENCIL              Stencil buffer texture
     * @member TEXTURE_FORMAT_RGB_PVRTC_2BPPV1     PVRTC compressed RGB at 2 bits per pixel
     * @member TEXTURE_FORMAT_RGB_PVRTC_4BPPV1     PVRTC compressed RGB at 4 bits per pixel
     * @member TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1    PVRTC compressed RGBA at 2 bits per pixel
     * @member TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1    PVRTC compressed RGBA at 4 bits per pixel
     * @member TEXTURE_FORMAT_RGB_ETC1             ETC1 compressed RGB (no alpha support)
     * @member TEXTURE_FORMAT_R_ETC2               ETC2 single-channel
     * @member TEXTURE_FORMAT_RG_ETC2              ETC2 two-channel
     * @member TEXTURE_FORMAT_RGBA_ETC2            ETC2 four-channel (with alpha)
     * @member TEXTURE_FORMAT_RGBA_ASTC_4X4        ASTC block-compressed 4×4
     * @member TEXTURE_FORMAT_RGB_BC1              BC1/DXT1 compressed RGB
     * @member TEXTURE_FORMAT_RGBA_BC3             BC3/DXT5 compressed RGBA
     * @member TEXTURE_FORMAT_R_BC4                BC4 single-channel
     * @member TEXTURE_FORMAT_RG_BC5               BC5 two-channel
     * @member TEXTURE_FORMAT_RGBA_BC7             BC7 high-quality compressed RGBA
     * @member TEXTURE_FORMAT_RGB16F               Half-precision float RGB
     * @member TEXTURE_FORMAT_RGB32F               Full 32-bit float RGB
     * @member TEXTURE_FORMAT_RGBA16F              Half-precision float RGBA
     * @member TEXTURE_FORMAT_RGBA32F              Full 32-bit float RGBA
     * @member TEXTURE_FORMAT_R16F                 Half-precision float single channel
     * @member TEXTURE_FORMAT_RG16F                Half-precision float two channels
     * @member TEXTURE_FORMAT_R32F                 Full 32-bit float single channel
     * @member TEXTURE_FORMAT_RG32F                Full 32-bit float two channels
     * @member TEXTURE_FORMAT_RGBA32UI             Internal: 32-bit unsigned integer RGBA (not script-exposed)
     * @member TEXTURE_FORMAT_BGRA8U               Internal: 32-bit BGRA layout
     * @member TEXTURE_FORMAT_R32UI                Internal: 32-bit unsigned integer single channel
     * @member TEXTURE_FORMAT_RGBA_ASTC_5X4        ASTC 5x4 block compression
     * @member TEXTURE_FORMAT_RGBA_ASTC_5X5        ASTC 5x5 block compression
     * @member TEXTURE_FORMAT_RGBA_ASTC_6X5        ASTC 6x5 block compression
     * @member TEXTURE_FORMAT_RGBA_ASTC_6X6        ASTC 6x6 block compression
     * @member TEXTURE_FORMAT_RGBA_ASTC_8X5        ASTC 8x5 block compression
     * @member TEXTURE_FORMAT_RGBA_ASTC_8X6        ASTC 8x6 block compression
     * @member TEXTURE_FORMAT_RGBA_ASTC_8X8        ASTC 8x8 block compression
     * @member TEXTURE_FORMAT_RGBA_ASTC_10X5       ASTC 10x5 block compression
     * @member TEXTURE_FORMAT_RGBA_ASTC_10X6       ASTC 10x6 block compression
     * @member TEXTURE_FORMAT_RGBA_ASTC_10X8       ASTC 10x8 block compression
     * @member TEXTURE_FORMAT_RGBA_ASTC_10X10      ASTC 10x10 block compression
     * @member TEXTURE_FORMAT_RGBA_ASTC_12X10      ASTC 12x10 block compression
     * @member TEXTURE_FORMAT_RGBA_ASTC_12X12      ASTC 12x12 block compression
     */
    enum TextureFormat
    {
        TEXTURE_FORMAT_LUMINANCE            = 0,
        TEXTURE_FORMAT_LUMINANCE_ALPHA      = 1,
        TEXTURE_FORMAT_RGB                  = 2,
        TEXTURE_FORMAT_RGBA                 = 3,
        TEXTURE_FORMAT_RGB_16BPP            = 4,
        TEXTURE_FORMAT_RGBA_16BPP           = 5,
        TEXTURE_FORMAT_DEPTH                = 6,
        TEXTURE_FORMAT_STENCIL              = 7,
        // Compressed formats
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
        // Floating point texture formats
        TEXTURE_FORMAT_RGB16F               = 22,
        TEXTURE_FORMAT_RGB32F               = 23,
        TEXTURE_FORMAT_RGBA16F              = 24,
        TEXTURE_FORMAT_RGBA32F              = 25,
        TEXTURE_FORMAT_R16F                 = 26,
        TEXTURE_FORMAT_RG16F                = 27,
        TEXTURE_FORMAT_R32F                 = 28,
        TEXTURE_FORMAT_RG32F                = 29,
        // Internal formats (not exposed via script APIs)
        TEXTURE_FORMAT_RGBA32UI             = 30,
        TEXTURE_FORMAT_BGRA8U               = 31,
        TEXTURE_FORMAT_R32UI                = 32,

        // ASTC Formats
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
    };

    /*#
     * Texture data upload status flags
     * @enum
     * @name TextureStatusFlags
     * @member TEXTURE_STATUS_OK            Texture updated and ready-to-use
     * @member TEXTURE_STATUS_DATA_PENDING  Data upload to the texture is in progress
     */
    enum TextureStatusFlags
    {
        TEXTURE_STATUS_OK               = 0,
        TEXTURE_STATUS_DATA_PENDING     = (1 << 0)
    };

    /*#
     * Get the attachment texture from a render target. Returns zero if no such attachment texture exists.
     * @name GetRenderTargetAttachment
     * @param context [type:dmGraphics::HContext] Graphics context
     * @param render_target [type:dmGraphics::HRenderTarget] the render target
     * @param attachment_type [type:dmGraphics::RenderTargetAttachment] the attachment to get
     * @return attachment [type:dmGraphics::HTexture] the attachment texture
     */
    HTexture GetRenderTargetAttachment(HContext context, HRenderTarget render_target, RenderTargetAttachment attachment_type);

    /*#
     * Get the native graphics API texture object from an engine texture handle. This depends on the graphics backend and is not
     * guaranteed to be implemented on the currently running adapter.
     * @name GetTextureHandle
     * @param texture [type:dmGraphics::HTexture] the texture handle
     * @param out_handle [type:void**] a pointer to where the raw object should be stored
     * @return handle_result [type:dmGraphics::HandleResult] the result of the query
     */
    HandleResult GetTextureHandle(HTexture texture, void** out_handle);

    /*#
     * Depth and alpha test comparison functions.
     * Defines how incoming values are compared against stored ones
     * @enum
     * @name CompareFunc
     * @member COMPARE_FUNC_NEVER        Never passes.
     * @member COMPARE_FUNC_LESS         Passes if incoming < stored
     * @member COMPARE_FUNC_LEQUAL       Passes if incoming <= stored
     * @member COMPARE_FUNC_GREATER      Passes if incoming > stored
     * @member COMPARE_FUNC_GEQUAL       Passes if incoming >= stored
     * @member COMPARE_FUNC_EQUAL        Passes if incoming == stored
     * @member COMPARE_FUNC_NOTEQUAL     Passes if incoming != stored
     * @member COMPARE_FUNC_ALWAYS       Always passes (ignores stored values)
     */
    enum CompareFunc
    {
        COMPARE_FUNC_NEVER    = 0,
        COMPARE_FUNC_LESS     = 1,
        COMPARE_FUNC_LEQUAL   = 2,
        COMPARE_FUNC_GREATER  = 3,
        COMPARE_FUNC_GEQUAL   = 4,
        COMPARE_FUNC_EQUAL    = 5,
        COMPARE_FUNC_NOTEQUAL = 6,
        COMPARE_FUNC_ALWAYS   = 7,
    };

    /*#
     * @enum
     * @name FaceWinding
     * @member FACE_WINDING_CCW
     * @member FACE_WINDING_CW
     */
    enum FaceWinding
    {
        FACE_WINDING_CCW = 0,
        FACE_WINDING_CW  = 1,
    };

    // Face type
    enum FaceType
    {
        FACE_TYPE_FRONT          = 0,
        FACE_TYPE_BACK           = 1,
        FACE_TYPE_FRONT_AND_BACK = 2,
    };

    /*#
     * Stencil test actions.
     * Defines what happens to a stencil buffer value depending on the outcome of the stencil/depth test.
     * @enum
     * @name StencilOp
     * @member STENCIL_OP_KEEP            Keep the current stencil value
     * @member STENCIL_OP_ZERO            Set stencil value to 0
     * @member STENCIL_OP_REPLACE         Replace stencil value with reference value
     * @member STENCIL_OP_INCR            Increment stencil value (clamps at max)
     * @member STENCIL_OP_INCR_WRAP       Increment stencil value, wrapping around
     * @member STENCIL_OP_DECR            Decrement stencil value (clamps at 0)
     * @member STENCIL_OP_DECR_WRAP       Decrement stencil value, wrapping around
     * @member STENCIL_OP_INVERT          Bitwise invert stencil value
     */
    enum StencilOp
    {
        STENCIL_OP_KEEP      = 0,
        STENCIL_OP_ZERO      = 1,
        STENCIL_OP_REPLACE   = 2,
        STENCIL_OP_INCR      = 3,
        STENCIL_OP_INCR_WRAP = 4,
        STENCIL_OP_DECR      = 5,
        STENCIL_OP_DECR_WRAP = 6,
        STENCIL_OP_INVERT    = 7,
    };

    /*#
     * Buffer usage hints.
     * Indicates how often the data in a buffer will be updated.
     * Helps the driver optimize memory placement
     * @enum
     * @name BufferUsage
     * @member BUFFER_USAGE_STREAM_DRAW     Updated every frame, used once (e.g. dynamic geometry)
     * @member BUFFER_USAGE_DYNAMIC_DRAW    Updated occasionally, used many times
     * @member BUFFER_USAGE_STATIC_DRAW     Set once, used many times (e.g. meshes, textures). Preferred for buffers that never change
     */
    enum BufferUsage
    {
        BUFFER_USAGE_STREAM_DRAW  = 0,
        BUFFER_USAGE_DYNAMIC_DRAW = 1,
        BUFFER_USAGE_STATIC_DRAW  = 2,
        BUFFER_USAGE_TRANSFER     = 4,
    };

    /*#
     * @enum
     * @name BufferAccess
     * @member BUFFER_ACCESS_READ_ONLY
     * @member BUFFER_ACCESS_WRITE_ONLY
     * @member BUFFER_ACCESS_READ_WRITE
     */
    enum BufferAccess
    {
        BUFFER_ACCESS_READ_ONLY  = 0,
        BUFFER_ACCESS_WRITE_ONLY = 1,
        BUFFER_ACCESS_READ_WRITE = 2,
    };


    /*#
     * Index buffer element types.
     * Defines the integer size used for vertex indices
     * @enum
     * @name IndexBufferFormat
     * @member INDEXBUFFER_FORMAT_16    16-bit unsigned integers (max 65535 vertices)
     * @member INDEXBUFFER_FORMAT_32    32-bit unsigned integers (supports larger meshes)
     */
    enum IndexBufferFormat
    {
        INDEXBUFFER_FORMAT_16 = 0,
        INDEXBUFFER_FORMAT_32 = 1,
    };

    /*#
     * Primitive drawing modes.
     * Defines how vertex data is assembled into primitives
     * @enum
     * @name PrimitiveType
     * @member PRIMITIVE_LINES          Each pair of vertices forms a line
     * @member PRIMITIVE_TRIANGLES      Each group of 3 vertices forms a triangle
     * @member PRIMITIVE_TRIANGLE_STRIP Connected strip of triangles (shares vertices)
     */
    enum PrimitiveType
    {
        PRIMITIVE_LINES          = 0,
        PRIMITIVE_TRIANGLES      = 1,
        PRIMITIVE_TRIANGLE_STRIP = 2,
    };

    /*#
     * Data type.
     * Represents scalar, vector, matrix, image, or sampler types used
     * for vertex attributes, uniforms, and shader interface definitions
     * @enum
     * @name Type
     * @member TYPE_BYTE                Signed 8-bit integer. Compact storage, often used for colors, normals, or compressed vertex attributes
     * @member TYPE_UNSIGNED_BYTE       Unsigned 8-bit integer. Common for color channels (0–255) or normalized texture data
     * @member TYPE_SHORT               Signed 16-bit integer. Used for medium-range numeric attributes such as bone weights or coordinates with normalization
     * @member TYPE_UNSIGNED_SHORT      Unsigned 16-bit integer. Often used for indices or normalized attributes when extra precision over bytes is required
     * @member TYPE_INT                 Signed 32-bit integer. Typically used for uniform values, shader constants, or counters
     * @member TYPE_UNSIGNED_INT        Unsigned 32-bit integer. Used for indices, IDs, or GPU counters
     * @member TYPE_FLOAT               32-bit floating point. Standard for most vertex attributes and uniform values (positions, UVs, weights)
     * @member TYPE_FLOAT_VEC4          4-component floating-point vector (`vec4` in GLSL). Typically used for homogeneous coordinates, colors (RGBA), or combined attributes
     * @member TYPE_FLOAT_MAT4          4x4 floating-point matrix (`mat4` in GLSL). Standard for 3D transformations (model, view, projection)
     * @member TYPE_SAMPLER_2D          2D texture sampler. Standard type for most texture lookups
     * @member TYPE_SAMPLER_CUBE        Cube map sampler. Used for environment mapping, reflections, and skyboxes
     * @member TYPE_SAMPLER_2D_ARRAY    Array of 2D texture samplers. Enables efficient texture indexing when using multiple layers (e.g. terrain textures, sprite atlases)
     * @member TYPE_FLOAT_VEC2          2-component floating-point vector (`vec2` in GLSL). Commonly used for texture coordinates or 2D positions
     * @member TYPE_FLOAT_VEC3          3-component floating-point vector (`vec3` in GLSL). Used for positions, normals, and directions in 3D space
     * @member TYPE_FLOAT_MAT2          2x2 floating-point matrix (`mat2` in GLSL). Used in transformations (e.g. 2D rotations, scaling)
     * @member TYPE_FLOAT_MAT3          3x3 floating-point matrix (`mat3` in GLSL). Commonly used for normal matrix calculations in lighting
     * @member TYPE_IMAGE_2D            2D image object. Unlike samplers, images allow read/write access in shaders (e.g. compute shaders or image load/store operations)
     * @member TYPE_TEXTURE_2D          2D texture object handle. Represents an actual GPU texture resource
     * @member TYPE_SAMPLER             Generic sampler handle, used as a placeholder for texture units without specifying the dimension
     * @member TYPE_TEXTURE_2D_ARRAY    2D texture array object handle
     * @member TYPE_TEXTURE_CUBE        Cube map texture object handle
     * @member TYPE_SAMPLER_3D          3D texture sampler. Used for volumetric effects, noise fields, or voxel data
     * @member TYPE_TEXTURE_3D          3D texture object handle
     * @member TYPE_IMAGE_3D            3D image object. Used for compute-based volume processing
     * @member TYPE_SAMPLER_3D_ARRAY    Array of 3D texture samplers
     * @member TYPE_TEXTURE_3D_ARRAY    3D texture array object handle
     */
    enum Type
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
    };

    /*#
     * Blend factors for color blending.
     * Defines how source and destination colors are combined
     * @enum
     * @name BlendFactor
     * @member BLEND_FACTOR_ZERO                        Always use 0.0
     * @member BLEND_FACTOR_ONE                         Always use 1.0
     * @member BLEND_FACTOR_SRC_COLOR                   Use source color
     * @member BLEND_FACTOR_ONE_MINUS_SRC_COLOR         Use (1 - source color)
     * @member BLEND_FACTOR_DST_COLOR                   Use destination color
     * @member BLEND_FACTOR_ONE_MINUS_DST_COLOR         Use (1 - destination color)
     * @member BLEND_FACTOR_SRC_ALPHA                   Use source alpha
     * @member BLEND_FACTOR_ONE_MINUS_SRC_ALPHA         Use (1 - source alpha)
     * @member BLEND_FACTOR_DST_ALPHA                   Use destination alpha
     * @member BLEND_FACTOR_ONE_MINUS_DST_ALPHA         Use (1 - destination alpha)
     * @member BLEND_FACTOR_SRC_ALPHA_SATURATE          Use min(srcAlpha, 1 - dstAlpha)
     * @member BLEND_FACTOR_CONSTANT_COLOR
     * @member BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR
     * @member BLEND_FACTOR_CONSTANT_ALPHA
     * @member BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA
     */
    enum BlendFactor
    {
        BLEND_FACTOR_ZERO                     = 0,
        BLEND_FACTOR_ONE                      = 1,
        BLEND_FACTOR_SRC_COLOR                = 2,
        BLEND_FACTOR_ONE_MINUS_SRC_COLOR      = 3,
        BLEND_FACTOR_DST_COLOR                = 4,
        BLEND_FACTOR_ONE_MINUS_DST_COLOR      = 5,
        BLEND_FACTOR_SRC_ALPHA                = 6,
        BLEND_FACTOR_ONE_MINUS_SRC_ALPHA      = 7,
        BLEND_FACTOR_DST_ALPHA                = 8,
        BLEND_FACTOR_ONE_MINUS_DST_ALPHA      = 9,
        BLEND_FACTOR_SRC_ALPHA_SATURATE       = 10,
        BLEND_FACTOR_CONSTANT_COLOR           = 11,
        BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR = 12,
        BLEND_FACTOR_CONSTANT_ALPHA           = 13,
        BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA = 14,
    };

    /*#
     * Graphics adapter family.
     * Identifies the type of graphics backend used by the rendering system
     * @enum
     * @name AdapterFamily
     * @member ADAPTER_FAMILY_NONE      No adapter detected. Used as an error state or uninitialized value
     * @member ADAPTER_FAMILY_NULL      Null (dummy) backend. Used for headless operation, testing, or environments where rendering output is not required
     * @member ADAPTER_FAMILY_OPENGL    OpenGL desktop backend. Common on Windows, macOS and Linux systems
     * @member ADAPTER_FAMILY_OPENGLES  OpenGL ES backend. Primarily used on mobile devices (Android, iOS), as well as WebGL (browser)
     * @member ADAPTER_FAMILY_VULKAN    Vulkan backend. Cross-platform modern graphics API with explicit control over GPU resources and multithreading
     * @member ADAPTER_FAMILY_VENDOR    Vendor-specific backend. A placeholder for proprietary or experimental APIs tied to a particular GPU vendor.
     * @member ADAPTER_FAMILY_WEBGPU    WebGPU backend. Modern web graphics API designed as the successor to WebGL
     * @member ADAPTER_FAMILY_DIRECTX   DirectX backend. Microsoft’s graphics API used on Windows and Xbox
     */
    enum AdapterFamily
    {
        ADAPTER_FAMILY_NONE     = -1,
        ADAPTER_FAMILY_NULL     = 1,
        ADAPTER_FAMILY_OPENGL   = 2,
        ADAPTER_FAMILY_OPENGLES = 3,
        ADAPTER_FAMILY_VULKAN   = 4,
        ADAPTER_FAMILY_VENDOR   = 5,
        ADAPTER_FAMILY_WEBGPU   = 6,
        ADAPTER_FAMILY_DIRECTX  = 7,
    };

    /*#
     * Texture types
     * @enum
     * @name TextureType
     * @member TEXTURE_TYPE_2D
     * @member TEXTURE_TYPE_2D_ARRAY
     * @member TEXTURE_TYPE_3D
     * @member TEXTURE_TYPE_CUBE_MAP
     * @member TEXTURE_TYPE_IMAGE_2D
     * @member TEXTURE_TYPE_IMAGE_3D
     * @member TEXTURE_TYPE_SAMPLER
     * @member TEXTURE_TYPE_TEXTURE_2D
     * @member TEXTURE_TYPE_TEXTURE_2D_ARRAY
     * @member TEXTURE_TYPE_TEXTURE_3D
     * @member TEXTURE_TYPE_TEXTURE_CUBE
     */
    enum TextureType
    {
        TEXTURE_TYPE_2D               = 0,
        TEXTURE_TYPE_2D_ARRAY         = 1,
        TEXTURE_TYPE_3D               = 2,
        TEXTURE_TYPE_CUBE_MAP         = 3,
        TEXTURE_TYPE_IMAGE_2D         = 4,
        TEXTURE_TYPE_IMAGE_3D         = 5,
        TEXTURE_TYPE_SAMPLER          = 6,
        TEXTURE_TYPE_TEXTURE_2D       = 7,
        TEXTURE_TYPE_TEXTURE_2D_ARRAY = 8,
        TEXTURE_TYPE_TEXTURE_3D       = 9,
        TEXTURE_TYPE_TEXTURE_CUBE     = 10,
    };

    /*#
     * Texture filtering modes.
     * Controls how texels are sampled when scaling or rotating textures
     * @enum
     * @name TextureFilter
     * @member TEXTURE_FILTER_DEFAULT                   Default texture filtering mode. Depeneds on graphics backend (for example, for OpenGL - TEXTURE_FILTER_LINEAR)
     * @member TEXTURE_FILTER_NEAREST                   Nearest-neighbor sampling (blocky look, fastest)
     * @member TEXTURE_FILTER_LINEAR                    Linear interpolation between texels (smooth look)
     * @member TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST    Nearest mipmap level, nearest texel
     * @member TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR     Linear blend between two mipmap levels, nearest texel
     * @member TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST     Nearest mipmap level, linear texel
     * @member TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR      Linear blend between mipmap levels and texels (trilinear)
     */ 
    enum TextureFilter
    {
        TEXTURE_FILTER_DEFAULT                = 0,
        TEXTURE_FILTER_NEAREST                = 1,
        TEXTURE_FILTER_LINEAR                 = 2,
        TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST = 3,
        TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR  = 4,
        TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST  = 5,
        TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR   = 6,
    };

    /*#
     * Texture addressing/wrapping modes.
     * Controls behavior when texture coordinates fall outside the [0,1] range
     * @enum
     * @name TextureWrap
     * @member TEXTURE_WRAP_CLAMP_TO_BORDER     Clamp to the color defined as 'border'
     * @member TEXTURE_WRAP_CLAMP_TO_EDGE       Clamp to the edge pixel of the texture
     * @member TEXTURE_WRAP_MIRRORED_REPEAT     Repeat texture, mirroring every other repetition
     * @member TEXTURE_WRAP_REPEAT              Repeat texture in a tiled fashion
     */ 
    enum TextureWrap
    {
        TEXTURE_WRAP_CLAMP_TO_BORDER = 0,
        TEXTURE_WRAP_CLAMP_TO_EDGE   = 1,
        TEXTURE_WRAP_MIRRORED_REPEAT = 2,
        TEXTURE_WRAP_REPEAT          = 3,
    };

    // render states
    enum State
    {
        STATE_DEPTH_TEST           = 0,
        STATE_SCISSOR_TEST         = 1,
        STATE_STENCIL_TEST         = 2,
        STATE_ALPHA_TEST           = 3,
        STATE_BLEND                = 4,
        STATE_CULL_FACE            = 5,
        STATE_POLYGON_OFFSET_FILL  = 6,
        STATE_ALPHA_TEST_SUPPORTED = 7,
    };

    // buffer clear types, each value is guaranteed to be separate bits
    enum BufferType
    {
        BUFFER_TYPE_COLOR0_BIT  = 0x01,
        BUFFER_TYPE_COLOR1_BIT  = 0x02,
        BUFFER_TYPE_COLOR2_BIT  = 0x04,
        BUFFER_TYPE_COLOR3_BIT  = 0x08,
        BUFFER_TYPE_DEPTH_BIT   = 0x10,
        BUFFER_TYPE_STENCIL_BIT = 0x20,
    };

    struct PipelineState
    {
        uint64_t m_WriteColorMask           : 4;
        uint64_t m_WriteDepth               : 1;
        uint64_t m_PrimtiveType             : 3;
        // Depth Test
        uint64_t m_DepthTestEnabled         : 1;
        uint64_t m_DepthTestFunc            : 3;
        // Stencil Test
        uint64_t m_StencilEnabled           : 1;

        // Front
        uint64_t m_StencilFrontOpFail       : 3;
        uint64_t m_StencilFrontOpPass       : 3;
        uint64_t m_StencilFrontOpDepthFail  : 3;
        uint64_t m_StencilFrontTestFunc     : 3;

        // Back
        uint64_t m_StencilBackOpFail        : 3;
        uint64_t m_StencilBackOpPass        : 3;
        uint64_t m_StencilBackOpDepthFail   : 3;
        uint64_t m_StencilBackTestFunc      : 3;

        uint64_t m_StencilWriteMask         : 8;
        uint64_t m_StencilCompareMask       : 8;
        uint64_t m_StencilReference         : 8;
        // Blending
        uint64_t m_BlendEnabled             : 1;
        uint64_t m_BlendSrcFactor           : 4;
        uint64_t m_BlendDstFactor           : 4;
        // Culling
        uint64_t m_CullFaceEnabled          : 1;
        uint64_t m_CullFaceType             : 2;
        // Face winding
        uint64_t m_FaceWinding              : 1;
        // Polygon offset
        uint64_t m_PolygonOffsetFillEnabled : 1;
    };

    /*#
     * Texture creation parameters.
     *
     * Defines how a texture is created, initialized, and used.
     * This structure is typically passed when allocating GPU memory
     * for a new texture. It controls dimensions, format, layering,
     * mipmapping, and intended usage.
     * @struct
     * @name TextureCreationParams
     * @member m_Type [type:dmGraphics::TextureType] Texture type. Defines the dimensionality and interpretation of the texture (2D, 3D, cube map, array)
     * @member m_Width [type:uint16_t]               Width of the texture in pixels at the base mip level
     * @member m_Height [type:uint16_t]              Height of the texture in pixels at the base mip level
     * @member m_Depth [type:uint16_t]               Depth of the texture. Used for 3D textures or texture arrays. For standard 2D textures, this is typically `1`
     * @member m_OriginalWidth [type:uint16_t]       Width of the original source data before scaling or compression
     * @member m_OriginalHeight [type:uint16_t]      Height of the original source data before scaling or compression
     * @member m_OriginalDepth [type:uint16_t]       Depth of the original source data
     * @member m_LayerCount [type:uint8_t]           Number of layers in the texture. Used for array textures (`TEXTURE_TYPE_2D_ARRAY`). For standard 2D textures, this is `1`
     * @member m_MipMapCount [type:uint8_t]          Number of mipmap levels. A value of `1` means no mipmaps (only the base level is stored). Larger values allow for mipmapped sampling.
     * @member m_UsageHintBits [type:uint8_t]        Bitfield of usage hints. Indicates how the texture will be used (e.g. sampling, render target, storage image). See dmGraphics::TextureUsageFlag
     */
    struct TextureCreationParams
    {
        TextureCreationParams()
        : m_Type(TEXTURE_TYPE_2D)
        , m_Width(0)
        , m_Height(0)
        , m_Depth(1)
        , m_OriginalWidth(0)
        , m_OriginalHeight(0)
        , m_OriginalDepth(1)
        , m_LayerCount(1)
        , m_MipMapCount(1)
        , m_UsageHintBits(TEXTURE_USAGE_FLAG_SAMPLE)
        {}

        TextureType m_Type;
        uint16_t    m_Width;
        uint16_t    m_Height;
        uint16_t    m_Depth;
        uint16_t    m_OriginalWidth;
        uint16_t    m_OriginalHeight;
        uint16_t    m_OriginalDepth;
        uint8_t     m_LayerCount;
        uint8_t     m_MipMapCount;
        uint8_t     m_UsageHintBits;
    };

    /*#
     * Texture update parameters.
     *
     * Defines a block of pixel data to be uploaded to a texture,
     * along with filtering, wrapping, and sub-region update options.
     * Typically used when calling texture upload/update functions
     * after a texture object has been created with `TextureCreationParams`
     * @struct
     * @name TextureParams
     * @member m_Data [type:const void*]                    Pointer to raw pixel data in CPU memory. The format is defined by `m_Format`
     * @member m_DataSize [type:uint32_t]                   Size of the pixel data in bytes. Must match the expected size from width, height, depth, and format
     * @member m_Format [type:dmGraphics::TextureFormat]    Format of the pixel data (e.g. RGBA, RGB, compressed formats). Dictates how the GPU interprets the memory pointed by `m_Data`
     * @member m_MinFilter [type:dmGraphics::TextureFilter] Minification filter (applied when shrinking). Determines how pixels are sampled when the texture is displayed smaller than its native resolution
     * @member m_MagFilter [type:dmGraphics::TextureFilter] Magnification filter (applied when enlarging). Determines how pixels are sampled when the texture is displayed larger than its native resolution
     * @member m_UWrap [type:dmGraphics::TextureWrap]       Wrapping mode for U (X) texture coordinate. Controls behavior when texture coordinates exceed [0,1]
     * @member m_VWrap [type:dmGraphics::TextureWrap]       Wrapping mode for V (Y) texture coordinate. Controls behavior when texture coordinates exceed [0,1]
     * @member m_X [type:uint32_t]                          X offset in pixels for sub-texture updates. Defines the left edge of the destination region
     * @member m_Y [type:uint32_t]                          Y offset in pixels for sub-texture updates. Defines the top edge of the destination region
     * @member m_Z [type:uint32_t]                          Z offset (depth layer) for 3D textures. Ignored for standard 2D textures
     * @member m_Slice [type:uint32_t]                      Slice index in an array texture where the data should be uploaded
     * @member m_Width [type:uint16_t]                      Width of the pixel data block in pixels. Used for both full uploads and sub-updates
     * @member m_Height [type:uint16_t]                     Height of the pixel data block in pixels. Used for both full uploads and sub-updates
     * @member m_Depth [type:uint16_t]                      Depth of the pixel data block in pixels. Only relevant for 3D textures
     * @member m_LayerCount [type:uint8_t]                  Number of layers to update. For array textures, this specifies how many pages are updated
     * @member m_MipMap [type:uint8_t] Only 7 bit available Mipmap level to update. Level 0 is the base level, higher levels are progressively downscaled versions
     * @member m_SubUpdate [type:uint8_t]                   If true, this represents a partial texture update (sub-region), using `m_X`, `m_Y`, `m_Z`, and `m_Slice` offsets. If false, the entire texture/mipmap level is replaced
     */
    struct TextureParams
    {
        TextureParams()
        : m_Data(0x0)
        , m_DataSize(0)
        , m_Format(TEXTURE_FORMAT_RGBA)
        , m_MinFilter(TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST)
        , m_MagFilter(TEXTURE_FILTER_LINEAR)
        , m_UWrap(TEXTURE_WRAP_CLAMP_TO_EDGE)
        , m_VWrap(TEXTURE_WRAP_CLAMP_TO_EDGE)
        , m_X(0)
        , m_Y(0)
        , m_Z(0)
        , m_Slice(0)
        , m_Width(0)
        , m_Height(0)
        , m_Depth(0)
        , m_LayerCount(0)
        , m_MipMap(0)
        , m_SubUpdate(false)
        {}

        const void*   m_Data;
        uint32_t      m_DataSize;
        TextureFormat m_Format;
        TextureFilter m_MinFilter;
        TextureFilter m_MagFilter;
        TextureWrap   m_UWrap;
        TextureWrap   m_VWrap;

        // For sub texture updates
        uint32_t m_X;
        uint32_t m_Y;
        uint32_t m_Z;
        uint32_t m_Slice;

        uint16_t m_Width;
        uint16_t m_Height;
        uint16_t m_Depth;
        uint8_t  m_LayerCount; // For array texture, this is page count
        uint8_t  m_MipMap    : 7;
        uint8_t  m_SubUpdate : 1;
    };

    struct RenderTargetCreationParams
    {
        TextureCreationParams m_ColorBufferCreationParams[MAX_BUFFER_COLOR_ATTACHMENTS];
        TextureCreationParams m_DepthBufferCreationParams;
        TextureCreationParams m_StencilBufferCreationParams;
        TextureParams         m_ColorBufferParams[MAX_BUFFER_COLOR_ATTACHMENTS];
        TextureParams         m_DepthBufferParams;
        TextureParams         m_StencilBufferParams;
        AttachmentOp          m_ColorBufferLoadOps[MAX_BUFFER_COLOR_ATTACHMENTS];
        AttachmentOp          m_ColorBufferStoreOps[MAX_BUFFER_COLOR_ATTACHMENTS];
        float                 m_ColorBufferClearValue[MAX_BUFFER_COLOR_ATTACHMENTS][4];
        // TODO: Depth/Stencil

        uint8_t               m_DepthTexture   : 1;
        uint8_t               m_StencilTexture : 1;
    };

    /*#
     * Create new vertex stream declaration. A stream declaration contains a list of vertex streams
     * that should be used to create a vertex declaration from.
     * @name NewVertexStreamDeclaration
     * @param context [type:dmGraphics::HContext] the context
     * @return declaration [type:dmGraphics::HVertexStreamDeclaration] the vertex declaration
     */
    HVertexStreamDeclaration NewVertexStreamDeclaration(HContext context);

    /*#
     * Create new vertex stream declaration. A stream declaration contains a list of vertex streams
     * that should be used to create a vertex declaration from.
     * @name NewVertexStreamDeclaration
     * @param context [type:dmGraphics::HContext] the context
     * @param step_function [type:dmGraphics::VertexStepFunction] the vertex step function to use
     * @return declaration [type:dmGraphics::HVertexStreamDeclaration] the vertex declaration
     */
    HVertexStreamDeclaration NewVertexStreamDeclaration(HContext context, VertexStepFunction step_function);

    /*#
     * Adds a stream to a vertex stream declaration
     * @name AddVertexStream
     * @param name [type:const char*] the name of the stream
     * @param size [type:uint32_t] the size of the stream, i.e number of components
     * @param type [type:dmGraphics::Type] the data type of the stream
     * @param normalize [type:bool] true if the stream should be normalized in the 0..1 range
     */
    void AddVertexStream(HVertexStreamDeclaration stream_declaration, const char* name, uint32_t size, Type type, bool normalize);

    /*#
     * Adds a stream to a vertex stream declaration
     * @name AddVertexStream
     * @param name_hash [type:dmhash_t] the name hash of the stream
     * @param size [type:uint32_t] the size of the stream, i.e number of components
     * @param type [type:dmGraphics::Type] the data type of the stream
     * @param normalize [type:bool] true if the stream should be normalized in the 0..1 range
     */
    void AddVertexStream(HVertexStreamDeclaration stream_declaration, dmhash_t name_hash, uint32_t size, Type type, bool normalize);

    /*#
     * Delete vertex stream declaration
     * @name DeleteVertexStreamDeclaration
     * @param stream_declaration [type:dmGraphics::HVertexStreamDeclaration] the vertex stream declaration
     */
    void DeleteVertexStreamDeclaration(HVertexStreamDeclaration stream_declaration);

    /*#
     * Create new vertex declaration from a vertex stream declaration
     * @name NewVertexDeclaration
     * @param context [type:dmGraphics::HContext] the context
     * @param stream_declaration [type:dmGraphics::HVertexStreamDeclaration] the vertex stream declaration
     * @return declaration [type:dmGraphics::HVertexDeclaration] the vertex declaration
     */
    HVertexDeclaration NewVertexDeclaration(HContext context, HVertexStreamDeclaration stream_declaration);

    /*#
     * Create new vertex declaration from a vertex stream declaration and an explicit stride value,
     * where the stride is the number of bytes between each consecutive vertex in a vertex buffer
     * @name NewVertexDeclaration
     * @param context [type:dmGraphics::HContext] the context
     * @param stream_declaration [type:dmGraphics::HVertexStreamDeclaration] the vertex stream declaration
     * @param stride [type:uint32_t] the stride between the start of each vertex (in bytes)
     * @return declaration [type:dmGraphics::HVertexDeclaration] the vertex declaration
     */
    HVertexDeclaration NewVertexDeclaration(HContext context, HVertexStreamDeclaration stream_declaration, uint32_t stride);

    /*#
     * Delete vertex declaration
     * @name DeleteVertexDeclaration
     * @param vertex_declaration [type:dmGraphics::HVertexDeclaration] the vertex declaration
     */
    void DeleteVertexDeclaration(HVertexDeclaration vertex_declaration);

    /*#
     * Get the physical offset into the vertex data for a particular stream
     * @name GetVertexStreamOffset
     * @param vertex_declaration [type:dmGraphics::HVertexDeclaration] the vertex declaration
     * @param name_hash [type:dmhash_t] the name hash of the vertex stream (as passed into AddVertexStream())
     * @return Offset in bytes into the vertex or INVALID_STREAM_OFFSET if not found
     */
    uint32_t GetVertexStreamOffset(HVertexDeclaration vertex_declaration, dmhash_t name_hash);

    /*#
     * Create new vertex buffer with initial data
     * @name NewVertexBuffer
     * @param context [type:dmGraphics::HContext] the context
     * @param size [type:uint32_t] the size of the buffer (in bytes). May be 0
     * @param data [type:void*] the data
     * @param buffer_usage [type:dmGraphics::BufferUsage] the usage
     * @return buffer [type:dmGraphics::HVertexBuffer] the vertex buffer
     */
    HVertexBuffer NewVertexBuffer(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage);

    /*#
     * Delete vertex buffer
     * @name DeleteVertexBuffer
     * @param buffer [type:dmGraphics::HVertexBuffer] the buffer
     */
    void DeleteVertexBuffer(HVertexBuffer buffer);

    /*#
     * Set vertex buffer data
     * @name SetVertexBufferData
     * @param buffer [type:dmGraphics::HVertexBuffer] the buffer
     * @param size [type:uint32_t] the size of the buffer (in bytes). May be 0
     * @param data [type:void*] the data
     * @param buffer_usage [type:dmGraphics::BufferUsage] the usage
     */
    void SetVertexBufferData(HVertexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage);

    /*#
     * Set subset of vertex buffer data
     * @name SetVertexBufferSubData
     * @param buffer [type:dmGraphics::HVertexBuffer] the buffer
     * @param offset [type:uint32_t] the offset into the desination buffer (in bytes)
     * @param size [type:uint32_t] the size of the buffer (in bytes). May be 0
     * @param data [type:void*] the data
     */
    void SetVertexBufferSubData(HVertexBuffer buffer, uint32_t offset, uint32_t size, const void* data);

    /*#
     * Get the max number of vertices allowed by the system in a vertex buffer
     * @name GetMaxElementsVertices
     * @param context [type:dmGraphics::HContext] the context
     * @return count [type:uint32_t] the count
     */
    uint32_t GetMaxElementsVertices(HContext context);

    /*#
     * Create new index buffer with initial data
     * @note The caller need to track if the indices are 16 or 32 bit.
     * @name NewIndexBuffer
     * @param context [type:dmGraphics::HContext] the context
     * @param size [type:uint32_t] the size of the buffer (in bytes). May be 0
     * @param data [type:void*] the data
     * @param buffer_usage [type:dmGraphics::BufferUsage] the usage
     * @return buffer [type:dmGraphics::HIndexBuffer] the index buffer
     */
    HIndexBuffer NewIndexBuffer(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage);

    /*#
     * Delete the index buffer
     * @name DeleteIndexBuffer
     * @param buffer [type:dmGraphics::HIndexBuffer] the index buffer
     */
    void DeleteIndexBuffer(HIndexBuffer buffer);

    /*#
     * Set index buffer data
     * @name SetIndexBufferData
     * @param buffer [type:dmGraphics::HIndexBuffer] the buffer
     * @param size [type:uint32_t] the size of the buffer (in bytes). May be 0
     * @param data [type:void*] the data
     * @param buffer_usage [type:dmGraphics::BufferUsage] the usage
     */
    void SetIndexBufferData(HIndexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage);

    /*#
     * Set subset of index buffer data
     * @name SetIndexBufferSubData
     * @param buffer [type:dmGraphics::HVertexBuffer] the buffer
     * @param offset [type:uint32_t] the offset into the desination buffer (in bytes)
     * @param size [type:uint32_t] the size of the buffer (in bytes). May be 0
     * @param data [type:void*] the data
     */
    void SetIndexBufferSubData(HIndexBuffer buffer, uint32_t offset, uint32_t size, const void* data);

    /*#
     * Check if the index format is supported
     * @name IsIndexBufferFormatSupported
     * @param context [type:dmGraphics::HContext] the context
     * @param format [type:dmGraphics::IndexBufferFormat] the format
     * @param result [type:bool] true if the format is supoprted
     */
    bool IsIndexBufferFormatSupported(HContext context, IndexBufferFormat format);

    /*#
     * Get the max number of indices allowed by the system in an index buffer
     * @name GetMaxElementsIndices
     * @param context [type:dmGraphics::HContext] the context
     * @return count [type:uint32_t] the count
     */
    uint32_t GetMaxElementsIndices(HContext context);

    /*# check if an extension is supported
     * @name IsExtensionSupported
     * @param context [type:dmGraphics::HContext] the context
     * @param extension [type:const char*] the extension.
     * @return result [type:bool] true if the extension was supported
     */
    bool IsExtensionSupported(HContext context, const char* extension);

    /*# check if a specific texture format is supported
     * @name IsTextureFormatSupported
     * @param context [type:dmGraphics::HContext] the context
     * @param format [type:dmGraphics::TextureFormat] the texture format.
     * @return result [type:bool] true if the texture format was supported
     */
    bool IsTextureFormatSupported(HContext context, TextureFormat format);

    /*#
     * @name GetNumSupportedExtensions
     * @param context [type:dmGraphics::HContext] the context
     * @return count [type:uint32_t] the number of supported extensions
     */
    uint32_t GetNumSupportedExtensions(HContext context);

    /*# get the supported extension
     * @name GetSupportedExtension
     * @param context [type:dmGraphics::HContext] the context
     * @param index [type:uint32_t] the index of the extension
     * @return extension [type:const char*] the extension. 0 if index was out of bounds
     */
    const char* GetSupportedExtension(HContext context, uint32_t index);

    /*#
     * Read frame buffer pixels in BGRA format
     * @name ReadPixels
     * @param context [type:dmGraphics::HContext] the context
     * @param x [type:int32_t] x-coordinate of the starting position
     * @param y [type:int32_t] y-coordinate of the starting position
     * @param width [type:uint32_t] width of the region
     * @param height [type:uint32_t] height of the region
     * @param buffer [type:void*] buffer to read to
     * @param buffer_size [type:uint32_t] buffer size
     */
    void ReadPixels(HContext context, int32_t x, int32_t y, uint32_t width, uint32_t height, void* buffer, uint32_t buffer_size);

    /*#
     * Get viewport's parameters
     * @name GetViewport
     * @param context [type:dmGraphics::HContext] the context
     * @param x [type:int32_t] x-coordinate of the viewport's origin
     * @param y [type:int32_t] y-coordinate of the viewport's origin
     * @param width [type:uint32_t] viewport's width
     * @param height [type:uint32_t] viewport's height
     */
    void GetViewport(HContext context, int32_t* x, int32_t* y, uint32_t* width, uint32_t* height);

    /*#
     * Get installed graphics adapter family
     * @name GetInstalledAdapterFamily
     * @return family [type:dmGraphics::AdapterFamily] Installed adapter family
     */
    AdapterFamily GetInstalledAdapterFamily();

    /*#
     * Create new texture
     * @name NewTexture
     * @param context [type:HContext] Graphics context
     * @param params [type:const dmGraphics::TextureCreationParams&] Creation parameters
     * @return texture_handle [type:dmGraphics::HTexture] Opaque texture handle
     */
    HTexture NewTexture(HContext context, const TextureCreationParams& params);

    /*#
     * Delete texture
     * @name DeleteTexture
     * @param context [type:dmGraphics::HContext] Graphics context
     * @param texture [type:dmGraphics::HTexture] Texture handle
     */
    void DeleteTexture(HContext context, HTexture texture);

    /*#
     * Set texture data. For textures of type TEXTURE_TYPE_CUBE_MAP it's assumed that
     * 6 mip-maps are present contiguously in memory with stride m_DataSize
     * @name SetTexture
     * @param context [type:dmGraphics::HContext] Graphics context
     * @param texture [type:dmGraphics::HTexture] Texture handle
     * @param params [type:const dmGraphics::TextureParams&]
     */
    void SetTexture(HContext context, HTexture texture, const TextureParams& params);

    /*#
     * Function called when a texture has been set asynchronously
     * @typedef
     * @name SetTextureAsyncCallback
     * @param texture [type:dmGraphics::HTexture] Texture handle
     * @param user_data [type:void*] User data that will be passed to the SetTextureAsyncCallback
     */
    typedef void (*SetTextureAsyncCallback)(HTexture texture, void* user_data);

    /*#
     * Set texture data asynchronously. For textures of type TEXTURE_TYPE_CUBE_MAP it's assumed that
     * 6 mip-maps are present contiguously in memory with stride m_DataSize
     * @name SetTextureAsync
     * @param context [type:dmGraphics::HContext] Graphics context
     * @param texture [type:dmGraphics::HTexture] Texture handle
     * @param params [type:const dmGraphics::TextureParams&] Texture parameters. Texture will be recreated if parameters differ from creation parameters
     * @param callback [type:dmGraphics::SetTextureAsyncCallback] Completion callback
     * @param user_data [type:void*] User data that will be passed to completion callback
     */
    void SetTextureAsync(HContext context, HTexture texture, const TextureParams& params, SetTextureAsyncCallback callback, void* user_data);

    /*#
     * Set texture parameters
     * @name SetTextureParams
     * @param context [type:dmGraphics::HContext] Graphics context
     * @param texture [type:dmGraphics::HTexture] Texture handle
     * @param min_filter [type:dmGraphics::TextureFilter] Minification filter type
     * @param mag_filter [type:dmGraphics::TextureFilter] Magnification filter type
     * @param uwrap [type:dmGraphics::TextureWrap] Wrapping mode for the U (X) texture coordinate.
     * @param vwrap [type:dmGraphics::TextureWrap] Wrapping mode for the V (Y) texture coordinate
     * @param max_anisotropy [type:float]
     */
    void SetTextureParams(HContext context, HTexture texture, TextureFilter min_filter, TextureFilter mag_filter, TextureWrap uwrap, TextureWrap vwrap, float max_anisotropy);

    /*#
     * Get approximate size of how much memory texture consumes
     * @name GetTextureResourceSize
     * @param context [type:dmGraphics::HContext] Graphics context
     * @param texture [type:dmGraphics::HTexture] Texture handle
     * @return data_size [type:uint32_t] Resource data size in bytes
     */ 
    uint32_t GetTextureResourceSize(HContext context, HTexture texture);

    /*#
     * Get texture width
     * @name GetTextureWidth
     * @param context [type:dmGraphics::HContext] Graphics context
     * @param texture [type:dmGraphics::HTexture] Texture handle
     * @return width [type:uint16_t] Texture's width
     */
    uint16_t GetTextureWidth(HContext context, HTexture texture);

    /*#
     * Get texture height
     * @name GetTextureHeight
     * @param context [type:dmGraphics::HContext] Graphics context
     * @param texture [type:dmGraphics::HTexture] Texture handle
     * @return height [type:uint16_t] Texture's height
     */
    uint16_t GetTextureHeight(HContext context, HTexture texture);

    /*#
     * Get texture depth. applicable for 3D and cube map textures 
     * @name GetTextureDepth
     * @param context [type:dmGraphics::HContext] Graphics context
     * @param texture [type:dmGraphics::HTexture] Texture handle
     * @return depth [type:uint16_t] Texture's depth
     */
    uint16_t GetTextureDepth(HContext context, HTexture texture);

    /*#
     * Get texture original (before uploading to GPU) width
     * @name GetOriginalTextureWidth
     * @param context [type:dmGraphics::HContext] Graphics context
     * @param texture [type:dmGraphics::HTexture] Texture handle
     * @return original_width [type:uin16_t] Texture's original width
     */
    uint16_t GetOriginalTextureWidth(HContext context, HTexture texture);

    /*#
     * Get texture original (before uploading to GPU) height
     * @name GetOriginalTextureHeight
     * @param context [type:dmGraphics::HContext] Graphics context
     * @param texture [type:dmGraphics::HTexture] Texture handle
     * @return original_height [type:uint16_t]
     */
    uint16_t GetOriginalTextureHeight(HContext context, HTexture texture);

    /*#
     * Get texture mipmap count
     * @name GetTextureMipmapCount
     * @param context [type:dmGraphice::HContext] Graphics context
     * @param texture [type:dmGraphics::HTexture] Texture handle
     * @return count [type:uint8_t] Texture mipmap count
     */
    uint8_t GetTextureMipmapCount(HContext context, HTexture texture);

    /*#
     * Get texture type
     * @name GetTextureType
     * @param context [type:dmGraphics::HContext] Graphics context
     * @param texture [type:dmGraphics::HTexture] Texture handle
     * @return type [type:dmGraphics::TextureType] Texture type
     */
    TextureType GetTextureType(HContext context, HTexture texture);

    /*#
     * Get how many platform-dependent texture handle used for engine texture handle.
     * Applicable only for OpenGL/ES backend. All other backends return 1.
     * @name GetNumTextureHandles
     * @param context [type:dmGraphics::Context] Graphics context
     * @param texture [type:dmGraphics::HTexture] Texture handle
     * @return handles_amount [type:uint8_t] Platform-dependent handles amount
     */
    uint8_t GetNumTextureHandles(HContext context, HTexture texture);

    /*#
     * Query usage hint flags for a texture.
     *
     * Retrieves the bitwise usage flags that were assigned to a texture
     * when it was created. These flags indicate the intended role(s) of
     * the texture in the rendering pipeline and allow the graphics
     * backend to apply optimizations or enforce restrictions
     * @name GetTextureUsageHintFlags
     * @see dmGraphics::TextureUsageFlag
     * @param context [type:dmGraphics::HContext] Graphics context
     * @param texture [type:dmGraphics::HTexture] Texture handle
     * @return flags [type:uint32_t] A bitwise OR of usage flags describing how the texture may be used. Applications can test specific flags using bitmask operations
     */
    uint32_t GetTextureUsageHintFlags(HContext context, HTexture texture);

    /*#
     * Get status of texture
     *
     * @name GetTextureStatusFlags
     * @param context [type:dmGraphics::HContext] Graphics context
     * @param texture [type:dmGraphics::HTexture] Texture handle
     * @return flags [type:dmGraphics::TextureStatusFlags] Enumerated status bit flags
     */
    uint32_t GetTextureStatusFlags(HContext context, HTexture texture);

    /*#
     * Enable and bind a texture to a texture unit.
     *
     * Associates a texture object with a specific texture unit in the
     * graphics context, making it available for sampling in shaders.
     * Multiple textures can be active simultaneously by binding them
     * to different units. The shader must reference the correct unit
     * (via sampler uniform) to access the bound texture
     * @name EnableTexture
     * @param context [type:dmGraphics::HContext] Graphics context
     * @param unit [type: uint32_t] Texture unit index to bind the texture to. Valid range depends on GPU capabilities (commonly 0–15 for at least 16 texture units)
     * @param id_index [type:uint8_t] Index for internal tracking or binding variation. Typically used when multiple texture IDs are managed within the same unit (e.g. array textures or multi-bind)
     * @param texture [type:dmGraphics::HTexture] Handle to the texture object to enable and bind
     */
    void EnableTexture(HContext context, uint32_t unit, uint8_t id_index, HTexture texture);

    /*#
     * Disable a texture bound to a texture unit.
     *
     * Unbinds the given texture handle from the specified unit,
     * releasing the association in the graphics pipeline.
     * This is useful to prevent unintended reuse of textures,
     * or to free up texture units for other bindings.
     * @name DisableTexture
     * @param context [type:dmGraphics::HContext] Graphics context
     * @param unit [type:uint32_t] Texture unit index to disable. Must match the one previously used in `dmGraphics::EnableTexture`
     * @param texture [type:dmGraphics::HTexture] Handle to the texture object to disable
     */
    void DisableTexture(HContext context, uint32_t unit, HTexture texture);

    /*#
     * Get string representation of texture type
     * @name GetTextureTypeLiteral
     * @param texture_type [type:dmGraphics::TextureType] Texture type
     * @return literal_type [type:const char*] String representation of type
     */
    const char* GetTextureTypeLiteral(TextureType texture_type);

    /*#
     * Get string representation of texture format
     * @name GetTextureFormatLiteral
     * @param format [type:dmGraphics::TextureFormat] Texture format
     * @return literal_format [type:const char*] String representation of format
     */
    const char* GetTextureFormatLiteral(TextureFormat format);

    /*#
     * Get maximum supported size in pixels of non-array texture
     * @name GetMaxTextureSize
     * @param context [type:dmGraphics::HContext] Graphics context
     * @return max_texture_size [type:uint32_t] Maximum texture size supported by GPU
     */
    uint32_t GetMaxTextureSize(HContext context);

    /*#
     * Return the width of the opened window, if any.
     * @name GetWindowWidth
     * @param context [type:dmGraphics::HContext] Graphics context
     * @return window_width [type:uint32_t] Width of the window. If no window is opened, 0 is always returned
     */
    uint32_t GetWindowWidth(HContext context);

    /*#
     * Return the height of the opened window, if any.
     * @name GetWindowHeight
     * @param context [type:dmGraphics::HContext] Graphics context
     * @return window_height [type:uint32_t] Height of the window. If no window is opened, 0 is always returned
     */
    uint32_t GetWindowHeight(HContext context);

    /*#
     * Returns the specified width of the opened window, which might differ from the actual window width.
     * @name GetWidth
     * @param context [type:dmGraphics::HContext] Graphics context
     * @return width [type:uint32_t] Specified width of the window. If no window is opened, 0 is always returned.
     */
    uint32_t GetWidth(HContext context);

    /*#
     * Returns the specified height of the opened window, which might differ from the actual window width.
     * @name GetHeight
     * @param context [type:dmGraphics::HContext] Graphics context
     * @return height [type:uint32_t] Specified height of the window. If no window is opened, 0 is always returned.
     */
    uint32_t GetHeight(HContext context);

    /*#
     * @name SetStencilMask
     * @param context [type:dmGraphics::HContext] Graphics context
     * @param mask [type: uint32_t]
     */
    void SetStencilMask(HContext context, uint32_t mask);

    /*#
     * @name SetStencilFunc
     * @param context [type:dmGraphics::HContext] Graphics context
     * @param func [type:dmGraphics::CompareFunc]
     * @param ref [type:uint32_t]
     * @param mask [type:uint32_t]
     */
    void SetStencilFunc(HContext context, CompareFunc func, uint32_t ref, uint32_t mask);

    /*#
     * @name SetStencilFuncSeparate
     * @param context [type:dmGraphics::HContext] Graphics context
     * @param face_type [type:dmGraphics::FaceType]
     * @param func [type:dmGraphics::CompareFunc]
     * @param ref [type:uint32_t]
     * @param mask [type:uint32_t]
     */
    void SetStencilFuncSeparate(HContext context, FaceType face_type, CompareFunc func, uint32_t ref, uint32_t mask);

    /*#
     * @name SetStencilOp
     * @param context [type:dmGraphics::HContext] Graphics context
     * @param sfail [type:dmGraphics::StencilOp]
     * @param dpfail [type:dmGraphics::StencilOp]
     * @param dppass [type:dmGraphics::StencilOp]
     */
    void SetStencilOp(HContext context, StencilOp sfail, StencilOp dpfail, StencilOp dppass);

    /*#
     * @name SetStencilOpSeparate
     * @param context [type:dmGraphics::HContext] Graphics context
     * @param face_type [type:dmGraphics::FaceType]
     * @param sfail [type:dmGraphics::StencilOp]
     * @param dpfail [type:dmGraphics::StencilOp]
     * @param dppass [type:dmGraphics::StencilOp]
     */
    void SetStencilOpSeparate(HContext context, FaceType face_type, StencilOp sfail, StencilOp dpfail, StencilOp dppass);

    /*#
     * @name EnableState
     * @param context [type:dmGraphics::HContext] Graphics context
     * @param state [type:dmGraphics::State] Render state
     */
    void EnableState(HContext context, State state);

    /*#
     * @name DisableState
     * @param context [type:dmGraphics::HContext] Graphics context
     * @param state [type:dmGraphics::State] Render state
     */
    void DisableState(HContext context, State state);

    /*#
     * @name SetBlendFunc
     * @param context [type:dmGraphics::HContext] Graphics context
     * @param source_factor [type:gmGraphics::BlendFactor]
     * @param destination_factor [type:dmGraphics::BlendFactor]
     */
    void SetBlendFunc(HContext context, BlendFactor source_factor, BlendFactor destinaton_factor);

    /*#
     * @name SetColorMask
     * @param context [type:dmGraphics::HContext] Graphics context
     * @param red [type:bool]
     * @param green [type:bool]
     * @param blue [type:bool]
     * @param alpha [type:bool]
     */
    void SetColorMask(HContext context, bool red, bool green, bool blue, bool alpha);

    /*#
     * @name SetDepthMask
     * @param context [type:dmGraphics::HContext] Graphics context
     * @param enable_mask [type:bool]
     */
    void SetDepthMask(HContext context, bool enable_mask);

    /*#
     * @name SetDepthFunc
     * @param context [type:dmGraphics::HContext] Graphics context
     * @param func [type:dmGraphics::CompareFunc]
     */
    void SetDepthFunc(HContext context, CompareFunc func);

    /*#
     * @name GetInstalledContext
     * @return context [type:dmGraphics::HContext] Installed graphics context
     */
    HContext GetInstalledContext();

    /*#
     * @name NewRenderTarget
     * @param context [type:dmGraphics::HContext] Graphics context
     * @param buffer_type_flags [type:uint32_t]
     * @param params [type:dmGraphics::const RenderTargetCreationParams]
     * @return render_target [type:dmGraphics::HRenderTarget] Newly created render target
     */
    HRenderTarget NewRenderTarget(HContext context, uint32_t buffer_type_flags, const RenderTargetCreationParams params);

    /*#
     * @name DeleteRenderTarget
     * @param context [type:dmGraphics::HContext] Graphics context
     * @param render_target [type:dmGraphics::HRenderTarget]
     */
    void DeleteRenderTarget(HContext context, HRenderTarget render_target);

    /*#
     * @name SetRenderTarget
     * @param context [type:dmGraphics::HContext] Graphics context
     * @param render_target [type:dmGraphics::HRenderTarget]
     * @param transient_buffer_types [type:uint32_t]
     */
    void SetRenderTarget(HContext context, HRenderTarget render_target, uint32_t transient_buffer_types);

    /*#
     * @name GetRenderTargetTexture
     * @param context [type:dmGraphics::HContext] Graphics context
     * @param render_target [type:dmGraphics::HRenderTarget]
     * @param buffer_type [type:dmGraphics::BufferType]
     * @return render_target_texture [type:dmGraphics::HTexture]
     */
    HTexture GetRenderTargetTexture(HContext context, HRenderTarget render_target, BufferType buffer_type);

    /*#
     * @name GetRenderTargetSize
     * @param context [type:dmGraphics::HContext] Graphics context
     * @param render_target [type:dmGraphics::HRenderTarget]
     * @param buffer_type [type:dmGraphics::BufferType]
     * @param width [type:uint32_t&]
     * @param height [type:uint32_t&]
     */
    void GetRenderTargetSize(HContext context, HRenderTarget render_target, BufferType buffer_type, uint32_t& width, uint32_t& height);

    /*#
     * @name SetRenderTargetSize
     * @param context [type:dmGraphics::HContext] Graphics context
     * @param render_target [type:dmGraphics::HRenderTarget]
     * @param width [type:uint32_t]
     * @param height [type:uint32_t]
     */
    void SetRenderTargetSize(HContext context, HRenderTarget render_target, uint32_t width, uint32_t height);

    /*#
     * @name GetPipelineState
     * @param context [type:dmGraphics::HContext] Graphics context
     * @return pipeline_state [type:dmGraphics::PipelineState]
     */
    PipelineState GetPipelineState(HContext context);

    /*#
     * @name SetCullFace
     * @param context [type:dmGraphics::HContext] Graphics context
     * @param face_type [type:dmGraphics::FaceType]
     */
    void SetCullFace(HContext context, FaceType face_type);

    /*#
     * @name RepackRGBToRGBA
     * @param num_pixels [type:uint32_t]
     * @param rgb [type:uint8_t*]
     * @param rgba [type:uint8_t*]
     */
    void RepackRGBToRGBA(uint32_t num_pixels, uint8_t* rgb, uint8_t* rgba);

    /*#
     * Get the scale factor of the display.
     * The display scale factor is usally 1.0 but will for instance be 2.0 on a macOS Retina display.
     * @name GetDisplayScaleFactor
     * @param context [type:dmGraphics::HContext] Graphics context
     * @return scale_factor [type:float] Display scale factor
     */
    float GetDisplayScaleFactor(HContext context);

}

#endif // DMSDK_GRAPHICS_H
