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

#ifndef DM_TEXC_JNI_PRIVATE_H
#define DM_TEXC_JNI_PRIVATE_H

#include <stdlib.h>

namespace dmTexc
{
    // The generated JNI converters copy data with new[] and paths with strdup.
    // These copies belong to the call, not to the Java settings or the encoder.
    template <typename T>
    struct ScopedEncodeSettings
    {
        T m_Settings;

        ScopedEncodeSettings() : m_Settings() {}
        ~ScopedEncodeSettings()
        {
            delete[] m_Settings.m_Data;
            free((void*)m_Settings.m_Path);
        }

    private:
        ScopedEncodeSettings(const ScopedEncodeSettings&);
        ScopedEncodeSettings& operator=(const ScopedEncodeSettings&);
    };
}

#endif // DM_TEXC_JNI_PRIVATE_H
