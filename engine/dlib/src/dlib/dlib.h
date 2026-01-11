// Copyright 2020-2026 The Defold Foundation
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

#ifndef DM_DLIB
#define DM_DLIB

#include <stdint.h>

namespace dLib
{
    #define DM_FEATURE_BIT_NONE                  0
    #define DM_FEATURE_BIT_SOCKET_CLIENT_TCP  (1 << 0)
    #define DM_FEATURE_BIT_SOCKET_CLIENT_UDP  (1 << 1)
    #define DM_FEATURE_BIT_SOCKET_SERVER_TCP  (1 << 2)
    #define DM_FEATURE_BIT_SOCKET_SERVER_UDP  (1 << 3)
    #define DM_FEATURE_BIT_THREADS            (1 << 4)

    /**
     * Enable/disable debug mode. Debug mode enables:
     *  - Reverse hash
     *  - Profiler
     *  - Log server
     *  Default value is true
     *  Other system might use IsDebugMode() to determine whether debug-mode is enabled or not.
     * @param debug_mode
     */
    void SetDebugMode(bool debug_mode);

    /**
     * Check if debug mode is enabled
     * @return true if debug-mode is enabled
     */
    bool IsDebugMode();

    /**
     * Check if a feature (or more) is/are available for current target platform.
     * @param feature Feature bit (see FEATURE_BIT_* defines)
     */
    bool FeaturesSupported(uint32_t features);
}

#endif // DM_DLIB
