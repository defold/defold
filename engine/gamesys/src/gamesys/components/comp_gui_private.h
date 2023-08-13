// Copyright 2020-2023 The Defold Foundation
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

#ifndef DM_GAMESYS_COMP_GUI_PRIVATE_H
#define DM_GAMESYS_COMP_GUI_PRIVATE_H

#include <stdint.h>

#include <gui/gui.h>
#include <render/render.h>
#include <dmsdk/dlib/buffer.h>
#include <dmsdk/gameobject/gameobject.h>
#include <dmsdk/gamesys/gui.h>
#include <dmsdk/gamesys/render_constants.h>
#include <dmsdk/script/script.h>

namespace dmGameSystem
{
    struct CompGuiContext;
    struct GuiSceneResource;
    struct MaterialResource;

    struct GuiComponent
    {
        struct GuiWorld*        m_World;
        GuiSceneResource*       m_Resource;
        dmGui::HScene           m_Scene;
        dmGameObject::HInstance m_Instance;
        MaterialResource*       m_Material;
        uint16_t                m_ComponentIndex;
        uint8_t                 m_Enabled : 1;
        uint8_t                 m_AddedToUpdate : 1;
        dmArray<void*>          m_ResourcePropertyPointers;
    };

    struct BoxVertex
    {
        inline BoxVertex() {}
        inline BoxVertex(const dmVMath::Vector4& p, float u, float v, const dmVMath::Vector4& color)
        {
            SetPosition(p);
            SetUV(u, v);
            SetColor(color);
        }

        inline void SetPosition(const dmVMath::Vector4& p)
        {
            m_Position[0] = p.getX();
            m_Position[1] = p.getY();
            m_Position[2] = p.getZ();
        }

        inline void SetUV(float u, float v)
        {
            m_UV[0] = u;
            m_UV[1] = v;
        }

        inline void SetColor(const dmVMath::Vector4& c)
        {
            m_Color[0] = c.getX();
            m_Color[1] = c.getY();
            m_Color[2] = c.getZ();
            m_Color[3] = c.getW();
        }

        float m_Position[3];
        float m_UV[2];
        float m_Color[4];
    };

    struct GuiRenderObject
    {
        dmRender::RenderObject m_RenderObject;
        uint32_t m_SortOrder;
    };

    /*#
     * Context used when registering a new component type
     * @struct
     * @name CompGuiNodeTypeCtx
     * @member m_Config [type: dmConfigFile::HConfig] The config file
     * @member m_Factory [type: dmResource::HFactory] The resource factory
     * @member m_Render [type: dmRender::HRender] The render context
     * @member m_Contexts [type: dmHashTable64<void*>] Mappings between names and contexts
     */
    struct CompGuiNodeTypeCtx {
        dmConfigFile::HConfig    m_Config;
        dmResource::HFactory     m_Factory;
        dmRender::HRenderContext m_Render;
        dmGui::HContext          m_GuiContext;
        dmScript::HContext       m_Script;
        dmHashTable64<void*>     m_Contexts;
    };

    struct CompGuiNodeType
    {
        CompGuiNodeType() {}

        CompGuiNodeTypeDescriptor*  m_TypeDesc;
        void*                       m_Context;

        CompGuiNodeCreateFn         m_Create;
        CompGuiNodeDestroyFn        m_Destroy;
        CompGuiNodeCloneFn          m_Clone;
        CompGuiNodeUpdateFn         m_Update;
        CompGuiNodeGetVerticesFn    m_GetVertices;
        CompGuiNodeSetNodeDescFn    m_SetNodeDesc;
    };


    struct GuiWorld
    {
        dmArray<GuiRenderObject>            m_GuiRenderObjects;
        dmArray<HComponentRenderConstants>  m_RenderConstants;
        dmArray<GuiComponent*>              m_Components;
        dmGraphics::HVertexDeclaration      m_VertexDeclaration;
        dmGraphics::HVertexBuffer           m_VertexBuffer;
        dmArray<BoxVertex>                  m_ClientVertexBuffer;
        dmGraphics::HTexture                m_WhiteTexture;
        dmParticle::HParticleContext        m_ParticleContext;
        uint32_t                            m_MaxParticleFXCount;
        uint32_t                            m_MaxParticleCount;
        uint32_t                            m_RenderedParticlesSize;
        uint32_t                            m_MaxAnimationCount;
        float                               m_DT;
        dmScript::ScriptWorld*              m_ScriptWorld;
        CompGuiContext*                     m_CompGuiContext;
    };

    typedef BoxVertex ParticleGuiVertex;
}

#endif // DM_GAMESYS_COMP_GUI_PRIVATE_H
