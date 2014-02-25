#include <stdint.h>
#include <dlib/array.h>
#include <dlib/index_pool.h>
#include <dlib/endian.h>
#include <dlib/math.h>
#include <dlib/log.h>
#include <dlib/profile.h>

#include "stb_vorbis/stb_vorbis.h"

#include "sound_codec.h"

#if DM_ENDIAN == DM_ENDIAN_LITTLE
#define FOUR_CC(a,b,c,d) (((uint32_t)d << 24) | ((uint32_t)c << 16) | ((uint32_t)b << 8) | ((uint32_t)a))
#else
#define FOUR_CC(a,b,c,d) (((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) | ((uint32_t)d))
#endif

namespace dmSoundCodec
{

#if DM_ENDIAN == DM_ENDIAN_LITTLE
    static uint16_t Swap16(uint16_t x)
    {
        return x;
    }

    static uint32_t Swap32(uint32_t x)
    {
        return x;
    }
#else
    static uint16_t Swap16(uint16_t x)
    {
        return ((x & 0xff00) >> 8) |
               ((x & 0x00ff) << 8);
    }

    static uint32_t Swap32(uint32_t x)
    {
        return ((x & 0xff000000) >> 24) |
               ((x & 0x00ff0000) >>  8) |
               ((x & 0x0000ff00) <<  8) |
               ((x & 0x000000ff) << 24);
    }
#endif

    struct Decoder
    {
        Format      m_Format;
        void*       m_Buffer;
        uint32_t    m_Cursor;
        Info        m_Info;
        stb_vorbis* m_StbVorbis;
        uint16_t    m_Index;

        void Clear()
        {
            memset(this, 0, sizeof(*this));
            m_Index = 0xffff;
        }
    };

    struct CommonHeader
    {
        uint32_t m_ChunkID;
        uint32_t m_ChunkSize;

        void SwapHeader() {
            m_ChunkID = Swap32(m_ChunkID);
            m_ChunkSize = Swap32(m_ChunkSize);
        }
    };

    struct RiffHeader : CommonHeader
    {
        uint32_t m_Format;
        void Swap()
        {
            SwapHeader();
            m_Format = Swap32(m_Format);
        }
    };

    struct FmtChunk : CommonHeader
    {
        uint16_t m_AudioFormat;
        uint16_t m_NumChannels;
        uint32_t m_SampleRate;
        uint32_t m_ByteRate;
        uint16_t m_BlockAlign;
        uint16_t m_BitsPerSample;

        void Swap()
        {
            SwapHeader();
            m_AudioFormat = Swap16(m_AudioFormat);
            m_NumChannels = Swap16(m_NumChannels);
            m_SampleRate = Swap32(m_SampleRate);
            m_ByteRate = Swap32(m_ByteRate);
            m_BlockAlign = Swap16(m_BlockAlign);
            m_BitsPerSample = Swap16(m_BitsPerSample);
        }

    };

    struct DataChunk : CommonHeader
    {
        char m_Data[0];
        void Swap() {
            SwapHeader();
        }
    };

    struct CodecContext
    {
        dmArray<Decoder> m_Decoders;
        dmIndexPool16    m_DecodersPool;
    };

    HCodecContext New(const NewCodecContextParams* params)
    {
        CodecContext* c = new CodecContext;
        c->m_Decoders.SetCapacity(params->m_MaxDecoders);
        c->m_Decoders.SetSize(params->m_MaxDecoders);
        for (uint32_t i = 0; i < params->m_MaxDecoders; ++i) {
            c->m_Decoders[i].Clear();
        }
        c->m_DecodersPool.SetCapacity(params->m_MaxDecoders);
        return c;
    }

    void Delete(HCodecContext context)
    {
        delete context;
    }

    static Result OpenWAV(const void* buffer, uint32_t buffer_size, Decoder* decoder)
    {
        RiffHeader* header = (RiffHeader*) buffer;

        bool fmt_found = false;
        bool data_found = false;

        if (header->m_ChunkID == FOUR_CC('R', 'I', 'F', 'F') &&
            header->m_Format == FOUR_CC('W', 'A', 'V', 'E')) {

            const char* current = (const char*) buffer;
            const char* end = (const char*) buffer + buffer_size;
            current += sizeof(RiffHeader);
            do {
                CommonHeader header;
                memcpy(&header, current, sizeof(header));
                header.SwapHeader();
                if (header.m_ChunkID == FOUR_CC('f', 'm', 't', ' ')) {
                    FmtChunk fmt;
                    memcpy(&fmt, current, sizeof(fmt));
                    fmt.Swap();
                    fmt_found = true;

                    if (fmt.m_AudioFormat != 1) {
                        dmLogWarning("Only wav-files with PCM format supported");
                        return RESULT_INVALID_FORMAT;
                    }

                    decoder->m_Info.m_Rate = fmt.m_SampleRate;
                    decoder->m_Info.m_Channels = fmt.m_NumChannels;
                    decoder->m_Info.m_BitsPerSample = fmt.m_BitsPerSample;

                } else if (header.m_ChunkID == FOUR_CC('d', 'a', 't', 'a')) {
                    // NOTE: We don't byte-swap PCM-data and a potential problem on big-endian architectures
                    DataChunk data;
                    memcpy(&data, current, sizeof(data));
                    data.Swap();
                    decoder->m_Buffer = (void*) (current + sizeof(DataChunk));
                    decoder->m_Info.m_Size = data.m_ChunkSize;
                    data_found = true;
                }
                current += header.m_ChunkSize + sizeof(CommonHeader);
            } while (current < end);

            if (fmt_found && data_found) {
                return RESULT_OK;
            } else {
                return RESULT_INVALID_FORMAT;
            }
        } else {
            return RESULT_INVALID_FORMAT;
        }
    }

    static Result OpenVorbis(const void* buffer, uint32_t buffer_size, Decoder* decoder)
    {
        int error;
        stb_vorbis* vorbis = stb_vorbis_open_memory((unsigned char*) buffer, buffer_size, &error, NULL);

        if (vorbis) {
            stb_vorbis_info info = stb_vorbis_get_info(vorbis);

            decoder->m_Info.m_Rate = info.sample_rate;
            decoder->m_Info.m_Size = 0;
            decoder->m_Info.m_Channels = info.channels;
            decoder->m_Info.m_BitsPerSample = 16;
            decoder->m_StbVorbis = vorbis;

            return RESULT_OK;
        } else {
            return RESULT_INVALID_FORMAT;
        }
    }

    Result NewDecoder(HCodecContext context, Format format, const void* buffer, uint32_t buffer_size, HDecoder* decoder)
    {
        if (context->m_DecodersPool.Remaining() == 0) {
            return RESULT_OUT_OF_RESOURCES;
        }

        uint16_t index = context->m_DecodersPool.Pop();
        Decoder* d = &context->m_Decoders[index];
        d->m_Cursor = 0;
        d->m_Format = format;
        d->m_Index = index;

        Result r = RESULT_UNKNOWN_ERROR;

        if (format == FORMAT_WAV) {
            r = OpenWAV(buffer, buffer_size, d);
        } else if (format == FORMAT_VORBIS) {
            r = OpenVorbis(buffer, buffer_size, d);
        } else {
            assert(0);
        }

        if (r != RESULT_OK) {
            context->m_DecodersPool.Push(index);
        } else {
            *decoder = d;
        }

        return r;
    }

    Result Decode(HCodecContext context, HDecoder decoder, char* buffer, uint32_t buffer_size, uint32_t* decoded)
    {
        if (decoder->m_Format == FORMAT_WAV) {
            DM_PROFILE(SoundCodec, "Wav")

            assert(decoder->m_Cursor <= decoder->m_Info.m_Size);
            uint32_t n = dmMath::Min(buffer_size, decoder->m_Info.m_Size - decoder->m_Cursor);
            *decoded = n;
            memcpy(buffer, (const char*) decoder->m_Buffer + decoder->m_Cursor, n);
            decoder->m_Cursor += n;
            return RESULT_OK;
        } else if (decoder->m_Format == FORMAT_VORBIS) {
            DM_PROFILE(SoundCodec, "Vorbis")

            int ret = 0;

            if (decoder->m_Info.m_Channels == 1) {
                ret = stb_vorbis_get_samples_short_interleaved(decoder->m_StbVorbis, 1, (short*) buffer, buffer_size / 2);
            } else if (decoder->m_Info.m_Channels == 2) {
                ret = stb_vorbis_get_samples_short_interleaved(decoder->m_StbVorbis, 2, (short*) buffer, buffer_size / 2);
            } else {
                assert(0);
            }

            if (ret < 0) {
                return RESULT_DECODE_ERROR;
            } else {
                if (decoder->m_Info.m_Channels == 1) {
                    *decoded = ret * 2;
                } else if (decoder->m_Info.m_Channels == 2) {
                    *decoded = ret * 4;
                } else {
                    assert(0);
                }
            }
            return RESULT_OK;
        } else {
            assert(0);
        }
        // Never reached
        return RESULT_UNKNOWN_ERROR;
    }

    Result Reset(HCodecContext context, HDecoder decoder)
    {
        if (decoder->m_Format == FORMAT_WAV) {
            decoder->m_Cursor = 0;
        } else if (decoder->m_Format == FORMAT_VORBIS){
            stb_vorbis_seek_start(decoder->m_StbVorbis);
        } else {
            assert(0);
        }
        return RESULT_OK;
    }

    void DeleteDecoder(HCodecContext context, HDecoder decoder)
    {
        context->m_DecodersPool.Push(decoder->m_Index);
        if (decoder->m_Format == FORMAT_VORBIS) {
            stb_vorbis_close(decoder->m_StbVorbis);
        }
        decoder->Clear();
    }

    void GetInfo(HCodecContext context, HDecoder decoder, Info* info)
    {
        *info = decoder->m_Info;
    }

}

