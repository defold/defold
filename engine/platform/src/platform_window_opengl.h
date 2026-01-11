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

#ifndef DM_PLATFORM_WINDOW_OPENGL_H
#define DM_PLATFORM_WINDOW_OPENGL_H

namespace dmPlatform
{
    int32_t OpenGLGetDefaultFramebufferId();

    static inline bool OpenGLGetVersion(int32_t version, uint32_t* major, uint32_t* minor)
    {
        uint32_t tmp_major = version / 10;
        uint32_t tmp_minor = version % 10;

        bool use_highest_version_supported = true;

        // Osx doesn't support using the "highest version available"
    #ifdef __MACH__
        use_highest_version_supported = false;
    #endif

        if ((version == 0 && use_highest_version_supported) ||   // Highest available
            (tmp_major == 3 && tmp_minor == 3) ||              // Only 3.3 is supported from 3.x
            (tmp_major == 4 && tmp_minor >= 0 && *minor <= 6)) // Only 4.0 - 4.6 are proper versions
        {
            *major = tmp_major;
            *minor = tmp_minor;
            return true;
        }
        return false;
    }
}

#endif // DM_PLATFORM_WINDOW_OPENGL_H
