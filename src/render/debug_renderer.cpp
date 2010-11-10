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
    struct DebugRenderInfo
    {
        static const uint32_t VERTEX_COUNT = 1000;
        float m_Vertices[VERTEX_COUNT * 3];
        float m_Colours[VERTEX_COUNT * 4];
        DebugRenderType m_RenderType;
        uint32_t m_VertexCount;
    };

    void RenderTypeDebugDraw3d(HRenderContext rendercontext, dmRender::HRenderObject ro, uint32_t count);
    void RenderTypeDebugDraw2d(HRenderContext rendercontext, dmRender::HRenderObject ro, uint32_t count);

    void InitializeDebugRenderer(dmRender::HRenderContext render_context, const void* vp_data, uint32_t vp_data_size, const void* fp_data, uint32_t fp_data_size)
    {
        DebugRenderer& debug_renderer = render_context->m_DebugRenderer;

        debug_renderer.m_RenderContext = render_context;

        dmGraphics::HVertexProgram vertex_program = dmGraphics::CreateVertexProgram(vp_data, vp_data_size);
        dmGraphics::HFragmentProgram fragment_program = dmGraphics::CreateFragmentProgram(fp_data, fp_data_size);

        dmGraphics::HMaterial material3d = dmGraphics::NewMaterial();
        dmGraphics::AddMaterialTag(material3d, dmHashString32(DEBUG_3D_NAME));
        dmGraphics::SetMaterialVertexProgram(material3d, vertex_program);
        dmGraphics::SetMaterialFragmentProgram(material3d, fragment_program);

        dmGraphics::HMaterial material2d = dmGraphics::NewMaterial();
        dmGraphics::AddMaterialTag(material2d, dmHashString32(DEBUG_2D_NAME));
        dmGraphics::SetMaterialVertexProgram(material2d, vertex_program);
        dmGraphics::SetMaterialFragmentProgram(material2d, fragment_program);

        RenderType render_type;
        uint32_t debug_render_type;

        render_type.m_DrawCallback = RenderTypeDebugDraw3d;
        dmRender::RegisterRenderType(render_context, render_type, &debug_render_type);
        for (uint32_t i = 0; i < MAX_DEBUG_RENDER_TYPE_COUNT; ++i)
        {
            debug_renderer.m_RenderObject3d[i] = dmRender::NewRenderObject(debug_render_type, material3d, 0x0);
            DebugRenderInfo* info = new DebugRenderInfo();
            info->m_RenderType = (DebugRenderType)i;
            info->m_VertexCount = 0;
            debug_renderer.m_RenderObject3d[i]->m_UserData = (void*)info;
        }

        render_type.m_DrawCallback = RenderTypeDebugDraw2d;
        dmRender::RegisterRenderType(render_context, render_type, &debug_render_type);
        for (uint32_t i = 0; i < MAX_DEBUG_RENDER_TYPE_COUNT; ++i)
        {
            debug_renderer.m_RenderObject2d[i] = dmRender::NewRenderObject(debug_render_type, material2d, 0x0);
            DebugRenderInfo* info = new DebugRenderInfo();
            info->m_RenderType = (DebugRenderType)i;
            info->m_VertexCount = 0;
            debug_renderer.m_RenderObject2d[i]->m_UserData = (void*)info;
        }
    }

    void FinalizeDebugRenderer(HRenderContext context)
    {
        dmGraphics::HMaterial material = context->m_DebugRenderer.m_RenderObject3d[0]->m_Material;
        dmGraphics::DestroyVertexProgram(dmGraphics::GetMaterialVertexProgram(material));
        dmGraphics::DestroyFragmentProgram(dmGraphics::GetMaterialFragmentProgram(material));
        dmGraphics::DeleteMaterial(material);
        material = context->m_DebugRenderer.m_RenderObject2d[0]->m_Material;
        dmGraphics::DeleteMaterial(material);
        for (uint32_t i = 0; i < MAX_DEBUG_RENDER_TYPE_COUNT; ++i)
        {
            delete (DebugRenderInfo*)context->m_DebugRenderer.m_RenderObject3d[i]->m_UserData;
            dmRender::DeleteRenderObject(context->m_DebugRenderer.m_RenderObject3d[i]);
            delete (DebugRenderInfo*)context->m_DebugRenderer.m_RenderObject2d[i]->m_UserData;
            dmRender::DeleteRenderObject(context->m_DebugRenderer.m_RenderObject2d[i]);
        }
    }

    void ClearDebugRenderObjects(HRenderContext context)
    {
        for (uint32_t i = 0; i < MAX_DEBUG_RENDER_TYPE_COUNT; ++i)
        {
            DebugRenderInfo* info = (DebugRenderInfo*)context->m_DebugRenderer.m_RenderObject3d[i]->m_UserData;
            info->m_VertexCount = 0;
            info = (DebugRenderInfo*)context->m_DebugRenderer.m_RenderObject2d[i]->m_UserData;
            info->m_VertexCount = 0;
        }
    }

#define ADD_TO_RENDER(object)\
    if (((DebugRenderInfo*)object->m_UserData)->m_VertexCount == 0)\
        dmRender::AddToRender(context, object);

    void Square2d(HRenderContext context, float x0, float y0, float x1, float y1, Vector4 color)
    {
        ADD_TO_RENDER(context->m_DebugRenderer.m_RenderObject2d[DEBUG_RENDER_TYPE_FACE]);
        DebugRenderInfo* info = (DebugRenderInfo*)context->m_DebugRenderer.m_RenderObject2d[DEBUG_RENDER_TYPE_FACE]->m_UserData;
        float* v = &info->m_Vertices[info->m_VertexCount*3];
        v[0] = x0; v[1] = y0; v[2] = 0.0f;
        v[3] = x1; v[4] = y0; v[5] = 0.0f;
        v[6] = x0; v[7] = y1; v[8] = 0.0f;
        v[9] = x0; v[10] = y1; v[11] = 0.0f;
        v[12] = x1; v[13] = y1; v[14] = 0.0f;
        v[15] = x1; v[16] = y0; v[17] = 0.0f;
        float* c = &info->m_Colours[info->m_VertexCount*4];

        for (uint32_t i = 0; i < 6; ++i)
            for (uint32_t j = 0; j < 4; ++j)
                c[i*4 + j] = color.getElem(j);
        info->m_VertexCount += 6;
    }

    void Line2D(HRenderContext context, float x0, float y0, float x1, float y1, Vector4 color0, Vector4 color1)
    {
        ADD_TO_RENDER(context->m_DebugRenderer.m_RenderObject2d[DEBUG_RENDER_TYPE_LINE]);
        DebugRenderInfo* info = (DebugRenderInfo*)context->m_DebugRenderer.m_RenderObject2d[DEBUG_RENDER_TYPE_LINE]->m_UserData;
        float* v = &info->m_Vertices[info->m_VertexCount*3];
        v[0] = x0; v[1] = y0; v[2] = 0.0f;
        v[3] = x1; v[4] = y1; v[5] = 0.0f;
        float* c = &info->m_Colours[info->m_VertexCount*4];
        for (uint32_t i = 0; i < 4; ++i)
        {
            c[i] = color0.getElem(i);
            c[4 + i] = color1.getElem(i);
        }
        info->m_VertexCount += 2;
    }

    void Line3D(HRenderContext context, Point3 start, Point3 end, Vector4 start_color, Vector4 end_color)
    {
        ADD_TO_RENDER(context->m_DebugRenderer.m_RenderObject3d[DEBUG_RENDER_TYPE_LINE]);
        DebugRenderInfo* info = (DebugRenderInfo*)context->m_DebugRenderer.m_RenderObject3d[DEBUG_RENDER_TYPE_LINE]->m_UserData;
        float* v = &info->m_Vertices[info->m_VertexCount*3];
        for (uint32_t i = 0; i < 3; ++i)
        {
            v[i] = start.getElem(i);
            v[3 + i] = end.getElem(i);
        }
        float* c = &info->m_Colours[info->m_VertexCount*4];
        for (uint32_t i = 0; i < 4; ++i)
        {
            c[i] = start_color.getElem(i);
            c[4 + i] = end_color.getElem(i);
        }
        info->m_VertexCount += 2;
    }

#undef ADD_TO_RENDER

    void RenderTypeDebugDraw3d(HRenderContext render_context, dmRender::HRenderObject ro, uint32_t count)
    {
        DebugRenderInfo* info = (DebugRenderInfo*)dmRender::GetUserData(ro);

        dmGraphics::HContext context = render_context->m_GFXContext;

        Matrix4* view_proj = GetViewProjectionMatrix(render_context);
        dmGraphics::SetVertexConstantBlock(context, (Vector4*)view_proj, 0, 4);

        dmGraphics::SetVertexStream(context, 0, 3, dmGraphics::TYPE_FLOAT, 0, (void*) info->m_Vertices);
        dmGraphics::SetVertexStream(context, 1, 4, dmGraphics::TYPE_FLOAT, 0, (void*) info->m_Colours);
        dmGraphics::DisableVertexStream(context, 2);

        switch (info->m_RenderType)
        {
            case DEBUG_RENDER_TYPE_FACE:
                dmGraphics::Draw(context, dmGraphics::PRIMITIVE_TRIANGLES, 0, info->m_VertexCount);
                break;
            case DEBUG_RENDER_TYPE_LINE:
                dmGraphics::Draw(context, dmGraphics::PRIMITIVE_LINES, 0, info->m_VertexCount);
                break;
            default:
                break;
        }
    }

    void RenderTypeDebugDraw2d(HRenderContext render_context, dmRender::HRenderObject ro, uint32_t count)
    {
        DebugRenderInfo* info = (DebugRenderInfo*)dmRender::GetUserData(ro);

        dmGraphics::HContext context = render_context->m_GFXContext;

        Matrix4 view_proj = Matrix4::orthographic(-1.0f, 1.0f, 1.0f, -1.0f, 1.0f, -1.0f);
        dmGraphics::SetVertexConstantBlock(context, (Vector4*)&view_proj, 0, 4);

        dmGraphics::SetVertexStream(context, 0, 3, dmGraphics::TYPE_FLOAT, 0, (void*) info->m_Vertices);
        dmGraphics::SetVertexStream(context, 1, 4, dmGraphics::TYPE_FLOAT, 0, (void*) info->m_Colours);
        dmGraphics::DisableVertexStream(context, 2);
        switch (info->m_RenderType)
        {
            case DEBUG_RENDER_TYPE_FACE:
                dmGraphics::Draw(context, dmGraphics::PRIMITIVE_TRIANGLES, 0, info->m_VertexCount);
                break;
            case DEBUG_RENDER_TYPE_LINE:
                dmGraphics::Draw(context, dmGraphics::PRIMITIVE_LINES, 0, info->m_VertexCount);
                break;
            default:
                break;
        }
    }
}
