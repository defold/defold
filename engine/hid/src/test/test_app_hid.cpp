// Copyright 2020-2024 The Defold Foundation
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

// Creating a small app test for verifying input
// not an actual unit test

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <dlib/log.h>
#include <dlib/time.h>
#include <hid.h>

#include <graphics/graphics.h>
#include <platform/platform_window.h>

// From engine_private.h

enum UpdateResult
{
    RESULT_OK       =  0,
    RESULT_REBOOT   =  1,
    RESULT_EXIT     = -1,
};

typedef void* (*EngineCreateFn)(int argc, char** argv);
typedef void (*EngineDestroyFn)(void* engine);
typedef UpdateResult (*EngineUpdateFn)(void* engine);
typedef void (*EngineGetResultFn)(void* engine, int* run_action, int* exit_code, int* argc, char*** argv);

struct RunLoopParams
{
    int     m_Argc;
    char**  m_Argv;

    void*   m_AppCtx;
    void    (*m_AppCreate)(void* ctx);
    void    (*m_AppDestroy)(void* ctx);

    EngineCreateFn      m_EngineCreate;
    EngineDestroyFn     m_EngineDestroy;
    EngineUpdateFn      m_EngineUpdate;
    EngineGetResultFn   m_EngineGetResult;
};

// From engine_loop.cpp

static int RunLoop(const RunLoopParams* params)
{
    if (params->m_AppCreate)
        params->m_AppCreate(params->m_AppCtx);

    int argc = params->m_Argc;
    char** argv = params->m_Argv;
    int exit_code = 0;
    void* engine = 0;
    UpdateResult result = RESULT_OK;
    while (RESULT_OK == result)
    {
        if (engine == 0)
        {
            engine = params->m_EngineCreate(argc, argv);
            if (!engine)
            {
                exit_code = 1;
                break;
            }
        }

        result = params->m_EngineUpdate(engine);

        if (RESULT_OK != result)
        {
            int run_action = 0;
            params->m_EngineGetResult(engine, &run_action, &exit_code, &argc, &argv);

            params->m_EngineDestroy(engine);
            engine = 0;

            if (RESULT_REBOOT == result)
            {
                // allows us to reboot
                result = RESULT_OK;
            }
        }
    }

    if (params->m_AppDestroy)
        params->m_AppDestroy(params->m_AppCtx);

    return exit_code;
}


static bool GamepadConnectivityCallback(uint32_t gamepad_index, bool connected, void* userdata);


static void AppCreate(void* _ctx)
{
    (void)_ctx;
}

static void AppDestroy(void* _ctx)
{
    (void)_ctx;
}

struct EngineCtx
{
    dmPlatform::HWindow m_Window;
    dmHID::HContext m_HidContext;
    dmHID::GamepadPacket m_OldPackets[dmHID::MAX_GAMEPAD_COUNT];
} g_EngineCtx;

static void EngineDestroy(void* _engine)
{
    EngineCtx* engine = (EngineCtx*)_engine;
    dmHID::Final(engine->m_HidContext);
    dmHID::DeleteContext(engine->m_HidContext);

    dmPlatform::CloseWindow(engine->m_Window);
    dmPlatform::DeleteWindow(engine->m_Window);
}

static void* EngineCreate(int argc, char** argv)
{
    EngineCtx* engine = (EngineCtx*)&g_EngineCtx;
    memset(engine, 0, sizeof(EngineCtx));

    engine->m_Window = dmPlatform::NewWindow();

    dmPlatform::WindowParams window_params = {};
    window_params.m_Width  = 32;
    window_params.m_Height = 32;
    window_params.m_Title  = "hid_test_app";
    (void)dmPlatform::OpenWindow(engine->m_Window, window_params);

    dmGraphics::ContextParams graphics_context_params;
    graphics_context_params.m_DefaultTextureMinFilter = dmGraphics::TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST;
    graphics_context_params.m_DefaultTextureMagFilter = dmGraphics::TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST;
    graphics_context_params.m_VerifyGraphicsCalls = false;
    graphics_context_params.m_Window = engine->m_Window;

    dmGraphics::InstallAdapter();
    dmGraphics::HContext graphics_context = dmGraphics::NewContext(graphics_context_params);
    if (graphics_context == 0x0)
    {
        dmLogFatal("Unable to create the graphics context.");
        return 0;
    }

    dmHID::NewContextParams new_hid_params = dmHID::NewContextParams();

    int32_t use_accelerometer = false;
    new_hid_params.m_IgnoreAcceleration = use_accelerometer ? 0 : 1;

    engine->m_HidContext = dmHID::NewContext(new_hid_params);

    if (use_accelerometer)
    {
        dmHID::EnableAccelerometer(engine->m_HidContext); // Creates and enables the accelerometer
    }

    dmHID::SetGamepadConnectivityCallback(engine->m_HidContext, GamepadConnectivityCallback, 0);
    dmHID::SetWindow(engine->m_HidContext, engine->m_Window);

    bool hid_result = dmHID::Init(engine->m_HidContext);
    if (!hid_result)
    {
        EngineDestroy(engine);
        return 0;
    }

    return engine;
}

static UpdateResult EngineUpdate(void* _engine)
{
    EngineCtx* engine = (EngineCtx*)_engine;

    dmHID::Update(engine->m_HidContext);


#define LOG if (packet_changed) printf

    for (uint32_t i = 0; i < dmHID::MAX_GAMEPAD_COUNT; ++i)
    {
        dmHID::HGamepad pad = dmHID::GetGamepad(engine->m_HidContext, i);
        if (!pad)
            continue;

        if (!dmHID::IsGamepadConnected(pad))
        {
            continue;
        }

        dmHID::GamepadPacket packet;
        if (!GetGamepadPacket(pad, &packet))
        {
            printf("failed to get gamepad packet\n");
            continue;
        }

        int packet_changed = memcmp(&packet, &engine->m_OldPackets[i], sizeof(dmHID::GamepadPacket)) != 0;
        memcpy(&engine->m_OldPackets[i], &packet, sizeof(dmHID::GamepadPacket));

        LOG("PAD %d: ", i);
        LOG("BN: ");

        uint32_t bncount = dmHID::GetGamepadButtonCount(pad);
        for (uint32_t b = 0; b < bncount; ++b)
        {
            bool pressed = GetGamepadButton(&packet, b);

            LOG("%s", pressed ? "*" : "_");
        }

        LOG("HAT: ");
        for (uint32_t a = 0; a < dmHID::GetGamepadHatCount(pad); ++a)
        {
            LOG("%d ", packet.m_Hat[a]);
        }

        LOG("AXIS: ");

        for (uint32_t a = 0; a < dmHID::GetGamepadAxisCount(pad); ++a)
        {
            LOG("%1.3f ", packet.m_Axis[a]);
        }

        LOG("\n");
    }

    dmTime::Sleep(100000);

    return RESULT_OK;

#undef LOG
}

static void EngineGetResult(void* _engine, int* run_action, int* exit_code, int* argc, char*** argv)
{
    EngineCtx* engine = (EngineCtx*)_engine;
    (void)engine;
}

static bool GamepadConnectivityCallback(uint32_t gamepad_index, bool connected, void* userdata)
{
    dmHID::HGamepad pad = dmHID::GetGamepad(g_EngineCtx.m_HidContext, gamepad_index);

    if (connected)
    {
        char name_buffer[128];
        dmHID::GetGamepadDeviceName(g_EngineCtx.m_HidContext, pad, name_buffer, sizeof(name_buffer));
        printf("Gamepad %d connected: %s\n", gamepad_index, name_buffer);
    }
    else
    {
        printf("Gamepad %d disconnected\n", gamepad_index);
    }

    return true;
}

int main(int argc, char **argv)
{
    memset(&g_EngineCtx, 0, sizeof(g_EngineCtx));

    RunLoopParams params;
    params.m_AppCtx = 0;
    params.m_AppCreate = AppCreate;
    params.m_AppDestroy = AppDestroy;
    params.m_EngineCreate = EngineCreate;
    params.m_EngineDestroy = EngineDestroy;
    params.m_EngineUpdate = EngineUpdate;
    params.m_EngineGetResult = EngineGetResult;

    return RunLoop(&params);
}
