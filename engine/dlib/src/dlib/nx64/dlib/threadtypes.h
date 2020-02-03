#ifndef DM_THREADTYPES_H
#define DM_THREADTYPES_H

//https://developer.nintendo.com/html/online-docs/nx-en/g1kr9vj6-en/Packages/SDK/NintendoSDK/Documents/Api/HtmlNX/structnn_1_1os_1_1_thread_type.html
#include <nn/os/os_ThreadTypes.h>
#include <nn/os/os_ThreadLocalStorageCommon.h>

namespace dmThread
{
    typedef nn::os::ThreadType* Thread;
    typedef nn::os::TlsSlot TlsKey;
}

#endif // DM_THREADTYPES_H