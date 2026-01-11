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

#include <dlib/platform.h>

#include <graphics/graphics.h>

#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/time.h>
#include <ddf/ddf.h>

#include <hid/hid.h>
#include <input/input_ddf.h>

extern "C" void dmExportedSymbols();

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
    const char* m_Device;
    const char* m_Platform;
    float m_DeadZone;
    Trigger m_Triggers[dmInputDDF::MAX_GAMEPAD_COUNT];
};

void GetDelta(dmHID::GamepadPacket* zero_packet, dmHID::GamepadPacket* input_packet, dmInputDDF::GamepadType* gamepad_type, uint32_t* index, float* value, float* delta);
void DumpDriver(FILE* out, Driver* driver);
bool IsIgnoredTrigger(uint32_t trigger_id) {
    // Ignore connected/disconnected triggers.
    if (trigger_id == dmInputDDF::GAMEPAD_CONNECTED ||
        trigger_id == dmInputDDF::GAMEPAD_DISCONNECTED) {
        return true;
    }

    return false;
}

dmHID::HContext g_HidContext = 0;

static volatile bool g_SkipTrigger = false;

static void sig_handler(int _)
{
    (void)_;
    g_SkipTrigger = true;
}

int main(int argc, char *argv[])
{
    dmExportedSymbols();

    int result = 0;
    dmHID::HGamepad gamepad = dmHID::INVALID_GAMEPAD_HANDLE;
    dmHID::HGamepad gamepads[dmHID::MAX_GAMEPAD_COUNT];

    float wait_delay = 1.0f;
    float read_delay = 0.5f;
    float timer = 0.0f;
    float dt = 0.016667f;

    FILE* out = 0x0;

    const char* filename = "default.gamepads";
    if (argc > 1)
        filename = argv[1];

    dmGraphics::InstallAdapter();

    dmPlatform::WindowParams window_params = {};
    window_params.m_Width = 32;
    window_params.m_Height = 32;
    window_params.m_Title = "gdc";
    window_params.m_PrintDeviceInfo = false;
    window_params.m_OpenGLVersionHint = 33;
    if (dmGraphics::GetInstalledAdapterFamily() == dmGraphics::ADAPTER_FAMILY_OPENGLES)
    {
        window_params.m_GraphicsApi = dmPlatform::PLATFORM_GRAPHICS_API_OPENGLES;
    }
    else
    {
        window_params.m_GraphicsApi = dmPlatform::PLATFORM_GRAPHICS_API_OPENGL;
    }
    window_params.m_ContextAlphabits = 8;

    dmPlatform::HWindow window = dmPlatform::NewWindow();
    dmPlatform::OpenWindow(window, window_params);
    dmPlatform::ShowWindow(window);

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

    g_HidContext = dmHID::NewContext(dmHID::NewContextParams());
    dmHID::SetWindow(g_HidContext, window);
    dmHID::Init(g_HidContext);

retry:
    uint32_t gamepad_count = 0;
    dmHID::Update(g_HidContext);

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
        printf("No connected gamepads, rechecking in 3 seconds. "); fflush(stdout);
        dmTime::Sleep(1000000);
        printf("2.. "); fflush(stdout);
        dmTime::Sleep(1000000);
        printf("1.. "); fflush(stdout);
        dmTime::Sleep(1000000);
        printf("Rechecking...\n");
        goto retry;
    }
    if (gamepad_count > 1)
    {
        for (uint32_t i = 0; i < gamepad_count; ++i)
        {
            char device_name[dmHID::MAX_GAMEPAD_NAME_LENGTH];
            dmHID::GetGamepadDeviceName(g_HidContext, gamepads[i], device_name);
            printf("%d: %s\n", i+1, device_name);
        }
        printf("\n* Which gamepad do you want to calibrate? [1-%d] ", gamepad_count);
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
            goto bail;
        }
    }
    else
    {
        gamepad = gamepads[0];
    }

    char device_name[dmHID::MAX_GAMEPAD_NAME_LENGTH];
    dmHID::GetGamepadDeviceName(g_HidContext, gamepad, device_name);

    printf("\n%s will be added to %s\n\n", device_name, filename);

    Driver driver;
    memset(&driver, 0, sizeof(Driver));
    driver.m_Device = device_name;
    driver.m_Platform = DM_PLATFORM;
    driver.m_DeadZone = 0.2f;

    dmHID::GamepadPacket prev_packet;
    dmHID::GamepadPacket packet;
    dmHID::GetGamepadPacket(gamepad, &packet);
    prev_packet = packet;

    dmInputDDF::GamepadType gamepad_type;
    uint32_t index;
    float value;
    float delta;

    // Register signal handler so user can press Ctrl+C to skip a trigger.
    signal(SIGINT, sig_handler);

    printf("* Don't press anything on the gamepad...\n");
    timer = wait_delay;
    while (true)
    {
        dmHID::Update(g_HidContext);
        dmHID::GetGamepadPacket(gamepad, &packet);
        GetDelta(&prev_packet, &packet, &gamepad_type, &index, &value, &delta);
        if (dmMath::Abs(delta) < 0.01f)
        {
            timer -= dt;
            if (timer < 0.0f)
            {
                prev_packet = packet;
                break;
            }
        }
        else
        {
            timer = wait_delay;
            prev_packet = packet;
        }
    }

    printf("* NOTE: If gamepad is missing a trigger, you can skip it with Ctrl+C.\n");
    for (uint8_t i = 0; i < dmInputDDF::MAX_GAMEPAD_COUNT; ++i)
    {
        // Ignore connected/disconnected triggers.
        if (IsIgnoredTrigger(dmInputDDF_Gamepad_DESCRIPTOR.m_EnumValues[i].m_Value)) {
            continue;
        }

        State state = STATE_WAITING;
        timer = wait_delay;
        bool run = true;
        while (run)
        {
            if (g_SkipTrigger) {
                printf("Skipping trigger.\n");
                driver.m_Triggers[i].m_Skip = true;
                g_SkipTrigger = false;
                break;
            }

            dmHID::Update(g_HidContext);
            dmHID::GetGamepadPacket(gamepad, &packet);
            GetDelta(&prev_packet, &packet, &gamepad_type, &index, &value, &delta);
            switch (state)
            {
            case STATE_WAITING:
                if (dmMath::Abs(delta) < 0.2f)
                {
                    state = STATE_READING;
                    printf("* Press %s...\n", dmInputDDF_Gamepad_DESCRIPTOR.m_EnumValues[i].m_Name);
                    timer = read_delay;
                }
                break;
            case STATE_READING:
                if (dmMath::Abs(delta) > 0.7f)
                {
                    timer -= dt;
                    if (timer < 0.0f)
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
                }
                else
                {
                    timer = read_delay;
                }
                break;
            }
            dmTime::Sleep((uint32_t)(dt * 1000000));
        }
    }

    out = fopen(filename, "w");
    if (out != 0x0)
    {
        DumpDriver(out, &driver);
    }
    else
    {
        printf("Could not open %s to write to, dumping to stdout instead.\n\n", filename);
        DumpDriver(stdout, &driver);
    }

    printf("Bye!\n");

bail:
    dmHID::Final(g_HidContext);
    dmHID::DeleteContext(g_HidContext);
    if (out != 0x0)
        fclose(out);

    return result;
}

void GetDelta(dmHID::GamepadPacket* prev_packet, dmHID::GamepadPacket* packet, dmInputDDF::GamepadType* gamepad_type, uint32_t* index, float* value, float* delta)
{
    float max_delta = -1.0f;
    for (uint32_t i = 0; i < dmHID::MAX_GAMEPAD_AXIS_COUNT; ++i)
    {
        if (dmMath::Abs(packet->m_Axis[i] - prev_packet->m_Axis[i]) > max_delta)
        {
            max_delta = dmMath::Abs(packet->m_Axis[i] - prev_packet->m_Axis[i]);
            if (gamepad_type != 0x0)
                *gamepad_type = dmInputDDF::GAMEPAD_TYPE_AXIS;
            if (index != 0x0)
                *index = i;
            if (value != 0x0)
                *value = packet->m_Axis[i];
            if (delta != 0x0)
                *delta = packet->m_Axis[i] - prev_packet->m_Axis[i];
        }
    }
    for (uint32_t i = 0; i < dmHID::MAX_GAMEPAD_HAT_COUNT; ++i)
    {
        if (prev_packet->m_Hat[i] != packet->m_Hat[i]) {
            if (gamepad_type != 0x0)
                *gamepad_type = dmInputDDF::GAMEPAD_TYPE_HAT;
            if (index != 0x0)
                *index = i;
            if (value != 0x0)
                *value = packet->m_Hat[i];
            if (delta != 0x0)
                *delta = 1.0f;
        }
    }
    for (uint32_t i = 0; i < dmHID::MAX_GAMEPAD_BUTTON_COUNT; ++i)
    {
        if (dmHID::GetGamepadButton(packet, i))
        {
            if (gamepad_type != 0x0)
                *gamepad_type = dmInputDDF::GAMEPAD_TYPE_BUTTON;
            if (index != 0x0)
                *index = i;
            if (value != 0x0)
                *value = 1.0f;
            if (delta != 0x0)
                *delta = 1.0f;
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
        if (IsIgnoredTrigger(dmInputDDF_Gamepad_DESCRIPTOR.m_EnumValues[i].m_Value) || driver->m_Triggers[i].m_Skip) {
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
