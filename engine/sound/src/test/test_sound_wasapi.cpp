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

#include <Audioclient.h>
#include <mmdeviceapi.h>
#include <assert.h>
#include <string.h>
#include <wchar.h>

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include "../sound.h"
#include "test/stereo_tone_440_44100_11025.wav.embed.h"

namespace
{
    const uint32_t BUFFER_FRAMES = 256;
    const uint32_t MAX_BUFFER_FRAMES = 1024;

    template <typename T>
    struct FakeComObject : T
    {
        ULONG m_References;

        FakeComObject()
            : m_References(1)
        {
        }
        virtual ~FakeComObject() {}

        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object)
        {
            *object = 0;
            if (iid != __uuidof(IUnknown) && iid != __uuidof(T))
                return E_NOINTERFACE;
            *object = static_cast<T*>(this);
            AddRef();
            return S_OK;
        }

        ULONG STDMETHODCALLTYPE AddRef()
        {
            return ++m_References;
        }
        ULONG STDMETHODCALLTYPE Release()
        {
            ULONG references = --m_References;
            if (!references)
                delete this;
            return references;
        }
    };

    struct FakeEndpoint
    {
        const wchar_t* m_Id;
        uint32_t       m_NonzeroSamples;
        uint32_t       m_Activations;
        uint32_t       m_Stops;
        uint32_t       m_SampleRate;
        uint32_t       m_BufferFrames;
        uint32_t       m_Channels;
        bool           m_Invalidated;

        FakeEndpoint()
            : m_Id(0)
            , m_NonzeroSamples(0)
            , m_Activations(0)
            , m_Stops(0)
            , m_SampleRate(44100)
            , m_BufferFrames(BUFFER_FRAMES)
            , m_Channels(2)
            , m_Invalidated(false)
        {
        }
    };

    struct FakeRenderClient : FakeComObject<IAudioRenderClient>
    {
        FakeEndpoint* m_Endpoint;
        HANDLE        m_BufferEvent;
        float         m_Buffer[MAX_BUFFER_FRAMES * 2];

        FakeRenderClient(FakeEndpoint* endpoint)
            : m_Endpoint(endpoint)
            , m_BufferEvent(0)
        {
        }

        HRESULT STDMETHODCALLTYPE GetBuffer(UINT32 frames, BYTE** buffer)
        {
            if (m_Endpoint->m_Invalidated)
                return AUDCLNT_E_DEVICE_INVALIDATED;
            if (frames > m_Endpoint->m_BufferFrames || frames > MAX_BUFFER_FRAMES)
                return AUDCLNT_E_BUFFER_TOO_LARGE;
            memset(m_Buffer, 0, sizeof(m_Buffer));
            *buffer = (BYTE*)m_Buffer;
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE ReleaseBuffer(UINT32 frames, DWORD flags)
        {
            if (m_Endpoint->m_Invalidated)
                return AUDCLNT_E_DEVICE_INVALIDATED;
            if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT))
            {
                for (uint32_t i = 0; i < frames * m_Endpoint->m_Channels; ++i)
                    m_Endpoint->m_NonzeroSamples += m_Buffer[i] != 0.0f;
            }
            // Complete each render synchronously, with no wall-clock sleeps.
            SetEvent(m_BufferEvent);
            return S_OK;
        }
    };

    struct FakeAudioClient : FakeComObject<IAudioClient2>
    {
        FakeEndpoint*     m_Endpoint;
        FakeRenderClient* m_RenderClient;

        FakeAudioClient(FakeEndpoint* endpoint)
            : m_Endpoint(endpoint)
            , m_RenderClient(new FakeRenderClient(endpoint))
        {
        }

        ~FakeAudioClient()
        {
            m_RenderClient->Release();
        }

        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object)
        {
            if (iid == __uuidof(IAudioClient))
            {
                *object = static_cast<IAudioClient*>(this);
                AddRef();
                return S_OK;
            }
            return FakeComObject<IAudioClient2>::QueryInterface(iid, object);
        }

        HRESULT STDMETHODCALLTYPE Initialize(AUDCLNT_SHAREMODE, DWORD, REFERENCE_TIME, REFERENCE_TIME, const WAVEFORMATEX*, LPCGUID)
        {
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE GetBufferSize(UINT32* frames)
        {
            if (m_Endpoint->m_Invalidated)
                return AUDCLNT_E_DEVICE_INVALIDATED;
            *frames = m_Endpoint->m_BufferFrames;
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE GetStreamLatency(REFERENCE_TIME*)
        {
            return E_NOTIMPL;
        }
        HRESULT STDMETHODCALLTYPE GetCurrentPadding(UINT32* frames)
        {
            if (m_Endpoint->m_Invalidated)
                return AUDCLNT_E_DEVICE_INVALIDATED;
            *frames = 0;
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE IsFormatSupported(AUDCLNT_SHAREMODE, const WAVEFORMATEX*, WAVEFORMATEX** closest)
        {
            if (closest)
                *closest = 0;
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE GetMixFormat(WAVEFORMATEX** format)
        {
            WAVEFORMATEX* mix = (WAVEFORMATEX*)CoTaskMemAlloc(sizeof(WAVEFORMATEX));
            memset(mix, 0, sizeof(*mix));
            mix->wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
            mix->nChannels = (WORD)m_Endpoint->m_Channels;
            mix->nSamplesPerSec = m_Endpoint->m_SampleRate;
            mix->wBitsPerSample = 32;
            mix->nBlockAlign = mix->nChannels * sizeof(float);
            mix->nAvgBytesPerSec = mix->nSamplesPerSec * mix->nBlockAlign;
            *format = mix;
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE GetDevicePeriod(REFERENCE_TIME*, REFERENCE_TIME*)
        {
            return E_NOTIMPL;
        }
        HRESULT STDMETHODCALLTYPE Start()
        {
            if (m_Endpoint->m_Invalidated)
                return AUDCLNT_E_DEVICE_INVALIDATED;
            SetEvent(m_RenderClient->m_BufferEvent);
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE Stop()
        {
            ++m_Endpoint->m_Stops;
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE Reset()
        {
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE SetEventHandle(HANDLE event)
        {
            m_RenderClient->m_BufferEvent = event;
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE GetService(REFIID iid, void** object)
        {
            return m_RenderClient->QueryInterface(iid, object);
        }
        HRESULT STDMETHODCALLTYPE IsOffloadCapable(AUDIO_STREAM_CATEGORY, BOOL*)
        {
            return E_NOTIMPL;
        }
        HRESULT STDMETHODCALLTYPE SetClientProperties(const AudioClientProperties*)
        {
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE GetBufferSizeLimits(const WAVEFORMATEX*, BOOL, REFERENCE_TIME*, REFERENCE_TIME*)
        {
            return E_NOTIMPL;
        }
    };

    struct FakeDevice : FakeComObject<IMMDevice>
    {
        FakeEndpoint* m_Endpoint;

        FakeDevice(FakeEndpoint* endpoint)
            : m_Endpoint(endpoint)
        {
        }

        HRESULT STDMETHODCALLTYPE Activate(REFIID iid, DWORD, PROPVARIANT*, void** object)
        {
            ++m_Endpoint->m_Activations;
            FakeAudioClient* client = new FakeAudioClient(m_Endpoint);
            HRESULT          result = client->QueryInterface(iid, object);
            client->Release();
            return result;
        }
        HRESULT STDMETHODCALLTYPE OpenPropertyStore(DWORD, IPropertyStore**)
        {
            return E_NOTIMPL;
        }
        HRESULT STDMETHODCALLTYPE GetId(LPWSTR* id)
        {
            size_t size = (wcslen(m_Endpoint->m_Id) + 1) * sizeof(wchar_t);
            *id = (LPWSTR)CoTaskMemAlloc(size);
            memcpy(*id, m_Endpoint->m_Id, size);
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE GetState(DWORD* state)
        {
            *state = m_Endpoint->m_Invalidated ? DEVICE_STATE_UNPLUGGED : DEVICE_STATE_ACTIVE;
            return S_OK;
        }
    };

    struct FakeEnumerator : FakeComObject<IMMDeviceEnumerator>
    {
        FakeEndpoint           m_Endpoints[2];
        int32_t                m_DefaultEndpoint;
        int32_t                m_ChangeDefaultDuringOpen;
        HRESULT                m_RegisterResult;
        IMMNotificationClient* m_NotificationClient;

        FakeEnumerator()
            : m_DefaultEndpoint(0)
            , m_ChangeDefaultDuringOpen(-1)
            , m_RegisterResult(S_OK)
            , m_NotificationClient(0)
        {
            m_Endpoints[0].m_Id = L"output-a";
            m_Endpoints[1].m_Id = L"output-b";
        }

        HRESULT STDMETHODCALLTYPE EnumAudioEndpoints(EDataFlow, DWORD, IMMDeviceCollection**)
        {
            return E_NOTIMPL;
        }
        HRESULT STDMETHODCALLTYPE GetDefaultAudioEndpoint(EDataFlow flow, ERole role, IMMDevice** device)
        {
            if (flow != eRender || role != eConsole)
                return E_INVALIDARG;
            if (m_DefaultEndpoint < 0)
            {
                *device = 0;
                return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
            }
            *device = new FakeDevice(&m_Endpoints[m_DefaultEndpoint]);
            if (m_ChangeDefaultDuringOpen >= 0)
            {
                int32_t endpoint = m_ChangeDefaultDuringOpen;
                m_ChangeDefaultDuringOpen = -1;
                ChangeDefaultEndpoint(endpoint);
            }
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE GetDevice(LPCWSTR id, IMMDevice** device)
        {
            for (uint32_t i = 0; i < 2; ++i)
            {
                if (wcscmp(id, m_Endpoints[i].m_Id) == 0)
                {
                    *device = new FakeDevice(&m_Endpoints[i]);
                    return S_OK;
                }
            }
            *device = 0;
            return E_INVALIDARG;
        }
        HRESULT STDMETHODCALLTYPE RegisterEndpointNotificationCallback(IMMNotificationClient* client)
        {
            assert(!m_NotificationClient);
            if (FAILED(m_RegisterResult))
                return m_RegisterResult;
            // MMDeviceAPI does not AddRef the callback; its owner keeps it alive.
            m_NotificationClient = client;
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE UnregisterEndpointNotificationCallback(IMMNotificationClient* client)
        {
            assert(m_NotificationClient == client);
            m_NotificationClient = 0;
            return S_OK;
        }

        void ChangeDefaultEndpoint(int32_t endpoint)
        {
            m_DefaultEndpoint = endpoint;
            // Both endpoints remain valid. A default change alone must not
            // manufacture AUDCLNT_E_DEVICE_INVALIDATED on the old endpoint.
            if (m_NotificationClient)
                m_NotificationClient->OnDefaultDeviceChanged(eRender, eConsole, endpoint < 0 ? 0 : m_Endpoints[endpoint].m_Id);
        }
    };

    FakeEnumerator* g_Enumerator = 0;

    HRESULT         TestCoCreateInstance(REFCLSID clsid, LPUNKNOWN, DWORD, REFIID iid, LPVOID* object)
    {
        if (clsid != __uuidof(MMDeviceEnumerator))
            return E_NOTIMPL;
        return g_Enumerator->QueryInterface(iid, object);
    }
} // namespace

// Compile the real backend with only COM creation substituted. Keeping this in
// a separate test binary avoids hooks in production and cannot touch real audio
// endpoints. The mixer and decoder are linked from the normal sound libraries.
#define CoCreateInstance TestCoCreateInstance
#define DefaultSoundDevice TestWasapiSoundDevice
#include "../devices/device_wasapi.cpp"
#undef DefaultSoundDevice
#undef CoCreateInstance

struct WasapiRoutingTest : jc_test_base_class
{
    dmSound::HSoundData     m_SoundData;
    dmSound::HSoundInstance m_Instance;

    void                    SetUp()
    {
        m_SoundData = 0;
        m_Instance = 0;
        g_Enumerator = new FakeEnumerator;
        dmSound::InitializeParams params;
        dmSound::SetDefaultInitializeParams(&params);
        params.m_UseThread = false;
        ASSERT_EQ(dmSound::RESULT_OK, dmSound::Initialize(0, &params));
        ASSERT_EQ(dmSound::RESULT_OK, dmSound::NewSoundData(STEREO_TONE_440_44100_11025_WAV, STEREO_TONE_440_44100_11025_WAV_SIZE, dmSound::SOUND_DATA_TYPE_WAV, &m_SoundData, 0));
        ASSERT_EQ(dmSound::RESULT_OK, dmSound::NewSoundInstance(m_SoundData, &m_Instance));
        ASSERT_EQ(dmSound::RESULT_OK, dmSound::SetLooping(m_Instance, true, -1));
        ASSERT_EQ(dmSound::RESULT_OK, dmSound::Play(m_Instance));
    }

    void TearDown()
    {
        if (m_Instance)
        {
            dmSound::Stop(m_Instance);
            dmSound::DeleteSoundInstance(m_Instance);
        }
        if (m_SoundData)
            dmSound::DeleteSoundData(m_SoundData);
        dmSound::Finalize();
        EXPECT_EQ((IMMNotificationClient*)0, g_Enumerator->m_NotificationClient);
        EXPECT_EQ(1u, g_Enumerator->m_References);
        g_Enumerator->Release();
        g_Enumerator = 0;
    }
};

// Verify that the fake endpoints receive actual decoded audio through WASAPI.
TEST_F(WasapiRoutingTest, PlaysOnInitialDefaultEndpoint)
{
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::Update());
    ASSERT_GT(g_Enumerator->m_Endpoints[0].m_NonzeroSamples, 0u);
    ASSERT_EQ(0u, g_Enumerator->m_Endpoints[1].m_NonzeroSamples);
    ASSERT_TRUE(dmSound::IsPlaying(m_Instance));
}

// Reproduce #10258: a loop must follow the new default while both outputs remain connected.
TEST_F(WasapiRoutingTest, FollowsDefaultEndpointWithoutDeviceLoss)
{
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::Update());
    ASSERT_GT(g_Enumerator->m_Endpoints[0].m_NonzeroSamples, 0u);
    ASSERT_EQ(0u, g_Enumerator->m_Endpoints[1].m_NonzeroSamples);

    g_Enumerator->ChangeDefaultEndpoint(1);
    // Allow an update to request recovery and the next update to reopen/render.
    dmSound::Update();
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::Update());
    ASSERT_FALSE(g_Enumerator->m_Endpoints[0].m_Invalidated);
    ASSERT_FALSE(g_Enumerator->m_Endpoints[1].m_Invalidated);
    ASSERT_GT(g_Enumerator->m_Endpoints[1].m_NonzeroSamples, 0u);
    ASSERT_TRUE(dmSound::IsPlaying(m_Instance));

    uint32_t old_samples = g_Enumerator->m_Endpoints[0].m_NonzeroSamples;
    uint32_t new_samples = g_Enumerator->m_Endpoints[1].m_NonzeroSamples;
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::Update());
    ASSERT_EQ(old_samples, g_Enumerator->m_Endpoints[0].m_NonzeroSamples);
    ASSERT_GT(g_Enumerator->m_Endpoints[1].m_NonzeroSamples, new_samples);
    ASSERT_EQ(1u, g_Enumerator->m_Endpoints[0].m_Stops);

    g_Enumerator->ChangeDefaultEndpoint(0);
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::Update());
    ASSERT_GT(g_Enumerator->m_Endpoints[0].m_NonzeroSamples, old_samples);
    ASSERT_EQ(1u, g_Enumerator->m_Endpoints[1].m_Stops);
}

// Verify existing device-loss recovery works, independently of default-change notifications.
TEST_F(WasapiRoutingTest, RecoversInvalidatedEndpoint)
{
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::Update());
    ASSERT_GT(g_Enumerator->m_Endpoints[0].m_NonzeroSamples, 0u);
    g_Enumerator->m_DefaultEndpoint = 1;
    g_Enumerator->m_Endpoints[0].m_Invalidated = true;
    ASSERT_EQ(dmSound::RESULT_DEVICE_LOST, dmSound::Update());
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::Update());
    ASSERT_GT(g_Enumerator->m_Endpoints[1].m_NonzeroSamples, 0u);
    ASSERT_TRUE(dmSound::IsPlaying(m_Instance));
}

// Guard #10258 while idle: reopen before the next sound is decoded or queued.
TEST_F(WasapiRoutingTest, SwitchesWhileIdleBeforeNextPlayback)
{
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::DeleteSoundInstance(m_Instance));
    m_Instance = 0;
    ASSERT_EQ(dmSound::RESULT_NOTHING_TO_PLAY, dmSound::Update());
    g_Enumerator->ChangeDefaultEndpoint(1);
    ASSERT_EQ(dmSound::RESULT_NOTHING_TO_PLAY, dmSound::Update());
    ASSERT_EQ(1u, g_Enumerator->m_Endpoints[1].m_Activations);

    ASSERT_EQ(dmSound::RESULT_OK, dmSound::NewSoundInstance(m_SoundData, &m_Instance));
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::Play(m_Instance));
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::Update());
    ASSERT_EQ(0u, g_Enumerator->m_Endpoints[0].m_NonzeroSamples);
    ASSERT_GT(g_Enumerator->m_Endpoints[1].m_NonzeroSamples, 0u);
}

// Capture and communications defaults must not interrupt console playback.
TEST_F(WasapiRoutingTest, IgnoresOtherFlowsAndRoles)
{
    ASSERT_NE((IMMNotificationClient*)0, g_Enumerator->m_NotificationClient);
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::Update());
    g_Enumerator->m_NotificationClient->OnDefaultDeviceChanged(eCapture, eConsole, L"microphone");
    g_Enumerator->m_NotificationClient->OnDefaultDeviceChanged(eRender, eCommunications, L"headset");
    g_Enumerator->m_NotificationClient->OnDefaultDeviceChanged(eRender, eMultimedia, L"speakers");
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::Update());
    ASSERT_EQ(1u, g_Enumerator->m_Endpoints[0].m_Activations);
    ASSERT_EQ(0u, g_Enumerator->m_Endpoints[0].m_Stops);
}

// Reopening must use the new endpoint's format without overrunning existing mixer buffers.
TEST_F(WasapiRoutingTest, SwitchesToDifferentMixFormat)
{
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::Update());
    g_Enumerator->m_Endpoints[1].m_SampleRate = 48000;
    g_Enumerator->m_Endpoints[1].m_BufferFrames = MAX_BUFFER_FRAMES;
    g_Enumerator->m_Endpoints[1].m_Channels = 1;
    g_Enumerator->ChangeDefaultEndpoint(1);
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::Update());
    ASSERT_EQ(48000u, dmSound::GetMixRate());
    ASSERT_GT(g_Enumerator->m_Endpoints[1].m_NonzeroSamples, 0u);
    ASSERT_TRUE(dmSound::IsPlaying(m_Instance));
}

// A null default must release the old subscription and retry until an output returns.
TEST_F(WasapiRoutingTest, RetriesWhenNoDefaultEndpointIsAvailable)
{
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::Update());
    g_Enumerator->ChangeDefaultEndpoint(-1);
    ASSERT_EQ(dmSound::RESULT_INIT_ERROR, dmSound::Update());
    ASSERT_EQ((IMMNotificationClient*)0, g_Enumerator->m_NotificationClient);
    ASSERT_EQ(1u, g_Enumerator->m_References);
    ASSERT_EQ(dmSound::RESULT_INIT_ERROR, dmSound::Update());
    g_Enumerator->ChangeDefaultEndpoint(1);
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::Update());
    ASSERT_GT(g_Enumerator->m_Endpoints[1].m_NonzeroSamples, 0u);
    ASSERT_TRUE(dmSound::IsPlaying(m_Instance));
}

// A notification received while reopening must survive until the following update.
TEST_F(WasapiRoutingTest, PreservesNotificationDuringReopen)
{
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::Update());
    g_Enumerator->m_ChangeDefaultDuringOpen = 0;
    g_Enumerator->ChangeDefaultEndpoint(1);
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::Update());
    ASSERT_EQ(1u, g_Enumerator->m_Endpoints[1].m_Activations);
    uint32_t old_samples = g_Enumerator->m_Endpoints[0].m_NonzeroSamples;
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::Update());
    ASSERT_EQ(2u, g_Enumerator->m_Endpoints[0].m_Activations);
    ASSERT_GT(g_Enumerator->m_Endpoints[0].m_NonzeroSamples, old_samples);
}

// Failed callback registration must release COM resources and allow the next retry.
TEST_F(WasapiRoutingTest, CleansUpFailedNotificationRegistration)
{
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::Update());
    g_Enumerator->m_RegisterResult = E_FAIL;
    g_Enumerator->ChangeDefaultEndpoint(1);
    ASSERT_EQ(dmSound::RESULT_INIT_ERROR, dmSound::Update());
    ASSERT_EQ((IMMNotificationClient*)0, g_Enumerator->m_NotificationClient);
    ASSERT_EQ(1u, g_Enumerator->m_References);
    g_Enumerator->m_RegisterResult = S_OK;
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::Update());
    ASSERT_GT(g_Enumerator->m_Endpoints[1].m_NonzeroSamples, 0u);
}

static DWORD WINAPI NotifyDefaultFromThread(void* context)
{
    IMMNotificationClient* client = (IMMNotificationClient*)context;
    HRESULT                result = client->OnDefaultDeviceChanged(eRender, eConsole, L"output-b");
    client->Release();
    return result;
}

// Windows callbacks run on another thread and must only schedule work for the sound update.
TEST_F(WasapiRoutingTest, DefersNotificationFromAnotherThread)
{
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::Update());
    ASSERT_NE((IMMNotificationClient*)0, g_Enumerator->m_NotificationClient);
    g_Enumerator->m_DefaultEndpoint = 1;
    g_Enumerator->m_NotificationClient->AddRef();
    HANDLE thread = CreateThread(0, 0, NotifyDefaultFromThread, g_Enumerator->m_NotificationClient, 0, 0);
    ASSERT_NE((HANDLE)0, thread);
    DWORD wait_result = WaitForSingleObject(thread, 5000);
    CloseHandle(thread);
    ASSERT_EQ((DWORD)WAIT_OBJECT_0, wait_result);
    ASSERT_EQ(0u, g_Enumerator->m_Endpoints[1].m_Activations);
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::Update());
    ASSERT_GT(g_Enumerator->m_Endpoints[1].m_NonzeroSamples, 0u);
}

extern "C" void dmExportedSymbols();

int             main(int argc, char** argv)
{
    dmExportedSymbols();
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
