// Copyright 2020-2026 The Defold Foundation
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
import static org.junit.Assert.assertFalse;
import java.util.List;
import java.util.Objects;

import com.dynamo.bob.util.MurmurHash;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Platform;
import com.dynamo.graphics.proto.Graphics.ShaderDesc;

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
        return ShaderDesc.Language.LANGUAGE_GLSL_SM330;
    }

    private static int getPlatformGLSLVersion() {
        return 330;
    }

    private void checkExpectedLanguages(ShaderDesc shader, ShaderDesc.Language[] expectedLanguages) {
        assertEquals(expectedLanguages.length, shader.getShadersCount());

        boolean found = false;
        for (int i = 0; i < shader.getShadersList().size(); i++) {
            ShaderDesc.Shader shaderDesc = shader.getShaders(i);
            assertNotNull(shaderDesc.getSource());
            for (ShaderDesc.Language expectedLanguage : expectedLanguages) {
                if (shaderDesc.getLanguage() == expectedLanguage) {
                    found = true;
                    break;
                }
            }
        }
        assertTrue(found);
    }

    private void doTest(ShaderDesc.Language[] expectedLanguages, String outputResource) throws Exception {
        // Test GL vp
        ShaderDesc shader = addAndBuildShaderDesc("/test_shader.vp", vp, outputResource);
        checkExpectedLanguages(shader, expectedLanguages);

        // Test GL fp
        shader = addAndBuildShaderDesc("/test_shader.fp", fp, outputResource);
        checkExpectedLanguages(shader, expectedLanguages);
    }

    private void doTestEs3(ShaderDesc.Language[] expectedLanguagesES3, String outputResource) throws Exception {
        ShaderDesc shader = addAndBuildShaderDesc("/test_shader.vp", vpEs3, outputResource);
        checkExpectedLanguages(shader, expectedLanguagesES3);

        shader = addAndBuildShaderDesc("/test_shader.fp", fpEs3, outputResource);
        checkExpectedLanguages(shader, expectedLanguagesES3);
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
        boolean debugPrintReflectionEnabled = false;
        if (!debugPrintReflectionEnabled) {
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
        getProject().getProjectProperties().putBooleanValue("shader", "output_spirv", true);

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

            ShaderDesc shaderDesc = addAndBuildShaderDesc("/reflection_0.fp", fs_src, "/reflection_0.shbundle");

            assertTrue(shaderDesc.getShadersCount() > 0);
            ShaderDesc.Shader shader = getShaderByLanguage(shaderDesc, ShaderDesc.Language.LANGUAGE_SPIRV);
            assertNotNull(shader);

            ShaderDesc.ShaderReflection r = shaderDesc.getReflection();
            assertEquals(1, r.getUniformBuffersCount());

            debugPrintShaderReflection("Reflection Test 0", r);

            // Note: When we don't specify a version and the uniforms are not packed in constant buffers,
            //       we wrap them around generated uniform buffers, which requires us to dig out the type information like this:
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

            ShaderDesc shaderDesc = addAndBuildShaderDesc("/reflection_1.fp", fs_src, "/reflection_1.shbundle");

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

        // Shared shader for split/non-split sampler tests:
        String fs_sampler_type_src =
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


        // Test non-split samplers
        {
            ShaderDesc shaderDesc = addAndBuildShaderDesc("/reflection_2.fp", fs_sampler_type_src, "/reflection_2.shbundle");

            assertTrue(shaderDesc.getShadersCount() > 0);
            ShaderDesc.Shader shader = getShaderByLanguage(shaderDesc, ShaderDesc.Language.LANGUAGE_SPIRV);
            assertNotNull(shader);

            ShaderDesc.ShaderReflection r = shaderDesc.getReflection();
            debugPrintShaderReflection("Reflection Test 2", r);

            // 8 texture units (no split samplers)
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

            // The non-constructed sampler shouldn't have any reference to a texture
            assertFalse(r.getTextures(7).hasSamplerTextureIndex());
        }

        // Note that we rely on using "output_wgsl" here to make sure the samplers are split,
        // but since we only build shaders for the host platform no actual WGSL shaders will be built!
        IShaderCompiler.CompileOptions compileOptions = new IShaderCompiler.CompileOptions();
        compileOptions.forceSplitSamplers = true;

        ShaderProgramBuilderBundle.ModuleBundle modules = createShaderModules(new String[] {"/reflection_3.fp"}, compileOptions);
        ShaderDesc shaderDesc = addAndBuildShaderDescs(modules, new String[] {fs_sampler_type_src}, "/reflection_3.shbundle");

        assertTrue(shaderDesc.getShadersCount() > 0);
        assertNotNull(getShaderByLanguage(shaderDesc, ShaderDesc.Language.LANGUAGE_SPIRV));

        ShaderDesc.ShaderReflection r = shaderDesc.getReflection();
        debugPrintShaderReflection("Reflection Test 3 - Split texture/samplers", r);

        // 8 texture units + 3 samplers
        assertEquals(11, r.getTexturesCount());
        validateResourceBindingWithKnownType(r.getTextures(0), "sampler_2d",                 ShaderDesc.ShaderDataType.SHADER_TYPE_TEXTURE2D);
        validateResourceBindingWithKnownType(r.getTextures(1), "sampler_2d_separated",       ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER);
        validateResourceBindingWithKnownType(r.getTextures(2), "sampler_2d_array",           ShaderDesc.ShaderDataType.SHADER_TYPE_TEXTURE2D_ARRAY);
        validateResourceBindingWithKnownType(r.getTextures(3), "sampler_2d_array_separated", ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER);
        validateResourceBindingWithKnownType(r.getTextures(4), "sampler_cube",               ShaderDesc.ShaderDataType.SHADER_TYPE_TEXTURE_CUBE);
        validateResourceBindingWithKnownType(r.getTextures(5), "sampler_cube_separated",     ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER);

        // Test that the constructed samplers have a valid connection to it's texture unit
        assertTrue(r.getTextures(1).hasSamplerTextureIndex());
        assertEquals(0, r.getTextures(1).getSamplerTextureIndex());
        assertTrue(r.getTextures(3).hasSamplerTextureIndex());
        assertEquals(2, r.getTextures(3).getSamplerTextureIndex());
        assertTrue(r.getTextures(5).hasSamplerTextureIndex());
        assertEquals(4, r.getTextures(5).getSamplerTextureIndex());

        //TODO:
        //validateResourceBindingWithKnownType(shader.getTextures(2), "sampler_buffer",   ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER_);
        validateResourceBindingWithKnownType(r.getTextures(6), "texture_2d",       ShaderDesc.ShaderDataType.SHADER_TYPE_TEXTURE2D);
        validateResourceBindingWithKnownType(r.getTextures(7), "utexture_2d",      ShaderDesc.ShaderDataType.SHADER_TYPE_UTEXTURE2D);
        validateResourceBindingWithKnownType(r.getTextures(8), "uimage_2d",        ShaderDesc.ShaderDataType.SHADER_TYPE_UIMAGE2D);
        validateResourceBindingWithKnownType(r.getTextures(9), "image_2d",         ShaderDesc.ShaderDataType.SHADER_TYPE_IMAGE2D);
        validateResourceBindingWithKnownType(r.getTextures(10), "sampler_name",    ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER);

        // The non-constructed sampler shouldn't have any reference to a texture
        assertFalse(r.getTextures(10).hasSamplerTextureIndex());
    }

    @Test
    public void testShaderPrograms() throws Exception {
        boolean spirvIsDefault = IsSpirvDefault(getProject().getPlatform());

        ShaderDesc.Language firstLanguage = spirvIsDefault ? ShaderDesc.Language.LANGUAGE_SPIRV : getPlatformGLSLLanguage();
        ShaderDesc.Language secondLanguage = spirvIsDefault ? getPlatformGLSLLanguage() : ShaderDesc.Language.LANGUAGE_SPIRV;
        ShaderDesc.Language[] expectedLanguages = new ShaderDesc.Language[] { firstLanguage };

        doTest(expectedLanguages, "/test_shader.shbundle");
        doTestEs3(expectedLanguages, "/test_shader_es3.shbundle");

        expectedLanguages = new ShaderDesc.Language[] { firstLanguage, secondLanguage };

        if (spirvIsDefault) {
            getProject().getProjectProperties().putBooleanValue("shader", "output_glsl", true);
        } else {
            getProject().getProjectProperties().putBooleanValue("shader", "output_spirv", true);
        }
        doTest(expectedLanguages, "/test_shader_secondary.shbundle");
        doTestEs3(expectedLanguages, "/test_shader_secondary_es3.shbundle");
    }

    private boolean IsSpirvDefault(Platform platform) {
        return platform == Platform.Arm64MacOS || platform == Platform.X86_64MacOS;
    }

    private void testOutput(String expected, String source) {
        if (!expected.equals(source)) {
            System.err.printf("EXPECTED:\n'%s'%n", expected);
            System.err.printf("SOURCE:\n'%s'%n", source);
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

        boolean spirvIsDefault = IsSpirvDefault(getProject().getPlatform());
        if (spirvIsDefault) {
            getProject().getProjectProperties().putBooleanValue("shader", "output_glsl", true);
        }

        // Test include a valid shader from the same folder
        {
            ShaderDesc shader = addAndBuildShaderDesc("/test_glsl_same_folder.fp", String.format(shader_base, "glsl_same_folder.glsl"), "/test_glsl_same_folder.shbundle");
            testOutput(String.format(expected_base,
                "const float same_folder = 0.0;\n"),
                    Objects.requireNonNull(getShaderByLanguage(shader, getPlatformGLSLLanguage())).getSource().toStringUtf8());
        }

        // Test include a valid shader from a subfolder
        {
            ShaderDesc shader = addAndBuildShaderDesc("/test_glsl_sub_folder_includes.fp", String.format(shader_base, "shader_includes/glsl_sub_include.glsl"), "/test_glsl_sub_folder_includes.shbundle");
            testOutput(String.format(expected_base,
                "const float sub_include = 0.0;\n"),
                Objects.requireNonNull(getShaderByLanguage(shader, getPlatformGLSLLanguage())).getSource().toStringUtf8());
        }

        // Test include a valid shader from a subfolder that includes other files
        {
            ShaderDesc shader = addAndBuildShaderDesc("/test_glsl_sub_folder_multiple_includes.fp", String.format(shader_base, "shader_includes/glsl_sub_include_multi.glsl"), "/test_glsl_sub_folder_multiple_includes.shbundle");
            testOutput(String.format(expected_base,
                "const float sub_include = 0.0;\n" +
                "\n" +
                "const float sub_include_from_multi = 0.0;\n"),
                Objects.requireNonNull(getShaderByLanguage(shader, getPlatformGLSLLanguage())).getSource().toStringUtf8());
        }

        // Test wrong path
        {
            boolean didFail = false;
            try {
                addAndBuildShaderDesc("/test_glsl_missing.fp", String.format(shader_base, "path-doesnt-exist.glsl"), "/test_glsl_missing.shbundle");
            } catch (CompileExceptionError e) {
                didFail = true;
            }
            assertTrue(didFail);
        }

        // Test path outside of project
        {
            boolean didFail = false;
            try {
                addAndBuildShaderDesc("/test_glsl_outside_of_project.fp", String.format(shader_base, "../path-doesnt-exist.glsl"), "/test_glsl_outside_of_project.shbundle");
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
                addAndBuildShaderDesc("/test_glsl_self_include.fp", String.format(shader_base, "shader_includes/glsl_self_include.glsl"), "/test_glsl_self_include.shbundle");
            } catch (CompileExceptionError e) {
                didFail = true;
            }
            assertTrue(didFail);
        }

        // Test cyclic include
        {
            boolean didFail = false;
            try {
                addAndBuildShaderDesc("/test_glsl_cyclic_include.fp", String.format(shader_base, "shader_includes/glsl_cyclic_include.glsl"), "/test_glsl_cyclic_include.shbundle");
            } catch (CompileExceptionError e) {
                didFail = true;
            }
            assertTrue(didFail);
        }

        if (spirvIsDefault) {
            getProject().getProjectProperties().putBooleanValue("shader", "output_glsl", false);
        }
    }

    @Test
    public void testGlslDirectives() throws Exception {
        String source;
        String expected;

        source = ShaderUtil.Common.compileGLSL("", ShaderDesc.ShaderType.SHADER_TYPE_VERTEX, ShaderDesc.Language.LANGUAGE_GLSL_SM330, true, false, false);
        expected =  "#version 330\n" +
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
        source = ShaderUtil.Common.compileGLSL(source, ShaderDesc.ShaderType.SHADER_TYPE_VERTEX, ShaderDesc.Language.LANGUAGE_GLSL_SM330, true, false, false);
        expected =  "#version 330\n" +
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
        source = ShaderUtil.Common.compileGLSL(source, ShaderDesc.ShaderType.SHADER_TYPE_FRAGMENT, ShaderDesc.Language.LANGUAGE_GLES_SM100, true, false, false);
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
        source = ShaderUtil.Common.compileGLSL(source, ShaderDesc.ShaderType.SHADER_TYPE_FRAGMENT, ShaderDesc.Language.LANGUAGE_GLES_SM300, true, false, false);
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

        ShaderUtil.ES2ToES3Converter.Result res = ShaderUtil.ES2ToES3Converter.transform(source, ShaderDesc.ShaderType.SHADER_TYPE_FRAGMENT, "", 140, true, false);

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

    private static ShaderDesc.Shader getShaderByLanguage(ShaderDesc shaderDesc, ShaderDesc.Language language) {
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

        getProject().getProjectProperties().putBooleanValue("shader", "output_spirv", true);

        ShaderDesc shaderDesc = addAndBuildShaderDesc("/test_shader.vp", vp, "/test_shader.shbundle");

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

        getProject().getProjectProperties().putBooleanValue("shader", "output_spirv", true);

        ShaderDesc shaderDesc = addAndBuildShaderDesc("/test_compute.cp", source_no_version, "/test_compute.shbundle");

        assertTrue(shaderDesc.getShadersCount() > 0);
        assertNotNull(getShaderByLanguage(shaderDesc, ShaderDesc.Language.LANGUAGE_SPIRV));
        assertEquals(ShaderDesc.ShaderType.SHADER_TYPE_COMPUTE, shaderDesc.getShaders(0).getShaderType());

        // Compute not supported for OSX on GL contexts
        if (Platform.getHostPlatform().isWindows()) {
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
    public void testCarriageReturn() throws Exception {
        String src =
            "#version 140 \r\n" +
            "\r\n" +
            "void main(){\r\n" +
            "   gl_FragColor = vec4(1.0); \r\n" +
            "}\r\n";

        ShaderDesc shaderDesc = addAndBuildShaderDesc("/test_shader_cr.fp", src, "/test_shader_cr.shbundle");
        assertTrue(shaderDesc.getShadersCount() > 0);
    }

    @Test
    public void testGenerateErrorsWithProjectPath() throws Exception {
        String src =
                """
                #version wrong-version
                void main(){
                   gl_FragColor = vec4(1.0);
                }
                """;

        boolean didFail = false;
        try {
            addAndBuildShaderDesc("/test_shader_error.fp", src, "/test_shader_error.shbundle");
        } catch (CompileExceptionError e) {
            assertTrue(e.getMessage().contains("/test_shader_error.fp:3"));
            didFail = true;
        }
        assertTrue(didFail);
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

        ShaderDesc shaderDesc = addAndBuildShaderDesc("/test_new_pipeline.fp", shaderNewPipeline, "/test_new_pipeline.shbundle");

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

        shaderDesc = addAndBuildShaderDesc("/test_old_pipeline.fp", shaderLegacyPipeline, "/test_old_pipeline.shbundle");
        assertTrue(shaderDesc.getShadersCount() > 0);

        s = shaderDesc.getReflection();
        assertEquals("tint", s.getTypes(0).getMembers(0).getName());
        assertEquals("_DMENGINE_GENERATED_UB_FS_0", s.getUniformBuffers(0).getName());
    }

    @Test
    public void testNativeReflectionAPI() throws Exception {
        byte[] spvSimple = getFile("simple.spv");
        assertNotNull(spvSimple);

        long ctx = ShadercJni.NewShaderContext(Shaderc.ShaderStage.SHADER_STAGE_FRAGMENT.getValue(), spvSimple);
        Shaderc.ShaderReflection reflection = ShadercJni.GetReflection(ctx);

        assertEquals("FragColor", reflection.outputs[0].name);

        ShadercJni.DeleteShaderContext(ctx);
    }

    static Shaderc.ShaderResource getShaderResourceByName(Shaderc.ShaderResource[] resources, String name) {
        for (Shaderc.ShaderResource res : resources) {
            if (res.name.equals(name)) {
                return res;
            }
        }
        return null;
    }

    @Test
    public void testNativeReflectionAPIRemapBindings() throws Exception {
        byte[] spvBindings = getFile("bindings.spv");
        assertNotNull(spvBindings);

        long ctx = ShadercJni.NewShaderContext(Shaderc.ShaderStage.SHADER_STAGE_VERTEX.getValue(), spvBindings);
        Shaderc.ShaderReflection reflection = ShadercJni.GetReflection(ctx);

        Shaderc.ShaderResource res_position = getShaderResourceByName(reflection.inputs, "position");
        assertNotNull(res_position);
        Shaderc.ShaderResource res_normal = getShaderResourceByName(reflection.inputs, "normal");
        assertNotNull(res_normal);
        Shaderc.ShaderResource res_tex_coord = getShaderResourceByName(reflection.inputs, "tex_coord");
        assertNotNull(res_tex_coord);

        assertEquals(0, res_position.location);
        assertEquals(1, res_normal.location);
        assertEquals(2, res_tex_coord.location);

        long spvCompiler = ShadercJni.NewShaderCompiler(ctx, Shaderc.ShaderLanguage.SHADER_LANGUAGE_SPIRV.getValue());
        ShadercJni.SetResourceLocation(ctx, spvCompiler, MurmurHash.hash64("position"), 3);
        ShadercJni.SetResourceLocation(ctx, spvCompiler, MurmurHash.hash64("normal"), 4);
        ShadercJni.SetResourceLocation(ctx, spvCompiler, MurmurHash.hash64("tex_coord"), 5);

        Shaderc.ShaderCompilerOptions opts = new Shaderc.ShaderCompilerOptions();
        opts.entryPoint = "no-entry-point"; // JNI will crash if this is null!

        Shaderc.ShaderCompileResult spvCompiledResult = ShadercJni.Compile(ctx, spvCompiler, opts);
        long spvCompiledCtx = ShadercJni.NewShaderContext(Shaderc.ShaderStage.SHADER_STAGE_VERTEX.getValue(), spvCompiledResult.data);
        Shaderc.ShaderReflection spvCompiledReflection = ShadercJni.GetReflection(spvCompiledCtx);

        res_position = getShaderResourceByName(spvCompiledReflection.inputs, "position");
        assertNotNull(res_position);
        res_normal = getShaderResourceByName(spvCompiledReflection.inputs, "normal");
        assertNotNull(res_normal);
        res_tex_coord = getShaderResourceByName(spvCompiledReflection.inputs, "tex_coord");
        assertNotNull(res_tex_coord);

        assertEquals(3, res_position.location);
        assertEquals(4, res_normal.location);
        assertEquals(5, res_tex_coord.location);

        ShadercJni.DeleteShaderContext(ctx);
    }
}
