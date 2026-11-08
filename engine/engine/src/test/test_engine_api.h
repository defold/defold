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

#ifndef DM_TEST_ENGINE_API_H
#define DM_TEST_ENGINE_API_H

#include <stdbool.h>
#include <dmsdk/dlib/context_registry.h>

#if defined(__cplusplus)
#include <dmsdk/engine/engine.hpp>
extern "C" {
#else
#include <dmsdk/engine/engine.h>
#endif

// Test-only bridge for native engine setup. Registry and window handles are borrowed.
    HEngine TestEngineNew(HContextRegistry* registry);
    bool    TestEngineInit(HEngine engine, HWindow* window);
    void    TestEngineDelete(HEngine engine);
    int     TestEngineApiRunTests(int argc, char** argv);

#if defined(__cplusplus)
}
#endif

#endif // DM_TEST_ENGINE_API_H
