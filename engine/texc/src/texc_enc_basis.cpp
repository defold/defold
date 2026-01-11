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


#include <dlib/log.h>

#include "texc.h"
#include "texc_private.h"

#include <basis/encoder/basisu_enc.h>
#include <basis/encoder/basisu_comp.h>
#include <basis/encoder/basisu_frontend.h>

namespace dmTexc
{
    // TODO: Clean this up. There is no need for this struct anymore, nor CreateBasis() DestroyBasis()
    struct TextureImpl
    {
        dmArray<Buffer> m_Mips;
        const char*     m_Name; // For easier debugging
        PixelFormat m_PixelFormat;
        ColorSpace m_ColorSpace;
        CompressionType m_CompressionType;
        uint32_t m_Width;
        uint32_t m_Height;
        uint64_t m_CompressionFlags;

        //Encoder m_Encoder;

        // Used with CT_BASIS_xx encoding
        basisu::image m_BasisImage; // Original image
        dmArray<uint8_t> m_BasisFile;
        bool m_BasisGenMipmaps;
    };


    static void InitBasisU()
    {
        static int first = 1;
        if (first)
        {
            basisu::basisu_encoder_init();
            first = 0;
        }
    }

    Image* ResizeBasis(Image* image, uint32_t width, uint32_t height)
    {
        InitBasisU();

        int components = 4;
        basisu::image orig;
        orig.init(image->m_Data, image->m_Width, image->m_Height, components);

        basisu::image tmp(width, height);
        basisu::image_resample(orig, tmp);

        Image* out = new Image;
        out->m_Width = width;
        out->m_Height = height;
        out->m_DataCount = width * height * components;
        out->m_Data = (uint8_t*)malloc(out->m_DataCount);
        memcpy(out->m_Data, tmp.get_ptr(), out->m_DataCount);
        return out;
    }

    static bool BasisUEncodeInternal(int num_threads, basisu::basis_compressor_params& comp_params, PixelFormat pixel_format, dmArray<uint8_t>& out, int debug)
    {
        basisu::enable_debug_printf(debug);

        basisu::job_pool jpool(num_threads);

        comp_params.m_pJob_pool = &jpool;
        comp_params.m_multithreading = num_threads > 1;

        comp_params.m_read_source_images = false;
        comp_params.m_write_output_basis_files = false;
        comp_params.m_status_output = debug;
        comp_params.m_debug = debug;

        basisu::basis_compressor compressor;
        if (!compressor.init(comp_params))
        {
            dmLogError("basis_compressor::init() failed!\n");
            return false;
        }

        basisu::interval_timer tm;
        tm.start();

        basisu::basis_compressor::error_code ec = compressor.process();

        tm.stop();

        if (ec == basisu::basis_compressor::cECSuccess)
        {
            dmLogDebug("Compression succeeded in %3.3f secs\n", tm.get_elapsed_secs());
        }
        else
        {
            switch (ec)
            {
                case basisu::basis_compressor::cECFailedReadingSourceImages: dmLogError("Compressor failed reading a source image!\n"); break;
                case basisu::basis_compressor::cECFailedValidating: dmLogError("Compressor failed 2darray/cubemap/video validation checks!\n"); break;
                case basisu::basis_compressor::cECFailedEncodeUASTC: dmLogError("Compressor UASTC encode failed!\n"); break;
                case basisu::basis_compressor::cECFailedFrontEnd: dmLogError("Compressor frontend stage failed!\n"); break;
                case basisu::basis_compressor::cECFailedFontendExtract: dmLogError("Compressor frontend data extraction failed!\n"); break;
                case basisu::basis_compressor::cECFailedBackend: dmLogError("Compressor backend stage failed!\n"); break;
                case basisu::basis_compressor::cECFailedCreateBasisFile: dmLogError("Compressor failed creating Basis file data!\n"); break;
                case basisu::basis_compressor::cECFailedWritingOutput: dmLogError("Compressor failed writing to output Basis file!\n"); break;
                case basisu::basis_compressor::cECFailedUASTCRDOPostProcess: dmLogError("Compressor failed during the UASTC post process step!\n"); break;
                default: dmLogError("basis_compress::process() failed!\n"); break;
            }

            return false;
        }

        //const std::vector<basisu::image_stats>& stats = compressor.get_stats();
        //double bits_per_texel = compressor.get_basis_bits_per_texel();
        // Statistics:

        // basisu::image_metrics im;
        // im.calc(a, b, 0, 3);
        // im.print("RGB    ");

        // im.calc(a, b, 0, 4);
        // im.print("RGBA   ");

        // im.calc(a, b, 0, 1);
        // im.print("R      ");

        // im.calc(a, b, 1, 1);
        // im.print("G      ");

        // im.calc(a, b, 2, 1);
        // im.print("B      ");

        // im.calc(a, b, 3, 1);
        // im.print("A      ");

        // OUTPUT
        //const uint8_vec& comp_data = m_basis_file.get_compressed_data();

        const basisu::uint8_vec& data = compressor.get_output_basis_file();
        out.SetCapacity(data.size());
        out.SetSize(data.size());
        memcpy(out.Begin(), &data[0], data.size());

        return true;
    }

    static bool CreateBasis(TextureImpl* texture, uint32_t width, uint32_t height, PixelFormat pixel_format, ColorSpace color_space, CompressionType compression_type, void* data)
    {
        InitBasisU();

        uint32_t size = width * height * 4;
        uint8_t* base_image = new uint8_t[size];
        if (!ConvertToRGBA8888((uint8_t*)data, width, height, pixel_format, base_image))
        {
            delete[] base_image;
            return false;
        }

        int components = 4;
        texture->m_CompressionFlags = 0;
        texture->m_PixelFormat = pixel_format;
        texture->m_ColorSpace = color_space;
        texture->m_Width = width;
        texture->m_Height = height;
        texture->m_BasisGenMipmaps = false;
        texture->m_BasisImage.init((uint8_t*)base_image, width, height, components);
        delete[] base_image;
        return true;
    }

    static void DestroyBasis(TextureImpl* texture)
    {
        (void)texture;
    }

    bool BasisUEncode(BasisUEncodeSettings* input, uint8_t** out, uint32_t* out_size)
    {
        InitBasisU();

        // During refactoring, we'll reuse the old code
        TextureImpl impl;
        impl.m_CompressionType = CT_BASIS_UASTC;
        bool res = CreateBasis(&impl, input->m_Width, input->m_Height, input->m_PixelFormat, input->m_ColorSpace, impl.m_CompressionType, input->m_Data);
        if (!res)
        {
            dmLogError("Failed to encode Basis texture: %s\n", input->m_Path);
            return false;
        }

        basisu::basis_compressor_params comp_params;

        comp_params.m_mip_gen = 0;
        comp_params.m_pack_uastc_flags = input->m_pack_uastc_flags;
        comp_params.m_uastc = 1;
        comp_params.m_rdo_uastc = input->m_rdo_uastc;
        if (comp_params.m_rdo_uastc)
        {
            comp_params.m_rdo_uastc_quality_scalar = input->m_rdo_uastc_quality_scalar;
            comp_params.m_rdo_uastc_dict_size = input->m_rdo_uastc_dict_size;
        }

        comp_params.m_source_images.push_back(impl.m_BasisImage);

        dmArray<uint8_t> arr;
        res = BasisUEncodeInternal(input->m_NumThreads, comp_params, input->m_OutPixelFormat, arr, input->m_Debug);

        // TODO: Avoid extra copies!
        uint32_t size = arr.Size();
        uint8_t* compressed = (uint8_t*)malloc(size);
        memcpy(compressed, arr.Begin(), size);

        *out = compressed;
        *out_size = size;

        DestroyBasis(&impl);

        return true;
    }
}
