#ifndef DM_MUTEX_H
#define DM_MUTEX_H

// https://developer.nintendo.com/html/online-docs/nx-en/g1kr9vj6-en/Packages/SDK/NintendoSDK/Documents/Api/HtmlNX/classnn_1_1os_1_1_mutex.html
#include <dmsdk/dlib/mutex.h> // the api
#include <nn/os/os_Mutex.h>

namespace dmMutex
{
    struct Mutex
    {
    	nn::os::MutexType m_NativeHandle;
    };

}


#endif // DM_MUTEX_H
