#ifndef RENDERINTERNAL_H
#define RENDERINTERNAL_H

#include <vectormath/cpp/vectormath_aos.h>

#include <dlib/array.h>

#include <graphics/material.h>

#include "render/render.h"

namespace dmRender
{
    using namespace Vectormath::Aos;

    struct RenderObject
    {
        Vector4                 m_Colour[MAX_COLOR];
        Point3                  m_Pos;
        Quat                    m_Rot;
        Vector3                 m_Size;
        dmGraphics::HMaterial   m_Material;
        void*                   m_UserData;
        void*                   m_VisualObject;
        uint32_t                m_Type;
    };

    struct RenderContext
    {
        dmArray<RenderType>         m_RenderTypes;
        dmArray<RenderObject*>      m_RenderObjects;

        dmGraphics::HContext        m_GFXContext;

        SetObjectModel              m_SetObjectModel;

        Matrix4                     m_View;
        Matrix4                     m_Projection;
        Matrix4                     m_ViewProj;

        uint32_t                    m_TextRenderType;
    };

    struct SFontVertex
    {
        float m_Position[3];
        float m_UV[2];
        float m_Colour[4];
    };

    void RenderTypeTextBegin(HRenderContext rendercontext);
    void RenderTypeTextDraw(HRenderContext rendercontext, HRenderObject ro_, uint32_t count);

    enum DebugRenderType
    {
        DEBUG_RENDER_TYPE_SQUARE,
        DEBUG_RENDER_TYPE_CUBE,
        DEBUG_RENDER_TYPE_PLANE,
        DEBUG_RENDER_TYPE_LINE,
        DEBUG_RENDER_TYPE_LINES
    };

    enum RenderMode
    {
        RENDERMODE_2D,
        RENDERMODE_3D
    };

    struct DebugRenderInfo
    {
        DebugRenderType m_Type;
        RenderMode      m_Mode;
        void*           m_Data0;
        void*           m_Data1;
        void*           m_Data2;
        void*           m_Data3;
        void*           m_Data4;
        void*           m_Data5;
    };

    void RenderTypeDebugBegin(HRenderContext rendercontext);
    void RenderTypeDebugDraw(HRenderContext rendercontext, dmRender::HRenderObject ro, uint32_t count);
}

#endif

