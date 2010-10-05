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
        State(): m_VertexProgram(0), m_FragmentProgram(0)
        {

        }
        dmGraphics::HVertexProgram      m_VertexProgram;
        dmGraphics::HFragmentProgram    m_FragmentProgram;
        dmRender::HRenderWorld          m_RenderCollection;
        dmRender::HRenderWorld          m_RenderWorld;
        dmArray<dmRender::HRenderObject> m_ROs[2];
        uint32_t                        m_WorldIndex;


        static const float              m_CubeVertices[];
        static const int                m_CubeIndices[];
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
        dmRender::DeleteRenderWorld(m_State.m_RenderCollection);
    }

    void Update()
    {
        uint32_t delete_index = 1 - m_State.m_WorldIndex;

        for (uint32_t i=0; i < m_State.m_ROs[delete_index].Size(); i++)
        {
            dmRender::DeleteRenderObject(m_State.m_RenderCollection, m_State.m_ROs[delete_index][i]);
        }
        m_State.m_ROs[delete_index].SetSize(0);

        dmRender::AddToRender(m_State.m_RenderCollection, m_State.m_RenderWorld);

        m_State.m_WorldIndex = delete_index;
    }


    static dmRender::HRenderObject NewRO()
    {
        dmRender::HRenderObject ro = dmRender::NewRenderObject(m_State.m_RenderCollection, 0x0, 0x0, 2, 4);
        m_State.m_ROs[m_State.m_WorldIndex].Push(ro);
        return ro;
    }

    static bool SetupPrograms(dmGraphics::HContext context)
    {
        if (m_State.m_VertexProgram == 0 || m_State.m_FragmentProgram == 0)
        {
            dmLogError("dmRenderDebug: No vertex/fragment program set, debug rendering disabled\n");
            return false;
        }

        dmGraphics::SetVertexProgram(context, m_State.m_VertexProgram);
        dmGraphics::SetFragmentProgram(context, m_State.m_FragmentProgram);

        return true;
    }


    void SetFragmentProgram(dmGraphics::HFragmentProgram program)
    {
        m_State.m_FragmentProgram = program;
    }

    void SetVertexProgram(dmGraphics::HVertexProgram program)
    {
        m_State.m_VertexProgram = program;
    }


    void Square(Matrix4 view_proj, Point3 position, Vector3 size, Vector4 color)
    {
        dmRender::HRenderObject ro = NewRO();
        DebugRenderInfo info;
        info.m_Type = DEBUG_RENDER_TYPE_SQUARE;
        info.m_Data0 = &position;
        info.m_Data1 = &size;
        info.m_Data2 = &color;
        info.m_Data4 = (void*)m_State.m_FragmentProgram;
        info.m_Data5 = (void*)m_State.m_VertexProgram;

        dmRender::SetData(ro, &info);
    }

    void Plane(Matrix4* view_proj, const float* vertices, Vector4 color)
    {
#if 0
        dmGraphics::HContext context = dmGraphics::GetContext();

        if (!SetupPrograms(context)) return;

        dmGraphics::SetFragmentConstant(context, &color, 0);

        Matrix4 mat = Matrix4::identity();

        SetupMatrices(context, view_proj, &mat);

        dmGraphics::SetVertexStream(context, 0, 3, dmGraphics::TYPE_FLOAT, 0, (void*) vertices);
        dmGraphics::DisableVertexStream(context, 1);
        dmGraphics::DisableVertexStream(context, 2);

        dmGraphics::Draw(context, dmGraphics::PRIMITIVE_TRIANGLE_STRIP, 0, 4);
#endif
    }

    void Cube(Point3 position, float size, Vector4 color)
    {
#if 0
        dmGraphics::HContext context = dmGraphics::GetContext();

        if (!SetupPrograms(context)) return;

        dmGraphics::SetFragmentConstant(context, &color, 0);

        Matrix4 mat = Matrix4::scale(Vector3(size));
        mat.setTranslation(Vector3(position) );

        SetupMatrices(context, 0x0, &mat);

        dmGraphics::SetVertexStream(context, 0, 3, dmGraphics::TYPE_FLOAT, 0, (void*) &State::m_CubeVertices[0]);
        dmGraphics::DisableVertexStream(context, 1);
        dmGraphics::DisableVertexStream(context, 2);

        dmGraphics::DrawElements(context, dmGraphics::PRIMITIVE_TRIANGLES, 3*12, dmGraphics::TYPE_UNSIGNED_INT, (void*) &State::m_CubeIndices[0]);
#endif
    }


    void Line(Matrix4 view_proj, Point3 start, Point3 end, Vector4 color)
    {
#if 0
        dmGraphics::HContext context = dmGraphics::GetContext();

        if (!SetupPrograms(context)) return;

        dmGraphics::SetFragmentConstant(context, &color, 0);

        Matrix4 mat = Matrix4::identity();

        SetupMatrices(context, &view_proj, &mat);

        Point3 list[2];
        list[0] = start;
        list[1] = end;

        dmGraphics::SetVertexStream(context, 0, 4, dmGraphics::TYPE_FLOAT, 0, (void*)&list);
        dmGraphics::DisableVertexStream(context, 1);
        dmGraphics::DisableVertexStream(context, 2);

        dmGraphics::Draw(context, dmGraphics::PRIMITIVE_LINES, 0, 2);
#endif
    }

    void Lines(Matrix4 view_proj, Point3* vertices, int vertex_count, Vector4 color)
    {
        dmRender::HRenderObject ro = NewRO();
        DebugRenderInfo info;
        info.m_Type = DEBUG_RENDER_TYPE_LINES;
        info.m_Data0 = vertices;
        info.m_Data1 = &vertex_count;
        info.m_Data2 = &color;

        dmRender::SetData(ro, &info);


    }

}
