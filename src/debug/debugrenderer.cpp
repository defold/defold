#include <dlib/log.h>
#include "debugrenderer.h"
#include "graphics/graphics_device.h"


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

        static const float              m_CubeVertices[];
        static const int                m_CubeIndices[];
    };



    const float State::m_CubeVertices[] =
    {
            -1, -1, -1,
            1, -1, -1,
            1, 1, -1,
            -1, 1, -1,
            -1, -1, 1,
            1, -1, 1,
            1, 1, 1,
            -1, 1, 1
    };

    const int State::m_CubeIndices[] =
    {
            0, 1, 2,
            2, 3, 0,
            0, 3, 7,
            7, 4, 0,
            4, 7, 6,
            6, 5, 4,
            1, 5, 6,
            6, 2, 1,
            0, 4, 5,
            5, 1, 0,
            3, 2, 6,
            6, 7, 3
    };

    State   m_State;


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

    void SetupMatrices(dmGraphics::HContext context, Matrix4* view_proj, Matrix4* mat)
    {
        if (view_proj) dmGraphics::SetVertexConstantBlock(context, (const Vector4*)view_proj, 0, 4);
        if (mat)       dmGraphics::SetVertexConstantBlock(context, (const Vector4*)mat, 4, 4);
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
        dmGraphics::HContext context = dmGraphics::GetContext();

        if (!SetupPrograms(context)) return;

        dmGraphics::SetFragmentConstant(context, &color, 0);

        size.setZ(0.0f);
        Matrix4 mat = Matrix4::scale(size*0.5f);
        mat.setTranslation(Vector3(position) );

        SetupMatrices(context, &view_proj, &mat);

        dmGraphics::SetVertexStream(context, 0, 3, dmGraphics::TYPE_FLOAT, 0, (void*) &State::m_CubeVertices[0]);
        dmGraphics::DisableVertexStream(context, 1);
        dmGraphics::DisableVertexStream(context, 2);

        dmGraphics::DrawElements(context, dmGraphics::PRIMITIVE_TRIANGLES, 3*12, dmGraphics::TYPE_UNSIGNED_INT, (void*) &State::m_CubeIndices[0]);
    }

    void Cube(Point3 position, float size, Vector4 color)
    {
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
    }


    void Line(Matrix4 view_proj, Point3 start, Point3 end, Vector4 color)
    {
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
    }

    void Lines(Matrix4 view_proj, Point3* vertices, int vertex_count, Vector4 color)
    {
        dmGraphics::HContext context = dmGraphics::GetContext();

        if (!SetupPrograms(context)) return;

        dmGraphics::SetFragmentConstant(context, &color, 0);

        Matrix4 mat = Matrix4::identity();

        SetupMatrices(context, &view_proj, &mat);

        dmGraphics::SetVertexStream(context, 0, 4, dmGraphics::TYPE_FLOAT, 0, (void*)vertices);
        dmGraphics::DisableVertexStream(context, 1);
        dmGraphics::DisableVertexStream(context, 2);

        dmGraphics::Draw(context, dmGraphics::PRIMITIVE_LINE_STRIP, 0, vertex_count);
    }

}
