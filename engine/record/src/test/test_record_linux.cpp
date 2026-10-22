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

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include <dmsdk/dlib/time.h>
#include <dmsdk/graphics/graphics.h>
#include <wels/codec_api.h>
#include <minimp4.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "record/record_private.h"

static uint32_t                  g_Width, g_Height, g_Reads;
static dmGraphics::AdapterFamily g_Adapter;
static bool                      g_Backbuffer;

// Exercise the real recorder and codec with a controlled top-down BGRA image.
namespace dmGraphics
{
    AdapterFamily GetInstalledAdapterFamily()
    {
        return g_Adapter;
    }
    uint32_t GetWindowWidth(HContext)
    {
        return g_Width;
    }
    uint32_t GetWindowHeight(HContext)
    {
        return g_Height;
    }
    void SetRenderTarget(HContext, HRenderTarget target, const RenderTargetBindingParams&)
    {
        g_Backbuffer = target == 0;
    }
    void ReadPixels(HContext, int32_t x, int32_t y, uint32_t width, uint32_t height, void* buffer, uint32_t size)
    {
        ASSERT_TRUE(g_Backbuffer);
        ASSERT_EQ(0, x);
        ASSERT_EQ(0, y);
        ASSERT_EQ(g_Width & ~1U, width);
        ASSERT_EQ(g_Height & ~1U, height);
        ASSERT_EQ(width * height * 4, size);
        const uint8_t colors[4][4] = { { 0, 0, 255, 255 }, { 0, 255, 0, 255 }, { 255, 0, 0, 255 }, { 255, 255, 255, 255 } };
        for (uint32_t row = 0; row < height; ++row)
        {
            for (uint32_t col = 0; col < width; ++col)
            {
                memcpy((uint8_t*)buffer + (row * width + col) * 4, colors[(row >= height / 2) * 2 + (col >= width / 2)], 4);
            }
        }
        ++g_Reads;
    }
} // namespace dmGraphics

class LinuxRecordingTest : public jc_test_base_class
{
protected:
    char m_Path[256];

    void SetUp()
    {
        g_Width = 160;
        g_Height = 120;
        g_Reads = 0;
        g_Adapter = dmGraphics::ADAPTER_FAMILY_OPENGL;
        g_Backbuffer = false;
        strcpy(m_Path, "/tmp/defold-record-linux-XXXXXX");
        int fd = mkstemp(m_Path);
        ASSERT_NE(-1, fd);
        close(fd);
    }

    void TearDown()
    {
        dmRecord::Finalize();
        unlink(m_Path);
    }

    dmRecord::StartParams Params()
    {
        dmRecord::StartParams params;
        params.m_Window = (void*)1;
        params.m_Path = m_Path;
        params.m_Fps = 60;
        return params;
    }

    static dmRecord::Status Complete()
    {
        dmRecord::Status status;
        uint64_t deadline = dmTime::GetMonotonicTime() + 10000000;
        do
        {
            dmRecord::GetStatus(&status);
            if (!status.m_Pending)
            {
                bool was_active = status.m_Active;
                dmRecord::Update();
                dmRecord::GetStatus(&status);
                if (was_active && !status.m_Pending && !status.m_Active)
                {
                    dmRecord::Update();
                }
            }
        } while (status.m_Pending && dmTime::GetMonotonicTime() < deadline);
        EXPECT_FALSE(status.m_Pending);
        return status;
    }

    void Start()
    {
        dmRecord::StartParams params = Params();
        dmRecord::Status status;
        ASSERT_TRUE(dmRecord::Start(&params, &status));
        status = Complete();
        ASSERT_TRUE(status.m_Active);
        ASSERT_FALSE(status.m_Audio);
        ASSERT_EQ(0, status.m_Failure[0]);
    }

    static void Capture()
    {
        uint32_t reads = g_Reads;
        uint64_t deadline = dmTime::GetMonotonicTime() + 10000000;
        // Wait for an accepted frame, not an assumed encoder or scheduler delay.
        while (g_Reads == reads && dmTime::GetMonotonicTime() < deadline)
        {
            dmRecord::CaptureFrame();
        }
        ASSERT_EQ(reads + 1, g_Reads);
    }

    static int ReadMp4(int64_t offset, void* buffer, size_t size, void* file)
    {
        return fseeko((FILE*)file, (off_t)offset, SEEK_SET) || fread(buffer, 1, size, (FILE*)file) != size ? -1 : 0;
    }

    static void ParameterSet(ISVCDecoder* decoder, const void* data, int size)
    {
        ASSERT_NE((const void*)0, data);
        uint8_t* nal = (uint8_t*)malloc(size + 4);
        nal[0] = nal[1] = nal[2] = 0;
        nal[3] = 1;
        memcpy(nal + 4, data, size);
        uint8_t* planes[3];
        SBufferInfo info = {};
        ASSERT_EQ(dsErrorFree, decoder->DecodeFrameNoDelay(nal, size + 4, planes, &info));
        free(nal);
    }

    void Verify(uint32_t frames, uint32_t width, uint32_t height, const dmRecord::Status* status)
    {
        FILE* file = fopen(m_Path, "rb");
        ASSERT_NE((FILE*)0, file);
        ASSERT_EQ(0, fseeko(file, 0, SEEK_END));
        MP4D_demux_t mp4 = {};
        ASSERT_TRUE(MP4D_open(&mp4, ReadMp4, file, ftello(file)));
        ASSERT_EQ(1U, mp4.track_count); // No audio track, including silent AAC.
        MP4D_track_t* track = &mp4.track[0];
        ASSERT_EQ((unsigned)MP4D_HANDLER_TYPE_VIDE, track->handler_type);
        ASSERT_EQ((unsigned)MP4_OBJECT_TYPE_AVC, track->object_type_indication);
        ASSERT_EQ(width, track->SampleDescription.video.width);
        ASSERT_EQ(height, track->SampleDescription.video.height);
        ASSERT_EQ(frames, track->sample_count);
        if (status)
        {
            uint64_t expected = (status->m_StoppedMonotonicTime - status->m_StartedMonotonicTime) * track->timescale / 1000000;
            uint64_t actual = ((uint64_t)track->duration_hi << 32) | track->duration_lo;
            ASSERT_GE(actual, expected);
            ASSERT_LE(actual, expected + frames);
        }
        ISVCDecoder* decoder = 0;
        ASSERT_EQ(0, WelsCreateDecoder(&decoder));
        SDecodingParam params = {};
        params.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_AVC;
        ASSERT_EQ(0, decoder->Initialize(&params));
        int size = 0;
        const void* data = MP4D_read_sps(&mp4, 0, 0, &size);
        ParameterSet(decoder, data, size);
        data = MP4D_read_pps(&mp4, 0, 0, &size);
        ParameterSet(decoder, data, size);
        uint32_t next_timestamp = 0;
        for (uint32_t i = 0; i < frames; ++i)
        {
            unsigned bytes, timestamp, duration;
            MP4D_file_offset_t offset = MP4D_frame_offset(&mp4, 0, i, &bytes, &timestamp, &duration);
            ASSERT_EQ(next_timestamp, timestamp);
            ASSERT_GT(duration, 0U);
            next_timestamp += duration;
            uint8_t* sample = (uint8_t*)malloc(bytes);
            ASSERT_EQ(0, ReadMp4(offset, sample, bytes, file));
            for (uint32_t n = 0; n < bytes;)
            {
                ASSERT_LE(n + 4, bytes);
                uint32_t length = ((uint32_t)sample[n] << 24) | ((uint32_t)sample[n + 1] << 16) | ((uint32_t)sample[n + 2] << 8) | sample[n + 3];
                ASSERT_LE(length, bytes - n - 4);
                sample[n] = sample[n + 1] = sample[n + 2] = 0;
                sample[n + 3] = 1;
                n += length + 4;
            }
            uint8_t* planes[3];
            SBufferInfo info = {};
            ASSERT_EQ(dsErrorFree, decoder->DecodeFrameNoDelay(sample, bytes, planes, &info));
            ASSERT_EQ(1, info.iBufferStatus);
            ASSERT_EQ((int)width, info.UsrData.sSystemBuffer.iWidth);
            ASSERT_EQ((int)height, info.UsrData.sSystemBuffer.iHeight);
            const int expected[4][3] = { { 82, 90, 240 }, { 144, 54, 34 }, { 41, 240, 110 }, { 235, 128, 128 } };
            for (uint32_t corner = 0; corner < 4; ++corner)
            {
                uint32_t x = width * (corner % 2 ? 3 : 1) / 4;
                uint32_t y = height * (corner / 2 ? 3 : 1) / 4;
                for (uint32_t plane = 0; plane < 3; ++plane)
                {
                    uint32_t scale = plane ? 2 : 1;
                    int pixel = planes[plane][(y / scale) * info.UsrData.sSystemBuffer.iStride[plane ? 1 : 0] + x / scale];
                    ASSERT_LE(abs(pixel - expected[corner][plane]), 12);
                }
            }
            free(sample);
        }
        decoder->Uninitialize();
        WelsDestroyDecoder(decoder);
        MP4D_close(&mp4);
        fclose(file);
    }
};

// Decode the native-size MP4 and check color channels, orientation, timing and absence of audio.
TEST_F(LinuxRecordingTest, VideoOnly)
{
    Start();
    Capture();
    Capture();
    Capture();
    dmRecord::Status status;
    ASSERT_TRUE(dmRecord::Stop(&status));
    status = Complete();
    ASSERT_TRUE(status.m_Finalized);
    ASSERT_EQ(0, status.m_Failure[0]);
    Verify(3, 160, 120, &status);
    // Optional artifact for independent ffprobe/FFmpeg validation outside the test runner.
    const char* output = getenv("DEFOLD_RECORD_TEST_OUTPUT");
    if (output)
    {
        FILE* source = fopen(m_Path, "rb");
        FILE* target = fopen(output, "wb");
        ASSERT_NE((FILE*)0, target);
        uint8_t buffer[4096];
        size_t bytes;
        while ((bytes = fread(buffer, 1, sizeof(buffer), source)))
        {
            ASSERT_EQ(bytes, fwrite(buffer, 1, bytes, target));
        }
        fclose(source);
        ASSERT_EQ(0, fclose(target));
    }
}

// Growth and shrink stop before readback, preserve the completed video and permit recording at the new size.
TEST_F(LinuxRecordingTest, ResizeFinalizesAndRestarts)
{
    const uint32_t widths[] = { 320, 160 };
    const uint32_t heights[] = { 240, 120 };
    for (uint32_t i = 0; i < 2; ++i)
    {
        uint32_t width = g_Width;
        uint32_t height = g_Height;
        Start();
        Capture();
        uint32_t reads = g_Reads;
        g_Width = widths[i];
        g_Height = heights[i];
        dmRecord::Status status;
        uint64_t deadline = dmTime::GetMonotonicTime() + 10000000;
        do
        {
            dmRecord::CaptureFrame();
            dmRecord::Update();
            dmRecord::GetStatus(&status);
        } while ((status.m_Active || status.m_Pending) && dmTime::GetMonotonicTime() < deadline);
        ASSERT_FALSE(status.m_Active);
        ASSERT_FALSE(status.m_Pending);
        ASSERT_TRUE(status.m_Finalized);
        ASSERT_EQ(0, status.m_Failure[0]);
        ASSERT_EQ(reads, g_Reads);
        Verify(1, width, height, &status);
    }
}

// Engine shutdown drains queued frames and writes the MP4 index before freeing the window.
TEST_F(LinuxRecordingTest, FinalizeAndRestart)
{
    for (int i = 0; i < 2; ++i)
    {
        Start();
        Capture();
        dmRecord::Finalize();
        Verify(1, 160, 120, 0);
    }
}

// Reject unsupported audio and headless graphics before launching the encoder.
TEST_F(LinuxRecordingTest, CapabilitiesAndAudio)
{
    ASSERT_TRUE(dmRecord::IsSupported());
    ASSERT_FALSE(dmRecord::IsAudioSupported());
    ASSERT_STREQ("openh264", dmRecord::GetBackendName());
    g_Adapter = dmGraphics::ADAPTER_FAMILY_OPENGLES;
    ASSERT_TRUE(dmRecord::IsSupported());
    g_Adapter = dmGraphics::ADAPTER_FAMILY_VULKAN;
    ASSERT_TRUE(dmRecord::IsSupported());
    dmRecord::StartParams params = Params();
    params.m_Audio = true;
    dmRecord::Status status;
    ASSERT_FALSE(dmRecord::Start(&params, &status));
    g_Adapter = dmGraphics::ADAPTER_FAMILY_NULL;
    ASSERT_FALSE(dmRecord::IsSupported());
    params.m_Audio = false;
    ASSERT_FALSE(dmRecord::Start(&params, &status));
    g_Adapter = dmGraphics::ADAPTER_FAMILY_WEBGPU;
    ASSERT_FALSE(dmRecord::IsSupported());
}

// An invalid path fails asynchronously and does not prevent a subsequent successful recording.
TEST_F(LinuxRecordingTest, FailedStartCanRetry)
{
    dmRecord::StartParams params = Params();
    char invalid_path[512];
    snprintf(invalid_path, sizeof(invalid_path), "%s/missing.mp4", m_Path);
    params.m_Path = invalid_path;
    dmRecord::Status status;
    ASSERT_TRUE(dmRecord::Start(&params, &status));
    status = Complete();
    ASSERT_FALSE(status.m_Active);
    ASSERT_NE(0, status.m_Failure[0]);
    Start();
    Capture();
    ASSERT_TRUE(dmRecord::Stop(&status));
    status = Complete();
    ASSERT_TRUE(status.m_Finalized);
    Verify(1, 160, 120, &status);
}

// Stopping before rendering reports an empty recording as a failure, not a downloadable MP4.
TEST_F(LinuxRecordingTest, EmptyRecordingFails)
{
    Start();
    dmRecord::Status status;
    ASSERT_TRUE(dmRecord::Stop(&status));
    status = Complete();
    ASSERT_FALSE(status.m_Active);
    ASSERT_FALSE(status.m_Finalized);
    ASSERT_NE(0, status.m_Failure[0]);
}

// Odd native dimensions crop one row/column for I420 without resampling the image.
TEST_F(LinuxRecordingTest, OddNativeDimensions)
{
    g_Width = 319;
    g_Height = 239;
    dmRecord::StartParams params = Params();
    dmRecord::Status status;
    ASSERT_TRUE(dmRecord::Start(&params, &status));
    status = Complete();
    ASSERT_TRUE(status.m_Active);
    ASSERT_EQ(318U, status.m_Width);
    ASSERT_EQ(238U, status.m_Height);
    Capture();
    ASSERT_TRUE(dmRecord::Stop(&status));
    status = Complete();
    ASSERT_TRUE(status.m_Finalized);
    Verify(1, 318, 238, &status);
}

// Native dimensions beyond the readback limit fail before allocating buffers and permit retry.
TEST_F(LinuxRecordingTest, OversizedFramebufferCanRetry)
{
    g_Width = g_Height = 8192;
    dmRecord::StartParams params = Params();
    dmRecord::Status status;
    ASSERT_TRUE(dmRecord::Start(&params, &status));
    status = Complete();
    ASSERT_FALSE(status.m_Active);
    ASSERT_FALSE(status.m_Finalized);
    ASSERT_NE(0, status.m_Failure[0]);
    ASSERT_EQ(0U, g_Reads);
    g_Width = 160;
    g_Height = 120;
    Start();
    Capture();
    ASSERT_TRUE(dmRecord::Stop(&status));
    status = Complete();
    ASSERT_TRUE(status.m_Finalized);
    Verify(1, 160, 120, &status);
}

// The encoder rejects excessive dimensions before allocating large frame buffers.
TEST_F(LinuxRecordingTest, OversizedOutputFails)
{
    g_Width = 4096;
    g_Height = 2306;
    dmRecord::StartParams params = Params();
    dmRecord::Status status;
    ASSERT_TRUE(dmRecord::Start(&params, &status));
    status = Complete();
    ASSERT_FALSE(status.m_Active);
    ASSERT_NE(0, status.m_Failure[0]);
}

// A native framebuffer below the encoder minimum fails instead of being silently enlarged.
TEST_F(LinuxRecordingTest, SmallFramebufferFails)
{
    g_Width = 15;
    dmRecord::StartParams params = Params();
    dmRecord::Status status;
    ASSERT_TRUE(dmRecord::Start(&params, &status));
    status = Complete();
    ASSERT_FALSE(status.m_Active);
    ASSERT_NE(0, status.m_Failure[0]);
    ASSERT_EQ(0U, g_Reads);
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
