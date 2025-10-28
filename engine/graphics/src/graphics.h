// Copyright 2020-2025 The Defold Foundation
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

#ifndef DM_GRAPHICS_H
#define DM_GRAPHICS_H

#include <stdint.h>
#include <dmsdk/dlib/vmath.h>
#include <dmsdk/graphics/graphics.h>

#include <dlib/hash.h>
#include <dlib/job_thread.h>
#include <dlib/opaque_handle_container.h>

#include <ddf/ddf.h>
#include <graphics/graphics_ddf.h>

#include <platform/platform_window.h>

namespace dmGraphics
{
    /**
     * Callback function called when the window is requested to close.
     * @param user_data user data that was supplied when opening the window
     * @return whether the window should be closed or not
     */
    typedef bool (*WindowCloseCallback)(void* user_data);

    /**
     * Function used when in the application loop when performing a single iteration.
     * @param user_data user data that will be passed into the method when running the application loop.
     */
    typedef void (*WindowStepMethod)(void* user_data);

    /**
     * Function used to determine whether the application loop should continue running.
     * @param user_data user data used that will be passed into the method when making a determination.
     * @return a non-zero value will indicate that the application should continue running, whereas zero will
     *  lead to the application loop terminating.
     */
    typedef int32_t (*WindowIsRunning)(void* user_data);

    // See documentation in engine.h
    typedef void (*EngineInit)(void* ctx);
    typedef void (*EngineExit)(void* ctx);
    typedef void* (*EngineCreate)(int argc, char** argv);
    typedef void (*EngineDestroy)(void* engine);
    typedef int (*EngineUpdate)(void* engine);
    typedef void (*EngineGetResult)(void* engine, int* run_action, int* exit_code, int* argc, char*** argv);

    // Decorated asset handle with 21 bits meta | 32 bits opaque handle
    // Note: that we can only use a total of 53 bits out of the 64 due to how we expose the handles
    //       to the users via lua: http://lua-users.org/wiki/NumbersTutorial
    typedef uint64_t HAssetHandle;

    const static uint64_t MAX_ASSET_HANDLE_VALUE  = 0x20000000000000-1; // 2^53 - 1
    static const uint8_t  MAX_BUFFER_TYPE_COUNT   = 2 + MAX_BUFFER_COLOR_ATTACHMENTS;
    const static uint8_t  MAX_VERTEX_STREAM_COUNT = 8;

    const static uint8_t DM_GRAPHICS_STATE_WRITE_R   = 0x1;
    const static uint8_t DM_GRAPHICS_STATE_WRITE_G   = 0x2;
    const static uint8_t DM_GRAPHICS_STATE_WRITE_B   = 0x4;
    const static uint8_t DM_GRAPHICS_STATE_WRITE_A   = 0x8;

    static const HProgram         INVALID_PROGRAM_HANDLE   = ~0u;
    static const HUniformLocation INVALID_UNIFORM_LOCATION = ~0ull;

    enum AdapterFamilyPriority
    {
        ADAPTER_FAMILY_PRIORITY_NULL     = 32,
        ADAPTER_FAMILY_PRIORITY_OPENGL   = 2,
        ADAPTER_FAMILY_PRIORITY_OPENGLES = 2,
        ADAPTER_FAMILY_PRIORITY_VULKAN   = 1,
        ADAPTER_FAMILY_PRIORITY_VENDOR   = 0,
        ADAPTER_FAMILY_PRIORITY_WEBGPU   = 0,
        ADAPTER_FAMILY_PRIORITY_DIRECTX  = 0,
    };

    enum AssetType
    {
        ASSET_TYPE_NONE          = 0,
        ASSET_TYPE_TEXTURE       = 1,
        ASSET_TYPE_RENDER_TARGET = 2,
    };

    enum ContextFeature
    {
        CONTEXT_FEATURE_MULTI_TARGET_RENDERING = 0,
        CONTEXT_FEATURE_TEXTURE_ARRAY          = 1,
        CONTEXT_FEATURE_COMPUTE_SHADER         = 2,
        CONTEXT_FEATURE_STORAGE_BUFFER         = 3,
        CONTEXT_FEATURE_VSYNC                  = 4,
        CONTEXT_FEATURE_INSTANCING             = 5,
        CONTEXT_FEATURE_3D_TEXTURES            = 6,
        // ASTC for 2D array textures (paged atlases). Some WebGL/GLES drivers
        // fail array texture ASTC uploads while 2D ASTC works.
        CONTEXT_FEATURE_ASTC_ARRAY_TEXTURES    = 7,
    };

    // Translation table to translate RenderTargetAttachment to BufferType
    struct AttachmentToBufferType
    {
        BufferType m_AttachmentToBufferType[MAX_ATTACHMENT_COUNT];
        AttachmentToBufferType();
    };

    // Parameters structure for NewContext
    struct ContextParams
    {
        ContextParams();

        dmPlatform::HWindow   m_Window;
        dmJobThread::HContext m_JobThread;
        TextureFilter         m_DefaultTextureMinFilter;
        TextureFilter         m_DefaultTextureMagFilter;
        uint32_t              m_Width;
        uint32_t              m_Height;
        uint32_t              m_GraphicsMemorySize;             // The max allowed Gfx memory (default 0)
        uint32_t              m_SwapInterval;                   // Initial VSync setting (default 1)
        uint8_t               m_VerifyGraphicsCalls : 1;
        uint8_t               m_PrintDeviceInfo : 1;
        uint8_t               m_RenderDocSupport : 1;           // Vulkan only
        uint8_t               m_UseValidationLayers : 1;        // Vulkan only
        uint8_t               : 4;
    };

    struct VertexAttributeInfo
    {
        dmhash_t                       m_NameHash;
        VertexAttribute::SemanticType  m_SemanticType;
        VertexAttribute::DataType      m_DataType;
        VertexAttribute::VectorType    m_VectorType;
        dmGraphics::VertexStepFunction m_StepFunction;
        CoordinateSpace                m_CoordinateSpace;
        const uint8_t*                 m_ValuePtr;
        VertexAttribute::VectorType    m_ValueVectorType;
        bool                           m_Normalize;
    };

    struct VertexAttributeInfos
    {
        VertexAttributeInfos()
        {
            memset(this, 0, sizeof(*this));
            m_StructSize = sizeof(*this);
        }

        VertexAttributeInfo m_Infos[MAX_VERTEX_STREAM_COUNT];
        uint32_t            m_VertexStride;
        uint32_t            m_NumInfos;
        uint32_t            m_StructSize;
    };

    struct VertexAttributeInfoMetadata
    {
        uint32_t m_HasAttributeWorldPosition : 1;
        uint32_t m_HasAttributeLocalPosition : 1;
        uint32_t m_HasAttributeNormal        : 1;
        uint32_t m_HasAttributeTangent       : 1;
        uint32_t m_HasAttributeColor         : 1;
        uint32_t m_HasAttributeTexCoord      : 1;
        uint32_t m_HasAttributePageIndex     : 1;
        uint32_t m_HasAttributeWorldMatrix   : 1;
        uint32_t m_HasAttributeNormalMatrix  : 1;
        uint32_t m_HasAttributeNone          : 1;
    };

    struct WriteAttributeStreamDesc
    {
        const float** m_Data;
        // Data source vector types corresponding to each data source passed in.
        // The destination vector type is stored in the vertexattribute infos.
        VertexAttribute::VectorType m_VectorType;
        // Number of data "streams" for this attribute
        uint8_t m_StreamCount  : 7;
        // Some streams share the value across the entire mesh (page indices for example).
        // An alternative would be to replicate the data across all vertices in the mesh,
        // which would be cleaner but a bit of a waste. Perhaps it should be configurable by the write params?
        // Assume the data is not global by default
        uint8_t m_IsGlobalData : 1;
    };

    struct WriteAttributeParams
    {
        const VertexAttributeInfos* m_VertexAttributeInfos;
        WriteAttributeStreamDesc    m_WorldMatrix;
        WriteAttributeStreamDesc    m_NormalMatrix;
        WriteAttributeStreamDesc    m_PositionsLocalSpace;
        WriteAttributeStreamDesc    m_PositionsWorldSpace;
        WriteAttributeStreamDesc    m_Normals;
        WriteAttributeStreamDesc    m_Tangents;
        WriteAttributeStreamDesc    m_Colors;
        WriteAttributeStreamDesc    m_TexCoords;
        WriteAttributeStreamDesc    m_PageIndices;
        VertexStepFunction          m_StepFunction;
    };

    struct VertexDeclaration
    {
        struct Stream
        {
            dmhash_t m_NameHash;
            int16_t  m_Location;
            uint16_t m_Size;
            uint16_t m_Offset;
            Type     m_Type;
            bool     m_Normalize;
        };

        Stream             m_Streams[MAX_VERTEX_STREAM_COUNT];
        dmhash_t           m_PipelineHash; // Vulkan
        uint16_t           m_StreamCount;
        uint16_t           m_Stride;
        VertexStepFunction m_StepFunction;
        HProgram           m_BoundForProgram;     // OpenGL
        uint32_t           m_ModificationVersion; // OpenGL
    };

    struct Uniform
    {
        char*            m_Name; // Name, e.g "my_member" or "some_struct.my_member"
        dmhash_t         m_NameHash;
        HUniformLocation m_Location;
        Type             m_Type;
        uint32_t         m_Count;
    };

    /** Creates a graphics context
     * Currently, there can only be one context active at a time.
     * @return New graphics context
     */
    HContext NewContext(const ContextParams& params);

    /**
     * Destroy device
     */
    void DeleteContext(HContext context);

    /**
     * Install a graphics adapter
     * @params family AdapterFamily identifier for which adapter to use (vulkan/opengl/null/vendor)
     * @return True if a graphics backend could be created, false otherwise.
     */
    bool InstallAdapter(AdapterFamily family = ADAPTER_FAMILY_NONE);
    AdapterFamily GetAdapterFamily(const char* adapter_name);

    /**
     * Finalize graphics system
     */
    void Finalize();

    /**
     * Starts the app that needs to control the update loop (iOS only)
     */
    void AppBootstrap(int argc, char** argv, void* init_ctx, EngineInit init_fn, EngineExit exit_fn, EngineCreate create_fn, EngineDestroy destroy_fn, EngineUpdate update_fn, EngineGetResult result_fn);

    /**
     * Get the window refresh rate
     * @params context Graphics context handle
     * @return The window refresh rate, 0 if refresh rate could not be read.
     */
    uint32_t GetWindowRefreshRate(HContext context);

    /**
     * Get the window handle from the graphics context
     * @param context Graphics context handle
     * @return The window handle
     */
    dmPlatform::HWindow GetWindow(HContext context);

    /**
     * Close the open window if any.
     * @param context Graphics context handle
     */
    void CloseWindow(HContext context);

    /**
     * Iconify the open window if any.
     * @param context Graphics context handle
     */
    void IconifyWindow(HContext context);

    /**
     * Retrieve current state of the opened window, if any.
     * @param context Graphics context handle
     * @param state Aspect of the window state to query for
     * @return State of the supplied aspect. If no window is opened, 0 is always returned.
     */
    uint32_t GetWindowStateParam(HContext context, dmPlatform::WindowState state);

    /**
     * Returns the specified dpi of default monitor.
     *
     * @param context Graphics context handle
     * @return Specified dpi of the default display. If not supported, 0 is returned
     */
    uint32_t GetDisplayDpi(HContext context);

    /**
     * Set the size of the opened window, if any. If no window is opened, this function does nothing. If successfull,
     * the WindowResizeCallback will be called, if any was supplied when the window was opened.
     * @param context Graphics context handle
     * @param width New width of the window
     * @param height New height of the window
     */
    void SetWindowSize(HContext context, uint32_t width, uint32_t height);

    /**
     * Resizes a previously opened window. Only the window width and height will be changed with its framebuffer size
     * changed accordingly. The game width and height will be kept as specified from initial boot.
     * @param context Graphics context handle
     * @param width New width of the window
     * @param height New height of the window
     */
    void ResizeWindow(HContext context, uint32_t width, uint32_t height);

    /**
     * Return the default texture filtering modes.
     * @param context Graphics context handle
     * @param out_min_filter Out parameter to write the default min filtering mode to
     * @param out_mag_filter Out parameter to write the default mag filtering mode to
     */
    void GetDefaultTextureFilters(HContext context, TextureFilter& out_min_filter, TextureFilter& out_mag_filter);

    /**
     * Begin frame rendering.
     *
     * @param context Graphics context handle
     */
    void BeginFrame(HContext context);

    /**
     * Flip screen buffers.
     *
     * @param context Graphics context handle
     */
    void Flip(HContext context);

    /**
     * Set buffer swap interval.
     * 1 for base frequency, eg every frame (60hz)
     * 2 for every second frame
     * etc
     * @param context Graphics context
     * @param swap_interval swap interval
     *
     */
    void SetSwapInterval(HContext context, uint32_t swap_interval);

    /**
     * Clear render target
     * @param context Graphics context
     * @param flags
     * @param red
     * @param green
     * @param blue
     * @param alpha
     * @param depth
     * @param stencil
     */
    void Clear(HContext context, uint32_t flags, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha, float depth, uint32_t stencil);

    bool     SetStreamOffset(HVertexDeclaration vertex_declaration, uint32_t stream_index, uint16_t offset);
    void     EnableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration, uint32_t binding_index, uint32_t base_offset, HProgram program);
    void     DisableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration);
    void     HashVertexDeclaration(HashState32 *state, HVertexDeclaration vertex_declaration);
    uint32_t GetVertexDeclarationStride(HVertexDeclaration vertex_declaration);
    uint32_t GetVertexDeclarationStreamCount(HVertexDeclaration vertex_declaration);

    void     EnableVertexBuffer(HContext context, HVertexBuffer vertex_buffer, uint32_t binding_index);
    void     DisableVertexBuffer(HContext context, HVertexBuffer vertex_buffer);
    uint32_t GetVertexBufferSize(HVertexBuffer vertex_buffer);
    uint32_t GetIndexBufferSize(HIndexBuffer buffer);

    void     DrawElements(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count, Type type, HIndexBuffer index_buffer, uint32_t instance_count);
    void     Draw(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count, uint32_t instance_count);
    void     DispatchCompute(HContext context, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z);

    HProgram             NewProgram(HContext context, ShaderDesc* ddf, char* error_buffer, uint32_t error_buffer_size);
    void                 DeleteProgram(HContext context, HProgram program);

    bool                 IsShaderLanguageSupported(HContext _context, ShaderDesc::Language language, ShaderDesc::ShaderType shader_type);
    ShaderDesc::Language GetProgramLanguage(HProgram program);

    void                 EnableProgram(HContext context, HProgram program);
    void                 DisableProgram(HContext context);
    bool                 ReloadProgram(HContext context, HProgram program, ShaderDesc* ddf);

    // Attributes
    uint32_t         GetAttributeCount(HProgram prog);
    void             GetAttribute(HProgram prog, uint32_t index, dmhash_t* name_hash, Type* type, uint32_t* element_count, uint32_t* num_values, int32_t* location);
    void             GetAttributeValues(const VertexAttribute& attribute, const uint8_t** data_ptr, uint32_t* data_size);
    Type             GetGraphicsType(VertexAttribute::DataType data_type);

    float            VertexAttributeDataTypeToFloat(const dmGraphics::VertexAttribute::DataType data_type, const uint8_t* value_ptr);
    uint8_t*         WriteVertexAttributeFromFloat(uint8_t* value_write_ptr, float value, dmGraphics::VertexAttribute::DataType data_type);
    uint8_t*         WriteAttributes(uint8_t* write_ptr, uint32_t vertex_index, uint32_t vertex_count, const WriteAttributeParams& params);

    // Uniforms
    uint32_t         GetUniformCount(HProgram prog);
    void             GetUniform(HProgram prog, uint32_t index, Uniform* uniform);

    void SetConstantV4(HContext context, const dmVMath::Vector4* data, int count, HUniformLocation base_location);
    void SetConstantM4(HContext context, const dmVMath::Vector4* data, int count, HUniformLocation base_location);
    void SetSampler(HContext context, HUniformLocation location, int32_t unit);
    void SetViewport(HContext context, int32_t x, int32_t y, int32_t width, int32_t height);

    void SetFaceWinding(HContext context, FaceWinding face_winding);
    void SetPolygonOffset(HContext context, float factor, float units);

    uint32_t      GetBufferTypeIndex(BufferType buffer_type);
    BufferType    GetBufferTypeFromIndex(uint32_t index);
    const char*   GetBufferTypeLiteral(BufferType buffer_type);
    bool          IsContextFeatureSupported(HContext context, ContextFeature feature);

    bool          IsTextureFormatSupportedForType(HContext context, TextureType type, TextureFormat format);
    TextureFormat GetSupportedCompressionFormatForType(HContext context, TextureFormat format, uint32_t width, uint32_t height, TextureType type);

    TextureFormat GetSupportedCompressionFormat(HContext context, TextureFormat format, uint32_t width, uint32_t height);

    uint32_t GetTextureFormatBitsPerPixel(TextureFormat format);
    uint8_t     GetTexturePageCount(HTexture texture);

    // Calculating mipmap info helpers
    uint16_t    GetMipmapSize(uint16_t size_0, uint8_t mipmap);
    uint8_t     GetMipmapCount(uint16_t size);

    // Asset handle helpers
    const char* GetAssetTypeLiteral(AssetType type);
    bool        IsAssetHandleValid(HContext context, HAssetHandle asset_handle);

    static inline HAssetHandle MakeAssetHandle(HOpaqueHandle opaque_handle, AssetType asset_type)
    {
        assert(asset_type != ASSET_TYPE_NONE && "Invalid asset type");
        uint64_t handle = ((uint64_t) asset_type) << 32 | opaque_handle;
        assert(handle <= MAX_ASSET_HANDLE_VALUE);
        return handle;
    }

    static inline AssetType GetAssetType(HAssetHandle asset_handle)
    {
        return (AssetType) (asset_handle >> 32);
    }

    static inline HOpaqueHandle GetOpaqueHandle(HAssetHandle asset_handle)
    {
        return (HOpaqueHandle) asset_handle & 0xFFFFFFFF;
    }

    static inline bool IsColorBufferType(BufferType buffer_type)
    {
        return buffer_type == BUFFER_TYPE_COLOR0_BIT ||
               buffer_type == BUFFER_TYPE_COLOR1_BIT ||
               buffer_type == BUFFER_TYPE_COLOR2_BIT ||
               buffer_type == BUFFER_TYPE_COLOR3_BIT;
    }

    static inline void SetWriteAttributeStreamDesc(WriteAttributeStreamDesc* stream, const float** data, VertexAttribute::VectorType vector_type, uint8_t stream_count, bool is_global_data)
    {
        stream->m_Data = data;
        stream->m_VectorType = vector_type;
        stream->m_StreamCount = stream_count;
        stream->m_IsGlobalData = is_global_data;
    }

    static inline void VertexAttributeInfoMetadataMember(VertexAttributeInfoMetadata& metadata, VertexAttribute::SemanticType semantic_type, CoordinateSpace coordinate_space)
    {
        switch(semantic_type)
        {
        case VertexAttribute::SEMANTIC_TYPE_POSITION:
            metadata.m_HasAttributeWorldPosition |= coordinate_space == COORDINATE_SPACE_WORLD;
            metadata.m_HasAttributeLocalPosition |= coordinate_space == COORDINATE_SPACE_LOCAL;
            break;
        case VertexAttribute::SEMANTIC_TYPE_TEXCOORD:
            metadata.m_HasAttributeTexCoord = true;
            break;
        case VertexAttribute::SEMANTIC_TYPE_PAGE_INDEX:
            metadata.m_HasAttributePageIndex = true;
            break;
        case VertexAttribute::SEMANTIC_TYPE_NORMAL:
            metadata.m_HasAttributeNormal = true;
            break;
        case VertexAttribute::SEMANTIC_TYPE_TANGENT:
            metadata.m_HasAttributeTangent = true;
            break;
        case VertexAttribute::SEMANTIC_TYPE_COLOR:
            metadata.m_HasAttributeColor = true;
            break;
        case VertexAttribute::SEMANTIC_TYPE_WORLD_MATRIX:
            metadata.m_HasAttributeWorldMatrix = true;
            break;
        case VertexAttribute::SEMANTIC_TYPE_NORMAL_MATRIX:
            metadata.m_HasAttributeNormalMatrix = true;
            break;
        case VertexAttribute::SEMANTIC_TYPE_NONE:
            metadata.m_HasAttributeNone = true;
            break;
        default:
            break;
        }
    }

    static inline VertexAttributeInfoMetadata GetVertexAttributeInfosMetaData(const VertexAttributeInfos& attribute_infos)
    {
        VertexAttributeInfoMetadata metadata = {};
        for (int i = 0; i < attribute_infos.m_NumInfos; ++i)
        {
            const VertexAttributeInfo& info = attribute_infos.m_Infos[i];
            VertexAttributeInfoMetadataMember(metadata, info.m_SemanticType, info.m_CoordinateSpace);
        }
        return metadata;
    }

    static inline bool IsTypeTextureType(Type type)
    {
        return type == TYPE_SAMPLER_2D ||
               type == TYPE_SAMPLER_2D_ARRAY ||
               type == TYPE_TEXTURE_2D ||
               type == TYPE_TEXTURE_2D_ARRAY ||
               type == TYPE_IMAGE_2D ||

               type == TYPE_SAMPLER_3D ||
               type == TYPE_SAMPLER_3D_ARRAY ||
               type == TYPE_TEXTURE_3D ||
               type == TYPE_IMAGE_3D ||

               type == TYPE_SAMPLER_CUBE ||
               type == TYPE_TEXTURE_CUBE;
    }

    static inline bool IsTextureType3D(TextureType type)
    {
        return type == TEXTURE_TYPE_3D || type == TEXTURE_TYPE_IMAGE_3D;
    }

    static inline uint32_t GetLayerCount(TextureType type)
    {
        return type == TEXTURE_TYPE_CUBE_MAP ? 6 : 1;
    }

    static inline TextureType TypeToTextureType(Type type)
    {
        switch(type)
        {
            case TYPE_SAMPLER_2D:       return TEXTURE_TYPE_2D;
            case TYPE_SAMPLER_3D:       return TEXTURE_TYPE_3D;
            case TYPE_SAMPLER_2D_ARRAY: return TEXTURE_TYPE_2D_ARRAY;
            case TYPE_SAMPLER_CUBE:     return TEXTURE_TYPE_CUBE_MAP;
            case TYPE_IMAGE_2D:         return TEXTURE_TYPE_IMAGE_2D;
            case TYPE_IMAGE_3D:         return TEXTURE_TYPE_IMAGE_3D;
            case TYPE_TEXTURE_2D:       return TEXTURE_TYPE_TEXTURE_2D;
            case TYPE_TEXTURE_3D:       return TEXTURE_TYPE_TEXTURE_3D;
            case TYPE_TEXTURE_2D_ARRAY: return TEXTURE_TYPE_TEXTURE_2D_ARRAY;
            case TYPE_TEXTURE_CUBE:     return TEXTURE_TYPE_TEXTURE_CUBE;
            case TYPE_SAMPLER:          return TEXTURE_TYPE_SAMPLER;
            default:break;
        }
        assert(0);
        return (TextureType) -1;
    }

    static inline uint32_t VectorTypeToElementCount(VertexAttribute::VectorType vector_type)
    {
        switch(vector_type)
        {
            case VertexAttribute::VECTOR_TYPE_SCALAR: return 1;
            case VertexAttribute::VECTOR_TYPE_VEC2:   return 2;
            case VertexAttribute::VECTOR_TYPE_VEC3:   return 3;
            case VertexAttribute::VECTOR_TYPE_VEC4:   return 4;
            case VertexAttribute::VECTOR_TYPE_MAT2:   return 4;
            case VertexAttribute::VECTOR_TYPE_MAT3:   return 9;
            case VertexAttribute::VECTOR_TYPE_MAT4:   return 16;
        }
        return 0;
    }

    static inline uint32_t DataTypeToByteWidth(VertexAttribute::DataType data_type)
    {
        switch(data_type)
        {
            case dmGraphics::VertexAttribute::TYPE_BYTE:           return 1;
            case dmGraphics::VertexAttribute::TYPE_UNSIGNED_BYTE:  return 1;
            case dmGraphics::VertexAttribute::TYPE_SHORT:          return 2;
            case dmGraphics::VertexAttribute::TYPE_UNSIGNED_SHORT: return 2;
            case dmGraphics::VertexAttribute::TYPE_INT:            return 4;
            case dmGraphics::VertexAttribute::TYPE_UNSIGNED_INT:   return 4;
            case dmGraphics::VertexAttribute::TYPE_FLOAT:          return 4;
            default: break;
        }
        return 0;
    }

    void InvalidateGraphicsHandles(HContext context);

    /** checks if the texture format is compressed
     * @name IsFormatTranscoded
     * @param format TextureImage::CompressionType
     * @return true if the format is compressed
     */
    bool IsFormatTranscoded(TextureImage::CompressionType format);

    /** checks if the texture format is compressed
     * @name Transcode
     * @param path The path of the texture
     * @param image The input image
     * @param format The desired output format
     * @param images An array of transcoded mipmaps
     * @param sizes An array of transcoded mipmap sizes
     * @param num_transcoded_mips (in) the size of the input arrays, (out) the number of mipmaps stored in the arrays
     * @param format TextureImage::CompressionType
     * @return true if the format is transcoded
     */
    bool Transcode(const char* path, TextureImage::Image* image, uint8_t image_count, uint8_t* image_bytes, TextureFormat format, uint8_t** images, uint32_t* sizes, uint32_t* num_transcoded_mips);

    uint32_t    GetTypeSize(Type type);
    const char* GetGraphicsTypeLiteral(Type type);

    // Test functions:
    void* MapVertexBuffer(HContext context, HVertexBuffer buffer, BufferAccess access);
    bool  UnmapVertexBuffer(HContext context, HVertexBuffer buffer);
    void* MapIndexBuffer(HContext context, HIndexBuffer buffer, BufferAccess access);
    bool  UnmapIndexBuffer(HContext context, HIndexBuffer buffer);
}

#endif // DM_GRAPHICS_H
