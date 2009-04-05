///////////////////////////////////////////////////////////////////////////////////
//
//	graphics_device.h - Graphics device interface
//
///////////////////////////////////////////////////////////////////////////////////
#ifndef __GRAPHICSDEVICE_H__
#define __GRAPHICSDEVICE_H__
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>



#include "opengl/opengl_device.h"



// primitive type
enum GFXPrimitiveType
{
    GFX_PRIMTYPE_POINTLIST      = GD_PRIMTYPE_POINTLIST,
    GFX_PRIMTYPE_LINES          = GD_PRIMTYPE_LINES,
    GFX_PRIMTYPE_LINE_LOOP      = GD_PRIMTYPE_LINE_LOOP,
    GFX_PRIMTYPE_LINE_STRIP     = GD_PRIMTYPE_LINE_STRIP,
    GFX_PRIMTYPE_TRIANGLES      = GD_PRIMTYPE_TRIANGLES,
    GFX_PRIMTYPE_TRIANGLE_STRIP = GD_PRIMTYPE_TRIANGLE_STRIP,
    GFX_PRIMTYPE_TRIANGLE_FAN   = GD_PRIMTYPE_TRIANGLE_FAN,
};


// buffer clear types
enum GFXBufferClear
{
    GFX_CLEAR_RENDERBUFFER_R    = GD_CLEAR_RENDERBUFFER_R,
    GFX_CLEAR_RENDERBUFFER_G    = GD_CLEAR_RENDERBUFFER_G,
    GFX_CLEAR_RENDERBUFFER_B    = GD_CLEAR_RENDERBUFFER_B,
    GFX_CLEAR_RENDERBUFFER_A    = GD_CLEAR_RENDERBUFFER_A,
    GFX_CLEAR_RENDERBUFFER_ALL  = GD_CLEAR_RENDERBUFFER_ALL,
    GFX_CLEAR_DEPTHBUFFER       = GD_CLEAR_DEPTHBUFFER,
    GFX_CLEAR_STENCILBUFFER     = GD_CLEAR_STENCILBUFFER
};

// bool states
enum GFXRenderState
{
    GFX_DEPTH_TEST               = GD_STATE_DEPTH_TEST,
    GFX_ALPHA_BLEND              = GD_STATE_ALPHA_TEST
};


enum GFXMatrixType
{
    GFX_MATRIX_TYPE_WORLDVIEW   = GD_MATRIX_TYPE_WORLDVIEW,
    GFX_MATRIX_TYPE_PROJECTION  = GD_MATRIX_TYPE_PROJECTION
};



// Parameter structure for CreateDevice
struct GFXSCreateDeviceParams
{
    uint32_t        m_DisplayWidth;
    uint32_t        m_DisplayHeight;
    const char*	    m_AppTitle;
    bool            m_Fullscreen;
};




GFXHDevice GFXCreateDevice(GFXSCreateDeviceParams *params);

// clear/draw functions
void GFXFlip();
void GFXClear(GFXHContext context, uint32_t flags, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha, float depth, uint32_t stencil);
void GFXDrawTriangle2D(GFXHContext context, const float* vertices, const float* colours);
void GFXDrawTriangle3D(GFXHContext context, const float* vertices, const float* colours);

void GFXSetViewport(GFXHContext context, int width, int height, float field_of_view, float z_near, float z_far);

void GFXEnableState(GFXHContext context, GFXRenderState state);
void GFXDisableState(GFXHContext context, GFXRenderState state);



//        bool ResetDevice ( SCreateDeviceParams *param );
//       void DestroyDevice ( void );
//      void GetDeviceInfo( SDeviceInfo *p_deviceinfo );
//        void GetDeviceStats( SDeviceStats *p_stats );

//        void Draw (GFXHContext context, EPrimitiveType primitive_type, int32_t first, int32_t count );
//        void DrawInstanced ( void );
//        void DrawIndexed (HContext context, EPrimitiveType primitive_type, int32 offset, int32 count, HVertexBuffer vertex_buffer, Graphics::HIndexBuffer index_buffer );
//        void DrawIndexedInstanced ( void );
//        void DrawAuto ( void );
//        void DrawQuad(HContext context, float u0, float v0, float u1, float v1, int32_t x0, int32_t y0, int32_t x1, int32_t y1);
//        void DrawInline( HContext context, HVertexDeclaration decl,     uint32 vertex_count, float* vertex_data, uint32 triangle_count, uint32* index_data );



#if 0
// resource type id (is used as table offset - must be 0-15)
enum EResourceType
{
    RESOURCETYPE_UNMOVABLE = 0,
    RESOURCETYPE_TEXTURE,
    RESOURCETYPE_RENDERTARGET,
    RESOURCETYPE_SURFACE,
    RESOURCETYPE_INDEXBUFFER,
    RESOURCETYPE_VERTEXBUFFER,
    RESOURCETYPE_VERTEXPROGRAM,
    RESOURCETYPE_FRAGMENTPROGRAM,
    RESOURCETYPE_CONSTANTBUFFER,
    RESOURCETYPE_VERTEXDECLARATION,
    RESOURCETYPE_CONTEXT,
    RESOURCETYPE_DEVICE,
    RESOURCETYPE_RENDERSETUP,
    RESOURCETYPE_SOUND = 15,
    RESOURCETYPE_ENUM,
};

// usagetype of surface/texture
enum EUsageType
{
    USAGETYPE_IMMUTABLE		= GD_USAGETYPE_IMMUTABLE, // Never changes
    USAGETYPE_DEFAULT		= GD_USAGETYPE_DEFAULT,   // Changes rarely
    USAGETYPE_DYNAMIC		= GD_USAGETYPE_DYNAMIC,   // Changes frequently
    USAGETYPE_RENDERTARGET	= GD_USAGETYPE_RENDERTARGET,
};

// memory pool type
enum EPoolType
{
    POOLTYPE_GFXMEM			= GD_POOLTYPE_GFXMEM,
    POOLTYPE_SYSMEM			= GD_POOLTYPE_SYSMEM
};

// type of buffer locking
enum ELockType
{
    LOCKTYPE_READ_WRITE 	= GD_LOCKTYPE_READ_WRITE,
    LOCKTYPE_READ_ONLY  	= GD_LOCKTYPE_READ_ONLY,
    LOCKTYPE_WRITE_ONLY 	= GD_LOCKTYPE_WRITE_ONLY,
};

// type of shader allocation flags
enum EShaderAllocationFlags
{
    SHADERALLOCATION_NONE		= GD_SHADERALLOCATION_NONE,
    SHADERALLOCATION_PREDICATED = GD_SHADERALLOCATION_PREDICATED
};

// index type
enum EIndexType
{
    INDEXTYPE_16			= GD_INDEXTYPE_16,
    INDEXTYPE_32			= GD_INDEXTYPE_32
};



// color mask types
enum EColorMask
{
    COLORMASK_R	= GD_COLORMASK_R,
    COLORMASK_G	= GD_COLORMASK_G,
    COLORMASK_B	= GD_COLORMASK_B,
    COLORMASK_A	= GD_COLORMASK_A,
    COLORMASK_NONE = 0,
    COLORMASK_ALL = GD_COLORMASK_R | GD_COLORMASK_G | GD_COLORMASK_B | GD_COLORMASK_A,
    COLORMASK_NO_A = GD_COLORMASK_R | GD_COLORMASK_G | GD_COLORMASK_B,
};

// fillmode states
enum EFillMode
{
    FILLMODE_POINT		= GD_FILLMODE_POINT,
    FILLMODE_WIREFRAME	= GD_FILLMODE_WIREFRAME,
    FILLMODE_SOLID		= GD_FILLMODE_SOLID,
};


// result codes
enum EResult
{
    RESULT_OK				= GD_RESULT_OK,
    RESULT_FAIL				= GD_RESULT_FAIL,
    RESULT_DEVICELOST		= GD_RESULT_DEVICELOST,
};

//  context locking parameter declaration
enum ELockContextParam
{
    LC_TILING_PREDICATE_COMPONENTS	= GD_LC_TILING_PREDICATE_COMPONENTS,
    LC_TILING_PREDICATE_WHOLE		= GD_LC_TILING_PREDICATE_WHOLE,
    LC_ZPASS						= GD_LC_ZPASS
};

//  resolvesurface parameter declaration
enum EResolveSurfaceParam
{
    RS_RENDERTARGET0		= GD_RS_RENDERTARGET0,
    RS_RENDERTARGET1		= GD_RS_RENDERTARGET1,
    RS_RENDERTARGET2		= GD_RS_RENDERTARGET2,
    RS_RENDERTARGET3		= GD_RS_RENDERTARGET3,
    RS_DEPTHSTENCIL			= GD_RS_DEPTHSTENCIL,
    RS_ALLFRAGMENTS			= GD_RS_ALLFRAGMENTS,
    RS_FRAGMENT0			= GD_RS_FRAGMENT0,
    RS_FRAGMENT1			= GD_RS_FRAGMENT1,
    RS_FRAGMENT2			= GD_RS_FRAGMENT2,
    RS_FRAGMENT3			= GD_RS_FRAGMENT3,
    RS_FRAGMENTS01			= GD_RS_FRAGMENTS01,
    RS_FRAGMENTS23			= GD_RS_FRAGMENTS23,
    RS_FRAGMENTS0123		= GD_RS_FRAGMENTS0123,
    RS_CLEARRENDERTARGET	= GD_RS_CLEARRENDERTARGET,
    RS_CLEARDEPTHSTENCIL	= GD_RS_CLEARDEPTHSTENCIL,
};

//  surface format declarations
enum ESurfaceFormat
{
    SURFACEFORMAT_GetFromFile			= GD_SURFACEFORMAT_GetFromFile,
    SURFACEFORMAT_Unknown				= GD_SURFACEFORMAT_Unknown,

    SURFACEFORMAT_ARGB32				= GD_SURFACEFORMAT_ARGB32,
    SURFACEFORMAT_LIN_ARGB32			= GD_SURFACEFORMAT_LIN_ARGB32,
    SURFACEFORMAT_ABGR32				= GD_SURFACEFORMAT_ABGR32,
    SURFACEFORMAT_LIN_ABGR32			= GD_SURFACEFORMAT_LIN_ABGR32,
    SURFACEFORMAT_RGBA32				= GD_SURFACEFORMAT_RGBA32,
    SURFACEFORMAT_LIN_RGBA32			= GD_SURFACEFORMAT_LIN_RGBA32,
    SURFACEFORMAT_BGRA32				= GD_SURFACEFORMAT_BGRA32,
    SURFACEFORMAT_LIN_BGRA32			= GD_SURFACEFORMAT_LIN_BGRA32,
    SURFACEFORMAT_RGB32					= GD_SURFACEFORMAT_RGB32,
    SURFACEFORMAT_LIN_RGB32				= GD_SURFACEFORMAT_LIN_RGB32,
    SURFACEFORMAT_BGR32					= GD_SURFACEFORMAT_BGR32,
    SURFACEFORMAT_LIN_BGR32				= GD_SURFACEFORMAT_LIN_BGR32,
    SURFACEFORMAT_G32R32				= GD_SURFACEFORMAT_G32R32,
    SURFACEFORMAT_D24S8					= GD_SURFACEFORMAT_D24S8,
    SURFACEFORMAT_LIN_D24S8				= GD_SURFACEFORMAT_LIN_D24S8,
    SURFACEFORMAT_D24X8					= GD_SURFACEFORMAT_D24X8,
    SURFACEFORMAT_F24S8					= GD_SURFACEFORMAT_F24S8,
    SURFACEFORMAT_LIN_F24S8				= GD_SURFACEFORMAT_LIN_F24S8,
    SURFACEFORMAT_RGB24					= GD_SURFACEFORMAT_RGB24,
    SURFACEFORMAT_ARGB1555				= GD_SURFACEFORMAT_ARGB1555,
    SURFACEFORMAT_LIN_ARGB1555			= GD_SURFACEFORMAT_LIN_ARGB1555,
    SURFACEFORMAT_ARGB4444				= GD_SURFACEFORMAT_ARGB4444,
    SURFACEFORMAT_LIN_ARGB4444			= GD_SURFACEFORMAT_LIN_ARGB4444,
    SURFACEFORMAT_RGB565				= GD_SURFACEFORMAT_RGB565,
    SURFACEFORMAT_LIN_RGB565			= GD_SURFACEFORMAT_LIN_RGB565,
    SURFACEFORMAT_RGB16					= GD_SURFACEFORMAT_RGB16,
    SURFACEFORMAT_G16R16F				= GD_SURFACEFORMAT_G16R16F,
    SURFACEFORMAT_G16R16				= GD_SURFACEFORMAT_G16R16,
    SURFACEFORMAT_LIN_RGB16				= GD_SURFACEFORMAT_LIN_RGB16,
    SURFACEFORMAT_L16					= GD_SURFACEFORMAT_L16,
    SURFACEFORMAT_LIN_L16				= GD_SURFACEFORMAT_LIN_L16,
    SURFACEFORMAT_D16					= GD_SURFACEFORMAT_D16,
    SURFACEFORMAT_LIN_D16				= GD_SURFACEFORMAT_LIN_D16,
    SURFACEFORMAT_F16					= GD_SURFACEFORMAT_F16,
    SURFACEFORMAT_LIN_F16				= GD_SURFACEFORMAT_LIN_F16,
    SURFACEFORMAT_V8U8					= GD_SURFACEFORMAT_V8U8,
    SURFACEFORMAT_LIN_V8U8				= GD_SURFACEFORMAT_LIN_V8U8,
    SURFACEFORMAT_A8L8					= GD_SURFACEFORMAT_A8L8,
    SURFACEFORMAT_LIN_A8L8				= GD_SURFACEFORMAT_LIN_A8L8,
    SURFACEFORMAT_G8R8					= GD_SURFACEFORMAT_G8R8,
    SURFACEFORMAT_LIN_G8R8				= GD_SURFACEFORMAT_LIN_G8R8,
    SURFACEFORMAT_L8					= GD_SURFACEFORMAT_L8,
    SURFACEFORMAT_LIN_L8				= GD_SURFACEFORMAT_LIN_L8,
    SURFACEFORMAT_A8					= GD_SURFACEFORMAT_A8,
    SURFACEFORMAT_LIN_A8				= GD_SURFACEFORMAT_LIN_A8,
    SURFACEFORMAT_DXT1					= GD_SURFACEFORMAT_DXT1,
    SURFACEFORMAT_DXT2					= GD_SURFACEFORMAT_DXT2,
    SURFACEFORMAT_DXT3					= GD_SURFACEFORMAT_DXT3,
    SURFACEFORMAT_DXT4					= GD_SURFACEFORMAT_DXT4,
    SURFACEFORMAT_DXT5					= GD_SURFACEFORMAT_DXT5,
    SURFACEFORMAT_W32Z32Y32X32			= GD_SURFACEFORMAT_W32Z32Y32X32,
    SURFACEFORMAT_W16Z16Y16X16			= GD_SURFACEFORMAT_W16Z16Y16X16,
    SURFACEFORMAT_R32F					= GD_SURFACEFORMAT_R32F,
    SURFACEFORMAT_LIN_R32F				= GD_SURFACEFORMAT_LIN_R32F,
    SURFACEFORMAT_X32					= GD_SURFACEFORMAT_R32F,

    //And some 360 specials
    SURFACEFORMAT_DXN					= GD_SURFACEFORMAT_DXN,
    SURFACEFORMAT_LIN_DXN				= GD_SURFACEFORMAT_LIN_DXN,
    SURFACEFORMAT_LIN_DXT1				= GD_SURFACEFORMAT_LIN_DXT1,
    SURFACEFORMAT_LIN_DXT2				= GD_SURFACEFORMAT_LIN_DXT2,
    SURFACEFORMAT_LIN_DXT3				= GD_SURFACEFORMAT_LIN_DXT3,
    SURFACEFORMAT_LIN_DXT4				= GD_SURFACEFORMAT_LIN_DXT4,
    SURFACEFORMAT_LIN_DXT5				= GD_SURFACEFORMAT_LIN_DXT5,
    SURFACEFORMAT_A2R10G10B10			= GD_SURFACEFORMAT_A2R10G10B10,
    SURFACEFORMAT_A2B10G10R10F_EDRAM	= GD_SURFACEFORMAT_A2B10G10R10F_EDRAM,
    SURFACEFORMAT_A16B16G16R16F			= GD_SURFACEFORMAT_A16B16G16R16F,
    SURFACEFORMAT_A32B32G32R32F			= GD_SURFACEFORMAT_A32B32G32R32F,
    SURFACEFORMAT_LIN_A16B16G16R16F		= GD_SURFACEFORMAT_LIN_A16B16G16R16F,
    SURFACEFORMAT_LIN_A32B32G32R32F		= GD_SURFACEFORMAT_LIN_A32B32G32R32F,
    SURFACEFORMAT_A16B16G16R16			= GD_SURFACEFORMAT_A16B16G16R16,
    SURFACEFORMAT_D24FS8				= GD_SURFACEFORMAT_D24FS8,

    // ATI special
    SURFACEFORMAT_FOURCC_DF16			= GD_SURFACEFORMAT_FOURCC_DF16,
    SURFACEFORMAT_FOURCC_DF24			= GD_SURFACEFORMAT_FOURCC_DF24,

    // Nvidia special
    SURFACEFORMAT_FOURCC_RAWZ			= GD_SURFACEFORMAT_FOURCC_RAWZ,
    SURFACEFORMAT_FOURCC_INTZ			= GD_SURFACEFORMAT_FOURCC_INTZ,
};

// type of cubetexture face
enum ECubeTextureFaceType
{
    CUBETEX_FACE_POSITIVE_X = GD_CUBETEX_FACE_POSITIVE_X,
    CUBETEX_FACE_NEGATIVE_X = GD_CUBETEX_FACE_NEGATIVE_X,
    CUBETEX_FACE_POSITIVE_Y = GD_CUBETEX_FACE_POSITIVE_Y,
    CUBETEX_FACE_NEGATIVE_Y = GD_CUBETEX_FACE_NEGATIVE_Y,
    CUBETEX_FACE_POSITIVE_Z = GD_CUBETEX_FACE_POSITIVE_Z,
    CUBETEX_FACE_NEGATIVE_Z = GD_CUBETEX_FACE_NEGATIVE_Z
};

// type of multisample
enum EMultisampleFormat
{
    MULTISAMPLE_NONE		= GD_MULTISAMPLE_NONE,
    MULTISAMPLE_2X			= GD_MULTISAMPLE_2X,
    MULTISAMPLE_4X			= GD_MULTISAMPLE_4X,
    MULTISAMPLE_8X			= GD_MULTISAMPLE_8X,
};

enum EConstantBuffer
{
    CB_0 = 0,
    CB_1 = 1,
    CB_2 = 2,
    CB_3 = 3,
};


// ..
enum EProgramParameterType
{
    PARAMTYPE_SCALAR,
    PARAMTYPE_VECTOR,
    PARAMTYPE_MATRIX
};

// declaration type of data constants
// (PS3: if you change this, make sure you update "int32 GetSizeofElement(int32 e)")
enum EType
{
    VERTEX_TYPE_SHORT		= GD_VERTEX_TYPE_SHORT,
    VERTEX_TYPE_USHORT		= GD_VERTEX_TYPE_USHORT,
    VERTEX_TYPE_INT			= GD_VERTEX_TYPE_INT,
    VERTEX_TYPE_UINT		= GD_VERTEX_TYPE_UINT,
    VERTEX_TYPE_FLOAT		= GD_VERTEX_TYPE_FLOAT,
    VERTEX_TYPE_FLOAT16		= GD_VERTEX_TYPE_FLOAT16,
    VERTEX_TYPE_UBYTE		= GD_VERTEX_TYPE_UBYTE,
    VERTEX_TYPE_SHORT_NN	= GD_VERTEX_TYPE_SHORT_NN,
    VERTEX_TYPE_USHORT_NN	= GD_VERTEX_TYPE_USHORT_NN,
    VERTEX_TYPE_CMP			= GD_VERTEX_TYPE_CMP,
    VERTEX_TYPE_UBYTE256	= GD_VERTEX_TYPE_UBYTE256
};

// usage declaration of vertexdeclaration element
enum EVertexDeclarationUsage
{
    DECLUSAGE_POSITION		= GD_DECLUSAGE_POSITION,
    DECLUSAGE_BLENDWEIGHT	= GD_DECLUSAGE_BLENDWEIGHT,
    DECLUSAGE_BLENDINDICES	= GD_DECLUSAGE_BLENDINDICES,
    DECLUSAGE_NORMAL		= GD_DECLUSAGE_NORMAL,
    DECLUSAGE_PSIZE			= GD_DECLUSAGE_PSIZE,
    DECLUSAGE_TEXCOORD		= GD_DECLUSAGE_TEXCOORD,
    DECLUSAGE_TANGENT		= GD_DECLUSAGE_TANGENT,
    DECLUSAGE_BINORMAL		= GD_DECLUSAGE_BINORMAL,
    DECLUSAGE_TESSFACTOR	= GD_DECLUSAGE_TESSFACTOR,
    DECLUSAGE_COLOR			= GD_DECLUSAGE_COLOR,
    DECLUSAGE_FOG			= GD_DECLUSAGE_FOG,
    DECLUSAGE_DEPTH			= GD_DECLUSAGE_DEPTH,
    DECLUSAGE_SAMPLE		= GD_DECLUSAGE_SAMPLE,
    DECLUSAGE_VOID			= GD_DECLUSAGE_VOID
};


// front face
enum EFrontFace
{
    FRONTFACE_CW			 = GD_FRONTFACE_CW,
    FRONTFACE_CCW			 = GD_FRONTFACE_CCW
};


// back face culling
enum ECullFace
{
    CULLFACE_NONE			= GD_CULLFACE_NONE,
    CULLFACE_BACK			= GD_CULLFACE_BACK,
    CULLFACE_FRONT			= GD_CULLFACE_FRONT
};

// type of depthfunc cmp
enum EDepthFunc
{
    DEPTHFUNC_NEVER			= GD_DEPTHFUNC_NEVER,
    DEPTHFUNC_LESS			= GD_DEPTHFUNC_LESS,
    DEPTHFUNC_EQUAL			= GD_DEPTHFUNC_EQUAL,
    DEPTHFUNC_LESSEQUAL		= GD_DEPTHFUNC_LESSEQUAL,
    DEPTHFUNC_GREATER		= GD_DEPTHFUNC_GREATER,
    DEPTHFUNC_NOTEQUAL		= GD_DEPTHFUNC_NOTEQUAL,
    DEPTHFUNC_GREATEREQUAL	= GD_DEPTHFUNC_GREATEREQUAL,
    DEPTHFUNC_ALWAYS		= GD_DEPTHFUNC_ALWAYS
};

// type of depthfunc cmp
enum EStencilFunc
{
    STENCILFUNC_NEVER			= GD_STENCILFUNC_NEVER,
    STENCILFUNC_LESS			= GD_STENCILFUNC_LESS,
    STENCILFUNC_EQUAL			= GD_STENCILFUNC_EQUAL,
    STENCILFUNC_LESSEQUAL		= GD_STENCILFUNC_LESSEQUAL,
    STENCILFUNC_GREATER			= GD_STENCILFUNC_GREATER,
    STENCILFUNC_NOTEQUAL		= GD_STENCILFUNC_NOTEQUAL,
    STENCILFUNC_GREATEREQUAL	= GD_STENCILFUNC_GREATEREQUAL,
    STENCILFUNC_ALWAYS			= GD_STENCILFUNC_ALWAYS
};

enum EHiStencilFunc
{
    HISTENCILFUNC_NEVER			= GD_HISTENCILFUNC_NEVER,
    HISTENCILFUNC_LESS			= GD_HISTENCILFUNC_LESS,
    HISTENCILFUNC_EQUAL			= GD_HISTENCILFUNC_EQUAL,
    HISTENCILFUNC_LESSEQUAL		= GD_HISTENCILFUNC_LESSEQUAL,
    HISTENCILFUNC_GREATER		= GD_HISTENCILFUNC_GREATER,
    HISTENCILFUNC_NOTEQUAL		= GD_HISTENCILFUNC_NOTEQUAL,
    HISTENCILFUNC_GREATEREQUAL	= GD_HISTENCILFUNC_GREATEREQUAL,
    HISTENCILFUNC_ALWAYS		= GD_HISTENCILFUNC_ALWAYS
};

enum EHiStenciFlushType
{
    HISTENCILFLUSHTYPE_ASYNCHRONOUS	= GD_HISTENCILFLUSHTYPE_ASYNCHRONOUS,
    HISTENCILFLUSHTYPE_SYNCHRONOUS	= GD_HISTENCILFLUSHTYPE_SYNCHRONOUS
};

enum EStencilOp
{
    STENCILOP_KEEP				= GD_STENCILOP_KEEP,
    STENCILOP_ZERO				= GD_STENCILOP_ZERO,
    STENCILOP_REPLACE			= GD_STENCILOP_REPLACE,
    STENCILOP_INCR_CLAMP		= GD_STENCILOP_INCR_CLAMP,
    STENCILOP_DECR_CLAMP		= GD_STENCILOP_DECR_CLAMP,
    STENCILOP_INCR_WRAP 		= GD_STENCILOP_INCR_WRAP,
    STENCILOP_DECR_WRAP 		= GD_STENCILOP_DECR_WRAP,
    STENCILOP_INVERT			= GD_STENCILOP_INVERT
};

// blend enable state
enum EBlendEnable
{
    BLEND_DISABLED			= GD_BLEND_DISABLED,
    BLEND_ENABLED			= GD_BLEND_ENABLED,
    BLEND_HIGHPRECISION		= GD_BLEND_HIGHPRECISION,
};

// type of blendfunc
enum EBlendFunc
{
    BLENDFUNC_ZERO				= GD_BLENDFUNC_ZERO,
    BLENDFUNC_ONE				= GD_BLENDFUNC_ONE,
    BLENDFUNC_SRCCOLOR			= GD_BLENDFUNC_SRCCOLOR,
    BLENDFUNC_INVSRCCOLOR		= GD_BLENDFUNC_INVSRCCOLOR,
    BLENDFUNC_SRCALPHA			= GD_BLENDFUNC_SRCALPHA,
    BLENDFUNC_INVSRCALPHA		= GD_BLENDFUNC_INVSRCALPHA,
    BLENDFUNC_DESTALPHA			= GD_BLENDFUNC_DESTALPHA,
    BLENDFUNC_INVDESTALPHA		= GD_BLENDFUNC_INVDESTALPHA,
    BLENDFUNC_DESTCOLOR			= GD_BLENDFUNC_DESTCOLOR,
    BLENDFUNC_INVDESTCOLOR		= GD_BLENDFUNC_INVDESTCOLOR,
    BLENDFUNC_SRCALPHASAT		= GD_BLENDFUNC_SRCALPHASAT,
    BLENDFUNC_BLENDFACTOR		= GD_BLENDFUNC_BLENDFACTOR,
    BLENDFUNC_INVBLENDFACTOR	= GD_BLENDFUNC_INVBLENDFACTOR
};

// type of blendfunc
enum EBlendEq
{
    BLENDEQ_ADD			= GD_BLENDEQ_ADD,
    BLENDEQ_SUBTRACT	= GD_BLENDEQ_SUBTRACT,
    BLENDEQ_REVSUBTRACT	= GD_BLENDEQ_REVSUBTRACT,
    BLENDEQ_MIN			= GD_BLENDEQ_MIN,
    BLENDEQ_MAX 		= GD_BLENDEQ_MAX
};

// type of textureaddress
enum ETextureAddress
{
    TEXADDRESS_WRAP			= GD_TEXADDRESS_WRAP,
    TEXADDRESS_MIRROR		= GD_TEXADDRESS_MIRROR,
    TEXADDRESS_CLAMP		= GD_TEXADDRESS_CLAMP,
    TEXADDRESS_BORDER		= GD_TEXADDRESS_BORDER,
    TEXADDRESS_MIRRORONCE	= GD_TEXADDRESS_MIRRORONCE
};

enum ETextureRemap
{
    TEXREMAP_NORMAL			= GD_TEXREMAP_NORMAL,
    TEXREMAP_BIASED			= GD_TEXREMAP_BIASED
};
enum ETextureZFunc
{
    TEXZFUNC_NONE			= GD_TEXZFUNC_NONE,
    TEXZFUNC_NEVER			= GD_TEXZFUNC_NEVER,
    TEXZFUNC_LESS			= GD_TEXZFUNC_LESS,
    TEXZFUNC_EQUAL			= GD_TEXZFUNC_EQUAL,
    TEXZFUNC_LESSEQUAL		= GD_TEXZFUNC_LESSEQUAL,
    TEXZFUNC_GREATER		= GD_TEXZFUNC_GREATER,
    TEXZFUNC_NOTEQUAL		= GD_TEXZFUNC_NOTEQUAL,
    TEXZFUNC_GREATEREQUAL	= GD_TEXZFUNC_GREATEREQUAL,
    TEXZFUNC_ALWAYS			= GD_TEXZFUNC_ALWAYS
};
enum ETextureGamma
{
    TEXGAMMA_R				= GD_TEXGAMMA_R,
    TEXGAMMA_G				= GD_TEXGAMMA_G,
    TEXGAMMA_B				= GD_TEXGAMMA_B,
    TEXGAMMA_A				= GD_TEXGAMMA_A,
    TEXGAMMA_RGB			= GD_TEXGAMMA_R | GD_TEXGAMMA_G | GD_TEXGAMMA_B,
    TEXGAMMA_NONE			= GD_TEXGAMMA_NONE
};

enum EColourSpace
{
    E_COLOURSPACE_LINEAR	= 0,
    E_COLOURSPACE_SRGB		= 1
};


enum EMaxAniso
{
    MAX_ANISO_1				= GD_MAX_ANISO_1,
    MAX_ANISO_2				= GD_MAX_ANISO_2,
    MAX_ANISO_4				= GD_MAX_ANISO_4,
    MAX_ANISO_6				= GD_MAX_ANISO_6,
    MAX_ANISO_8				= GD_MAX_ANISO_8,
    MAX_ANISO_10			= GD_MAX_ANISO_10,
    MAX_ANISO_12			= GD_MAX_ANISO_12,
    MAX_ANISO_16			= GD_MAX_ANISO_16,
};

/*!
 *	The available types of hardware supported filtering
 *	Where there are two filter modes, the first is for minification
 *	and the second one is for mip filtering.
 */
enum ETextureFilter
{
    TEXFILTER_NONE				= GD_TEXFILTER_NONE,
    TEXFILTER_NEAREST			= GD_TEXFILTER_NEAREST,
    TEXFILTER_LINEAR			= GD_TEXFILTER_LINEAR,
    TEXFILTER_LINEAR_LINEAR		= GD_TEXFILTER_LINEAR_LINEAR,
    TEXFILTER_LINEAR_NEAREST	= GD_TEXFILTER_LINEAR_NEAREST,
    TEXFILTER_NEAREST_LINEAR	= GD_TEXFILTER_NEAREST_LINEAR,
    TEXFILTER_NEAREST_NEAREST	= GD_TEXFILTER_NEAREST_NEAREST,
    TEXFILTER_ANISOTROPIC		= GD_TEXFILTER_ANISOTROPIC,
    TEXFILTER_QUINCUNX			= GD_TEXFILTER_QUINCUNX,
    TEXFILTER_GAUSSIAN			= GD_TEXFILTER_GAUSSIAN
};

enum ESpriteRMode
{
    SPRITE_RMODE_ZERO			= GD_SPRITE_RMODE_ZERO,
    SPRITE_RMODE_FROM_R			= GD_SPRITE_RMODE_FROM_R,
    SPRITE_RMODE_FROM_S			= GD_SPRITE_RMODE_FROM_S
};

enum ESpriteTexcoord
{
    SPRITE_TEXCOORD_TEX0		= GD_SPRITE_TEXCOORD_TEX0,
    SPRITE_TEXCOORD_TEX1		= GD_SPRITE_TEXCOORD_TEX1,
    SPRITE_TEXCOORD_TEX2		= GD_SPRITE_TEXCOORD_TEX2,
    SPRITE_TEXCOORD_TEX3		= GD_SPRITE_TEXCOORD_TEX3,
    SPRITE_TEXCOORD_TEX4		= GD_SPRITE_TEXCOORD_TEX4,
    SPRITE_TEXCOORD_TEX5		= GD_SPRITE_TEXCOORD_TEX5,
    SPRITE_TEXCOORD_TEX6		= GD_SPRITE_TEXCOORD_TEX6,
    SPRITE_TEXCOORD_TEX7		= GD_SPRITE_TEXCOORD_TEX7,
    SPRITE_TEXCOORD_TEX8		= GD_SPRITE_TEXCOORD_TEX8,
    SPRITE_TEXCOORD_TEX9		= GD_SPRITE_TEXCOORD_TEX9
};


//! Default RenderSetups that is created by the device
enum EDefaultRenderSetup
{
    RENDER_SETUP_DEFAULT,                     //!< The default render targets with default depth target.
    RENDER_SETUP_DEFAULT_TILED,               //!< The default tiled render targets with default depth target (The same as RENDER_SETUP_DEFAULT on Win32 and PS3).
    RENDER_SETUP_DEFAULT_NO_DEPTH,            //!< The default render targets with no depth target.
    RENDER_SETUP_DEFAULT_Z_PASS,              //!< The default render targets for the Z Pass.
    RENDER_SETUP_DEFAULT_Z_AND_VELOCITY_PASS, //!< The default render targets for the ZAndVelocity Pass.
    RENDER_SETUP_MULTISAMPLED,                //!< The default multisampled render targets with default depth target.
    RENDER_SETUP_POST_DRAW //!< This setup should be used for rendering gui. (It's the same as RENDER_SETUP_DEFAULT except no gamma correction is done on xenon.)
};

//! Warning type for the device
enum EWarningType
{
    WARNING_COMMAND_BUFFER_OVERRUN //!< The command buffer is full which causes a CPU stall.
};

//! Extended attribute parameters creating device
enum EExtendedDeviceCreationAttributes
{
    CREATE_DEVICE_EXT_ATTRIBUTE_REFERENCE_DEVICE = 0x1,
    CREATE_DEVICE_EXT_ATTRIBUTE_NULL_DEVICE = 0x2,
    CREATE_DEVICE_EXT_ATTRIBUTE_SOFTWARE_VERTEX_PROCESSING = 0x4,
    CREATE_DEVICE_EXT_ATTRIBUTE_MULTITHREADED = 0x8
};

// blend state struct
struct SBlendState
{
    EBlendFunc SrcBlend;
    EBlendEq BlendOp;
    EBlendFunc DestBlend;
    EBlendFunc SrcBlendAlpha;
    EBlendEq BlendOpAlpha;
    EBlendFunc DestBlendAlpha;
};

// GraphDevice structure of a vertexdeclaration element
struct SVertexElement
{
    int32 Stream;
    int32 Size;
    EType Type;
    EVertexDeclarationUsage Usage;
    int32 UsageIndex;
};

// Device info structure
struct SDeviceInfo
{
    uint32 display_width;
    uint32 display_height;
    float  display_ratio; // Output display ratio. Not necessarily equal to width / height.
    uint32 display_presentationinterval;
    EMultisampleFormat multisample_format;
    ESurfaceFormat rendertarget_format;
    ESurfaceFormat depthbuffer_format;
    uint32 rendertarget_count;
    uint32 tile_count;
    bool   fullscreen;
};




// device functions
void SetInvalidateDeviceCallback(void (* func)());
void SetRestoreDeviceCallback(void (* func)());


// vertex buffer functions
Graphics::HVertexBuffer CreateVertexBuffer ( int32 element_size, int32 element_count, EUsageType usage_type, EPoolType pool_type, int32 vb_count = -1, void *data = NULL );
void* LockVertexBuffer ( HVertexBuffer buffer, ELockType lock_type, int32 bufferindex = -1 );
void UnlockVertexBuffer ( HVertexBuffer buffer, int32 bufferindex = -1 );
void DestroyVertexBuffer ( HVertexBuffer buffer );
void  DuplicateVertexBuffer ( HVertexBuffer buffer, int32 frombufferindex = -1 );
int32 GetVertexBufferElementCount( HVertexBuffer buffer );
int32 GetVertexBufferStride( HVertexBuffer buffer );
int32 GetVertexBufferSize ( HVertexBuffer buffer );

// index buffer functions
Graphics::HIndexBuffer CreateIndexBuffer ( EIndexType index_type, int32 element_count, EUsageType usage_type, EPoolType pool_type, int32 ib_count = -1, void *data = NULL );
void* LockIndexBuffer ( HIndexBuffer buffer, ELockType lock_type, int32 bufferindex = -1  );
void UnlockIndexBuffer ( HIndexBuffer buffer, int32 bufferindex = -1 );
void DestroyIndexBuffer ( HIndexBuffer buffer );
void  DuplicateIndexBuffer ( HIndexBuffer buffer, int32 frombufferindex = -1 );
int32 GetIndexBufferElementCount( HIndexBuffer buffer );
int32 GetIndexBufferStride( HIndexBuffer buffer );
int32 GetIndexBufferSize ( HIndexBuffer buffer );

// constant buffer functions
HConstantBuffer CreateConstantBuffer ( int32 Vector4fcount, EUsageType usage_type );
void* LockConstantBuffer ( HConstantBuffer buffer, ELockType lock_type );
void UnlockConstantBuffer ( HConstantBuffer buffer );
void DestroyConstantBuffer ( HConstantBuffer buffer );

// vertex declaration functions
Graphics::HVertexDeclaration CreateVertexDeclaration( SVertexElement *parameters, int32 parameters_count, HVertexProgram program );
void DestroyVertexDeclaration( HVertexDeclaration vertex_declaration );

// vertex program functions
//! Attempts to create a vertex program from the data supplied in the program parameter. Returns null as the handle if not successful
HVertexProgram CreateVertexProgram(void *code, const uint32 size);
void DestroyVertexProgram(HVertexProgram program);
void ReCreateVertexProgram(HVertexProgram handle, void *code, const uint32 size);

// fragment program functions
//! Attempts to create a fragment program from the data supplied in the program parameter. Returns null as the handle if not successful
HFragmentProgram CreateFragmentProgram(void *code, const uint32 size);
void DestroyFragmentProgram(HFragmentProgram program);
void ReCreateFragmentProgram(HVertexProgram handle, void *code, const uint32 size);

// surface functions
//! \param inverted_zcull True if a depth surface should use "greater" as the depth compare function
//! \param tiled_memory False if a render target is not allowed to be a tiled surface
HSurface CreateRenderTargetSurface( uint32 width, uint32 height, ESurfaceFormat format, EMultisampleFormat ms_format, const char* name, bool inverted_zcull = false, bool tiled_memory = true );
void DestroySurface( HSurface surface );
//! Writes the description of the surface to the supplied pointer.
void GetSurfaceDesc( HSurface surface, SSurfaceDesc* desc );

// texture functions
//! \param inverted_zcull True if a depth surface should use "greater" as the depth compare function
//! \param tiled_memory False if a render target is not allowed to be a tiled surface
//! \param packed_mips (Xbox 360 only, ignored on other platforms) if True will pack mip-chain to conserve memory, thus the mipmaps may not
// adhere to alignment and tile boundaries.
HTexture CreateTexture( uint32 width, uint32 height, ESurfaceFormat format, EMultisampleFormat multisample_type, EUsageType usage_type, EPoolType pool_type, int32 lod_levels, const char* name, void *data = NULL, uint32 *out_base_size = 0, uint32 *out_mip_size = 0, bool inverted_zcull = false, bool tiled_memory = true, bool generate_mipmap = false, bool packed_mips = true );
void DestroyTexture ( HTexture texture );
void * LockTexture( HTexture texture, uint32 index, uint32 *p_out_pitch );
void UnlockTexture( HTexture texture, uint32 index );
void GetTextureDesc( HTexture texture, STextureDesc* desc );
void SetTextureSize( HTexture texture , uint16 height, uint16 width );

//! Returns a render target surface of one mip map level of the texture.
/*! The texture has to been created with the USAGETYPE_RENDERTARGET set.*/
HSurface GetRenderTarget( HContext context, HTexture texture, uint32 mip_level );
bool SetTextureData ( HTexture texture, uint32 index, void *source_data, int32 bytes_per_pixel );
uint32 GetTextureByteSize( uint32 width, uint32 height, uint32 mipmaps, ESurfaceFormat format );
uint32 GetSurfaceByteSize( uint32 width, uint32 height, ESurfaceFormat format );
float  GetTexturePixelByteSize( ESurfaceFormat format );

// cube texture functions
HTexture CreateCubeTexture( uint32 edgelength, ESurfaceFormat format, EUsageType usage_type, EPoolType pool_type, int32 lod_levels, const char* name, void *data = NULL, uint32 *out_base_size = 0, uint32 *out_mip_size = 0 );
void DestroyCubeTexture ( HTexture texture );
void * LockCubeTexture( HTexture texture, uint32 index, ECubeTextureFaceType face, uint32 *p_out_pitch );
void UnlockCubeTexture( HTexture texture, uint32 index, ECubeTextureFaceType face );
bool SetCubeTextureData (HTexture texture, uint32 index, ECubeTextureFaceType face, void *source_data, int32 bytes_per_pixel );

// 1D texture functions
HTexture CreateTexture1D(uint32 width, ESurfaceFormat format, EUsageType usage_type, EPoolType pool_type, int32 lod_levels, const char* name, void *data = NULL, uint32 *out_base_size = 0, uint32 *out_mip_size = 0);
void DestroyTexture1D(HTexture texture);
void *LockTexture1D(HTexture texture, uint32 index);
void UnlockTexture1D(HTexture texture, uint32 index);

// volume texture functions
HTexture CreateVolumeTexture(uint32 width, uint32 height, uint32 depth, ESurfaceFormat format, EUsageType usage_type, EPoolType pool_type, int32 lod_levels, uint32 *out_base_size = 0, uint32 *out_mip_size = 0);
void DestroyVolumeTexture(HTexture texture);
void *LockVolumeTexture(HTexture texture, uint32 index, uint32 *p_out_pitch, uint32 *p_out_slicepitch);
void UnlockVolumeTexture(HTexture texture, uint32 index);

void ClearVertexStreams( HContext context );


HRenderSetup CreateRenderSetup( HContext context, HSurface depth_target, HSurface color_target0,
        HSurface color_target1 = 0, HSurface color_target2 = 0, HSurface color_target3 = 0,
        uint32 mask = COLORMASK_ALL, bool custom_vp = false, int32 vp_x_scale = 0, int32 vp_x_offset = 0,
        int32 vp_y_scale = 0, int32 vp_y_offset = 0, int32 vp_z_scale = 0, int32 vp_z_offset = 0, bool redist = true, bool auto_resolve = true );
//! Activates the supplied RenderSetup and bind the render targets for rendering.
void SetRenderSetup(HContext context, HRenderSetup rendertarget, bool resolvecheck = true );
//! Returns one of the default RenderSetups created and owned of the device.
HRenderSetup GetDefaultRenderSetup(EDefaultRenderSetup setup);
HRenderSetup GetRenderSetup(HContext context);
//! Returns a render target from a render setup
HSurface GetRenderSetupSurface(HRenderSetup rendersetup, const uint32 index = 0);

//!-
void DestroyRenderSetup( HRenderSetup rendersetup );

void ResolveSurface(HContext context, HTexture desttexture, uint32 resolveflags, uint8 clearcolor_r = 0, uint8 clearcolor_g = 0, uint8 clearcolor_b = 0, uint8 clearcolor_a = 0, float clear_z = 1.0f, uint32 clear_stencil = 0, uint32 *source_rect = 0, uint32 *dest_point = 0, uint32 dest_mipmaplevel = 0, ECubeTextureFaceType dest_sliceorface = CUBETEX_FACE_POSITIVE_X, uint32 mask = COLORMASK_ALL );



// render scope functions
void BeginDraw();
void EndDraw();
EResult Flip();

// context operations
HContext CreateContext(EUsageType usage, uint32 cb_count = 2);
void DestroyContext(HContext context);
bool LockContext(HContext context, bool istiling = true, uint32 flags = LC_TILING_PREDICATE_COMPONENTS);
bool UnlockContext(HContext context);
void QueryContextCBMemoryStatus(HContext, uint32 *used, uint32 *remaining);
void ApplyContext(HContext context, HContext subcontext, uint32 predicationselect = 0);
void AcquireThreadOwnership(HContext context);
void ReleaseThreadOwnership(HContext context);
void SetThreadOwnership(HContext context, uint32 thread_id);
uint32 QueryThreadOwnership(HContext context);

// state setting (valid within LockContext - UnlockContext scope)
void SetVertexStream(HContext context, HVertexDeclaration vertex_declaration, HVertexBuffer buffer, int32 stream, int32 offset, int32 stride = -1);
void SetVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration );
void SetVertexProgram (HContext context, HVertexProgram program );
void SetVertexProgramBoolConstant( HContext context, int32 index, bool value );
void ResetVertexProgramCache(int i);

void SetFragmentProgram(HContext context, HFragmentProgram program );
void SetFragmentProgramBoolConstant( HContext context, int32 index, bool value );
void SetFragmentProgramRegisterCount( HFragmentProgram program, uint8 new_count );
void SetFragmentProgramGammaEnable( HContext context, bool enable );

// Constant buffer friendly constant management interface
void SetVertexProgramConstantBuffer(HContext context, EConstantBuffer buffer, HConstantBuffer constants);
void SetFragmentProgramConstantBuffer(HContext context, EConstantBuffer buffer, HConstantBuffer constants);
void SetVertexProgramConstantBufferSize(HContext context, EConstantBuffer buffer, uint32 index, uint32 count);
void SetFragmentProgramConstantBufferSize(HContext context, EConstantBuffer buffer, uint32 index, uint32 count);
void SetVertexProgramConstants(HContext context, EConstantBuffer buffer, uint32 index, const float *data, uint32 count);
void SetFragmentProgramConstants(HContext context, EConstantBuffer buffer, uint32 index, const float *data, uint32 count);

void SetViewport(HContext context, int32 x, int32 y, int32 w, int32 h, float minz, float maxz);
void SetAlphaTestFunc(HContext context, EDepthFunc sfactor, uint32 referencevalue);
void SetAlphaTestEnable(HContext context, ETrueFalse state);
void SetAntiAliasControl(HContext context, ETrueFalse enable, uint32 samplemask);
void SetFillMode(HContext context, EFillMode fillmode);

void SetBlendColor(HContext context, uint32 color, uint32 color2);
void SetBlendEnable(HContext context, EBlendEnable state);
void SetBlendState(HContext context, uint32 rendertargetindex, SBlendState *state);
void SetBlendFunc(HContext context, EBlendFunc scolor, EBlendFunc salpha, EBlendFunc dcolor, EBlendFunc dalpha);
void SetBlendFunc(HContext context, EBlendFunc sfactor, EBlendFunc dfactor);
void SetBlendEquation(HContext context, EBlendEq color, EBlendEq alpha);

//! Sets if alpha to coverage should be enabled
void SetAlphaToCoverageEnable(HContext context, ETrueFalse enable);

void SetStencilFunc(HContext context, EStencilFunc func, uint32 ref, uint32 mask);
void SetStencilMask(HContext context, uint8 mask);
void SetStencilOp(HContext context, EStencilOp fail, EStencilOp depthFail, EStencilOp depthPass);
void SetStencilTestEnable(HContext context, ETrueFalse state);
void SetHiStencilControl(HContext context, EHiStencilFunc sFunc, uint8 sRef, uint8 sMask);

void SetColorMask(HContext context, uint32 mask);
void SetCullFace(HContext context, ECullFace cfm);
void SetFrontFace(HContext context, EFrontFace dir);

void SetDepthFunc(HContext context, EDepthFunc func);
void SetDepthWriteEnable(HContext context, ETrueFalse state);
void SetDepthClampEnable(HContext context, ETrueFalse state);

void SetLineSmoothEnable(HContext context, ETrueFalse state);
void SetPointSize(HContext context, float size);
void SetPointSpriteControl(HContext context, ETrueFalse enable, ESpriteRMode rmode, ESpriteTexcoord texcoord);
void SetDepthBias(HContext context, float factor, float units);
void SetDepthBiasEnable(HContext context, ETrueFalse state);
void SetPolySmoothEnable(HContext context, ETrueFalse state);

void SetTexture(HContext context, HTexture texture, uint8 index, bool force_hi_precision_sampler );
void SetTexture(HContext context, HTexture texture, uint8 index );
void SetTextureAddress(HContext context, uint8 index, ETextureAddress wrapu, ETextureAddress wrapv, ETextureAddress wrapw, ETextureRemap remap, ETextureZFunc zfunc, ETextureGamma gamma);
void SetTextureFilter(HContext context, uint8 index, float bias, ETextureFilter min_filter, ETextureFilter mag_filter, ETextureFilter conv);
void SetTextureControl(HContext context, uint8 index, uint32 enable, float minLod, float maxLod, uint8 maxAniso);
void SetTextureBorderColor(HContext context, uint8 index, uint32 border_color);
void SetColourSpace(HTexture texture, EColourSpace colourspace);
EColourSpace GetColourSpace(HTexture texture);


void SetVertexTexture(HContext context, HTexture texture, uint8 index);
void SetVertexTextureAddress(HContext context, uint8 index, ETextureAddress wraps, ETextureAddress wrapt);
void SetVertexTextureFilter(HContext context, uint8 index, float bias);
void SetVertexTextureControl(HContext context, uint8 index, uint32 enable, float minLod, float maxLod);
void SetVertexTextureBorderColor(HContext context, uint8 index, uint32 border_color);



void PushMarker(HContext context, const char* s);
void PopMarker(HContext context);
typedef void(*WarningCallback)( uint32, char * );
#endif



#endif	// __GRAPHICSDEVICE_H__
