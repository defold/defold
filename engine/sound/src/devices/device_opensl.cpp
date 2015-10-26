#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/mutex.h>
#include <dlib/time.h>
#include <dlib/profile.h>
#include "sound.h"
#include "sound2.h"

#include <android_native_app_glue.h>

#include <SLES/OpenSLES.h>

extern struct android_app* __attribute__((weak)) g_AndroidApp;

/**
 * OpenSL ES audio device
 *
 * - Mix-rate is hard-coded to 44100 but should probably match hardware mix-rate
 *   in order to reduce latency
 * - SL_ENGINEOPTION_THREADSAFE is set to false. Hence, all api-calls to OpenSL must
 *   be performed from a single thread.
 * - Error checking isn't complete and we assume that basic
 *   operations doesn't fail. However, logging should be present at
 *   all api-calls (CheckAndPrintError)
 */
namespace dmDeviceOpenSL
{
    struct Buffer
    {
        void*    m_Buffer;
        uint32_t m_FrameCount;
        uint32_t m_FrameCapacity;

        Buffer()
        {
            Clear();
        }

        Buffer(void* buffer, uint32_t frame_count)
        {
            m_Buffer = buffer;
            m_FrameCount = frame_count;
            m_FrameCapacity = 0;
        }

        void Clear()
        {
            memset(this, 0, sizeof(*this));
        }
    };

    // Candidate for dlib?
    struct Queue
    {
        dmArray<Buffer> m_Queue;
        uint32_t m_Size;
        int      m_Start;
        int      m_End;

        Queue()
        {
            m_Size = 0;
            m_Start = 0;
            m_End = 0;
        }

        void SetSize(uint32_t size)
        {
            m_Queue.SetCapacity(size);
            m_Queue.SetSize(size);
            m_Size = 0;
            m_Start = 0;
            m_End = 0;
        }

        void Push(Buffer& b)
        {
            assert(m_Size < m_Queue.Size());
            m_Queue[m_End] = b;
            m_End = (m_End + 1) % m_Queue.Size();
            m_Size++;
        }

        Buffer Pop()
        {
            assert(m_Size > 0);
            int i = m_Start;
            m_Start = (m_Start + 1) % m_Queue.Size();
            m_Size--;
            return m_Queue[i];
        }

        uint32_t Size()
        {
            return m_Size;
        }

        bool Full()
        {
            return m_Size >= m_Queue.Size();
        }
    };

    struct OpenSLDevice
    {
        uint32_t        m_MixRate;

        Queue           m_Free;
        Queue           m_Playing;
        Queue           m_Ready;

        SLObjectItf      m_SL;
        SLEngineItf      m_Engine;
        SLObjectItf      m_OutputMix;

        SLObjectItf      m_Player;
        SLPlayItf        m_Play;
        SLBufferQueueItf m_BufferQueue;
        uint32_t         m_BufferQueueCount;
        SLVolumeItf      m_Volume;

        bool             m_Underflow;

        dmMutex::Mutex   m_Mutex;

        OpenSLDevice()
        {
            m_MixRate = 0;

            m_SL = 0;
            m_Engine = 0;
            m_OutputMix = 0;

            m_Player = 0;
            m_Play = 0;
            m_BufferQueue = 0;
            m_BufferQueueCount = 0;
            m_Volume = 0;

            m_Underflow = true;

            m_Mutex = 0;
        }
    };

    static bool CheckAndPrintError(SLresult res)
    {
        if (res != SL_RESULT_SUCCESS)
        {
            dmLogError("OpenSL error: %d", res);
            return true;
        }
        return false;
    }

    static void BufferQueueCallback(SLBufferQueueItf queueItf, void *context)
    {
        OpenSLDevice* opensl = (OpenSLDevice*) context;
        DM_MUTEX_SCOPED_LOCK(opensl->m_Mutex);

        if (opensl->m_Playing.Size() > 0) {
            Buffer b = opensl->m_Playing.Pop();
            opensl->m_Free.Push(b);
        }
        if (opensl->m_Ready.Size() > 0) {
            Buffer b = opensl->m_Ready.Pop();
            SLresult res = (*queueItf)->Enqueue(queueItf, b.m_Buffer, b.m_FrameCount * sizeof(uint16_t) * 2);
            CheckAndPrintError(res);
            opensl->m_Playing.Push(b);
        } else {
            opensl->m_Underflow = true;
        }
    }

    static jclass LoadClass(JNIEnv* env, const char* class_name)
    {
        jclass activity_class = env->FindClass("android/app/NativeActivity");
        jmethodID get_class_loader = env->GetMethodID(activity_class,"getClassLoader", "()Ljava/lang/ClassLoader;");
        jobject cls = env->CallObjectMethod(g_AndroidApp->activity->clazz, get_class_loader);
        jclass class_loader = env->FindClass("java/lang/ClassLoader");
        jmethodID find_class = env->GetMethodID(class_loader, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
        jstring str_class_name = env->NewStringUTF(class_name);
        jclass klass = (jclass)env->CallObjectMethod(cls, find_class, str_class_name);
        assert(klass);
        env->DeleteLocalRef(str_class_name);
        return klass;
    }

    static int GetSampleRate()
    {
        JNIEnv* env;
        g_AndroidApp->activity->vm->AttachCurrentThread(&env, NULL);

        jclass sound_class = LoadClass(env, "com.defold.sound.Sound2");

        jmethodID get_sample_rate = env->GetStaticMethodID(sound_class, "getSampleRate", "(Landroid/content/Context;)I");
        assert(get_sample_rate);
        jint sample_rate = env->CallStaticIntMethod(sound_class, get_sample_rate, g_AndroidApp->activity->clazz);
        g_AndroidApp->activity->vm->DetachCurrentThread();

        return sample_rate;
    }

    dmSound::Result DeviceOpenSLOpen(const dmSound::OpenDeviceParams* params, dmSound::HDevice* device)
    {
        SLObjectItf sl = 0;
        SLEngineItf engine = 0;
        SLObjectItf output_mix = 0;
        SLObjectItf player = 0;
        SLPlayItf play = 0;
        SLBufferQueueItf buffer_queue = 0;
        SLVolumeItf volume = 0;

        SLEngineOption options[] = {
                { (SLuint32) SL_ENGINEOPTION_THREADSAFE,
                  (SLuint32) SL_BOOLEAN_FALSE }
        };

        const SLboolean required[] = { };
        SLInterfaceID ids[] = { };

        const SLboolean required_player[] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
        SLInterfaceID ids_player[] = {SL_IID_VOLUME, SL_IID_BUFFERQUEUE};
        assert(sizeof(required_player)/sizeof(required_player[0]) == sizeof(ids_player)/sizeof(ids_player[0]));

        OpenSLDevice* opensl = 0;

        // Actual initalization starts here.

        const int rate = GetSampleRate();

        SLresult res = slCreateEngine(&sl, 1, options, 0, NULL, NULL);
        if (CheckAndPrintError(res))
            return dmSound::RESULT_UNKNOWN_ERROR;

        res = (*sl)->Realize(sl, SL_BOOLEAN_FALSE);
        if (CheckAndPrintError(res))
            goto cleanup_sl;

        res = (*sl)->GetInterface(sl, SL_IID_ENGINE, &engine);
        if (CheckAndPrintError(res))
            goto cleanup_sl;

        res = (*engine)->CreateOutputMix(engine, &output_mix, 0, ids, required);
        if (CheckAndPrintError(res))
            goto cleanup_sl;

        res = (*output_mix)->Realize(output_mix, SL_BOOLEAN_FALSE);
        if (CheckAndPrintError(res))
            goto cleanup_mix;

        SLDataLocator_BufferQueue locator;
        locator.locatorType = SL_DATALOCATOR_BUFFERQUEUE;
        locator.numBuffers = params->m_BufferCount;

        SLDataFormat_PCM format;
        format.formatType = SL_DATAFORMAT_PCM;
        format.numChannels = 2;
        format.samplesPerSec = rate * 1000;
        format.bitsPerSample = 16;
        // TODO: Correct?
        format.containerSize = 16;
        // NOTE: It seems that 0 is the correct default and not SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT
        // The latter results in error for mono sounds
        format.channelMask = 0;
        format.endianness = SL_BYTEORDER_LITTLEENDIAN;

        SLDataSource data_source;
        data_source.pFormat = &format;
        data_source.pLocator = &locator;

        SLDataLocator_OutputMix locator_out_mix;
        locator_out_mix.locatorType = SL_DATALOCATOR_OUTPUTMIX;
        locator_out_mix.outputMix = output_mix;

        SLDataSink sink;
        sink.pFormat = NULL;
        sink.pLocator = &locator_out_mix;

        res = (*engine)->CreateAudioPlayer(engine, &player, &data_source, &sink, sizeof(required_player) / sizeof(required_player[0]), ids_player, required_player);
        if (res != SL_RESULT_SUCCESS) {
            dmLogError("Failed to create player: %d", res);
            goto cleanup_mix;
        }

        res = (*player)->Realize(player, SL_BOOLEAN_FALSE);
        if (CheckAndPrintError(res))
            goto cleanup_player;

        res = (*player)->GetInterface(player, SL_IID_PLAY, (void*)&play);
        if (CheckAndPrintError(res))
            goto cleanup_player;

        res = (*player)->GetInterface(player, SL_IID_BUFFERQUEUE, (void*)&buffer_queue);
        if (CheckAndPrintError(res))
            goto cleanup_player;

        res = (*player)->GetInterface(player, SL_IID_VOLUME, (void*)&volume);
        if (CheckAndPrintError(res))
            goto cleanup_player;

        opensl = new OpenSLDevice;
        opensl->m_MixRate = rate;

        opensl->m_Free.SetSize(params->m_BufferCount);
        opensl->m_Ready.SetSize(params->m_BufferCount);
        opensl->m_Playing.SetSize(params->m_BufferCount);

        for (uint32_t i = 0; i < params->m_BufferCount; ++i) {
            uint32_t n = params->m_FrameCount * sizeof(uint16_t) * 2;
            Buffer b(malloc(n), params->m_FrameCount);
            opensl->m_Free.Push(b);
        }

        opensl->m_SL = sl;
        opensl->m_Engine = engine;
        opensl->m_OutputMix = output_mix;

        opensl->m_Play = play;
        opensl->m_Player = player;
        opensl->m_BufferQueue = buffer_queue;
        opensl->m_BufferQueueCount = params->m_BufferCount;
        opensl->m_Volume = volume;

        opensl->m_Mutex = dmMutex::New();

        res = (*buffer_queue)->RegisterCallback(buffer_queue, BufferQueueCallback, opensl);
        if (CheckAndPrintError(res))
            goto cleanup_device;

        *device = opensl;

        res = (*play)->SetPlayState(play, SL_PLAYSTATE_PLAYING);
        CheckAndPrintError(res);

        return dmSound::RESULT_OK;

cleanup_device:
        dmMutex::Delete(opensl->m_Mutex);
        delete opensl;
cleanup_player:
        (*player)->Destroy(player);
cleanup_mix:
        (*output_mix)->Destroy(output_mix);
cleanup_sl:
        (*sl)->Destroy(sl);
        return dmSound::RESULT_UNKNOWN_ERROR;
    }

    void DeviceOpenSLClose(dmSound::HDevice device)
    {
        OpenSLDevice* opensl = (OpenSLDevice*) device;
        dmMutex::Lock(opensl->m_Mutex);

        SLBufferQueueItf buffer_queue = opensl->m_BufferQueue;

        SLBufferQueueState state;
        SLresult res = (*buffer_queue)->GetState(buffer_queue, &state);
        while (state.count > 0) {
            dmMutex::Unlock(opensl->m_Mutex);
            dmTime::Sleep(10 * 1000);
            dmMutex::Lock(opensl->m_Mutex);
            (*buffer_queue)->GetState(buffer_queue, &state);
        }

        SLPlayItf play = opensl->m_Play;
        res = (*play)->SetPlayState(play, SL_PLAYSTATE_STOPPED);
        CheckAndPrintError(res);

        (*buffer_queue)->Clear(buffer_queue);

        SLObjectItf player = opensl->m_Player;
        (*player)->Destroy(player);

        (*opensl->m_OutputMix)->Destroy(opensl->m_OutputMix);
        (*opensl->m_SL)->Destroy(opensl->m_SL);

        if (opensl->m_Playing.Size() > 0) {
            dmLogError("Unexpected buffers playing (%d)", opensl->m_Playing.Size());
        }

        if (opensl->m_Ready.Size() > 0) {
            dmLogError("Unexpected ready buffers (%d)", opensl->m_Ready.Size());
        }

        while (opensl->m_Free.Size() > 0) {
            Buffer b = opensl->m_Free.Pop();
            free(b.m_Buffer);
        }

        dmMutex::Unlock(opensl->m_Mutex);
        dmMutex::Delete(opensl->m_Mutex);

        delete opensl;
    }

    dmSound::Result DeviceOpenSLQueue(dmSound::HDevice device, const int16_t* samples, uint32_t sample_count)
    {
        OpenSLDevice* opensl = (OpenSLDevice*) device;
        DM_MUTEX_SCOPED_LOCK(opensl->m_Mutex);

        assert(opensl->m_Free.Size() > 0);
        Buffer b = opensl->m_Free.Pop();

        memcpy(b.m_Buffer, samples, sample_count * sizeof(uint16_t) * 2);
        b.m_FrameCount = sample_count;
        opensl->m_Ready.Push(b);

        if (opensl->m_Underflow) {
            opensl->m_Underflow = false;
            BufferQueueCallback(opensl->m_BufferQueue, opensl);
        }

        return dmSound::RESULT_OK;
    }

    uint32_t DeviceOpenSLFreeBufferSlots(dmSound::HDevice device)
    {
        OpenSLDevice* opensl = (OpenSLDevice*) device;
        DM_MUTEX_SCOPED_LOCK(opensl->m_Mutex);

        return opensl->m_Free.Size();
    }

    void DeviceOpenSLDeviceInfo(dmSound::HDevice device, dmSound::DeviceInfo* info)
    {
        OpenSLDevice* opensl = (OpenSLDevice*) device;
        info->m_MixRate = opensl->m_MixRate;
    }

    DM_DECLARE_SOUND_DEVICE(DefaultSoundDevice, "default", DeviceOpenSLOpen, DeviceOpenSLClose, DeviceOpenSLQueue, DeviceOpenSLFreeBufferSlots, DeviceOpenSLDeviceInfo);
}

