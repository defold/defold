
#include <assert.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <webp/encode.h>
#include <PVRTexture.h>

#include "texc.h"
#include "texc_private.h"

namespace dmTexc
{

    bool CompressWebP(HTexture texture, PixelFormat pixel_format, ColorSpace color_space, CompressionLevel compression_level, CompressionType compression_type)
    {
        Texture* t = (Texture*) texture;
        assert(t->m_CompressedMips.Empty());
        assert(compression_type == dmTexc::CT_WEBP || compression_type == dmTexc::CT_WEBP_LOSSY);
        t->m_CompressionFlags = 0;

        pvrtexture::CPVRTexture* pt = (pvrtexture::CPVRTexture*)t->m_PVRTexture;
        uint32_t mip_maps = t->m_PVRTexture->getNumMIPLevels();
        uint32_t mip_map = 0;
        uint32_t bpp = pt->getBitsPerPixel();

        // validate dimensions
        if((pt->getWidth() > WEBP_MAX_DIMENSION) || (pt->getHeight() > WEBP_MAX_DIMENSION))
        {
            dmLogError("WebP compression failed, max dimension (%d) exceeded. WebP compression failed.", WEBP_MAX_DIMENSION);
            return false;
        }

        // setup compressor config from options
        WebPConfig config;
        if(compression_type == dmTexc::CT_WEBP)
        {
            uint32_t quality_lut[CL_ENUM] = {2,4,6,6};
            WebPConfigInit(&config);
            WebPConfigLosslessPreset(&config, quality_lut[compression_level]);
            // Disabling exact mode will allow for the RGB values to be modified, which can give better results. This however have a run-time penalty in that it needs to be cleaned up after decoding
            // by setting RGB to zero (if not zero), when alpha is zero. Only affect alpha enabled formats.
            config.exact = compression_level == CL_BEST ? 0 : 1;
            if(bpp == 32)
            {
                t->m_CompressionFlags |= config.exact == 0 ? dmTexc::CF_ALPHA_CLEAN : 0;
            }
        }
        else
        {
            if(compression_level == CL_BEST)
            {
                WebPConfigInit(&config);
                WebPConfigLosslessPreset(&config, 6);
                config.exact = 1;
                config.near_lossless = 50;
            }
            else
            {
                float quality_lut[CL_ENUM] = {50,75,90};
                WebPConfigPreset(&config, WEBP_PRESET_DEFAULT, quality_lut[compression_level]);
            }
        }

        for(; mip_map < mip_maps; ++mip_map)
        {
            uint32_t mip_width = pt->getWidth(mip_map);
            uint32_t mip_height = pt->getHeight(mip_map);

            // check compression size threshold
            if((mip_width * mip_height) <= COMPRESSION_ENABLED_PIXELCOUNT_THRESHOLD)
            {
                mip_map = mip_maps;
                break;
            }

            // setup memorywriter method for compression
            WebPMemoryWriter memory_writer;
            WebPMemoryWriterInit(&memory_writer);

            int32_t config_error = WebPValidateConfig(&config);
            if(config_error != 1)
            {
                dmLogError("WebPValidateConfig failed, error code %d. WebP compression failed.", config_error);
                break;
            }

            // setup the input data, allocating a picture from the prvt texture source
            WebPPicture pic;
            if (!WebPPictureInit(&pic))
            {
                dmLogError("WebPPictureInit failed, version error");
                break;
            }
            pic.width = mip_width;
            pic.height = mip_height;
            pic.writer = WebPMemoryWrite;
            pic.custom_ptr = &memory_writer;
            uint8_t* data = (uint8_t*) pt->getDataPtr(mip_map);

            if(bpp > 8)
            {
                if(bpp == 32)
                {
                    if(!WebPPictureImportRGBA(&pic, (const uint8_t*) data, pt->getWidth(mip_map)*4))
                    {
                        dmLogError("WebPPictureImportRGBA failed to import with 32bpp, memory error.");
                        break;
                    }
                    if(compression_type == dmTexc::CT_WEBP_LOSSY)
                    {
                        WebPCleanupTransparentArea(&pic);
                    }
                }
                else if(bpp == 24)
                {
                    if(!WebPPictureImportRGB(&pic, (const uint8_t*) data, pt->getWidth(mip_map)*(bpp>>3)))
                    {
                        dmLogError("WebPPictureImportRGB failed to import with %dbpp, memory error.", bpp);
                        break;
                    }
                }
                else
                {
                    dmLogError("WebP failed to import image with %dbpp, unsupported bit-depth.", bpp);
                    break;
                }
            }
            else
            {
                uint32_t stride = mip_width/(8/bpp);
                uint32_t data_src_size = mip_height*stride;
                uint8_t* data_src_end = data + data_src_size;
                uint8_t* data_dst_ptr = new uint8_t[data_src_size*3];
                uint8_t* data_dst = data_dst_ptr;
                for(uint8_t* data_src = data; data_src != data_src_end; ++data_src)
                {
                    uint8_t p = *data_src;
                    *data_dst++ = p;
                    *data_dst++ = p;
                    *data_dst++ = p;
                }
                bool ok = WebPPictureImportRGB(&pic, (const uint8_t*) data_dst_ptr, stride*3);
                delete[] data_dst_ptr;
                if(!ok)
                {
                    dmLogError("WebPPictureImportRGB failed to import with width %d and %dbpp, memory error.", pt->getWidth(mip_map), bpp);
                    break;
                }
            }

            // compress the input samples and free the memory associated when done
            int32_t res = WebPEncode(&config, &pic);
            WebPPictureFree(&pic);
            if(res != 1)
            {
                dmLogError("WebPEncode failed, error code %d", res);
                break;
            }

            // check compression isn't larger than source, if so stop compressing
            if(memory_writer.size >= pt->getDataSize(mip_map))
            {
                WebPMemoryWriterClear(&memory_writer);
                mip_map = mip_maps;
                break;
            }
#if 0
            dmLogInfo("WebPEncode compressed %d -> %d", pt->getDataSize(mip_map), (uint32_t) memory_writer.size);
#endif
            CompressedTextureData ctd;
            ctd.m_ByteSize = memory_writer.size;
            ctd.m_Data = new uint8_t[memory_writer.size];
            memcpy(ctd.m_Data, memory_writer.mem, memory_writer.size);
            t->m_CompressedMips.Push(ctd);
            WebPMemoryWriterClear(&memory_writer);
        }

        // if we haven't got a complete mip-map chain, something went' wrong so free all used memory
        if(mip_map != mip_maps)
        {
            dmLogInfo("WebPEncode compression failed at mip index(%d)", mip_map);
            for(size_t i = 0; i < t->m_CompressedMips.Size(); ++i)
            {
                delete[] &t->m_CompressedMips[i].m_Data;
            }
            t->m_CompressedMips.SetSize(0);
            return false;
        }

        // compression success, free source picture
        return true;
    }

}
