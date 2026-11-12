// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
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

#include "record_private.h"
#include "record_timeline.h"
#include <dmsdk/dlib/condition_variable.h>
#include <dmsdk/dlib/dstrings.h>
#include <dmsdk/dlib/thread.h>
#include <dmsdk/dlib/time.h>
#include <dmsdk/graphics/graphics.h>
#include <wels/codec_api.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MINIMP4_IMPLEMENTATION
#include <minimp4.h>

namespace dmRecord
{
    const uint32_t FRAME_QUEUE_SIZE = 3;
    const uint32_t MAX_CAPTURE_PIXELS = 4096 * 4096;
    const uint32_t MP4_TIMESCALE = 90000;

    struct Frame
    {
        uint8_t* m_Pixels;
        uint32_t m_Capacity;
        uint64_t m_Time;
    };

    struct LinuxRecording
    {
        dmMutex::HMutex                         m_Mutex;
        dmConditionVariable::HConditionVariable m_Condition;
        dmThread::Thread                        m_Thread;
        dmGraphics::HContext                    m_Context;
        Status                                  m_Status;
        uint32_t                                m_CaptureWidth;
        uint32_t                                m_CaptureHeight;
        Frame                                   m_Frames[FRAME_QUEUE_SIZE];
        uint32_t                                m_Read;
        uint32_t                                m_Write;
        uint32_t                                m_Count; // Includes the frame being encoded.
        uint64_t                                m_NextCapture;
        bool                                    m_Stop;
        ISVCEncoder*                            m_Encoder;
        uint8_t*                                m_Yuv;
        FILE*                                   m_File;
        MP4E_mux_t*                             m_Mux;
        mp4_h26x_writer_t                       m_Writer;
        uint8_t*                                m_Pending;
        uint32_t                                m_PendingSize;
        uint32_t                                m_PendingCapacity;
        uint64_t                                m_PendingTime;
        uint32_t                                m_WrittenDuration;
    };
    static LinuxRecording g_Capture;

    bool IsSupported()
    {
        dmGraphics::AdapterFamily adapter = dmGraphics::GetInstalledAdapterFamily();
        return adapter == dmGraphics::ADAPTER_FAMILY_OPENGL || adapter == dmGraphics::ADAPTER_FAMILY_OPENGLES || adapter == dmGraphics::ADAPTER_FAMILY_VULKAN;
    }

    bool IsAudioSupported()
    {
        return false;
    }
    const char* GetBackendName()
    {
        return "openh264";
    }
    const char* GetMinimumPlatformVersion()
    {
        return "";
    }
    const char* GetUnsupportedReason()
    {
        return "Linux recording requires an OpenGL, OpenGL ES or Vulkan debug runtime with a window";
    }

    bool GetWindow(void* context, Window* window)
    {
        if (!IsSupported())
        {
            return false;
        }
        window->m_Handle = (uintptr_t)context;
        window->m_Width = dmGraphics::GetWindowWidth((dmGraphics::HContext)context);
        window->m_Height = dmGraphics::GetWindowHeight((dmGraphics::HContext)context);
        window->m_Scale = 1.0;
        if (!window->m_Width || !window->m_Height)
        {
            return false;
        }
        if (!g_Capture.m_Mutex)
        {
            g_Capture.m_Mutex = dmMutex::New();
            g_Capture.m_Condition = dmConditionVariable::New();
        }
        return true;
    }

    void GetNativeStatus(Status* status)
    {
        DM_MUTEX_SCOPED_LOCK(g_Capture.m_Mutex);
        *status = g_Capture.m_Status;
    }

    bool PrepareWindow(void* context, Window* window)
    {
        return GetWindow(context, window);
    }

    void RestoreWindow() {}

    static void Fail(LinuxRecording* recording, const char* message)
    {
        // Caller holds the mutex, or startup has not published an active recorder.
        if (!recording->m_Status.m_Failure[0])
        {
            dmStrlCpy(recording->m_Status.m_Failure, message, sizeof(recording->m_Status.m_Failure));
        }
    }

    static int WriteMp4(int64_t offset, const void* buffer, size_t size, void* context)
    {
        FILE* file = (FILE*)context;
        return fseeko(file, (off_t)offset, SEEK_SET) || fwrite(buffer, 1, size, file) != size ? -1 : 0;
    }

    // Convert native-size top-down BGRA to limited-range BT.601 I420,
    // averaging chroma over each 2x2 block without resampling the image.
    static void ConvertFrame(const Frame* frame, uint8_t* yuv, uint32_t width, uint32_t height)
    {
        uint8_t* u = yuv + width * height;
        uint8_t* v = u + width * height / 4;
        for (uint32_t y = 0; y < height; y += 2)
        {
            for (uint32_t x = 0; x < width; x += 2)
            {
                int r = 0, g = 0, b = 0;
                for (uint32_t dy = 0; dy < 2; ++dy)
                {
                    for (uint32_t dx = 0; dx < 2; ++dx)
                    {
                        const uint8_t* pixel = frame->m_Pixels + ((y + dy) * width + x + dx) * 4;
                        yuv[(y + dy) * width + x + dx] = ((66 * pixel[2] + 129 * pixel[1] + 25 * pixel[0] + 128) >> 8) + 16;
                        r += pixel[2];
                        g += pixel[1];
                        b += pixel[0];
                    }
                }
                uint32_t chroma = (y / 2) * (width / 2) + x / 2;
                u[chroma] = ((-38 * r - 74 * g + 112 * b + 512) >> 10) + 128;
                v[chroma] = ((112 * r - 94 * g - 18 * b + 512) >> 10) + 128;
            }
        }
    }

    static bool WritePending(LinuxRecording* recording, uint64_t end_time)
    {
        if (!recording->m_PendingSize)
        {
            return true;
        }
        uint64_t duration = end_time > recording->m_PendingTime ? end_time - recording->m_PendingTime : 1;
        // minimp4 writes 32-bit track/movie durations as well as sample durations.
        // Include the minimum one-tick sample duration in the accumulated total.
        if (duration > UINT32_MAX - recording->m_WrittenDuration)
        {
            DM_MUTEX_SCOPED_LOCK(recording->m_Mutex);
            Fail(recording, "recording exceeds the MP4 duration limit");
            return false;
        }
        if (mp4_h26x_write_nal(&recording->m_Writer, recording->m_Pending, recording->m_PendingSize, (uint32_t)duration) != MP4E_STATUS_OK)
        {
            return false;
        }
        recording->m_WrittenDuration += (uint32_t)duration;
        return true;
    }

    static bool EncodeFrame(LinuxRecording* recording, const Frame* frame)
    {
        uint32_t width = recording->m_Status.m_Width;
        uint32_t height = recording->m_Status.m_Height;
        ConvertFrame(frame, recording->m_Yuv, width, height);
        SSourcePicture picture = {};
        picture.iColorFormat = videoFormatI420;
        picture.iPicWidth = width;
        picture.iPicHeight = height;
        picture.iStride[0] = width;
        picture.iStride[1] = picture.iStride[2] = width / 2;
        picture.pData[0] = recording->m_Yuv;
        picture.pData[1] = picture.pData[0] + width * height;
        picture.pData[2] = picture.pData[1] + width * height / 4;
        picture.uiTimeStamp = (frame->m_Time - recording->m_Status.m_StartedMonotonicTime) / 1000;
        SFrameBSInfo encoded = {};
        if (recording->m_Encoder->EncodeFrame(&picture, &encoded) != cmResultSuccess)
        {
            return false;
        }
        if (encoded.eFrameType == videoFrameTypeSkip)
        {
            return true;
        }
        uint64_t time = (frame->m_Time - recording->m_Status.m_StartedMonotonicTime) * MP4_TIMESCALE / 1000000;
        bool first = recording->m_PendingSize == 0;
        if (!WritePending(recording, time))
        {
            return false;
        }
        if ((uint32_t)encoded.iFrameSizeInBytes > recording->m_PendingCapacity)
        {
            void* data = realloc(recording->m_Pending, encoded.iFrameSizeInBytes);
            if (!data)
            {
                return false;
            }
            recording->m_Pending = (uint8_t*)data;
            recording->m_PendingCapacity = encoded.iFrameSizeInBytes;
        }
        recording->m_PendingSize = 0;
        for (int i = 0; i < encoded.iLayerNum; ++i)
        {
            SLayerBSInfo* layer = &encoded.sLayerInfo[i];
            uint32_t size = 0;
            for (int j = 0; j < layer->iNalCount; ++j)
            {
                size += layer->pNalLengthInByte[j];
            }
            memcpy(recording->m_Pending + recording->m_PendingSize, layer->pBsBuf, size);
            recording->m_PendingSize += size;
        }
        // The first image covers startup latency; later samples retain capture
        // times so a full queue or slow rendering never speeds up the video.
        recording->m_PendingTime = first ? 0 : time;
        return recording->m_PendingSize != 0;
    }

    static bool CloseResources(LinuxRecording* recording)
    {
        bool ok = true;
        if (recording->m_Mux)
        {
            mp4_h26x_write_close(&recording->m_Writer);
            ok = MP4E_close(recording->m_Mux) == MP4E_STATUS_OK;
            recording->m_Mux = 0;
        }
        if (recording->m_File)
        {
            if (fclose(recording->m_File))
            {
                ok = false;
            }
            recording->m_File = 0;
        }
        if (recording->m_Encoder)
        {
            recording->m_Encoder->Uninitialize();
            WelsDestroySVCEncoder(recording->m_Encoder);
            recording->m_Encoder = 0;
        }
        free(recording->m_Yuv);
        recording->m_Yuv = 0;
        free(recording->m_Pending);
        recording->m_Pending = 0;
        recording->m_PendingSize = recording->m_PendingCapacity = 0;
        for (uint32_t i = 0; i < FRAME_QUEUE_SIZE; ++i)
        {
            free(recording->m_Frames[i].m_Pixels);
            memset(&recording->m_Frames[i], 0, sizeof(Frame));
        }
        return ok;
    }

    static void EncoderWorker(void* context)
    {
        LinuxRecording* recording = (LinuxRecording*)context;
        for (;;)
        {
            Frame* frame;
            {
                DM_MUTEX_SCOPED_LOCK(recording->m_Mutex);
                while (!recording->m_Count && !recording->m_Stop)
                {
                    dmConditionVariable::Wait(recording->m_Condition, recording->m_Mutex);
                }
                if ((!recording->m_Count && recording->m_Stop) || recording->m_Status.m_Failure[0])
                {
                    break;
                }
                frame = &recording->m_Frames[recording->m_Read];
            }
            bool ok = EncodeFrame(recording, frame);
            DM_MUTEX_SCOPED_LOCK(recording->m_Mutex);
            recording->m_Read = (recording->m_Read + 1) % FRAME_QUEUE_SIZE;
            --recording->m_Count;
            if (!ok)
            {
                Fail(recording, "H.264 encoding or MP4 writing failed");
                recording->m_Stop = true;
            }
        }
        {
            DM_MUTEX_SCOPED_LOCK(recording->m_Mutex);
            recording->m_Stop = true;
            if (!recording->m_Status.m_StoppedMonotonicTime)
            {
                recording->m_Status.m_StoppedMonotonicTime = dmTime::GetMonotonicTime();
                recording->m_Status.m_StoppedWallTime = dmTime::GetTime();
            }
        }
        uint64_t end = (recording->m_Status.m_StoppedMonotonicTime - recording->m_Status.m_StartedMonotonicTime) * MP4_TIMESCALE / 1000000;
        bool frames = recording->m_PendingSize != 0;
        bool ok = WritePending(recording, end);
        if (!CloseResources(recording))
        {
            ok = false;
        }
        DM_MUTEX_SCOPED_LOCK(recording->m_Mutex);
        if (!frames)
        {
            Fail(recording, "no rendered frames were captured");
        }
        if (!ok)
        {
            Fail(recording, "unable to finalize the MP4 file");
        }
        recording->m_Status.m_Active = false;
        recording->m_Status.m_Finalized = !recording->m_Status.m_Failure[0];
    }

    bool StartNative(const Window* window, const char* path, uint32_t fps, bool audio, Status* status)
    {
        LinuxRecording* recording = &g_Capture;
        if (recording->m_Thread)
        {
            dmThread::Join(recording->m_Thread);
            recording->m_Thread = 0;
        }
        DM_MUTEX_SCOPED_LOCK(recording->m_Mutex);
        memset(&recording->m_Status, 0, sizeof(Status));
        memset(&recording->m_Writer, 0, sizeof(recording->m_Writer));
        recording->m_Status.m_Supported = true;
        dmStrlCpy(recording->m_Status.m_Path, path, sizeof(recording->m_Status.m_Path));
        recording->m_CaptureWidth = (uint32_t)window->m_Width;
        recording->m_CaptureHeight = (uint32_t)window->m_Height;
        uint32_t width = recording->m_CaptureWidth & ~1U;
        uint32_t height = recording->m_CaptureHeight & ~1U;
        recording->m_Status.m_Width = width;
        recording->m_Status.m_Height = height;
        recording->m_Status.m_Fps = fps;
        recording->m_Context = (dmGraphics::HContext)window->m_Handle;
        recording->m_Read = recording->m_Write = recording->m_Count = 0;
        recording->m_WrittenDuration = 0;
        recording->m_Stop = false;
        if (audio)
        {
            Fail(recording, "Linux recording supports video only");
        }
        else if (width < 16 || height < 16)
        {
            Fail(recording, "recording requires a framebuffer of at least 16x16 pixels");
        }
        else if ((uint64_t)recording->m_CaptureWidth * recording->m_CaptureHeight > MAX_CAPTURE_PIXELS)
        {
            Fail(recording, "framebuffer exceeds the recording readback size limit");
        }
        else if ((uint64_t)((width + 15) / 16) * ((height + 15) / 16) > 36864)
        {
            Fail(recording, "recording dimensions exceed the H.264 encoder frame-size limit");
        }
        else if (WelsCreateSVCEncoder(&recording->m_Encoder) != cmResultSuccess)
        {
            Fail(recording, "unable to create the H.264 encoder");
        }
        if (!recording->m_Status.m_Failure[0])
        {
            SEncParamExt params;
            recording->m_Encoder->GetDefaultParams(&params);
            params.iUsageType = SCREEN_CONTENT_REAL_TIME;
            params.iPicWidth = width;
            params.iPicHeight = height;
            params.fMaxFrameRate = (float)fps;
            uint64_t bitrate = (uint64_t)width * height * fps / 8;
            params.iTargetBitrate = (int)(bitrate < 128000 ? 128000 : bitrate > 20000000 ? 20000000 :
                                                                                           bitrate);
            params.iRCMode = RC_BITRATE_MODE;
            params.bEnableFrameSkip = true;
            params.bEnableAdaptiveQuant = false;
            params.bEnableBackgroundDetection = false;
            params.iTemporalLayerNum = params.iSpatialLayerNum = 1;
            params.iMultipleThreadIdc = 1;
            params.uiIntraPeriod = fps * 2;
            SSpatialLayerConfig* layer = &params.sSpatialLayers[0];
            layer->iVideoWidth = width;
            layer->iVideoHeight = height;
            layer->fFrameRate = (float)fps;
            layer->iSpatialBitrate = params.iTargetBitrate;
            layer->uiProfileIdc = PRO_BASELINE;
            layer->bVideoSignalTypePresent = true;
            layer->uiVideoFormat = 5;
            layer->bFullRange = false;
            layer->bColorDescriptionPresent = true;
            layer->uiColorPrimaries = layer->uiTransferCharacteristics = layer->uiColorMatrix = 6; // SMPTE 170M / BT.601.
            if (recording->m_Encoder->InitializeExt(&params) != cmResultSuccess)
            {
                Fail(recording, "unable to initialize the H.264 encoder");
            }
        }
        if (!recording->m_Status.m_Failure[0])
        {
            recording->m_Yuv = (uint8_t*)malloc(width * height * 3 / 2);
            recording->m_File = fopen(path, "wb");
            if (!recording->m_Yuv || !recording->m_File)
            {
                Fail(recording, "unable to allocate recording buffers or open the MP4 file");
            }
            else
            {
                recording->m_Mux = MP4E_open(0, 0, recording->m_File, WriteMp4);
                if (!recording->m_Mux || mp4_h26x_write_init(&recording->m_Writer, recording->m_Mux, width, height, 0) != MP4E_STATUS_OK)
                {
                    Fail(recording, "unable to initialize the MP4 writer");
                }
            }
        }
        if (!recording->m_Status.m_Failure[0])
        {
            recording->m_Status.m_StartedWallTime = dmTime::GetTime();
            recording->m_Status.m_StartedMonotonicTime = dmTime::GetMonotonicTime();
            recording->m_NextCapture = recording->m_Status.m_StartedMonotonicTime;
            recording->m_Thread = dmThread::New(EncoderWorker, 512 * 1024, recording, "record_encode");
            if (!recording->m_Thread)
            {
                Fail(recording, "recording encoder worker unavailable");
            }
        }
        recording->m_Status.m_Active = !recording->m_Status.m_Failure[0];
        if (!recording->m_Status.m_Active)
        {
            CloseResources(recording);
        }
        *status = recording->m_Status;
        return status->m_Active;
    }

    static void CaptureFrameLocked(LinuxRecording* recording)
    {
        uint64_t now = dmTime::GetMonotonicTime();
        if (!recording->m_Status.m_Active || recording->m_Stop || now < recording->m_NextCapture)
        {
            return;
        }
        uint64_t elapsed = now - recording->m_Status.m_StartedMonotonicTime;
        recording->m_NextCapture = recording->m_Status.m_StartedMonotonicTime + NextVideoFrameTime(elapsed, recording->m_Status.m_Fps);
        if (recording->m_Count == FRAME_QUEUE_SIZE)
        {
            return; // Bound memory and never wait for the encoder on the render thread.
        }
        uint32_t width = dmGraphics::GetWindowWidth(recording->m_Context);
        uint32_t height = dmGraphics::GetWindowHeight(recording->m_Context);
        if (!width || !height)
        {
            return;
        }
        if (width != recording->m_CaptureWidth || height != recording->m_CaptureHeight)
        {
            recording->m_Status.m_Stopping = true;
            recording->m_Status.m_StoppedMonotonicTime = now;
            recording->m_Status.m_StoppedWallTime = dmTime::GetTime();
            recording->m_Stop = true;
            dmConditionVariable::Signal(recording->m_Condition);
            return;
        }
        // Crop at most one row/column for I420; never scale the captured pixels.
        width = recording->m_Status.m_Width;
        height = recording->m_Status.m_Height;
        Frame* frame = &recording->m_Frames[recording->m_Write];
        uint32_t bytes = width * height * 4;
        if (frame->m_Capacity < bytes)
        {
            void* pixels = realloc(frame->m_Pixels, bytes);
            if (!pixels)
            {
                Fail(recording, "unable to allocate framebuffer readback storage");
                recording->m_Stop = true;
                dmConditionVariable::Signal(recording->m_Condition);
                return;
            }
            frame->m_Pixels = (uint8_t*)pixels;
            frame->m_Capacity = bytes;
        }
        frame->m_Time = now;
        dmGraphics::RenderTargetBindingParams params = {};
        dmGraphics::SetRenderTarget(recording->m_Context, 0, params);
        dmGraphics::ReadPixels(recording->m_Context, 0, 0, width, height, frame->m_Pixels, bytes);
        recording->m_Write = (recording->m_Write + 1) % FRAME_QUEUE_SIZE;
        ++recording->m_Count;
        dmConditionVariable::Signal(recording->m_Condition);
    }

    void CaptureFrame()
    {
        LinuxRecording* recording = &g_Capture;
        // In particular, do not wait for asynchronous encoder initialization.
        if (!recording->m_Mutex || !dmMutex::TryLock(recording->m_Mutex))
        {
            return;
        }
        CaptureFrameLocked(recording);
        dmMutex::Unlock(recording->m_Mutex);
    }

    bool StopNative(Status* status)
    {
        LinuxRecording* recording = &g_Capture;
        {
            DM_MUTEX_SCOPED_LOCK(recording->m_Mutex);
            if (!recording->m_Stop)
            {
                recording->m_Status.m_StoppedWallTime = dmTime::GetTime();
                recording->m_Status.m_StoppedMonotonicTime = dmTime::GetMonotonicTime();
                recording->m_Stop = true;
                dmConditionVariable::Signal(recording->m_Condition);
            }
        }
        if (recording->m_Thread)
        {
            dmThread::Join(recording->m_Thread);
            recording->m_Thread = 0;
        }
        GetNativeStatus(status);
        return status->m_Finalized;
    }

    void FinalizeNative()
    {
        LinuxRecording* recording = &g_Capture;
        if (!recording->m_Mutex)
        {
            return;
        }
        Status status;
        StopNative(&status);
        dmConditionVariable::Delete(recording->m_Condition);
        dmMutex::Delete(recording->m_Mutex);
        memset(recording, 0, sizeof(*recording));
    }
} // namespace dmRecord
