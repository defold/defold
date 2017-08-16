#include <stdint.h>
#include <gtest/gtest.h>

#include <dlib/hash.h>
#include <dlib/math.h>
#include <script/script.h>

#include "render/render.h"
#include "render/render_private.h"

using namespace Vectormath::Aos;
namespace dmGraphics
{
    extern const Vector4& GetConstantV4Ptr(dmGraphics::HContext context, int base_register);
}

TEST(dmMaterialTest, TestTags)
{
    dmGraphics::HContext context = dmGraphics::NewContext(dmGraphics::ContextParams());
    dmRender::RenderContextParams params;
    params.m_ScriptContext = dmScript::NewContext(0, 0, true);
    dmRender::HRenderContext render_context = dmRender::NewRenderContext(context, params);

    dmGraphics::HVertexProgram vp = dmGraphics::NewVertexProgram(context, "foo", 3);
    dmGraphics::HFragmentProgram fp = dmGraphics::NewFragmentProgram(context, "foo", 3);

    dmRender::HMaterial material = dmRender::NewMaterial(render_context, vp, fp);

    dmhash_t tags[] = {dmHashString64("tag1"), dmHashString64("tag2")};
    uint32_t mask = dmRender::ConvertMaterialTagsToMask(tags, 1);
    ASSERT_NE(0u, mask);
    dmRender::AddMaterialTag(material, tags[0]);
    dmRender::AddMaterialTag(material, tags[1]);
    mask = dmRender::ConvertMaterialTagsToMask(tags, 2);
    ASSERT_EQ(mask, dmRender::GetMaterialTagMask(material));

    dmGraphics::DeleteVertexProgram(vp);
    dmGraphics::DeleteFragmentProgram(fp);

    dmRender::DeleteMaterial(render_context, material);

    dmRender::DeleteRenderContext(render_context, 0);
    dmGraphics::DeleteContext(context);
    dmScript::DeleteContext(params.m_ScriptContext);
}

TEST(dmMaterialTest, TestMaterialConstants)
{
    dmGraphics::HContext context = dmGraphics::NewContext(dmGraphics::ContextParams());
    dmRender::RenderContextParams params;
    params.m_ScriptContext = dmScript::NewContext(0, 0, true);
    dmRender::HRenderContext render_context = dmRender::NewRenderContext(context, params);

    // create default material
    dmGraphics::HVertexProgram vp = dmGraphics::NewVertexProgram(context, "uniform vec4 tint;\n", 19);
    dmGraphics::HFragmentProgram fp = dmGraphics::NewFragmentProgram(context, "foo", 3);
    dmRender::HMaterial material = dmRender::NewMaterial(render_context, vp, fp);

    // renderobject default setup
    dmRender::RenderObject ro;
    ro.m_Material = material;
    dmRender::EnableRenderObjectConstant(&ro, dmHashString64("tint"), Vector4(1.0f, 0.0f, 0.0f, 0.0f));

    // test setting constant
    dmGraphics::HProgram program = dmRender::GetMaterialProgram(material);
    dmGraphics::EnableProgram(context, program);
    uint32_t tint_loc = dmGraphics::GetUniformLocation(program, "tint");
    ASSERT_EQ(0, tint_loc);
    dmRender::ApplyRenderObjectConstants(render_context, 0, &ro);
    const Vector4& v = dmGraphics::GetConstantV4Ptr(context, tint_loc);
    ASSERT_EQ(1.0f, v.getX());
    ASSERT_EQ(0.0f, v.getY());
    ASSERT_EQ(0.0f, v.getZ());
    ASSERT_EQ(0.0f, v.getW());

    dmGraphics::DisableProgram(context);
    dmGraphics::DeleteVertexProgram(vp);
    dmGraphics::DeleteFragmentProgram(fp);
    dmRender::DeleteMaterial(render_context, material);
    dmRender::DeleteRenderContext(render_context, 0);
    dmGraphics::DeleteContext(context);
    dmScript::DeleteContext(params.m_ScriptContext);
}

TEST(dmMaterialTest, TestMaterialConstantsOverride)
{
    dmGraphics::HContext context = dmGraphics::NewContext(dmGraphics::ContextParams());
    dmRender::RenderContextParams params;
    params.m_ScriptContext = dmScript::NewContext(0, 0, true);
    dmRender::HRenderContext render_context = dmRender::NewRenderContext(context, params);

    // create default material
    dmGraphics::HVertexProgram vp = dmGraphics::NewVertexProgram(context, "uniform vec4 tint;\n", 19);
    dmGraphics::HFragmentProgram fp = dmGraphics::NewFragmentProgram(context, "foo", 3);
    dmRender::HMaterial material = dmRender::NewMaterial(render_context, vp, fp);
    dmGraphics::HProgram program = dmRender::GetMaterialProgram(material);

    // create override material which contains tint, but at a different location
    dmGraphics::HVertexProgram vp_ovr = dmGraphics::NewVertexProgram(context, "uniform vec4 dummy;\nuniform vec4 tint;\n", 42);
    dmGraphics::HFragmentProgram fp_ovr = dmGraphics::NewFragmentProgram(context, "foo", 3);
    dmRender::HMaterial material_ovr = dmRender::NewMaterial(render_context, vp_ovr, fp_ovr);
    dmGraphics::HProgram program_ovr = dmRender::GetMaterialProgram(material_ovr);

    // renderobject default setup
    dmRender::RenderObject ro;
    ro.m_Material = material;
    dmRender::EnableRenderObjectConstant(&ro, dmHashString64("tint"), Vector4(1.0f, 0.0f, 0.0f, 0.0f));

    // using the null graphics device, constant locations are assumed to be in declaration order.
    // test setting constant, no override material
    uint32_t tint_loc = dmGraphics::GetUniformLocation(program, "tint");
    ASSERT_EQ(0, tint_loc);
    dmGraphics::EnableProgram(context, program);
    dmRender::ApplyRenderObjectConstants(render_context, 0, &ro);
    const Vector4& v = dmGraphics::GetConstantV4Ptr(context, tint_loc);
    ASSERT_EQ(1.0f, v.getX());
    ASSERT_EQ(0.0f, v.getY());
    ASSERT_EQ(0.0f, v.getZ());
    ASSERT_EQ(0.0f, v.getW());

    // test setting constant, override material
    dmRender::EnableRenderObjectConstant(&ro, dmHashString64("tint"), Vector4(2.0f, 1.0f, 1.0f, 1.0f));
    ASSERT_EQ(0,  ro.m_Constants[0].m_Location);
    ASSERT_EQ(-1, ro.m_Constants[1].m_Location);
    ASSERT_EQ(-1, ro.m_Constants[2].m_Location);
    ASSERT_EQ(-1, ro.m_Constants[3].m_Location);
    uint32_t tint_loc_ovr = dmGraphics::GetUniformLocation(program_ovr, "tint");
    ASSERT_EQ(1, tint_loc_ovr);
    dmGraphics::EnableProgram(context, program_ovr);
    dmRender::ApplyRenderObjectConstants(render_context, material_ovr, &ro);
    const Vector4& v_ovr = dmGraphics::GetConstantV4Ptr(context, tint_loc_ovr);
    ASSERT_EQ(2.0f, v_ovr.getX());
    ASSERT_EQ(1.0f, v_ovr.getY());
    ASSERT_EQ(1.0f, v_ovr.getZ());
    ASSERT_EQ(1.0f, v_ovr.getW());

    dmGraphics::DisableProgram(context);
    dmGraphics::DeleteVertexProgram(vp_ovr);
    dmGraphics::DeleteFragmentProgram(fp_ovr);
    dmRender::DeleteMaterial(render_context, material_ovr);
    dmGraphics::DeleteVertexProgram(vp);
    dmGraphics::DeleteFragmentProgram(fp);
    dmRender::DeleteMaterial(render_context, material);
    dmRender::DeleteRenderContext(render_context, 0);
    dmGraphics::DeleteContext(context);
    dmScript::DeleteContext(params.m_ScriptContext);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
