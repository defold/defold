// Copyright 2020-2024 The Defold Foundation
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

#ifndef DMSDK_SOCKETTYPES_WIN32_H
#define DMSDK_SOCKETTYPES_WIN32_H

// Let's only include these in a single place, in the correct order
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iphlpapi.h>

typedef int socklen_t;

namespace dmSocket
{
    #define DM_SOCKET_ERRNO WSAGetLastError()
    #define DM_SOCKET_HERRNO WSAGetLastError()
}

#endif // #ifndef DMSDK_SOCKETTYPES_WIN32_H

