// Copyright 2020-2024 The Defold Foundation
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

#ifndef DMSDK_GAMESYSTEM_RES_GUI_H
#define DMSDK_GAMESYSTEM_RES_GUI_H

#include <dmsdk/dlib/array.h>
#include <dmsdk/dlib/hashtable.h>
#include <dmsdk/render/render.h>
#include <dmsdk/gui/gui.h>

#include <gamesys/gui_ddf.h>

#include <dmsdk/script/script.h>

namespace dmParticle
{
    typedef struct Prototype* HPrototype;
}

namespace dmGameSystem
{
    struct MaterialResource;

    struct GuiSceneTextureSetResource
    {
        void*   m_Resource;
        uint8_t m_ResourceIsTextureSet : 1;
    };

    struct GuiSceneResource
    {
        dmGuiDDF::SceneDesc*                m_SceneDesc;
        dmGui::HScript                      m_Script;
        dmArray<dmRender::HFontMap>         m_FontMaps;
        dmArray<dmhash_t>                   m_FontMapPaths;
        dmArray<GuiSceneTextureSetResource> m_GuiTextureSets;
        dmArray<dmParticle::HPrototype>     m_ParticlePrototypes;
        dmArray<MaterialResource*>          m_Materials;
        const char*                         m_Path;
        dmGui::HContext                     m_GuiContext;
        MaterialResource*                   m_Material;
        dmHashTable64<void*>                m_Resources;
        dmHashTable64<dmhash_t>             m_ResourceTypes;
    };
}

#endif // DMSDK_GAMESYSTEM_RES_GUI_H
