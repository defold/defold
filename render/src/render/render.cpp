#include <assert.h>
#include <string.h>

#include <dlib/hash.h>
#include <dlib/profile.h>

#include <ddf/ddf.h>

#include "render_private.h"
#include "render_script.h"
#include "debug_renderer.h"
#include "font_renderer.h"

namespace dmRender
{
    using namespace Vectormath::Aos;

    const char* RENDER_SOCKET_NAME = "@render";

    RenderObject::RenderObject()
    {
        memset(this, 0, sizeof(RenderObject));
        m_WorldTransform = Matrix4::identity();
        m_TextureTransform = Matrix4::identity();
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

        context->m_RenderTargets.SetCapacity(params.m_MaxRenderTargets);

        context->m_RenderObjects.SetCapacity(params.m_MaxInstances);
        context->m_RenderObjects.SetSize(0);

        context->m_GraphicsContext = graphics_context;

        context->m_SystemFontMap = params.m_SystemFontMap;

        context->m_Material = 0;

        context->m_View = Matrix4::identity();
        context->m_Projection = Matrix4::identity();
        context->m_ViewProj = context->m_Projection * context->m_View;

        InitializeRenderScriptContext(context->m_RenderScriptContext, params.m_ScriptContext, params.m_CommandBufferSize);

        InitializeDebugRenderer(context, params.m_VertexProgramData, params.m_VertexProgramDataSize, params.m_FragmentProgramData, params.m_FragmentProgramDataSize);

        context->m_VertexConstantMask = 0;
        context->m_FragmentConstantMask = 0;

        memset(context->m_Textures, 0, sizeof(dmGraphics::HTexture) * RenderObject::MAX_TEXTURE_COUNT);

        InitializeTextContext(context, params.m_MaxCharacters);

        context->m_OutOfResources = 0;

        assert(dmMessage::NewSocket(RENDER_SOCKET_NAME, &context->m_Socket) == dmMessage::RESULT_OK);

        return context;
    }

    Result DeleteRenderContext(HRenderContext render_context)
    {
        if (render_context == 0x0) return RESULT_INVALID_CONTEXT;

        FinalizeRenderScriptContext(render_context->m_RenderScriptContext);
        FinalizeDebugRenderer(render_context);
        FinalizeTextContext(render_context);
        dmMessage::DeleteSocket(render_context->m_Socket);
        delete render_context;

        return RESULT_OK;
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

        context->m_TextContext.m_RenderObjectIndex = 0;
        context->m_TextContext.m_VertexIndex = 0;

        return RESULT_OK;
    }

    Result GenerateKey(HRenderContext render_context, const Matrix4& view_proj)
    {
        DM_PROFILE(Render, "GenerateKey");

        if (render_context == 0x0)
            return RESULT_INVALID_CONTEXT;

        // start by calculating depth (z of object in clip space) as part of the key
        for (uint32_t i = 0; i < render_context->m_RenderObjects.Size(); ++i)
        {
            RenderObject* ro = render_context->m_RenderObjects[i];
            Vector4 pos_clip_space(ro->m_WorldTransform.getTranslation(), 1.0f);
            pos_clip_space = view_proj * pos_clip_space;
            float depth = 1.0f - pos_clip_space.getZ()/pos_clip_space.getW();
            ro->m_RenderKey.m_Depth = *((uint64_t*)&depth);
        }

        return RESULT_OK;
    }

    static int SortFunction(const void *a, const void* b)
    {
        uintptr_t __a = *(uintptr_t*)a;
        uintptr_t __b = *(uintptr_t*)b;
        RenderObject* _a = (RenderObject*)(__a);
        RenderObject* _b = (RenderObject*)(__b);
        int64_t diff = _a->m_RenderKey.m_Key - _b->m_RenderKey.m_Key;

        return diff;
    }

    Result Draw(HRenderContext render_context, Predicate* predicate)
    {
        if (render_context == 0x0)
            return RESULT_INVALID_CONTEXT;

        uint32_t tag_mask = 0;
        if (predicate != 0x0)
            tag_mask = ConvertMaterialTagsToMask(&predicate->m_Tags[0], predicate->m_TagCount);

        dmGraphics::HContext context = dmRender::GetGraphicsContext(render_context);

        GenerateKey(render_context, GetViewProjectionMatrix(render_context));
        if (render_context->m_RenderObjects.Size() > 0)
        {
            qsort( &render_context->m_RenderObjects.Front(),
                   render_context->m_RenderObjects.Size(),
                   sizeof(RenderObject*),
                   SortFunction);
        }

        for (uint32_t i = 0; i < render_context->m_RenderObjects.Size(); ++i)
        {
            RenderObject* ro = render_context->m_RenderObjects[i];

            if (ro->m_VertexCount > 0 && (GetMaterialTagMask(ro->m_Material) & tag_mask) == tag_mask)
            {
                HMaterial material = ro->m_Material;
                if (render_context->m_Material)
                    material = render_context->m_Material;

                dmGraphics::EnableFragmentProgram(context, GetMaterialFragmentProgram(material));
                dmGraphics::EnableVertexProgram(context, GetMaterialVertexProgram(material));

                uint32_t material_vertex_mask = GetMaterialVertexConstantMask(material);
                uint32_t material_fragment_mask = GetMaterialFragmentConstantMask(material);
                for (uint32_t j = 0; j < MAX_CONSTANT_COUNT; ++j)
                {
                    uint32_t mask = 1 << j;
                    if (ro->m_VertexConstantMask & mask)
                    {
                        dmGraphics::SetVertexConstantBlock(context, &ro->m_VertexConstants[j], j, 1);
                    }
                    else if (material_vertex_mask & mask)
                    {
                        switch (GetMaterialVertexProgramConstantType(material, j))
                        {
                            case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER:
                            {
                                Vector4 constant = GetMaterialVertexProgramConstant(material, j);
                                dmGraphics::SetVertexConstantBlock(context, &constant, j, 1);
                                break;
                            }
                            case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_VIEWPROJ:
                            {
                                dmGraphics::SetVertexConstantBlock(context, (Vector4*)&render_context->m_ViewProj, j, 4);
                                break;
                            }
                            case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_WORLD:
                            {
                                dmGraphics::SetVertexConstantBlock(context, (Vector4*)&ro->m_WorldTransform, j, 4);
                                break;
                            }
                            case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_TEXTURE:
                            {
                                dmGraphics::SetVertexConstantBlock(context, (Vector4*)&ro->m_TextureTransform, j, 4);
                                break;
                            }
                        }
                    }
                    if (ro->m_FragmentConstantMask & (1 << j))
                    {
                        dmGraphics::SetFragmentConstant(context, &ro->m_FragmentConstants[j], j);
                    }
                    else if (material_fragment_mask & mask)
                    {
                        switch (GetMaterialFragmentProgramConstantType(material, j))
                        {
                            case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER:
                            {
                                Vector4 constant = GetMaterialFragmentProgramConstant(material, j);
                                dmGraphics::SetFragmentConstantBlock(context, &constant, j, 1);
                                break;
                            }
                            case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_VIEWPROJ:
                            {
                                dmGraphics::SetFragmentConstantBlock(context, (Vector4*)&render_context->m_ViewProj, j, 4);
                                break;
                            }
                            case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_WORLD:
                            {
                                dmGraphics::SetFragmentConstantBlock(context, (Vector4*)&ro->m_WorldTransform, j, 4);
                                break;
                            }
                            case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_TEXTURE:
                            {
                                dmGraphics::SetFragmentConstantBlock(context, (Vector4*)&ro->m_TextureTransform, j, 4);
                                break;
                            }
                        }
                    }
                    if (render_context->m_VertexConstantMask & mask)
                    {
                        dmGraphics::SetVertexConstantBlock(context, &render_context->m_VertexConstants[j], j, 1);
                    }
                    if (render_context->m_FragmentConstantMask & mask)
                    {
                        dmGraphics::SetFragmentConstant(context, &render_context->m_FragmentConstants[j], j);
                    }
                }

                if (ro->m_SetBlendFactors)
                    dmGraphics::SetBlendFunc(context, ro->m_SourceBlendFactor, ro->m_DestinationBlendFactor);

                for (uint32_t i = 0; i < RenderObject::MAX_TEXTURE_COUNT; ++i)
                {
                    dmGraphics::HTexture texture = ro->m_Textures[i];
                    if (render_context->m_Textures[i])
                        texture = render_context->m_Textures[i];
                    if (texture)
                        dmGraphics::EnableTexture(context, i, texture);
                }

                dmGraphics::EnableVertexDeclaration(context, ro->m_VertexDeclaration, ro->m_VertexBuffer);

                if (ro->m_IndexBuffer)
                    dmGraphics::DrawRangeElements(context, ro->m_PrimitiveType, ro->m_VertexStart, ro->m_VertexCount, ro->m_IndexType, ro->m_IndexBuffer);
                else
                    dmGraphics::Draw(context, ro->m_PrimitiveType, ro->m_VertexStart, ro->m_VertexCount);

                dmGraphics::DisableVertexDeclaration(context, ro->m_VertexDeclaration);

                for (uint32_t i = 0; i < RenderObject::MAX_TEXTURE_COUNT; ++i)
                {
                    dmGraphics::HTexture texture = ro->m_Textures[i];
                    if (render_context->m_Textures[i])
                        texture = render_context->m_Textures[i];
                    if (texture)
                        dmGraphics::DisableTexture(context, i);
                }

                for (uint32_t j = 0; j < MAX_CONSTANT_COUNT; ++j)
                {
                    uint32_t mask = 1 << j;
                    if (render_context->m_VertexConstantMask & mask)
                    {
                        uint32_t register_count = 0;
                        if (ro->m_VertexConstantMask & mask)
                        {
                            register_count = 1;
                        }
                        else if (material_vertex_mask & mask)
                        {
                            switch (GetMaterialVertexProgramConstantType(material, j))
                            {
                                case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER:
                                    register_count = 1;
                                    break;
                                case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_VIEWPROJ:
                                case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_WORLD:
                                case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_TEXTURE:
                                    register_count = 4;
                                    break;
                                default:
                                    break;
                            }
                        }
                        for (uint32_t k = 0; k < register_count; ++k)
                            dmGraphics::SetVertexConstantBlock(context, (Vector4*)&render_context->m_VertexConstants[j + k], j + k, 1);
                    }
                    if (render_context->m_FragmentConstantMask & mask)
                    {
                        uint32_t register_count = 0;
                        if (ro->m_FragmentConstantMask & (1 << j))
                            register_count = 1;
                        else if (material_fragment_mask & mask)
                        {
                            switch (GetMaterialFragmentProgramConstantType(material, j))
                            {
                                case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER:
                                    register_count = 1;
                                    break;
                                case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_VIEWPROJ:
                                case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_WORLD:
                                case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_TEXTURE:
                                    register_count = 4;
                                    break;
                            }
                        }
                        for (uint32_t k = 0; k < register_count; ++k)
                            dmGraphics::SetFragmentConstant(context, &render_context->m_FragmentConstants[j + k], j + k);
                    }
                }
            }
        }
        return RESULT_OK;
    }

    Result DrawDebug3d(HRenderContext context)
    {
        return Draw(context, &context->m_DebugRenderer.m_3dPredicate);
    }

    Result DrawDebug2d(HRenderContext context)
    {
        return Draw(context, &context->m_DebugRenderer.m_2dPredicate);
    }

    void EnableVertexConstant(HRenderContext context, uint32_t reg, const Vector4& value)
    {
        assert(context);
        if (reg < MAX_CONSTANT_COUNT)
        {
            context->m_VertexConstants[reg] = value;
            context->m_VertexConstantMask |= 1 << reg;
        }
        else
        {
            dmLogWarning("Illegal register (%d) supplied as vertex constant.", reg);
        }
    }

    void DisableVertexConstant(HRenderContext context, uint32_t reg)
    {
        assert(context);
        if (reg < MAX_CONSTANT_COUNT)
            context->m_VertexConstantMask &= ~(1 << reg);
        else
            dmLogWarning("Illegal register (%d) supplied as vertex constant.", reg);
    }

    void EnableFragmentConstant(HRenderContext context, uint32_t reg, const Vector4& value)
    {
        assert(context);
        if (reg < MAX_CONSTANT_COUNT)
        {
            context->m_FragmentConstants[reg] = value;
            context->m_FragmentConstantMask |= 1 << reg;
        }
        else
        {
            dmLogWarning("Illegal register (%d) supplied as fragment constant.", reg);
        }
    }

    void DisableFragmentConstant(HRenderContext context, uint32_t reg)
    {
        assert(context);
        if (reg < MAX_CONSTANT_COUNT)
            context->m_FragmentConstantMask &= ~(1 << reg);
        else
            dmLogWarning("Illegal register (%d) supplied as fragment constant.", reg);
    }

    void EnableRenderObjectVertexConstant(RenderObject* ro, uint32_t reg, const Vector4& value)
    {
        assert(ro);
        if (reg < MAX_CONSTANT_COUNT)
        {
            ro->m_VertexConstants[reg] = value;
            ro->m_VertexConstantMask |= 1 << reg;
        }
        else
        {
            dmLogWarning("Illegal register (%d) supplied as vertex constant.", reg);
        }
    }

    void DisableRenderObjectVertexConstant(RenderObject* ro, uint32_t reg)
    {
        assert(ro);
        if (reg < MAX_CONSTANT_COUNT)
            ro->m_VertexConstantMask &= ~(1 << reg);
        else
            dmLogWarning("Illegal register (%d) supplied as vertex constant.", reg);
    }

    void EnableRenderObjectFragmentConstant(RenderObject* ro, uint32_t reg, const Vector4& value)
    {
        assert(ro);
        if (reg < MAX_CONSTANT_COUNT)
        {
            ro->m_FragmentConstants[reg] = value;
            ro->m_FragmentConstantMask |= 1 << reg;
        }
        else
        {
            dmLogWarning("Illegal register (%d) supplied as fragment constant.", reg);
        }
    }

    void DisableRenderObjectFragmentConstant(RenderObject* ro, uint32_t reg)
    {
        assert(ro);
        if (reg < MAX_CONSTANT_COUNT)
            ro->m_FragmentConstantMask &= ~(1 << reg);
        else
            dmLogWarning("Illegal register (%d) supplied as fragment constant.", reg);
    }
}
