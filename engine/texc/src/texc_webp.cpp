// Copyright 2020 The Defold Foundation
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


#include <assert.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <webp/encode.h>

#include "texc.h"
#include "texc_private.h"

namespace dmTexc
{
    static bool CompressWebPInternal(WebPConfig* config, uint32_t width, uint32_t height, uint32_t bits_per_pixel, uint8_t* srcbuffer, uint32_t srcsize, uint8_t** outbuffer, uint32_t* outsize, PixelFormat pixel_format, CompressionLevel compression_level, CompressionType compression_type)
    {
        // setup memorywriter method for compression
        WebPMemoryWriter memory_writer;
        WebPMemoryWriterInit(&memory_writer);

        int32_t config_error = WebPValidateConfig(config);
        if(config_error != 1)
        {
            dmLogError("WebPValidateConfig failed, error code %d. WebP compression failed.", config_error);
            return false;
        }

        // setup the input data, allocating a picture from the prvt texture source
        WebPPicture pic;
        if (!WebPPictureInit(&pic))
        {
            dmLogError("WebPPictureInit failed, version error");
            return false;
        }
        pic.use_argb = config->lossless;
        pic.width = width;
        pic.height = height;
        pic.writer = WebPMemoryWrite;
        pic.custom_ptr = &memory_writer;

        if(pixel_format == dmTexc::PF_L8)
        {
            uint8_t* lum = new uint8_t[pic.width*pic.height*3];
            L8ToRGB888((const uint8_t*) srcbuffer, pic.width, pic.height, lum);
            bool ok = WebPPictureImportRGB(&pic, (const uint8_t*) lum, pic.width*3);
            delete []lum;
            if(!ok)
            {
                dmLogError("WebPPictureImportRGB from L8 (%dbpp) failed, code %d.", bits_per_pixel, pic.error_code);
                return false;
            }
        }
        else if(pixel_format == dmTexc::PF_L8A8)
        {
            uint8_t* luma = new uint8_t[pic.width*pic.height*4];
            L8A8ToRGBA8888((const uint8_t*) srcbuffer, pic.width, pic.height, luma);
            bool ok = WebPPictureImportRGBA(&pic, (const uint8_t*) luma, pic.width*4);
            delete []luma;
            if(!ok)
            {
                dmLogError("WebPPictureImportRGB from L8A8 (%dbpp) failed, code %d.", bits_per_pixel, pic.error_code);
                return false;
            }
            if((compression_type == dmTexc::CT_WEBP_LOSSY) || (config->exact == 0))
            {
                WebPCleanupTransparentArea(&pic);
            }
        }
        else if(pixel_format == dmTexc::PF_R5G6B5)
        {
            uint8_t* rgb = new uint8_t[pic.width*pic.height*3];
            RGB565ToRGB888((const uint16_t*) srcbuffer, pic.width, pic.height, rgb);
            bool ok = WebPPictureImportRGB(&pic, (const uint8_t*) rgb, pic.width*3);
            delete []rgb;
            if(!ok)
            {
                dmLogError("WebPPictureImportRGB from RGB565 (%dbpp) failed, code %d.", bits_per_pixel, pic.error_code);
                return false;
            }
        }
        else if(pixel_format == dmTexc::PF_R4G4B4A4)
        {
            uint8_t* rgba = new uint8_t[pic.width*pic.height*4];
            RGBA4444ToRGBA8888((const uint16_t*) srcbuffer, pic.width, pic.height, rgba);
            bool ok = WebPPictureImportRGBA(&pic, (const uint8_t*) rgba, pic.width*4);
            delete []rgba;
            if(!ok)
            {
                dmLogError("WebPPictureImportRGBA from RGBA444 (%dbpp) failed, code %d.", bits_per_pixel, pic.error_code);
                return false;
            }
            if((compression_type == dmTexc::CT_WEBP_LOSSY) || (config->exact == 0))
            {
                WebPCleanupTransparentArea(&pic);
            }
        }
        else if(bits_per_pixel >= 24)
        {
            if(bits_per_pixel == 32)
            {
                if(!WebPPictureImportRGBA(&pic, (const uint8_t*) srcbuffer, width*4))
                {
                    dmLogError("WebPPictureImportRGBA failed, code %d.", pic.error_code);
                    return false;
                }
                if((compression_type == dmTexc::CT_WEBP_LOSSY) || (config->exact == 0))
                {
                    WebPCleanupTransparentArea(&pic);
                }
            }
            else if(bits_per_pixel == 24)
            {
                if(!WebPPictureImportRGB(&pic, (const uint8_t*) srcbuffer, width*(bits_per_pixel>>3)))
                {
                    dmLogError("WebPPictureImportRGB failed, code %d.", pic.error_code);
                    return false;
                }
            }
            else
            {
                dmLogError("WebP failed to import image with %dbpp, unsupported bit-depth.", bits_per_pixel);
                return false;
            }
        }
        else
        {
            // Check to be able to compress by 4-byte aligned stride (RGBA)
            if(width & 4)
            {
                dmLogError("WebP compression with %d bpp requires width to be a multiple of 32.", bits_per_pixel);
                return false;
            }

            // Require lossless compression
            if(compression_type == dmTexc::CT_WEBP_LOSSY)
            {
                dmLogError("WebP compression with %d bpp requires lossless compression.", bits_per_pixel);
                return false;
            }

            // Exact compression
            config->exact = 1;
            uint32_t stride = (width*bits_per_pixel) >> 3;
            bool ok = WebPPictureImportRGBA(&pic, (const uint8_t*) srcbuffer, stride);
            if(!ok)
            {
                dmLogError("WebPPictureImportRGBA (%dbpp) failed, code %d.", bits_per_pixel, pic.error_code);
                return false;
            }
        }

        // compress the input samples and free the memory associated when done
        int32_t res = WebPEncode(config, &pic);
        WebPPictureFree(&pic);
        if(res != 1)
        {
            dmLogError("WebPEncode failed, error code %d", res);
            return false;
        }

        // check compression isn't larger than source, if so stop compressing
        if(memory_writer.size >= srcsize)
        {
            WebPMemoryWriterClear(&memory_writer);
            *outsize = 0xFFFFFFFF;
            return false;
        }

        *outsize = memory_writer.size;
        *outbuffer = new uint8_t[memory_writer.size];
        memcpy(*outbuffer, memory_writer.mem, memory_writer.size);
        WebPMemoryWriterClear(&memory_writer);

        return true;
    }


    bool CompressWebP(HTexture texture, PixelFormat pixel_format, ColorSpace color_space, CompressionLevel compression_level, CompressionType compression_type)
    {
        return false;
    }


    HBuffer CompressWebPBuffer(uint32_t width, uint32_t height, uint32_t bits_per_pixel, void* data, uint32_t size, PixelFormat pixel_format, CompressionLevel compression_level, CompressionType compression_type)
    {
        WebPConfig config;
        if(compression_type == dmTexc::CT_WEBP)
        {
            uint32_t quality_lut[CL_ENUM] = {3,6,9,9};
            WebPConfigInit(&config);
            WebPConfigLosslessPreset(&config, quality_lut[compression_level]);
            config.exact = 1; // expect lossless to not alter alpha values
        }
        else
        {
            if(compression_level == CL_BEST)
            {
                WebPConfigInit(&config);
                WebPConfigLosslessPreset(&config, 9);
                config.near_lossless = 50;
            }
            else
            {
                float quality_lut[CL_ENUM] = {50,75,90};
                WebPConfigPreset(&config, WEBP_PRESET_DEFAULT, quality_lut[compression_level]);
            }
        }

        uint32_t outsize = 0;
        TextureData* out = new TextureData;

        if (!CompressWebPInternal(&config, width, height, bits_per_pixel, (uint8_t*)data, size, &out->m_Data, &outsize, pixel_format, compression_level, compression_type)) {
            if (outsize == 0xFFFFFFFF) {
                out->m_IsCompressed = 0;
                out->m_ByteSize = size;
                out->m_Data = (uint8_t*)malloc(size);
                memcpy(out->m_Data, data, size);
                return out;
            }
            return 0;
        }
        out->m_IsCompressed = 1;
        out->m_ByteSize = outsize;
        return out;
    }

    uint32_t GetTotalBufferDataSize(HBuffer _buffer)
    {
        TextureData* buffer = (TextureData*) _buffer;
        return (uint32_t)buffer->m_ByteSize + 1;
    }

    uint32_t GetBufferData(HBuffer _buffer, void* _out_data, uint32_t out_data_size)
    {
        TextureData* buffer = (TextureData*) _buffer;
        uint8_t* out_data = (uint8_t*)_out_data;
        out_data[0] = buffer->m_IsCompressed;
        memcpy(out_data+1, buffer->m_Data, dmMath::Min(out_data_size, (uint32_t)buffer->m_ByteSize));
        return dmMath::Min(out_data_size, (uint32_t)buffer->m_ByteSize + 1);
    }

    void DestroyBuffer(HBuffer buffer)
    {
        delete (TextureData*)buffer;
    }
}
