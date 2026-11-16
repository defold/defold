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
#include "test_engine_api.h"
#include "../engine_private.h"

#include <ddf/ddf.h>
#include <dlib/hash.h>
#include <dlib/profile.h>
#include <dlib/testutil.h>
#include <testmain/testmain.h>

extern "C" void dmExportedSymbols();

extern "C" HWindow TestEngineGetWindowCpp(HEngine engine)
{
    return EngineGetWindow(engine);
}

extern "C" HEngine TestEngineNew(HContextRegistry* registry)
{
    dmEngineInitialize();
    dmEngine::HEngine engine = dmEngine::New(0);
    *registry = engine->m_ContextRegistry;
    return engine;
}

extern "C" bool TestEngineInit(HEngine engine, HWindow* window)
{
    char project_path[256];
    dmTestUtil::MakeHostPathf(project_path, sizeof(project_path), "%s/game.projectc", CONTENT_ROOT);
    const char* argv[] = {
        "test_engine_api",
        "--config=dmengine.unload_builtins=0",
        "--config=bootstrap.main_collection=/text_input/text_input.collectionc",
        "--config=input.game_binding=/text_input/text_input.input_bindingc",
        project_path
    };
    bool initialized = dmEngine::Init(engine, DM_ARRAY_SIZE(argv), (char**)argv);
    *window = engine->m_Window;
    return initialized;
}

extern "C" void TestEngineDelete(HEngine engine)
{
    dmEngine::Delete(engine);
    dmEngineFinalize();
}

int main(int argc, char** argv)
{
    dmExportedSymbols();
    TestMainPlatformInit();
    ProfileInitialize();
    dmDDF::RegisterAllTypes();
    dmHashEnableReverseHash(true);
    dmGraphics::InstallAdapter(dmGraphics::ADAPTER_FAMILY_NONE);

    int result = TestEngineApiRunTests(argc, argv);
    ProfileFinalize();
    return result;
}
