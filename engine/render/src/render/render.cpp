#include <assert.h>
#include <string.h>
#include <float.h>
#include <algorithm>

#include <dlib/hash.h>
#include <dlib/hashtable.h>
#include <dlib/profile.h>
#include <dlib/math.h>

#include <ddf/ddf.h>

#include "render_private.h"
#include "render_script.h"
#include "debug_renderer.h"
#include "font_renderer.h"

namespace dmRender
{
    using namespace Vectormath::Aos;

    const char* RENDER_SOCKET_NAME = "@render";

    StencilTestParams::StencilTestParams() {
        Init();
    }

    void StencilTestParams::Init() {
        m_Func = dmGraphics::COMPARE_FUNC_ALWAYS;
        m_OpSFail = dmGraphics::STENCIL_OP_KEEP;
        m_OpDPFail = dmGraphics::STENCIL_OP_KEEP;
        m_OpDPPass = dmGraphics::STENCIL_OP_KEEP;
        m_Ref = 0;
        m_RefMask = 0xff;
        m_BufferMask = 0xff;
        m_ColorBufferMask = 0xf;
        m_ClearBuffer = 0;
        m_Padding = 0;
    }

    RenderObject::RenderObject()
    {
        Init();
    }

    void RenderObject::Init()
    {
        // See case 2264 why this method was added
        memset(this, 0, sizeof(RenderObject));
        m_WorldTransform = Matrix4::identity();
        m_TextureTransform = Matrix4::identity();

        for (uint32_t i = 0; i < RenderObject::MAX_CONSTANT_COUNT; ++i)
        {
            m_Constants[i].m_Location = -1;
        }
    }

    void RenderObject::ClearConstants()
    {
        for (uint32_t i = 0; i < RenderObject::MAX_CONSTANT_COUNT; ++i)
        {
            m_Constants[i].m_Location = -1;
        }
    }

    RenderContextParams::RenderContextParams()
    : m_ScriptContext(0x0)
    , m_SystemFontMap(0)
    , m_VertexProgramData(0x0)
    , m_FragmentProgramData(0x0)
    , m_MaxRenderTypes(0)
    , m_MaxInstances(0)
    , m_MaxRenderTargets(0)
    , m_VertexProgramDataSize(0)
    , m_FragmentProgramDataSize(0)
    , m_MaxCharacters(0)
    , m_CommandBufferSize(1024)
    , m_MaxDebugVertexCount(0)
    {

    }

    RenderScriptContext::RenderScriptContext()
    : m_LuaState(0)
    , m_CommandBufferSize(0)
    {

    }

    HRenderContext NewRenderContext(dmGraphics::HContext graphics_context, const RenderContextParams& params)
    {
        RenderContext* context = new RenderContext;

        context->m_CurrentRenderTarget = 0x0;
        context->m_RenderTargets.SetCapacity(params.m_MaxRenderTargets);

        context->m_RenderObjects.SetCapacity(params.m_MaxInstances);
        context->m_RenderObjects.SetSize(0);

        context->m_GraphicsContext = graphics_context;

        context->m_SystemFontMap = params.m_SystemFontMap;

        context->m_Material = 0;

        context->m_View = Matrix4::identity();
        context->m_Projection = Matrix4::identity();
        context->m_ViewProj = context->m_Projection * context->m_View;

        context->m_ScriptContext = params.m_ScriptContext;
        InitializeRenderScriptContext(context->m_RenderScriptContext, params.m_ScriptContext, params.m_CommandBufferSize);

        context->m_DebugRenderer.m_RenderContext = 0;
        if (params.m_VertexProgramData != 0 && params.m_VertexProgramDataSize != 0 &&
            params.m_FragmentProgramData != 0 && params.m_FragmentProgramDataSize != 0) {
            InitializeDebugRenderer(context, params.m_MaxDebugVertexCount, params.m_VertexProgramData, params.m_VertexProgramDataSize, params.m_FragmentProgramData, params.m_FragmentProgramDataSize);
        }

        memset(context->m_Textures, 0, sizeof(dmGraphics::HTexture) * RenderObject::MAX_TEXTURE_COUNT);

        InitializeTextContext(context, params.m_MaxCharacters);

        context->m_OutOfResources = 0;

        context->m_StencilBufferCleared = 0;

        context->m_RenderListDispatch.SetCapacity(255);

        context->m_ResourceFactory = params.m_Factory;

        dmMessage::Result r = dmMessage::NewSocket(RENDER_SOCKET_NAME, &context->m_Socket);
        assert(r == dmMessage::RESULT_OK);

        return context;
    }

    Result DeleteRenderContext(HRenderContext render_context, dmScript::HContext script_context)
    {
        if (render_context == 0x0) return RESULT_INVALID_CONTEXT;

        FinalizeRenderScriptContext(render_context->m_RenderScriptContext, script_context);
        FinalizeDebugRenderer(render_context);
        FinalizeTextContext(render_context);
        dmMessage::DeleteSocket(render_context->m_Socket);
        delete render_context;

        return RESULT_OK;
    }

    dmScript::HContext GetScriptContext(HRenderContext render_context) {
        return render_context->m_ScriptContext;
    }

    void RenderListBegin(HRenderContext render_context)
    {
        render_context->m_RenderList.SetSize(0);
        render_context->m_RenderListSortIndices.SetSize(0);
        render_context->m_RenderListDispatch.SetSize(0);
    }

    HRenderListDispatch RenderListMakeDispatch(HRenderContext render_context, RenderListDispatchFn fn, void *user_data)
    {
        if (render_context->m_RenderListDispatch.Size() == render_context->m_RenderListDispatch.Capacity())
        {
            dmLogError("Exhausted number of render dispatches. Too many collections?");
            return RENDERLIST_INVALID_DISPATCH;
        }

        // store & return index
        RenderListDispatch d;
        d.m_Fn = fn;
        d.m_UserData = user_data;
        render_context->m_RenderListDispatch.Push(d);

        return render_context->m_RenderListDispatch.Size() - 1;
    }

    // Allocate a buffer (from the array) with room for 'entries' entries.
    //
    // NOTE: Pointer might go invalid after a consecutive call to RenderListAlloc if reallocatino
    //       of backing buffer happens.
    RenderListEntry* RenderListAlloc(HRenderContext render_context, uint32_t entries)
    {
        dmArray<RenderListEntry> & render_list = render_context->m_RenderList;

        if (render_list.Remaining() < entries)
        {
            const uint32_t needed = entries - render_list.Remaining();
            render_list.OffsetCapacity(dmMath::Max<uint32_t>(256, needed));
            render_context->m_RenderListSortIndices.SetCapacity(render_list.Capacity());
        }

        uint32_t size = render_list.Size();
        render_list.SetSize(size + entries);
        return (render_list.Begin() + size);
    }

    // Submit a range of entries (pointers must be from a range allocated by RenderListAlloc, and not between two alloc calls).
    void RenderListSubmit(HRenderContext render_context, RenderListEntry *begin, RenderListEntry *end)
    {
        // Insert the used up indices into the sort buffer.
        assert(end - begin <= render_context->m_RenderListSortIndices.Remaining());

        // Transform pointers back to indices.
        RenderListEntry *base = render_context->m_RenderList.Begin();
        uint32_t *insert = render_context->m_RenderListSortIndices.End();

        for (RenderListEntry* i=begin;i!=end;i++)
            *insert++ = i - base;

        render_context->m_RenderListSortIndices.SetSize(render_context->m_RenderListSortIndices.Size() + (end - begin));
    }

    struct RenderListSorter
    {
        bool operator()(uint32_t a, uint32_t b) const
        {
            const RenderListSortValue& u = values[a];
            const RenderListSortValue& v = values[b];
            if (u.m_SortKey == v.m_SortKey)
                return a < b;
            return u.m_SortKey < v.m_SortKey;
        }
        RenderListSortValue* values;
    };

    void RenderListEnd(HRenderContext render_context)
    {
        // Unflushed leftovers are assumed to be the debug rendering
        // and we give them render orders statically here
        FlushTexts(render_context, RENDER_ORDER_AFTER_WORLD, 0xffffff, true);

    }

    void SetSystemFontMap(HRenderContext render_context, HFontMap font_map)
    {
        render_context->m_SystemFontMap = font_map;
    }

    Result RegisterRenderTarget(HRenderContext render_context, dmGraphics::HRenderTarget rendertarget, dmhash_t hash)
    {
        if (render_context == 0x0)
            return RESULT_INVALID_CONTEXT;
        if (render_context->m_RenderTargets.Full())
            return RESULT_BUFFER_IS_FULL;

        RenderTargetSetup setup;
        setup.m_RenderTarget = rendertarget;
        setup.m_Hash = hash;
        render_context->m_RenderTargets.Push(setup);

        return RESULT_OK;
    }

    dmGraphics::HRenderTarget GetRenderTarget(HRenderContext render_context, dmhash_t hash)
    {
        for (uint32_t i=0; i < render_context->m_RenderTargets.Size(); i++)
        {
            if (render_context->m_RenderTargets[i].m_Hash == hash)
                return render_context->m_RenderTargets[i].m_RenderTarget;
        }

        return 0x0;
    }

    dmGraphics::HContext GetGraphicsContext(HRenderContext render_context)
    {
        return render_context->m_GraphicsContext;
    }

    const Matrix4& GetViewProjectionMatrix(HRenderContext render_context)
    {
        return render_context->m_ViewProj;
    }

    void SetViewMatrix(HRenderContext render_context, const Matrix4& view)
    {
        render_context->m_View = view;
        render_context->m_ViewProj = render_context->m_Projection * view;
    }

    void SetProjectionMatrix(HRenderContext render_context, const Matrix4& projection)
    {
        render_context->m_Projection = projection;
        render_context->m_ViewProj = projection * render_context->m_View;
    }

    Result AddToRender(HRenderContext context, RenderObject* ro)
    {
        if (context == 0x0) return RESULT_INVALID_CONTEXT;
        if (context->m_RenderObjects.Full())
        {
            if (!context->m_OutOfResources)
            {
                dmLogWarning("Renderer is out of resources, some objects will not be rendered.");
                context->m_OutOfResources = 1;
            }
            return RESULT_OUT_OF_RESOURCES;
        }
        context->m_RenderObjects.Push(ro);

        return RESULT_OK;
    }

    Result ClearRenderObjects(HRenderContext context)
    {
        context->m_RenderObjects.SetSize(0);
        ClearDebugRenderObjects(context);

        // Should probably be moved and/or refactored, see case 2261
        // (Cannot reset the text buffer until all render objects are dispatched)
        context->m_TextContext.m_RenderObjectIndex = 0;
        context->m_TextContext.m_VertexIndex = 0;
        context->m_TextContext.m_VerticesFlushed = 0;
        context->m_TextContext.m_Frame += 1;
        context->m_TextContext.m_TextBuffer.SetSize(0);
        context->m_TextContext.m_TextEntries.SetSize(0);
        context->m_TextContext.m_TextEntriesFlushed = 0;

        return RESULT_OK;
    }

    static void ApplyStencilTest(HRenderContext render_context, const RenderObject* ro)
    {
        dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(render_context);
        const StencilTestParams& stp = ro->m_StencilTestParams;
        if (stp.m_ClearBuffer)
        {
            if (render_context->m_StencilBufferCleared)
            {
                // render.clear command will set context m_StencilBufferCleared to 1 if stencil clear flag is set.
                // We skip clear and reset context m_StencilBufferCleared to 0, indicating that the stencil is no longer cleared.
                // Concecutive calls with m_ClearBuffer option will result in a clear until render.clear is called with stencil clear flag set.
                render_context->m_StencilBufferCleared = 0;
            }
            else
            {
                dmGraphics::SetStencilMask(graphics_context, 0xff);
                dmGraphics::Clear(graphics_context, dmGraphics::BUFFER_TYPE_STENCIL_BIT, 0, 0, 0, 0, 1.0f, 0);
            }
        }
        dmGraphics::SetColorMask(graphics_context, stp.m_ColorBufferMask & (1<<3), stp.m_ColorBufferMask & (1<<2), stp.m_ColorBufferMask & (1<<1), stp.m_ColorBufferMask & (1<<0));
        dmGraphics::SetStencilMask(graphics_context, stp.m_BufferMask);
        dmGraphics::SetStencilFunc(graphics_context, stp.m_Func, stp.m_Ref, stp.m_RefMask);
        dmGraphics::SetStencilOp(graphics_context, stp.m_OpSFail, stp.m_OpDPFail, stp.m_OpDPPass);
    }

    void ApplyRenderObjectConstants(HRenderContext render_context, HMaterial material, const RenderObject* ro)
    {
        dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(render_context);
        if(!material)
        {
            for (uint32_t i = 0; i < RenderObject::MAX_CONSTANT_COUNT; ++i)
            {
                const Constant* c = &ro->m_Constants[i];
                if (c->m_Location != -1)
                {
                    dmGraphics::SetConstantV4(graphics_context, &c->m_Value, c->m_Location);
                }
            }
            return;
        }
        for (uint32_t i = 0; i < RenderObject::MAX_CONSTANT_COUNT; ++i)
        {
            const Constant* c = &ro->m_Constants[i];
            if (c->m_Location != -1)
            {
                int32_t* location = material->m_NameHashToLocation.Get(ro->m_Constants[i].m_NameHash);
                if (location)
                {
                    dmGraphics::SetConstantV4(graphics_context, &c->m_Value, *location);
                }
            }
        }
    }

    // Compute new sort values for everything that matches tag_mask
    static void MakeSortBuffer(HRenderContext context, uint32_t tag_mask)
    {
        const uint32_t count = context->m_RenderListSortIndices.Size();

        const uint32_t required_capacity = context->m_RenderListSortIndices.Capacity();
        // SetCapacity does early out if they are the same, so just call anyway.
        context->m_RenderListSortBuffer.SetCapacity(required_capacity);
        context->m_RenderListSortBuffer.SetSize(0);
        context->m_RenderListSortValues.SetCapacity(required_capacity);
        context->m_RenderListSortValues.SetSize(context->m_RenderListSortIndices.Size());

        RenderListSortValue* sort_values = context->m_RenderListSortValues.Begin();

        const Matrix4& transform = context->m_ViewProj;
        RenderListEntry *base = context->m_RenderList.Begin();

        float minZW = FLT_MAX;
        float maxZW = -FLT_MAX;


        // Write z values and compute range
        int c = 0;
        for (uint32_t i=0;i!=count;i++)
        {
            uint32_t idx = context->m_RenderListSortIndices[i];
            RenderListEntry *entry = &base[idx];
            if ((entry->m_TagMask & tag_mask) != tag_mask)
                continue;
            if (entry->m_MajorOrder != RENDER_ORDER_WORLD)
                continue;

            const Point3& world_pos = entry->m_WorldPosition;
            const Vector4 tmp(world_pos.getX(), world_pos.getY(), world_pos.getZ(), 1.0f);
            const Vector4 res = transform * tmp;
            const float zw = res.getZ() / res.getW();
            sort_values[idx].m_ZW = zw;
            if (zw < minZW) minZW = zw;
            if (zw > maxZW) maxZW = zw;
            c++;
        }

        float rc = 0;
        if (c > 1 && maxZW != minZW)
            rc = 1.0f / (maxZW - minZW);

        for (uint32_t i=0;i!=count;i++)
        {
            uint32_t idx = context->m_RenderListSortIndices[i];
            RenderListEntry *entry = &base[idx];
            if ((entry->m_TagMask & tag_mask) != tag_mask)
                continue;

            sort_values[idx].m_MajorOrder = entry->m_MajorOrder;
            if (entry->m_MajorOrder == RENDER_ORDER_WORLD)
            {
                const float z = sort_values[idx].m_ZW;
                sort_values[idx].m_Order = (uint32_t) (0xfffff8 - 0xfffff0 * rc * (z - minZW));
            }
            else
            {
                // use the integer value provided.
                sort_values[idx].m_Order = entry->m_Order;
            }
            sort_values[idx].m_BatchKey = entry->m_BatchKey & 0xffffff;
            sort_values[idx].m_Dispatch = entry->m_Dispatch;
            context->m_RenderListSortBuffer.Push(idx);
        }
    }

    Result DrawRenderList(HRenderContext context, Predicate* predicate, HNamedConstantBuffer constant_buffer)
    {
        DM_PROFILE(Render, "DrawRenderList");

        // This will add new entries for the most recent debug draw render objects.
        // The internal dispatch functions knows to only actually use the latest ones.
        // The sort order is also one below the Texts flush which is only also debug stuff.
        FlushDebug(context, 0xfffffe);

        uint32_t tag_mask = 0;
        if (predicate != 0x0)
            tag_mask = ConvertMaterialTagsToMask(&predicate->m_Tags[0], predicate->m_TagCount);

        MakeSortBuffer(context, tag_mask);

        RenderListSorter sort;
        sort.values = context->m_RenderListSortValues.Begin();
        std::sort(context->m_RenderListSortBuffer.Begin(), context->m_RenderListSortBuffer.End(), sort);

        // Construct render objects
        context->m_RenderObjects.SetSize(0);

        RenderListDispatchParams params;
        memset(&params, 0x00, sizeof(params));
        params.m_Operation = RENDER_LIST_OPERATION_BEGIN;
        params.m_Context = context;

        // All get begin operation first
        for (uint32_t i=0;i!=context->m_RenderListDispatch.Size();i++)
        {
            const RenderListDispatch& d = context->m_RenderListDispatch[i];
            params.m_UserData = d.m_UserData;
            d.m_Fn(params);
        }

        params.m_Operation = RENDER_LIST_OPERATION_BATCH;
        params.m_Buf = context->m_RenderList.Begin();

        // Make batches for matching dispatch & batch key
        RenderListEntry *base = context->m_RenderList.Begin();
        uint32_t *last = context->m_RenderListSortBuffer.Begin();
        uint32_t count = context->m_RenderListSortBuffer.Size();

        for (uint32_t i=1;i<=count;i++)
        {
            uint32_t *idx = context->m_RenderListSortBuffer.Begin() + i;
            const RenderListEntry *last_entry = &base[*last];

            // continue batch on match, or dispatch
            if (i < count && (last_entry->m_Dispatch == base[*idx].m_Dispatch && last_entry->m_BatchKey == base[*idx].m_BatchKey))
                continue;

            if (last_entry->m_Dispatch != RENDERLIST_INVALID_DISPATCH)
            {
                assert(last_entry->m_Dispatch < context->m_RenderListDispatch.Size());
                const RenderListDispatch* d = &context->m_RenderListDispatch[last_entry->m_Dispatch];
                params.m_UserData = d->m_UserData;
                params.m_Begin = last;
                params.m_End = idx;
                d->m_Fn(params);
            }

            last = idx;
        }

        params.m_Operation = RENDER_LIST_OPERATION_END;
        params.m_Begin = 0;
        params.m_End = 0;
        params.m_Buf = 0;

        for (uint32_t i=0;i!=context->m_RenderListDispatch.Size();i++)
        {
            const RenderListDispatch& d = context->m_RenderListDispatch[i];
            params.m_UserData = d.m_UserData;
            d.m_Fn(params);
        }

        return Draw(context, predicate, constant_buffer);
    }

    Result Draw(HRenderContext render_context, Predicate* predicate, HNamedConstantBuffer constant_buffer)
    {
        if (render_context == 0x0)
            return RESULT_INVALID_CONTEXT;

        uint32_t tag_mask = 0;
        if (predicate != 0x0)
            tag_mask = ConvertMaterialTagsToMask(&predicate->m_Tags[0], predicate->m_TagCount);

        dmGraphics::HContext context = dmRender::GetGraphicsContext(render_context);

        HMaterial material = render_context->m_Material;
        HMaterial context_material = render_context->m_Material;
        if(context_material)
        {
            dmGraphics::EnableProgram(context, GetMaterialProgram(context_material));
        }

        dmGraphics::HTexture current_color_attachment = 0x0;
        if (render_context->m_CurrentRenderTarget)
        {
            current_color_attachment = dmGraphics::GetRenderTargetTexture(render_context->m_CurrentRenderTarget, dmGraphics::BUFFER_TYPE_COLOR_BIT);
        }

        for (uint32_t i = 0; i < render_context->m_RenderObjects.Size(); ++i)
        {
            RenderObject* ro = render_context->m_RenderObjects[i];

            if (ro->m_VertexCount > 0 && (GetMaterialTagMask(ro->m_Material) & tag_mask) == tag_mask)
            {
                if (!context_material)
                {
                    if(material != ro->m_Material)
                    {
                        material = ro->m_Material;
                        dmGraphics::EnableProgram(context, GetMaterialProgram(material));
                    }
                }

                ApplyMaterialConstants(render_context, material, ro);
                ApplyRenderObjectConstants(render_context, context_material, ro);

                if (constant_buffer)
                    ApplyNamedConstantBuffer(render_context, material, constant_buffer);

                if (ro->m_SetBlendFactors)
                    dmGraphics::SetBlendFunc(context, ro->m_SourceBlendFactor, ro->m_DestinationBlendFactor);

                if (ro->m_SetStencilTest)
                    ApplyStencilTest(render_context, ro);

                for (uint32_t i = 0; i < RenderObject::MAX_TEXTURE_COUNT; ++i)
                {
                    dmGraphics::HTexture texture = ro->m_Textures[i];
                    if (render_context->m_Textures[i])
                        texture = render_context->m_Textures[i];
                    if (texture)
                    {
                        if (texture != current_color_attachment) {
                            dmGraphics::EnableTexture(context, i, texture);
                            ApplyMaterialSampler(render_context, material, i, texture);
                        } else {
                            dmLogWarning("Trying to use same texture as being rendered to. Texture will not be bound!");
                        }
                    }

                }

                dmGraphics::EnableVertexDeclaration(context, ro->m_VertexDeclaration, ro->m_VertexBuffer, GetMaterialProgram(material));

                if (ro->m_IndexBuffer)
                    dmGraphics::DrawElements(context, ro->m_PrimitiveType, ro->m_VertexCount, ro->m_IndexType, ro->m_IndexBuffer);
                else
                    dmGraphics::Draw(context, ro->m_PrimitiveType, ro->m_VertexStart, ro->m_VertexCount);

                dmGraphics::DisableVertexDeclaration(context, ro->m_VertexDeclaration);

                for (uint32_t i = 0; i < RenderObject::MAX_TEXTURE_COUNT; ++i)
                {
                    dmGraphics::HTexture texture = ro->m_Textures[i];
                    if (render_context->m_Textures[i])
                        texture = render_context->m_Textures[i];
                    if (texture)
                        dmGraphics::DisableTexture(context, i, texture);
                }

            }
        }
        return RESULT_OK;
    }

    Result DrawDebug3d(HRenderContext context)
    {
        if (!context->m_DebugRenderer.m_RenderContext) {
            return RESULT_INVALID_CONTEXT;
        }
        return DrawRenderList(context, &context->m_DebugRenderer.m_3dPredicate, 0);
    }

    Result DrawDebug2d(HRenderContext context)
    {
        if (!context->m_DebugRenderer.m_RenderContext) {
            return RESULT_INVALID_CONTEXT;
        }
        return DrawRenderList(context, &context->m_DebugRenderer.m_2dPredicate, 0);
    }

    void EnableRenderObjectConstant(RenderObject* ro, dmhash_t name_hash, const Vector4& value)
    {
        assert(ro);
        HMaterial material = ro->m_Material;
        assert(material);

        int32_t location = GetMaterialConstantLocation(material, name_hash);
        if (location == -1)
        {
            // Unknown constant, ie at least not defined in material
            return;
        }

        for (uint32_t i = 0; i < RenderObject::MAX_CONSTANT_COUNT; ++i)
        {
            Constant* c = &ro->m_Constants[i];
            if (c->m_Location == -1 || c->m_NameHash == name_hash)
            {
                // New or current slot found
                c->m_Value = value;
                c->m_NameHash = name_hash;
                c->m_Type = dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER;
                c->m_Location = location;
                return;
            }
        }

        dmLogError("Out of per object constant slots, max %d, when setting constant '%s' '", RenderObject::MAX_CONSTANT_COUNT, dmHashReverseSafe64(name_hash));
    }

    void DisableRenderObjectConstant(RenderObject* ro, dmhash_t name_hash)
    {
        assert(ro);
        for (uint32_t i = 0; i < RenderObject::MAX_CONSTANT_COUNT; ++i)
        {
            Constant* c = &ro->m_Constants[i];
            if (c->m_NameHash == name_hash)
            {
                c->m_Location = -1;
                return;
            }
        }
    }


    struct NamedConstantBuffer
    {
        dmHashTable64<Vectormath::Aos::Vector4> m_Constants;
    };

    HNamedConstantBuffer NewNamedConstantBuffer()
    {
        HNamedConstantBuffer buffer = new NamedConstantBuffer();
        buffer->m_Constants.SetCapacity(16, 8);
        return buffer;
    }

    void DeleteNamedConstantBuffer(HNamedConstantBuffer buffer)
    {
        delete buffer;
    }

    void SetNamedConstant(HNamedConstantBuffer buffer, const char* name, Vectormath::Aos::Vector4 value)
    {
        dmHashTable64<Vectormath::Aos::Vector4>& constants = buffer->m_Constants;
        if (constants.Full())
        {
            uint32_t capacity = constants.Capacity();
            capacity += 8;
            constants.SetCapacity(capacity * 2, capacity);
        }
        constants.Put(dmHashString64(name), value);
    }

    bool GetNamedConstant(HNamedConstantBuffer buffer, const char* name, Vectormath::Aos::Vector4& value)
    {
        Vectormath::Aos::Vector4*v = buffer->m_Constants.Get(dmHashString64(name));
        if (v)
        {
            value = *v;
            return true;
        }
        else
        {
            return false;
        }
    }

    struct ApplyContext
    {
        dmGraphics::HContext m_GraphicsContext;
        HMaterial            m_Material;
        ApplyContext(dmGraphics::HContext graphics_context, HMaterial material)
        {
            m_GraphicsContext = graphics_context;
            m_Material = material;
        }
    };

    static inline void ApplyConstant(ApplyContext* context, const uint64_t* name_hash, Vectormath::Aos::Vector4* value)
    {
        int32_t* location = context->m_Material->m_NameHashToLocation.Get(*name_hash);
        if (location)
        {
            dmGraphics::SetConstantV4(context->m_GraphicsContext, value, *location);
        }
    }

    void ApplyNamedConstantBuffer(dmRender::HRenderContext render_context, HMaterial material, HNamedConstantBuffer buffer)
    {
        dmHashTable64<Vectormath::Aos::Vector4>& constants = buffer->m_Constants;
        dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(render_context);
        ApplyContext context(graphics_context, material);
        constants.Iterate(ApplyConstant, &context);
    }

}
