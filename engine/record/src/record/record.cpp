// Skip the content of the file vpx_integer.h
// We got redefinitions of standard integers types
#define VPX_INTEGER_H
#include <stdint.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "record.h"
#include <vpx/vpx_encoder.h>
#include <vpx/vp8cx.h>
#include <dlib/log.h>

namespace dmRecord
{
    struct Recorder
    {
        Recorder(const NewParams* params)
        {
            memset(this, 0, sizeof(*this));
            m_Width = params->m_Width;
            m_Height = params->m_Height;
            m_Fps = params->m_Fps;
            m_Filename = strdup(params->m_Filename);
        }

        ~Recorder()
        {
            free(m_Filename);
            if (m_File)
            {
                fclose(m_File);
            }
        }

        uint32_t            m_Width;
        uint32_t            m_Height;
        uint32_t            m_Fps;
        char*               m_Filename;
        FILE*               m_File;
        vpx_codec_ctx_t     m_Codec;
        vpx_image_t         m_VpxImage;
        uint32_t            m_FrameCount;
    };

    static void MemPutLE16(char *mem, unsigned int val)
    {
        mem[0] = val;
        mem[1] = val >> 8;
    }

    static void MemPutLE32(char *mem, unsigned int val)
    {
        mem[0] = val;
        mem[1] = val >> 8;
        mem[2] = val >> 16;
        mem[3] = val >> 24;
    }

    const uint32_t IVF_HEADER_SIZE = 32;
    const uint32_t FOURCC = 0x30385056;

    static bool WriteIvfFileHeader(Recorder* recorder)
    {
        char header[IVF_HEADER_SIZE];

        header[0] = 'D';
        header[1] = 'K';
        header[2] = 'I';
        header[3] = 'F';
        MemPutLE16(header+4,  0);                       /* version */
        MemPutLE16(header+6,  32);                      /* headersize */
        MemPutLE32(header+8,  FOURCC);                  /* fourcc */
        MemPutLE16(header+12, recorder->m_Width);       /* width */
        MemPutLE16(header+14, recorder->m_Height);      /* height */
        MemPutLE32(header+16, recorder->m_Fps);         /* rate */
        MemPutLE32(header+20, 1);                       /* scale */
        MemPutLE32(header+24, recorder->m_FrameCount);  /* length */
        MemPutLE32(header+28, 0);                       /* unused */

        return fwrite(header, 1, sizeof(header), recorder->m_File) == sizeof(header);
    }

    static bool WriteIvfFrameHeader(HRecorder recorder, const vpx_codec_cx_pkt_t *pkt)
    {
        char             header[12];
        vpx_codec_pts_t  pts;

        if(pkt->kind != VPX_CODEC_CX_FRAME_PKT)
            return true;

        pts = pkt->data.frame.pts;
        MemPutLE32(header, pkt->data.frame.sz);
        MemPutLE32(header+4, pts&0xFFFFFFFF);
        MemPutLE32(header+8, pts >> 32);

        return fwrite(header, 1, sizeof(header), recorder->m_File) == sizeof(header);
    }

    Result New(const NewParams* params, HRecorder* recorder)
    {
        *recorder = 0;

        if (params->m_ContainerFormat != CONTAINER_FORMAT_IVF)
        {
            return RESULT_INVAL_ERROR;
        }

        if (params->m_VideoCodec != VIDOE_CODEC_VP8)
        {
            return RESULT_INVAL_ERROR;
        }

        if (params->m_Filename == 0)
        {
            return RESULT_INVAL_ERROR;
        }
        else if (params->m_Width % 8 != 0 || params->m_Height % 8 != 0
                || params->m_Width == 0 || params->m_Height == 0)
        {
            return RESULT_INVAL_ERROR;
        }

        vpx_codec_enc_cfg_t cfg;
        vpx_image_t vpx_image;
        if(!vpx_img_alloc(&vpx_image, VPX_IMG_FMT_YV12, params->m_Width, params->m_Height, 1))
        {
            return RESULT_UNKNOWN_ERROR;
        }

        vpx_codec_err_t res = vpx_codec_enc_config_default(vpx_codec_vp8_cx(), &cfg, 0);
        if (res)
        {
            dmLogError("Faild to get default configuration (%s)", vpx_codec_err_to_string(res));
            vpx_img_free(&vpx_image);
            return RESULT_UNKNOWN_ERROR;
        }

        cfg.rc_target_bitrate = params->m_Width * params->m_Height * cfg.rc_target_bitrate / cfg.g_w / cfg.g_h;
        cfg.g_w = params->m_Width;
        cfg.g_h = params->m_Height;
        cfg.g_timebase.num = 1;
        cfg.g_timebase.den = params->m_Fps;

        vpx_codec_ctx_t codec;
        res = vpx_codec_enc_init(&codec, vpx_codec_vp8_cx(), &cfg, 0);
        if(res)
        {
            dmLogError("Unable to init codec (%s)", vpx_codec_err_to_string(res));
            vpx_img_free(&vpx_image);
            return RESULT_UNKNOWN_ERROR;
        }

        FILE* f = fopen(params->m_Filename, "wb");
        if (!f)
        {
            vpx_img_free(&vpx_image);
            vpx_codec_destroy(&codec);
            return RESULT_IO_ERROR;
        }

        fseek(f, IVF_HEADER_SIZE, SEEK_SET);

        Recorder* r = new Recorder(params);
        r->m_Codec = codec;
        r->m_VpxImage = vpx_image;
        r->m_File = f;
        *recorder = r;
        return RESULT_OK;
    }

    /*
     * TODO: This code needs to be optimized.
     * RGBA to YV12 with flipped y
     * Links:
     * http://groups.google.com/a/chromium.org/group/chromium-reviews/browse_thread/thread/720aafe35a78942a?pli=1
     */
    static void RGBAToYV12FlipY(const uint8_t *rgba, uint32_t width, uint32_t height, void *y_plane, void *u_plane, void *v_plane)
    {
        for (uint32_t iy = 0; iy < height; ++iy)
        {
            char* y_plane_row = (char*) y_plane;
            y_plane_row += (height - 1 - iy) * width;
            for (uint32_t ix = 0; ix < width; ++ix)
            {
                int i = (iy * width + ix) * 4;
                uint8_t B = rgba[i+0];
                uint8_t G = rgba[i+1];
                uint8_t R = rgba[i+2];
                float y = (float)( R*66 + G*129 + B*25 + 128 ) / 256 + 16;
                *y_plane_row = (char) y;
                ++y_plane_row;
            }
        }

        uint32_t half_height = height >> 1;
        uint32_t half_width = width >> 1;

        for (uint32_t iy = 0; iy < half_height; iy++)
        {
            uint32_t base_src = (iy*2) * width * 4;

            uint8_t *v_plane_row = (uint8_t*)v_plane;
            uint8_t *u_plane_row = (uint8_t*)u_plane;
            v_plane_row += (half_height - 1 - iy) * half_width;
            u_plane_row += (half_height - 1 - iy) * half_width;

            for (uint32_t xPixel = 0; xPixel < half_width; xPixel++ )
            {
                uint8_t B = rgba[base_src+0];
                uint8_t G = rgba[base_src+1];
                uint8_t R = rgba[base_src+2];

                float u = (float)( R*-38 + G*-74 + B*112 + 128 ) / 256 + 128;
                float v = (float)( R*112 + G*-94 + B*-18 + 128 ) / 256 + 128;

                *v_plane_row = (uint8_t) v;
                *u_plane_row = (uint8_t) u;
                ++u_plane_row;
                ++v_plane_row;

                base_src += 8;
            }
        }
    }

    Result Delete(HRecorder recorder)
    {
        Result result = RESULT_OK;

        fseek(recorder->m_File, 0, SEEK_SET);
        if (!WriteIvfFileHeader(recorder))
        {
            result = RESULT_IO_ERROR;
        }

        vpx_img_free(&recorder->m_VpxImage);
        vpx_codec_destroy(&recorder->m_Codec);

        delete recorder;
        return result;
    }

    Result RecordFrame(HRecorder recorder, const void* frame_buffer,
            uint32_t frame_buffer_size, BufferFormat format)
    {
        vpx_codec_iter_t iter = NULL;
        const vpx_codec_cx_pkt_t *pkt;
        vpx_codec_err_t res;
        int flags = 0;

        RGBAToYV12FlipY((const uint8_t*) frame_buffer, recorder->m_Width, recorder->m_Height, recorder->m_VpxImage.planes[0], recorder->m_VpxImage.planes[1], recorder->m_VpxImage.planes[2]);
        res = vpx_codec_encode(&recorder->m_Codec, &recorder->m_VpxImage, recorder->m_FrameCount, 1, flags, VPX_DL_REALTIME);
        if (res)
        {
            dmLogError("Failed to encode frame (%s)", vpx_codec_err_to_string(res))
            return RESULT_UNKNOWN_ERROR;
        }

        while ((pkt = vpx_codec_get_cx_data(&recorder->m_Codec, &iter)))
        {
            switch (pkt->kind)
            {
            case VPX_CODEC_CX_FRAME_PKT:
                if (!WriteIvfFrameHeader(recorder, pkt))
                {
                    return RESULT_IO_ERROR;
                }

                if (fwrite(pkt->data.frame.buf, 1, pkt->data.frame.sz,
                        recorder->m_File) != pkt->data.frame.sz)
                {
                    return RESULT_IO_ERROR;
                }
                break;
            default:
                break;
            }
        }
        recorder->m_FrameCount++;

        return RESULT_OK;
    }
}
