// Copyright 2020-2023 The Defold Foundation
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

package com.dynamo.bob.pipeline;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import java.util.List;

import org.junit.Before;
import org.junit.Test;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Platform;
import com.dynamo.bob.pipeline.ShaderCompilerHelpers;
import com.dynamo.graphics.proto.Graphics.ShaderDesc;
import com.google.protobuf.Message;

public class ShaderProgramBuilderTest extends AbstractProtoBuilderTest {

    @Before
    public void setup() {
        addTestFiles();
    }

    private final String vp =
            "attribute vec4 position; \n" +
            "varying vec4 fragColor; \n" +
            "uniform vec4 color; \n" +
            "void main(){ \n" +
            "fragColor = color;\n" +
            "gl_Position = position; \n" +
            "}\n";

    private final String vpEs3 =
            "#version 310 es \n" +
            "in vec4 position; \n" +
            "out vec4 fragColor; \n" +
            "uniform NonOpaqueBlock { vec4 color; }; \n" +
            "void main(){ \n" +
            "   fragColor   = color;\n" +
            "   gl_Position = position; \n" +
            "}\n";

    private final String fp =
            "varying vec4 fragColor; \n" +
            "void main(){ \n" +
            "gl_FragColor = fragColor; \n" +
            "}\n";

    private final String fpEs3 =
            "#version 310 es \n" +
            "precision mediump float; \n" +
            "in vec4 fragColor; \n" +
            "out vec4 FragColorOut; \n" +
            "void main(){ \n" +
            "   FragColorOut = fragColor; \n" +
            "}\n";

    private static boolean hasPlatformSupportsSpirv() {
        switch(Platform.getHostPlatform())
        {
            case Arm64MacOS:
            case X86_64MacOS:
            case X86_64Linux:
            case X86_64Win32:
                return true;
            default:break;
        }
        return false;
    }

    private void doTest(boolean expectSpirv) throws Exception {
        // Test GL vp
        List<Message> outputs = build("/test_shader.vp", vp);
        ShaderDesc shader = (ShaderDesc)outputs.get(0);
        assertNotNull(shader.getShaders(0).getSource());
        assertEquals(ShaderDesc.Language.LANGUAGE_GLSL_SM140, shader.getShaders(0).getLanguage());

        if (expectSpirv && hasPlatformSupportsSpirv()) {
            assertEquals(2, shader.getShadersCount());
            assertNotNull(shader.getShaders(1).getSource());
            assertEquals(ShaderDesc.Language.LANGUAGE_SPIRV, shader.getShaders(1).getLanguage());
        } else {
            assertEquals(1, shader.getShadersCount());
        }

        // Test GL fp
        outputs = build("/test_shader.fp", fp);
        shader = (ShaderDesc)outputs.get(0);
        assert(shader.getShaders(0).getLanguage() == ShaderDesc.Language.LANGUAGE_GLSL_SM140);
        assertNotNull(shader.getShaders(0).getSource());
        assertEquals(ShaderDesc.Language.LANGUAGE_GLSL_SM140, shader.getShaders(0).getLanguage());

        if (expectSpirv && hasPlatformSupportsSpirv()) {
            assertEquals(2, shader.getShadersCount());
            assertNotNull(shader.getShaders(1).getSource());
            assertEquals(ShaderDesc.Language.LANGUAGE_SPIRV, shader.getShaders(1).getLanguage());
        } else {
            assertEquals(1, shader.getShadersCount());
        }

        // Test GLES vp
        if (expectSpirv) {
            if (hasPlatformSupportsSpirv()) {
                // If we have requested Spir-V, we have to test a ready-made ES3 version
                // Since we will not process the input shader if the #version preprocessor exists
                outputs = build("/test_shader.vp", vpEs3);
                shader = (ShaderDesc)outputs.get(0);
                assertEquals(2, shader.getShadersCount());
                assertNotNull(shader.getShaders(1).getSource());
                assertEquals(ShaderDesc.Language.LANGUAGE_SPIRV, shader.getShaders(1).getLanguage());
            }
        } else {
            outputs = build("/test_shader.vp", "#version 310 es\n" + vp);
            shader = (ShaderDesc)outputs.get(0);
            assertEquals(1, shader.getShadersCount());
        }

        // Test GLES fp
        if (expectSpirv) {
            if (hasPlatformSupportsSpirv()) {
                outputs = build("/test_shader.fp", fpEs3);
                shader = (ShaderDesc)outputs.get(0);
                assertEquals(2, shader.getShadersCount());
                assertNotNull(shader.getShaders(1).getSource());
                assertEquals(ShaderDesc.Language.LANGUAGE_SPIRV, shader.getShaders(1).getLanguage());
            }
        } else {
            outputs = build("/test_shader.fp", "#version 310 es\n" + fpEs3);
            shader = (ShaderDesc)outputs.get(0);
            assertEquals(1, shader.getShadersCount());
        }
    }

    @Test
    public void testShaderPrograms() throws Exception {
        doTest(false);
        GetProject().getProjectProperties().putBooleanValue("shader", "output_spirv", true);
        doTest(true);
    }

    private void testOutput(String expected, String source) {
        if (!expected.equals(source)) {
            System.err.println(String.format("EXPECTED:\n'%s'", expected));
            System.err.println(String.format("SOURCE:\n'%s'", source));
        }
        assertEquals(expected, source);
    }

    @Test
    public void testIncludes() throws Exception {
        String shader_base =
            "#include \"%s\"\n" +
            "void main(){\n" +
            "   gl_FragColor = vec4(1.0);\n" +
            "}\n";

        String expected_base = "#version 140\n" +
                               "\n" +
                               "\n" +
                               "#ifndef GL_ES\n" +
                               "#define lowp\n" +
                               "#define mediump\n" +
                               "#define highp\n" +
                               "#endif\n" +
                               "\n" +
                               "\n" +
                               "out vec4 _DMENGINE_GENERATED_gl_FragColor_0;\n" +
                               "%s" +
                               "\n" +
                               "void main(){\n" +
                               "   _DMENGINE_GENERATED_gl_FragColor_0 = vec4(1.0);\n" +
                               "}\n";

        // Test include a valid shader from the same folder
        {
            List<Message> outputs = build("/test_glsl_same_folder.fp", String.format(shader_base, "glsl_same_folder.glsl"));
            ShaderDesc shader     = (ShaderDesc)outputs.get(0);
            testOutput(String.format(expected_base,
                "const float same_folder = 0.0;\n"),
                shader.getShaders(0).getSource().toStringUtf8());
        }

        // Test include a valid shader from a subfolder
        {
            List<Message> outputs = build("/test_glsl_sub_folder_includes.fp", String.format(shader_base, "shader_includes/glsl_sub_include.glsl"));
            ShaderDesc shader     = (ShaderDesc)outputs.get(0);
            testOutput(String.format(expected_base,
                "const float sub_include = 0.0;\n"),
                shader.getShaders(0).getSource().toStringUtf8());
        }

        // Test include a valid shader from a subfolder that includes other files
        {
            List<Message> outputs = build("/test_glsl_sub_folder_multiple_includes.fp", String.format(shader_base, "shader_includes/glsl_sub_include_multi.glsl"));
            ShaderDesc shader     = (ShaderDesc)outputs.get(0);
            testOutput(String.format(expected_base,
                "const float sub_include = 0.0;\n" +
                "\n" +
                "const float sub_include_from_multi = 0.0;\n"),
                shader.getShaders(0).getSource().toStringUtf8());
        }

        // Test wrong path
        {
            boolean didFail = false;
            try {
                List<Message> outputs = build("/test_glsl_missing.fp", String.format(shader_base, "path-doesnt-exist.glsl"));
            } catch (CompileExceptionError e) {
                didFail = true;
            }
            assertTrue(didFail);
        }

        // Test path outside of project
        {
            boolean didFail = false;
            try {
                List<Message> outputs = build("/test_glsl_outside_of_project.fp", String.format(shader_base, "../path-doesnt-exist.glsl"));
            } catch (CompileExceptionError e) {
                didFail = true;
                assertTrue(e.getMessage().contains("includes file from outside of project root"));
            }
            assertTrue(didFail);
        }

        // Test self include
        {
            boolean didFail = false;
            try {
                List<Message> outputs = build("/test_glsl_self_include.fp", String.format(shader_base, "shader_includes/glsl_self_include.glsl"));
            } catch (CompileExceptionError e) {
                didFail = true;
            }
            assertTrue(didFail);
        }

        // Test cyclic include
        {
            boolean didFail = false;
            try {
                List<Message> outputs = build("/test_glsl_cyclic_include.fp", String.format(shader_base, "shader_includes/glsl_cyclic_include.glsl"));
            } catch (CompileExceptionError e) {
                didFail = true;
            }
            assertTrue(didFail);
        }
    }

    @Test
    public void testGlslDirectives() throws Exception {
        String source;
        String expected;

        source = ShaderCompilerHelpers.compileGLSL("", ShaderUtil.ES2ToES3Converter.ShaderType.VERTEX_SHADER, ShaderDesc.Language.LANGUAGE_GLSL_SM140, true);
        expected =  "#version 140\n" +
                    "#ifndef GL_ES\n" +
                    "#define lowp\n" +
                    "#define mediump\n" +
                    "#define highp\n" +
                    "#endif\n" +
                    "\n" +
                    "#line 0\n";
        testOutput(expected, source);

        source = "#extension GL_OES_standard_derivatives : enable\n" +
                 "varying highp vec2 var_texcoord0;";
        source = ShaderCompilerHelpers.compileGLSL(source, ShaderUtil.ES2ToES3Converter.ShaderType.VERTEX_SHADER, ShaderDesc.Language.LANGUAGE_GLSL_SM140, true);
        expected =  "#version 140\n" +
                    "#extension GL_OES_standard_derivatives : enable\n" +
                    "\n" +
                    "#ifndef GL_ES\n" +
                    "#define lowp\n" +
                    "#define mediump\n" +
                    "#define highp\n" +
                    "#endif\n" +
                    "\n" +
                    "#line 1\n" +
                    "out highp vec2 var_texcoord0;\n";
        testOutput(expected, source);

        source = "#extension GL_OES_standard_derivatives : enable\n" +
                 "void main() {\n" +
                 "    gl_FragColor = vec4(1.0);\n" +
                 "}";
        source = ShaderCompilerHelpers.compileGLSL(source, ShaderUtil.ES2ToES3Converter.ShaderType.FRAGMENT_SHADER, ShaderDesc.Language.LANGUAGE_GLES_SM100, true);
        expected =  "#extension GL_OES_standard_derivatives : enable\n" +
                    "\n" +
                    "precision mediump float;\n" +
                    "#line 1\n" +
                    "void main() {\n" +
                    "    gl_FragColor = vec4(1.0);\n" +
                    "}\n";
        testOutput(expected, source);

        source = "#extension GL_OES_standard_derivatives : enable\n" +
                 "void main() {\n" +
                 "    gl_FragColor = vec4(1.0);\n" +
                 "}";
        source = ShaderCompilerHelpers.compileGLSL(source, ShaderUtil.ES2ToES3Converter.ShaderType.FRAGMENT_SHADER, ShaderDesc.Language.LANGUAGE_GLES_SM300, true);
        expected =  "#version 300 es\n" +
                    "#extension GL_OES_standard_derivatives : enable\n" +
                    "\n" +
                    "precision mediump float;\n" +
                    "\n" +
                    "out vec4 _DMENGINE_GENERATED_gl_FragColor_0;\n" +
                    "#line 1\n" +
                    "void main() {\n" +
                    "    _DMENGINE_GENERATED_gl_FragColor_0 = vec4(1.0);\n" +
                    "}\n";
        testOutput(expected, source);
    }

    @Test
    public void testGlslMisc() throws Exception {
        String source;
        String expected;

        // Test guards around a uniform when transforming with latest features (uniform blocks)
        source =
            "varying vec4 my_varying;\n" +
            "#ifndef MY_DEFINE\n" +
            "#define MY_DEFINE\n" +
            "   uniform vec4 my_uniform;\n" +
            "#endif\n" +
            "void main(){\n" +
            "   gl_FragColor = my_uniform + my_varying;\n" +
            "}\n";

        ShaderUtil.ES2ToES3Converter.Result res = ShaderUtil.ES2ToES3Converter.transform(source, ShaderUtil.ES2ToES3Converter.ShaderType.FRAGMENT_SHADER, "", 140, true);

        expected =
            "#version 140\n" +
            "\n" +
            "out vec4 _DMENGINE_GENERATED_gl_FragColor_0;\n" +
            "in vec4 my_varying;\n" +
            "#ifndef MY_DEFINE\n" +
            "#define MY_DEFINE\n" +
            "\n" +
            "layout(set=1) uniform _DMENGINE_GENERATED_UB_FS_0 { vec4 my_uniform  ; };\n" +
            "#endif\n" +
            "void main(){\n" +
            "   _DMENGINE_GENERATED_gl_FragColor_0 = my_uniform + my_varying;\n" +
            "}\n";

        testOutput(expected, res.output);
    }

    static ShaderDesc.Shader getShaderByLanguage(ShaderDesc shaderDesc, ShaderDesc.Language language) {
        for (ShaderDesc.Shader shader : shaderDesc.getShadersList()) {
            if (shader.getLanguage() == language) {
                return shader;
            }
        }
        return null;
    }

    @Test
    public void testUniformBuffers() throws Exception {
        String vp =
            "attribute vec4 position; \n" +
            "varying vec4 fragColor; \n" +

            "uniform uniform_buffer {\n" +
            "   vec4 color_one;\n" +
            "   vec4 color_two;\n" +
            "} ubo;\n" +
            "uniform vec4 color; \n" +
            "void main(){ \n" +
            "   fragColor = ubo.color_one + ubo.color_two;\n" +
            "   gl_Position = position; \n" +
            "}\n";

        GetProject().getProjectProperties().putBooleanValue("shader", "output_spirv", true);

        List<Message> outputs = build("/test_shader.vp", vp);
        ShaderDesc shaderDesc = (ShaderDesc) outputs.get(0);

        assert(shaderDesc.getShadersCount() > 0);

        ShaderDesc.Shader shader = getShaderByLanguage(shaderDesc, ShaderDesc.Language.LANGUAGE_SPIRV);
        assertNotNull(shader);

        ShaderDesc.ResourceBlock ubo = shader.getResources(0);
        assertTrue(ubo.getName().equals("uniform_buffer"));

        ShaderDesc.ResourceBinding binding_one = ubo.getBindings(0);
        assertTrue(binding_one.getName().equals("color_one"));

        ShaderDesc.ResourceBinding binding_two = ubo.getBindings(1);
        assertTrue(binding_two.getName().equals("color_two"));
    }
}
