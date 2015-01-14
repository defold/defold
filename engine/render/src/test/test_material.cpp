#include <stdint.h>
#include <gtest/gtest.h>

#include <dlib/hash.h>
#include <script/script.h>

#include "render/render.h"

TEST(dmMaterialTest, TestTags)
{
    dmGraphics::HContext context = dmGraphics::NewContext(dmGraphics::ContextParams());
    dmRender::RenderContextParams params;
    params.m_ScriptContext = dmScript::NewContext(0, 0);
    dmRender::HRenderContext render_context = dmRender::NewRenderContext(context, params);

    dmGraphics::HVertexProgram vp = dmGraphics::NewVertexProgram(context, "foo", 3);
    dmGraphics::HFragmentProgram fp = dmGraphics::NewFragmentProgram(context, "foo", 3);

    dmRender::HMaterial material = dmRender::NewMaterial(render_context, vp, fp);

    uint32_t tags[] = {dmHashString32("tag1"), dmHashString32("tag2")};
    uint32_t mask = dmRender::ConvertMaterialTagsToMask(tags, 1);
    ASSERT_NE(0u, mask);
    dmRender::AddMaterialTag(material, tags[0]);
    dmRender::AddMaterialTag(material, tags[1]);
    mask = dmRender::ConvertMaterialTagsToMask(tags, 2);
    ASSERT_EQ(mask, dmRender::GetMaterialTagMask(material));

    dmGraphics::DeleteVertexProgram(context, vp);
    dmGraphics::DeleteFragmentProgram(context, fp);

    dmRender::DeleteMaterial(render_context, material);

    dmGraphics::DeleteContext(context);
    dmRender::DeleteRenderContext(render_context, 0);
    dmScript::DeleteContext(params.m_ScriptContext);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
