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

package com.dynamo.bob.pipeline;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import java.util.List;

import org.junit.Before;
import org.junit.Test;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Platform;
import com.dynamo.graphics.proto.Graphics.ShaderDesc;
import com.google.protobuf.Message;

public class ShaderProgramBuilderTest extends AbstractProtoBuilderTest {

    @Before
    public void setup() {
        addTestFiles();
    }

    public static final String vp =
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

    public static final String fp =
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

    private static ShaderDesc.Language getPlatformGLSLLanguage() {
        switch(Platform.getHostPlatform())
        {
            case Arm64MacOS:
            case X86_64MacOS:
                return ShaderDesc.Language.LANGUAGE_GLSL_SM330;
            case X86_64Linux:
            case X86_64Win32:
                return ShaderDesc.Language.LANGUAGE_GLSL_SM140;
            default:break;
        }
        return null;
    }

    private static int getPlatformGLSLVersion() {
        switch(Platform.getHostPlatform())
        {
            case Arm64MacOS:
            case X86_64MacOS:
                return 330;
            case X86_64Linux:
            case X86_64Win32:
                return 140;
            default:break;
        }
        return 0;
    }

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
        assertEquals(getPlatformGLSLLanguage(), shader.getShaders(0).getLanguage());

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
        assertNotNull(shader.getShaders(0).getSource());
        assertEquals(getPlatformGLSLLanguage(), shader.getShaders(0).getLanguage());

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
            outputs = build("/test_shader.vp", vpEs3);
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
            outputs = build("/test_shader.fp", fpEs3);
            shader = (ShaderDesc)outputs.get(0);
            assertEquals(1, shader.getShadersCount());
        }
    }

    private static void debugPrintResourceList(String label, List<ShaderDesc.ResourceBinding> lst) {
        System.out.println(label);

        for (ShaderDesc.ResourceBinding r : lst) {
            System.out.println("Resource");
            System.out.println("  name: " + r.getName());
            System.out.println("  hash: " + r.getNameHash());
            System.out.println("  set: " + r.getSet());
            System.out.println("  binding: " + r.getBinding());

            ShaderDesc.ResourceType resourceType = r.getType();
            if (resourceType.hasShaderType()) {
                System.out.println("   type: " + resourceType.getShaderType());
            } else {
                System.out.println("   type: " + resourceType.getTypeIndex());
            }
        }
    }

    private static void debugPrintShaderReflection(String label, ShaderDesc.ShaderReflection r) {
        // Remove for debugging:
        if (false) {
            return;
        }

        System.out.println("debugPrintShaderReflection: " + label);

        debugPrintResourceList("UBOs", r.getUniformBuffersList());
        debugPrintResourceList("SSBOs", r.getStorageBuffersList());
        debugPrintResourceList("Textures", r.getTexturesList());

        for (ShaderDesc.ResourceTypeInfo t : r.getTypesList()) {
            System.out.println("Type");
            System.out.println(" name: " + t.getName());
            System.out.println(" hash: " + t.getNameHash());

            for (ShaderDesc.ResourceMember m : t.getMembersList()) {
                System.out.println(" Members:");
                System.out.println("   name: " + m.getName());
                System.out.println("   hash: " + m.getNameHash());
                System.out.println("   count: " + m.getElementCount());

                ShaderDesc.ResourceType resourceType = m.getType();
                if (resourceType.hasShaderType()) {
                    System.out.println("   type: " + resourceType.getShaderType());
                } else {
                    System.out.println("   type: " + resourceType.getTypeIndex());
                }
            }
        }
    }

    private static void validateResourceBindingWithKnownType(ShaderDesc.ResourceBinding binding, String expectedName, ShaderDesc.ShaderDataType expectedDataType) {
        assertEquals(expectedName, binding.getName());
        ShaderDesc.ShaderDataType bindingType = binding.getType().getShaderType();
        assertEquals(expectedDataType, bindingType);
    }

    @Test
    public void testShaderReflection() throws Exception {
        GetProject().getProjectProperties().putBooleanValue("shader", "output_spirv", true);

        {
            String fs_src =
                "struct Light \n" +
                "{ \n" +
                "    int type; \n" +
                "    vec3 position; \n" +
                "    vec4 color; \n" +
                "}; \n" +
                "uniform Light u_lights[4];\n" +
                "void main() \n" +
                "{ \n" +
                "    gl_FragColor = u_lights[0].color + u_lights[3].color; \n" +
                "} \n";

            List<Message> outputs = build("/reflection_0.fp", fs_src);
            ShaderDesc shaderDesc = (ShaderDesc) outputs.get(0);

            assertTrue(shaderDesc.getShadersCount() > 0);
            ShaderDesc.Shader shader = getShaderByLanguage(shaderDesc, ShaderDesc.Language.LANGUAGE_SPIRV);
            assertNotNull(shader);

            ShaderDesc.ShaderReflection r = shaderDesc.getReflection();
            assertEquals(1, r.getUniformBuffersCount());

            debugPrintShaderReflection("Reflection Test 0", r);

            // Note: When we don't specify a version and the uniforms are not packed in constant buffers,
            //       we wrap them around generated uniform buffers, which requires us to dig out the type infomartion like this:
            ShaderDesc.ResourceBinding binding_test = r.getUniformBuffers(0);
            ShaderDesc.ResourceTypeInfo binding_type = r.getTypes(binding_test.getType().getTypeIndex());
            ShaderDesc.ResourceMember binding_type_member = binding_type.getMembers(0);
            assertEquals("u_lights", binding_type_member.getName());

            ShaderDesc.ResourceTypeInfo lights_binding_type = r.getTypes(binding_type_member.getType().getTypeIndex());
            assertEquals("Light", lights_binding_type.getName());
            assertEquals(3, lights_binding_type.getMembersCount());

            ShaderDesc.ResourceMember light_member_type  = lights_binding_type.getMembers(0);
            ShaderDesc.ResourceMember light_member_pos   = lights_binding_type.getMembers(1);
            ShaderDesc.ResourceMember light_member_color = lights_binding_type.getMembers(2);

            assertEquals("type", light_member_type.getName());
            assertEquals(ShaderDesc.ShaderDataType.SHADER_TYPE_INT, light_member_type.getType().getShaderType());
            assertEquals("position", light_member_pos.getName());
            assertEquals(ShaderDesc.ShaderDataType.SHADER_TYPE_VEC3, light_member_pos.getType().getShaderType());
            assertEquals("color", light_member_color.getName());
            assertEquals(ShaderDesc.ShaderDataType.SHADER_TYPE_VEC4, light_member_color.getType().getShaderType());
        }

        {
            String fs_src =
                "#version 430 \n" +
                "struct Data \n" +
                "{ \n" +
                "    vec4 member1; \n" +
                "}; \n" +
                "buffer Test \n" +
                "{ \n" +
                "    Data my_data_one; \n" +
                "    Data my_data_two[]; \n" +
                "}; \n" +
                "out vec4 color_out; \n" +
                "void main() \n" +
                "{ \n" +
                "    color_out = my_data_one.member1 + my_data_two[0].member1; \n" +
                "} \n";

            List<Message> outputs = build("/reflection_1.fp", fs_src);
            ShaderDesc shaderDesc = (ShaderDesc) outputs.get(0);

            assertTrue(shaderDesc.getShadersCount() > 0);
            ShaderDesc.Shader shader = getShaderByLanguage(shaderDesc, ShaderDesc.Language.LANGUAGE_SPIRV);
            assertNotNull(shader);

            ShaderDesc.ShaderReflection r = shaderDesc.getReflection();
            debugPrintShaderReflection("Reflection Test 1", r);

            assertEquals(1, r.getStorageBuffersCount());
            ShaderDesc.ResourceBinding binding_test = r.getStorageBuffers(0);
            assertEquals("Test", binding_test.getName());

            ShaderDesc.ResourceTypeInfo binding_type = r.getTypes(binding_test.getType().getTypeIndex());
            assertEquals("Test", binding_type.getName());
        }

        {
            String fs_src =
                "#version 430\n" +
                "uniform sampler2D sampler_2d;\n" +
                "uniform sampler2DArray sampler_2d_array;\n" +
                "uniform samplerCube sampler_cube;\n" +
                // TODO:
                //"uniform samplerBuffer sampler_buffer;\n" +
                "uniform texture2D texture_2d;\n" +
                "uniform utexture2D utexture_2d;\n" +
                "uniform layout(rgba8ui) uimage2D uimage_2d;\n" +
                "uniform layout(rgba32f) image2D image_2d;\n" +
                "uniform sampler sampler_name;\n" +
                "out vec4 color_out;\n" +
                "void main()\n" +
                "{\n" +
                "   color_out  = vec4(0.0);\n" +
                "   color_out += texture(sampler_2d, vec2(0.0));\n" +
                "   color_out += texture(sampler_2d_array, vec3(0.0));\n" +
                "   color_out += texture(sampler_cube, vec3(0.0));\n" +
                // TODO:
                //"   color_out += texelFetch(sampler_buffer, 0);\n" +
                "   color_out += texture(sampler2D(texture_2d, sampler_name), vec2(0.0));\n" +
                "   color_out += texture(usampler2D(utexture_2d, sampler_name), vec2(0));\n" +
                "   color_out += imageLoad(uimage_2d, ivec2(0));\n" +
                "   color_out += imageLoad(image_2d, ivec2(0));\n" +
                "}\n";

            List<Message> outputs = build("/reflection_2.fp", fs_src);
            ShaderDesc shaderDesc = (ShaderDesc) outputs.get(0);

            assertTrue(shaderDesc.getShadersCount() > 0);
            ShaderDesc.Shader shader = getShaderByLanguage(shaderDesc, ShaderDesc.Language.LANGUAGE_SPIRV);
            assertNotNull(shader);

            ShaderDesc.ShaderReflection r = shaderDesc.getReflection();
            debugPrintShaderReflection("Reflection Test 2", r);

            assertEquals(8, r.getTexturesCount());
            validateResourceBindingWithKnownType(r.getTextures(0), "sampler_2d",       ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER2D);
            validateResourceBindingWithKnownType(r.getTextures(1), "sampler_2d_array", ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER2D_ARRAY);
            validateResourceBindingWithKnownType(r.getTextures(2), "sampler_cube",     ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER_CUBE);
            //TODO:
            //validateResourceBindingWithKnownType(shader.getTextures(2), "sampler_buffer",   ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER_);
            validateResourceBindingWithKnownType(r.getTextures(3), "texture_2d",       ShaderDesc.ShaderDataType.SHADER_TYPE_TEXTURE2D);
            validateResourceBindingWithKnownType(r.getTextures(4), "utexture_2d",      ShaderDesc.ShaderDataType.SHADER_TYPE_UTEXTURE2D);
            validateResourceBindingWithKnownType(r.getTextures(5), "uimage_2d",        ShaderDesc.ShaderDataType.SHADER_TYPE_UIMAGE2D);
            validateResourceBindingWithKnownType(r.getTextures(6), "image_2d",         ShaderDesc.ShaderDataType.SHADER_TYPE_IMAGE2D);
            validateResourceBindingWithKnownType(r.getTextures(7), "sampler_name",     ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER);
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

        String expected_base = "#version " + getPlatformGLSLVersion() + "\n" +
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

        source = ShaderUtil.Common.compileGLSL("", ShaderDesc.ShaderType.SHADER_TYPE_VERTEX, ShaderDesc.Language.LANGUAGE_GLSL_SM140, true, false);
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
        source = ShaderUtil.Common.compileGLSL(source, ShaderDesc.ShaderType.SHADER_TYPE_VERTEX, ShaderDesc.Language.LANGUAGE_GLSL_SM140, true, false);
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
        source = ShaderUtil.Common.compileGLSL(source, ShaderDesc.ShaderType.SHADER_TYPE_FRAGMENT, ShaderDesc.Language.LANGUAGE_GLES_SM100, true, false);
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
        source = ShaderUtil.Common.compileGLSL(source, ShaderDesc.ShaderType.SHADER_TYPE_FRAGMENT, ShaderDesc.Language.LANGUAGE_GLES_SM300, true, false);
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

        ShaderUtil.ES2ToES3Converter.Result res = ShaderUtil.ES2ToES3Converter.transform(source, ShaderDesc.ShaderType.SHADER_TYPE_FRAGMENT, "", 140, true);

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

        assertTrue(shaderDesc.getShadersCount() > 0);

        ShaderDesc.Shader shader = getShaderByLanguage(shaderDesc, ShaderDesc.Language.LANGUAGE_SPIRV);
        assertNotNull(shader);

        ShaderDesc.ShaderReflection r = shaderDesc.getReflection();

        debugPrintShaderReflection("uniform_buffer_test", r);

        ShaderDesc.ResourceBinding ubo = r.getUniformBuffers(0);
        assertEquals("uniform_buffer", ubo.getName());

        ShaderDesc.ResourceTypeInfo ubo_type = r.getTypes(ubo.getType().getTypeIndex());
        assertEquals("uniform_buffer", ubo_type.getName());

        assertEquals("color_one", ubo_type.getMembers(0).getName());
        assertEquals(ShaderDesc.ShaderDataType.SHADER_TYPE_VEC4, ubo_type.getMembers(0).getType().getShaderType());

        assertEquals("color_two", ubo_type.getMembers(1).getName());
        assertEquals(ShaderDesc.ShaderDataType.SHADER_TYPE_VEC4, ubo_type.getMembers(1).getType().getShaderType());
    }

    @Test
    public void testCompute() throws Exception {
        String source_no_version =
            "layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n" +
            "void main() {}";

        String expected =
            "#version 430\n" +
            source_no_version +
            "\n";

        GetProject().getProjectProperties().putBooleanValue("shader", "output_spirv", true);

        List<Message> outputs = build("/test_compute.cp", source_no_version);
        ShaderDesc shaderDesc = (ShaderDesc) outputs.get(0);

        assertTrue(shaderDesc.getShadersCount() > 0);
        assertNotNull(getShaderByLanguage(shaderDesc, ShaderDesc.Language.LANGUAGE_SPIRV));
        assertEquals(ShaderDesc.ShaderType.SHADER_TYPE_COMPUTE, shaderDesc.getShaderType());

        // Compute not supported for OSX on GL contexts
        if (Platform.getHostPlatform() == Platform.X86_64Win32 || Platform.getHostPlatform() == Platform.X86_64Win32) {
            ShaderDesc.Shader glslShader = getShaderByLanguage(shaderDesc, ShaderDesc.Language.LANGUAGE_GLSL_SM430);
            assertNotNull(glslShader);
            testOutput(expected, glslShader.getSource().toStringUtf8());
        }
    }

    @Test
    public void testStripComments()  {
        String shader_base =
            "#version 430 //Comment0\n" +
            "#ifndef MY_DEFINE //Comment1\n" +
            "//Comment2\n" +
            "#define MY_DEFINE//Comment3\n" +
            "   uniform vec4 my_uniform;\n" +
            "#endif\n" +
            "/*\n" +
            "Comment block\n" +
            "*/\n" +
            "void main(){\n" +
            "   gl_FragColor = vec4(1.0); //Comment3\n" +
            "}\n";

        String expected_base =
            "#version 430 \n" +
            "#ifndef MY_DEFINE \n" +
            "\n" +
            "#define MY_DEFINE\n" +
            "   uniform vec4 my_uniform;\n" +
            "#endif\n" +
            "\n" +
            "\n" +
            "\n" +
            "void main(){\n" +
            "   gl_FragColor = vec4(1.0); \n" +
            "}\n";
        String result = ShaderUtil.Common.stripComments(shader_base);
        testOutput(expected_base, result);
    }

    @Test
    public void testShaderCompilePipelines() throws Exception {
        String shaderNewPipeline =
            """
            #version 140
            uniform my_uniforms
            {
                vec4 tint;
            };
            
            out vec4 color;
            void main()
            {
                color = tint;
            }
            """;

        List<Message> outputs = build("/test_new_pipeline.fp", shaderNewPipeline);
        ShaderDesc shaderDesc = (ShaderDesc) outputs.get(0);
        assertTrue(shaderDesc.getShadersCount() > 0);

        ShaderDesc.ShaderReflection s = shaderDesc.getReflection();

        assertEquals("color", s.getOutputs(0).getName());
        assertEquals("my_uniforms", s.getUniformBuffers(0).getName());
        assertEquals("my_uniforms", s.getTypes(0).getName());
        assertEquals("tint", s.getTypes(0).getMembers(0).getName());

        String shaderLegacyPipeline =
            """
            uniform vec4 tint;
            void main()
            {
                gl_FragColor = tint;
            }
            """;

        outputs = build("/test_old_pipeline.fp", shaderLegacyPipeline);
        shaderDesc = (ShaderDesc) outputs.get(0);
        assertTrue(shaderDesc.getShadersCount() > 0);

        s = shaderDesc.getReflection();
        assertEquals("tint", s.getTypes(0).getMembers(0).getName());
        assertEquals("_DMENGINE_GENERATED_UB_FS_0", s.getUniformBuffers(0).getName());
    }
}
