// Creating a small app test for verifying input
// not an actual unit test

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <app/app.h>
#include <dlib/log.h>
#include <dlib/time.h>
#include <hid.h>


static void GamepadConnectivityCallback(uint32_t gamepad_index, bool connected, void* userdata);


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
    dmHID::HContext m_HidContext;
    dmHID::GamepadPacket m_OldPackets[dmHID::MAX_GAMEPAD_COUNT];
} g_EngineCtx;

static void* EngineCreate(int argc, char** argv)
{
    EngineCtx* engine = (EngineCtx*)&g_EngineCtx;
    memset(engine, 0, sizeof(EngineCtx));

    dmHID::NewContextParams new_hid_params = dmHID::NewContextParams();
    new_hid_params.m_GamepadConnectivityCallback = GamepadConnectivityCallback;

    int32_t use_accelerometer = false;
    if (use_accelerometer) {
        dmHID::EnableAccelerometer(); // Creates and enables the accelerometer
    }
    new_hid_params.m_IgnoreAcceleration = use_accelerometer ? 0 : 1;

    engine->m_HidContext = dmHID::NewContext(new_hid_params);
    dmHID::Init(engine->m_HidContext);

    return engine;
}

static void EngineDestroy(void* _engine)
{
    EngineCtx* engine = (EngineCtx*)_engine;
    dmHID::Final(engine->m_HidContext);
    dmHID::DeleteContext(engine->m_HidContext);
}

static dmApp::Result EngineUpdate(void* _engine)
{
    EngineCtx* engine = (EngineCtx*)_engine;

    dmHID::Update(engine->m_HidContext);


#define LOG if (packet_changed) printf
//#define LOG

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

        //bool GetGamepadHat(GamepadPacket* packet, uint32_t hat, uint8_t& hat_value);

        LOG("AXIS: ");

        uint32_t numaxis = dmHID::GetGamepadAxisCount(pad);
        for (uint32_t a = 0; a < numaxis; ++a)
        {
            LOG("%1.3f ", packet.m_Axis[a]);
        }

        LOG("\n");
    }

    dmTime::Sleep(100000);

    return dmApp::RESULT_OK;

#undef LOG
}

static void EngineGetResult(void* _engine, int* run_action, int* exit_code, int* argc, char*** argv)
{
    EngineCtx* engine = (EngineCtx*)_engine;
    (void)engine;
}

static void GamepadConnectivityCallback(uint32_t gamepad_index, bool connected, void* userdata)
{
    dmHID::HGamepad pad = dmHID::GetGamepad(g_EngineCtx.m_HidContext, gamepad_index);

    if (connected) {
        const char* name;
        dmHID::GetGamepadDeviceName(pad, &name);
        printf("Gamepad %d connected: %s\n", gamepad_index, name);
    } else {
        printf("Gamepad %d disconnected\n", gamepad_index);
    }
}

int main(int argc, char **argv)
{
    memset(&g_EngineCtx, 0, sizeof(g_EngineCtx));

    dmApp::Params params;
    params.m_AppCtx = 0;
    params.m_AppCreate = AppCreate;
    params.m_AppDestroy = AppDestroy;
    params.m_EngineCreate = EngineCreate;
    params.m_EngineDestroy = EngineDestroy;
    params.m_EngineUpdate = EngineUpdate;
    params.m_EngineGetResult = EngineGetResult;

    return dmApp::Run(&params);
}
