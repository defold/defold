#include "switch_private.h"
#include <stdint.h>
#include <dlib/log.h>

#include <nn/hid.h>
#include <nn/hid/hid_Npad.h>
#include <nn/hid/hid_NpadJoy.h>
#include <nn/hid/hid_KeyboardKey.h>


namespace dmSwitch {
namespace Hid {

    static inline nn::hid::NpadIdType GetNpadIdFromIndex(int index) {
        // NOTE: This list is also the same as in hid_native.cpp, which is also the order listed in the original enum
        nn::hid::NpadIdType ids[] = {
            nn::hid::NpadId::Handheld, nn::hid::NpadId::No1, nn::hid::NpadId::No2, nn::hid::NpadId::No3, nn::hid::NpadId::No4,
            nn::hid::NpadId::No5, nn::hid::NpadId::No6, nn::hid::NpadId::No7, nn::hid::NpadId::No8,
        };
        return ids[index];
    }

    Result GetGamepadColor(int gamepad, float rgbleft[4], float rgbright[4])
    {
        nn::hid::NpadControllerColor colorleft;
        nn::hid::NpadControllerColor colorright;
        nn::Result result = nn::hid::GetNpadControllerColor(&colorleft, &colorright, GetNpadIdFromIndex(gamepad));
        if (nn::hid::ResultNpadColorNotAvailable::Includes(result))
        {
            return RESULT_NO_COLOR;
        }
        if (nn::hid::ResultNpadControllerNotConnected ::Includes(result))
        {
            return RESULT_NOT_CONNECTED;
        }

        rgbleft[0] = colorleft.main.v[0] / 255.0f;
        rgbleft[1] = colorleft.main.v[1] / 255.0f;
        rgbleft[2] = colorleft.main.v[2] / 255.0f;
        rgbleft[3] = colorleft.main.v[3] / 255.0f;

        rgbright[0] = colorright.main.v[0] / 255.0f;
        rgbright[1] = colorright.main.v[1] / 255.0f;
        rgbright[2] = colorright.main.v[2] / 255.0f;
        rgbright[3] = colorright.main.v[3] / 255.0f;

        return RESULT_OK;
    }


    Result SetGamepadAssignmentMode(int gamepad, int mode)
    {
        if (mode == ASSIGN_MODE_DUAL)
            nn::hid::SetNpadJoyAssignmentModeDual(GetNpadIdFromIndex(gamepad));
        else
            nn::hid::SetNpadJoyAssignmentModeSingle(GetNpadIdFromIndex(gamepad));
        return RESULT_OK;
    }

    Result SetGamepadSupportedStyleset(int mask)
    {
        nn::hid::NpadStyleSet styleset;
        if (mask & GAMEPAD_STYLE_HANDHELD)  styleset |= nn::hid::NpadStyleHandheld::Mask;
        if (mask & GAMEPAD_STYLE_FULLKEY)   styleset |= nn::hid::NpadStyleFullKey::Mask;
        if (mask & GAMEPAD_STYLE_DUAL)      styleset |= nn::hid::NpadStyleJoyDual::Mask;
        if (mask & GAMEPAD_STYLE_JOYLEFT)   styleset |= nn::hid::NpadStyleJoyLeft::Mask;
        if (mask & GAMEPAD_STYLE_JOYRIGHT)  styleset |= nn::hid::NpadStyleJoyRight::Mask;

        nn::hid::SetSupportedNpadStyleSet(styleset);
        return RESULT_OK;
    }
    
    Result ShowControllerSupport(bool multi_player)
    {
        nn::hid::ControllerSupportArg controllerArg;
        controllerArg.SetDefault();
        controllerArg.enableSingleMode = !multi_player;
        nn::Result result = nn::hid::ShowControllerSupport(controllerArg);

        if (nn::hid::ResultControllerSupportCanceled::Includes(result))
            return RESULT_USER_CANCELLED;
        if (nn::hid::ResultControllerSupportNotSupportedNpadStyle::Includes(result))
            return RESULT_UNSUPPORTED;
        return RESULT_OK;
    }
}}