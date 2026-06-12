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

#if defined(_MSC_VER)
#include <dlib/safe_windows.h>
#endif

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>

#include <dlib/platform.h>

#include <graphics/graphics.h>
#include <platform/window.hpp>

#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/time.h>
#include <ddf/ddf.h>

#include <hid/hid.h>
#include <input/input_ddf.h>

extern "C" void dmExportedSymbols();

static const uint16_t USB_VENDOR_NINTENDO = 0x057e;
static const float AXIS_DETECT_THRESHOLD = 0.25f;

enum State
{
    STATE_WAITING,
    STATE_READING
};

struct Trigger
{
    dmInputDDF::GamepadType m_Type;
    uint32_t m_Index;
    bool m_Modifiers[dmInputDDF::MAX_GAMEPAD_MODIFIER_COUNT];
    uint32_t m_HatMask;
    bool m_Skip;
};

struct Driver
{
    const char* m_Guid;
    const char* m_Device;
    const char* m_Platform;
    float m_DeadZone;
    Trigger m_Triggers[dmInputDDF::MAX_GAMEPAD_COUNT];
};

void GetDelta(dmHID::HGamepad gamepad, dmHID::GamepadPacket* zero_packet, dmHID::GamepadPacket* input_packet, dmInputDDF::GamepadType* gamepad_type, uint32_t* index, float* value, float* delta);
void DumpDriver(FILE* out, Driver* driver);
void DumpSDLEntry(FILE* out, Driver* driver);

static const char* GetSDLPlatformName(const char* platform)
{
    if (strcmp(platform, DM_PLATFORM_NAME_WINDOWS) == 0)
        return "Windows";
    if (strcmp(platform, DM_PLATFORM_NAME_LINUX) == 0)
        return "Linux";
    if (strcmp(platform, DM_PLATFORM_NAME_MACOS) == 0)
        return "Mac OS X";
    if (strcmp(platform, DM_PLATFORM_NAME_ANDROID) == 0)
        return "Android";
    if (strcmp(platform, DM_PLATFORM_NAME_IOS) == 0)
        return "iOS";
    if (strcmp(platform, DM_PLATFORM_NAME_WEB) == 0)
        return "Web";
    return platform;
}

static int HexCharToInt(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

static bool GetGuidUint16LE(const char* guid, uint32_t byte_offset, uint16_t* value)
{
    if (guid == 0x0 || value == 0x0)
        return false;

    const uint32_t char_offset = byte_offset * 2;
    int b0_hi = HexCharToInt(guid[char_offset + 0]);
    int b0_lo = HexCharToInt(guid[char_offset + 1]);
    int b1_hi = HexCharToInt(guid[char_offset + 2]);
    int b1_lo = HexCharToInt(guid[char_offset + 3]);
    if (b0_hi < 0 || b0_lo < 0 || b1_hi < 0 || b1_lo < 0)
        return false;

    uint8_t b0 = (uint8_t)((b0_hi << 4) | b0_lo);
    uint8_t b1 = (uint8_t)((b1_hi << 4) | b1_lo);
    *value = (uint16_t)(b0 | (b1 << 8));
    return true;
}

static bool IsNintendoController(const Driver* driver)
{
    uint16_t vendor = 0;
    if (!GetGuidUint16LE(driver->m_Guid, 4, &vendor))
        return false;

    return vendor == USB_VENDOR_NINTENDO;
}

static const char* GetSDLInputName(const Driver* driver, uint32_t trigger_id)
{
    const bool is_nintendo = IsNintendoController(driver);

    switch (trigger_id)
    {
    case dmInputDDF::GAMEPAD_LSTICK_LEFT:
    case dmInputDDF::GAMEPAD_LSTICK_RIGHT:
        return "leftx";
    case dmInputDDF::GAMEPAD_LSTICK_DOWN:
    case dmInputDDF::GAMEPAD_LSTICK_UP:
        return "lefty";
    case dmInputDDF::GAMEPAD_LSTICK_CLICK:
        return "leftstick";
    case dmInputDDF::GAMEPAD_LTRIGGER:
        return "lefttrigger";
    case dmInputDDF::GAMEPAD_LSHOULDER:
        return "leftshoulder";
    case dmInputDDF::GAMEPAD_LPAD_LEFT:
        return "dpleft";
    case dmInputDDF::GAMEPAD_LPAD_RIGHT:
        return "dpright";
    case dmInputDDF::GAMEPAD_LPAD_DOWN:
        return "dpdown";
    case dmInputDDF::GAMEPAD_LPAD_UP:
        return "dpup";
    case dmInputDDF::GAMEPAD_RSTICK_LEFT:
    case dmInputDDF::GAMEPAD_RSTICK_RIGHT:
        return "rightx";
    case dmInputDDF::GAMEPAD_RSTICK_DOWN:
    case dmInputDDF::GAMEPAD_RSTICK_UP:
        return "righty";
    case dmInputDDF::GAMEPAD_RSTICK_CLICK:
        return "rightstick";
    case dmInputDDF::GAMEPAD_RTRIGGER:
        return "righttrigger";
    case dmInputDDF::GAMEPAD_RSHOULDER:
        return "rightshoulder";
    case dmInputDDF::GAMEPAD_RPAD_LEFT:
        return is_nintendo ? "y" : "x";
    case dmInputDDF::GAMEPAD_RPAD_RIGHT:
        return "b";
    case dmInputDDF::GAMEPAD_RPAD_DOWN:
        return "a";
    case dmInputDDF::GAMEPAD_RPAD_UP:
        return is_nintendo ? "x" : "y";
    case dmInputDDF::GAMEPAD_START:
        return "start";
    case dmInputDDF::GAMEPAD_BACK:
        return "back";
    case dmInputDDF::GAMEPAD_GUIDE:
        return "guide";
    default:
        return 0x0;
    }
}

static void DumpSDLDeviceName(FILE* out, const char* device_name)
{
    for (const char* c = device_name; *c != '\0'; ++c)
    {
        fputc(*c == ',' ? ' ' : *c, out);
    }
}

static void DumpSDLBinding(FILE* out, const char* input_name, const Trigger* trigger)
{
    fprintf(out, ",%s:", input_name);

    switch (trigger->m_Type)
    {
    case dmInputDDF::GAMEPAD_TYPE_BUTTON:
        fprintf(out, "b%d", trigger->m_Index);
        break;
    case dmInputDDF::GAMEPAD_TYPE_HAT:
        fprintf(out, "h%d.%u", trigger->m_Index, trigger->m_HatMask);
        break;
    case dmInputDDF::GAMEPAD_TYPE_AXIS:
    {
        bool negate = trigger->m_Modifiers[dmInputDDF::GAMEPAD_MODIFIER_NEGATE];
        bool clamp  = trigger->m_Modifiers[dmInputDDF::GAMEPAD_MODIFIER_CLAMP];
        if (clamp)
            fprintf(out, "%ca%d", negate ? '-' : '+', trigger->m_Index);
        else
            fprintf(out, "a%d%s", trigger->m_Index, negate ? "~" : "");
        break;
    }
    default:
        break;
    }
}

bool IsIgnoredID(uint32_t trigger_id) {
    // Ignore connected/disconnected triggers.
    if (trigger_id == dmInputDDF::GAMEPAD_CONNECTED ||
        trigger_id == dmInputDDF::GAMEPAD_DISCONNECTED ||
        trigger_id == dmInputDDF::GAMEPAD_RAW)
    {
        return true;
    }

    return false;
}

dmHID::HContext g_HidContext = 0;

static volatile bool g_SkipTrigger = false;

static void UpdateHID(HWindow window)
{
    dmHID::Update(g_HidContext);
    dmPlatform::PollEvents(window);
}

static void PrintGamepadPacketDebug(dmHID::HGamepad gamepad, dmHID::GamepadPacket* packet)
{
    printf("\tdebug packet:");

    printf(" axis");
    uint32_t axis_count = dmHID::GetGamepadAxisCount(gamepad);
    if (axis_count == 0)
    {
        printf("=<none>");
    }
    else
    {
        printf("=");
        for (uint32_t a = 0; a < axis_count; ++a)
        {
            printf("%s%.3f", a == 0 ? "" : ",", packet->m_Axis[a]);
        }
    }

    printf(" buttons");
    uint32_t button_count = dmHID::GetGamepadButtonCount(gamepad);
    if (button_count == 0)
    {
        printf("=<none>");
    }
    else
    {
        printf("=");
        for (uint32_t b = 0; b < button_count; ++b)
        {
            printf("%c", dmHID::GetGamepadButton(packet, b) ? '*' : '_');
        }
    }

    printf(" hats");
    uint32_t hat_count = dmHID::GetGamepadHatCount(gamepad);
    if (hat_count == 0)
    {
        printf("=<none>");
    }
    else
    {
        printf("=");
        for (uint32_t h = 0; h < hat_count; ++h)
        {
            printf("%s%d", h == 0 ? "" : ",", packet->m_Hat[h]);
        }
    }

    printf("\n");
    fflush(stdout);
}

static bool HasGamepadInputs(dmHID::HGamepad gamepad)
{
    return dmHID::GetGamepadAxisCount(gamepad) != 0 || dmHID::GetGamepadButtonCount(gamepad) != 0 || dmHID::GetGamepadHatCount(gamepad) != 0;
}

static bool WaitForGamepadReady(HWindow window, dmHID::HGamepad gamepad, dmHID::GamepadPacket* packet, bool debug_input)
{
    const uint32_t wait_steps = 300;
    for (uint32_t step = 0; step < wait_steps; ++step)
    {
        UpdateHID(window);
        if (!dmHID::GetGamepadPacket(gamepad, packet))
        {
            return false;
        }

        if (HasGamepadInputs(gamepad))
        {
            if (debug_input)
            {
                PrintGamepadPacketDebug(gamepad, packet);
            }
            return true;
        }

        if (debug_input && (step == 0 || (step % 60) == 0))
        {
            PrintGamepadPacketDebug(gamepad, packet);
        }

        dmTime::Sleep(16667);
    }

    return false;
}

static void sig_handler(int _)
{
    (void)_;
    g_SkipTrigger = true;
}

static bool GamepadConnectivityCallback(uint32_t gamepad_index, bool connected, void* userdata)
{
    (void)userdata;
    dmHID::HGamepad pad = dmHID::GetGamepad(g_HidContext, gamepad_index);

    if (connected)
    {
        char device_name[dmHID::MAX_GAMEPAD_NAME_LENGTH];
        dmHID::GetGamepadDeviceName(g_HidContext, pad, device_name);

        dmHID::GamepadGuid guid;
        dmHID::GetGamepadDeviceGuid(g_HidContext, pad, &guid);
        char guid_string[dmHID::MAX_GAMEPAD_GUID_LENGTH+1];
        dmHID::FormatGamepadGuid(&guid, guid_string);

        dmLogInfo("Gamepad %u connected: '%s' - '%s'", gamepad_index, device_name, guid_string);
    }
    else
    {
        dmLogInfo("Gamepad %u disconnected.", gamepad_index);
    }

    return true;
}

int main(int argc, char *argv[])
{
    dmExportedSymbols();

    int result = 0;
    dmHID::HGamepad gamepad = dmHID::INVALID_GAMEPAD_HANDLE;
    dmHID::HGamepad gamepads[dmHID::MAX_GAMEPAD_COUNT];

    float wait_delay = 1.0f;
    float timer = 0.0f;
    float dt = 0.016667f;
    bool settled = false;

    FILE* out = 0x0;

    const char* filename = "default.gamepads";
    if (argc > 1)
        filename = argv[1];

    const bool debug_input = getenv("GDC_DEBUG") != 0x0;

    dmGraphics::InstallAdapter(dmGraphics::ADAPTER_FAMILY_OPENGL);

    WindowCreateParams window_params;
    WindowCreateParamsInitialize(&window_params);
    window_params.m_Width = 32;
    window_params.m_Height = 32;
    window_params.m_Title = "gdc";
    window_params.m_PrintDeviceInfo = false;
    window_params.m_OpenGLVersionHint = 33;
    window_params.m_GraphicsApi = WINDOW_GRAPHICS_API_OPENGL;
    window_params.m_ContextAlphabits = 8;
    window_params.m_Hidden = 1;

    HWindow window = dmPlatform::NewWindow();
    WindowResult window_result = dmPlatform::OpenWindow(window, window_params);
    if (window_result != WINDOW_RESULT_OK)
    {
        dmLogFatal("Unable to open window: %d.", window_result);
        return 1;
    }

    dmGraphics::ContextParams graphics_context_params;
    graphics_context_params.m_DefaultTextureMinFilter = dmGraphics::TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST;
    graphics_context_params.m_DefaultTextureMagFilter = dmGraphics::TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST;
    graphics_context_params.m_Window                  = window;

    dmGraphics::HContext graphics_context = dmGraphics::NewContext(graphics_context_params);
    if (graphics_context == 0x0)
    {
        dmLogFatal("Unable to create the graphics context.");
        return 1;
    }

    dmHID::NewContextParams new_hid_params = dmHID::NewContextParams();
    new_hid_params.m_IgnoreAcceleration = 1;

    g_HidContext = dmHID::NewContext(new_hid_params);
    dmHID::SetWindow(g_HidContext, window);
    dmHID::SetGamepadConnectivityCallback(g_HidContext, GamepadConnectivityCallback, 0);
    bool hid_result = dmHID::Init(g_HidContext);
    if (!hid_result)
    {
        dmLogFatal("Unable to initialize HID.");
        return 1;
    }

    const int settle_timeout_count = (int)(5.0f / dt) + 1;
    int wait_count = settle_timeout_count;

retry:

    dmLogInfo("Starting checking for gamepads.");

    uint32_t gamepad_count = 0;
    const uint32_t wait_steps = 180;
    for (uint32_t step = 0; step < wait_steps && gamepad_count == 0; ++step)
    {
        UpdateHID(window);

        gamepad_count = 0;
        for (uint32_t i = 0; i < dmHID::MAX_GAMEPAD_COUNT; ++i)
        {
            gamepad = dmHID::GetGamepad(g_HidContext, i);
            if (dmHID::IsGamepadConnected(gamepad))
            {
                gamepads[gamepad_count++] = gamepad;
            }
        }

        if (gamepad_count == 0)
        {
            dmTime::Sleep(16667);
        }
    }

    if (gamepad_count == 0)
    {
        printf("No connected gamepads, rechecking...\n");
        goto retry;
    }

    if (gamepad_count > 1)
    {
        for (uint32_t i = 0; i < gamepad_count; ++i)
        {
            char device_name[dmHID::MAX_GAMEPAD_NAME_LENGTH];
            dmHID::GetGamepadDeviceName(g_HidContext, gamepads[i], device_name);

            dmHID::GamepadGuid guid;
            dmHID::GetGamepadDeviceGuid(g_HidContext, gamepads[i], &guid);
            char guid_string[dmHID::MAX_GAMEPAD_GUID_LENGTH+1];
            dmHID::FormatGamepadGuid(&guid, guid_string);
            printf("%d: '%s' - '%s'\n", i+1, device_name, guid_string);
            fflush(stdout);
        }
        printf("\n* Which gamepad do you want to calibrate? [1-%d] ", gamepad_count);
        fflush(stdout);

        uint32_t index = 0;
        while (true)
        {
            scanf("%d", &index);
            if (index > 0 && index <= gamepad_count)
            {
                gamepad = gamepads[index-1];
                break;
            }
            printf("Invalid input!");
            fflush(stdout);
            goto bail;
        }
    }
    else
    {
        gamepad = gamepads[0];
    }

    char device_name[dmHID::MAX_GAMEPAD_NAME_LENGTH];
    dmHID::GetGamepadDeviceName(g_HidContext, gamepad, device_name);

    dmHID::GamepadGuid guid;
    dmHID::GetGamepadDeviceGuid(g_HidContext, gamepad, &guid);
    char guid_string[dmHID::MAX_GAMEPAD_GUID_LENGTH+1];
    dmHID::FormatGamepadGuid(&guid, guid_string);

    printf("\n'%s' - '%s' will be added to %s\n\n", device_name, guid_string, filename);

    Driver driver;
    memset(&driver, 0, sizeof(Driver));
    driver.m_Guid = guid_string;
    driver.m_Device = device_name;
    driver.m_Platform = DM_PLATFORM;
    driver.m_DeadZone = 0.2f;

    dmHID::GamepadPacket prev_packet;
    dmHID::GamepadPacket packet;
    if (debug_input)
    {
        printf("* GDC_DEBUG=1 HID input dump is enabled.\n");
    }

    if (!WaitForGamepadReady(window, gamepad, &packet, debug_input))
    {
        printf("* The selected gamepad is connected, but it reports no axes, buttons or hats.\n");
        goto bail;
    }
    prev_packet = packet;

    dmInputDDF::GamepadType gamepad_type;
    uint32_t index;
    float value;
    float delta;

    // Register signal handler so user can press Ctrl+C to skip a trigger.
    signal(SIGINT, sig_handler);

    printf("* Waiting for the gamepad values to settle. Don't press anything on the gamepad...\n");
    settled = false;
    wait_count = settle_timeout_count;
    timer = wait_delay;
    for (int w = wait_count; w > 0; --w, --wait_count)
    {
        UpdateHID(window);
        if (!dmHID::GetGamepadPacket(gamepad, &packet))
        {
            printf("%d: Failed to get gamepad packet\n", __LINE__);
            break;
        }
        GetDelta(gamepad, &prev_packet, &packet, &gamepad_type, &index, &value, &delta);
        if (debug_input && dmMath::Abs(delta) >= 0.01f)
        {
            printf("\tdebug settle: type=%d index=%u value=%.3f delta=%.3f\n", gamepad_type, index, value, delta);
            PrintGamepadPacketDebug(gamepad, &packet);
        }
        if (dmMath::Abs(delta) < 0.01f)
        {
            timer -= dt;
            if (timer <= 0.0f)
            {
                prev_packet = packet;
                settled = true;
                break;
            }
        }
        else
        {
            timer = wait_delay;
            prev_packet = packet;
        }
    }

    if (!settled)
    {
        printf("* The gamepad didn't settle\n");
        goto bail;
    }
    wait_count = 100;

    printf("* NOTE: If gamepad is missing a trigger, you can skip it with Ctrl+C.\n");
    for (uint8_t i = 0; i < dmInputDDF::MAX_GAMEPAD_COUNT; ++i)
    {
        // Ignore connected/disconnected triggers.
        if (IsIgnoredID(dmInputDDF_Gamepad_DESCRIPTOR.m_EnumValues[i].m_Value)) {
            continue;
        }

        State state = STATE_WAITING;
        timer = wait_delay;
        bool run = true;
        uint32_t debug_sample_count = 0;
        dmHID::GamepadPacket debug_prev_packet = prev_packet;
        while (run && wait_count>0)
        {
            if (g_SkipTrigger) {
                printf("Skipping trigger.\n");
                driver.m_Triggers[i].m_Skip = true;
                g_SkipTrigger = false;
                break;
            }

            UpdateHID(window);
            if (!dmHID::GetGamepadPacket(gamepad, &packet))
            {
                printf("%d: Failed to get gamepad packet\n", __LINE__);
                goto bail;
            }
            if (debug_input)
            {
                bool packet_changed = memcmp(&debug_prev_packet, &packet, sizeof(dmHID::GamepadPacket)) != 0;
                if (packet_changed || debug_sample_count == 0 || (debug_sample_count % 30) == 0)
                {
                    PrintGamepadPacketDebug(gamepad, &packet);
                    debug_prev_packet = packet;
                }
                ++debug_sample_count;
            }
            GetDelta(gamepad, &prev_packet, &packet, &gamepad_type, &index, &value, &delta);
            if (debug_input && gamepad_type == dmInputDDF::GAMEPAD_TYPE_AXIS && dmMath::Abs(delta) > 0.05f)
            {
                printf("\tdebug axis: index=%u value=%.3f delta=%.3f\n", index, value, delta);
            }

            switch (state)
            {
            case STATE_WAITING:
                if (dmMath::Abs(delta) < 0.2f)
                {
                    state = STATE_READING;
                    printf("* Press %s...\n", dmInputDDF_Gamepad_DESCRIPTOR.m_EnumValues[i].m_Name);
                    wait_count = 100;
                }
                else
                {
                    --wait_count;
                }
                break;
            case STATE_READING:
                if (gamepad_type == dmInputDDF::GAMEPAD_TYPE_BUTTON || gamepad_type == dmInputDDF::GAMEPAD_TYPE_HAT)
                {
                    driver.m_Triggers[i].m_Type = gamepad_type;
                    driver.m_Triggers[i].m_Index = index;

                    if (gamepad_type == dmInputDDF::GAMEPAD_TYPE_HAT)
                        driver.m_Triggers[i].m_HatMask = (uint32_t)value;

                    static const char* gamepad_types[] = {"axis", "button", "hat"};
                    printf("\ttype: %s, index: %d, value: %.2f\n", gamepad_types[gamepad_type], index, value);
                    run = false;
                }
                else if (dmMath::Abs(delta) > AXIS_DETECT_THRESHOLD)
                {
                    driver.m_Triggers[i].m_Type = gamepad_type;
                    driver.m_Triggers[i].m_Index = index;
                    if (dmMath::Abs(delta) > 1.5f)
                        driver.m_Triggers[i].m_Modifiers[dmInputDDF::GAMEPAD_MODIFIER_SCALE] = true;
                    else if (gamepad_type == dmInputDDF::GAMEPAD_TYPE_AXIS)
                        driver.m_Triggers[i].m_Modifiers[dmInputDDF::GAMEPAD_MODIFIER_CLAMP] = true;
                    if (delta < 0.0f)
                        driver.m_Triggers[i].m_Modifiers[dmInputDDF::GAMEPAD_MODIFIER_NEGATE] = true;

                    if (gamepad_type == dmInputDDF::GAMEPAD_TYPE_HAT) {
                        driver.m_Triggers[i].m_HatMask = (uint32_t)value;
                    }

                    static const char* gamepad_types[] = {"axis", "button", "hat"};
                    printf("\ttype: %s, index: %d, value: %.2f\n", gamepad_types[gamepad_type], index, value);
                    run = false;
                }
                break;
            }
            dmTime::Sleep((uint32_t)(dt * 1000000));
        }

        if (wait_count == 0)
        {
            printf("* The gamepad didn't settle\n");
            goto bail;
        }
    }

    out = fopen(filename, "w");
    if (out != 0x0)
    {
        DumpDriver(out, &driver);
        printf("Wrote '%s'\n", filename);
    }
    else
    {
        printf("Could not open %s to write to, dumping to stdout instead.\n\n", filename);
        DumpDriver(stdout, &driver);
    }

    DumpSDLEntry(stdout, &driver);

    printf("Bye!\n");

bail:
    dmHID::Final(g_HidContext);
    dmHID::DeleteContext(g_HidContext);
    if (out != 0x0)
        fclose(out);

    return result;
}

void GetDelta(dmHID::HGamepad gamepad, dmHID::GamepadPacket* prev_packet, dmHID::GamepadPacket* packet, dmInputDDF::GamepadType* gamepad_type, uint32_t* index, float* value, float* delta)
{
    if (gamepad_type != 0x0)
        *gamepad_type = dmInputDDF::GAMEPAD_TYPE_AXIS;
    if (index != 0x0)
        *index = 0;
    if (value != 0x0)
        *value = packet->m_Axis[0];
    if (delta != 0x0)
        *delta = 0.0f;

    float max_delta = 0.0f;
    uint32_t axis_count = dmHID::GetGamepadAxisCount(gamepad);
    for (uint32_t i = 0; i < axis_count; ++i)
    {
        float axis_delta = packet->m_Axis[i] - prev_packet->m_Axis[i];
        float abs_axis_delta = dmMath::Abs(axis_delta);
        if (abs_axis_delta > max_delta)
        {
            max_delta = abs_axis_delta;
            if (gamepad_type != 0x0)
                *gamepad_type = dmInputDDF::GAMEPAD_TYPE_AXIS;
            if (index != 0x0)
                *index = i;
            if (value != 0x0)
                *value = packet->m_Axis[i];
            if (delta != 0x0)
            {
                *delta = axis_delta;
            }
        }
    }
    uint32_t hat_count = dmHID::GetGamepadHatCount(gamepad);
    for (uint32_t i = 0; i < hat_count; ++i)
    {
        if (prev_packet->m_Hat[i] != packet->m_Hat[i]) {
            if (gamepad_type != 0x0)
                *gamepad_type = dmInputDDF::GAMEPAD_TYPE_HAT;
            if (index != 0x0)
                *index = i;
            if (value != 0x0)
                *value = packet->m_Hat[i];
            if (delta != 0x0)
            {
                *delta = 1.0f;
            }
        }
    }
    uint32_t button_count = dmHID::GetGamepadButtonCount(gamepad);
    for (uint32_t i = 0; i < button_count; ++i)
    {
        bool was_down = dmHID::GetGamepadButton(prev_packet, i);
        bool is_down = dmHID::GetGamepadButton(packet, i);
        if (!was_down && is_down)
        {
            if (gamepad_type != 0x0)
                *gamepad_type = dmInputDDF::GAMEPAD_TYPE_BUTTON;
            if (index != 0x0)
                *index = i;
            if (value != 0x0)
                *value = 1.0f;
            if (delta != 0x0)
            {
                *delta = 1.0f;
            }
        }
    }
}

void DumpDriver(FILE* out, Driver* driver)
{
    fprintf(out, "driver\n{\n");
    fprintf(out, "    device: \"%s\"\n", driver->m_Device);
    fprintf(out, "    platform: \"%s\"\n", driver->m_Platform);
    fprintf(out, "    dead_zone: %.3f\n", driver->m_DeadZone);
    for (uint32_t i = 0; i < dmInputDDF::MAX_GAMEPAD_COUNT; ++i)
    {
        // Ignore connected/disconnected triggers.
        if (IsIgnoredID(dmInputDDF_Gamepad_DESCRIPTOR.m_EnumValues[i].m_Value) || driver->m_Triggers[i].m_Skip) {
            continue;
        }

        fprintf(out, "    map { input: %s type: %s index: %d ",
                dmInputDDF_Gamepad_DESCRIPTOR.m_EnumValues[i].m_Name,
                dmInputDDF_GamepadType_DESCRIPTOR.m_EnumValues[driver->m_Triggers[i].m_Type].m_Name,
                driver->m_Triggers[i].m_Index);
        if (driver->m_Triggers[i].m_Type == dmInputDDF::GAMEPAD_TYPE_HAT) {
            fprintf(out, "hat_mask: %d ", driver->m_Triggers[i].m_HatMask);
        }
        for (uint32_t j = 0; j < dmInputDDF::MAX_GAMEPAD_MODIFIER_COUNT; ++j)
        {
            if (driver->m_Triggers[i].m_Modifiers[j])
                fprintf(out, "mod { mod: %s } ", dmInputDDF_GamepadModifier_DESCRIPTOR.m_EnumValues[j].m_Name);
        }

        fprintf(out, "}\n");
    }
    fprintf(out, "}\n");
}

void DumpSDLEntry(FILE* out, Driver* driver)
{
    static const uint32_t binding_order[] =
    {
        dmInputDDF::GAMEPAD_RPAD_DOWN,
        dmInputDDF::GAMEPAD_RPAD_RIGHT,
        dmInputDDF::GAMEPAD_RPAD_LEFT,
        dmInputDDF::GAMEPAD_RPAD_UP,
        dmInputDDF::GAMEPAD_BACK,
        dmInputDDF::GAMEPAD_GUIDE,
        dmInputDDF::GAMEPAD_START,
        dmInputDDF::GAMEPAD_LSTICK_CLICK,
        dmInputDDF::GAMEPAD_RSTICK_CLICK,
        dmInputDDF::GAMEPAD_LSHOULDER,
        dmInputDDF::GAMEPAD_RSHOULDER,
        dmInputDDF::GAMEPAD_LPAD_UP,
        dmInputDDF::GAMEPAD_LPAD_DOWN,
        dmInputDDF::GAMEPAD_LPAD_LEFT,
        dmInputDDF::GAMEPAD_LPAD_RIGHT,
        dmInputDDF::GAMEPAD_LSTICK_LEFT,
        dmInputDDF::GAMEPAD_LSTICK_RIGHT,
        dmInputDDF::GAMEPAD_LSTICK_UP,
        dmInputDDF::GAMEPAD_LSTICK_DOWN,
        dmInputDDF::GAMEPAD_RSTICK_LEFT,
        dmInputDDF::GAMEPAD_RSTICK_RIGHT,
        dmInputDDF::GAMEPAD_RSTICK_UP,
        dmInputDDF::GAMEPAD_RSTICK_DOWN,
        dmInputDDF::GAMEPAD_LTRIGGER,
        dmInputDDF::GAMEPAD_RTRIGGER,
    };

    fprintf(out, "%s,", driver->m_Guid);
    DumpSDLDeviceName(out, driver->m_Device);

    for (uint32_t i = 0; i < sizeof(binding_order) / sizeof(binding_order[0]); ++i)
    {
        const uint32_t trigger_id = binding_order[i];
        if (driver->m_Triggers[trigger_id].m_Skip)
            continue;

        const char* input_name = GetSDLInputName(driver, trigger_id);
        if (input_name == 0x0)
            continue;

        DumpSDLBinding(out, input_name, &driver->m_Triggers[trigger_id]);
    }

    fprintf(out, ",platform:%s,\n", GetSDLPlatformName(driver->m_Platform));
}
