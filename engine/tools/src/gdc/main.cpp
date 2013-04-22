#if defined(_MSC_VER)
#include <dlib/safe_windows.h>
#endif

#include <stdio.h>
#include <string.h>

#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/platform.h>
#include <dlib/time.h>
#include <ddf/ddf.h>

#include <hid/hid.h>
#include <input/input_ddf.h>

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
};

struct Driver
{
    const char* m_Device;
    const char* m_Platform;
    float m_DeadZone;
    Trigger m_Triggers[dmInputDDF::MAX_GAMEPAD_COUNT];
};

void GetDelta(dmHID::GamepadPacket* zero_packet, dmHID::GamepadPacket* input_packet, bool* axis, uint32_t* index, float* value, float* delta);
void DumpDriver(FILE* out, Driver* driver);

dmHID::HContext g_HidContext = 0;

int main(int argc, char *argv[])
{
    int result = 0;
    dmHID::HGamepad gamepad = dmHID::INVALID_GAMEPAD_HANDLE;
    dmHID::HGamepad gamepads[dmHID::MAX_GAMEPAD_COUNT];

    float wait_delay = 1.0f;
    float read_delay = 0.5f;
    float timer = 0.0f;
    float dt = 0.016667f;

    FILE* out = 0x0;

    uint32_t gamepad_count = 0;

    const char* filename = "default.gamepads";
    if (argc > 1)
        filename = argv[1];

    g_HidContext = dmHID::NewContext(dmHID::NewContextParams());
    dmHID::Init(g_HidContext);
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
        printf("No connected gamepads, bye!\n");
        goto bail;
    }
    if (gamepad_count > 1)
    {
        for (uint32_t i = 0; i < gamepad_count; ++i)
        {
            const char* device_name;
            dmHID::GetGamepadDeviceName(gamepads[i], &device_name);
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
            printf("Invalid input, try again: ");
        }
    }
    else
    {
        gamepad = gamepads[0];
    }

    const char* device_name;
    dmHID::GetGamepadDeviceName(gamepad, &device_name);

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

    bool axis;
    uint32_t index;
    float value;
    float delta;

    printf("* Don't press anything on the gamepad...\n");
    timer = wait_delay;
    while (true)
    {
        dmHID::Update(g_HidContext);
        dmHID::GetGamepadPacket(gamepad, &packet);
        GetDelta(&prev_packet, &packet, &axis, &index, &value, &delta);
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
    for (uint8_t i = 0; i < dmInputDDF::MAX_GAMEPAD_COUNT; ++i)
    {
        State state = STATE_WAITING;
        timer = wait_delay;
        bool run = true;
        while (run)
        {
            dmHID::Update(g_HidContext);
            dmHID::GetGamepadPacket(gamepad, &packet);
            GetDelta(&prev_packet, &packet, &axis, &index, &value, &delta);
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
                        driver.m_Triggers[i].m_Type = axis ? dmInputDDF::GAMEPAD_TYPE_AXIS : dmInputDDF::GAMEPAD_TYPE_BUTTON;
                        driver.m_Triggers[i].m_Index = index;
                        if (dmMath::Abs(delta) > 1.5f)
                            driver.m_Triggers[i].m_Modifiers[dmInputDDF::GAMEPAD_MODIFIER_SCALE] = true;
                        else if (axis)
                            driver.m_Triggers[i].m_Modifiers[dmInputDDF::GAMEPAD_MODIFIER_CLAMP] = true;
                        if (delta < 0.0f)
                            driver.m_Triggers[i].m_Modifiers[dmInputDDF::GAMEPAD_MODIFIER_NEGATE] = true;
                        printf("\ttype: %s, index: %d, value: %.2f\n", axis ? "axis" : "button", index, value);
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

void GetDelta(dmHID::GamepadPacket* prev_packet, dmHID::GamepadPacket* packet, bool* axis, uint32_t* index, float* value, float* delta)
{
    float max_delta = -1.0f;
    for (uint32_t i = 0; i < dmHID::MAX_GAMEPAD_AXIS_COUNT; ++i)
    {
        if (dmMath::Abs(packet->m_Axis[i] - prev_packet->m_Axis[i]) > max_delta)
        {
            max_delta = dmMath::Abs(packet->m_Axis[i] - prev_packet->m_Axis[i]);
            if (axis != 0x0)
                *axis = true;
            if (index != 0x0)
                *index = i;
            if (value != 0x0)
                *value = packet->m_Axis[i];
            if (delta != 0x0)
                *delta = packet->m_Axis[i] - prev_packet->m_Axis[i];
        }
    }
    for (uint32_t i = 0; i < dmHID::MAX_GAMEPAD_BUTTON_COUNT; ++i)
    {
        if (dmHID::GetGamepadButton(packet, i))
        {
            if (axis != 0x0)
                *axis = false;
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
        fprintf(out, "    map { input: %s type: %s index: %d ",
                dmInputDDF_Gamepad_DESCRIPTOR.m_EnumValues[i].m_Name,
                dmInputDDF_GamepadType_DESCRIPTOR.m_EnumValues[driver->m_Triggers[i].m_Type].m_Name,
                driver->m_Triggers[i].m_Index);
        for (uint32_t j = 0; j < dmInputDDF::MAX_GAMEPAD_MODIFIER_COUNT; ++j)
        {
            if (driver->m_Triggers[i].m_Modifiers[j])
                fprintf(out, "mod { mod: %s } ", dmInputDDF_GamepadModifier_DESCRIPTOR.m_EnumValues[j].m_Name);
        }
        fprintf(out, "}\n");
    }
    fprintf(out, "}\n");
}
