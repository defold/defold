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

#ifndef DM_TEST_INPUT_API_H
#define DM_TEST_INPUT_API_H

#if defined(__cplusplus)
#include <dmsdk/input/input.hpp>
extern "C" {
#else
#include <dmsdk/input/input.h>
#endif

    typedef struct TestInputApiFixture* HTestInputApiFixture;

// Test-only bridge to native bindings and the null HID backend. The input context is borrowed.
    HTestInputApiFixture TestInputApiCreate(void);
    void                 TestInputApiDelete(HTestInputApiFixture fixture);
    HInputContext        TestInputApiGetContext(HTestInputApiFixture fixture);
    void                 TestInputApiSetKey(HTestInputApiFixture fixture, bool down);
    void                 TestInputApiSetGamepadConnected(HTestInputApiFixture fixture, bool connected);
    void                 TestInputApiSetGamepadAxis(HTestInputApiFixture fixture, float value);
    void                 TestInputApiSetGamepadNameHash(HTestInputApiFixture fixture, dmhash_t name_hash);
    void                 TestInputApiUpdate(HTestInputApiFixture fixture, float dt);
    void                 TestInputApiClearBinding(HTestInputApiFixture fixture);
    int                  TestInputApiRunTests(int argc, char** argv);

#if defined(__cplusplus)
}
#endif

#endif // DM_TEST_INPUT_API_H
