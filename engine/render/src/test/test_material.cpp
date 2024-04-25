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

#include "../../../graphics/src/graphics_private.h"
#include "../../../graphics/src/null/graphics_null_private.h"

#define ASSERT_EPS 0.0001f

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
    attribute_overrides[0].m_ShaderType                    = dmGraphics::VertexAttribute::SHADER_TYPE_VEC2;
    attribute_overrides[0].m_DataType                      = dmGraphics::VertexAttribute::TYPE_BYTE;
    attribute_overrides[0].m_Values.m_BinaryValues.m_Data  = bytes_one;
    attribute_overrides[0].m_Values.m_BinaryValues.m_Count = 2;

    uint8_t bytes_two[] = { 4, 3, 2, 1 };
    attribute_overrides[1].m_NameHash                      = dmHashString64("attribute_two");
    attribute_overrides[1].m_ShaderType                    = dmGraphics::VertexAttribute::SHADER_TYPE_VEC4;
    attribute_overrides[1].m_DataType                      = dmGraphics::VertexAttribute::TYPE_BYTE;
    attribute_overrides[1].m_Values.m_BinaryValues.m_Data  = bytes_two;
    attribute_overrides[1].m_Values.m_BinaryValues.m_Count = 4;

    uint8_t bytes_three[] = { 64, 32, 16 };
    attribute_overrides[2].m_NameHash                      = dmHashString64("attribute_three");
    attribute_overrides[2].m_ShaderType                    = dmGraphics::VertexAttribute::SHADER_TYPE_VEC3;
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

TEST_F(dmRenderMaterialTest, TestMaterialInstanceNotSupported)
{
    const char* vs_src = \
       "attribute vec4 position;\n \
        attribute vec2 normal;\n \
        attribute mat3 mtx_normal;\n \
        attribute mat4 mtx_world;\n";


    dmGraphics::NullContext* null_context = (dmGraphics::NullContext*) m_GraphicsContext;
    // Turn off all context features manually
    null_context->m_ContextFeatures = 0;

    dmGraphics::ShaderDesc::Shader vp_shader = MakeDDFShader(vs_src, strlen(vs_src));
    dmGraphics::HVertexProgram vp            = dmGraphics::NewVertexProgram(m_GraphicsContext, &vp_shader);

    dmGraphics::ShaderDesc::Shader fp_shader = MakeDDFShader("foo", 3);
    dmGraphics::HFragmentProgram fp          = dmGraphics::NewFragmentProgram(m_GraphicsContext, &fp_shader);
    dmRender::HMaterial material             = dmRender::NewMaterial(m_RenderContext, vp, fp);

    dmGraphics::HVertexDeclaration vx_decl_shared = dmRender::GetVertexDeclaration(material);
    dmGraphics::HVertexDeclaration vx_decl_vert = dmRender::GetVertexDeclaration(material, dmGraphics::VERTEX_STEP_FUNCTION_VERTEX);
    dmGraphics::HVertexDeclaration vx_decl_inst = dmRender::GetVertexDeclaration(material, dmGraphics::VERTEX_STEP_FUNCTION_INSTANCE);

    ASSERT_EQ(vx_decl_shared, vx_decl_vert);
    ASSERT_EQ((void*) 0, vx_decl_inst);

    dmGraphics::DeleteVertexProgram(vp);
    dmGraphics::DeleteFragmentProgram(fp);
    dmRender::DeleteMaterial(m_RenderContext, material);
}

TEST_F(dmRenderMaterialTest, TestMaterialInstanceAttributes)
{
    const char* vs_src = \
       "attribute vec4 position;\n \
        attribute vec2 normal;\n \
        attribute mat3 mtx_normal;\n \
        attribute mat4 mtx_world;\n";

    dmGraphics::ShaderDesc::Shader vp_shader = MakeDDFShader(vs_src, strlen(vs_src));
    dmGraphics::HVertexProgram vp            = dmGraphics::NewVertexProgram(m_GraphicsContext, &vp_shader);

    dmGraphics::ShaderDesc::Shader fp_shader = MakeDDFShader("foo", 3);
    dmGraphics::HFragmentProgram fp          = dmGraphics::NewFragmentProgram(m_GraphicsContext, &fp_shader);
    dmRender::HMaterial material             = dmRender::NewMaterial(m_RenderContext, vp, fp);

    const dmGraphics::VertexAttribute* attributes;
    uint32_t attribute_count;

    dmRender::GetMaterialProgramAttributes(material, &attributes, &attribute_count);

    ASSERT_NE((void*) 0x0, attributes);
    ASSERT_EQ(4, attribute_count);

    ASSERT_EQ(dmHashString64("position"),                    attributes[0].m_NameHash);
    ASSERT_EQ(4,                                             attributes[0].m_ElementCount);
    ASSERT_EQ(dmGraphics::VertexAttribute::SHADER_TYPE_VEC4, attributes[0].m_ShaderType);
    ASSERT_EQ(dmGraphics::VertexAttribute::TYPE_FLOAT,       attributes[0].m_DataType);

    ASSERT_EQ(dmHashString64("normal"),                      attributes[1].m_NameHash);
    ASSERT_EQ(2,                                             attributes[1].m_ElementCount);
    ASSERT_EQ(dmGraphics::VertexAttribute::SHADER_TYPE_VEC2, attributes[1].m_ShaderType);
    ASSERT_EQ(dmGraphics::VertexAttribute::TYPE_FLOAT,       attributes[1].m_DataType);

    ASSERT_EQ(dmHashString64("mtx_normal"),                  attributes[2].m_NameHash);
    ASSERT_EQ(9,                                             attributes[2].m_ElementCount);
    ASSERT_EQ(dmGraphics::VertexAttribute::SHADER_TYPE_MAT3, attributes[2].m_ShaderType);
    ASSERT_EQ(dmGraphics::VertexAttribute::TYPE_FLOAT,       attributes[2].m_DataType);

    ASSERT_EQ(dmHashString64("mtx_world"),                   attributes[3].m_NameHash);
    ASSERT_EQ(16,                                            attributes[3].m_ElementCount);
    ASSERT_EQ(dmGraphics::VertexAttribute::SHADER_TYPE_MAT4, attributes[3].m_ShaderType);
    ASSERT_EQ(dmGraphics::VertexAttribute::TYPE_FLOAT,       attributes[3].m_DataType);

    dmGraphics::HVertexDeclaration vx_decl_shared = dmRender::GetVertexDeclaration(material);
    dmGraphics::HVertexDeclaration vx_decl        = dmRender::GetVertexDeclaration(material, dmGraphics::VERTEX_STEP_FUNCTION_VERTEX);
    ASSERT_NE((void*) 0x0, vx_decl);
    ASSERT_NE(vx_decl_shared, vx_decl);

    ASSERT_EQ(2,                                       vx_decl->m_StreamCount);
    ASSERT_EQ(dmGraphics::VERTEX_STEP_FUNCTION_VERTEX, vx_decl->m_StepFunction);

    ASSERT_EQ(dmHashString64("position"), vx_decl->m_Streams[0].m_NameHash);
    ASSERT_EQ(4,                          vx_decl->m_Streams[0].m_Size);
    ASSERT_EQ(dmGraphics::TYPE_FLOAT,     vx_decl->m_Streams[0].m_Type);

    ASSERT_EQ(dmHashString64("normal"),   vx_decl->m_Streams[1].m_NameHash);
    ASSERT_EQ(2,                          vx_decl->m_Streams[1].m_Size);
    ASSERT_EQ(dmGraphics::TYPE_FLOAT,     vx_decl->m_Streams[1].m_Type);

    dmGraphics::HVertexDeclaration inst_decl = dmRender::GetVertexDeclaration(material, dmGraphics::VERTEX_STEP_FUNCTION_INSTANCE);
    ASSERT_NE((void*) 0x0, inst_decl);

    ASSERT_EQ(2,                                         inst_decl->m_StreamCount);
    ASSERT_EQ(dmGraphics::VERTEX_STEP_FUNCTION_INSTANCE, inst_decl->m_StepFunction);

    ASSERT_EQ(dmHashString64("mtx_normal"), inst_decl->m_Streams[0].m_NameHash);
    ASSERT_EQ(9,                            inst_decl->m_Streams[0].m_Size);
    ASSERT_EQ(dmGraphics::TYPE_FLOAT,       inst_decl->m_Streams[0].m_Type);

    ASSERT_EQ(dmHashString64("mtx_world"), inst_decl->m_Streams[1].m_NameHash);
    ASSERT_EQ(16,                          inst_decl->m_Streams[1].m_Size);
    ASSERT_EQ(dmGraphics::TYPE_FLOAT,      inst_decl->m_Streams[1].m_Type);

    // Override the attributes with custom values and settings
    dmGraphics::VertexAttribute attribute_overrides[4] = {};

    uint8_t bytes_position[] = { 127, 32 };
    attribute_overrides[0].m_NameHash                      = dmHashString64("position");
    attribute_overrides[0].m_ShaderType                    = dmGraphics::VertexAttribute::SHADER_TYPE_VEC2;
    attribute_overrides[0].m_DataType                      = dmGraphics::VertexAttribute::TYPE_BYTE;
    attribute_overrides[0].m_Values.m_BinaryValues.m_Data  = bytes_position;
    attribute_overrides[0].m_Values.m_BinaryValues.m_Count = sizeof(bytes_position);

    uint8_t bytes_normal[] = { 4, 3, 2, 1 };
    attribute_overrides[1].m_NameHash                      = dmHashString64("normal");
    attribute_overrides[1].m_ShaderType                    = dmGraphics::VertexAttribute::SHADER_TYPE_VEC4;
    attribute_overrides[1].m_DataType                      = dmGraphics::VertexAttribute::TYPE_BYTE;
    attribute_overrides[1].m_Values.m_BinaryValues.m_Data  = bytes_normal;
    attribute_overrides[1].m_Values.m_BinaryValues.m_Count = sizeof(bytes_normal);

    float bytes_mtx_normal_2x2[2][2] = { { 1.0f, 2.0f }, { 3.0f, 4.0f } };
    attribute_overrides[2].m_NameHash                      = dmHashString64("mtx_normal");
    attribute_overrides[2].m_ShaderType                    = dmGraphics::VertexAttribute::SHADER_TYPE_MAT2;
    attribute_overrides[2].m_DataType                      = dmGraphics::VertexAttribute::TYPE_FLOAT;
    attribute_overrides[2].m_Values.m_BinaryValues.m_Data  = (uint8_t*) bytes_mtx_normal_2x2;
    attribute_overrides[2].m_Values.m_BinaryValues.m_Count = sizeof(bytes_mtx_normal_2x2);

    float bytes_mtx_world_3x3[3][3] = { { 1.0f, 2.0f, 3.0f }, { 4.0f, 5.0f, 6.0f }, { 7.0f, 8.0f, 9.0f } };
    attribute_overrides[3].m_NameHash                      = dmHashString64("mtx_world");
    attribute_overrides[3].m_ShaderType                    = dmGraphics::VertexAttribute::SHADER_TYPE_MAT3;
    attribute_overrides[3].m_DataType                      = dmGraphics::VertexAttribute::TYPE_FLOAT;
    attribute_overrides[3].m_Values.m_BinaryValues.m_Data  = (uint8_t*) bytes_mtx_world_3x3;
    attribute_overrides[3].m_Values.m_BinaryValues.m_Count = sizeof(bytes_mtx_world_3x3);

    dmRender::SetMaterialProgramAttributes(material, attribute_overrides, DM_ARRAY_SIZE(attribute_overrides));

    const uint8_t* value_ptr;
    uint32_t num_values;

    // position
    dmRender::GetMaterialProgramAttributeValues(material, 0, &value_ptr, &num_values);
    ASSERT_EQ(attribute_overrides[0].m_Values.m_BinaryValues.m_Count, num_values);
    ASSERT_EQ(0, memcmp(attribute_overrides[0].m_Values.m_BinaryValues.m_Data, value_ptr, num_values));

    // normal
    dmRender::GetMaterialProgramAttributeValues(material, 1, &value_ptr, &num_values);
    ASSERT_EQ(attribute_overrides[1].m_Values.m_BinaryValues.m_Count, num_values);
    ASSERT_EQ(0, memcmp(attribute_overrides[1].m_Values.m_BinaryValues.m_Data, value_ptr, num_values));

    // mtx_normal
    dmRender::GetMaterialProgramAttributeValues(material, 2, &value_ptr, &num_values);
    ASSERT_EQ(attribute_overrides[2].m_Values.m_BinaryValues.m_Count, num_values);
    {
        float* f_value = (float*) value_ptr;
        float* f_exp   = (float*) attribute_overrides[2].m_Values.m_BinaryValues.m_Data;
        for (int i = 0; i < num_values / sizeof(float); ++i)
        {
            ASSERT_NEAR(f_exp[i], f_value[i], ASSERT_EPS);
        }
    }

    // mtx_world
    dmRender::GetMaterialProgramAttributeValues(material, 3, &value_ptr, &num_values);
    ASSERT_EQ(attribute_overrides[3].m_Values.m_BinaryValues.m_Count, num_values);
    {
        float* f_value = (float*) value_ptr;
        float* f_exp   = (float*) attribute_overrides[3].m_Values.m_BinaryValues.m_Data;
        for (int i = 0; i < num_values / sizeof(float); ++i)
        {
            ASSERT_NEAR(f_exp[i], f_value[i], ASSERT_EPS);
        }
    }

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
