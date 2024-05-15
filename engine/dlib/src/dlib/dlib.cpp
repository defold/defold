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

#include <stdint.h>
#include "dlib.h"

namespace dLib
{
    bool g_DebugMode = true;
    void SetDebugMode(bool debug_mode)
    {
        g_DebugMode = debug_mode;
    }

    bool IsDebugMode()
    {
        return g_DebugMode;
    }

    bool FeaturesSupported(uint32_t features)
    {
#if defined(__EMSCRIPTEN__)
        return false;
#else
        return (features & (DM_FEATURE_BIT_SOCKET_CLIENT_TCP |
                            DM_FEATURE_BIT_SOCKET_CLIENT_UDP |
                            DM_FEATURE_BIT_SOCKET_SERVER_TCP |
                            DM_FEATURE_BIT_SOCKET_SERVER_UDP |
                            DM_FEATURE_BIT_THREADS)) == features;
#endif
    }
}
