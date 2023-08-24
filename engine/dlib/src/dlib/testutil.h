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

#pragma once

#include <dlib/configfile.h>

namespace dmTestUtil
{
    void GetSocketsFromConfig(dmConfigFile::HConfig config, int* socket, int* ssl_socket, int* test_sslsocket);
    const char* GetIpFromConfig(dmConfigFile::HConfig config, char* ip, uint32_t iplen);

    // Prepends DM_HOSTFS to the path
    // Returns the dst buffer
    // dst buffer is always returned, even if truncation happened
    const char* MakeHostPath(char* dst, uint32_t dst_len, const char* path);

    // Prepends DM_HOSTFS to the path
    // Returns the dst buffer
    // dst buffer is always returned, even if truncation happened
#ifdef __GNUC__
    const char* MakeHostPathf(char* dst, uint32_t dst_len, const char* path_format, ...) __attribute__ ((format (printf, 3, 4)));
#else
    const char* MakeHostPathf(char* dst, uint32_t dst_len, const char* path_format, ...);
#endif

    // Reads a file and returns the contents
    // * Free memory with dmMemory::AlignedFree
    // * DM_HOSTFS is added automatically to the path argument!
    uint8_t* ReadFile(const char* path, uint32_t* file_size);

    // Reads a file and returns the contents
    // * Free memory with dmMemory::AlignedFree
    // * DM_HOSTFS is added automatically to the path argument!
    uint8_t* ReadHostFile(const char* path, uint32_t* file_size);
}
