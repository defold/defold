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

        /*#
         * Initialize the user system
         */
        UserResult InitUser();

        /*#
         * Keep this alive during user operations (e.g. save)
         */
        UserResult SelectUser(UserInfo* info);

        /*#
         * Keep this alive during user operations (e.g. save)
         * Currently used for unit tests
         */
        UserResult OpenLastUser(UserInfo* info);

        /*# Close an opened user handle
         */
        void CloseUser(UserInfo* info);
    }

    // HID

}

#endif // DM_SWITCH_PRIVATE_H