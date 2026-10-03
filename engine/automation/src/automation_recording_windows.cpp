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

#include "automation_recording.h"

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

#include <dmsdk/dlib/time.h>
#include <dmsdk/graphics/graphics_native.h>

#include <algorithm>
#include <new>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

using Microsoft::WRL::ComPtr;

namespace CaptureAbi = ABI::Windows::Graphics::Capture;
namespace Direct3DAbi = ABI::Windows::Graphics::DirectX::Direct3D11;
namespace Direct3DInterop = Windows::Graphics::DirectX::Direct3D11;
typedef __FITypedEventHandler_2_Windows__CGraphics__CCapture__CDirect3D11CaptureFramePool_IInspectable FrameArrivedHandler;

class AgileFrameArrivedHandler
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::WinRtClassicComMix>,
          FrameArrivedHandler,
          Microsoft::WRL::FtmBase>
{
public:
    // A delegate needs no runtime class name. Avoid WRL's InspectableClass macro,
    // which introduces a mandatory WindowsCreateString import on older Windows.
    IFACEMETHODIMP GetRuntimeClassName(HSTRING* name) override
    {
        if (!name) return E_POINTER;
        *name = 0;
        return S_OK;
    }

    IFACEMETHODIMP GetTrustLevel(TrustLevel* level) override
    {
        if (!level) return E_POINTER;
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
    void* m_Context;
    Callback m_Callback;
};

namespace dmAutomation
{
    // Resolve newer recording APIs on demand. Stock debug engines retain their
    // existing Windows baseline even when these optional DLLs are unavailable.
    static INIT_ONCE g_RecorderApisOnce = INIT_ONCE_STATIC_INIT;
    static bool g_RecorderApisAvailable;
    static decltype(&::RoInitialize) pRoInitialize;
    static decltype(&::RoUninitialize) pRoUninitialize;
    static decltype(&::RoGetActivationFactory) pRoGetActivationFactory;
    static decltype(&::WindowsCreateString) pWindowsCreateString;
    static decltype(&::WindowsDeleteString) pWindowsDeleteString;
    static decltype(&::D3D11CreateDevice) pD3D11CreateDevice;
    typedef HRESULT (WINAPI* CreateDirect3DDeviceFn)(IDXGIDevice*, IInspectable**);
    static CreateDirect3DDeviceFn pCreateDirect3D11DeviceFromDXGIDevice;
    static decltype(&::MFStartup) pMFStartup;
    static decltype(&::MFShutdown) pMFShutdown;
    static decltype(&::MFCreateAttributes) pMFCreateAttributes;
    static decltype(&::MFCreateMediaType) pMFCreateMediaType;
    static decltype(&::MFCreateMemoryBuffer) pMFCreateMemoryBuffer;
    static decltype(&::MFCreateSample) pMFCreateSample;
    static decltype(&::MFCreateSinkWriterFromURL) pMFCreateSinkWriterFromURL;
    static BOOL CALLBACK LoadRecorderApis(PINIT_ONCE, PVOID, PVOID*)
    {
        wchar_t system_directory[MAX_PATH];
        if (!GetSystemDirectoryW(system_directory, MAX_PATH)) return TRUE;
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
    static RecordingStatus g_Status;

    static void CopyText(char* output, size_t output_size, const char* value)
    {
        snprintf(output, output_size, "%s", value ? value : "");
    }

    static void HResultText(HRESULT result, char* output, size_t output_size)
    {
        char* system_message = 0;
        DWORD length = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                                      FORMAT_MESSAGE_IGNORE_INSERTS, 0, (DWORD)result, 0,
                                      (char*)&system_message, 0, 0);
        if (length && system_message)
        {
            while (length && (system_message[length - 1] == '\r' || system_message[length - 1] == '\n'))
                system_message[--length] = 0;
            snprintf(output, output_size, "%s (0x%08lx)", system_message, (unsigned long)result);
            LocalFree(system_message);
        }
        else
        {
            snprintf(output, output_size, "Windows error 0x%08lx", (unsigned long)result);
        }
    }

    static bool Utf8ToWide(const char* input, wchar_t* output, uint32_t output_count)
    {
        if (!input || !input[0]) return false;
        int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input, -1, output, (int)output_count);
        return count > 0;
    }

    static HRESULT GetActivationFactory(const wchar_t* class_name, REFIID interface_id, void** output)
    {
        HSTRING name = 0;
        HRESULT result = WindowsCreateString(class_name, (UINT32)wcslen(class_name), &name);
        if (SUCCEEDED(result))
            result = RoGetActivationFactory(name, interface_id, output);
        if (name) WindowsDeleteString(name);
        return result;
    }

    static void CloseInspectable(IInspectable* object)
    {
        if (!object) return;
        ComPtr<ABI::Windows::Foundation::IClosable> closable;
        if (SUCCEEDED(object->QueryInterface(IID_PPV_ARGS(&closable))))
            closable->Close();
    }

    static bool GraphicsCaptureSupported()
    {
        ComPtr<CaptureAbi::IGraphicsCaptureSessionStatics> statics;
        HRESULT result = GetActivationFactory(
            RuntimeClass_Windows_Graphics_Capture_GraphicsCaptureSession,
            __uuidof(CaptureAbi::IGraphicsCaptureSessionStatics),
            reinterpret_cast<void**>(statics.ReleaseAndGetAddressOf()));
        boolean supported = FALSE;
        return SUCCEEDED(result) && SUCCEEDED(statics->IsSupported(&supported)) && supported;
    }

    bool GetRecordingWindow(RecordingWindow* target)
    {
        target->m_Handle = (uintptr_t)dmGraphics::GetNativeWindowsHWND();
        return target->m_Handle != 0;
    }

    class WindowsRecorder
    {
    public:
        WindowsRecorder()
        : m_Window(0), m_CornerPreference(0), m_RestoreCornerPreference(false),
          m_OutputWidth(0), m_OutputHeight(0), m_Fps(0), m_CropX(0), m_CropY(0),
          m_CropWidth(0), m_CropHeight(0), m_StreamIndex(0), m_FrameCount(0),
          m_LastFrameSlot(UINT64_MAX), m_Stopping(0), m_MfStarted(false), m_RoInitialized(false),
          m_FirstFrameEvent(0)
        {
            InitializeCriticalSection(&m_FrameLock);
            m_FirstFrameEvent = CreateEventW(0, TRUE, FALSE, 0);
            m_Failure[0] = 0;
            QueryPerformanceFrequency(&m_CounterFrequency);
            QueryPerformanceCounter(&m_StartCounter);
        }

        ~WindowsRecorder()
        {
            Cleanup(false);
            if (m_FirstFrameEvent) CloseHandle(m_FirstFrameEvent);
            DeleteCriticalSection(&m_FrameLock);
        }

        bool Start(HWND window, const char* path, uint32_t requested_width, uint32_t requested_height,
                   uint32_t fps, char* failure, size_t failure_size)
        {
            if (!RecorderApisAvailable()) return Fail(E_NOTIMPL, failure, failure_size);
            HRESULT result = RoInitialize(RO_INIT_MULTITHREADED);
            if (SUCCEEDED(result)) m_RoInitialized = true;
            else if (result != RPC_E_CHANGED_MODE)
                return Fail(result, failure, failure_size);

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
            if (SUCCEEDED(DwmGetWindowAttribute(m_Window, corner_attribute,
                                                &m_CornerPreference, sizeof(m_CornerPreference))))
            {
                DWORD do_not_round = 1; // DWMWCP_DONOTROUND
                if (SUCCEEDED(DwmSetWindowAttribute(m_Window, corner_attribute,
                                                    &do_not_round, sizeof(do_not_round))))
                    m_RestoreCornerPreference = true;
            }

            result = CreateCaptureItem();
            if (FAILED(result)) return Fail(result, failure, failure_size);
            if (!ResolveClientCrop())
            {
                CopyText(failure, failure_size, "could not resolve the Defold window client area inside the capture surface");
                return false;
            }

            // The Media Foundation H.264 encoder requires even dimensions.
            m_OutputWidth = std::max<uint32_t>(2, (requested_width ? requested_width : m_CropWidth) & ~1U);
            m_OutputHeight = std::max<uint32_t>(2, (requested_height ? requested_height : m_CropHeight) & ~1U);
            m_Fps = fps;
            if (!Utf8ToWide(path, m_OutputPath, sizeof(m_OutputPath) / sizeof(m_OutputPath[0])))
            {
                CopyText(failure, failure_size, "recording path is not valid UTF-8 or is too long");
                return false;
            }
            DeleteFileW(m_OutputPath);

            result = CreateDevice();
            if (FAILED(result)) return Fail(result, failure, failure_size);
            result = CreateWriter();
            if (FAILED(result)) return Fail(result, failure, failure_size);

            result = CreateFramePool();
            if (FAILED(result)) return Fail(result, failure, failure_size);
            result = m_Session->StartCapture();
            if (FAILED(result)) return Fail(result, failure, failure_size);

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
            return ok;
        }

        bool Stop(char* failure, size_t failure_size)
        {
            InterlockedExchange(&m_Stopping, 1);
            if (m_FramePool && m_FrameHandler)
                m_FramePool->remove_FrameArrived(m_FrameToken);
            CloseInspectable(m_Session.Get());
            CloseInspectable(m_FramePool.Get());
            m_FrameHandler.Reset();
            m_Session.Reset();
            m_FramePool.Reset();

            // FramePool is free-threaded. Taking this lock after closing it waits
            // for an in-flight callback before the sink writer is finalized.
            EnterCriticalSection(&m_FrameLock);
            HRESULT result = S_OK;
            if (m_Writer) result = m_Writer->Finalize();
            if (m_Failure[0] && !failure[0]) CopyText(failure, failure_size, m_Failure);
            LeaveCriticalSection(&m_FrameLock);
            if (FAILED(result) && !failure[0]) HResultText(result, failure, failure_size);

            Cleanup(false);
            WIN32_FILE_ATTRIBUTE_DATA attributes;
            bool finalized = failure[0] == 0 &&
                GetFileAttributesExW(m_OutputPath, GetFileExInfoStandard, &attributes) &&
                (attributes.nFileSizeHigh != 0 || attributes.nFileSizeLow != 0);
            if (!finalized && !failure[0])
                CopyText(failure, failure_size, "Media Foundation did not produce a non-empty MP4 file");
            return finalized;
        }

        uint32_t Width() const { return m_OutputWidth; }
        uint32_t Height() const { return m_OutputHeight; }
        uint32_t Fps() const { return m_Fps; }

    private:
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
            if (FAILED(result)) return result;
            return interop->CreateForWindow(m_Window,
                                            __uuidof(CaptureAbi::IGraphicsCaptureItem),
                                            reinterpret_cast<void**>(m_Item.ReleaseAndGetAddressOf()));
        }

        bool ResolveClientCrop()
        {
            RECT window_rect;
            RECT client_rect;
            POINT client_origin = {0, 0};
            if (FAILED(DwmGetWindowAttribute(m_Window, DWMWA_EXTENDED_FRAME_BOUNDS,
                                             &window_rect, sizeof(window_rect))) &&
                !GetWindowRect(m_Window, &window_rect))
                return false;
            if (!GetClientRect(m_Window, &client_rect) || !ClientToScreen(m_Window, &client_origin))
                return false;

            ABI::Windows::Graphics::SizeInt32 capture_size;
            if (FAILED(m_Item->get_Size(&capture_size))) return false;
            LONG window_width = window_rect.right - window_rect.left;
            LONG window_height = window_rect.bottom - window_rect.top;
            LONG client_width = client_rect.right - client_rect.left;
            LONG client_height = client_rect.bottom - client_rect.top;
            if (capture_size.Width <= 0 || capture_size.Height <= 0 ||
                window_width <= 0 || window_height <= 0 || client_width <= 0 || client_height <= 0)
                return false;

            double scale_x = (double)capture_size.Width / window_width;
            double scale_y = (double)capture_size.Height / window_height;
            m_CropX = (uint32_t)std::max(0.0, (client_origin.x - window_rect.left) * scale_x + 0.5);
            m_CropY = (uint32_t)std::max(0.0, (client_origin.y - window_rect.top) * scale_y + 0.5);
            m_CropWidth = (uint32_t)std::max(1.0, client_width * scale_x + 0.5);
            m_CropHeight = (uint32_t)std::max(1.0, client_height * scale_y + 0.5);
            if (m_CropX >= (uint32_t)capture_size.Width || m_CropY >= (uint32_t)capture_size.Height)
                return false;
            m_CropWidth = std::min(m_CropWidth, (uint32_t)capture_size.Width - m_CropX);
            m_CropHeight = std::min(m_CropHeight, (uint32_t)capture_size.Height - m_CropY);
            return true;
        }

        HRESULT CreateDevice()
        {
            UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
            D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
                                          D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
            D3D_FEATURE_LEVEL selected;
            HRESULT result = D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE, 0, flags, levels,
                                               sizeof(levels) / sizeof(levels[0]), D3D11_SDK_VERSION,
                                               &m_Device, &selected, &m_Context);
            if (FAILED(result)) return result;
            ComPtr<IDXGIDevice> dxgi_device;
            result = m_Device.As(&dxgi_device);
            if (FAILED(result)) return result;
            ComPtr<IInspectable> inspectable;
            result = CreateDirect3D11DeviceFromDXGIDevice(dxgi_device.Get(), inspectable.ReleaseAndGetAddressOf());
            if (FAILED(result)) return result;
            return inspectable.As(&m_Direct3DDevice);
        }

        HRESULT CreateFramePool()
        {
            ComPtr<CaptureAbi::IDirect3D11CaptureFramePoolStatics2> statics;
            HRESULT result = GetActivationFactory(
                RuntimeClass_Windows_Graphics_Capture_Direct3D11CaptureFramePool,
                __uuidof(CaptureAbi::IDirect3D11CaptureFramePoolStatics2),
                reinterpret_cast<void**>(statics.ReleaseAndGetAddressOf()));
            if (FAILED(result)) return result;
            ABI::Windows::Graphics::SizeInt32 size;
            result = m_Item->get_Size(&size);
            if (FAILED(result)) return result;
            result = statics->CreateFreeThreaded(
                m_Direct3DDevice.Get(),
                ABI::Windows::Graphics::DirectX::DirectXPixelFormat_B8G8R8A8UIntNormalized,
                3, size, &m_FramePool);
            if (FAILED(result)) return result;
            result = m_FramePool->CreateCaptureSession(m_Item.Get(), &m_Session);
            if (FAILED(result)) return result;
            ComPtr<AgileFrameArrivedHandler> frame_handler =
                Microsoft::WRL::Make<AgileFrameArrivedHandler>();
            if (!frame_handler) return E_OUTOFMEMORY;
            result = frame_handler->RuntimeClassInitialize(this, &WindowsRecorder::DispatchFrame);
            if (FAILED(result)) return result;
            m_FrameHandler = frame_handler;
            return m_FramePool->add_FrameArrived(m_FrameHandler.Get(), &m_FrameToken);
        }

        static void DispatchFrame(void* context,
                                  CaptureAbi::IDirect3D11CaptureFramePool* sender)
        {
            static_cast<WindowsRecorder*>(context)->OnFrame(sender);
        }

        HRESULT SetMediaAttributeSize(IMFAttributes* attributes, REFGUID key, uint32_t width, uint32_t height)
        {
            return MFSetAttributeSize(attributes, key, width, height);
        }

        HRESULT CreateWriter()
        {
            HRESULT result = MFStartup(MF_VERSION);
            if (FAILED(result)) return result;
            m_MfStarted = true;

            ComPtr<IMFAttributes> attributes;
            result = MFCreateAttributes(&attributes, 2);
            if (FAILED(result)) return result;
            attributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
            attributes->SetUINT32(MF_SINK_WRITER_DISABLE_THROTTLING, TRUE);
            result = MFCreateSinkWriterFromURL(m_OutputPath, 0, attributes.Get(), &m_Writer);
            if (FAILED(result)) return result;

            ComPtr<IMFMediaType> output_type;
            result = MFCreateMediaType(&output_type);
            if (FAILED(result)) return result;
            if (FAILED(result = output_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video))) return result;
            if (FAILED(result = output_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264))) return result;
            uint64_t pixels_per_second = (uint64_t)m_OutputWidth * m_OutputHeight * m_Fps;
            uint32_t bitrate = (uint32_t)std::min<uint64_t>(25000000, std::max<uint64_t>(1000000, pixels_per_second / 8));
            if (FAILED(result = output_type->SetUINT32(MF_MT_AVG_BITRATE, bitrate))) return result;
            if (FAILED(result = output_type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive))) return result;
            if (FAILED(result = SetMediaAttributeSize(output_type.Get(), MF_MT_FRAME_SIZE, m_OutputWidth, m_OutputHeight))) return result;
            if (FAILED(result = MFSetAttributeRatio(output_type.Get(), MF_MT_FRAME_RATE, m_Fps, 1))) return result;
            if (FAILED(result = MFSetAttributeRatio(output_type.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1))) return result;
            if (FAILED(result = m_Writer->AddStream(output_type.Get(), &m_StreamIndex))) return result;

            ComPtr<IMFMediaType> input_type;
            result = MFCreateMediaType(&input_type);
            if (FAILED(result)) return result;
            if (FAILED(result = input_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video))) return result;
            if (FAILED(result = input_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32))) return result;
            if (FAILED(result = input_type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive))) return result;
            if (FAILED(result = SetMediaAttributeSize(input_type.Get(), MF_MT_FRAME_SIZE, m_OutputWidth, m_OutputHeight))) return result;
            if (FAILED(result = MFSetAttributeRatio(input_type.Get(), MF_MT_FRAME_RATE, m_Fps, 1))) return result;
            if (FAILED(result = MFSetAttributeRatio(input_type.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1))) return result;
            if (FAILED(result = m_Writer->SetInputMediaType(m_StreamIndex, input_type.Get(), 0))) return result;
            return m_Writer->BeginWriting();
        }

        HRESULT EnsureStagingTexture()
        {
            if (m_Staging) return S_OK;
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
            if (InterlockedCompareExchange(&m_Stopping, 0, 0)) return;
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
            ComPtr<Direct3DAbi::IDirect3DSurface> surface;
            if (SUCCEEDED(result)) result = frame->get_Surface(&surface);
            ComPtr<Direct3DInterop::IDirect3DDxgiInterfaceAccess> access;
            if (SUCCEEDED(result)) result = surface.As(&access);
            ComPtr<ID3D11Texture2D> texture;
            if (SUCCEEDED(result)) result = access->GetInterface(IID_PPV_ARGS(&texture));
            if (SUCCEEDED(result)) result = WriteFrame(texture.Get());
            if (FAILED(result) && !m_Failure[0])
                HResultText(result, m_Failure, sizeof(m_Failure));
            if (m_FrameCount > 0 || m_Failure[0]) SetEvent(m_FirstFrameEvent);
            LeaveCriticalSection(&m_FrameLock);
        }

        HRESULT WriteFrame(ID3D11Texture2D* texture)
        {
            LARGE_INTEGER counter;
            QueryPerformanceCounter(&counter);
            uint64_t frame_slot = (uint64_t)(counter.QuadPart - m_StartCounter.QuadPart) * m_Fps /
                                  (uint64_t)m_CounterFrequency.QuadPart;
            if (m_LastFrameSlot != UINT64_MAX && frame_slot <= m_LastFrameSlot)
                return S_FALSE;

            HRESULT result = EnsureStagingTexture();
            if (FAILED(result)) return result;
            D3D11_TEXTURE2D_DESC source_description;
            texture->GetDesc(&source_description);
            if (m_CropX + m_CropWidth > source_description.Width ||
                m_CropY + m_CropHeight > source_description.Height)
                return E_INVALIDARG;

            D3D11_BOX source_box = {m_CropX, m_CropY, 0,
                                    m_CropX + m_CropWidth, m_CropY + m_CropHeight, 1};
            m_Context->CopySubresourceRegion(m_Staging.Get(), 0, 0, 0, 0, texture, 0, &source_box);
            D3D11_MAPPED_SUBRESOURCE mapped;
            result = m_Context->Map(m_Staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
            if (FAILED(result)) return result;

            uint64_t buffer_size_64 = (uint64_t)m_OutputWidth * m_OutputHeight * 4;
            if (buffer_size_64 > 0xffffffffU)
            {
                m_Context->Unmap(m_Staging.Get(), 0);
                return E_INVALIDARG;
            }
            ComPtr<IMFMediaBuffer> buffer;
            result = MFCreateMemoryBuffer((DWORD)buffer_size_64, &buffer);
            BYTE* destination = 0;
            if (SUCCEEDED(result)) result = buffer->Lock(&destination, 0, 0);
            if (SUCCEEDED(result))
            {
                const BYTE* source = (const BYTE*)mapped.pData;
                uint32_t destination_stride = m_OutputWidth * 4;
                for (uint32_t y = 0; y < m_OutputHeight; ++y)
                {
                    uint32_t source_y = (uint32_t)((uint64_t)y * m_CropHeight / m_OutputHeight);
                    // RGB32 is conventionally bottom-up in Media Foundation.
                    BYTE* destination_row = destination + (size_t)(m_OutputHeight - 1 - y) * destination_stride;
                    const BYTE* source_row = source + (size_t)source_y * mapped.RowPitch;
                    for (uint32_t x = 0; x < m_OutputWidth; ++x)
                    {
                        uint32_t source_x = (uint32_t)((uint64_t)x * m_CropWidth / m_OutputWidth);
                        memcpy(destination_row + (size_t)x * 4, source_row + (size_t)source_x * 4, 4);
                    }
                }
                buffer->Unlock();
                buffer->SetCurrentLength((DWORD)buffer_size_64);
            }
            m_Context->Unmap(m_Staging.Get(), 0);
            if (FAILED(result)) return result;

            ComPtr<IMFSample> sample;
            result = MFCreateSample(&sample);
            if (FAILED(result)) return result;
            if (FAILED(result = sample->AddBuffer(buffer.Get()))) return result;
            LONGLONG duration = 10000000LL / m_Fps;
            sample->SetSampleTime((LONGLONG)frame_slot * duration);
            sample->SetSampleDuration(duration);
            result = m_Writer->WriteSample(m_StreamIndex, sample.Get());
            if (SUCCEEDED(result))
            {
                m_LastFrameSlot = frame_slot;
                ++m_FrameCount;
            }
            return result;
        }

        void RestoreWindowCorners()
        {
            if (m_Window && m_RestoreCornerPreference)
            {
                const DWORD corner_attribute = 33;
                DwmSetWindowAttribute(m_Window, corner_attribute,
                                      &m_CornerPreference, sizeof(m_CornerPreference));
                m_RestoreCornerPreference = false;
            }
        }

        void Cleanup(bool finalize)
        {
            (void)finalize;
            InterlockedExchange(&m_Stopping, 1);
            if (m_FramePool && m_FrameHandler)
                m_FramePool->remove_FrameArrived(m_FrameToken);
            CloseInspectable(m_Session.Get());
            CloseInspectable(m_FramePool.Get());
            m_FrameHandler.Reset();
            m_Session.Reset();
            m_FramePool.Reset();
            m_Item.Reset();
            m_Direct3DDevice.Reset();
            m_Staging.Reset();
            m_Writer.Reset();
            m_Context.Reset();
            m_Device.Reset();
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

        HWND m_Window;
        DWORD m_CornerPreference;
        bool m_RestoreCornerPreference;
        uint32_t m_OutputWidth, m_OutputHeight, m_Fps;
        uint32_t m_CropX, m_CropY, m_CropWidth, m_CropHeight;
        DWORD m_StreamIndex;
        uint64_t m_FrameCount;
        uint64_t m_LastFrameSlot;
        volatile LONG m_Stopping;
        LARGE_INTEGER m_StartCounter;
        LARGE_INTEGER m_CounterFrequency;
        bool m_MfStarted;
        bool m_RoInitialized;
        HANDLE m_FirstFrameEvent;
        CRITICAL_SECTION m_FrameLock;
        char m_Failure[512];
        wchar_t m_OutputPath[1024];
        ComPtr<ID3D11Device> m_Device;
        ComPtr<ID3D11DeviceContext> m_Context;
        ComPtr<ID3D11Texture2D> m_Staging;
        ComPtr<IMFSinkWriter> m_Writer;
        ComPtr<Direct3DAbi::IDirect3DDevice> m_Direct3DDevice;
        ComPtr<CaptureAbi::IGraphicsCaptureItem> m_Item;
        ComPtr<CaptureAbi::IDirect3D11CaptureFramePool> m_FramePool;
        ComPtr<CaptureAbi::IGraphicsCaptureSession> m_Session;
        ComPtr<FrameArrivedHandler> m_FrameHandler;
        EventRegistrationToken m_FrameToken;
    };

    static WindowsRecorder* g_Recorder = 0;

    bool IsNativeRecordingSupported()
    {
        if (!RecorderApisAvailable()) return false;
        HRESULT initialized = RoInitialize(RO_INIT_MULTITHREADED);
        bool uninitialize = SUCCEEDED(initialized);
        bool supported = false;
        if (SUCCEEDED(initialized) || initialized == RPC_E_CHANGED_MODE)
            supported = GraphicsCaptureSupported();
        if (uninitialize) RoUninitialize();
        return supported;
    }

    bool IsNativeRecordingAudioSupported()
    {
        return false;
    }

    const char* NativeRecordingBackendName()
    {
        return "windows_graphics_capture";
    }

    const char* NativeRecordingMinimumPlatformVersion()
    {
        return "Windows 10 version 1903";
    }

    const char* NativeRecordingUnsupportedReason()
    {
        return "native video recording requires Windows 10 version 1903 or newer";
    }

    void GetNativeRecordingStatus(RecordingStatus* status)
    {
        AcquireSRWLockShared(&g_StateLock);
        *status = g_Status;
        ReleaseSRWLockShared(&g_StateLock);
        status->m_Supported = IsNativeRecordingSupported();
        if (!status->m_Supported && !status->m_Failure[0])
            CopyText(status->m_Failure, sizeof(status->m_Failure), NativeRecordingUnsupportedReason());
    }

    bool StartNativeRecording(const RecordingWindow* target, const char* path, uint32_t width, uint32_t height,
                              uint32_t fps, bool audio, RecordingStatus* status)
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
        g_Status.m_Supported = IsNativeRecordingSupported();
        if (!g_Status.m_Supported)
        {
            CopyText(g_Status.m_Failure, sizeof(g_Status.m_Failure), NativeRecordingUnsupportedReason());
            *status = g_Status;
            ReleaseSRWLockExclusive(&g_StateLock);
            return false;
        }
        if (audio)
        {
            CopyText(g_Status.m_Failure, sizeof(g_Status.m_Failure),
                     "application audio recording is not implemented by the Windows backend; use audio=false");
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
        bool started = recorder->Start((HWND)target->m_Handle, path, width, height, fps, failure, sizeof(failure));
        AcquireSRWLockExclusive(&g_StateLock);
        if (started)
        {
            g_Status.m_Active = true;
            g_Status.m_Audio = false;
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

    bool StopNativeRecording(RecordingStatus* status)
    {
        AcquireSRWLockShared(&g_StateLock);
        WindowsRecorder* recorder = g_Recorder;
        ReleaseSRWLockShared(&g_StateLock);
        if (!recorder)
        {
            GetNativeRecordingStatus(status);
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

    void FinalizeNativeRecording()
    {
        RecordingStatus status;
        GetNativeRecordingStatus(&status);
        if (status.m_Active) StopNativeRecording(&status);
    }
}

#endif
