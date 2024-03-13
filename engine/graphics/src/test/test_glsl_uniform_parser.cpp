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

#include <string.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include "graphics.h"
#include "null/glsl_uniform_parser.h"

class dmGLSLUniformTest : public jc_test_base_class
{
protected:
    virtual void SetUp()
    {
    }

    virtual void TearDown()
    {
    }
};

struct Uniform
{
    char                m_Name[64];
    uint32_t            m_Length;
    uint32_t            m_Size;
    dmGraphics::Type    m_Type;
};

static void UniformCallback(dmGraphics::GLSLUniformParserBindingType binding_type, const char* name, uint32_t name_length, dmGraphics::Type type, uint32_t size, uintptr_t userdata)
{
    Uniform* uniform = (Uniform*)userdata;
    if (name_length > sizeof(uniform->m_Name))
    {
        name_length = sizeof(uniform->m_Name)-1;
    }

    memcpy(uniform->m_Name, name, name_length);
    uniform->m_Name[name_length] = 0;
    uniform->m_Length = name_length;
    uniform->m_Size = size;
    uniform->m_Type = type;
}

#define ASSERT_TYPE(type_name, type)\
        {\
            Uniform uniform;\
            const char* program = "uniform mediump " type_name " " type_name ";\n";\
            bool result = dmGraphics::GLSLUniformParse(dmGraphics::ShaderDesc::LANGUAGE_GLES_SM100, program, UniformCallback, (uintptr_t)&uniform);\
            ASSERT_TRUE(result);\
            ASSERT_EQ(0, strncmp(type_name, uniform.m_Name, strnlen(type_name, uniform.m_Length)));\
            ASSERT_EQ(dmGraphics::TYPE_##type, uniform.m_Type);\
        }

TEST_F(dmGLSLUniformTest, Types)
{
    ASSERT_TYPE("int", INT);
    ASSERT_TYPE("uint", UNSIGNED_INT);
    ASSERT_TYPE("float", FLOAT);
    ASSERT_TYPE("vec4", FLOAT_VEC4);
    ASSERT_TYPE("mat4", FLOAT_MAT4);
    ASSERT_TYPE("sampler2D", SAMPLER_2D);
}

TEST_F(dmGLSLUniformTest, IntroductionJunk)
{
    Uniform uniform;
    const char* program = ""
            "varying mediump vec4 position;\n"
            "varying mediump vec2 var_texcoord0;\n"
            "uniform lowp sampler2D texture_sampler;\n";
    bool result = dmGraphics::GLSLUniformParse(dmGraphics::ShaderDesc::LANGUAGE_GLES_SM100, program, UniformCallback, (uintptr_t)&uniform);
    ASSERT_TRUE(result);
    ASSERT_STREQ("texture_sampler", uniform.m_Name);
    ASSERT_EQ(dmGraphics::TYPE_SAMPLER_2D, uniform.m_Type);
}

TEST_F(dmGLSLUniformTest, UniformArraySize)
{
    Uniform uniform = {};
    const char* program = "uniform lowp vec4 uniform_array[16];\n";
    bool result = dmGraphics::GLSLUniformParse(dmGraphics::ShaderDesc::LANGUAGE_GLES_SM100, program, UniformCallback, (uintptr_t)&uniform);
    ASSERT_TRUE(result);
    ASSERT_STREQ("uniform_array", uniform.m_Name);
    ASSERT_EQ(dmGraphics::TYPE_FLOAT_VEC4, uniform.m_Type);
    ASSERT_EQ(16U, uniform.m_Size);
}

TEST_F(dmGLSLUniformTest, AttributeTest)
{
    Uniform attribute = {};
    const char* program = "attribute lowp vec4 attribute_one;\n";
    bool result = dmGraphics::GLSLAttributeParse(dmGraphics::ShaderDesc::LANGUAGE_GLES_SM100, program, UniformCallback, (uintptr_t)&attribute);
    ASSERT_TRUE(result);
    ASSERT_STREQ("attribute_one", attribute.m_Name);
    ASSERT_EQ(dmGraphics::TYPE_FLOAT_VEC4, attribute.m_Type);
    ASSERT_EQ(1, attribute.m_Size);
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
