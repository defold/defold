#include "debug_renderer.h"

#include <stdint.h>

#include <dlib/array.h>
#include <dlib/hash.h>
#include <dlib/log.h>

#include <graphics/graphics.h>

#include "render.h"
#include "render_private.h"

using namespace Vectormath::Aos;

namespace dmRender
{
    struct DebugVertex
    {
        Vector4 m_Position;
        Vector4 m_Color;
    };

    void InitializeDebugRenderer(dmRender::HRenderContext render_context, uint32_t max_vertex_count, const void* vp_data, uint32_t vp_data_size, const void* fp_data, uint32_t fp_data_size)
    {
        DebugRenderer& debug_renderer = render_context->m_DebugRenderer;
        debug_renderer.m_MaxVertexCount = max_vertex_count;

        debug_renderer.m_RenderContext = render_context;
        const uint32_t buffer_size = max_vertex_count * sizeof(DebugVertex);
        const uint32_t total_buffer_size = MAX_DEBUG_RENDER_TYPE_COUNT * buffer_size;
        debug_renderer.m_VertexBuffer = dmGraphics::NewVertexBuffer(render_context->m_GraphicsContext, total_buffer_size, 0x0, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
        dmGraphics::VertexElement ve[] =
        {
            {"position", 0, 4, dmGraphics::TYPE_FLOAT, false },
            {"color", 1, 4, dmGraphics::TYPE_FLOAT, false }
        };
        debug_renderer.m_VertexDeclaration = dmGraphics::NewVertexDeclaration(render_context->m_GraphicsContext, ve, 2);

        dmGraphics::HVertexProgram vertex_program = dmGraphics::INVALID_VERTEX_PROGRAM_HANDLE;
        if (vp_data_size > 0)
            vertex_program = dmGraphics::NewVertexProgram(render_context->m_GraphicsContext, vp_data, vp_data_size);
        dmGraphics::HFragmentProgram fragment_program = dmGraphics::INVALID_FRAGMENT_PROGRAM_HANDLE;
        if (fp_data_size > 0)
            fragment_program = dmGraphics::NewFragmentProgram(render_context->m_GraphicsContext, fp_data, fp_data_size);

        HMaterial material3d = NewMaterial(render_context, vertex_program, fragment_program);
        SetMaterialProgramConstantType(material3d, dmHashString64("view_proj"), dmRenderDDF::MaterialDesc::CONSTANT_TYPE_VIEWPROJ);
        AddMaterialTag(material3d, dmHashString64(DEBUG_3D_NAME));

        HMaterial material2d = NewMaterial(render_context, vertex_program, fragment_program);
        SetMaterialProgramConstantType(material2d, dmHashString64("view_proj"), dmRenderDDF::MaterialDesc::CONSTANT_TYPE_VIEWPROJ);
        AddMaterialTag(material2d, dmHashString64(DEBUG_2D_NAME));

        dmGraphics::PrimitiveType primitive_types[MAX_DEBUG_RENDER_TYPE_COUNT] = {dmGraphics::PRIMITIVE_TRIANGLES, dmGraphics::PRIMITIVE_LINES, dmGraphics::PRIMITIVE_TRIANGLES, dmGraphics::PRIMITIVE_LINES};
        HMaterial materials[MAX_DEBUG_RENDER_TYPE_COUNT] = {material3d, material3d, material2d, material2d};

        for (uint32_t i = 0; i < MAX_DEBUG_RENDER_TYPE_COUNT; ++i)
        {
            RenderObject ro;
            ro.m_Material = materials[i];
            ro.m_PrimitiveType = primitive_types[i];
            ro.m_VertexBuffer = debug_renderer.m_VertexBuffer;
            ro.m_VertexDeclaration = debug_renderer.m_VertexDeclaration;
            ro.m_VertexCount = 0;
            DebugRenderTypeData& type_data = debug_renderer.m_TypeData[i];
            type_data.m_RenderObject = ro;
            type_data.m_ClientBuffer = new char[buffer_size];
        }

        debug_renderer.m_3dPredicate.m_Tags[0] = dmHashString64(DEBUG_3D_NAME);
        debug_renderer.m_3dPredicate.m_TagCount = 1;
        debug_renderer.m_2dPredicate.m_Tags[0] = dmHashString64(DEBUG_2D_NAME);
        debug_renderer.m_2dPredicate.m_TagCount = 1;
        debug_renderer.m_RenderBatchVersion = 0;
    }

    void FinalizeDebugRenderer(HRenderContext context)
    {
        if (!context->m_DebugRenderer.m_RenderContext)
            return;
        DebugRenderer& debug_renderer = context->m_DebugRenderer;
        HMaterial material = debug_renderer.m_TypeData[DEBUG_RENDER_TYPE_FACE_3D].m_RenderObject.m_Material;

        dmGraphics::HVertexProgram vp = GetMaterialVertexProgram(material);
        if (vp != dmGraphics::INVALID_VERTEX_PROGRAM_HANDLE)
            dmGraphics::DeleteVertexProgram(vp);
        dmGraphics::HFragmentProgram fp = GetMaterialFragmentProgram(material);
        if (fp != dmGraphics::INVALID_FRAGMENT_PROGRAM_HANDLE)
            dmGraphics::DeleteFragmentProgram(fp);

        DeleteMaterial(context, material);
        material = debug_renderer.m_TypeData[DEBUG_RENDER_TYPE_FACE_2D].m_RenderObject.m_Material;
        DeleteMaterial(context, material);

        for (uint32_t i = 0; i < MAX_DEBUG_RENDER_TYPE_COUNT; ++i)
        {
            delete [] (char*)debug_renderer.m_TypeData[i].m_ClientBuffer;
        }
        dmGraphics::DeleteVertexBuffer(debug_renderer.m_VertexBuffer);
        dmGraphics::DeleteVertexDeclaration(debug_renderer.m_VertexDeclaration);
    }

    void ClearDebugRenderObjects(HRenderContext context)
    {
        if (!context->m_DebugRenderer.m_RenderContext)
            return;
        for (uint32_t i = 0; i < MAX_DEBUG_RENDER_TYPE_COUNT; ++i)
        {
            context->m_DebugRenderer.m_TypeData[i].m_RenderObject.m_VertexCount = 0;
        }
        context->m_DebugRenderer.m_RenderBatchVersion = 0;
    }

    static void LogVertexWarning(HRenderContext context)
    {
        static bool has_warned = false;
        if (!has_warned)
        {
            dmLogWarning("Out of debug vertex data (%u). Increase graphics.max_debug_vertices to avoid this warning.", context->m_DebugRenderer.m_MaxVertexCount);
            has_warned = true;
        }
    }

    void Square2d(HRenderContext context, float x0, float y0, float x1, float y1, Vector4 color)
    {
        if (!context->m_DebugRenderer.m_RenderContext)
            return;
        DebugRenderTypeData& type_data = context->m_DebugRenderer.m_TypeData[DEBUG_RENDER_TYPE_FACE_2D];
        RenderObject& ro = type_data.m_RenderObject;
        const uint32_t vertex_count = 6;
        if (ro.m_VertexCount + vertex_count < context->m_DebugRenderer.m_MaxVertexCount)
        {
            DebugVertex v[vertex_count];
            v[0].m_Position = Vector4(x0, y0, 0.0f, 0.0f);
            v[1].m_Position = Vector4(x0, y1, 0.0f, 0.0f);
            v[2].m_Position = Vector4(x1, y0, 0.0f, 0.0f);
            v[5].m_Position = Vector4(x1, y1, 0.0f, 0.0f);
            v[3].m_Position = v[2].m_Position;
            v[4].m_Position = v[1].m_Position;
            for (uint32_t i = 0; i < vertex_count; ++i)
                v[i].m_Color = color;
            char* buffer = (char*)type_data.m_ClientBuffer;
            memcpy(&buffer[ro.m_VertexCount * sizeof(DebugVertex)], (const void*)v, vertex_count * sizeof(DebugVertex));
            ro.m_VertexCount += vertex_count;
        }
        else
        {
            LogVertexWarning(context);
        }
    }

    void Triangle3d(HRenderContext context, Point3 vertices[3], Vector4 color)
    {
        if (!context->m_DebugRenderer.m_RenderContext)
            return;
        DebugRenderTypeData& type_data = context->m_DebugRenderer.m_TypeData[DEBUG_RENDER_TYPE_FACE_3D];
        RenderObject& ro = type_data.m_RenderObject;
        const uint32_t vertex_count = 3;
        if (ro.m_VertexCount + vertex_count < context->m_DebugRenderer.m_MaxVertexCount)
        {
            DebugVertex v[vertex_count];
            for (uint32_t i = 0; i < vertex_count; ++i)
            {
                v[i].m_Position = Vector4(vertices[i]);
                v[i].m_Color = color;
            }
            char* buffer = (char*)type_data.m_ClientBuffer;
            memcpy(&buffer[ro.m_VertexCount * sizeof(DebugVertex)], (const void*)v, vertex_count * sizeof(DebugVertex));
            ro.m_VertexCount += vertex_count;
        }
        else
        {
            LogVertexWarning(context);
        }
    }

    void Line2D(HRenderContext context, float x0, float y0, float x1, float y1, Vector4 color0, Vector4 color1)
    {
        if (!context->m_DebugRenderer.m_RenderContext)
            return;
        DebugRenderTypeData& type_data = context->m_DebugRenderer.m_TypeData[DEBUG_RENDER_TYPE_LINE_2D];
        RenderObject& ro = type_data.m_RenderObject;
        const uint32_t vertex_count = 2;
        if (ro.m_VertexCount + vertex_count < context->m_DebugRenderer.m_MaxVertexCount)
        {
            DebugVertex v[vertex_count];
            v[0].m_Position = Vector4(x0, y0, 0.0f, 0.0f);
            v[0].m_Color = color0;
            v[1].m_Position = Vector4(x1, y1, 0.0f, 0.0f);
            v[1].m_Color = color1;
            char* buffer = (char*)type_data.m_ClientBuffer;
            memcpy(&buffer[ro.m_VertexCount * sizeof(DebugVertex)], (const void*)v, vertex_count * sizeof(DebugVertex));
            ro.m_VertexCount += vertex_count;
        }
        else
        {
            LogVertexWarning(context);
        }
    }

    void Line3D(HRenderContext context, Point3 start, Point3 end, Vector4 start_color, Vector4 end_color)
    {
        if (!context->m_DebugRenderer.m_RenderContext)
            return;
        DebugRenderTypeData& type_data = context->m_DebugRenderer.m_TypeData[DEBUG_RENDER_TYPE_LINE_3D];
        RenderObject& ro = type_data.m_RenderObject;
        const uint32_t vertex_count = 2;
        if (ro.m_VertexCount + vertex_count < context->m_DebugRenderer.m_MaxVertexCount)
        {
            DebugVertex v[vertex_count];
            v[0].m_Position = Vector4(start);
            v[0].m_Color = start_color;
            v[1].m_Position = Vector4(end);
            v[1].m_Color = end_color;
            char* buffer = (char*)type_data.m_ClientBuffer;
            memcpy(&buffer[ro.m_VertexCount * sizeof(DebugVertex)], (const void*)v, vertex_count * sizeof(DebugVertex));
            ro.m_VertexCount += vertex_count;
        }
        else
        {
            LogVertexWarning(context);
        }
    }

    static void DebugRenderListDispatch(dmRender::RenderListDispatchParams const &params)
    {
        DebugRenderer *debug_renderer = (DebugRenderer *)params.m_UserData;

        if (params.m_Operation == dmRender::RENDER_LIST_OPERATION_BATCH)
        {
            for (uint32_t *i=params.m_Begin;i!=params.m_End;i++)
            {
                if (params.m_Buf[*i].m_BatchKey == debug_renderer->m_RenderBatchVersion)
                {
                    dmRender::RenderObject *ro = (dmRender::RenderObject*) params.m_Buf[*i].m_UserData;
                    dmRender::AddToRender(params.m_Context, ro);
                }
            }
        }
    }

    void FlushDebug(HRenderContext render_context, uint32_t render_order)
    {
        if (!render_context->m_DebugRenderer.m_RenderContext)
            return;
        DebugRenderer& debug_renderer = render_context->m_DebugRenderer;
        uint32_t total_vertex_count = 0;
        uint32_t total_render_objects = 0;
        dmGraphics::SetVertexBufferData(debug_renderer.m_VertexBuffer, 0, 0, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
        for (uint32_t i = 0; i < MAX_DEBUG_RENDER_TYPE_COUNT; ++i)
        {
            DebugRenderTypeData& type_data = debug_renderer.m_TypeData[i];
            RenderObject& ro = type_data.m_RenderObject;
            uint32_t vertex_count = ro.m_VertexCount;
            if (vertex_count > 0)
            {
                ro.m_VertexStart = total_vertex_count;
                total_vertex_count += vertex_count;
                total_render_objects++;
            }
        }

        dmGraphics::SetVertexBufferData(debug_renderer.m_VertexBuffer, total_vertex_count * sizeof(DebugVertex), 0, dmGraphics::BUFFER_USAGE_STREAM_DRAW);

        dmRender::RenderListEntry* render_list = dmRender::RenderListAlloc(render_context, total_render_objects);
        dmRender::HRenderListDispatch dispatch = dmRender::RenderListMakeDispatch(render_context, &DebugRenderListDispatch, &debug_renderer);
        dmRender::RenderListEntry* write_ptr = render_list;

        // Upgrade the batch key, since we might submit these render object multiple times
        // during a frame (because render scripts might draw lines, and then we are forced to flush
        // debug rendering during command queue execution, and thus submit the same objects again).
        //
        // TODO: Rewrite this so we can flush only what has been added since last time.
        //
        debug_renderer.m_RenderBatchVersion++;

        for (uint32_t i = 0; i < MAX_DEBUG_RENDER_TYPE_COUNT; ++i)
        {
            DebugRenderTypeData& type_data = debug_renderer.m_TypeData[i];
            RenderObject& ro = type_data.m_RenderObject;
            uint32_t vertex_count = ro.m_VertexCount;
            if (vertex_count > 0)
            {
                dmGraphics::SetVertexBufferSubData(debug_renderer.m_VertexBuffer, ro.m_VertexStart * sizeof(DebugVertex), vertex_count * sizeof(DebugVertex), type_data.m_ClientBuffer);
                write_ptr->m_MajorOrder = RENDER_ORDER_AFTER_WORLD;
                write_ptr->m_Order = render_order;
                write_ptr->m_UserData = (uintptr_t) &ro;
                write_ptr->m_BatchKey = debug_renderer.m_RenderBatchVersion;
                write_ptr->m_TagMask = dmRender::GetMaterialTagMask(ro.m_Material);
                write_ptr->m_Dispatch = dispatch;
                write_ptr++;
            }
        }

        dmRender::RenderListSubmit(render_context, render_list, write_ptr);
    }
}
