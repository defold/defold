#include <assert.h>

#include <dlib/hash.h>
#include <dlib/profile.h>

#include <ddf/ddf.h>

#include <graphics/graphics_device.h>

#include "render_private.h"
#include "debug_renderer.h"
#include "font_renderer.h"
#include "render/mesh_ddf.h"

#include "model/model.h"

namespace dmRender
{
    RenderType::RenderType()
    : m_BeginCallback(0x0)
    , m_DrawCallback(0x0)
    , m_EndCallback(0x0)
    , m_UserContext(0x0)
    {

    }

    RenderContextParams::RenderContextParams()
    : m_MaxRenderTypes(0)
    , m_MaxInstances(0)
    , m_SetObjectModel(0x0)
    , m_VertexProgramData(0x0)
    , m_VertexProgramDataSize(0)
    , m_FragmentProgramData(0x0)
    , m_FragmentProgramDataSize(0)
    , m_MaxCharacters(0)
    {

    }

    HRenderContext NewRenderContext(const RenderContextParams& params)
    {
        RenderContext* context = new RenderContext;

        context->m_RenderTypes.SetCapacity(params.m_MaxRenderTypes);

        context->m_RenderObjects.SetCapacity(params.m_MaxInstances);
        context->m_RenderObjects.SetSize(0);

        context->m_SetObjectModel = params.m_SetObjectModel;
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

    Result RegisterRenderType(HRenderContext render_context, RenderType render_type, HRenderType* out_render_type)
    {
        if (render_context == 0x0)
            return RESULT_INVALID_CONTEXT;
        if (render_context->m_RenderTypes.Full())
            return RESULT_INVALID_CONTEXT;
        render_context->m_RenderTypes.Push(render_type);
        *out_render_type = render_context->m_RenderTypes.Size() - 1;
        return RESULT_OK;
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

    Result AddToRender(HRenderContext context, HRenderObject ro)
    {
        if (context == 0x0) return RESULT_INVALID_CONTEXT;

        if (context->m_SetObjectModel && ro->m_VisualObject)
            context->m_SetObjectModel(ro->m_VisualObject, &ro->m_Rot, &ro->m_Pos);
        context->m_RenderObjects.Push(ro);

        return RESULT_OK;
    }

    Result ClearRenderObjects(HRenderContext context)
    {
        context->m_RenderObjects.SetSize(0);
        ClearDebugRenderObjects(context);

        context->m_TextContext.m_RenderObjectIndex = 0;
        context->m_TextContext.m_Vertices.SetSize(0);

        return RESULT_OK;
    }

    Result Draw(HRenderContext render_context, Predicate* predicate)
    {
        if (render_context == 0x0)
            return RESULT_INVALID_CONTEXT;
        uint32_t tag_mask = 0;
        if (predicate != 0x0)
            tag_mask = dmGraphics::ConvertMaterialTagsToMask(&predicate->m_Tags[0], predicate->m_TagCount);
        uint32_t type = ~0u;
        for (uint32_t i = 0; i < render_context->m_RenderObjects.Size(); ++i)
        {
            HRenderObject ro = render_context->m_RenderObjects[i];
            if ((dmGraphics::GetMaterialTagMask(ro->m_Material) & tag_mask) == tag_mask)
            {
                dmGraphics::HContext context = dmRender::GetGraphicsContext(render_context);
                dmGraphics::SetFragmentProgram(context, dmGraphics::GetMaterialFragmentProgram(dmRender::GetMaterial(ro)));
                dmGraphics::SetVertexProgram(context, dmGraphics::GetMaterialVertexProgram(dmRender::GetMaterial(ro)));

                void* user_context = render_context->m_RenderTypes[ro->m_Type].m_UserContext;
                // check if we need to change render type and run its setup func
                if (type != ro->m_Type)
                {
                    if (render_context->m_RenderTypes[ro->m_Type].m_BeginCallback)
                        render_context->m_RenderTypes[ro->m_Type].m_BeginCallback(render_context, user_context);
                    type = ro->m_Type;
                }

                // dispatch
                if (render_context->m_RenderTypes[ro->m_Type].m_DrawCallback)
                    render_context->m_RenderTypes[ro->m_Type].m_DrawCallback(render_context, user_context, ro, 1);

                if (i == render_context->m_RenderObjects.Size() - 1 || type != render_context->m_RenderObjects[i+1]->m_Type)
                {
                    if (render_context->m_RenderTypes[ro->m_Type].m_EndCallback)
                        render_context->m_RenderTypes[ro->m_Type].m_EndCallback(render_context, user_context);
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

    HRenderObject NewRenderObject(uint32_t type, dmGraphics::HMaterial material, void* visual_object)
    {
        RenderObject* ro = new RenderObject;
        ro->m_Type = type;
        ro->m_VisualObject = visual_object;

        SetMaterial(ro, material);

        return ro;
    }

    void DeleteRenderObject(HRenderObject ro)
    {
        delete ro;
    }

    void* GetUserData(HRenderObject ro)
    {
        return ro->m_UserData;
    }

    void SetUserData(HRenderObject ro, void* user_data)
    {
        ro->m_UserData = user_data;
    }

    dmGraphics::HMaterial GetMaterial(HRenderObject ro)
    {
        return ro->m_Material;
    }

    void SetMaterial(HRenderObject ro, dmGraphics::HMaterial material)
    {
        ro->m_Material = material;
        if (material != 0x0)
        {
            uint32_t reg;
            reg = DIFFUSE_COLOR;
            ro->m_Colour[reg] = dmGraphics::GetMaterialFragmentProgramConstant(material, reg);
            reg = EMISSIVE_COLOR;
            ro->m_Colour[reg] = dmGraphics::GetMaterialFragmentProgramConstant(material, reg);
            reg = SPECULAR_COLOR;
            ro->m_Colour[reg] = dmGraphics::GetMaterialFragmentProgramConstant(material, reg);
        }
    }

    Point3 GetPosition(HRenderObject ro)
    {
        return ro->m_Pos;
    }

    void SetPosition(HRenderObject ro, Point3 pos)
    {
        ro->m_Pos = pos;
    }

    Quat GetRotation(HRenderObject ro)
    {
        return ro->m_Rot;
    }

    void SetRotation(HRenderObject ro, Quat rot)
    {
        ro->m_Rot = rot;
    }

    Vector4 GetColor(HRenderObject ro, ColorType color_type)
    {
        return ro->m_Colour[color_type];
    }

    void SetColor(HRenderObject ro, Vector4 color, ColorType color_type)
    {
        ro->m_Colour[color_type] = color;
    }

    Vector3 GetSize(HRenderObject ro)
    {
        return ro->m_Size;
    }

    void SetSize(HRenderObject ro, Vector3 size)
    {
        ro->m_Size = size;
    }
}
