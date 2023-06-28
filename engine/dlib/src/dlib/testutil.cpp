// Copyright 2020-2023 The Defold Foundation
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
#include <dlib/log.h>
#include <dlib/memory.h>
#include <dlib/path.h>
#include <stdarg.h>

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
    int len = dmStrlCpy(dst, DM_HOSTFS, dst_len);
    if (len > 0 && dst[len-1] != '/')
        len = dmStrlCat(dst+len, "/", dst_len-len);
    dmStrlCat(dst+len, path, dst_len-len);

    dmPath::Normalize(dst, dst, dst_len);
    return dst;
}

const char* MakeHostPathf(char* dst, uint32_t dst_len, const char* path_format, ...)
{
    int len = dmStrlCpy(dst, DM_HOSTFS, dst_len);
    if (len > 0 && dst[len-1] != '/')
        len += dmStrlCat(dst+len, "/", dst_len-len);

    va_list argp;
    va_start(argp, path_format);

#if defined(_WIN32)
    int result = _vsnprintf_s(dst+len, dst_len-len, _TRUNCATE, path_format, argp);
#else
    int result = vsnprintf(dst+len, dst_len-len, path_format, argp);
#endif
    (void)result;
    va_end(argp);

    dmPath::Normalize(dst, dst, dst_len);
    return dst;
}

uint8_t* ReadFile(const char* path, uint32_t* file_size)
{
    FILE* f = fopen(path, "rb");
    if (!f)
    {
        dmLogError("Failed to load file %s", path);
        return 0;
    }

    if (fseek(f, 0, SEEK_END) != 0)
    {
        fclose(f);
        dmLogError("Failed to seek to end of file %s", path);
        return 0;
    }

    size_t size = (size_t)ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0)
    {
        fclose(f);
        dmLogError("Failed to seek to start of file %s", path);
        return 0;
    }

    uint8_t* buffer;
    dmMemory::AlignedMalloc((void**)&buffer, 16, size);

    size_t nread = fread(buffer, 1, size, f);
    fclose(f);

    if (nread != size)
    {
        dmMemory::AlignedFree((void*)buffer);
        dmLogError("Failed to read %u bytes from file %s", (uint32_t)size, path);
        return 0;
    }

    *file_size = size;
    return buffer;
}

uint8_t* ReadHostFile(const char* path, uint32_t* file_size)
{
    char path_buffer1[256];
    const char* file_path = dmTestUtil::MakeHostPath(path_buffer1, sizeof(path_buffer1), path);
    return dmTestUtil::ReadFile(file_path, file_size);
}


} // namespace
