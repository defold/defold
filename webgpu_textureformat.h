#pragma once

#include <stddef.h>
#include <stdio.h>

#if defined(DM_GRAPHICS_WEBGPU_WAGYU)
    #include <dmsdk/graphics/graphics_webgpu.h>
#elif defined(DM_GRAPHICS_WEBGPU)
    #include <webgpu/webgpu.h>
#elif defined(RIVE_WAGYU)
    #include <webgpu/webgpu_wagyu.h>
#else
    #error "Wrong header!"
#endif

struct WGPUFormatName
{
    WGPUTextureFormat format;
    const char* name;
};

static inline void PrintWGPUTextureFormats(void)
{
    static const WGPUFormatName formats[] = {
        { WGPUTextureFormat_Undefined, "WGPUTextureFormat_Undefined" },
        { WGPUTextureFormat_R8Unorm, "WGPUTextureFormat_R8Unorm" },
        { WGPUTextureFormat_R8Snorm, "WGPUTextureFormat_R8Snorm" },
        { WGPUTextureFormat_R8Uint, "WGPUTextureFormat_R8Uint" },
        { WGPUTextureFormat_R8Sint, "WGPUTextureFormat_R8Sint" },
#if !defined(DM_GRAPHICS_WEBGPU)
        { WGPUTextureFormat_R16Unorm, "WGPUTextureFormat_R16Unorm" },
        { WGPUTextureFormat_R16Snorm, "WGPUTextureFormat_R16Snorm" },
#endif
        { WGPUTextureFormat_R16Uint, "WGPUTextureFormat_R16Uint" },
        { WGPUTextureFormat_R16Sint, "WGPUTextureFormat_R16Sint" },
        { WGPUTextureFormat_R16Float, "WGPUTextureFormat_R16Float" },
        { WGPUTextureFormat_RG8Unorm, "WGPUTextureFormat_RG8Unorm" },
        { WGPUTextureFormat_RG8Snorm, "WGPUTextureFormat_RG8Snorm" },
        { WGPUTextureFormat_RG8Uint, "WGPUTextureFormat_RG8Uint" },
        { WGPUTextureFormat_RG8Sint, "WGPUTextureFormat_RG8Sint" },
        { WGPUTextureFormat_R32Float, "WGPUTextureFormat_R32Float" },
        { WGPUTextureFormat_R32Uint, "WGPUTextureFormat_R32Uint" },
        { WGPUTextureFormat_R32Sint, "WGPUTextureFormat_R32Sint" },
#if !defined(DM_GRAPHICS_WEBGPU)
        { WGPUTextureFormat_RG16Unorm, "WGPUTextureFormat_RG16Unorm" },
        { WGPUTextureFormat_RG16Snorm, "WGPUTextureFormat_RG16Snorm" },
#endif
        { WGPUTextureFormat_RG16Uint, "WGPUTextureFormat_RG16Uint" },
        { WGPUTextureFormat_RG16Sint, "WGPUTextureFormat_RG16Sint" },
        { WGPUTextureFormat_RG16Float, "WGPUTextureFormat_RG16Float" },
        { WGPUTextureFormat_RGBA8Unorm, "WGPUTextureFormat_RGBA8Unorm" },
        { WGPUTextureFormat_RGBA8UnormSrgb, "WGPUTextureFormat_RGBA8UnormSrgb" },
        { WGPUTextureFormat_RGBA8Snorm, "WGPUTextureFormat_RGBA8Snorm" },
        { WGPUTextureFormat_RGBA8Uint, "WGPUTextureFormat_RGBA8Uint" },
        { WGPUTextureFormat_RGBA8Sint, "WGPUTextureFormat_RGBA8Sint" },
        { WGPUTextureFormat_BGRA8Unorm, "WGPUTextureFormat_BGRA8Unorm" },
        { WGPUTextureFormat_BGRA8UnormSrgb, "WGPUTextureFormat_BGRA8UnormSrgb" },
        { WGPUTextureFormat_RGB10A2Uint, "WGPUTextureFormat_RGB10A2Uint" },
        { WGPUTextureFormat_RGB10A2Unorm, "WGPUTextureFormat_RGB10A2Unorm" },
        { WGPUTextureFormat_RG11B10Ufloat, "WGPUTextureFormat_RG11B10Ufloat" },
        { WGPUTextureFormat_RGB9E5Ufloat, "WGPUTextureFormat_RGB9E5Ufloat" },
        { WGPUTextureFormat_RG32Float, "WGPUTextureFormat_RG32Float" },
        { WGPUTextureFormat_RG32Uint, "WGPUTextureFormat_RG32Uint" },
        { WGPUTextureFormat_RG32Sint, "WGPUTextureFormat_RG32Sint" },
#if !defined(DM_GRAPHICS_WEBGPU)
        { WGPUTextureFormat_RGBA16Unorm, "WGPUTextureFormat_RGBA16Unorm" },
        { WGPUTextureFormat_RGBA16Snorm, "WGPUTextureFormat_RGBA16Snorm" },
#endif
        { WGPUTextureFormat_RGBA16Uint, "WGPUTextureFormat_RGBA16Uint" },
        { WGPUTextureFormat_RGBA16Sint, "WGPUTextureFormat_RGBA16Sint" },
        { WGPUTextureFormat_RGBA16Float, "WGPUTextureFormat_RGBA16Float" },
        { WGPUTextureFormat_RGBA32Float, "WGPUTextureFormat_RGBA32Float" },
        { WGPUTextureFormat_RGBA32Uint, "WGPUTextureFormat_RGBA32Uint" },
        { WGPUTextureFormat_RGBA32Sint, "WGPUTextureFormat_RGBA32Sint" },
        { WGPUTextureFormat_Stencil8, "WGPUTextureFormat_Stencil8" },
        { WGPUTextureFormat_Depth16Unorm, "WGPUTextureFormat_Depth16Unorm" },
        { WGPUTextureFormat_Depth24Plus, "WGPUTextureFormat_Depth24Plus" },
        { WGPUTextureFormat_Depth24PlusStencil8, "WGPUTextureFormat_Depth24PlusStencil8" },
        { WGPUTextureFormat_Depth32Float, "WGPUTextureFormat_Depth32Float" },
        { WGPUTextureFormat_Depth32FloatStencil8, "WGPUTextureFormat_Depth32FloatStencil8" },
        { WGPUTextureFormat_BC1RGBAUnorm, "WGPUTextureFormat_BC1RGBAUnorm" },
        { WGPUTextureFormat_BC1RGBAUnormSrgb, "WGPUTextureFormat_BC1RGBAUnormSrgb" },
        { WGPUTextureFormat_BC2RGBAUnorm, "WGPUTextureFormat_BC2RGBAUnorm" },
        { WGPUTextureFormat_BC2RGBAUnormSrgb, "WGPUTextureFormat_BC2RGBAUnormSrgb" },
        { WGPUTextureFormat_BC3RGBAUnorm, "WGPUTextureFormat_BC3RGBAUnorm" },
        { WGPUTextureFormat_BC3RGBAUnormSrgb, "WGPUTextureFormat_BC3RGBAUnormSrgb" },
        { WGPUTextureFormat_BC4RUnorm, "WGPUTextureFormat_BC4RUnorm" },
        { WGPUTextureFormat_BC4RSnorm, "WGPUTextureFormat_BC4RSnorm" },
        { WGPUTextureFormat_BC5RGUnorm, "WGPUTextureFormat_BC5RGUnorm" },
        { WGPUTextureFormat_BC5RGSnorm, "WGPUTextureFormat_BC5RGSnorm" },
        { WGPUTextureFormat_BC6HRGBUfloat, "WGPUTextureFormat_BC6HRGBUfloat" },
        { WGPUTextureFormat_BC6HRGBFloat, "WGPUTextureFormat_BC6HRGBFloat" },
        { WGPUTextureFormat_BC7RGBAUnorm, "WGPUTextureFormat_BC7RGBAUnorm" },
        { WGPUTextureFormat_BC7RGBAUnormSrgb, "WGPUTextureFormat_BC7RGBAUnormSrgb" },
        { WGPUTextureFormat_ETC2RGB8Unorm, "WGPUTextureFormat_ETC2RGB8Unorm" },
        { WGPUTextureFormat_ETC2RGB8UnormSrgb, "WGPUTextureFormat_ETC2RGB8UnormSrgb" },
        { WGPUTextureFormat_ETC2RGB8A1Unorm, "WGPUTextureFormat_ETC2RGB8A1Unorm" },
        { WGPUTextureFormat_ETC2RGB8A1UnormSrgb, "WGPUTextureFormat_ETC2RGB8A1UnormSrgb" },
        { WGPUTextureFormat_ETC2RGBA8Unorm, "WGPUTextureFormat_ETC2RGBA8Unorm" },
        { WGPUTextureFormat_ETC2RGBA8UnormSrgb, "WGPUTextureFormat_ETC2RGBA8UnormSrgb" },
        { WGPUTextureFormat_EACR11Unorm, "WGPUTextureFormat_EACR11Unorm" },
        { WGPUTextureFormat_EACR11Snorm, "WGPUTextureFormat_EACR11Snorm" },
        { WGPUTextureFormat_EACRG11Unorm, "WGPUTextureFormat_EACRG11Unorm" },
        { WGPUTextureFormat_EACRG11Snorm, "WGPUTextureFormat_EACRG11Snorm" },
        { WGPUTextureFormat_ASTC4x4Unorm, "WGPUTextureFormat_ASTC4x4Unorm" },
        { WGPUTextureFormat_ASTC4x4UnormSrgb, "WGPUTextureFormat_ASTC4x4UnormSrgb" },
        { WGPUTextureFormat_ASTC5x4Unorm, "WGPUTextureFormat_ASTC5x4Unorm" },
        { WGPUTextureFormat_ASTC5x4UnormSrgb, "WGPUTextureFormat_ASTC5x4UnormSrgb" },
        { WGPUTextureFormat_ASTC5x5Unorm, "WGPUTextureFormat_ASTC5x5Unorm" },
        { WGPUTextureFormat_ASTC5x5UnormSrgb, "WGPUTextureFormat_ASTC5x5UnormSrgb" },
        { WGPUTextureFormat_ASTC6x5Unorm, "WGPUTextureFormat_ASTC6x5Unorm" },
        { WGPUTextureFormat_ASTC6x5UnormSrgb, "WGPUTextureFormat_ASTC6x5UnormSrgb" },
        { WGPUTextureFormat_ASTC6x6Unorm, "WGPUTextureFormat_ASTC6x6Unorm" },
        { WGPUTextureFormat_ASTC6x6UnormSrgb, "WGPUTextureFormat_ASTC6x6UnormSrgb" },
        { WGPUTextureFormat_ASTC8x5Unorm, "WGPUTextureFormat_ASTC8x5Unorm" },
        { WGPUTextureFormat_ASTC8x5UnormSrgb, "WGPUTextureFormat_ASTC8x5UnormSrgb" },
        { WGPUTextureFormat_ASTC8x6Unorm, "WGPUTextureFormat_ASTC8x6Unorm" },
        { WGPUTextureFormat_ASTC8x6UnormSrgb, "WGPUTextureFormat_ASTC8x6UnormSrgb" },
        { WGPUTextureFormat_ASTC8x8Unorm, "WGPUTextureFormat_ASTC8x8Unorm" },
        { WGPUTextureFormat_ASTC8x8UnormSrgb, "WGPUTextureFormat_ASTC8x8UnormSrgb" },
        { WGPUTextureFormat_ASTC10x5Unorm, "WGPUTextureFormat_ASTC10x5Unorm" },
        { WGPUTextureFormat_ASTC10x5UnormSrgb, "WGPUTextureFormat_ASTC10x5UnormSrgb" },
        { WGPUTextureFormat_ASTC10x6Unorm, "WGPUTextureFormat_ASTC10x6Unorm" },
        { WGPUTextureFormat_ASTC10x6UnormSrgb, "WGPUTextureFormat_ASTC10x6UnormSrgb" },
        { WGPUTextureFormat_ASTC10x8Unorm, "WGPUTextureFormat_ASTC10x8Unorm" },
        { WGPUTextureFormat_ASTC10x8UnormSrgb, "WGPUTextureFormat_ASTC10x8UnormSrgb" },
        { WGPUTextureFormat_ASTC10x10Unorm, "WGPUTextureFormat_ASTC10x10Unorm" },
        { WGPUTextureFormat_ASTC10x10UnormSrgb, "WGPUTextureFormat_ASTC10x10UnormSrgb" },
        { WGPUTextureFormat_ASTC12x10Unorm, "WGPUTextureFormat_ASTC12x10Unorm" },
        { WGPUTextureFormat_ASTC12x10UnormSrgb, "WGPUTextureFormat_ASTC12x10UnormSrgb" },
        { WGPUTextureFormat_ASTC12x12Unorm, "WGPUTextureFormat_ASTC12x12Unorm" },
        { WGPUTextureFormat_ASTC12x12UnormSrgb, "WGPUTextureFormat_ASTC12x12UnormSrgb" },
        { WGPUTextureFormat_Force32, "WGPUTextureFormat_Force32" }
    };

#if defined(DM_PLATFORM_32BIT)
    const char* library = "DEFOLD_ENGINE";
#elif defined(DM_RIVE_EXTENSION)
    const char* library = "EXTENSION_RIVE";
#elif defined(RIVE_RUNTIME)
    const char* library = "RIVE_RUNTIME";
#else
    #error "No library version defined"
#endif

    int print_version = 2;
    printf("WGPUTextureFormat CHECK (v %d): %s\n", print_version, library);

    const size_t count = sizeof(formats) / sizeof(formats[0]);
    size_t i;
    for (i = 0; i < count; ++i)
    {
        printf("%s = 0x%08X\n", formats[i].name, (unsigned int)formats[i].format);
    }
}
