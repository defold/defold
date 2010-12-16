#include <assert.h>
#include <string.h>

#include <dlib/hash.h>
#include <dlib/profile.h>

#include <ddf/ddf.h>

#include <graphics/graphics_device.h>

#include "render_private.h"
#include "debug_renderer.h"
#include "font_renderer.h"

namespace dmRender
{
    RenderObject::RenderObject()
    {
        memset(this, 0, sizeof(RenderObject));
        m_TextureTransform = Vectormath::Aos::Matrix4::identity();
    }

    RenderContextParams::RenderContextParams()
    : m_MaxRenderTypes(0)
    , m_MaxInstances(0)
    , m_MaxRenderTargets(0)
    , m_VertexProgramData(0x0)
    , m_VertexProgramDataSize(0)
    , m_FragmentProgramData(0x0)
    , m_FragmentProgramDataSize(0)
    , m_DisplayWidth(0)
    , m_DisplayHeight(0)
    , m_MaxCharacters(0)
    {

    }

    HRenderContext NewRenderContext(const RenderContextParams& params)
    {
        RenderContext* context = new RenderContext;

        context->m_RenderTargets.SetCapacity(params.m_MaxRenderTargets);

        context->m_RenderObjects.SetCapacity(params.m_MaxInstances);
        context->m_RenderObjects.SetSize(0);

        context->m_GFXContext = dmGraphics::GetContext();

        context->m_View = Vectormath::Aos::Matrix4::identity();
        context->m_Projection = Vectormath::Aos::Matrix4::identity();

        InitializeDebugRenderer(context, params.m_VertexProgramData, params.m_VertexProgramDataSize, params.m_FragmentProgramData, params.m_FragmentProgramDataSize);

        context->m_DisplayWidth = params.m_DisplayWidth;
        context->m_DisplayHeight = params.m_DisplayHeight;

        InitializeTextContext(context, params.m_MaxCharacters);
        return context;
    }

    Result DeleteRenderContext(HRenderContext render_context)
    {
        if (render_context == 0x0) return RESULT_INVALID_CONTEXT;

        FinalizeDebugRenderer(render_context);
        FinalizeTextContext(render_context);
        delete render_context;

        return RESULT_OK;
    }

    Result RegisterRenderTarget(HRenderContext render_context, dmGraphics::HRenderTarget rendertarget, uint32_t hash)
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

    dmGraphics::HRenderTarget GetRenderTarget(HRenderContext render_context, uint32_t hash)
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
        return render_context->m_GFXContext;
    }

    Matrix4* GetViewProjectionMatrix(HRenderContext render_context)
    {
        return &render_context->m_ViewProj;
    }

    void SetViewMatrix(HRenderContext render_context, const Vectormath::Aos::Matrix4& view)
    {
        render_context->m_View = view;
        render_context->m_ViewProj = render_context->m_Projection * view;
    }

    void SetProjectionMatrix(HRenderContext render_context, const Vectormath::Aos::Matrix4& projection)
    {
        render_context->m_Projection = projection;
        render_context->m_ViewProj = projection * render_context->m_View;
    }

    uint32_t GetDisplayWidth(HRenderContext render_context)
    {
        return render_context->m_DisplayWidth;
    }

    uint32_t GetDisplayHeight(HRenderContext render_context)
    {
        return render_context->m_DisplayHeight;
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

    Result Draw(HRenderContext render_context, Predicate* predicate)
    {
        if (render_context == 0x0)
            return RESULT_INVALID_CONTEXT;
        uint32_t tag_mask = 0;
        if (predicate != 0x0)
            tag_mask = ConvertMaterialTagsToMask(&predicate->m_Tags[0], predicate->m_TagCount);

        for (uint32_t i = 0; i < render_context->m_RenderObjects.Size(); ++i)
        {
            RenderObject* ro = render_context->m_RenderObjects[i];
            if (ro->m_VertexCount > 0 && (GetMaterialTagMask(ro->m_Material) & tag_mask) == tag_mask)
            {
                dmGraphics::HContext context = dmRender::GetGraphicsContext(render_context);
                dmGraphics::SetFragmentProgram(context, GetMaterialFragmentProgram(ro->m_Material));
                dmGraphics::SetVertexProgram(context, GetMaterialVertexProgram(ro->m_Material));

                uint32_t material_vertex_mask = GetMaterialVertexConstantMask(ro->m_Material);
                uint32_t material_fragment_mask = GetMaterialFragmentConstantMask(ro->m_Material);
                for (uint32_t j = 0; j < MAX_CONSTANT_COUNT; ++j)
                {
                    uint32_t mask = 1 << j;
                    if (ro->m_VertexConstantMask & mask)
                    {
                        dmGraphics::SetVertexConstantBlock(context, &ro->m_VertexConstants[j], j, 1);
                    }
                    else if (material_vertex_mask & mask)
                    {
                        switch (GetMaterialVertexProgramConstantType(ro->m_Material, j))
                        {
                            case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER:
                            {
                                Vector4 constant = GetMaterialVertexProgramConstant(ro->m_Material, j);
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
                        switch (GetMaterialFragmentProgramConstantType(ro->m_Material, j))
                        {
                            case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER:
                            {
                                Vector4 constant = GetMaterialFragmentProgramConstant(ro->m_Material, j);
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
                }

                if (ro->m_SetBlendFactors)
                    dmGraphics::SetBlendFunc(context, ro->m_SourceBlendFactor, ro->m_DestinationBlendFactor);

                if (ro->m_Texture)
                    dmGraphics::SetTexture(context, ro->m_Texture);

                dmGraphics::EnableVertexDeclaration(context, ro->m_VertexDeclaration, ro->m_VertexBuffer);

                if (ro->m_IndexBuffer)
                    dmGraphics::DrawRangeElements(context, ro->m_PrimitiveType, ro->m_VertexStart, ro->m_VertexCount, ro->m_IndexType, ro->m_IndexBuffer);
                else
                    dmGraphics::Draw(context, ro->m_PrimitiveType, ro->m_VertexStart, ro->m_VertexCount);

                dmGraphics::DisableVertexDeclaration(context, ro->m_VertexDeclaration);
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

    void SetVertexConstant(RenderObject* ro, uint32_t reg, const Vectormath::Aos::Vector4& value)
    {
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

    void ResetVertexConstant(RenderObject* ro, uint32_t reg)
    {
        if (reg < MAX_CONSTANT_COUNT)
            ro->m_VertexConstantMask &= ~(1 << reg);
        else
            dmLogWarning("Illegal register (%d) supplied as vertex constant.", reg);
    }

    void SetFragmentConstant(RenderObject* ro, uint32_t reg, const Vectormath::Aos::Vector4& value)
    {
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

    void ResetFragmentConstant(RenderObject* ro, uint32_t reg)
    {
        if (reg < MAX_CONSTANT_COUNT)
            ro->m_FragmentConstantMask &= ~(1 << reg);
        else
            dmLogWarning("Illegal register (%d) supplied as fragment constant.", reg);
    }
}
