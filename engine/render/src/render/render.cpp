#include <assert.h>
#include <string.h>
#include <algorithm>

#include <dlib/hash.h>
#include <dlib/hashtable.h>
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

        InitializeDebugRenderer(context, params.m_MaxDebugVertexCount, params.m_VertexProgramData, params.m_VertexProgramDataSize, params.m_FragmentProgramData, params.m_FragmentProgramDataSize);

        memset(context->m_Textures, 0, sizeof(dmGraphics::HTexture) * RenderObject::MAX_TEXTURE_COUNT);

        InitializeTextContext(context, params.m_MaxCharacters);

        context->m_OutOfResources = 0;

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
        context->m_TextContext.m_RenderObjectIndex = 0;
        context->m_TextContext.m_VertexIndex = 0;
        context->m_TextContext.m_TextBuffer.SetSize(0);
        context->m_TextContext.m_Batches.Clear();
        context->m_TextContext.m_TextEntries.SetSize(0);

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
            if (ro->m_CalculateDepthKey)
            {
                Vector4 pos_clip_space(ro->m_WorldTransform.getTranslation(), 1.0f);
                pos_clip_space = view_proj * pos_clip_space;
                float depth = 1.0f - pos_clip_space.getZ() / pos_clip_space.getW();
                ro->m_RenderKey.m_Depth = *((uint64_t*) &depth);
            }
        }

        return RESULT_OK;
    }

    static bool SortPred(const RenderObject* a, const RenderObject* b)
    {
        return a->m_RenderKey.m_Key < b->m_RenderKey.m_Key;
    }

    static void ApplyRenderObjectConstants(HRenderContext render_context, const RenderObject* ro)
    {
        dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(render_context);

        for (uint32_t i = 0; i < RenderObject::MAX_CONSTANT_COUNT; ++i)
        {
            const Constant* c = &ro->m_Constants[i];
            if (c->m_Location != -1)
            {
                dmGraphics::SetConstantV4(graphics_context, &c->m_Value, c->m_Location);
            }
        }
    }

    Result Draw(HRenderContext render_context, Predicate* predicate, HNamedConstantBuffer constant_buffer)
    {
        if (render_context == 0x0)
            return RESULT_INVALID_CONTEXT;

        uint32_t tag_mask = 0;
        if (predicate != 0x0)
            tag_mask = ConvertMaterialTagsToMask(&predicate->m_Tags[0], predicate->m_TagCount);

        dmGraphics::HContext context = dmRender::GetGraphicsContext(render_context);
        GenerateKey(render_context, GetViewProjectionMatrix(render_context));


        // TODO: Move to "BeginFrame()" or similar? See case 2261
        FlushTexts(render_context);
        FlushDebug(render_context);

        // TODO: Move to "BeginFrame()" or similar? See case 2261
        std::sort(render_context->m_RenderObjects.Begin(),
                  render_context->m_RenderObjects.End(),
                  &SortPred);

        for (uint32_t i = 0; i < render_context->m_RenderObjects.Size(); ++i)
        {
            RenderObject* ro = render_context->m_RenderObjects[i];

            if (ro->m_VertexCount > 0 && (GetMaterialTagMask(ro->m_Material) & tag_mask) == tag_mask)
            {
                HMaterial material = ro->m_Material;
                if (render_context->m_Material)
                    material = render_context->m_Material;

                dmGraphics::EnableProgram(context, GetMaterialProgram(material));
                ApplyMaterialConstants(render_context, material, ro);
                ApplyMaterialSamplers(render_context, material);
                ApplyRenderObjectConstants(render_context, ro);

                if (constant_buffer)
                    ApplyNamedConstantBuffer(render_context, material, constant_buffer);

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
        return Draw(context, &context->m_DebugRenderer.m_3dPredicate, 0);
    }

    Result DrawDebug2d(HRenderContext context)
    {
        return Draw(context, &context->m_DebugRenderer.m_2dPredicate, 0);
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
                if (location != -1)
                {
                    c->m_Value = value;
                    c->m_NameHash = name_hash;
                    c->m_Type = dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER;
                    c->m_Location = location;
                    return;
                }
            }
        }

        dmLogError("Out of per object constant slots, max %d, when setting constant %s ", RenderObject::MAX_CONSTANT_COUNT, (const char*) dmHashReverse64(name_hash, 0));
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
