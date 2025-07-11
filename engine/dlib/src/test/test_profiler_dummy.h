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

#ifndef DM_TEST_PROFILER_DUMMY_H
#define DM_TEST_PROFILER_DUMMY_H

#include <dmsdk/dlib/profile.h>

struct ProfilerDummyProperty
{
    const char*             m_Name;
    ProfileIdx              m_Idx;
    ProfileIdx              m_ParentIdx;
    ProfileIdx              m_FirstChild;
    ProfileIdx              m_Sibling;
    ProfilePropertyType     m_Type;
    ProfilePropertyValue    m_DefaultValue;
    ProfilePropertyValue    m_Value;
};

struct ProfilerDummyContext
{
    int m_NumProperties;
    int m_NumErrors;

    char m_TextBuffer[1024];

    ProfilerDummyProperty m_Properties[256];
};

void                    DummyProfilerRegister();
void                    DummyProfilerUnregister();
ProfilerDummyContext*   DummyProfilerGetContext();

#endif // DM_TEST_PROFILER_DUMMY_H
