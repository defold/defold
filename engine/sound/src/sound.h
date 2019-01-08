#ifndef DM_SOUND_H
#define DM_SOUND_H

#include <dlib/configfile.h>
#include <ddf/ddf.h>

#include <vectormath/cpp/vectormath_aos.h>
using namespace Vectormath::Aos;

namespace dmSound
{
    typedef struct SoundData* HSoundData;
    typedef struct SoundInstance* HSoundInstance;

    enum SoundDataType
    {
        SOUND_DATA_TYPE_WAV        = 0,
        SOUND_DATA_TYPE_OGG_VORBIS = 1,
    };

    enum Parameter
    {
        PARAMETER_GAIN = 0,
    };

    enum Result
    {
        RESULT_OK                 =  0,    //!< RESULT_OK
        RESULT_OUT_OF_SOURCES     = -1,    //!< RESULT_OUT_OF_SOURCES
        RESULT_EFFECT_NOT_FOUND   = -2,    //!< RESULT_EFFECT_NOT_FOUND
        RESULT_OUT_OF_INSTANCES   = -3,    //!< RESULT_OUT_OF_INSTANCES
        RESULT_RESOURCE_LEAK      = -4,    //!< RESULT_RESOURCE_LEAK
        RESULT_OUT_OF_BUFFERS     = -5,    //!< RESULT_OUT_OF_BUFFERS
        RESULT_INVALID_PROPERTY   = -6,    //!< RESULT_INVALID_PROPERTY
        RESULT_UNKNOWN_SOUND_TYPE = -7,    //!< RESULT_UNKNOWN_SOUND_TYPE
        RESULT_INVALID_STREAM_DATA= -8,    //!< RESULT_INVALID_STREAM_DATA
        RESULT_OUT_OF_MEMORY      = -9,    //!< RESULT_OUT_OF_MEMORY
        RESULT_UNSUPPORTED        = -10,   //!< RESULT_UNSUPPORTED
        RESULT_DEVICE_NOT_FOUND   = -11,   //!< RESULT_DEVICE_NOT_FOUND
        RESULT_OUT_OF_GROUPS      = -12,   //!< RESULT_OUT_OF_GROUPS
        RESULT_NO_SUCH_GROUP      = -13,   //!< RESULT_NO_SUCH_GROUP
        RESULT_NOTHING_TO_PLAY    = -14,   //!< RESULT_NOTHING_TO_PLAY
        RESULT_INIT_ERROR         = -15,   //!< RESULT_INIT_ERROR
        RESULT_FINI_ERROR         = -16,   //!< RESULT_FINI_ERROR
        RESULT_UNKNOWN_ERROR      = -1000, //!< RESULT_UNKNOWN_ERROR
    };

    struct Stats
    {
        uint32_t m_BufferUnderflowCount;
    };

    // TODO:
    // - Music streaming.

    struct InitializeParams;
    void SetDefaultInitializeParams(InitializeParams* params);

    struct InitializeParams
    {
        const char* m_OutputDevice;
        float    m_MasterGain;
        uint32_t m_MaxSoundData;
        uint32_t m_MaxSources;
        uint32_t m_MaxBuffers;
        uint32_t m_BufferSize;
        uint32_t m_FrameCount;
        uint32_t m_MaxInstances;

        InitializeParams()
        {
            SetDefaultInitializeParams(this);
        }
    };

    Result Initialize(dmConfigFile::HConfig config, const InitializeParams* params);
    Result Finalize();

    void   GetStats(Stats* stats);

    Result NewSoundData(const void* sound_buffer, uint32_t sound_buffer_size, SoundDataType type, HSoundData* sound_data, dmhash_t name);
    Result SetSoundData(HSoundData sound_data, const void* sound_buffer, uint32_t sound_buffer_size);
    uint32_t GetSoundResourceSize(HSoundData sound_data);
    Result DeleteSoundData(HSoundData sound_data);

    Result NewSoundInstance(HSoundData sound_data, HSoundInstance* sound_instance);
    Result DeleteSoundInstance(HSoundInstance sound_instance);

    Result SetInstanceGroup(HSoundInstance instance, const char* group);
    Result SetInstanceGroup(HSoundInstance instance, dmhash_t group_hash);

    Result AddGroup(const char* group);
    Result SetGroupGain(dmhash_t group_hash, float gain);
    Result GetGroupGain(dmhash_t group_hash, float* gain);
    uint32_t GetGroupCount();
    Result GetGroupHash(uint32_t index, dmhash_t* hash);

    Result GetGroupRMS(dmhash_t group_hash, float window, float* rms_left, float* rms_right);
    Result GetGroupPeak(dmhash_t group_hash, float window, float* peak_left, float* peak_right);

    Result Update();

    Result Play(HSoundInstance sound_instance);
    Result Stop(HSoundInstance sound_instance);
    bool IsPlaying(HSoundInstance sound_instance);

    Result SetLooping(HSoundInstance sound_instance, bool looping);

    Result SetParameter(HSoundInstance sound_instance, Parameter parameter, const Vector4& value);
    Result GetParameter(HSoundInstance sound_instance, Parameter parameter, Vector4& value);

    bool IsMusicPlaying();
    bool IsPhoneCallActive();

}

namespace dmSound
{
    /**
     * Device handle
     */
    typedef void* HDevice;

    /**
     * Parameters
     */
    struct OpenDeviceParams
    {
        OpenDeviceParams()
        {
            memset(this, 0, sizeof(*this));
        }
        uint32_t m_BufferCount;
        uint32_t m_FrameCount;
    };

    /**
     * Device info
     */
    struct DeviceInfo
    {
        uint32_t m_MixRate;
    };

    /**
     * Device type
     */
    struct DeviceType
    {
        /**
         * Device name
         */
        const char* m_Name;

        /**
         * Open device
         * @param params
         * @param device
         * @return
         */
        Result (*m_Open)(const OpenDeviceParams* params, HDevice* device);

        /**
         * Close device
         * @param device
         */
        void (*m_Close)(HDevice device);

        /**
         * Queue buffer.
         * @note Buffer data in 16-bit signed PCM stereo
         * @param device
         * @param frames
         * @param frame_count
         * @return
         */
        Result (*m_Queue)(HDevice device, const int16_t* frames, uint32_t frame_count);

        /**
         * Number of free buffers
         * @param device
         * @return
         */
        uint32_t (*m_FreeBufferSlots)(HDevice device);

        /**
         * Get device info
         * @param device
         * @param info
         */
        void (*m_DeviceInfo)(HDevice device, DeviceInfo* info);

        /**
         * Enable device context
         * @param device
         */
        void (*m_DeviceRestart)(HDevice device);

        /**
         * Disable device context
         * @param device
         */
        void (*m_DeviceStop)(HDevice device);

        /**
         * Internal
         */
        DeviceType* m_Next;
    };

    /**
     * Internal function
     * @param device
     * @return
     */
    Result RegisterDevice(struct DeviceType* device);

    #ifdef __GNUC__
        // Workaround for dead-stripping on OSX/iOS. The symbol "name" is explicitly exported. See wscript "exported_symbols"
        // Otherwise it's dead-stripped even though -no_dead_strip_inits_and_terms is passed to the linker
        // The bug only happens when the symbol is in a static library though
        #define DM_REGISTER_SOUND_DEVICE(name, desc) extern "C" void __attribute__((constructor)) name () { \
            dmSound::RegisterDevice(&desc); \
        }
    #else
        #define DM_REGISTER_SOUND_DEVICE(name, desc) extern "C" void name () { \
            dmSound::RegisterDevice(&desc); \
            }\
            int name ## Wrapper(void) { name(); return 0; } \
            __pragma(section(".CRT$XCU",read)) \
            __declspec(allocate(".CRT$XCU")) int (* _Fp ## name)(void) = name ## Wrapper;
    #endif

    #define DM_SOUND_PASTE(x, y) x ## y
    #define DM_SOUND_PASTE2(x, y) DM_SOUND_PASTE(x, y)

    /**
     * Declare a new sound device
     */
    #define DM_DECLARE_SOUND_DEVICE(symbol, name, open, close, queue, free_buffer_slots, device_info, enable, disable) \
            dmSound::DeviceType DM_SOUND_PASTE2(symbol, __LINE__) = { \
                    name, \
                    open, \
                    close, \
                    queue, \
                    free_buffer_slots, \
                    device_info, \
                    enable, \
                    disable, \
                    0 \
            };\
        DM_REGISTER_SOUND_DEVICE(symbol, DM_SOUND_PASTE2(symbol, __LINE__))

}

#endif // DM_SOUND_H
