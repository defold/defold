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

#define WIN32_LEAN_AND_MEAN
#undef WINVER
#undef _WIN32_WINNT
#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00
#undef NTDDI_VERSION
#define NTDDI_VERSION 0x0A00000A
#include "record_audio_win32.h"
#include <audioclient.h>
#include <audioclientactivationparams.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>
#include <wrl/implements.h>
#include <new>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

using Microsoft::WRL::ComPtr;

namespace dmRecord
{
    static INIT_ONCE                                g_AudioApisOnce = INIT_ONCE_STATIC_INIT;
    static decltype(&::ActivateAudioInterfaceAsync) pActivateAudioInterfaceAsync;

    static BOOL CALLBACK LoadAudioApis(PINIT_ONCE, PVOID, PVOID*)
    {
        // Process loopback requires build 20348. Resolve the optional entry point
        // separately from video capture so older Windows can still record video.
        typedef LONG(WINAPI * RtlGetVersionFn)(OSVERSIONINFOW*);
        RtlGetVersionFn get_version = (RtlGetVersionFn)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion");
        OSVERSIONINFOW version = {};
        version.dwOSVersionInfoSize = sizeof(version);
        if (!get_version || get_version(&version) != 0 || version.dwBuildNumber < 20348)
        {
            return TRUE;
        }
        wchar_t directory[MAX_PATH];
        wchar_t path[MAX_PATH];
        if (!GetSystemDirectoryW(directory, MAX_PATH))
        {
            return TRUE;
        }
        _snwprintf_s(path, MAX_PATH, _TRUNCATE, L"%s\\mmdevapi.dll", directory);
        HMODULE module = LoadLibraryW(path);
        if (module)
        {
            pActivateAudioInterfaceAsync = (decltype(pActivateAudioInterfaceAsync))GetProcAddress(module, "ActivateAudioInterfaceAsync");
        }
        return TRUE;
    }

    bool IsProcessAudioCaptureSupported()
    {
        InitOnceExecuteOnce(&g_AudioApisOnce, LoadAudioApis, 0, 0);
        return pActivateAudioInterfaceAsync != 0;
    }

    // Own the event and activation data in the completion object, not the
    // recorder: Windows may finish activation after a timeout or shutdown.
    struct AudioActivation : Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IActivateAudioInterfaceCompletionHandler, Microsoft::WRL::FtmBase>
    {
        HANDLE                        m_Completed;
        HRESULT                       m_Result;
        ComPtr<IAudioClient>          m_Client;
        AUDIOCLIENT_ACTIVATION_PARAMS m_Parameters;
        PROPVARIANT                   m_Variant;

        AudioActivation()
            : m_Completed(CreateEventW(0, TRUE, FALSE, 0))
            , m_Result(E_PENDING)
        {
            memset(&m_Parameters, 0, sizeof(m_Parameters));
            m_Parameters.ActivationType = AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
            m_Parameters.ProcessLoopbackParams.TargetProcessId = GetCurrentProcessId();
            m_Parameters.ProcessLoopbackParams.ProcessLoopbackMode = PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;
            memset(&m_Variant, 0, sizeof(m_Variant));
            m_Variant.vt = VT_BLOB;
            m_Variant.blob.cbSize = sizeof(m_Parameters);
            m_Variant.blob.pBlobData = (BYTE*)&m_Parameters;
        }

        ~AudioActivation()
        {
            if (m_Completed)
            {
                CloseHandle(m_Completed);
            }
        }

        IFACEMETHODIMP ActivateCompleted(IActivateAudioInterfaceAsyncOperation* operation) override
        {
            ComPtr<IUnknown> object;
            HRESULT activation_result = E_FAIL;
            m_Result = operation->GetActivateResult(&activation_result, &object);
            if (SUCCEEDED(m_Result))
            {
                m_Result = activation_result;
            }
            if (SUCCEEDED(m_Result))
            {
                m_Result = object.As(&m_Client);
            }
            SetEvent(m_Completed);
            return S_OK;
        }
    };

    struct AudioCapture
    {
        HANDLE               m_Ready;
        HANDLE               m_Start;
        HANDLE               m_Stop;
        HANDLE               m_Thread;
        HRESULT              m_Result;
        AudioCaptureCallback m_Callback;
        void*                m_Context;
    };

    static HRESULT CapturePackets(AudioCapture* capture, IAudioCaptureClient* client, int16_t* copy, uint32_t capacity, bool draining)
    {
        UINT32 frames = 0;
        HRESULT result = client->GetNextPacketSize(&frames);
        while (SUCCEEDED(result) && frames)
        {
            // A slow encoder must not keep Stop waiting for a running producer
            // to empty its queue. The final drain happens after IAudioClient::Stop.
            if (!draining && WaitForSingleObject(capture->m_Stop, 0) == WAIT_OBJECT_0)
            {
                break;
            }
            BYTE* data = 0;
            DWORD flags = 0;
            UINT64 time = 0;
            result = client->GetBuffer(&data, &frames, &flags, 0, &time);
            if (FAILED(result))
            {
                return result;
            }
            if (frames > capacity)
            {
                client->ReleaseBuffer(frames);
                return E_UNEXPECTED;
            }
            if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT))
            {
                memcpy(copy, data, frames * RECORD_AUDIO_CHANNELS * sizeof(int16_t));
            }
            result = client->ReleaseBuffer(frames);
            if (FAILED(result))
            {
                return result;
            }
            // An invalid timestamp cannot be used to synchronize with video.
            // Fail visibly instead of producing an MP4 with an arbitrary offset.
            if (flags & AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR)
            {
                return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
            }
            result = capture->m_Callback(capture->m_Context,
                                         (flags & AUDCLNT_BUFFERFLAGS_SILENT) ? 0 : copy,
                                         frames,
                                         time,
                                         (flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) != 0);
            if (FAILED(result))
            {
                return result;
            }
            result = client->GetNextPacketSize(&frames);
        }
        return result;
    }

    static HRESULT RunCapture(AudioCapture* capture)
    {
        ComPtr<AudioActivation> activation = Microsoft::WRL::Make<AudioActivation>();
        if (!activation || !activation->m_Completed)
        {
            return E_OUTOFMEMORY;
        }
        ComPtr<IActivateAudioInterfaceAsyncOperation> operation;
        HRESULT result = pActivateAudioInterfaceAsync(VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK, __uuidof(IAudioClient), &activation->m_Variant, activation.Get(), &operation);
        if (FAILED(result))
        {
            return result;
        }
        HANDLE activation_events[] = { capture->m_Stop, activation->m_Completed };
        DWORD wait = WaitForMultipleObjects(2, activation_events, FALSE, 15000);
        if (wait != WAIT_OBJECT_0 + 1)
        {
            return HRESULT_FROM_WIN32(wait == WAIT_TIMEOUT ? ERROR_TIMEOUT : ERROR_CANCELLED);
        }
        if (FAILED(activation->m_Result))
        {
            return activation->m_Result;
        }
        ComPtr<IAudioClient> client = activation->m_Client;
        WAVEFORMATEX format = {};
        format.wFormatTag = WAVE_FORMAT_PCM;
        format.nChannels = RECORD_AUDIO_CHANNELS;
        format.nSamplesPerSec = RECORD_AUDIO_RATE;
        format.wBitsPerSample = 16;
        format.nBlockAlign = format.nChannels * sizeof(int16_t);
        format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
        result = client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                    AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM,
                                    0,
                                    0,
                                    &format,
                                    0);
        if (FAILED(result))
        {
            return result;
        }
        ComPtr<IAudioCaptureClient> reader;
        result = client->GetService(IID_PPV_ARGS(&reader));
        if (FAILED(result))
        {
            return result;
        }
        UINT32 capacity = 0;
        result = client->GetBufferSize(&capacity);
        if (FAILED(result))
        {
            return result;
        }
        HANDLE sample_ready = CreateEventW(0, FALSE, FALSE, 0);
        if (!sample_ready)
        {
            return HRESULT_FROM_WIN32(GetLastError());
        }
        int16_t* copy = (int16_t*)malloc((size_t)capacity * format.nBlockAlign);
        result = copy ? client->SetEventHandle(sample_ready) : E_OUTOFMEMORY;
        if (SUCCEEDED(result))
        {
            capture->m_Result = S_OK;
            SetEvent(capture->m_Ready);
            HANDLE start_events[] = { capture->m_Stop, capture->m_Start };
            if (WaitForMultipleObjects(2, start_events, FALSE, INFINITE) == WAIT_OBJECT_0 + 1)
            {
                result = client->Start();
                if (SUCCEEDED(result))
                {
                    HANDLE events[] = { capture->m_Stop, sample_ready };
                    for (;;)
                    {
                        wait = WaitForMultipleObjects(2, events, FALSE, INFINITE);
                        if (wait != WAIT_OBJECT_0 + 1)
                        {
                            break;
                        }
                        result = CapturePackets(capture, reader.Get(), copy, capacity, false);
                        if (FAILED(result))
                        {
                            break;
                        }
                    }
                    HRESULT stopped = client->Stop();
                    if (SUCCEEDED(result))
                    {
                        result = FAILED(stopped) ? stopped : CapturePackets(capture, reader.Get(), copy, capacity, true);
                    }
                }
            }
        }
        free(copy);
        CloseHandle(sample_ready);
        return result;
    }

    static DWORD WINAPI CaptureThread(void* context)
    {
        AudioCapture* capture = (AudioCapture*)context;
        HRESULT initialized = CoInitializeEx(0, COINIT_MULTITHREADED);
        capture->m_Result = FAILED(initialized) ? initialized : RunCapture(capture);
        // Also signal on initialization failure, so NewAudioCapture can return.
        SetEvent(capture->m_Ready);
        if (SUCCEEDED(initialized))
        {
            CoUninitialize();
        }
        return 0;
    }

    HRESULT NewAudioCapture(AudioCapture** output, AudioCaptureCallback callback, void* context)
    {
        *output = 0;
        if (!IsProcessAudioCaptureSupported())
        {
            return E_NOTIMPL;
        }
        AudioCapture* capture = new (std::nothrow) AudioCapture();
        if (!capture)
        {
            return E_OUTOFMEMORY;
        }
        capture->m_Ready = CreateEventW(0, TRUE, FALSE, 0);
        capture->m_Start = CreateEventW(0, TRUE, FALSE, 0);
        capture->m_Stop = CreateEventW(0, TRUE, FALSE, 0);
        capture->m_Result = E_PENDING;
        capture->m_Callback = callback;
        capture->m_Context = context;
        HRESULT result = E_OUTOFMEMORY;
        if (capture->m_Ready && capture->m_Start && capture->m_Stop)
        {
            capture->m_Thread = CreateThread(0, 0, CaptureThread, capture, 0, 0);
            if (capture->m_Thread)
            {
                WaitForSingleObject(capture->m_Ready, INFINITE);
                result = capture->m_Result;
            }
        }
        if (FAILED(result))
        {
            DeleteAudioCapture(capture);
        }
        else
        {
            *output = capture;
        }
        return result;
    }

    void StartAudioCapture(AudioCapture* capture)
    {
        SetEvent(capture->m_Start);
    }

    HRESULT GetAudioCaptureResult(AudioCapture* capture)
    {
        return capture && WaitForSingleObject(capture->m_Thread, 0) == WAIT_OBJECT_0 ? capture->m_Result : S_OK;
    }

    HRESULT DeleteAudioCapture(AudioCapture* capture)
    {
        if (!capture)
        {
            return S_OK;
        }
        if (capture->m_Stop)
        {
            SetEvent(capture->m_Stop);
        }
        if (capture->m_Thread)
        {
            WaitForSingleObject(capture->m_Thread, INFINITE);
            CloseHandle(capture->m_Thread);
        }
        HRESULT result = capture->m_Result;
        if (capture->m_Ready)
        {
            CloseHandle(capture->m_Ready);
        }
        if (capture->m_Start)
        {
            CloseHandle(capture->m_Start);
        }
        if (capture->m_Stop)
        {
            CloseHandle(capture->m_Stop);
        }
        delete capture;
        return result;
    }
}
