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

// Substitute the WinRT factory entry points in the real backend. These tests
// require no capture permission, display, encoder, or particular Windows build.
// Include the backend first so it sets the Windows SDK target before jc_test
// includes Windows.h.
#include "../record/record_win32.cpp"

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

struct CaptureItemFactory : Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IGraphicsCaptureItemInterop>
{
    IFACEMETHODIMP CreateForWindow(HWND, REFIID, void**) override
    {
        return E_NOTIMPL;
    }
    IFACEMETHODIMP CreateForMonitor(HMONITOR, REFIID, void**) override
    {
        return E_NOTIMPL;
    }
};

struct FramePoolFactory : Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>, CaptureAbi::IDirect3D11CaptureFramePoolStatics2>
{
    IFACEMETHODIMP GetRuntimeClassName(HSTRING* name) override
    {
        *name = 0;
        return S_OK;
    }
    IFACEMETHODIMP GetTrustLevel(TrustLevel* level) override
    {
        *level = BaseTrust;
        return S_OK;
    }
    IFACEMETHODIMP CreateFreeThreaded(Direct3DAbi::IDirect3DDevice*,
                                      ABI::Windows::Graphics::DirectX::DirectXPixelFormat,
                                      INT32,
                                      ABI::Windows::Graphics::SizeInt32,
                                      CaptureAbi::IDirect3D11CaptureFramePool**) override
    {
        return E_NOTIMPL;
    }
};

static bool     g_HasWindowCapture;
static bool     g_HasFreeThreadedPool;
static unsigned g_ActivationQueries;
static unsigned g_Initializations;
static unsigned g_Uninitializations;

static HRESULT WINAPI TestInitialize(RO_INIT_TYPE)
{
    ++g_Initializations;
    return S_OK;
}

static void WINAPI TestUninitialize()
{
    ++g_Uninitializations;
}
static BOOL CALLBACK RecordingApisLoaded(PINIT_ONCE, PVOID, PVOID*)
{
    return TRUE;
}

static HRESULT WINAPI TestCreateString(PCNZWCH text, UINT32, HSTRING* string)
{
    *string = reinterpret_cast<HSTRING>(const_cast<wchar_t*>(text));
    return S_OK;
}

static HRESULT WINAPI TestDeleteString(HSTRING)
{
    return S_OK;
}

static HRESULT WINAPI TestActivationFactory(HSTRING name, REFIID iid, void** output)
{
    ++g_ActivationQueries;
    *output = 0;
    const wchar_t* class_name = reinterpret_cast<const wchar_t*>(name);
    if (g_HasWindowCapture && wcscmp(class_name, RuntimeClass_Windows_Graphics_Capture_GraphicsCaptureItem) == 0)
    {
        ComPtr<CaptureItemFactory> factory = Microsoft::WRL::Make<CaptureItemFactory>();
        return factory.CopyTo(iid, output);
    }
    if (g_HasFreeThreadedPool && wcscmp(class_name, RuntimeClass_Windows_Graphics_Capture_Direct3D11CaptureFramePool) == 0)
    {
        ComPtr<FramePoolFactory> factory = Microsoft::WRL::Make<FramePoolFactory>();
        return factory.CopyTo(iid, output);
    }
    return E_NOINTERFACE;
}

class RecordingSupportTest : public jc_test_base_class
{
protected:
    void SetUp()
    {
        dmRecord::pWindowsCreateString = TestCreateString;
        dmRecord::pWindowsDeleteString = TestDeleteString;
        dmRecord::pRoGetActivationFactory = TestActivationFactory;
        g_ActivationQueries = g_Initializations = g_Uninitializations = 0;
        g_HasWindowCapture = g_HasFreeThreadedPool = true;
    }
};

// Session support on Windows 1803/1809 must not advertise recording without HWND capture.
TEST_F(RecordingSupportTest, MissingWindowCapture)
{
    g_HasWindowCapture = false;
    ASSERT_FALSE(dmRecord::CaptureFactoriesAvailable());
    ASSERT_FALSE(dmRecord::GraphicsCaptureSupported());
}

// The recorder cannot run on the dispatcher-bound pool when CreateFreeThreaded is absent.
TEST_F(RecordingSupportTest, MissingFreeThreadedPool)
{
    g_HasFreeThreadedPool = false;
    ASSERT_FALSE(dmRecord::CaptureFactoriesAvailable());
    ASSERT_FALSE(dmRecord::GraphicsCaptureSupported());
}

// Both required interfaces must pass the capability gate without creating a capture session.
TEST_F(RecordingSupportTest, RequiredFactoriesAvailable)
{
    ASSERT_TRUE(dmRecord::CaptureFactoriesAvailable());
}

// Repeated idle status queries must reuse the capability result and balance WinRT initialization.
TEST_F(RecordingSupportTest, StatusReusesCapabilities)
{
    dmRecord::pRoInitialize = TestInitialize;
    dmRecord::pRoUninitialize = TestUninitialize;
    dmRecord::g_RecorderApisAvailable = true;
    InitOnceExecuteOnce(&dmRecord::g_RecorderApisOnce, RecordingApisLoaded, 0, 0);
    g_HasWindowCapture = false;
    for (unsigned i = 0; i < 10; ++i)
    {
        dmRecord::Status status;
        dmRecord::GetStatus(&status);
        ASSERT_FALSE(status.m_Supported);
    }
    ASSERT_EQ(1U, g_ActivationQueries);
    ASSERT_EQ(1U, g_Initializations);
    ASSERT_EQ(1U, g_Uninitializations);
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
