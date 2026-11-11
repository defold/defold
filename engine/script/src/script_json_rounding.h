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

#ifndef DM_SCRIPT_JSON_ROUNDING_H
#define DM_SCRIPT_JSON_ROUNDING_H

#if defined(DM_PLATFORM_VENDOR) && __has_include("script_json_rounding_vendor.h")
#include "script_json_rounding_vendor.h"
#else
#include <fenv.h>

namespace dmScript
{
    enum JsonRoundingMode
    {
        JSON_ROUND_NEAREST    = FE_TONEAREST,
        JSON_ROUND_DOWNWARD   = FE_DOWNWARD,
        JSON_ROUND_UPWARD     = FE_UPWARD,
        JSON_ROUND_TOWARDZERO = FE_TOWARDZERO,
    };

    inline int GetJsonRoundingMode()
    {
        return fegetround();
    }

    inline int SetJsonRoundingMode(int mode)
    {
        return fesetround(mode);
    }
}
#endif

#endif // DM_SCRIPT_JSON_ROUNDING_H
