#ifndef DEBUGRENDERER_H
#define DEBUGRENDERER_H_

#include <vectormath/cpp/vectormath_aos.h>
#include <graphics/graphics_device.h>


using namespace Vectormath::Aos;

namespace dmRenderDebug
{

    void Initialize(dmRender::HRenderWorld renderworld);
    void Finalize();
    void Update();

    /**
     * SetFragmentProgram Assign a fragment program to renderer
     * @param program Fragment program
     */
    void SetFragmentProgram(dmGraphics::HFragmentProgram program);

    /**
     * SetVertexProgram Assign a vertex program to renderer
     * @param program Vertex program
     */
    void SetVertexProgram(dmGraphics::HVertexProgram program);

    /**
     * Square Render debug square (2D)
     * @param view_proj View-projection matrix
     * @param position Position
     * @param size Size
     * @param color Color
     */
    void Square(Matrix4 view_proj, Point3 position, Vector3 size, Vector4 color);

    void Plane(Matrix4* view_proj, const float* vertices, Vector4 color);

    /**
     * Cube Render debug cube (3D)
     * @param position Position
     * @param size Size
     */
    void Cube(Point3 position, float size);

    /**
     * Line Render debug line
     * @param view_proj View-projection matrix
     * @param start Start point
     * @param end End point
     * @param color Color
     */
    void Line(Matrix4 view_proj, Point3 start, Point3 end, Vector4 color);

    /**
     * Lines Render debug lines (line strip)
     * @param view_proj View-projection matrix
     * @param vertices Points
     * @param vertex_count Number of points (>1)
     * @param color Color
     */
    void Lines(Matrix4 view_proj, Point3* vertices, int vertex_count, Vector4 color);

}

#endif // DEBUGRENDERER_H
