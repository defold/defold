#include "debug_renderer.h"

#include <stdint.h>

#include <dlib/log.h>
#include <dlib/array.h>

#include <graphics/graphics_device.h>

#include "render.h"
#include "render_private.h"

using namespace Vectormath::Aos;

namespace dmRender
{
    struct DebugRenderer
    {
        dmRender::HRenderContext            m_RenderContext;
        dmGraphics::HMaterial               m_Material;
        dmArray<dmRender::HRenderObject>    m_ROs;
        HRenderType                         m_DebugRenderType;
    };

    DebugRenderer m_DebugRenderer;

    void InitializeDebugRenderer(dmRender::HRenderContext render_context, dmGraphics::HMaterial material)
    {
        m_DebugRenderer.m_RenderContext = render_context;
        m_DebugRenderer.m_Material = material;
        m_DebugRenderer.m_ROs.SetCapacity(1000);

        RenderType render_type;
        render_type.m_BeginCallback = RenderTypeDebugBegin;
        render_type.m_DrawCallback = RenderTypeDebugDraw;
        render_type.m_EndCallback = 0x0;
        dmRender::RegisterRenderType(render_context, render_type, &m_DebugRenderer.m_DebugRenderType);
    }

    void FinalizeDebugRenderer()
    {
        ClearDebugRenderObjects();
    }

    void ClearDebugRenderObjects()
    {
        for (uint32_t i = 0; i < m_DebugRenderer.m_ROs.Size(); i++)
            dmRender::DeleteRenderObject(m_DebugRenderer.m_ROs[i]);
        m_DebugRenderer.m_ROs.SetSize(0);
    }

    static dmRender::HRenderObject NewRO()
    {
        dmRender::HRenderObject ro = dmRender::NewRenderObject(m_DebugRenderer.m_DebugRenderType, m_DebugRenderer.m_Material, 0x0);
        m_DebugRenderer.m_ROs.Push(ro);
        dmRender::AddToRender(m_DebugRenderer.m_RenderContext, ro);
        return ro;
    }

    void Square(Point3 position, Vector3 size, Vector4 color)
    {
        dmRender::HRenderObject ro = NewRO();
        DebugRenderInfo* info = new DebugRenderInfo;
        info->m_Type = DEBUG_RENDER_TYPE_SQUARE;
        info->m_Mode = RENDERMODE_2D;

        dmRender::SetPosition(ro, position);
        dmRender::SetSize(ro, size);
        dmRender::SetColor(ro, color, dmRender::DIFFUSE_COLOR);
        info->m_Data4 = (void*)dmGraphics::GetMaterialFragmentProgram(m_DebugRenderer.m_Material);
        info->m_Data5 = (void*)dmGraphics::GetMaterialVertexProgram(m_DebugRenderer.m_Material);

        dmRender::SetUserData(ro, info);
    }

    static void SetupLines(RenderMode mode, Point3* vertices, uint32_t vertex_count, Vector4 color)
    {
        dmRender::HRenderObject ro = NewRO();
        DebugRenderInfo* info = new DebugRenderInfo;

        Point3* p = new Point3[vertex_count];
        memcpy(p, vertices, sizeof(Point3)*vertex_count);

        info->m_Type = DEBUG_RENDER_TYPE_LINES;
        info->m_Mode = mode;
        info->m_Data0 = p;
        info->m_Data1 = (void*)vertex_count;
        info->m_Data4 = (void*)dmGraphics::GetMaterialFragmentProgram(m_DebugRenderer.m_Material);
        info->m_Data5 = (void*)dmGraphics::GetMaterialVertexProgram(m_DebugRenderer.m_Material);

        dmRender::SetUserData(ro, info);
        dmRender::SetColor(ro, color, dmRender::DIFFUSE_COLOR);
    }

    void Lines2D(Point3* vertices, uint32_t vertex_count, Vector4 color)
    {
        SetupLines(RENDERMODE_2D, vertices, vertex_count, color);
    }

    void Lines3D(Point3* vertices, uint32_t vertex_count, Vector4 color)
    {
        SetupLines(RENDERMODE_3D, vertices, vertex_count, color);
    }

    void Line2D(Point3 start, Point3 end, Vector4 color)
    {
        Point3 vertices[] = {start, end};
        Lines2D(vertices, 2, color);
    }

    void Line3D(Point3 start, Point3 end, Vector4 color)
    {
        Point3 vertices[] = {start, end};
        Lines3D(vertices, 2, color);
    }

    static const float CubeVertices[] =
    {
            -1, -1, -1, 1, -1, -1, 1, 1, -1, -1, 1, -1, -1, -1, 1, 1, -1, 1, 1, 1, 1, -1, 1, 1
    };

    static const int CubeIndices[] =
    {
            0, 1, 2, 2, 3, 0, 0, 3, 7, 7, 4, 0, 4, 7, 6, 6, 5, 4, 1, 5, 6, 6, 2, 1, 0, 4, 5, 5, 1, 0, 3, 2, 6, 6, 7, 3
    };

    void RenderTypeDebugBegin(HRenderContext render_context)
    {
    }

    void RenderTypeDebugDraw(HRenderContext rendercontext, dmRender::HRenderObject ro, uint32_t count)
    {
        DebugRenderInfo* info = (DebugRenderInfo*)dmRender::GetUserData(ro);

        dmGraphics::HContext context = rendercontext->m_GFXContext;

        switch (info->m_Type)
        {
            case DEBUG_RENDER_TYPE_SQUARE:
            {
                dmGraphics::SetFragmentProgram(context, (dmGraphics::HFragmentProgram)info->m_Data4);
                dmGraphics::SetVertexProgram(context, (dmGraphics::HVertexProgram)info->m_Data5);

                Point3 position = dmRender::GetPosition(ro);
                Vector3 size = dmRender::GetSize(ro);
                Vector4 color = dmRender::GetColor(ro, dmRender::DIFFUSE_COLOR);


                dmGraphics::SetFragmentConstant(context, &color, 0);
                size.setZ(0.0f);
                Matrix4 mat = Matrix4::scale(size*0.5f);
                mat.setTranslation(Vector3(position) );

                Matrix4 m = Matrix4::orthographic( -1, 1, 1, -1, 10, -10 );

                dmGraphics::SetVertexConstantBlock(context, (const Vector4*)&m, 0, 4);
                dmGraphics::SetVertexConstantBlock(context, (const Vector4*)&mat, 4, 4);

                dmGraphics::SetVertexStream(context, 0, 3, dmGraphics::TYPE_FLOAT, 0, (void*) CubeVertices);
                dmGraphics::DisableVertexStream(context, 1);
                dmGraphics::DisableVertexStream(context, 2);

                dmGraphics::DrawElements(context, dmGraphics::PRIMITIVE_TRIANGLES, 3*12, dmGraphics::TYPE_UNSIGNED_INT, (void*) CubeIndices);
                break;
            }

            case DEBUG_RENDER_TYPE_LINES:
            {
                dmGraphics::SetFragmentProgram(context, (dmGraphics::HFragmentProgram)info->m_Data4);
                dmGraphics::SetVertexProgram(context, (dmGraphics::HVertexProgram)info->m_Data5);


                Point3* vertices = (Point3*)info->m_Data0;
                int vertex_count = (int)info->m_Data1;

                Vector4 color = dmRender::GetColor(ro, dmRender::DIFFUSE_COLOR);
                dmGraphics::SetFragmentConstant(context, &color, 0);

                Matrix4 mat = Matrix4::identity();

                if (info->m_Mode == RENDERMODE_2D)
                {
                    Matrix4 m = Matrix4::orthographic( -1, 1, 1, -1, 10, -10 );
                    dmGraphics::SetVertexConstantBlock(context, (const Vector4*)&m, 0, 4);
                }
                else
                {
                    dmGraphics::SetVertexConstantBlock(context, (const Vector4*)&rendercontext->m_ViewProj, 0, 4);
                }

                dmGraphics::SetVertexConstantBlock(context, (const Vector4*)&mat, 4, 4);

                dmGraphics::SetVertexStream(context, 0, 4, dmGraphics::TYPE_FLOAT, 0, (void*)vertices);
                dmGraphics::DisableVertexStream(context, 1);
                dmGraphics::DisableVertexStream(context, 2);

                dmGraphics::Draw(context, dmGraphics::PRIMITIVE_LINE_STRIP, 0, vertex_count);
                break;
            }

            default:
            {

            }
        }
    }
}
