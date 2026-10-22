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

// Opt-in native integration tests: requires an interactive desktop and an audio
// output device. Completion is driven by capture and waveOut events, not sleeps.
#define WIN32_LEAN_AND_MEAN
#undef WINVER
#undef _WIN32_WINNT
#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <dwmapi.h>
#include <mmsystem.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include <record/record_private.h>
#include <record/record_timeline.h>
#include <dlib/time.h>

using Microsoft::WRL::ComPtr;

static bool     g_White;
static bool     g_Animate;
static uint32_t g_Paints;
static uint32_t g_PaintTarget;
static HANDLE   g_Painted;

static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_TIMER)
    {
        if (g_Animate)
        {
            InvalidateRect(window, 0, FALSE);
        }
        return 0;
    }
    if (message == WM_PAINT)
    {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(window, &paint);
        RECT rect;
        GetClientRect(window, &rect);
        FillRect(dc, &rect, (HBRUSH)GetStockObject(g_White ? WHITE_BRUSH : BLACK_BRUSH));
        const COLORREF colors[] = { RGB(224, 32, 32), RGB(32, 224, 32), RGB(32, 32, 224), RGB(224, 224, 32) };
        for (uint32_t i = 0; i < 4; ++i)
        {
            LONG x = (i & 1) ? rect.right - 24 : 0;
            LONG y = (i & 2) ? rect.bottom - 24 : 0;
            RECT corner = { x, y, x + 24, y + 24 };
            HBRUSH brush = CreateSolidBrush(colors[i]);
            FillRect(dc, &corner, brush);
            DeleteObject(brush);
        }
        // Most tests animate a marker; static-duration tests deliberately stop
        // changing pixels so WGC has no new frames to deliver.
        rect.left = rect.right / 2;
        rect.right = rect.left + 8;
        rect.top = 8;
        rect.bottom = 16;
        ++g_Paints;
        HBRUSH marker = CreateSolidBrush(RGB(g_Animate ? g_Paints % 256 : 0, 40, 80));
        FillRect(dc, &rect, marker);
        DeleteObject(marker);
        EndPaint(window, &paint);
        if (g_PaintTarget && g_Paints >= g_PaintTarget)
        {
            SetEvent(g_Painted);
        }
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

static bool PumpUntil(HANDLE event)
{
    ULONGLONG deadline = GetTickCount64() + 20000;
    while (GetTickCount64() < deadline)
    {
        ULONGLONG now = GetTickCount64();
        DWORD result = MsgWaitForMultipleObjects(1, &event, FALSE, now < deadline ? (DWORD)(deadline - now) : 0, QS_ALLINPUT);
        if (result == WAIT_OBJECT_0)
        {
            return true;
        }
        if (result != WAIT_OBJECT_0 + 1)
        {
            return false;
        }
        MSG message;
        while (PeekMessageW(&message, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    return false;
}

struct Recording
{
    dmRecord::Window      m_Window;
    dmRecord::Status      m_Status;
    HANDLE                m_Started, m_Stop, m_Thread;
    bool                  m_Audio;
    bool                  m_Finalize;
    bool                  m_StartResult, m_StopResult;
    DPI_AWARENESS_CONTEXT m_DpiContext;
    bool                  m_DpiContextPreserved;
    uint64_t              m_StartBefore, m_StartAfter, m_StopBefore;
    char                  m_Path[MAX_PATH];
};

static DWORD WINAPI RecordWorker(void* context)
{
    Recording* recording = (Recording*)context;
    DPI_AWARENESS_CONTEXT previous = SetThreadDpiAwarenessContext(recording->m_DpiContext);
    recording->m_StartBefore = dmTime::GetMonotonicTime();
    recording->m_StartResult = dmRecord::StartNative(&recording->m_Window, recording->m_Path, 30, recording->m_Audio, &recording->m_Status);
    recording->m_StartAfter = dmTime::GetMonotonicTime();
    SetEvent(recording->m_Started);
    WaitForSingleObject(recording->m_Stop, INFINITE);
    recording->m_StopBefore = dmTime::GetMonotonicTime();
    if (recording->m_StartResult)
    {
        if (recording->m_Finalize)
        {
            dmRecord::FinalizeNative();
            dmRecord::GetNativeStatus(&recording->m_Status);
            recording->m_StopResult = recording->m_Status.m_Finalized;
        }
        else
        {
            recording->m_StopResult = dmRecord::StopNative(&recording->m_Status);
        }
    }
    dmRecord::FinalizeNative();
    recording->m_DpiContextPreserved = AreDpiAwarenessContextsEqual(GetThreadDpiAwarenessContext(), recording->m_DpiContext);
    SetThreadDpiAwarenessContext(previous);
    return 0;
}

static void CALLBACK WaveDone(HWAVEOUT, UINT message, DWORD_PTR context, DWORD_PTR, DWORD_PTR)
{
    if (message == WOM_DONE)
    {
        SetEvent((HANDLE)context);
    }
}

class RecordWindowsTest : public jc_test_base_class
{
protected:
    HWND      m_Window;
    HWAVEOUT  m_Output;
    HANDLE    m_Played;
    Recording m_Recording;

    void CreateTestWindow(DPI_AWARENESS_CONTEXT context)
    {
        DPI_AWARENESS_CONTEXT previous = SetThreadDpiAwarenessContext(context);
        ASSERT_NE((DPI_AWARENESS_CONTEXT)0, previous);
        RECT rect = { 0, 0, 320, 240 };
        AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
        m_Window = CreateWindowW(L"DefoldRecordingTest", L"Defold recording test", WS_OVERLAPPEDWINDOW, 40, 40, rect.right - rect.left, rect.bottom - rect.top, 0, 0, GetModuleHandleW(0), 0);
        SetThreadDpiAwarenessContext(previous);
        ASSERT_NE((HWND)0, m_Window);
        ShowWindow(m_Window, SW_SHOWNOACTIVATE);
        UpdateWindow(m_Window);
        SetTimer(m_Window, 1, 16, 0);
    }

    void SetUp()
    {
        memset(&m_Recording, 0, sizeof(m_Recording));
        m_Output = 0;
        m_Played = CreateEventW(0, FALSE, FALSE, 0);
        g_White = false;
        g_Animate = true;
        g_PaintTarget = 0;
        g_Painted = CreateEventW(0, TRUE, FALSE, 0);
        CreateTestWindow(DPI_AWARENESS_CONTEXT_UNAWARE);
        ASSERT_TRUE(dmRecord::IsSupported());
        ASSERT_TRUE(dmRecord::IsAudioSupported());
        WAVEFORMATEX format = {};
        format.wFormatTag = WAVE_FORMAT_PCM;
        format.nChannels = 2;
        format.nSamplesPerSec = 48000;
        format.wBitsPerSample = 16;
        format.nBlockAlign = 4;
        format.nAvgBytesPerSec = 192000;
        ASSERT_EQ((MMRESULT)MMSYSERR_NOERROR, waveOutOpen(&m_Output, WAVE_MAPPER, &format, (DWORD_PTR)WaveDone, (DWORD_PTR)m_Played, CALLBACK_FUNCTION));
    }

    void TearDown()
    {
        Finish();
        if (m_Output)
        {
            waveOutReset(m_Output);
            waveOutClose(m_Output);
        }
        CloseHandle(m_Played);
        DestroyWindow(m_Window);
        CloseHandle(g_Painted);
    }

    bool Begin(bool audio, const char* suffix, bool finalize = false, DPI_AWARENESS_CONTEXT dpi_context = DPI_AWARENESS_CONTEXT_UNAWARE)
    {
        memset(&m_Recording, 0, sizeof(m_Recording));
        m_Recording.m_Audio = audio;
        m_Recording.m_Finalize = finalize;
        m_Recording.m_DpiContext = dpi_context;
        dmRecord::GetWindow(m_Window, &m_Recording.m_Window);
        snprintf(m_Recording.m_Path, sizeof(m_Recording.m_Path), "record-native-%lu-%s.mp4", GetCurrentProcessId(), suffix);
        m_Recording.m_Started = CreateEventW(0, TRUE, FALSE, 0);
        m_Recording.m_Stop = CreateEventW(0, TRUE, FALSE, 0);
        m_Recording.m_Thread = CreateThread(0, 0, RecordWorker, &m_Recording, 0, 0);
        if (!m_Recording.m_Thread || !PumpUntil(m_Recording.m_Started))
        {
            return false;
        }
        if (!m_Recording.m_StartResult)
        {
            printf("Start failed: %s\n", m_Recording.m_Status.m_Failure);
        }
        return m_Recording.m_StartResult;
    }

    void Finish()
    {
        if (m_Recording.m_Thread)
        {
            SetEvent(m_Recording.m_Stop);
            ASSERT_TRUE(PumpUntil(m_Recording.m_Thread));
            CloseHandle(m_Recording.m_Thread);
            CloseHandle(m_Recording.m_Stop);
            CloseHandle(m_Recording.m_Started);
            m_Recording.m_Thread = 0;
            ASSERT_TRUE(m_Recording.m_DpiContextPreserved);
            if (m_Recording.m_StartResult && !m_Recording.m_StopResult)
            {
                printf("Stop failed: %s\n", m_Recording.m_Status.m_Failure);
            }
        }
    }

    void Play(bool tone, uint32_t frames)
    {
        g_White = tone;
        InvalidateRect(m_Window, 0, FALSE);
        UpdateWindow(m_Window);
        DwmFlush();
        int16_t* samples = (int16_t*)calloc(frames * 2, sizeof(int16_t));
        for (uint32_t i = 0; tone && i < frames; ++i)
        {
            samples[i * 2] = samples[i * 2 + 1] = (int16_t)(8000 * sin(i * 440.0 * 6.283185307179586 / 48000));
        }
        WAVEHDR buffer = {};
        buffer.lpData = (char*)samples;
        buffer.dwBufferLength = frames * 4;
        ASSERT_EQ((MMRESULT)MMSYSERR_NOERROR, waveOutPrepareHeader(m_Output, &buffer, sizeof(buffer)));
        ASSERT_EQ((MMRESULT)MMSYSERR_NOERROR, waveOutWrite(m_Output, &buffer, sizeof(buffer)));
        bool completed = PumpUntil(m_Played);
        if (!completed)
        {
            waveOutReset(m_Output);
        }
        waveOutUnprepareHeader(m_Output, &buffer, sizeof(buffer));
        free(samples);
        ASSERT_TRUE(completed);
    }

    void InspectVideo(LONGLONG* video_end, LONGLONG* white_onset, uint32_t minimum_frames)
    {
        wchar_t path[MAX_PATH];
        MultiByteToWideChar(CP_UTF8, 0, m_Recording.m_Path, -1, path, MAX_PATH);
        ComPtr<IMFAttributes> attributes;
        ASSERT_EQ(S_OK, MFCreateAttributes(&attributes, 1));
        attributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
        ComPtr<IMFSourceReader> reader;
        ASSERT_EQ(S_OK, MFCreateSourceReaderFromURL(path, attributes.Get(), &reader));
        reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE);
        reader->SetStreamSelection(MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);
        ComPtr<IMFMediaType> type;
        ASSERT_EQ(S_OK, MFCreateMediaType(&type));
        type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
        ASSERT_EQ(S_OK, reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, type.Get()));
        *video_end = 0;
        *white_onset = -1;
        uint32_t frames = 0;
        for (;;)
        {
            DWORD flags;
            LONGLONG time;
            ComPtr<IMFSample> sample;
            ASSERT_EQ(S_OK, reader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, 0, &flags, &time, &sample));
            if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
            {
                break;
            }
            if (!sample)
            {
                continue;
            }
            ComPtr<IMFMediaBuffer> buffer;
            ASSERT_EQ(S_OK, sample->ConvertToContiguousBuffer(&buffer));
            BYTE* data;
            DWORD bytes;
            ASSERT_EQ(S_OK, buffer->Lock(&data, 0, &bytes));
            if (bytes >= 320 * 240 * 4 && *white_onset < 0 && data[(120 * 320 + 160) * 4] > 200)
            {
                *white_onset = time;
            }
            buffer->Unlock();
            LONGLONG duration;
            ASSERT_EQ(S_OK, sample->GetSampleDuration(&duration));
            ASSERT_GT(duration, 0LL);
            if (frames)
            {
                ASSERT_LT(fabs((double)(time - *video_end)), 10000.0);
            }
            *video_end = time + duration;
            ++frames;
        }
        ASSERT_GT(frames, minimum_frames);
        // Bound the capture epoch by Start's entry/return, excluding finalization
        // latency. A static tail must reach Stop even without an audio track.
        LONGLONG minimum_duration = (m_Recording.m_StopBefore - m_Recording.m_StartAfter) * 10;
        LONGLONG maximum_duration = (m_Recording.m_StopBefore - m_Recording.m_StartBefore) * 10;
        ASSERT_GE(*video_end + 333334, minimum_duration);
        ASSERT_LE(*video_end, maximum_duration + 1000000);
        printf("%s: video=%u frames, duration=%.1f ms\n", m_Recording.m_Path, frames, *video_end / 10000.0);
    }

    void Inspect(bool audio, bool tone, uint32_t minimum_video_frames = 10)
    {
        wchar_t path[MAX_PATH];
        MultiByteToWideChar(CP_UTF8, 0, m_Recording.m_Path, -1, path, MAX_PATH);
        ComPtr<IMFAttributes> attributes;
        ASSERT_EQ(S_OK, MFCreateAttributes(&attributes, 1));
        attributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
        ComPtr<IMFSourceReader> reader;
        ASSERT_EQ(S_OK, MFCreateSourceReaderFromURL(path, attributes.Get(), &reader));
        ComPtr<IMFMediaType> native;
        ASSERT_EQ(S_OK, reader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &native));
        GUID subtype;
        ASSERT_EQ(S_OK, native->GetGUID(MF_MT_SUBTYPE, &subtype));
        ASSERT_TRUE(subtype == MFVideoFormat_H264);
        native.Reset();
        HRESULT result = reader->GetNativeMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, &native);
        if (!audio)
        {
            ASSERT_EQ(MF_E_INVALIDSTREAMNUMBER, result);
            LONGLONG video_end, white_onset;
            InspectVideo(&video_end, &white_onset, minimum_video_frames);
            return;
        }
        ASSERT_EQ(S_OK, result);
        ASSERT_EQ(S_OK, native->GetGUID(MF_MT_SUBTYPE, &subtype));
        ASSERT_TRUE(subtype == MFAudioFormat_AAC);
        ASSERT_EQ(48000u, MFGetAttributeUINT32(native.Get(), MF_MT_AUDIO_SAMPLES_PER_SECOND, 0));
        ASSERT_EQ(2u, MFGetAttributeUINT32(native.Get(), MF_MT_AUDIO_NUM_CHANNELS, 0));

        reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE);
        reader->SetStreamSelection(MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);
        ComPtr<IMFMediaType> type;
        ASSERT_EQ(S_OK, MFCreateMediaType(&type));
        type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
        type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
        ASSERT_EQ(S_OK, reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, type.Get()));
        uint64_t audio_frames = 0;
        LONGLONG audio_end = 0, audio_onset = -1;
        int peak = 0;
        for (;;)
        {
            DWORD flags;
            LONGLONG time;
            ComPtr<IMFSample> sample;
            ASSERT_EQ(S_OK, reader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, 0, &flags, &time, &sample));
            if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
            {
                break;
            }
            if (!sample)
            {
                continue;
            }
            ComPtr<IMFMediaBuffer> buffer;
            ASSERT_EQ(S_OK, sample->ConvertToContiguousBuffer(&buffer));
            BYTE* data;
            DWORD bytes;
            ASSERT_EQ(S_OK, buffer->Lock(&data, 0, &bytes));
            const int16_t* samples = (int16_t*)data;
            for (DWORD i = 0; i < bytes / sizeof(int16_t); ++i)
            {
                int amplitude = abs((int)samples[i]);
                if (amplitude > peak)
                {
                    peak = amplitude;
                }
                if (audio_onset < 0 && amplitude > 1000)
                {
                    audio_onset = time + dmRecord::FramesToTime(i / 2, 48000);
                }
            }
            buffer->Unlock();
            audio_frames += bytes / 4;
            LONGLONG duration;
            ASSERT_EQ(S_OK, sample->GetSampleDuration(&duration));
            ASSERT_GE(time + duration, audio_end);
            audio_end = time + duration;
        }
        ASSERT_GT(audio_frames, 24000ULL);
        if (tone)
        {
            ASSERT_GT(peak, 1000);
        }
        else
        {
            ASSERT_LT(peak, 100);
        }

        LONGLONG video_end, white_onset;
        InspectVideo(&video_end, &white_onset, minimum_video_frames);
        printf("%s: audio=%llu frames, peak=%d, end difference=%.1f ms\n",
               m_Recording.m_Path,
               audio_frames,
               peak,
               (audio_end - video_end) / 10000.0);
        ASSERT_LT(fabs((double)(audio_end - video_end)), 1500000.0);
        if (tone)
        {
            printf("Tone/flash offset: %.1f ms\n", (audio_onset - white_onset) / 10000.0);
            ASSERT_GE(audio_onset, 0LL);
            ASSERT_GE(white_onset, 0LL);
            ASSERT_LT(fabs((double)(audio_onset - white_onset)), 1500000.0);
        }
    }

    void InspectClientArea(uint32_t minimum_frames = 6)
    {
        wchar_t path[MAX_PATH];
        MultiByteToWideChar(CP_UTF8, 0, m_Recording.m_Path, -1, path, MAX_PATH);
        ComPtr<IMFAttributes> attributes;
        ASSERT_EQ(S_OK, MFCreateAttributes(&attributes, 1));
        attributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
        ComPtr<IMFSourceReader> reader;
        ASSERT_EQ(S_OK, MFCreateSourceReaderFromURL(path, attributes.Get(), &reader));
        reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE);
        reader->SetStreamSelection(MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);
        ComPtr<IMFMediaType> type;
        ASSERT_EQ(S_OK, MFCreateMediaType(&type));
        type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
        ASSERT_EQ(S_OK, reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, type.Get()));
        type.Reset();
        ASSERT_EQ(S_OK, reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &type));
        UINT32 width, height;
        ASSERT_EQ(S_OK, MFGetAttributeSize(type.Get(), MF_MT_FRAME_SIZE, &width, &height));
        ASSERT_EQ(m_Recording.m_Status.m_Width, width);
        ASSERT_EQ(m_Recording.m_Status.m_Height, height);
        LONG stride = (LONG)MFGetAttributeUINT32(type.Get(), MF_MT_DEFAULT_STRIDE, width * 4);
        uint32_t frames = 0;
        for (;;)
        {
            DWORD flags;
            ComPtr<IMFSample> sample;
            ASSERT_EQ(S_OK, reader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, 0, &flags, 0, &sample));
            if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
            {
                break;
            }
            if (!sample)
            {
                continue;
            }
            ComPtr<IMFMediaBuffer> buffer;
            ASSERT_EQ(S_OK, sample->ConvertToContiguousBuffer(&buffer));
            BYTE* data;
            DWORD bytes;
            ASSERT_EQ(S_OK, buffer->Lock(&data, 0, &bytes));
            bool valid = bytes >= abs(stride) * height;
            const BYTE expected[4][3] = { { 32, 32, 224 }, { 32, 224, 32 }, { 224, 32, 32 }, { 32, 224, 224 } };
            // Sample just inside each client corner, allowing H.264 color loss.
            // All four must survive: checking only the center misses title bars,
            // shifted crops, truncated content and an upside-down image.
            for (uint32_t i = 0; valid && i < 4; ++i)
            {
                uint32_t x = (i & 1) ? width - 9 : 8;
                uint32_t y = (i & 2) ? height - 9 : 8;
                if (stride < 0)
                {
                    y = height - 1 - y;
                }
                const BYTE* pixel = data + y * abs(stride) + x * 4;
                for (uint32_t c = 0; c < 3; ++c)
                {
                    valid = valid && abs((int)pixel[c] - expected[i][c]) < 25;
                }
                if (!valid)
                {
                    printf("%s: incorrect client corner %u: BGR=(%u,%u,%u)\n",
                           m_Recording.m_Path,
                           i,
                           pixel[0],
                           pixel[1],
                           pixel[2]);
                }
            }
            buffer->Unlock();
            ASSERT_TRUE(valid);
            ++frames;
        }
        ASSERT_GE(frames, minimum_frames);
        printf("%s: all four client corners preserved in %u frames\n", m_Recording.m_Path, frames);
    }
};

// Exclude window decorations and preserve every client corner in all DPI awareness modes.
// Run on a display above 100% scaling to catch logical/physical coordinate mismatches.
TEST_F(RecordWindowsTest, ClientAreaCropping)
{
    const DPI_AWARENESS_CONTEXT contexts[] = { DPI_AWARENESS_CONTEXT_UNAWARE,
                                               DPI_AWARENESS_CONTEXT_SYSTEM_AWARE,
                                               DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE };
    const char* suffixes[] = { "crop-unaware", "crop-system", "crop-per-monitor" };
    for (uint32_t i = 0; i < 3; ++i)
    {
        DestroyWindow(m_Window);
        CreateTestWindow(contexts[i]);
        ASSERT_TRUE(Begin(false, suffixes[i], false, contexts[i]));
        DPI_AWARENESS_CONTEXT previous = SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);
        RECT client;
        ASSERT_TRUE(GetClientRect(m_Window, &client));
        SetThreadDpiAwarenessContext(previous);
        ASSERT_EQ((uint32_t)(client.right - client.left) & ~1U, m_Recording.m_Status.m_Width);
        ASSERT_EQ((uint32_t)(client.bottom - client.top) & ~1U, m_Recording.m_Status.m_Height);
        ResetEvent(g_Painted);
        g_PaintTarget = g_Paints + 40;
        ASSERT_TRUE(PumpUntil(g_Painted));
        Finish();
        ASSERT_TRUE(m_Recording.m_StopResult);
        InspectClientArea();
    }
}

// A resized WGC frame must be discarded and the original-size recording can be finalized.
TEST_F(RecordWindowsTest, ResizeStopsCapture)
{
    DestroyWindow(m_Window);
    CreateTestWindow(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);
    ASSERT_TRUE(Begin(false, "resize", false, DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE));
    DPI_AWARENESS_CONTEXT previous = SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);
    RECT rect = { 0, 0, 480, 360 };
    ASSERT_TRUE(AdjustWindowRectExForDpi(&rect, WS_OVERLAPPEDWINDOW, FALSE, 0, GetDpiForWindow(m_Window)));
    ASSERT_TRUE(SetWindowPos(m_Window, 0, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE));
    SetThreadDpiAwarenessContext(previous);
    dmRecord::Status status;
    uint64_t deadline = dmTime::GetMonotonicTime() + 15000000;
    do
    {
        ResetEvent(g_Painted);
        g_PaintTarget = g_Paints + 1;
        ASSERT_TRUE(PumpUntil(g_Painted));
        dmRecord::GetNativeStatus(&status);
    } while (!status.m_Stopping && dmTime::GetMonotonicTime() < deadline);
    ASSERT_TRUE(status.m_Stopping);
    Finish();
    ASSERT_TRUE(m_Recording.m_StopResult);
    InspectClientArea(1);
}

// Decode both tracks and compare a visible flash with a tone, also exercising repeated start/stop.
TEST_F(RecordWindowsTest, AudioVideoSynchronization)
{
    for (uint32_t i = 0; i < 2; ++i)
    {
        ASSERT_TRUE(Begin(true, i ? "audio-repeat" : "audio"));
        ASSERT_TRUE(m_Recording.m_Status.m_Audio);
        Play(false, 24000);
        Play(true, 48000);
        Play(false, 24000);
        Finish();
        ASSERT_TRUE(m_Recording.m_StopResult);
        ASSERT_TRUE(m_Recording.m_Status.m_Finalized);
        Inspect(true, true);
        InspectClientArea();
    }
}

// A silent application must still produce a decodable, full-length silent AAC track.
TEST_F(RecordWindowsTest, SilentAudio)
{
    ASSERT_TRUE(Begin(true, "silence"));
    Play(false, 48000);
    Finish();
    ASSERT_TRUE(m_Recording.m_StopResult);
    Inspect(true, false);
}

// No render stream at all must still yield silent audio, including shutdown without Stop.
TEST_F(RecordWindowsTest, NoRenderStream)
{
    waveOutClose(m_Output);
    m_Output = 0;
    ASSERT_TRUE(Begin(true, "no-render-stream", true));
    g_PaintTarget = g_Paints + 80;
    ASSERT_TRUE(PumpUntil(g_Painted));
    Finish();
    ASSERT_TRUE(m_Recording.m_StopResult);
    Inspect(true, false);
}

// Failed writer startup must release the initialized audio thread so the next recording can start.
TEST_F(RecordWindowsTest, RetryFailedStart)
{
    ASSERT_FALSE(Begin(true, "missing-directory/output"));
    Finish();
    ASSERT_TRUE(Begin(true, "after-failure"));
    Play(false, 48000);
    Finish();
    ASSERT_TRUE(m_Recording.m_StopResult);
    Inspect(true, false);
}

// Explicit audio=false must retain a video-only MP4 even while the application plays sound.
TEST_F(RecordWindowsTest, VideoOnly)
{
    ASSERT_TRUE(Begin(false, "video-only"));
    ASSERT_FALSE(m_Recording.m_Status.m_Audio);
    Play(true, 24000);
    Finish();
    ASSERT_TRUE(m_Recording.m_StopResult);
    Inspect(false, true);
}

// Static windows and a paused animation must retain their full duration, with or without audio.
TEST_F(RecordWindowsTest, StaticVideoDuration)
{
    const char* suffixes[] = { "static-video", "static-audio", "paused-video", "paused-audio" };
    for (uint32_t i = 0; i < 4; ++i)
    {
        bool audio = (i & 1) != 0;
        g_Animate = (i & 2) != 0;
        ASSERT_TRUE(Begin(audio, suffixes[i], i == 3));
        if (g_Animate)
        {
            ResetEvent(g_Painted);
            g_PaintTarget = g_Paints + 40;
            ASSERT_TRUE(PumpUntil(g_Painted));
        }
        g_Animate = false;
        // Playback completion gives a bounded, measurable static interval without
        // relying on sleeps or forcing any further changes to the window's pixels.
        Play(false, 96000);
        Finish();
        ASSERT_TRUE(m_Recording.m_StopResult);
        Inspect(audio, false, 0);
    }
}

int main(int argc, char** argv)
{
    CoInitializeEx(0, COINIT_MULTITHREADED);
    MFStartup(MF_VERSION);
    WNDCLASSW window_class = {};
    window_class.lpfnWndProc = WindowProc;
    window_class.hInstance = GetModuleHandleW(0);
    window_class.lpszClassName = L"DefoldRecordingTest";
    RegisterClassW(&window_class);
    jc_test_init(&argc, argv);
    int result = jc_test_run_all();
    MFShutdown();
    CoUninitialize();
    return result;
}
