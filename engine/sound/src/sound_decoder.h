#ifndef DM_SOUND_DECODER_H
#define DM_SOUND_DECODER_H

#include "sound_codec.h"
#include "sound_decoder.h"
#include "sound.h"

namespace dmSoundCodec
{
    typedef void* HDecodeStream;

    enum DecoderFlags
    {
        DECODER_FLOATING_POINT = 1,
        DECODER_FIXED_POINT    = 2
    };

    struct DecoderInfo
    {
        /**
         * Implementation name
         */
        const char* m_Name;

        /**
         * Format the decoder handles
         */
        Format m_Format;

        /**
         * Performance score. Recommended range 0-10
         */
        int m_Score;

        /**
         * Open a stream for decoding
         */
        Result (*m_OpenStream)(const void* buffer, const uint32_t size, HDecodeStream* out);

        /**
         * Close and free decoding resources
         */
        void (*m_CloseStream)(HDecodeStream);

        /**
         * Fetch a chunk of PCM data from the decoder. The buffer will be filled as long as there is
         * enough data left in the compressed stream.
         */
        Result (*m_DecodeStream)(HDecodeStream decoder, char* buffer, uint32_t buffer_size, uint32_t* decoded);

        /**
         * Seek to the beginning.
         */
        Result (*m_ResetStream)(HDecodeStream);

        /**
         * Skip in stream
         */
        Result (*m_SkipInStream)(HDecodeStream, uint32_t bytes, uint32_t* skipped);

        /**
         * Get samplerate and number of channels.
         */
        void (*m_GetStreamInfo)(HDecodeStream, struct Info* out);

        DecoderInfo *m_Next;
    };

    /**
     * Store a decoder in the internal registry. Will update m_Next in the supplied instance.
     */
    void RegisterDecoder(DecoderInfo* info);

    /**
     * Finds the best match for a stream among all registered decoders.
     */
    const DecoderInfo* FindBestDecoder(Format format);

    /**
     * Get by name of implementation
     */
    const DecoderInfo* FindDecoderByName(const char *name);

    #ifdef __GNUC__
        // Workaround for dead-stripping on OSX/iOS. The symbol "name" is explicitly exported. See wscript "exported_symbols"
        // Otherwise it's dead-stripped even though -no_dead_strip_inits_and_terms is passed to the linker
        // The bug only happens when the symbol is in a static library though
        #define DM_REGISTER_SOUND_DECODER(name, desc) extern "C" void __attribute__((constructor)) name () { \
            dmSoundCodec::RegisterDecoder(&desc); \
        }
    #else
        #define DM_REGISTER_SOUND_DECODER(name, desc) extern "C" void name () { \
            dmSoundCodec::RegisterDecoder(&desc); \
            }\
            int name ## Wrapper(void) { name(); return 0; } \
            __pragma(section(".CRT$XCU",read)) \
            __declspec(allocate(".CRT$XCU")) int (* _Fp ## name)(void) = name ## Wrapper;
    #endif

    #ifndef DM_SOUND_PASTE
    #define DM_SOUND_PASTE(x, y) x ## y
    #define DM_SOUND_PASTE2(x, y) DM_SOUND_PASTE(x, y)
    #endif

    /**
     * Declare a new stream decoder
     */
    #define DM_DECLARE_SOUND_DECODER(symbol, name, format, score, open, close, decode, reset, skip, getinfo) \
            dmSoundCodec::DecoderInfo DM_SOUND_PASTE2(symbol, __LINE__) = { \
                    name, \
                    format, \
                    score, \
                    open, \
                    close, \
                    decode, \
                    reset, \
                    skip, \
                    getinfo, \
            };\
        DM_REGISTER_SOUND_DECODER(symbol, DM_SOUND_PASTE2(symbol, __LINE__))
}

#endif
