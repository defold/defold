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

#include "profile.h"

void ProfileInitialize()
{
}

void ProfileFinalize()
{
}

bool ProfileIsInitialized()
{
    return false;
}

void ProfileSetThreadName(const char* name)
{
}

HProfile ProfileFrameBegin()
{
    return 0;
}

void ProfileFrameEnd(HProfile profile)
{
}

void ProfileScopeBegin(const char* name, uint64_t* name_hash)
{
}

void ProfileScopeEnd(const char* name, uint64_t name_hash)
{
}

void ProfileLogText(const char* text, ...)
{
}

void ProfilePropertySetBool(ProfileIdx idx, int v)
{
    (void)idx;
    (void)v;
}

void ProfilePropertySetS32(ProfileIdx idx, int32_t v)
{
    (void)idx;
    (void)v;
}

void ProfilePropertySetU32(ProfileIdx idx, uint32_t v)
{
    (void)idx;
    (void)v;
}

void ProfilePropertySetF32(ProfileIdx idx, float v)
{
    (void)idx;
    (void)v;
}

void ProfilePropertySetS64(ProfileIdx idx, int64_t v)
{
    (void)idx;
    (void)v;
}

void ProfilePropertySetU64(ProfileIdx idx, uint64_t v)
{
    (void)idx;
    (void)v;
}

void ProfilePropertySetF64(ProfileIdx idx, double v)
{
    (void)idx;
    (void)v;
}

void ProfilePropertyAddS32(ProfileIdx idx, int32_t v)
{
    (void)idx;
    (void)v;
}

void ProfilePropertyAddU32(ProfileIdx idx, uint32_t v)
{
    (void)idx;
    (void)v;
}

void ProfilePropertyAddF32(ProfileIdx idx, float v)
{
    (void)idx;
    (void)v;
}

void ProfilePropertyAddS64(ProfileIdx idx, int64_t v)
{
    (void)idx;
    (void)v;
}

void ProfilePropertyAddU64(ProfileIdx idx, uint64_t v)
{
    (void)idx;
    (void)v;
}

void ProfilePropertyAddF64(ProfileIdx idx, double v)
{
    (void)idx;
    (void)v;
}

void ProfilePropertyReset(ProfileIdx idx)
{
    (void)idx;
}


ProfileIdx ProfileRegisterPropertyGroup(const char* name, const char* desc, ProfileIdx* (*parentfn)())
{
    return PROFILE_PROPERTY_INVALID_IDX;
}

ProfileIdx ProfileRegisterPropertyBool(const char* name, const char* desc, int value, uint32_t flags, ProfileIdx* (*parentfn)())
{
    return PROFILE_PROPERTY_INVALID_IDX;
}

ProfileIdx ProfileRegisterPropertyS32(const char* name, const char* desc, int32_t value, uint32_t flags, ProfileIdx* (*parentfn)())
{
    return PROFILE_PROPERTY_INVALID_IDX;
}

ProfileIdx ProfileRegisterPropertyU32(const char* name, const char* desc, uint32_t value, uint32_t flags, ProfileIdx* (*parentfn)())
{
    return PROFILE_PROPERTY_INVALID_IDX;
}

ProfileIdx ProfileRegisterPropertyF32(const char* name, const char* desc, float value, uint32_t flags, ProfileIdx* (*parentfn)())
{
    return PROFILE_PROPERTY_INVALID_IDX;
}

ProfileIdx ProfileRegisterPropertyS64(const char* name, const char* desc, int64_t value, uint32_t flags, ProfileIdx* (*parentfn)())
{
    return PROFILE_PROPERTY_INVALID_IDX;
}

ProfileIdx ProfileRegisterPropertyU64(const char* name, const char* desc, uint64_t value, uint32_t flags, ProfileIdx* (*parentfn)())
{
    return PROFILE_PROPERTY_INVALID_IDX;
}

ProfileIdx ProfileRegisterPropertyF64(const char* name, const char* desc, double value, uint32_t flags, ProfileIdx* (*parentfn)())
{
    return PROFILE_PROPERTY_INVALID_IDX;
}

void ProfileRegisterProfiler(const char* name, ProfileListener* profiler)
{
}

void ProfileUnregisterProfiler(const char* name)
{
}
