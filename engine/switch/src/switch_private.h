#if !defined(DM_SWITCH_PRIVATE_H)
#define DM_SWITCH_PRIVATE_H

namespace dmSwitch
{
    namespace Account
    {
        struct UserInfo
        {
            void* m_NativeInfo;
            const char* m_Name;
        };

        enum UserResult
        {
            RESULT_USER_OK,
            RESULT_USER_CANCELLED,
            RESULT_USER_NOT_EXIST,
            RESULT_USER_ERROR,
        };

        /*# initialize the user system
         * 
         * Initialize the user system
         */
        UserResult InitUser();

        /*# get the user selected user
         * @note Keep this alive during user operations (e.g. save)
         */
        UserResult GetUser(UserInfo* info);

        /*# mount user save data filesystem (save:)
         *
         * Mount user save data filesystem (save:)
         *
         * @note There needs to be an opened user handle
         */
        UserResult MountUserSaveData(UserInfo* info);

        /*# open the user selection screen
         * 
         * Open the user selection screen
         * 
         * @note Keep this alive during user operations (e.g. save)
         */
        UserResult SelectUser(UserInfo* info);

        /*# open the last opened user
         *
         * Open the last opened user
         *
         * @note Keep this alive during user operations (e.g. save)
         * @note Currently used for unit tests, or it you wish to skip the user selection
         */
        UserResult OpenLastUser(UserInfo* info);

        /*# Close an opened user handle
         */
        void CloseUser(UserInfo* info);
    }

    namespace Hid
    {
        enum Result
        {
            RESULT_OK,
            RESULT_NOT_CONNECTED,
            RESULT_NO_COLOR,
            RESULT_ERROR,
        };

        /*# gets the color of the controller
         *
         * Gets the color of the controller
         *
         * @param gamepad [type:int] the gamepad id
         * @param rgbleft [type:float[3]] the color of the left joypad
         * @param rgbright [type:float[3]] the color of the right joypad
         * @return RESULT_OK is successful, RESULT_NOT_CONNECTED if the controller wasn't connected, RESULT_NO_COLOR if the controller has no color
         */
        Result GetGamepadColor(int gamepad, float rgbaleft[4], float rgbaright[4]);


        enum GamepadAssignmentMode
        {
            ASSIGN_MODE_DUAL,
            ASSIGN_MODE_SINGLE,
        };

        /*#
         * @param gamepad [type:int] the gamepad id
         * @param mode [type:int] 0 = dual, 1 = single
         */
        Result SetGamepadAssignmentMode(int gamepad, int mode);

        enum GamepadStyle
        {
            GAMEPAD_STYLE_HANDHELD  = 1 << 0,
            GAMEPAD_STYLE_FULLKEY   = 1 << 1,
            GAMEPAD_STYLE_DUAL      = 1 << 2,
            GAMEPAD_STYLE_JOYLEFT   = 1 << 3,
            GAMEPAD_STYLE_JOYRIGHT  = 1 << 4,
        };

        /*# set the allowed gamepad styles
         *
         * set the allowed gamepad styles
         *
         * @param mask [type:int] the mask of bits in the enum GamepadStyle
         */
        Result SetGamepadSupportedStyleset(int mask);
    }
}

#endif // DM_SWITCH_PRIVATE_H