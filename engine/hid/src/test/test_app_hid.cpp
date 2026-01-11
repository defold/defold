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

// Creating a small app test for verifying input
// not an actual unit test

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <dlib/log.h>
#include <dlib/time.h>
#include "../hid.h"

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
    dmHID::GamepadPacket m_OldGamepadPackets[dmHID::MAX_GAMEPAD_COUNT];
    dmHID::KeyboardPacket m_OldKeyboardPackets[dmHID::MAX_KEYBOARD_COUNT];
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
    window_params.m_Width            = 32;
    window_params.m_Height           = 32;
    window_params.m_Title            = "hid_test_app";
    window_params.m_GraphicsApi      = dmPlatform::PLATFORM_GRAPHICS_API_OPENGL;
    window_params.m_ContextAlphabits = 8;

    (void)dmPlatform::OpenWindow(engine->m_Window, window_params);

    dmGraphics::ContextParams graphics_context_params = {};
    graphics_context_params.m_DefaultTextureMinFilter = dmGraphics::TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST;
    graphics_context_params.m_DefaultTextureMagFilter = dmGraphics::TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST;
    graphics_context_params.m_VerifyGraphicsCalls = false;
    graphics_context_params.m_Window = engine->m_Window;

    dmGraphics::InstallAdapter(dmGraphics::ADAPTER_FAMILY_OPENGL);
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

static const char* KeyToStr(dmHID::Key key)
{
#if !defined(DM_PLATFORM_VENDOR)
    switch(key)
    {
        case dmHID::KEY_SPACE:return "KEY_SPACE";
        case dmHID::KEY_EXCLAIM:return "KEY_EXCLAIM";
        case dmHID::KEY_QUOTEDBL:return "KEY_QUOTEDBL";
        case dmHID::KEY_HASH:return "KEY_HASH";
        case dmHID::KEY_DOLLAR:return "KEY_DOLLAR";
        case dmHID::KEY_AMPERSAND:return "KEY_AMPERSAND";
        case dmHID::KEY_QUOTE:return "KEY_QUOTE";
        case dmHID::KEY_LPAREN:return "KEY_LPAREN";
        case dmHID::KEY_RPAREN:return "KEY_RPAREN";
        case dmHID::KEY_ASTERISK:return "KEY_ASTERISK";
        case dmHID::KEY_PLUS:return "KEY_PLUS";
        case dmHID::KEY_COMMA:return "KEY_COMMA";
        case dmHID::KEY_MINUS:return "KEY_MINUS";
        case dmHID::KEY_PERIOD:return "KEY_PERIOD";
        case dmHID::KEY_SLASH:return "KEY_SLASH";
        case dmHID::KEY_0:return "KEY_0";
        case dmHID::KEY_1:return "KEY_1";
        case dmHID::KEY_2:return "KEY_2";
        case dmHID::KEY_3:return "KEY_3";
        case dmHID::KEY_4:return "KEY_4";
        case dmHID::KEY_5:return "KEY_5";
        case dmHID::KEY_6:return "KEY_6";
        case dmHID::KEY_7:return "KEY_7";
        case dmHID::KEY_8:return "KEY_8";
        case dmHID::KEY_9:return "KEY_9";
        case dmHID::KEY_COLON:return "KEY_COLON";
        case dmHID::KEY_SEMICOLON:return "KEY_SEMICOLON";
        case dmHID::KEY_LESS:return "KEY_LESS";
        case dmHID::KEY_EQUALS:return "KEY_EQUALS";
        case dmHID::KEY_GREATER:return "KEY_GREATER";
        case dmHID::KEY_QUESTION:return "KEY_QUESTION";
        case dmHID::KEY_AT:return "KEY_AT";
        case dmHID::KEY_A:return "KEY_A";
        case dmHID::KEY_B:return "KEY_B";
        case dmHID::KEY_C:return "KEY_C";
        case dmHID::KEY_D:return "KEY_D";
        case dmHID::KEY_E:return "KEY_E";
        case dmHID::KEY_F:return "KEY_F";
        case dmHID::KEY_G:return "KEY_G";
        case dmHID::KEY_H:return "KEY_H";
        case dmHID::KEY_I:return "KEY_I";
        case dmHID::KEY_J:return "KEY_J";
        case dmHID::KEY_K:return "KEY_K";
        case dmHID::KEY_L:return "KEY_L";
        case dmHID::KEY_M:return "KEY_M";
        case dmHID::KEY_N:return "KEY_N";
        case dmHID::KEY_O:return "KEY_O";
        case dmHID::KEY_P:return "KEY_P";
        case dmHID::KEY_Q:return "KEY_Q";
        case dmHID::KEY_R:return "KEY_R";
        case dmHID::KEY_S:return "KEY_S";
        case dmHID::KEY_T:return "KEY_T";
        case dmHID::KEY_U:return "KEY_U";
        case dmHID::KEY_V:return "KEY_V";
        case dmHID::KEY_W:return "KEY_W";
        case dmHID::KEY_X:return "KEY_X";
        case dmHID::KEY_Y:return "KEY_Y";
        case dmHID::KEY_Z:return "KEY_Z";
        case dmHID::KEY_LBRACKET:return "KEY_LBRACKET";
        case dmHID::KEY_BACKSLASH:return "KEY_BACKSLASH";
        case dmHID::KEY_RBRACKET:return "KEY_RBRACKET";
        case dmHID::KEY_CARET:return "KEY_CARET";
        case dmHID::KEY_UNDERSCORE:return "KEY_UNDERSCORE";
        case dmHID::KEY_BACKQUOTE:return "KEY_BACKQUOTE";
        case dmHID::KEY_LBRACE:return "KEY_LBRACE";
        case dmHID::KEY_PIPE:return "KEY_PIPE";
        case dmHID::KEY_RBRACE:return "KEY_RBRACE";
        case dmHID::KEY_TILDE:return "KEY_TILDE";
        case dmHID::KEY_ESC:return "KEY_ESC";
        case dmHID::KEY_F1:return "KEY_F1";
        case dmHID::KEY_F2:return "KEY_F2";
        case dmHID::KEY_F3:return "KEY_F3";
        case dmHID::KEY_F4:return "KEY_F4";
        case dmHID::KEY_F5:return "KEY_F5";
        case dmHID::KEY_F6:return "KEY_F6";
        case dmHID::KEY_F7:return "KEY_F7";
        case dmHID::KEY_F8:return "KEY_F8";
        case dmHID::KEY_F9:return "KEY_F9";
        case dmHID::KEY_F10:return "KEY_F10";
        case dmHID::KEY_F11:return "KEY_F11";
        case dmHID::KEY_F12:return "KEY_F12";
        case dmHID::KEY_UP:return "KEY_UP";
        case dmHID::KEY_DOWN:return "KEY_DOWN";
        case dmHID::KEY_LEFT:return "KEY_LEFT";
        case dmHID::KEY_RIGHT:return "KEY_RIGHT";
        case dmHID::KEY_LSHIFT:return "KEY_LSHIFT";
        case dmHID::KEY_RSHIFT:return "KEY_RSHIFT";
        case dmHID::KEY_LCTRL:return "KEY_LCTRL";
        case dmHID::KEY_RCTRL:return "KEY_RCTRL";
        case dmHID::KEY_LALT:return "KEY_LALT";
        case dmHID::KEY_RALT:return "KEY_RALT";
        case dmHID::KEY_TAB:return "KEY_TAB";
        case dmHID::KEY_ENTER:return "KEY_ENTER";
        case dmHID::KEY_BACKSPACE:return "KEY_BACKSPACE";
        case dmHID::KEY_INSERT:return "KEY_INSERT";
        case dmHID::KEY_DEL:return "KEY_DEL";
        case dmHID::KEY_PAGEUP:return "KEY_PAGEUP";
        case dmHID::KEY_PAGEDOWN:return "KEY_PAGEDOWN";
        case dmHID::KEY_HOME:return "KEY_HOME";
        case dmHID::KEY_END:return "KEY_END";
        case dmHID::KEY_KP_0:return "KEY_KP_0";
        case dmHID::KEY_KP_1:return "KEY_KP_1";
        case dmHID::KEY_KP_2:return "KEY_KP_2";
        case dmHID::KEY_KP_3:return "KEY_KP_3";
        case dmHID::KEY_KP_4:return "KEY_KP_4";
        case dmHID::KEY_KP_5:return "KEY_KP_5";
        case dmHID::KEY_KP_6:return "KEY_KP_6";
        case dmHID::KEY_KP_7:return "KEY_KP_7";
        case dmHID::KEY_KP_8:return "KEY_KP_8";
        case dmHID::KEY_KP_9:return "KEY_KP_9";
        case dmHID::KEY_KP_DIVIDE:return "KEY_KP_DIVIDE";
        case dmHID::KEY_KP_MULTIPLY:return "KEY_KP_MULTIPLY";
        case dmHID::KEY_KP_SUBTRACT:return "KEY_KP_SUBTRACT";
        case dmHID::KEY_KP_ADD:return "KEY_KP_ADD";
        case dmHID::KEY_KP_DECIMAL:return "KEY_KP_DECIMAL";
        case dmHID::KEY_KP_EQUAL:return "KEY_KP_EQUAL";
        case dmHID::KEY_KP_ENTER:return "KEY_KP_ENTER";
        case dmHID::KEY_KP_NUM_LOCK:return "KEY_KP_NUM_LOCK";
        case dmHID::KEY_CAPS_LOCK:return "KEY_CAPS_LOCK";
        case dmHID::KEY_SCROLL_LOCK:return "KEY_SCROLL_LOCK";
        case dmHID::KEY_PAUSE:return "KEY_PAUSE";
        case dmHID::KEY_LSUPER:return "KEY_LSUPER";
        case dmHID::KEY_RSUPER:return "KEY_RSUPER";
        case dmHID::KEY_MENU:return "KEY_MENU";
        case dmHID::KEY_BACK:return "KEY_BACK";
        default: break;
    }
#endif // DM_PLATFORM_VENDOR

    return "<UNKNOWN-KEY>";
}

static UpdateResult EngineUpdate(void* _engine)
{
    EngineCtx* engine = (EngineCtx*)_engine;

    dmHID::Update(engine->m_HidContext);

    dmPlatform::PollEvents(engine->m_Window);

#define LOG if (packet_changed) dmLogInfo

    for (int i = 0; i < dmHID::MAX_KEYBOARD_COUNT; ++i)
    {
        dmHID::HKeyboard keyboard = dmHID::GetKeyboard(engine->m_HidContext, i);
        if (!keyboard)
            continue;
        if (!dmHID::IsKeyboardConnected(keyboard))
            continue;

        dmHID::KeyboardPacket packet;
        if (!GetKeyboardPacket(keyboard, &packet))
        {
            printf("failed to get keyboard packet\n");
            continue;
        }

        int packet_changed = memcmp(&packet, &engine->m_OldKeyboardPackets[i], sizeof(dmHID::KeyboardPacket)) != 0;
        memcpy(&engine->m_OldKeyboardPackets[i], &packet, sizeof(dmHID::KeyboardPacket));

        LOG("KEYBOARD %d: ", i);

        for (int j = dmHID::KEY_SPACE; j < dmHID::MAX_KEY_COUNT; ++j)
        {
            if (GetKey(&packet, (dmHID::Key) j))
            {
                LOG("KEY %s (%d)", KeyToStr((dmHID::Key) j), j);
            }
        }
    }

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

        int packet_changed = memcmp(&packet, &engine->m_OldGamepadPackets[i], sizeof(dmHID::GamepadPacket)) != 0;
        memcpy(&engine->m_OldGamepadPackets[i], &packet, sizeof(dmHID::GamepadPacket));

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
        char device_name[dmHID::MAX_GAMEPAD_NAME_LENGTH];
        dmHID::GetGamepadDeviceName(g_EngineCtx.m_HidContext, pad, device_name);
        printf("Gamepad %d connected: %s\n", gamepad_index, device_name);
    }
    else
    {
        printf("Gamepad %d disconnected\n", gamepad_index);
    }

    return true;
}

extern "C" void dmExportedSymbols();

int main(int argc, char **argv)
{
    dmExportedSymbols();

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
