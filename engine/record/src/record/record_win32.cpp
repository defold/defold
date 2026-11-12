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

#if defined(DM_PLATFORM_WINDOWS)

#define WIN32_LEAN_AND_MEAN
// Defold's Windows compiler baseline is Vista, while Graphics Capture and the
// Media Foundation sink writer declarations are gated behind newer SDK targets.
#undef WINVER
#undef _WIN32_WINNT
#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <dwmapi.h>
#include <d3d11.h>
#include <dxgi.h>
#include <mfobjects.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <roapi.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.capture.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <wrl/client.h>
#include <wrl/event.h>
#include <wrl/implements.h>

#include "record_audio_win32.h"
#include "record_timeline.h"

#include <dmsdk/dlib/time.h>

#include <dlib/math.h>
#include <new>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

using Microsoft::WRL::ComPtr;

namespace CaptureAbi = ABI::Windows::Graphics::Capture;
namespace Direct3DAbi = ABI::Windows::Graphics::DirectX::Direct3D11;
namespace Direct3DInterop = Windows::Graphics::DirectX::Direct3D11;
typedef __FITypedEventHandler_2_Windows__CGraphics__CCapture__CDirect3D11CaptureFramePool_IInspectable FrameArrivedHandler;

struct AgileFrameArrivedHandler
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::WinRtClassicComMix | Microsoft::WRL::InhibitRoOriginateError>,
          FrameArrivedHandler,
          Microsoft::WRL::FtmBase>
{
public:
    // A delegate needs no runtime class name. Avoid WRL's InspectableClass macro,
    // which introduces a mandatory WindowsCreateString import on older Windows.
    IFACEMETHODIMP GetRuntimeClassName(HSTRING* name) override
    {
        if (!name)
        {
            return E_POINTER;
        }
        *name = 0;
        return S_OK;
    }

    IFACEMETHODIMP GetTrustLevel(TrustLevel* level) override
    {
        if (!level)
        {
            return E_POINTER;
        }
        *level = BaseTrust;
        return S_OK;
    }

    typedef void (*Callback)(void*, CaptureAbi::IDirect3D11CaptureFramePool*);

    HRESULT RuntimeClassInitialize(void* context, Callback callback)
    {
        m_Context = context;
        m_Callback = callback;
        return S_OK;
    }

    IFACEMETHODIMP Invoke(CaptureAbi::IDirect3D11CaptureFramePool* sender,
                          IInspectable*) override
    {
        m_Callback(m_Context, sender);
        return S_OK;
    }

private:
    void*    m_Context;
    Callback m_Callback;
};

namespace dmRecord
{
    // Resolve newer recording APIs on demand. Stock debug engines retain their
    // existing Windows baseline even when these optional DLLs are unavailable.
    static INIT_ONCE                           g_RecorderApisOnce = INIT_ONCE_STATIC_INIT;
    static bool                                g_RecorderApisAvailable;
    static decltype(&::RoInitialize)           pRoInitialize;
    static decltype(&::RoUninitialize)         pRoUninitialize;
    static decltype(&::RoGetActivationFactory) pRoGetActivationFactory;
    static decltype(&::WindowsCreateString)    pWindowsCreateString;
    static decltype(&::WindowsDeleteString)    pWindowsDeleteString;
    static decltype(&::D3D11CreateDevice)      pD3D11CreateDevice;
    typedef HRESULT(WINAPI* CreateDirect3DDeviceFn)(IDXGIDevice*, IInspectable**);
    static CreateDirect3DDeviceFn                    pCreateDirect3D11DeviceFromDXGIDevice;
    static decltype(&::MFStartup)                    pMFStartup;
    static decltype(&::MFShutdown)                   pMFShutdown;
    static decltype(&::MFCreateAttributes)           pMFCreateAttributes;
    static decltype(&::MFCreateMediaType)            pMFCreateMediaType;
    static decltype(&::MFCreateMemoryBuffer)         pMFCreateMemoryBuffer;
    static decltype(&::MFCreateSample)               pMFCreateSample;
    static decltype(&::MFCreateSinkWriterFromURL)    pMFCreateSinkWriterFromURL;
    static decltype(&::SetThreadDpiAwarenessContext) pSetThreadDpiAwarenessContext;
    static BOOL CALLBACK LoadRecorderApis(PINIT_ONCE, PVOID, PVOID*)
    {
        wchar_t system_directory[MAX_PATH];
        if (!GetSystemDirectoryW(system_directory, MAX_PATH))
        {
            return TRUE;
        }
        bool available = true;
        wchar_t path0[MAX_PATH];
        _snwprintf_s(path0, MAX_PATH, _TRUNCATE, L"%s\\combase.dll", system_directory);
        HMODULE module0 = LoadLibraryW(path0);
        pRoInitialize = module0 ? (decltype(pRoInitialize))GetProcAddress(module0, "RoInitialize") : 0;
        available = available && pRoInitialize;
        pRoUninitialize = module0 ? (decltype(pRoUninitialize))GetProcAddress(module0, "RoUninitialize") : 0;
        available = available && pRoUninitialize;
        pRoGetActivationFactory = module0 ? (decltype(pRoGetActivationFactory))GetProcAddress(module0, "RoGetActivationFactory") : 0;
        available = available && pRoGetActivationFactory;
        pWindowsCreateString = module0 ? (decltype(pWindowsCreateString))GetProcAddress(module0, "WindowsCreateString") : 0;
        available = available && pWindowsCreateString;
        pWindowsDeleteString = module0 ? (decltype(pWindowsDeleteString))GetProcAddress(module0, "WindowsDeleteString") : 0;
        available = available && pWindowsDeleteString;
        wchar_t path1[MAX_PATH];
        _snwprintf_s(path1, MAX_PATH, _TRUNCATE, L"%s\\d3d11.dll", system_directory);
        HMODULE module1 = LoadLibraryW(path1);
        pD3D11CreateDevice = module1 ? (decltype(pD3D11CreateDevice))GetProcAddress(module1, "D3D11CreateDevice") : 0;
        available = available && pD3D11CreateDevice;
        pCreateDirect3D11DeviceFromDXGIDevice = module1 ? (decltype(pCreateDirect3D11DeviceFromDXGIDevice))GetProcAddress(module1, "CreateDirect3D11DeviceFromDXGIDevice") : 0;
        available = available && pCreateDirect3D11DeviceFromDXGIDevice;
        wchar_t path2[MAX_PATH];
        _snwprintf_s(path2, MAX_PATH, _TRUNCATE, L"%s\\mfplat.dll", system_directory);
        HMODULE module2 = LoadLibraryW(path2);
        pMFStartup = module2 ? (decltype(pMFStartup))GetProcAddress(module2, "MFStartup") : 0;
        available = available && pMFStartup;
        pMFShutdown = module2 ? (decltype(pMFShutdown))GetProcAddress(module2, "MFShutdown") : 0;
        available = available && pMFShutdown;
        pMFCreateAttributes = module2 ? (decltype(pMFCreateAttributes))GetProcAddress(module2, "MFCreateAttributes") : 0;
        available = available && pMFCreateAttributes;
        pMFCreateMediaType = module2 ? (decltype(pMFCreateMediaType))GetProcAddress(module2, "MFCreateMediaType") : 0;
        available = available && pMFCreateMediaType;
        pMFCreateMemoryBuffer = module2 ? (decltype(pMFCreateMemoryBuffer))GetProcAddress(module2, "MFCreateMemoryBuffer") : 0;
        available = available && pMFCreateMemoryBuffer;
        pMFCreateSample = module2 ? (decltype(pMFCreateSample))GetProcAddress(module2, "MFCreateSample") : 0;
        available = available && pMFCreateSample;
        wchar_t path3[MAX_PATH];
        _snwprintf_s(path3, MAX_PATH, _TRUNCATE, L"%s\\mfreadwrite.dll", system_directory);
        HMODULE module3 = LoadLibraryW(path3);
        pMFCreateSinkWriterFromURL = module3 ? (decltype(pMFCreateSinkWriterFromURL))GetProcAddress(module3, "MFCreateSinkWriterFromURL") : 0;
        available = available && pMFCreateSinkWriterFromURL;
        HMODULE user32 = GetModuleHandleW(L"user32.dll");
        pSetThreadDpiAwarenessContext = user32 ? (decltype(pSetThreadDpiAwarenessContext))GetProcAddress(user32, "SetThreadDpiAwarenessContext") : 0;
        available = available && pSetThreadDpiAwarenessContext;
        g_RecorderApisAvailable = available;
        return TRUE;
    }

    static bool RecorderApisAvailable()
    {
        InitOnceExecuteOnce(&g_RecorderApisOnce, LoadRecorderApis, 0, 0);
        return g_RecorderApisAvailable;
    }

#define RoInitialize pRoInitialize
#define RoUninitialize pRoUninitialize
#define RoGetActivationFactory pRoGetActivationFactory
#define WindowsCreateString pWindowsCreateString
#define WindowsDeleteString pWindowsDeleteString
#define D3D11CreateDevice pD3D11CreateDevice
#define CreateDirect3D11DeviceFromDXGIDevice pCreateDirect3D11DeviceFromDXGIDevice
#define MFStartup pMFStartup
#define MFShutdown pMFShutdown
#define MFCreateAttributes pMFCreateAttributes
#define MFCreateMediaType pMFCreateMediaType
#define MFCreateMemoryBuffer pMFCreateMemoryBuffer
#define MFCreateSample pMFCreateSample
#define MFCreateSinkWriterFromURL pMFCreateSinkWriterFromURL

    static SRWLOCK g_StateLock = SRWLOCK_INIT;
    static Status  g_Status;

    static void CopyText(char* output, size_t output_size, const char* value)
    {
        CopyTextUtf8(output, output_size, value);
    }

    static void HResultText(HRESULT result, char* output, size_t output_size)
    {
        wchar_t* system_message = 0;
        DWORD length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                                          FORMAT_MESSAGE_IGNORE_INSERTS,
                                      0,
                                      (DWORD)result,
                                      0,
                                      (wchar_t*)&system_message,
                                      0,
                                      0);
        if (length && system_message)
        {
            while (length && (system_message[length - 1] == L'\r' || system_message[length - 1] == L'\n'))
            {
                system_message[--length] = 0;
            }
            int utf8_size = WideCharToMultiByte(CP_UTF8, 0, system_message, -1, 0, 0, 0, 0);
            char* utf8 = utf8_size > 0 ? new (std::nothrow) char[utf8_size] : 0;
            bool converted = utf8 && WideCharToMultiByte(CP_UTF8, 0, system_message, -1, utf8, utf8_size, 0, 0) > 0;
            LocalFree(system_message);
            char code[16];
            snprintf(code, sizeof(code), " (0x%08lx)", (unsigned long)result);
            size_t code_length = strlen(code);
            if (converted && output_size > code_length)
            {
                // Keep the HRESULT even when a localized description is too long.
                CopyTextUtf8(output, output_size - code_length, utf8);
                memcpy(output + strlen(output), code, code_length + 1);
            }
            delete[] utf8;
            if (converted && output_size > code_length)
            {
                return;
            }
        }
        snprintf(output, output_size, "Windows error 0x%08lx", (unsigned long)result);
    }

    static bool Utf8ToWide(const char* input, wchar_t* output, uint32_t output_count)
    {
        if (!input || !input[0])
        {
            return false;
        }
        int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input, -1, output, (int)output_count);
        return count > 0;
    }

    static HRESULT GetActivationFactory(const wchar_t* class_name, REFIID interface_id, void** output)
    {
        HSTRING name = 0;
        HRESULT result = WindowsCreateString(class_name, (UINT32)wcslen(class_name), &name);
        if (SUCCEEDED(result))
        {
            result = RoGetActivationFactory(name, interface_id, output);
        }
        if (name)
        {
            WindowsDeleteString(name);
        }
        return result;
    }

    static void CloseInspectable(IInspectable* object)
    {
        if (!object)
        {
            return;
        }
        ComPtr<ABI::Windows::Foundation::IClosable> closable;
        if (SUCCEEDED(object->QueryInterface(IID_PPV_ARGS(&closable))))
        {
            closable->Close();
        }
    }

    static bool CaptureFactoriesAvailable()
    {
        // Session IsSupported() predates HWND capture and free-threaded frame
        // pools. Probe the interfaces used below before advertising support.
        ComPtr<IGraphicsCaptureItemInterop> item;
        HRESULT result = GetActivationFactory(
            RuntimeClass_Windows_Graphics_Capture_GraphicsCaptureItem,
            __uuidof(IGraphicsCaptureItemInterop),
            reinterpret_cast<void**>(item.ReleaseAndGetAddressOf()));
        if (FAILED(result))
        {
            return false;
        }
        ComPtr<CaptureAbi::IDirect3D11CaptureFramePoolStatics2> pool;
        result = GetActivationFactory(
            RuntimeClass_Windows_Graphics_Capture_Direct3D11CaptureFramePool,
            __uuidof(CaptureAbi::IDirect3D11CaptureFramePoolStatics2),
            reinterpret_cast<void**>(pool.ReleaseAndGetAddressOf()));
        return SUCCEEDED(result);
    }

    static bool GraphicsCaptureSupported()
    {
        if (!CaptureFactoriesAvailable())
        {
            return false;
        }
        ComPtr<CaptureAbi::IGraphicsCaptureSessionStatics> statics;
        HRESULT result = GetActivationFactory(
            RuntimeClass_Windows_Graphics_Capture_GraphicsCaptureSession,
            __uuidof(CaptureAbi::IGraphicsCaptureSessionStatics),
            reinterpret_cast<void**>(statics.ReleaseAndGetAddressOf()));
        boolean supported = FALSE;
        return SUCCEEDED(result) && SUCCEEDED(statics->IsSupported(&supported)) && supported;
    }

    bool GetWindow(void* native_window, Window* target)
    {
        target->m_Handle = (uintptr_t)native_window;
        return target->m_Handle != 0;
    }

    bool PrepareWindow(void* native_window, Window* target)
    {
        return GetWindow(native_window, target);
    }

    void RestoreWindow() {}

    struct WindowsRecorder
    {
    public:
        WindowsRecorder()
            : m_Window(0)
            , m_CornerPreference(0)
            , m_RestoreCornerPreference(false)
            , m_OutputWidth(0)
            , m_OutputHeight(0)
            , m_Fps(0)
            , m_CropX(0)
            , m_CropY(0)
            , m_CropWidth(0)
            , m_CropHeight(0)
            , m_CaptureSize()
            , m_StreamIndex(0)
            , m_AudioStreamIndex(0)
            , m_AudioCapture(0)
            , m_AudioFrames(0)
            , m_StartTime(0)
            , m_StopTime(UINT64_MAX)
            , m_FrameCount(0)
            , m_LastFrameSlot(UINT64_MAX)
            , m_Stopping(0)
            , m_MfStarted(false)
            , m_RoInitialized(false)
            , m_FirstFrameEvent(0)
        {
            InitializeCriticalSection(&m_FrameLock);
            InitializeCriticalSection(&m_WriterLock);
            InitializeSRWLock(&m_FailureLock);
            m_FirstFrameEvent = CreateEventW(0, TRUE, FALSE, 0);
            m_Failure[0] = 0;
        }

        ~WindowsRecorder()
        {
            Cleanup(false);
            if (m_FirstFrameEvent)
            {
                CloseHandle(m_FirstFrameEvent);
            }
            DeleteCriticalSection(&m_FrameLock);
            DeleteCriticalSection(&m_WriterLock);
        }

        bool Start(HWND window, const char* path, uint32_t fps, bool audio, char* failure, size_t failure_size)
        {
            if (!RecorderApisAvailable())
            {
                return Fail(E_NOTIMPL, failure, failure_size);
            }
            HRESULT result = RoInitialize(RO_INIT_MULTITHREADED);
            if (SUCCEEDED(result))
            {
                m_RoInitialized = true;
            }
            else if (result != RPC_E_CHANGED_MODE)
            {
                return Fail(result, failure, failure_size);
            }

            if (!GraphicsCaptureSupported())
            {
                CopyText(failure, failure_size, "Windows Graphics Capture is not supported by this runtime");
                return false;
            }

            if (!window)
            {
                CopyText(failure, failure_size, "engine window is unavailable");
                return false;
            }
            m_Window = window;

            // Windows 11 otherwise masks the client pixels at the bottom corners.
            // Disable rounding only while recording and restore the prior preference.
            const DWORD corner_attribute = 33; // DWMWA_WINDOW_CORNER_PREFERENCE
            if (SUCCEEDED(DwmGetWindowAttribute(m_Window, corner_attribute, &m_CornerPreference, sizeof(m_CornerPreference))))
            {
                DWORD do_not_round = 1; // DWMWCP_DONOTROUND
                if (SUCCEEDED(DwmSetWindowAttribute(m_Window, corner_attribute, &do_not_round, sizeof(do_not_round))))
                {
                    m_RestoreCornerPreference = true;
                }
            }

            result = CreateCaptureItem();
            if (FAILED(result))
            {
                return Fail(result, failure, failure_size);
            }
            result = m_Item->get_Size(&m_CaptureSize);
            if (FAILED(result))
            {
                return Fail(result, failure, failure_size);
            }
            if (!ResolveClientCrop(m_CaptureSize))
            {
                CopyText(failure, failure_size, "could not resolve the Defold window client area inside the capture surface");
                return false;
            }

            // The Media Foundation H.264 encoder requires even dimensions.
            m_OutputWidth = m_CropWidth & ~1U;
            m_OutputHeight = m_CropHeight & ~1U;
            if (!m_OutputWidth || !m_OutputHeight)
            {
                CopyText(failure, failure_size, "recording requires a client area of at least 2x2 pixels");
                return false;
            }
            m_Fps = fps;
            if (!Utf8ToWide(path, m_OutputPath, sizeof(m_OutputPath) / sizeof(m_OutputPath[0])))
            {
                CopyText(failure, failure_size, "recording path is not valid UTF-8 or is too long");
                return false;
            }
            DeleteFileW(m_OutputPath);

            result = CreateDevice();
            if (FAILED(result))
            {
                return Fail(result, failure, failure_size);
            }
            if (audio)
            {
                result = NewAudioCapture(&m_AudioCapture, DispatchAudio, this);
                if (FAILED(result))
                {
                    return Fail(result, failure, failure_size);
                }
            }
            result = CreateWriter(audio);
            if (FAILED(result))
            {
                return Fail(result, failure, failure_size);
            }

            result = CreateFramePool();
            if (FAILED(result))
            {
                return Fail(result, failure, failure_size);
            }
            m_StartTime = GetCounterTime();
            if (m_AudioCapture)
            {
                StartAudioCapture(m_AudioCapture);
            }
            result = m_Session->StartCapture();
            if (FAILED(result))
            {
                return Fail(result, failure, failure_size);
            }

            DWORD wait_result = WaitForSingleObject(m_FirstFrameEvent, 15000);
            if (wait_result != WAIT_OBJECT_0)
            {
                CopyText(failure, failure_size, "timed out waiting for the first Windows capture frame");
                return false;
            }
            EnterCriticalSection(&m_FrameLock);
            bool ok = m_Failure[0] == 0 && m_FrameCount > 0;
            CopyText(failure, failure_size, m_Failure);
            LeaveCriticalSection(&m_FrameLock);
            result = GetAudioCaptureResult(m_AudioCapture);
            if (FAILED(result))
            {
                return Fail(result, failure, failure_size);
            }
            return ok;
        }

        bool Stop(char* failure, size_t failure_size)
        {
            EnterCriticalSection(&m_WriterLock);
            m_StopTime = GetCounterTime() - m_StartTime;
            LeaveCriticalSection(&m_WriterLock);
            InterlockedExchange(&m_Stopping, 1);
            if (m_FramePool && m_FrameHandler)
            {
                m_FramePool->remove_FrameArrived(m_FrameToken);
            }
            CloseInspectable(m_Session.Get());
            CloseInspectable(m_FramePool.Get());
            m_FrameHandler.Reset();
            m_Session.Reset();
            m_FramePool.Reset();

            // FramePool is free-threaded. Taking this lock after closing it waits
            // for an in-flight callback before the sink writer is finalized.
            EnterCriticalSection(&m_FrameLock);
            if (m_Failure[0] && !failure[0])
            {
                CopyText(failure, failure_size, m_Failure);
            }
            LeaveCriticalSection(&m_FrameLock);

            bool audio = m_AudioCapture != 0;
            HRESULT result = DeleteAudioCapture(m_AudioCapture);
            m_AudioCapture = 0;
            if (FAILED(result) && !failure[0])
            {
                HResultText(result, failure, failure_size);
            }
            // A silent application still needs a complete AAC track. Also pad
            // the final partial capture period up to the common stop boundary.
            if (audio && SUCCEEDED(result))
            {
                uint64_t end = TimeToFrames(m_StopTime, RECORD_AUDIO_RATE);
                if (end > m_AudioFrames)
                {
                    result = WriteSilence(end - m_AudioFrames);
                }
                if (FAILED(result) && !failure[0])
                {
                    HResultText(result, failure, failure_size);
                }
            }
            // WGC only delivers changed content. Keep the final image visible
            // through the same stop boundary as audio, including video-only files.
            result = FlushVideoFrame(m_StopTime);
            if (FAILED(result) && !failure[0])
            {
                HResultText(result, failure, failure_size);
            }
            if (m_Writer)
            {
                result = m_Writer->Finalize();
            }
            if (FAILED(result) && !failure[0])
            {
                HResultText(result, failure, failure_size);
            }

            Cleanup(false);
            WIN32_FILE_ATTRIBUTE_DATA attributes;
            bool finalized = failure[0] == 0 &&
                GetFileAttributesExW(m_OutputPath, GetFileExInfoStandard, &attributes) &&
                (attributes.nFileSizeHigh != 0 || attributes.nFileSizeLow != 0);
            if (!finalized && !failure[0])
            {
                CopyText(failure, failure_size, "Media Foundation did not produce a non-empty MP4 file");
            }
            return finalized;
        }

        uint32_t Width() const
        {
            return m_OutputWidth;
        }
        uint32_t Height() const
        {
            return m_OutputHeight;
        }
        uint32_t Fps() const
        {
            return m_Fps;
        }

        bool IsStopping()
        {
            return InterlockedCompareExchange(&m_Stopping, 0, 0) != 0;
        }

        void GetFailure(char* failure, size_t failure_size)
        {
            // Status is queried from the engine thread; never wait for GPU
            // readback or encoding while copying the asynchronous error.
            AcquireSRWLockShared(&m_FailureLock);
            CopyText(failure, failure_size, m_Failure);
            ReleaseSRWLockShared(&m_FailureLock);
            HRESULT result = GetAudioCaptureResult(m_AudioCapture);
            if (FAILED(result) && !failure[0])
            {
                HResultText(result, failure, failure_size);
            }
        }

    private:
        static uint64_t GetCounterTime()
        {
            LARGE_INTEGER counter, frequency;
            QueryPerformanceCounter(&counter);
            QueryPerformanceFrequency(&frequency);
            return counter.QuadPart / frequency.QuadPart * 10000000 +
                counter.QuadPart % frequency.QuadPart * 10000000 / frequency.QuadPart;
        }

        bool Fail(HRESULT result, char* failure, size_t failure_size)
        {
            HResultText(result, failure, failure_size);
            return false;
        }

        HRESULT CreateCaptureItem()
        {
            ComPtr<IGraphicsCaptureItemInterop> interop;
            HRESULT result = GetActivationFactory(
                RuntimeClass_Windows_Graphics_Capture_GraphicsCaptureItem,
                __uuidof(IGraphicsCaptureItemInterop),
                reinterpret_cast<void**>(interop.ReleaseAndGetAddressOf()));
            if (FAILED(result))
            {
                return result;
            }
            return interop->CreateForWindow(m_Window,
                                            __uuidof(CaptureAbi::IGraphicsCaptureItem),
                                            reinterpret_cast<void**>(m_Item.ReleaseAndGetAddressOf()));
        }

        bool ResolveClientCrop(const ABI::Windows::Graphics::SizeInt32& capture_size)
        {
            // WGC textures and DWM frame bounds are physical pixels. Query the
            // client rectangle in that same space even when the caller is DPI
            // unaware or system-aware on a monitor with a different scale.
            DPI_AWARENESS_CONTEXT previous = pSetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);
            if (!previous)
            {
                return false;
            }
            RECT window_rect;
            RECT client_rect;
            POINT client_origin = { 0, 0 };
            bool have_rects = (SUCCEEDED(DwmGetWindowAttribute(m_Window, DWMWA_EXTENDED_FRAME_BOUNDS, &window_rect, sizeof(window_rect))) ||
                               GetWindowRect(m_Window, &window_rect)) &&
                GetClientRect(m_Window, &client_rect) && ClientToScreen(m_Window, &client_origin);
            pSetThreadDpiAwarenessContext(previous);
            if (!have_rects)
            {
                return false;
            }

            LONG window_width = window_rect.right - window_rect.left;
            LONG window_height = window_rect.bottom - window_rect.top;
            LONG client_width = client_rect.right - client_rect.left;
            LONG client_height = client_rect.bottom - client_rect.top;
            if (capture_size.Width <= 0 || capture_size.Height <= 0 ||
                window_width <= 0 || window_height <= 0 || client_width <= 0 || client_height <= 0)
            {
                return false;
            }

            double scale_x = (double)capture_size.Width / window_width;
            double scale_y = (double)capture_size.Height / window_height;
            m_CropX = (uint32_t)dmMath::Max(0.0, (client_origin.x - window_rect.left) * scale_x + 0.5);
            m_CropY = (uint32_t)dmMath::Max(0.0, (client_origin.y - window_rect.top) * scale_y + 0.5);
            m_CropWidth = (uint32_t)dmMath::Max(1.0, client_width * scale_x + 0.5);
            m_CropHeight = (uint32_t)dmMath::Max(1.0, client_height * scale_y + 0.5);
            if (m_CropX >= (uint32_t)capture_size.Width || m_CropY >= (uint32_t)capture_size.Height)
            {
                return false;
            }
            m_CropWidth = dmMath::Min(m_CropWidth, (uint32_t)capture_size.Width - m_CropX);
            m_CropHeight = dmMath::Min(m_CropHeight, (uint32_t)capture_size.Height - m_CropY);
            return true;
        }

        HRESULT CreateDevice()
        {
            UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
            D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
            D3D_FEATURE_LEVEL selected;
            HRESULT result = D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE, 0, flags, levels, sizeof(levels) / sizeof(levels[0]), D3D11_SDK_VERSION, &m_Device, &selected, &m_Context);
            if (FAILED(result))
            {
                return result;
            }
            ComPtr<IDXGIDevice> dxgi_device;
            result = m_Device.As(&dxgi_device);
            if (FAILED(result))
            {
                return result;
            }
            ComPtr<IInspectable> inspectable;
            result = CreateDirect3D11DeviceFromDXGIDevice(dxgi_device.Get(), inspectable.ReleaseAndGetAddressOf());
            if (FAILED(result))
            {
                return result;
            }
            return inspectable.As(&m_Direct3DDevice);
        }

        HRESULT CreateFramePool()
        {
            ComPtr<CaptureAbi::IDirect3D11CaptureFramePoolStatics2> statics;
            HRESULT result = GetActivationFactory(
                RuntimeClass_Windows_Graphics_Capture_Direct3D11CaptureFramePool,
                __uuidof(CaptureAbi::IDirect3D11CaptureFramePoolStatics2),
                reinterpret_cast<void**>(statics.ReleaseAndGetAddressOf()));
            if (FAILED(result))
            {
                return result;
            }
            result = statics->CreateFreeThreaded(
                m_Direct3DDevice.Get(),
                ABI::Windows::Graphics::DirectX::DirectXPixelFormat_B8G8R8A8UIntNormalized,
                3,
                m_CaptureSize,
                &m_FramePool);
            if (FAILED(result))
            {
                return result;
            }
            result = m_FramePool->CreateCaptureSession(m_Item.Get(), &m_Session);
            if (FAILED(result))
            {
                return result;
            }
            ComPtr<AgileFrameArrivedHandler> frame_handler =
                Microsoft::WRL::Make<AgileFrameArrivedHandler>();
            if (!frame_handler)
            {
                return E_OUTOFMEMORY;
            }
            result = frame_handler->RuntimeClassInitialize(this, &WindowsRecorder::DispatchFrame);
            if (FAILED(result))
            {
                return result;
            }
            m_FrameHandler = frame_handler;
            return m_FramePool->add_FrameArrived(m_FrameHandler.Get(), &m_FrameToken);
        }

        static void DispatchFrame(void*                                    context,
                                  CaptureAbi::IDirect3D11CaptureFramePool* sender)
        {
            static_cast<WindowsRecorder*>(context)->OnFrame(sender);
        }

        HRESULT SetMediaAttributeSize(IMFAttributes* attributes, REFGUID key, uint32_t width, uint32_t height)
        {
            return MFSetAttributeSize(attributes, key, width, height);
        }

        HRESULT CreateWriter(bool audio)
        {
            HRESULT result = MFStartup(MF_VERSION, MFSTARTUP_FULL);
            if (FAILED(result))
            {
                return result;
            }
            m_MfStarted = true;

            ComPtr<IMFAttributes> attributes;
            result = MFCreateAttributes(&attributes, 2);
            if (FAILED(result))
            {
                return result;
            }
            attributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
            attributes->SetUINT32(MF_SINK_WRITER_DISABLE_THROTTLING, TRUE);
            result = MFCreateSinkWriterFromURL(m_OutputPath, 0, attributes.Get(), &m_Writer);
            if (FAILED(result))
            {
                return result;
            }

            ComPtr<IMFMediaType> output_type;
            result = MFCreateMediaType(&output_type);
            if (FAILED(result))
            {
                return result;
            }
            if (FAILED(result = output_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video)))
            {
                return result;
            }
            if (FAILED(result = output_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264)))
            {
                return result;
            }
            uint64_t pixels_per_second = (uint64_t)m_OutputWidth * m_OutputHeight * m_Fps;
            uint32_t bitrate = (uint32_t)dmMath::Min<uint64_t>(25000000, dmMath::Max<uint64_t>(1000000, pixels_per_second / 8));
            if (FAILED(result = output_type->SetUINT32(MF_MT_AVG_BITRATE, bitrate)))
            {
                return result;
            }
            if (FAILED(result = output_type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive)))
            {
                return result;
            }
            if (FAILED(result = SetMediaAttributeSize(output_type.Get(), MF_MT_FRAME_SIZE, m_OutputWidth, m_OutputHeight)))
            {
                return result;
            }
            if (FAILED(result = MFSetAttributeRatio(output_type.Get(), MF_MT_FRAME_RATE, m_Fps, 1)))
            {
                return result;
            }
            if (FAILED(result = MFSetAttributeRatio(output_type.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1)))
            {
                return result;
            }
            if (FAILED(result = m_Writer->AddStream(output_type.Get(), &m_StreamIndex)))
            {
                return result;
            }

            ComPtr<IMFMediaType> input_type;
            result = MFCreateMediaType(&input_type);
            if (FAILED(result))
            {
                return result;
            }
            if (FAILED(result = input_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video)))
            {
                return result;
            }
            if (FAILED(result = input_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32)))
            {
                return result;
            }
            if (FAILED(result = input_type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive)))
            {
                return result;
            }
            if (FAILED(result = SetMediaAttributeSize(input_type.Get(), MF_MT_FRAME_SIZE, m_OutputWidth, m_OutputHeight)))
            {
                return result;
            }
            if (FAILED(result = MFSetAttributeRatio(input_type.Get(), MF_MT_FRAME_RATE, m_Fps, 1)))
            {
                return result;
            }
            if (FAILED(result = MFSetAttributeRatio(input_type.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1)))
            {
                return result;
            }
            if (FAILED(result = m_Writer->SetInputMediaType(m_StreamIndex, input_type.Get(), 0)))
            {
                return result;
            }
            if (audio && FAILED(result = CreateAudioStream()))
            {
                return result;
            }
            return m_Writer->BeginWriting();
        }

        HRESULT CreateAudioStream()
        {
            ComPtr<IMFMediaType> output;
            HRESULT result = MFCreateMediaType(&output);
            if (FAILED(result))
            {
                return result;
            }
            if (FAILED(result = output->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio)) ||
                FAILED(result = output->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC)) ||
                FAILED(result = output->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, RECORD_AUDIO_CHANNELS)) ||
                FAILED(result = output->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, RECORD_AUDIO_RATE)) ||
                FAILED(result = output->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16)) ||
                FAILED(result = output->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 16000)) ||
                FAILED(result = m_Writer->AddStream(output.Get(), &m_AudioStreamIndex)))
            {
                return result;
            }
            ComPtr<IMFMediaType> input;
            result = MFCreateMediaType(&input);
            if (FAILED(result))
            {
                return result;
            }
            if (FAILED(result = input->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio)) ||
                FAILED(result = input->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM)) ||
                FAILED(result = input->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, RECORD_AUDIO_CHANNELS)) ||
                FAILED(result = input->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, RECORD_AUDIO_RATE)) ||
                FAILED(result = input->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16)) ||
                FAILED(result = input->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, RECORD_AUDIO_CHANNELS * sizeof(int16_t))) ||
                FAILED(result = input->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, RECORD_AUDIO_RATE * RECORD_AUDIO_CHANNELS * sizeof(int16_t))))
            {
                return result;
            }
            return m_Writer->SetInputMediaType(m_AudioStreamIndex, input.Get(), 0);
        }

        HRESULT WriteAudio(const int16_t* samples, uint32_t frames, bool discontinuity)
        {
            DWORD bytes = frames * RECORD_AUDIO_CHANNELS * sizeof(int16_t);
            ComPtr<IMFMediaBuffer> buffer;
            HRESULT result = MFCreateMemoryBuffer(bytes, &buffer);
            if (FAILED(result))
            {
                return result;
            }
            BYTE* data = 0;
            if (FAILED(result = buffer->Lock(&data, 0, 0)))
            {
                return result;
            }
            if (samples)
            {
                memcpy(data, samples, bytes);
            }
            else
            {
                memset(data, 0, bytes);
            }
            buffer->Unlock();
            buffer->SetCurrentLength(bytes);
            ComPtr<IMFSample> sample;
            if (FAILED(result = MFCreateSample(&sample)) || FAILED(result = sample->AddBuffer(buffer.Get())))
            {
                return result;
            }
            uint64_t time = FramesToTime(m_AudioFrames, RECORD_AUDIO_RATE);
            sample->SetSampleTime(time);
            sample->SetSampleDuration(FramesToTime(m_AudioFrames + frames, RECORD_AUDIO_RATE) - time);
            if (discontinuity)
            {
                sample->SetUINT32(MFSampleExtension_Discontinuity, TRUE);
            }
            result = m_Writer->WriteSample(m_AudioStreamIndex, sample.Get());
            if (SUCCEEDED(result))
            {
                m_AudioFrames += frames;
            }
            return result;
        }

        HRESULT WriteSilence(uint64_t frames)
        {
            // Bound allocations even after a long period without any packets.
            while (frames)
            {
                uint32_t count = (uint32_t)dmMath::Min<uint64_t>(frames, RECORD_AUDIO_RATE / 10);
                HRESULT result = WriteAudio(0, count, false);
                if (FAILED(result))
                {
                    return result;
                }
                frames -= count;
            }
            return S_OK;
        }

        static HRESULT DispatchAudio(void* context, const int16_t* samples, uint32_t frames, uint64_t time, bool discontinuity)
        {
            WindowsRecorder* recorder = (WindowsRecorder*)context;
            EnterCriticalSection(&recorder->m_WriterLock);
            uint64_t limit = recorder->m_StopTime == UINT64_MAX ? UINT64_MAX : TimeToFrames(recorder->m_StopTime, RECORD_AUDIO_RATE);
            AudioPacketRange range = GetAudioPacketRange(recorder->m_StartTime, time, frames, RECORD_AUDIO_RATE, recorder->m_AudioFrames, limit);
            HRESULT result = recorder->WriteSilence(range.m_SilenceFrames);
            if (SUCCEEDED(result) && range.m_Frames)
            {
                result = recorder->WriteAudio(samples ? samples + range.m_SkipFrames * RECORD_AUDIO_CHANNELS : 0,
                                              range.m_Frames,
                                              discontinuity || range.m_SilenceFrames != 0);
            }
            LeaveCriticalSection(&recorder->m_WriterLock);
            return result;
        }

        HRESULT EnsureStagingTexture()
        {
            if (m_Staging)
            {
                return S_OK;
            }
            D3D11_TEXTURE2D_DESC description = {};
            description.Width = m_CropWidth;
            description.Height = m_CropHeight;
            description.MipLevels = 1;
            description.ArraySize = 1;
            description.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            description.SampleDesc.Count = 1;
            description.Usage = D3D11_USAGE_STAGING;
            description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            return m_Device->CreateTexture2D(&description, 0, &m_Staging);
        }

        void OnFrame(CaptureAbi::IDirect3D11CaptureFramePool* sender)
        {
            if (InterlockedCompareExchange(&m_Stopping, 0, 0))
            {
                return;
            }
            EnterCriticalSection(&m_FrameLock);
            if (InterlockedCompareExchange(&m_Stopping, 0, 0))
            {
                LeaveCriticalSection(&m_FrameLock);
                return;
            }
            HRESULT result = S_OK;
            ComPtr<CaptureAbi::IDirect3D11CaptureFrame> frame;
            result = sender->TryGetNextFrame(&frame);
            if (SUCCEEDED(result) && !frame)
            {
                LeaveCriticalSection(&m_FrameLock);
                return;
            }
            ABI::Windows::Graphics::SizeInt32 size = {};
            if (SUCCEEDED(result))
            {
                result = frame->get_ContentSize(&size);
            }
            bool valid_size = size.Width > 0 && size.Height > 0;
            bool resized = valid_size && (size.Width != m_CaptureSize.Width || size.Height != m_CaptureSize.Height);
            if (SUCCEEDED(result) && resized)
            {
                InterlockedExchange(&m_Stopping, 1);
                if (!m_FrameCount)
                {
                    AcquireSRWLockExclusive(&m_FailureLock);
                    CopyText(m_Failure, sizeof(m_Failure), "window size changed before the first capture frame");
                    ReleaseSRWLockExclusive(&m_FailureLock);
                }
            }
            else if (SUCCEEDED(result) && valid_size)
            {
                result = ReadFrame(frame.Get());
            }

            // A resized frame belongs to the old pool and may be clipped or
            // undefined. Drop it and let the recording worker finalize the file.
            CloseInspectable(frame.Get());
            frame.Reset();
            if (FAILED(result))
            {
                AcquireSRWLockExclusive(&m_FailureLock);
                if (!m_Failure[0])
                {
                    HResultText(result, m_Failure, sizeof(m_Failure));
                }
                ReleaseSRWLockExclusive(&m_FailureLock);
            }
            if (m_FrameCount > 0 || m_Failure[0])
            {
                SetEvent(m_FirstFrameEvent);
            }
            LeaveCriticalSection(&m_FrameLock);
        }

        HRESULT ReadFrame(CaptureAbi::IDirect3D11CaptureFrame* frame)
        {
            ComPtr<Direct3DAbi::IDirect3DSurface> surface;
            HRESULT result = frame->get_Surface(&surface);
            ComPtr<Direct3DInterop::IDirect3DDxgiInterfaceAccess> access;
            if (SUCCEEDED(result))
            {
                result = surface.As(&access);
            }
            ComPtr<ID3D11Texture2D> texture;
            if (SUCCEEDED(result))
            {
                result = access->GetInterface(IID_PPV_ARGS(&texture));
            }
            ABI::Windows::Foundation::TimeSpan time;
            if (SUCCEEDED(result))
            {
                result = frame->get_SystemRelativeTime(&time);
            }
            if (SUCCEEDED(result))
            {
                result = WriteFrame(texture.Get(), time.Duration);
            }
            return result;
        }

        HRESULT WriteFrame(ID3D11Texture2D* texture, uint64_t time)
        {
            if (time < m_StartTime)
            {
                return S_FALSE;
            }
            uint64_t frame_slot = TimeToFrames(time - m_StartTime, m_Fps);
            if (m_LastFrameSlot != UINT64_MAX && frame_slot <= m_LastFrameSlot)
            {
                return S_FALSE;
            }

            HRESULT result = EnsureStagingTexture();
            if (FAILED(result))
            {
                return result;
            }
            D3D11_TEXTURE2D_DESC source_description;
            texture->GetDesc(&source_description);
            if (m_CropX + m_CropWidth > source_description.Width ||
                m_CropY + m_CropHeight > source_description.Height)
            {
                return E_INVALIDARG;
            }

            D3D11_BOX source_box = { m_CropX, m_CropY, 0, m_CropX + m_CropWidth, m_CropY + m_CropHeight, 1 };
            m_Context->CopySubresourceRegion(m_Staging.Get(), 0, 0, 0, 0, texture, 0, &source_box);
            D3D11_MAPPED_SUBRESOURCE mapped;
            result = m_Context->Map(m_Staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
            if (FAILED(result))
            {
                return result;
            }

            uint64_t buffer_size_64 = (uint64_t)m_OutputWidth * m_OutputHeight * 4;
            if (buffer_size_64 > 0xffffffffU)
            {
                m_Context->Unmap(m_Staging.Get(), 0);
                return E_INVALIDARG;
            }
            ComPtr<IMFMediaBuffer> buffer;
            result = MFCreateMemoryBuffer((DWORD)buffer_size_64, &buffer);
            BYTE* destination = 0;
            if (SUCCEEDED(result))
            {
                result = buffer->Lock(&destination, 0, 0);
            }
            if (SUCCEEDED(result))
            {
                const BYTE* source = (const BYTE*)mapped.pData;
                uint32_t destination_stride = m_OutputWidth * 4;
                for (uint32_t y = 0; y < m_OutputHeight; ++y)
                {
                    // RGB32 is conventionally bottom-up in Media Foundation.
                    BYTE* destination_row = destination + (size_t)(m_OutputHeight - 1 - y) * destination_stride;
                    const BYTE* source_row = source + (size_t)y * mapped.RowPitch;
                    memcpy(destination_row, source_row, destination_stride);
                }
                buffer->Unlock();
                buffer->SetCurrentLength((DWORD)buffer_size_64);
            }
            m_Context->Unmap(m_Staging.Get(), 0);
            if (FAILED(result))
            {
                return result;
            }

            ComPtr<IMFSample> sample;
            result = MFCreateSample(&sample);
            if (FAILED(result))
            {
                return result;
            }
            if (FAILED(result = sample->AddBuffer(buffer.Get())))
            {
                return result;
            }
            uint64_t sample_time = FramesToTime(frame_slot, m_Fps);
            sample->SetSampleTime(sample_time);
            EnterCriticalSection(&m_WriterLock);
            if (time - m_StartTime >= m_StopTime)
            {
                result = S_FALSE;
            }
            else
            {
                // Delay writing one frame until its duration is known. A static
                // image lasts until the next captured image or the stop boundary.
                result = FlushVideoFrame(sample_time);
                if (SUCCEEDED(result))
                {
                    m_PendingVideoSample = sample;
                    m_LastFrameSlot = frame_slot;
                    // A captured frame is enough to complete Start; waiting for
                    // a second frame would time out on a static window.
                    ++m_FrameCount;
                }
            }
            LeaveCriticalSection(&m_WriterLock);
            return result;
        }

        // Called with the writer lock held, or after both capture producers join.
        HRESULT FlushVideoFrame(uint64_t end_time)
        {
            if (!m_PendingVideoSample)
            {
                return S_OK;
            }
            uint64_t time = FramesToTime(m_LastFrameSlot, m_Fps);
            if (end_time <= time)
            {
                return S_FALSE;
            }
            HRESULT result = m_PendingVideoSample->SetSampleDuration(end_time - time);
            if (SUCCEEDED(result))
            {
                result = m_Writer->WriteSample(m_StreamIndex, m_PendingVideoSample.Get());
            }
            if (SUCCEEDED(result))
            {
                m_PendingVideoSample.Reset();
            }
            return result;
        }

        void RestoreWindowCorners()
        {
            if (m_Window && m_RestoreCornerPreference)
            {
                const DWORD corner_attribute = 33;
                DwmSetWindowAttribute(m_Window, corner_attribute, &m_CornerPreference, sizeof(m_CornerPreference));
                m_RestoreCornerPreference = false;
            }
        }

        void Cleanup(bool finalize)
        {
            (void)finalize;
            InterlockedExchange(&m_Stopping, 1);
            if (m_FramePool && m_FrameHandler)
            {
                m_FramePool->remove_FrameArrived(m_FrameToken);
            }
            CloseInspectable(m_Session.Get());
            CloseInspectable(m_FramePool.Get());
            // Join both producers before releasing the writer, including a
            // failed Start after one of the capture streams already began.
            DeleteAudioCapture(m_AudioCapture);
            m_AudioCapture = 0;
            EnterCriticalSection(&m_FrameLock);
            m_FrameHandler.Reset();
            m_Session.Reset();
            m_FramePool.Reset();
            m_Item.Reset();
            m_Direct3DDevice.Reset();
            m_Staging.Reset();
            m_PendingVideoSample.Reset();
            m_Writer.Reset();
            m_Context.Reset();
            m_Device.Reset();
            LeaveCriticalSection(&m_FrameLock);
            if (m_MfStarted)
            {
                MFShutdown();
                m_MfStarted = false;
            }
            RestoreWindowCorners();
            if (m_RoInitialized)
            {
                RoUninitialize();
                m_RoInitialized = false;
            }
        }

        HWND                                            m_Window;
        DWORD                                           m_CornerPreference;
        bool                                            m_RestoreCornerPreference;
        uint32_t                                        m_OutputWidth, m_OutputHeight, m_Fps;
        uint32_t                                        m_CropX, m_CropY, m_CropWidth, m_CropHeight;
        ABI::Windows::Graphics::SizeInt32               m_CaptureSize;
        DWORD                                           m_StreamIndex;
        DWORD                                           m_AudioStreamIndex;
        AudioCapture*                                   m_AudioCapture;
        uint64_t                                        m_AudioFrames;
        uint64_t                                        m_StartTime;
        uint64_t                                        m_StopTime;
        uint64_t                                        m_FrameCount;
        uint64_t                                        m_LastFrameSlot;
        volatile LONG                                   m_Stopping;
        bool                                            m_MfStarted;
        bool                                            m_RoInitialized;
        HANDLE                                          m_FirstFrameEvent;
        CRITICAL_SECTION                                m_FrameLock;
        CRITICAL_SECTION                                m_WriterLock;
        SRWLOCK                                         m_FailureLock;
        char                                            m_Failure[512];
        wchar_t                                         m_OutputPath[1024];
        ComPtr<ID3D11Device>                            m_Device;
        ComPtr<ID3D11DeviceContext>                     m_Context;
        ComPtr<ID3D11Texture2D>                         m_Staging;
        ComPtr<IMFSample>                               m_PendingVideoSample;
        ComPtr<IMFSinkWriter>                           m_Writer;
        ComPtr<Direct3DAbi::IDirect3DDevice>            m_Direct3DDevice;
        ComPtr<CaptureAbi::IGraphicsCaptureItem>        m_Item;
        ComPtr<CaptureAbi::IDirect3D11CaptureFramePool> m_FramePool;
        ComPtr<CaptureAbi::IGraphicsCaptureSession>     m_Session;
        ComPtr<FrameArrivedHandler>                     m_FrameHandler;
        EventRegistrationToken                          m_FrameToken;
    };

    static WindowsRecorder* g_Recorder = 0;

    static INIT_ONCE g_CaptureSupportOnce = INIT_ONCE_STATIC_INIT;
    static bool      g_CaptureSupported;

    static BOOL CALLBACK CheckCaptureSupport(PINIT_ONCE, PVOID, PVOID*)
    {
        if (!RecorderApisAvailable())
        {
            return TRUE;
        }
        HRESULT initialized = RoInitialize(RO_INIT_MULTITHREADED);
        bool uninitialize = SUCCEEDED(initialized);
        if (SUCCEEDED(initialized) || initialized == RPC_E_CHANGED_MODE)
        {
            g_CaptureSupported = GraphicsCaptureSupported();
        }
        if (uninitialize)
        {
            RoUninitialize();
        }
        return TRUE;
    }

    bool IsSupported()
    {
        // OS capture capabilities are shared by all callers. Status reads must
        // not repeatedly initialize WinRT and query the same activation factories.
        InitOnceExecuteOnce(&g_CaptureSupportOnce, CheckCaptureSupport, 0, 0);
        return g_CaptureSupported;
    }

    bool IsAudioSupported()
    {
        return IsSupported() && IsProcessAudioCaptureSupported();
    }

    const char* GetBackendName()
    {
        return "windows_graphics_capture";
    }

    const char* GetMinimumPlatformVersion()
    {
        return "Windows 10 version 1903";
    }

    const char* GetUnsupportedReason()
    {
        return "native video recording requires Windows 10 version 1903 or newer";
    }

    void GetNativeStatus(Status* status)
    {
        AcquireSRWLockShared(&g_StateLock);
        *status = g_Status;
        if (g_Recorder && status->m_Active)
        {
            g_Recorder->GetFailure(status->m_Failure, sizeof(status->m_Failure));
            status->m_Stopping = g_Recorder->IsStopping();
        }
        ReleaseSRWLockShared(&g_StateLock);
        status->m_Supported = IsSupported();
        if (!status->m_Supported && !status->m_Failure[0])
        {
            CopyText(status->m_Failure, sizeof(status->m_Failure), GetUnsupportedReason());
        }
    }

    bool StartNative(const Window* target, const char* path, uint32_t fps, bool audio, Status* status)
    {
        AcquireSRWLockExclusive(&g_StateLock);
        if (g_Recorder)
        {
            *status = g_Status;
            CopyText(status->m_Failure, sizeof(status->m_Failure), "a video recording is already active");
            ReleaseSRWLockExclusive(&g_StateLock);
            return false;
        }
        memset(&g_Status, 0, sizeof(g_Status));
        g_Status.m_Supported = IsSupported();
        if (!g_Status.m_Supported)
        {
            CopyText(g_Status.m_Failure, sizeof(g_Status.m_Failure), GetUnsupportedReason());
            *status = g_Status;
            ReleaseSRWLockExclusive(&g_StateLock);
            return false;
        }
        if (audio && !IsProcessAudioCaptureSupported())
        {
            CopyText(g_Status.m_Failure, sizeof(g_Status.m_Failure), "application audio recording requires Windows build 20348 or newer; use audio=false");
            *status = g_Status;
            ReleaseSRWLockExclusive(&g_StateLock);
            return false;
        }
        WindowsRecorder* recorder = new (std::nothrow) WindowsRecorder();
        if (!recorder)
        {
            CopyText(g_Status.m_Failure, sizeof(g_Status.m_Failure), "could not allocate the Windows recorder");
            *status = g_Status;
            ReleaseSRWLockExclusive(&g_StateLock);
            return false;
        }
        g_Recorder = recorder;
        ReleaseSRWLockExclusive(&g_StateLock);

        char failure[512] = {};
        bool started = recorder->Start((HWND)target->m_Handle, path, fps, audio, failure, sizeof(failure));
        AcquireSRWLockExclusive(&g_StateLock);
        if (started)
        {
            g_Status.m_Active = true;
            g_Status.m_Audio = audio;
            g_Status.m_Width = recorder->Width();
            g_Status.m_Height = recorder->Height();
            g_Status.m_Fps = recorder->Fps();
            g_Status.m_StartedWallTime = dmTime::GetTime();
            g_Status.m_StartedMonotonicTime = dmTime::GetMonotonicTime();
            CopyText(g_Status.m_Path, sizeof(g_Status.m_Path), path);
        }
        else
        {
            CopyText(g_Status.m_Failure, sizeof(g_Status.m_Failure), failure);
            delete g_Recorder;
            g_Recorder = 0;
        }
        *status = g_Status;
        ReleaseSRWLockExclusive(&g_StateLock);
        return started;
    }

    bool StopNative(Status* status)
    {
        AcquireSRWLockShared(&g_StateLock);
        WindowsRecorder* recorder = g_Recorder;
        ReleaseSRWLockShared(&g_StateLock);
        if (!recorder)
        {
            GetNativeStatus(status);
            CopyText(status->m_Failure, sizeof(status->m_Failure), "no video recording is active");
            return false;
        }

        char failure[512] = {};
        bool finalized = recorder->Stop(failure, sizeof(failure));
        AcquireSRWLockExclusive(&g_StateLock);
        g_Status.m_Active = false;
        g_Status.m_Finalized = finalized;
        g_Status.m_StoppedWallTime = dmTime::GetTime();
        g_Status.m_StoppedMonotonicTime = dmTime::GetMonotonicTime();
        CopyText(g_Status.m_Failure, sizeof(g_Status.m_Failure), failure);
        delete g_Recorder;
        g_Recorder = 0;
        *status = g_Status;
        ReleaseSRWLockExclusive(&g_StateLock);
        return finalized;
    }

    void FinalizeNative()
    {
        Status status;
        GetNativeStatus(&status);
        if (status.m_Active)
        {
            StopNative(&status);
        }
    }
} // namespace dmRecord

#endif
