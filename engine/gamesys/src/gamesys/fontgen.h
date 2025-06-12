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

#pragma once

#include <dmsdk/dlib/hash.h>
#include <dmsdk/extension/extension.h>

namespace dmGameSystem
{
    struct FontResource;

    dmExtension::Result FontGenInitialize(dmExtension::Params* params);
    dmExtension::Result FontGenFinalize(dmExtension::Params* params);
    dmExtension::Result FontGenUpdate(dmExtension::Params* params);

    // Scripting
    typedef void (*FGlyphCallback)(void* cbk_ctx, int result, const char* errmsg);
    bool FontGenAddGlyphs(FontResource* resource, const char* text, FGlyphCallback cbk, void* cbk_ctx);
    bool FontGenRemoveGlyphs(FontResource* resource, const char* text);
}
