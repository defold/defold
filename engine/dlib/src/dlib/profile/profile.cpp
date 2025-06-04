// Copyright 2020-2025 The Defold Foundation
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

#include "profile.h"
#include <stdio.h>

namespace dmProfile
{

// Unit test only
static void PrintIndent(int indent)
{
    for (int i = 0; i < indent; ++i) {
        printf("  ");
    }
}

#if defined(__ANDROID__)
    #define PROFILE_FMT_U64 "%llu"
    #define PROFILE_FMT_I64 "%lld"
#else
    #include <inttypes.h>
    #define PROFILE_FMT_U64 "%" PRIu64
    #define PROFILE_FMT_I64 "%" PRId64
#endif

void PrintProperty(dmProfile::HProperty property, int indent)
{
    const char* name = dmProfile::PropertyGetName(property); // Do not store this pointer!
    dmProfile::PropertyType type = dmProfile::PropertyGetType(property);
    dmProfile::PropertyValue value = dmProfile::PropertyGetValue(property);

    PrintIndent(indent); printf("%s: ", name);

    switch(type)
    {
    case dmProfile::PROPERTY_TYPE_BOOL:  printf("%s\n", value.m_Bool?"true":"false"); break;
    case dmProfile::PROPERTY_TYPE_S32:   printf("%d\n", value.m_S32); break;
    case dmProfile::PROPERTY_TYPE_U32:   printf("%u\n", value.m_U32); break;
    case dmProfile::PROPERTY_TYPE_F32:   printf("%f\n", value.m_F32); break;
    case dmProfile::PROPERTY_TYPE_S64:   printf(PROFILE_FMT_I64 "\n", value.m_S64); break;
    case dmProfile::PROPERTY_TYPE_U64:   printf(PROFILE_FMT_U64 "\n", value.m_U64); break;
    case dmProfile::PROPERTY_TYPE_F64:   printf("%g\n", value.m_F64); break;
    case dmProfile::PROPERTY_TYPE_GROUP: printf("\n"); break;
    default: break;
    }
}

#undef PROFILE_FMT_I64
#undef PROFILE_FMT_U64

// end unit test

} // namespace
