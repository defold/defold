// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include "debug_renderer.h"

#include <stdint.h>

#include <dlib/array.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dmsdk/dlib/vmath.h>

#include <graphics/graphics.h>

#include "render.h"
#include "render_private.h"

namespace dmRender
{
    using namespace dmVMath;

    struct DebugVertex
    {
        Vector4 m_Position;
        Vector4 m_Color;
    };

    void InitializeDebugRenderer(dmRender::HRenderContext render_context, uint32_t max_vertex_count, const void* sp_desc, uint32_t sp_desc_size)
    {
        DebugRenderer& debug_renderer = render_context->m_DebugRenderer;
        debug_renderer.m_MaxVertexCount = max_vertex_count;

        debug_renderer.m_RenderContext = render_context;
        const uint32_t buffer_size = max_vertex_count * sizeof(DebugVertex);
        const uint32_t total_buffer_size = MAX_DEBUG_RENDER_TYPE_COUNT * buffer_size;
        debug_renderer.m_VertexBuffer = dmGraphics::NewVertexBuffer(render_context->m_GraphicsContext, total_buffer_size, 0x0, dmGraphics::BUFFER_USAGE_STREAM_DRAW);

        dmGraphics::HVertexStreamDeclaration stream_declaration = dmGraphics::NewVertexStreamDeclaration(render_context->m_GraphicsContext);

        dmGraphics::AddVertexStream(stream_declaration, "position", 4, dmGraphics::TYPE_FLOAT, false);
        dmGraphics::AddVertexStream(stream_declaration, "color", 4, dmGraphics::TYPE_FLOAT, false);

        debug_renderer.m_VertexDeclaration = dmGraphics::NewVertexDeclaration(render_context->m_GraphicsContext, stream_declaration);

        dmGraphics::DeleteVertexStreamDeclaration(stream_declaration);

        dmGraphics::HProgram program = dmGraphics::INVALID_PROGRAM_HANDLE;

        if (sp_desc_size > 0)
        {
            dmGraphics::ShaderDesc* shader_desc;
            dmDDF::Result e = dmDDF::LoadMessage(sp_desc, sp_desc_size, &dmGraphics_ShaderDesc_DESCRIPTOR, (void**) &shader_desc);
            if (e != dmDDF::RESULT_OK)
            {
                dmLogWarning("Failed to create DebugRenderer shader (%d)", e);
            }
            else
            {
                program = dmGraphics::NewProgram(render_context->m_GraphicsContext, shader_desc, 0, 0);
                dmDDF::FreeMessage(shader_desc);
            }
        }

        HMaterial material3d = NewMaterial(render_context, program);
        SetMaterialProgramConstantType(material3d, dmHashString64("view_proj"), dmRenderDDF::MaterialDesc::CONSTANT_TYPE_VIEWPROJ);
        dmhash_t debug_tag_3d = dmHashString64(DEBUG_3D_NAME);
        SetMaterialTags(material3d, 1, &debug_tag_3d);

        HMaterial material2d = NewMaterial(render_context, program);
        SetMaterialProgramConstantType(material2d, dmHashString64("view_proj"), dmRenderDDF::MaterialDesc::CONSTANT_TYPE_VIEWPROJ);
        dmhash_t debug_tag_2d = dmHashString64(DEBUG_2D_NAME);
        SetMaterialTags(material2d, 1, &debug_tag_2d);

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

        dmGraphics::HProgram program = GetMaterialProgram(material);
        if (program != dmGraphics::INVALID_PROGRAM_HANDLE)
            dmGraphics::DeleteProgram(context->m_GraphicsContext, program);

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
                write_ptr->m_MinorOrder = 0;
                write_ptr->m_MajorOrder = RENDER_ORDER_AFTER_WORLD;
                write_ptr->m_Order = render_order;
                write_ptr->m_UserData = (uintptr_t) &ro;
                write_ptr->m_BatchKey = debug_renderer.m_RenderBatchVersion;
                write_ptr->m_TagListKey = dmRender::GetMaterialTagListKey(ro.m_Material);
                write_ptr->m_Dispatch = dispatch;
                write_ptr++;
            }
        }

        dmRender::RenderListSubmit(render_context, render_list, write_ptr);
    }
}
