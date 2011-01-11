#include <stdint.h>
#include <gtest/gtest.h>

#include <dlib/hash.h>

#include "render/material.h"

TEST(dmMaterialTest, TestCreateDestroy)
{
    dmRender::HMaterial material = dmRender::NewMaterial();
    dmRender::DeleteMaterial(material);
}

TEST(dmMaterialTest, TestPrograms)
{
    dmRender::HMaterial material = dmRender::NewMaterial();

    dmGraphics::HContext context = dmGraphics::NewContext();

    void* vp_source = new char[1024];
    dmGraphics::HVertexProgram vp = dmGraphics::NewVertexProgram(context, vp_source, 1024);
    ASSERT_NE(0u, vp);
    void* fp_source = new char[1024];
    dmGraphics::HFragmentProgram fp = dmGraphics::NewFragmentProgram(context, fp_source, 1024);
    ASSERT_NE(0u, fp);

    dmRender::SetMaterialVertexProgram(material, vp);
    ASSERT_EQ(vp, dmRender::GetMaterialVertexProgram(material));
    dmRender::SetMaterialFragmentProgram(material, fp);
    ASSERT_EQ(fp, dmRender::GetMaterialFragmentProgram(material));

    Vectormath::Aos::Vector4 user_value(1.0f, 2.0f, 3.0f, 4.0f);
    Vectormath::Aos::Vector4 user_value2;
    uint32_t mask = 3;

    dmRender::SetMaterialVertexProgramConstantType(material, 0, dmRenderDDF::MaterialDesc::CONSTANT_TYPE_VIEWPROJ);
    ASSERT_EQ(dmRenderDDF::MaterialDesc::CONSTANT_TYPE_VIEWPROJ, dmRender::GetMaterialVertexProgramConstantType(material, 0));
    dmRender::SetMaterialVertexProgramConstantType(material, 1, dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER);
    dmRender::SetMaterialVertexProgramConstant(material, 1, user_value);
    ASSERT_EQ(dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER, dmRender::GetMaterialVertexProgramConstantType(material, 1));
    user_value2 = dmRender::GetMaterialVertexProgramConstant(material, 1);
    ASSERT_EQ(user_value.getX(), user_value2.getX());
    ASSERT_EQ(user_value.getY(), user_value2.getY());
    ASSERT_EQ(user_value.getZ(), user_value2.getZ());
    ASSERT_EQ(user_value.getW(), user_value2.getW());
    dmRender::SetMaterialVertexConstantMask(material, mask);
    ASSERT_EQ(mask, dmRender::GetMaterialVertexConstantMask(material));

    dmRender::SetMaterialFragmentProgramConstantType(material, 0, dmRenderDDF::MaterialDesc::CONSTANT_TYPE_VIEWPROJ);
    ASSERT_EQ(dmRenderDDF::MaterialDesc::CONSTANT_TYPE_VIEWPROJ, dmRender::GetMaterialFragmentProgramConstantType(material, 0));
    dmRender::SetMaterialFragmentProgramConstantType(material, 1, dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER);
    dmRender::SetMaterialFragmentProgramConstant(material, 1, user_value);
    ASSERT_EQ(dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER, dmRender::GetMaterialFragmentProgramConstantType(material, 1));
    user_value2 = dmRender::GetMaterialFragmentProgramConstant(material, 1);
    ASSERT_EQ(user_value.getX(), user_value2.getX());
    ASSERT_EQ(user_value.getY(), user_value2.getY());
    ASSERT_EQ(user_value.getZ(), user_value2.getZ());
    ASSERT_EQ(user_value.getW(), user_value2.getW());
    dmRender::SetMaterialFragmentConstantMask(material, mask);
    ASSERT_EQ(mask, dmRender::GetMaterialFragmentConstantMask(material));

    dmGraphics::DeleteVertexProgram(vp);
    delete [] (char*)vp_source;
    dmGraphics::DeleteFragmentProgram(fp);
    delete [] (char*)fp_source;

    dmRender::DeleteMaterial(material);

    dmGraphics::DeleteContext(context);
}

TEST(dmMaterialTest, TestTags)
{
    dmRender::HMaterial material = dmRender::NewMaterial();

    uint32_t tags[] = {dmHashString32("tag1"), dmHashString32("tag2")};
    uint32_t mask = dmRender::ConvertMaterialTagsToMask(tags, 1);
    ASSERT_NE(0u, mask);
    dmRender::AddMaterialTag(material, tags[0]);
    dmRender::AddMaterialTag(material, tags[1]);
    mask = dmRender::ConvertMaterialTagsToMask(tags, 2);
    ASSERT_EQ(mask, dmRender::GetMaterialTagMask(material));

    dmRender::DeleteMaterial(material);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
