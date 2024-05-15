// Copyright 2020-2024 The Defold Foundation
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

#include <dlib/dstrings.h>

#include "texc/texc.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

dmTexc::PixelFormat GetPixelFormatFromChannels(int channels)
{
    switch(channels)
    {
        case 1: return dmTexc::PF_L8;
        case 2: return dmTexc::PF_L8A8;
        case 3: return dmTexc::PF_R8G8B8;
        case 4: return dmTexc::PF_R8G8B8A8;
        default:break;
    }
    return (dmTexc::PixelFormat) -1;
}

struct EncodeParams
{
    const char*              m_PathIn;
    const char*              m_PathOut;
    dmTexc::ColorSpace       m_ColorSpace;
    dmTexc::CompressionType  m_CompressionType;
    dmTexc::CompressionLevel m_CompressionLevel;
    uint8_t                  m_PremultiplyAlpha : 1;
    uint8_t                  m_FlipY            : 1;
    uint8_t                  m_FlipX            : 1;
    uint8_t                  m_MipMaps          : 1;
    uint8_t                  m_Verbose          : 1;
};

EncodeParams GetDefaultEncodeParams()
{
    EncodeParams params       = {};
    params.m_MipMaps          = 0;
    params.m_PremultiplyAlpha = 1;
    params.m_ColorSpace       = dmTexc::CS_SRGB;
    params.m_CompressionType  = dmTexc::CT_DEFAULT;
    params.m_CompressionLevel = dmTexc::CL_NORMAL;
    return params;
}

inline bool IsArg(const char* arg)
{
    size_t str_len = strlen(arg);
    return str_len > 2 && arg[0] == '-' && arg[1] == '-';
}

template <typename T>
struct ArgNameToTypeValue
{
    const char* m_Name;
    const T     m_TypeValue;
};

template <typename T>
T GetArgTypeValue(const char* arg, ArgNameToTypeValue<T>* args, T default_value)
{
    int i=0;
    while(args[i].m_Name)
    {
        if (dmStrCaseCmp(args[i].m_Name, arg) == 0)
        {
            return args[i].m_TypeValue;
        }
        i++;
    }

    return default_value;
}

template <typename T>
const char* GetArgTypeName(T from_value, ArgNameToTypeValue<T>* args)
{
    int i=0;
    while(args[i].m_Name)
    {
        if (args[i].m_TypeValue ==  from_value)
        {
            return args[i].m_Name;
        }
        i++;
    }

    return "<unknown>";
}

ArgNameToTypeValue<dmTexc::ColorSpace> g_cs_lut[] = {
    {"LRGB", dmTexc::CS_LRGB},
    {"SRGB", dmTexc::CS_SRGB},
    {0}
};

ArgNameToTypeValue<dmTexc::CompressionType> g_ct_lut[] = {
    {"DEFAULT",     dmTexc::CT_DEFAULT},
    {"WEBP",        dmTexc::CT_WEBP},
    {"WEBP_LOSSY",  dmTexc::CT_WEBP_LOSSY},
    {"BASIS_UASTC", dmTexc::CT_BASIS_UASTC},
    {"BASIS_ETC1S", dmTexc::CT_BASIS_ETC1S},
    {0}
};

ArgNameToTypeValue<dmTexc::CompressionLevel> g_cl_lut[] = {
    {"FAST",   dmTexc::CL_FAST},
    {"NORMAL", dmTexc::CL_NORMAL},
    {"HIGH",   dmTexc::CL_HIGH},
    {"BEST",   dmTexc::CL_BEST},
    {"ENUM",   dmTexc::CL_ENUM},
    {0}
};

void PrintEncodeParams(EncodeParams params)
{
    if (!params.m_Verbose)
        return;
#define TRUE_FALSE_LABEL(cond) (cond?"TRUE":"FALSE")
    printf("-----------------------------\n");
    printf("Input path        : %s\n", params.m_PathIn);
    printf("Output path       : %s\n", params.m_PathOut);
    printf("Color space       : %s\n", GetArgTypeName(params.m_ColorSpace, g_cs_lut));
    printf("Compression type  : %s\n", GetArgTypeName(params.m_CompressionType, g_ct_lut));
    printf("Compression level : %s\n", GetArgTypeName(params.m_CompressionLevel, g_cl_lut));
    printf("PreMultiplyAlpha  : %s\n", TRUE_FALSE_LABEL(params.m_PremultiplyAlpha));
    printf("Flip-x            : %s\n", TRUE_FALSE_LABEL(params.m_FlipX));
    printf("Flip-y            : %s\n", TRUE_FALSE_LABEL(params.m_FlipY));
    printf("Mipmaps           : %s\n", TRUE_FALSE_LABEL(params.m_MipMaps));
#undef TRUE_FALSE_LABEL
}

void GetEncodeParamsFromArgs(int argc, const char* argv[], EncodeParams& params)
{
    params.m_PathIn  = argc > 1 ? argv[1] : 0;
    params.m_PathOut = argc > 2 ? argv[2] : 0;

    for (int i = 3; i < argc; ++i)
    {
        if (IsArg(argv[i]))
        {
            const char* actual_arg    = argv[i]+2;
            #define CMP_ARG(name)      (dmStrCaseCmp(actual_arg, name) == 0)
            #define CMP_ARG_1_OP(name) (CMP_ARG(name) && (i+1) < argc)

            if (CMP_ARG("premultiply-alpha"))
                params.m_PremultiplyAlpha = 1;
            else if (CMP_ARG("flip-x"))
                params.m_FlipX = 1;
            else if (CMP_ARG("flip-y"))
                params.m_FlipY = 1;
            else if (CMP_ARG("verbose"))
                params.m_Verbose = 1;
            else if (CMP_ARG("mipmaps"))
                params.m_MipMaps = 1;
            else if (CMP_ARG_1_OP("color-space"))
                params.m_ColorSpace = GetArgTypeValue(argv[++i], g_cs_lut, params.m_ColorSpace);
            else if (CMP_ARG_1_OP("compression-type"))
                params.m_CompressionType = GetArgTypeValue(argv[++i], g_ct_lut, params.m_CompressionType);
            else if (CMP_ARG_1_OP("compression-level"))
                params.m_CompressionLevel = GetArgTypeValue(argv[++i], g_cl_lut, params.m_CompressionLevel);

            #undef CMP_ARG_1_OP
            #undef CMP_ARG
        }
    }
}

int DoEncode(EncodeParams params)
{
    int x,y,n;
    unsigned char *data = stbi_load(params.m_PathIn, &x, &y, &n, 0);

    dmTexc::HTexture tex = dmTexc::Create(params.m_PathIn, x, y,
        GetPixelFormatFromChannels(n), params.m_ColorSpace, params.m_CompressionType, data);

    if (params.m_PremultiplyAlpha && !dmTexc::PreMultiplyAlpha(tex))
    {
        printf("Unable to premultiply alpha\n");
        return -1;
    }

    if (params.m_FlipY && !dmTexc::Flip(tex, dmTexc::FLIP_AXIS_Y))
    {
        printf("Unable to flip Y\n");
        return -1;
    }

    if (params.m_FlipX && !dmTexc::Flip(tex, dmTexc::FLIP_AXIS_X))
    {
        printf("Unable to flip Y\n");
        return -1;
    }

    // Note: For basis, the mipmaps are actually created when we encode, this call just requests that we want mipmaps later
    if (params.m_MipMaps && !dmTexc::GenMipMaps(tex))
    {
        printf("Unable to generate mipmaps\n");
        return -1;
    }

    if (params.m_CompressionType != dmTexc::CT_DEFAULT &&
        !dmTexc::Encode(tex, dmTexc::PF_RGBA_BC3, params.m_ColorSpace,
            params.m_CompressionLevel, params.m_CompressionType, params.m_MipMaps, 4))
    {
        printf("Unable to encode texture data\n");
        return -1;
    }

    uint32_t out_data_size = dmTexc::GetTotalDataSize(tex);
    void* out_data         = malloc(out_data_size);
    dmTexc::GetData(tex, out_data, out_data_size);

    dmTexc::Destroy(tex);

    stbi_image_free(data);

    FILE* f = fopen(params.m_PathOut, "w");

    fwrite(out_data, out_data_size, 1, f);
    fclose(f);
    return 0;
}

void ShowHelp()
{
#define PRINT_ARG_LIST(lst) \
    { \
        for(int i=0; lst[i].m_Name != 0; i++) \
            printf("    %s\n", lst[i].m_Name); \
    }
    printf("Usage: texconvert <input-file> <output-file> [options]\n");
    printf("Options:\n");
    printf("  --premultiply-alpha         : Use premultiply alpha\n");
    printf("  --flip-x                    : Flip image on X axis\n");
    printf("  --flip-y                    : Flip image on Y axis\n");
    printf("  --verbose                   : Verbose output\n");
    printf("  --mipmaps                   : Generate mipmaps\n");
    printf("  --color-space <color-space> : Sets the color space, defaults to 'SRGB'. Supported values are:\n");
    PRINT_ARG_LIST(g_cs_lut);
    printf("  --compression-type <type>   : Sets the compression type, defaults to 'DEFAULT'. Supported values are:\n");
    PRINT_ARG_LIST(g_ct_lut);
    printf("  --compression-level <level> : Sets the compression level, defaults to 'NORMAL'. Supported values are:\n");
    PRINT_ARG_LIST(g_cl_lut);
#undef PRINT_ARG_LIST
}

int main(int argc, char const *argv[])
{
    EncodeParams params = GetDefaultEncodeParams();
    GetEncodeParamsFromArgs(argc, argv, params);

    if (!params.m_PathIn || !params.m_PathOut)
    {
        if (!params.m_PathIn)
        {
            printf("No input file found in arguments\n");
        }

        if (!params.m_PathOut)
        {
            printf("No output path found in arguments\n");
        }

        ShowHelp();
        return -1;
    }

    PrintEncodeParams(params);

    return DoEncode(params);
}
