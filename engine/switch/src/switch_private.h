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

    // HID

}

#endif // DM_SWITCH_PRIVATE_H