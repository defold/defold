#include <stdint.h>
#include <dlib/array.h>
#include <dlib/index_pool.h>
#include <dlib/endian.h>
#include <dlib/math.h>
#include <dlib/log.h>
#include <dlib/profile.h>

#include "sound_codec.h"
#include "sound_decoder.h"

namespace dmSoundCodec
{
    struct Decoder
    {
        int m_Index;
        HDecodeStream m_Stream;
        const DecoderInfo* m_DecoderInfo;

        void Clear()
        {
            memset(this, 0x00, sizeof(*this));
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
        uint32_t n = context->m_DecodersPool.Remaining() - context->m_DecodersPool.Capacity();
        if (n > 0) {
            dmLogError("Dangling decoders in codec context (%d)", n);
        }
        delete context;
    }

    Result NewDecoder(HCodecContext context, Format format, const void* buffer, uint32_t buffer_size, HDecoder* decoder)
    {
        if (context->m_DecodersPool.Remaining() == 0) {
            return RESULT_OUT_OF_RESOURCES;
        }

        const DecoderInfo* decoderImpl = FindBestDecoder(format);
        if (!decoderImpl) {
            return RESULT_UNSUPPORTED;
        }

        uint16_t index = context->m_DecodersPool.Pop();
        Decoder* d = &context->m_Decoders[index];
        d->m_Index = index;
        d->m_DecoderInfo = decoderImpl;

        Result r = decoderImpl->m_OpenStream(buffer, buffer_size, &d->m_Stream);
        if (r != RESULT_OK) {
            context->m_DecodersPool.Push(index);
            return r;
        }

        *decoder = d;
        return RESULT_OK;
    }

    void GetInfo(HCodecContext context, HDecoder decoder, Info* info)
    {
        assert(decoder);
        decoder->m_DecoderInfo->m_GetStreamInfo(decoder->m_Stream, info);
    }

    Result Decode(HCodecContext context, HDecoder decoder, char* buffer, uint32_t buffer_size, uint32_t* decoded)
    {
        DM_PROFILE(Sound, "Decode");
        assert(decoder);
        return decoder->m_DecoderInfo->m_DecodeStream(decoder->m_Stream, buffer, buffer_size, decoded);
    }

    Result Skip(HCodecContext context, HDecoder decoder, uint32_t bytes, uint32_t* skipped)
    {
        assert(context);
        assert(decoder);
        return decoder->m_DecoderInfo->m_SkipInStream(decoder->m_Stream, bytes, skipped);
    }

    Result Reset(HCodecContext context, HDecoder decoder)
    {
        assert(decoder);
        return decoder->m_DecoderInfo->m_ResetStream(decoder->m_Stream);
    }

    void DeleteDecoder(HCodecContext context, HDecoder decoder)
    {
        assert(decoder);
        decoder->m_DecoderInfo->m_CloseStream(decoder->m_Stream);
        context->m_DecodersPool.Push(decoder->m_Index);
        decoder->Clear();
    }
}
