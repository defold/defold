#include "debug_renderer.h"

#include <stdint.h>

#include <dlib/array.h>
#include <dlib/hash.h>
#include <dlib/log.h>

#include <graphics/graphics_device.h>

#include "render.h"
#include "render_private.h"

using namespace Vectormath::Aos;

namespace dmRender
{
    static const uint32_t MAX_VERTEX_COUNT = 40000;

    struct DebugVertex
    {
        Vector4 m_Position;
        Vector4 m_Color;
    };

    void InitializeDebugRenderer(dmRender::HRenderContext render_context, const void* vp_data, uint32_t vp_data_size, const void* fp_data, uint32_t fp_data_size)
    {
        DebugRenderer& debug_renderer = render_context->m_DebugRenderer;

        debug_renderer.m_RenderContext = render_context;
        debug_renderer.m_VertexIndex = 0;
        debug_renderer.m_VertexBuffer = dmGraphics::NewVertexBuffer(MAX_VERTEX_COUNT * sizeof(DebugVertex), 0x0, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
        dmGraphics::VertexElement ve[] =
        {
            {0, 4, dmGraphics::TYPE_FLOAT, 0, 0 },
            {1, 4, dmGraphics::TYPE_FLOAT, 0, 0 }
        };
        debug_renderer.m_VertexDeclaration = dmGraphics::NewVertexDeclaration(ve, 2);

        dmGraphics::HVertexProgram vertex_program = dmGraphics::INVALID_VERTEX_PROGRAM_HANDLE;
        if (vp_data_size > 0)
            vertex_program = dmGraphics::NewVertexProgram(vp_data, vp_data_size);
        dmGraphics::HFragmentProgram fragment_program = dmGraphics::INVALID_FRAGMENT_PROGRAM_HANDLE;
        if (fp_data_size > 0)
            fragment_program = dmGraphics::NewFragmentProgram(fp_data, fp_data_size);

        HMaterial material3d = NewMaterial();
        SetMaterialVertexProgramConstantType(material3d, 0, dmRenderDDF::MaterialDesc::CONSTANT_TYPE_VIEWPROJ);
        AddMaterialTag(material3d, dmHashString32(DEBUG_3D_NAME));
        SetMaterialVertexProgram(material3d, vertex_program);
        SetMaterialFragmentProgram(material3d, fragment_program);

        HMaterial material2d = NewMaterial();
        SetMaterialVertexProgramConstantType(material2d, 0, dmRenderDDF::MaterialDesc::CONSTANT_TYPE_VIEWPROJ);
        AddMaterialTag(material2d, dmHashString32(DEBUG_2D_NAME));
        SetMaterialVertexProgram(material2d, vertex_program);
        SetMaterialFragmentProgram(material2d, fragment_program);

        dmGraphics::PrimitiveType primitive_types[MAX_DEBUG_RENDER_TYPE_COUNT] = {dmGraphics::PRIMITIVE_QUADS, dmGraphics::PRIMITIVE_LINES};

        for (uint32_t i = 0; i < MAX_DEBUG_RENDER_TYPE_COUNT; ++i)
        {
            RenderObject* ro = &debug_renderer.m_RenderObject3d[i];
            ro->m_Material = material3d;
            ro->m_PrimitiveType = primitive_types[i];
            ro->m_VertexBuffer = debug_renderer.m_VertexBuffer;
            ro->m_VertexDeclaration = debug_renderer.m_VertexDeclaration;
        }

        for (uint32_t i = 0; i < MAX_DEBUG_RENDER_TYPE_COUNT; ++i)
        {
            RenderObject* ro = &debug_renderer.m_RenderObject2d[i];
            ro->m_Material = material2d;
            ro->m_PrimitiveType = primitive_types[i];
            ro->m_VertexBuffer = debug_renderer.m_VertexBuffer;
            ro->m_VertexDeclaration = debug_renderer.m_VertexDeclaration;
        }

        debug_renderer.m_3dPredicate.m_Tags[0] = dmHashString32(DEBUG_3D_NAME);
        debug_renderer.m_3dPredicate.m_TagCount = 1;
        debug_renderer.m_2dPredicate.m_Tags[0] = dmHashString32(DEBUG_2D_NAME);
        debug_renderer.m_2dPredicate.m_TagCount = 1;
    }

    void FinalizeDebugRenderer(HRenderContext context)
    {
        HMaterial material = context->m_DebugRenderer.m_RenderObject3d[0].m_Material;

        dmGraphics::HVertexProgram vp = GetMaterialVertexProgram(material);
        if (vp != dmGraphics::INVALID_VERTEX_PROGRAM_HANDLE)
            dmGraphics::DeleteVertexProgram(vp);
        dmGraphics::HFragmentProgram fp = GetMaterialFragmentProgram(material);
        if (fp != dmGraphics::INVALID_FRAGMENT_PROGRAM_HANDLE)
            dmGraphics::DeleteFragmentProgram(fp);

        DeleteMaterial(material);
        material = context->m_DebugRenderer.m_RenderObject2d[0].m_Material;
        DeleteMaterial(material);

        dmGraphics::DeleteVertexBuffer(context->m_DebugRenderer.m_VertexBuffer);
        dmGraphics::DeleteVertexDeclaration(context->m_DebugRenderer.m_VertexDeclaration);
    }

    void ClearDebugRenderObjects(HRenderContext context)
    {
        for (uint32_t i = 0; i < MAX_DEBUG_RENDER_TYPE_COUNT; ++i)
        {
            context->m_DebugRenderer.m_RenderObject3d[i].m_VertexCount = 0;
            context->m_DebugRenderer.m_RenderObject2d[i].m_VertexCount = 0;
        }
        context->m_DebugRenderer.m_VertexIndex = 0;
    }

#define ADD_TO_RENDER(object)\
    if (object.m_VertexCount == 0)\
        dmRender::AddToRender(context, &object);

    void Square2d(HRenderContext context, float x0, float y0, float x1, float y1, Vector4 color)
    {
        RenderObject& ro = context->m_DebugRenderer.m_RenderObject2d[DEBUG_RENDER_TYPE_FACE];
        ADD_TO_RENDER(ro);
        if (context->m_DebugRenderer.m_VertexIndex + 4 < MAX_VERTEX_COUNT)
        {
            DebugVertex v[4];
            v[0].m_Position = Vector4(x0, y0, 0.0f, 0.0f);
            v[1].m_Position = Vector4(x1, y0, 0.0f, 0.0f);
            v[2].m_Position = Vector4(x1, y1, 0.0f, 0.0f);
            v[3].m_Position = Vector4(x0, y1, 0.0f, 0.0f);
            for (uint32_t i = 0; i < 4; ++i)
                v[i].m_Color = color;
            dmGraphics::SetVertexBufferSubData(ro.m_VertexBuffer, ro.m_VertexCount * sizeof(DebugVertex), 4 * sizeof(DebugVertex), (const void*)v);
            ro.m_VertexCount += 4;
            context->m_DebugRenderer.m_VertexIndex += 4;
        }
        else
        {
            dmLogWarning("Out of Square2d vertex data");
        }
    }

    void Line2D(HRenderContext context, float x0, float y0, float x1, float y1, Vector4 color0, Vector4 color1)
    {
        RenderObject& ro = context->m_DebugRenderer.m_RenderObject2d[DEBUG_RENDER_TYPE_LINE];
        ADD_TO_RENDER(ro);
        if (context->m_DebugRenderer.m_VertexIndex + 2 < MAX_VERTEX_COUNT)
        {
            DebugVertex v[2];
            v[0].m_Position = Vector4(x0, y0, 0.0f, 0.0f);
            v[0].m_Color = color0;
            v[1].m_Position = Vector4(x1, y1, 0.0f, 0.0f);
            v[1].m_Color = color1;
            dmGraphics::SetVertexBufferSubData(ro.m_VertexBuffer, ro.m_VertexCount * sizeof(DebugVertex), 2 * sizeof(DebugVertex), (const void*)v);
            ro.m_VertexCount += 2;
            context->m_DebugRenderer.m_VertexIndex += 2;
        }
        else
        {
            dmLogWarning("Out of Line2D vertex data");
        }
    }

    void Line3D(HRenderContext context, Point3 start, Point3 end, Vector4 start_color, Vector4 end_color)
    {
        RenderObject& ro = context->m_DebugRenderer.m_RenderObject3d[DEBUG_RENDER_TYPE_LINE];
        ADD_TO_RENDER(ro);
        if (context->m_DebugRenderer.m_VertexIndex + 2 < MAX_VERTEX_COUNT)
        {
            DebugVertex v[2];
            v[0].m_Position = Vector4(start);
            v[0].m_Color = start_color;
            v[1].m_Position = Vector4(end);
            v[1].m_Color = end_color;
            dmGraphics::SetVertexBufferSubData(ro.m_VertexBuffer, ro.m_VertexCount * sizeof(DebugVertex), 2 * sizeof(DebugVertex), (const void*)v);
            ro.m_VertexCount += 2;
            context->m_DebugRenderer.m_VertexIndex += 2;
        }
        else
        {
            dmLogWarning("Out of Line3D vertex data");
        }
    }

#undef ADD_TO_RENDER
}
