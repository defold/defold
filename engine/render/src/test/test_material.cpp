// Copyright 2020-2024 The Defold Foundation
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

#include <stdint.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include <testmain/testmain.h>
#include <dlib/hash.h>
#include <dlib/math.h>
#include <script/script.h>

#include "render/render.h"
#include "render/render_private.h"

using namespace dmVMath;

namespace dmGraphics
{
    extern const Vector4& GetConstantV4Ptr(dmGraphics::HContext context, dmGraphics::HUniformLocation base_register);
}

class dmRenderMaterialTest : public jc_test_base_class
{
public:
    dmPlatform::HWindow           m_Window;
    dmGraphics::HContext          m_GraphicsContext;
    dmRender::HRenderContext      m_RenderContext;
    dmRender::RenderContextParams m_Params;

    virtual void SetUp()
    {
        dmGraphics::InstallAdapter();

        dmPlatform::WindowParams win_params = {};
        win_params.m_Width = 20;
        win_params.m_Height = 10;

        m_Window = dmPlatform::NewWindow();
        dmPlatform::OpenWindow(m_Window, win_params);

        dmGraphics::ContextParams graphics_context_params;
        graphics_context_params.m_Window = m_Window;

        m_GraphicsContext        = dmGraphics::NewContext(graphics_context_params);
        m_Params.m_ScriptContext = dmScript::NewContext(0, 0, true);
        m_Params.m_MaxCharacters = 256;
        m_Params.m_MaxBatches = 128;
        m_RenderContext          = dmRender::NewRenderContext(m_GraphicsContext, m_Params);
    }
    virtual void TearDown()
    {
        dmRender::DeleteRenderContext(m_RenderContext, 0);
        dmGraphics::DeleteContext(m_GraphicsContext);
        dmPlatform::CloseWindow(m_Window);
        dmPlatform::DeleteWindow(m_Window);
        dmScript::DeleteContext(m_Params.m_ScriptContext);
    }
};

static inline dmGraphics::ShaderDesc::Shader MakeDDFShader(const char* data, uint32_t count)
{
    dmGraphics::ShaderDesc::Shader ddf;
    memset(&ddf,0,sizeof(ddf));
    ddf.m_Source.m_Data  = (uint8_t*)data;
    ddf.m_Source.m_Count = count;
    return ddf;
}

TEST_F(dmRenderMaterialTest, TestTags)
{
    dmGraphics::ShaderDesc::Shader shader = MakeDDFShader("foo", 3);
    dmGraphics::HVertexProgram vp = dmGraphics::NewVertexProgram(m_GraphicsContext, &shader);
    dmGraphics::HFragmentProgram fp = dmGraphics::NewFragmentProgram(m_GraphicsContext, &shader);
    dmRender::HMaterial material = dmRender::NewMaterial(m_RenderContext, vp, fp);

    dmhash_t tags[] = {dmHashString64("tag1"), dmHashString64("tag2")};
    dmRender::SetMaterialTags(material, DM_ARRAY_SIZE(tags), tags);
    ASSERT_EQ(dmHashBuffer32(tags, DM_ARRAY_SIZE(tags)*sizeof(tags[0])), dmRender::GetMaterialTagListKey(material));

    dmGraphics::DeleteVertexProgram(vp);
    dmGraphics::DeleteFragmentProgram(fp);

    dmRender::DeleteMaterial(m_RenderContext, material);
}

TEST_F(dmRenderMaterialTest, TestMaterialConstants)
{
    // create default material
    dmGraphics::ShaderDesc::Shader vp_shader = MakeDDFShader("uniform vec4 tint;\n", 19);
    dmGraphics::HVertexProgram vp = dmGraphics::NewVertexProgram(m_GraphicsContext, &vp_shader);

    dmGraphics::ShaderDesc::Shader fp_shader = MakeDDFShader("foo", 3);
    dmGraphics::HFragmentProgram fp = dmGraphics::NewFragmentProgram(m_GraphicsContext, &fp_shader);
    dmRender::HMaterial material = dmRender::NewMaterial(m_RenderContext, vp, fp);

    // Constants buffer
    dmRender::HNamedConstantBuffer constants = dmRender::NewNamedConstantBuffer();
    Vector4 test_v(1.0f, 0.0f, 0.0f, 0.0f);
    dmRender::SetNamedConstant(constants, dmHashString64("tint"), &test_v, 1);

    // renderobject default setup
    dmRender::RenderObject ro;
    ro.m_Material = material;
    ro.m_ConstantBuffer = constants;

    // test setting constant
    dmGraphics::HProgram program = dmRender::GetMaterialProgram(material);
    dmGraphics::EnableProgram(m_GraphicsContext, program);
    dmGraphics::HUniformLocation tint_loc = dmGraphics::GetUniformLocation(program, "tint");
    ASSERT_EQ(0, tint_loc);
    dmRender::ApplyNamedConstantBuffer(m_RenderContext, material, ro.m_ConstantBuffer);
    const Vector4& v = dmGraphics::GetConstantV4Ptr(m_GraphicsContext, tint_loc);
    ASSERT_EQ(1.0f, v.getX());
    ASSERT_EQ(0.0f, v.getY());
    ASSERT_EQ(0.0f, v.getZ());
    ASSERT_EQ(0.0f, v.getW());

    dmRender::DeleteNamedConstantBuffer(constants);
    dmGraphics::DisableProgram(m_GraphicsContext);
    dmGraphics::DeleteVertexProgram(vp);
    dmGraphics::DeleteFragmentProgram(fp);
    dmRender::DeleteMaterial(m_RenderContext, material);
}

TEST_F(dmRenderMaterialTest, TestMaterialVertexAttributes)
{

    const char* vs_src = \
       "attribute vec4 attribute_one;\n \
        attribute vec2 attribute_two;\n \
        attribute float attribute_three;\n";

    dmGraphics::ShaderDesc::Shader vp_shader = MakeDDFShader(vs_src, strlen(vs_src));
    dmGraphics::HVertexProgram vp            = dmGraphics::NewVertexProgram(m_GraphicsContext, &vp_shader);

    dmGraphics::ShaderDesc::Shader fp_shader = MakeDDFShader("foo", 3);
    dmGraphics::HFragmentProgram fp          = dmGraphics::NewFragmentProgram(m_GraphicsContext, &fp_shader);
    dmRender::HMaterial material             = dmRender::NewMaterial(m_RenderContext, vp, fp);

    const dmGraphics::VertexAttribute* attributes;
    uint32_t attribute_count;

    dmRender::GetMaterialProgramAttributes(material, &attributes, &attribute_count);

    ASSERT_NE((void*) 0x0, attributes);
    ASSERT_EQ(3, attribute_count);

    ASSERT_EQ(dmHashString64("attribute_one"), attributes[0].m_NameHash);
    ASSERT_EQ(4, attributes[0].m_ElementCount);
    ASSERT_EQ(dmGraphics::VertexAttribute::TYPE_FLOAT, attributes[0].m_DataType);

    ASSERT_EQ(dmHashString64("attribute_two"), attributes[1].m_NameHash);
    ASSERT_EQ(2, attributes[1].m_ElementCount);
    ASSERT_EQ(dmGraphics::VertexAttribute::TYPE_FLOAT, attributes[1].m_DataType);

    ASSERT_EQ(dmHashString64("attribute_three"), attributes[2].m_NameHash);
    ASSERT_EQ(1, attributes[2].m_ElementCount);
    ASSERT_EQ(dmGraphics::VertexAttribute::TYPE_FLOAT, attributes[2].m_DataType);

    dmGraphics::VertexAttribute attribute_overrides[3] = {};

    // Reconfigure all streams and set new data
    uint8_t bytes_one[] = { 127, 32 };
    attribute_overrides[0].m_NameHash                      = dmHashString64("attribute_one");
    attribute_overrides[0].m_ElementCount                  = 2;
    attribute_overrides[0].m_DataType                      = dmGraphics::VertexAttribute::TYPE_BYTE;
    attribute_overrides[0].m_Values.m_BinaryValues.m_Data  = bytes_one;
    attribute_overrides[0].m_Values.m_BinaryValues.m_Count = 2;

    uint8_t bytes_two[] = { 4, 3, 2, 1 };
    attribute_overrides[1].m_NameHash                      = dmHashString64("attribute_two");
    attribute_overrides[1].m_ElementCount                  = 4;
    attribute_overrides[1].m_DataType                      = dmGraphics::VertexAttribute::TYPE_BYTE;
    attribute_overrides[1].m_Values.m_BinaryValues.m_Data  = bytes_two;
    attribute_overrides[1].m_Values.m_BinaryValues.m_Count = 4;

    uint8_t bytes_three[] = { 64, 32, 16 };
    attribute_overrides[2].m_NameHash                      = dmHashString64("attribute_three");
    attribute_overrides[2].m_ElementCount                  = 3;
    attribute_overrides[2].m_DataType                      = dmGraphics::VertexAttribute::TYPE_BYTE;
    attribute_overrides[2].m_Values.m_BinaryValues.m_Data  = bytes_three;
    attribute_overrides[2].m_Values.m_BinaryValues.m_Count = 3;

    dmRender::SetMaterialProgramAttributes(material, attribute_overrides, DM_ARRAY_SIZE(attribute_overrides));

    const uint8_t* value_ptr;
    uint32_t num_values;

    // ONE
    dmRender::GetMaterialProgramAttributeValues(material, 0, &value_ptr, &num_values);
    ASSERT_EQ(attribute_overrides[0].m_Values.m_BinaryValues.m_Count, num_values);
    ASSERT_EQ(0, memcmp(attribute_overrides[0].m_Values.m_BinaryValues.m_Data, value_ptr, num_values));

    // TWO
    dmRender::GetMaterialProgramAttributeValues(material, 1, &value_ptr, &num_values);
    ASSERT_EQ(attribute_overrides[1].m_Values.m_BinaryValues.m_Count, num_values);
    ASSERT_EQ(0, memcmp(attribute_overrides[1].m_Values.m_BinaryValues.m_Data, value_ptr, num_values));

    // THREE
    dmRender::GetMaterialProgramAttributeValues(material, 2, &value_ptr, &num_values);
    ASSERT_EQ(attribute_overrides[2].m_Values.m_BinaryValues.m_Count, num_values);
    ASSERT_EQ(0, memcmp(attribute_overrides[2].m_Values.m_BinaryValues.m_Data, value_ptr, num_values));

    dmGraphics::DeleteVertexProgram(vp);
    dmGraphics::DeleteFragmentProgram(fp);
    dmRender::DeleteMaterial(m_RenderContext, material);
}

TEST_F(dmRenderMaterialTest, TestMaterialConstantsOverride)
{
    // create default material
    dmGraphics::ShaderDesc::Shader vp_shader = MakeDDFShader("uniform vec4 tint;\n", 19);
    dmGraphics::HVertexProgram vp = dmGraphics::NewVertexProgram(m_GraphicsContext, &vp_shader);
    dmGraphics::ShaderDesc::Shader fp_shader = MakeDDFShader("foo", 3);
    dmGraphics::HFragmentProgram fp = dmGraphics::NewFragmentProgram(m_GraphicsContext, &fp_shader);
    dmRender::HMaterial material = dmRender::NewMaterial(m_RenderContext, vp, fp);
    dmGraphics::HProgram program = dmRender::GetMaterialProgram(material);

    // create override material which contains tint, but at a different location
    vp_shader = MakeDDFShader("uniform vec4 dummy;\nuniform vec4 tint;\n", 40);
    dmGraphics::HVertexProgram vp_ovr = dmGraphics::NewVertexProgram(m_GraphicsContext, &vp_shader);
    dmGraphics::HFragmentProgram fp_ovr = dmGraphics::NewFragmentProgram(m_GraphicsContext, &fp_shader);
    dmRender::HMaterial material_ovr = dmRender::NewMaterial(m_RenderContext, vp_ovr, fp_ovr);
    dmGraphics::HProgram program_ovr = dmRender::GetMaterialProgram(material_ovr);

    // Constants
    dmRender::HNamedConstantBuffer constants = dmRender::NewNamedConstantBuffer();
    Vector4 test_v(1.0f, 0.0f, 0.0f, 0.0f);
    dmRender::SetNamedConstant(constants, dmHashString64("tint"), &test_v, 1);

    // renderobject default setup
    dmRender::RenderObject ro;
    ro.m_Material = material;
    ro.m_ConstantBuffer = constants;

    // using the null graphics device, constant locations are assumed to be in declaration order.
    // test setting constant, no override material
    dmGraphics::HUniformLocation tint_loc = dmGraphics::GetUniformLocation(program, "tint");
    ASSERT_EQ(0, tint_loc);
    dmGraphics::EnableProgram(m_GraphicsContext, program);
    dmRender::ApplyNamedConstantBuffer(m_RenderContext, material, ro.m_ConstantBuffer);
    const Vector4& v = dmGraphics::GetConstantV4Ptr(m_GraphicsContext, tint_loc);
    ASSERT_EQ(1.0f, v.getX());
    ASSERT_EQ(0.0f, v.getY());
    ASSERT_EQ(0.0f, v.getZ());
    ASSERT_EQ(0.0f, v.getW());

    // test setting constant, override material
    test_v = Vector4(2.0f, 1.0f, 1.0f, 1.0f);
    dmRender::ClearNamedConstantBuffer(constants);
    dmRender::SetNamedConstant(constants, dmHashString64("tint"), &test_v, 1);
    dmGraphics::HUniformLocation tint_loc_ovr = dmGraphics::GetUniformLocation(program_ovr, "tint");
    ASSERT_EQ(1, tint_loc_ovr);
    dmGraphics::EnableProgram(m_GraphicsContext, program_ovr);
    dmRender::ApplyNamedConstantBuffer(m_RenderContext, material_ovr, ro.m_ConstantBuffer);

    const Vector4& v_ovr = dmGraphics::GetConstantV4Ptr(m_GraphicsContext, tint_loc_ovr);
    ASSERT_EQ(2.0f, v_ovr.getX());
    ASSERT_EQ(1.0f, v_ovr.getY());
    ASSERT_EQ(1.0f, v_ovr.getZ());
    ASSERT_EQ(1.0f, v_ovr.getW());

    dmRender::DeleteNamedConstantBuffer(constants);
    dmGraphics::DisableProgram(m_GraphicsContext);
    dmGraphics::DeleteVertexProgram(vp_ovr);
    dmGraphics::DeleteFragmentProgram(fp_ovr);
    dmRender::DeleteMaterial(m_RenderContext, material_ovr);
    dmGraphics::DeleteVertexProgram(vp);
    dmGraphics::DeleteFragmentProgram(fp);
    dmRender::DeleteMaterial(m_RenderContext, material);
}

TEST_F(dmRenderMaterialTest, MatchMaterialTags)
{
    dmhash_t material_tags[] = { 1, 2, 3, 4, 5 };

    dmhash_t tags_a[] = { 1 };
    ASSERT_TRUE(dmRender::MatchMaterialTags(DM_ARRAY_SIZE(material_tags), material_tags, DM_ARRAY_SIZE(tags_a), tags_a));

    dmhash_t tags_b[] = { 0 };
    ASSERT_FALSE(dmRender::MatchMaterialTags(DM_ARRAY_SIZE(material_tags), material_tags, DM_ARRAY_SIZE(tags_b), tags_b));

    dmhash_t tags_c[] = { 2, 3 };
    ASSERT_TRUE(dmRender::MatchMaterialTags(DM_ARRAY_SIZE(material_tags), material_tags, DM_ARRAY_SIZE(tags_c), tags_c));

    // This list is unsorted, and will fail!
    dmhash_t tags_d[] = { 2, 3, 1 };
    ASSERT_FALSE(dmRender::MatchMaterialTags(DM_ARRAY_SIZE(material_tags), material_tags, DM_ARRAY_SIZE(tags_d), tags_d));

    dmhash_t tags_e[] = { 3, 4, 6 };
    ASSERT_FALSE(dmRender::MatchMaterialTags(DM_ARRAY_SIZE(material_tags), material_tags, DM_ARRAY_SIZE(tags_e), tags_e));
}

extern "C" void dmExportedSymbols();

int main(int argc, char **argv)
{
    dmExportedSymbols();
    TestMainPlatformInit();
    dmHashEnableReverseHash(true);
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
