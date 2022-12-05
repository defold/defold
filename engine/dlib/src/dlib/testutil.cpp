// Copyright 2020-2022 The Defold Foundation
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

#include "testutil.h"
#include <dlib/dstrings.h>

#if !defined(DM_HOSTFS)
    #define DM_HOSTFS ""
#endif

namespace dmTestUtil
{

void GetSocketsFromConfig(dmConfigFile::HConfig config, int* socket, int* socket_ssl, int* socket_ssl_test)
{
    if( socket != 0 )
    {
        *socket = dmConfigFile::GetInt(config, "server.socket", -1);
    }
    if( socket_ssl != 0 )
    {
        *socket_ssl = dmConfigFile::GetInt(config, "server.socket_ssl", -1);
    }
    if( socket_ssl_test != 0 )
    {
        *socket_ssl_test = dmConfigFile::GetInt(config, "server.socket_ssl_test", -1);
    }
}

const char* GetIpFromConfig(dmConfigFile::HConfig config, char* ip, uint32_t iplen)
{
    const char* _ip = dmConfigFile::GetString(config, "server.ip", 0);
    if (!_ip) {
        return 0;
    }

    uint32_t nwritten = dmSnPrintf(ip, iplen, "%s", _ip);
    if (nwritten >= iplen)
        return 0;
    return ip;
}

const char* MakeHostPath(char* dst, uint32_t dst_len, const char* path)
{
    dmStrlCpy(dst, DM_HOSTFS, dst_len);
    dmStrlCat(dst, path, dst_len);
    return dst;
}


} // namespace
