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

#ifndef DM_PROFILE_H
#define DM_PROFILE_H

#include <dmsdk/dlib/profile.h>

// Snapshot of a property's metadata and highest value reached during the reporting period.
struct ProfilePropertyPeak
{
    const char*          m_Name;
    const char*          m_Desc;
    ProfilePropertyType  m_Type;
    ProfilePropertyValue m_Value;
};

// Copy a consistent snapshot of all non-group properties, including unused properties.
// Returns the number copied (at most capacity).
// Names and descriptions reference the original registered strings; no copies are allocated.
uint32_t ProfileGetPropertyPeaks(ProfilePropertyPeak* peaks, uint32_t capacity);

// Start a new reporting period, using each property's current value as its initial peak.
void ProfileResetPropertyPeaks();

#endif
