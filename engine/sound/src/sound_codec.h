#ifndef DM_SOUND_CODEC_H
#define DM_SOUND_CODEC_H

/**
 * Sound decoding support
 */
namespace dmSoundCodec
{
    /// Codec context handle
    typedef struct CodecContext* HCodecContext;
    /// Decoder handle
    typedef struct Decoder* HDecoder;

    /**
     * Result codes
     */
    enum Result
    {
        RESULT_OK = 0,               //!< RESULT_OK
        RESULT_OUT_OF_RESOURCES = -1,//!< RESULT_OUT_OF_RESOURCES
        RESULT_INVALID_FORMAT = -2,  //!< RESULT_INVALID_FORMAT
        RESULT_DECODE_ERROR = -3,    //!< RESULT_DECODE_ERROR
        RESULT_UNSUPPORTED = -4,     //!< RESULT_UNSUPPORTED
        RESULT_UNKNOWN_ERROR = -1000,//!< RESULT_UNKNOWN_ERROR
    };

    /**
     * Codec format
     */
    enum Format
    {
        FORMAT_WAV,   //!< FORMAT_WAV
        FORMAT_VORBIS,//!< FORMAT_VORBIS
    };

    /**
     * Info
     */
    struct Info
    {
        /// Rate
        uint32_t m_Rate;
        /// Size in bytes for decompressed stream. Might be 0 for ogg streams and similar
        uint32_t m_Size;
        /// Number of channels
        uint8_t  m_Channels;
        /// Bites per sample
        uint8_t  m_BitsPerSample;
    };

    /**
     * Parameters for new codec context
     */
    struct NewCodecContextParams
    {
        /// Maximum number of decoders supported in context
        uint32_t m_MaxDecoders;

        NewCodecContextParams()
        {
            m_MaxDecoders = 32;
        }
    };

    /**
     * Create a new codec context
     * @param params params
     * @return context
     */
    HCodecContext New(const NewCodecContextParams* params);

    /**
     * Delete codec context
     * @param context context
     */
    void Delete(HCodecContext context);

    /**
     * Create a new decoder
     * @param context context
     * @param format format
     * @param buffer buffer
     * @param buffer_size buffer size
     * @param decoder decoder (out)
     * @return RESULT_OK on success
     */
    Result NewDecoder(HCodecContext context, Format format, const void* buffer, uint32_t buffer_size, HDecoder* decoder);

    /**
     * Delete decoder
     * @param context context
     * @param decoder decoder
     */
    void DeleteDecoder(HCodecContext context, HDecoder decoder);

    /**
     * Get info
     * @param context context
     * @param decoder decoder
     * @param info info
     */
    void GetInfo(HCodecContext context, HDecoder decoder, Info* info);

    /**
     * Decode data
     * @param context context
     * @param decoder decoder
     * @param buffer buffer
     * @param buffer_size buffer size in bytes
     * @param decoded actual bytes decoded
     * @return RESULT_OK on success
     */
    Result Decode(HCodecContext context, HDecoder decoder, char* buffer, uint32_t buffer_size, uint32_t* decoded);

    /**
     * Reset decoder
     * @param context context
     * @param decoder decoder
     * @return RESULT_OK on success
     */
    Result Reset(HCodecContext context, HDecoder decoder);
}

#endif // #ifndef DM_SOUND_CODEC_H
