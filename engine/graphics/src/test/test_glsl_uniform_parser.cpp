// Copyright 2020 The Defold Foundation
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
    const char*         m_Name;
    uint32_t            m_Length;
    dmGraphics::Type    m_Type;
};

static void UniformCallback(const char* name, uint32_t name_length, dmGraphics::Type type, uintptr_t userdata)
{
    Uniform* uniform = (Uniform*)userdata;
    uniform->m_Name = name;
    uniform->m_Length = name_length;
    uniform->m_Type = type;
}

#define ASSERT_TYPE(type_name, type)\
        {\
            Uniform uniform;\
            const char* program = "uniform mediump " type_name " " type_name ";\n";\
            bool result = dmGraphics::GLSLUniformParse(program, UniformCallback, (uintptr_t)&uniform);\
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
    bool result = dmGraphics::GLSLUniformParse(program, UniformCallback, (uintptr_t)&uniform);
    ASSERT_TRUE(result);
    ASSERT_EQ(0, strncmp("texture_sampler", uniform.m_Name, strnlen("texture_sampler", uniform.m_Length)));
    ASSERT_EQ(dmGraphics::TYPE_SAMPLER_2D, uniform.m_Type);
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
