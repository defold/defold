#include <stdint.h>
#include <dlib/log.h>
#include <dlib/array.h>
#include "graphics/graphics_device.h"
#include "render/render.h"
#include "debugrenderer.h"
#include "rendertypedebugprim.h"


using namespace Vectormath::Aos;

namespace dmRenderDebug
{


    struct State
    {
        State(): m_VertexProgram(0), m_FragmentProgram(0), m_RenderWorld(0), m_RenderCollection(0)
        {

        }
        dmGraphics::HVertexProgram      m_VertexProgram;
        dmGraphics::HFragmentProgram    m_FragmentProgram;
        dmRender::HRenderWorld          m_RenderCollection;
        dmRender::HRenderWorld          m_RenderWorld;
        dmArray<dmRender::HRenderObject> m_ROs[2];
        uint32_t                        m_WorldIndex;
        DebugRenderInfo                 m_RenderData;

    };




    State   m_State;

    void Initialize(dmRender::HRenderWorld renderworld)
    {
        m_State.m_RenderCollection = dmRender::NewRenderWorld(1000, 10, 0x0);
        m_State.m_ROs[0].SetCapacity(1000);
        m_State.m_ROs[1].SetCapacity(1000);
        m_State.m_WorldIndex = 0;
        m_State.m_RenderWorld = renderworld;
        dmRender::RegisterRenderer(renderworld, 4, RenderTypeDebugPrimSetup, RenderTypeDebugPrimDraw);

    }

    void Finalize()
    {
        // really need a collection class here
        for (uint32_t i=0; i < m_State.m_ROs[0].Size(); i++)
            dmRender::DeleteRenderObject(m_State.m_RenderCollection, m_State.m_ROs[0][i]);

        for (uint32_t i=0; i < m_State.m_ROs[1].Size(); i++)
            dmRender::DeleteRenderObject(m_State.m_RenderCollection, m_State.m_ROs[1][i]);

        if (m_State.m_RenderCollection)
            dmRender::DeleteRenderWorld(m_State.m_RenderCollection);
    }

    void Update()
    {
        uint32_t delete_index = 1 - m_State.m_WorldIndex;

        for (uint32_t i=0; i < m_State.m_ROs[delete_index].Size(); i++)
        {
            dmRender::DeleteRenderObject(m_State.m_RenderCollection, m_State.m_ROs[delete_index][i]);
            DebugRenderInfo* p = (DebugRenderInfo*)dmRender::GetData(m_State.m_ROs[delete_index][i]);
            if (p)
            {
                if (p->m_Type == DEBUG_RENDER_TYPE_LINES)
                {
                    Point3* pd = (Point3*)p->m_Data0;
                    delete [] pd;
                }
                delete p;
            }
        }
        m_State.m_ROs[delete_index].SetSize(0);

        dmRender::AddToRender(m_State.m_RenderCollection, m_State.m_RenderWorld);

        m_State.m_WorldIndex = delete_index;
    }


    static dmRender::HRenderObject NewRO()
    {
        dmRender::HRenderObject ro = dmRender::NewRenderObject(m_State.m_RenderCollection, 0x0, 0x0, 1, 4);
        m_State.m_ROs[m_State.m_WorldIndex].Push(ro);
        dmRender::SetData(ro, 0x0);
        return ro;
    }



    void SetFragmentProgram(dmGraphics::HFragmentProgram program)
    {
        m_State.m_FragmentProgram = program;
    }

    void SetVertexProgram(dmGraphics::HVertexProgram program)
    {
        m_State.m_VertexProgram = program;
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
        info->m_Data4 = (void*)m_State.m_FragmentProgram;
        info->m_Data5 = (void*)m_State.m_VertexProgram;

        dmRender::SetData(ro, info);
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
        info->m_Data4 = (void*)m_State.m_FragmentProgram;
        info->m_Data5 = (void*)m_State.m_VertexProgram;

        dmRender::SetData(ro, info);
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
}
