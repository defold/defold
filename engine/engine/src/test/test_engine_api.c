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

#include <dmsdk/engine/engine.h>

// Exercise the C ABI using only public SDK declarations and a live engine.
int TestEngineWindowC(HEngine engine, HWindow expected)
{
    return EngineGetWindow(engine) == expected;
}

// Verify that callback parameters expose the source engine through the C registry API.
int TestEngineParamsC(ExtensionParams* params, HEngine expected)
{
    HContextRegistry registry = ExtensionParamsGetContextRegistry(params);
    HEngine engine = registry ? (HEngine)ContextRegistryGetByHash(registry, dmHashString64(ENGINE_CONTEXT_NAME)) : 0;
    return engine == expected;
}

// Application callbacks use the same registry and borrowed engine entry.
int TestEngineAppParamsC(ExtensionAppParams* params, HEngine expected)
{
    HContextRegistry registry = ExtensionAppParamsGetContextRegistry(params);
    HEngine engine = registry ? (HEngine)ContextRegistryGet(registry, ENGINE_CONTEXT_NAME) : 0;
    return engine == expected;
}
