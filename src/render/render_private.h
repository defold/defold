#ifndef RENDERINTERNAL_H
#define RENDERINTERNAL_H

#include <vectormath/cpp/vectormath_aos.h>

#include <dlib/array.h>

#include <graphics/material.h>

#include "render/render.h"

namespace dmRender
{
    using namespace Vectormath::Aos;

#define DEBUG_3D_NAME "_debug3d"
#define DEBUG_2D_NAME "_debug2d"

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

    enum DebugRenderType
    {
        DEBUG_RENDER_TYPE_FACE,
        DEBUG_RENDER_TYPE_LINE,
        MAX_DEBUG_RENDER_TYPE_COUNT
    };

    struct DebugRenderer
    {
        dmRender::HRenderContext            m_RenderContext;
        dmRender::HRenderObject             m_RenderObject3d[MAX_DEBUG_RENDER_TYPE_COUNT];
        dmRender::HRenderObject             m_RenderObject2d[MAX_DEBUG_RENDER_TYPE_COUNT];
    };

    struct RenderContext
    {
        dmArray<RenderType>         m_RenderTypes;
        dmArray<RenderObject*>      m_RenderObjects;
        DebugRenderer               m_DebugRenderer;
        Predicate                   m_Debug3dPredicate;
        Predicate                   m_Debug2dPredicate;

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

    void RenderTypeDebugBegin(HRenderContext rendercontext);
    void RenderTypeDebugDraw(HRenderContext rendercontext, dmRender::HRenderObject ro, uint32_t count);
}

#endif

