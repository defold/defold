
#include "switch_private.h"
#include <stdint.h>
#include <dlib/log.h>

#include <nn/fs.h>
#include <nn/account/account_Api.h>
#include <nn/account/account_ApiForApplications.h>
#include <nn/account/account_Result.h>
#include <nn/account/account_Selector.h>


namespace dmSwitch {
namespace Account {
    struct NativeUserInfo
    {
        nn::account::Uid        m_User;
        nn::account::UserHandle m_UserHandle;
    };

    UserResult InitUser()
    {
        nn::account::Initialize();
        return RESULT_USER_OK;
    }

    static nn::Result OpenUser(nn::account::Uid user, UserInfo* info)
    {
        nn::account::UserHandle userHandle;
        nn::Result result = nn::account::OpenUser(&userHandle, user);
        if (!result.IsSuccess())
        {
            dmLogWarning("Failed to open user handle\n");
            return result;
        }

        result = nn::fs::EnsureSaveData(user);
        if ( nn::fs::ResultUsableSpaceNotEnough::Includes(result) )
        {
            // Error handling when the application does not have enough memory is required for the nn::fs::EnsureSaveData() function.
            // The system automatically displays a message indicating that there was not enough capacity to create save data in the error viewer.
            // The application must offer options to cancel account selection and to return to the prior scene.
            dmLogError("Usable save space not enough.\n");
            nn::account::CloseUser(userHandle);
            return result;
        }

        result = nn::fs::MountSaveData("save", user);
        if (!result.IsSuccess())
        {
            dmLogError("Failed to mount save:/\n");
            nn::account::CloseUser(userHandle);
            return result;
        }

        NativeUserInfo* native_info = (NativeUserInfo*)malloc(sizeof(NativeUserInfo));
        native_info->m_UserHandle = userHandle;
        native_info->m_User = user;
        info->m_NativeInfo = native_info;

        nn::account::Nickname nickname;
        nn::account::GetNickname(&nickname, user);
        info->m_Name = strdup(nickname.name);

        return result;
    }

    UserResult SelectUser(UserInfo* info)
    {
        // Display the user account selection screen and make the user select a user account.
        // nn::account::ResultCancelledByUser is returned if the process is canceled by user operation.
        nn::account::Uid user;

        nn::Result result = nn::account::ShowUserSelector(&user);
        if( nn::account::ResultCancelledByUser::Includes(result) )
        {
            dmLogWarning("The user selection was canceled.\n");
            return RESULT_USER_CANCELLED;
        }

        result = OpenUser(user, info);
        if (result.IsFailure())
        {
            dmLogWarning("Failed to open user\n");
            return RESULT_USER_ERROR;
        }

        return RESULT_USER_OK;
    }

    UserResult OpenLastUser(UserInfo* info)
    {
        nn::account::Uid user;
        nn::Result result = nn::account::GetLastOpenedUser(&user);
        if(result.IsFailure()) {
            return RESULT_USER_ERROR;
        }

        if (!user) {
            dmLogError("The user does not exists anymore\n");
            return RESULT_USER_NOT_EXIST;
        }

        result = OpenUser(user, info);
        if (result.IsFailure())
        {
            dmLogWarning("Failed to open user\n");
            return RESULT_USER_ERROR;
        }

        return RESULT_USER_OK;
    }

    void CloseUser(UserInfo* info)
    {
        NativeUserInfo* native_info = (NativeUserInfo*)info->m_NativeInfo;
        free((void*)info->m_Name);
        free((void*)native_info);
        nn::account::CloseUser(native_info->m_UserHandle);
    }
}
}