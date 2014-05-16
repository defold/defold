#ifndef DM_SOUND2_H
#define DM_SOUND2_H

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
    #define DM_DECLARE_SOUND_DEVICE(symbol, name, open, close, queue, free_buffer_slots, device_info) \
            dmSound::DeviceType DM_SOUND_PASTE2(symbol, __LINE__) = { \
                    name, \
                    open, \
                    close, \
                    queue, \
                    free_buffer_slots, \
                    device_info, \
                    0 \
            };\
        DM_REGISTER_SOUND_DEVICE(symbol, DM_SOUND_PASTE2(symbol, __LINE__))

}


#endif // #ifndef DM_SOUND2_H
