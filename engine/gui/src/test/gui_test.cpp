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

#include "gui_test.hpp"
#include "../gui_private.h"

namespace dmGui
{
    void TestApplyLayoutCallback(HScene scene, dmhash_t layout)
    {
        scene->m_ApplyLayoutCallback(scene, layout);
    }

    uint32_t TestGetDynamicTextureCount(HScene scene)
    {
        return scene->m_DynamicTextures.Size();
    }

    uint32_t TestGetDynamicTextureCapacity(HScene scene)
    {
        return scene->m_DynamicTextures.Capacity();
    }
}
