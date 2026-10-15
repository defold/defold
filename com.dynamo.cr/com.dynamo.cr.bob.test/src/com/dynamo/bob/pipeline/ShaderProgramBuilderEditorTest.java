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

import com.dynamo.bob.Platform;
import com.dynamo.bob.pipeline.shader.ShaderCompilePipeline;
import com.dynamo.graphics.proto.Graphics.ShaderDesc;
import org.junit.Test;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assume.assumeTrue;

public class ShaderProgramBuilderEditorTest {
    private static final String VERTEX = """
        #version 140
        in vec4 position;
        uniform vertex_uniforms {
            mat4 view_proj;
            vec4 offsets[2];
        };
        void main() {
            gl_Position = view_proj * position + offsets[0] + offsets[1];
        }
        """;

    private static ShaderUtil.Common.GLSLCompileResult compile(String source, ShaderDesc.ShaderType stage, ShaderDesc.Language language) throws Exception {
        return ShaderProgramBuilderEditor.buildGLSLVariantTextureArray("editor-preview", source, stage, language, 2,
            Shaderc.ShaderPrecision.SHADER_PRECISION_MEDIUMP, Shaderc.ShaderPrecision.SHADER_PRECISION_HIGHP);
    }

    @Test
    public void plainUniformsAndReflection() throws Exception {
        // Both editor targets emit plain uniforms while preserving attribute and UBO reflection.
        for (ShaderDesc.Language language : new ShaderDesc.Language[]{ShaderDesc.Language.LANGUAGE_GLSL_SM120, ShaderDesc.Language.LANGUAGE_GLSL_SM330}) {
            ShaderUtil.Common.GLSLCompileResult result = compile(VERTEX, ShaderDesc.ShaderType.SHADER_TYPE_VERTEX, language);
            assertTrue(result.source.contains(language == ShaderDesc.Language.LANGUAGE_GLSL_SM330 ? "#version 330" : "#version 120"));
            assertFalse(result.source, result.source.matches("(?s).*uniform\\s+\\w+\\s*\\{.*"));
            assertTrue(result.source, result.source.contains("uniform vertex_uniforms"));
            assertTrue(result.source.contains("offsets[2]"));
            Shaderc.ShaderResource input = result.reflector.getInputs().get(0);
            assertEquals("position", input.name);
            assertEquals(0, input.location);
            assertEquals(ShaderDesc.ShaderDataType.SHADER_TYPE_VEC4, ShaderProgramBuilder.resourceTypeToShaderDataType(input.type));
            Shaderc.ShaderResource ubo = result.reflector.getUBOs().get(0);
            assertTrue(result.source.contains("_" + ubo.id));
            Shaderc.ResourceMember[] members = result.reflector.getTypes().stream().filter(t -> t.name.equals("vertex_uniforms")).findFirst().get().members;
            assertEquals("view_proj", members[0].name);
            assertEquals(ShaderDesc.ShaderDataType.SHADER_TYPE_MAT4, ShaderProgramBuilder.resourceTypeToShaderDataType(members[0].type));
            assertEquals("offsets", members[1].name);
            assertEquals(2, members[1].type.arraySize);
        }
    }

    @Test
    public void runtimeSm330RetainsUniformBlocks() throws Exception {
        // Runtime SM330 compilation retains UBOs unless plain uniforms are explicitly requested.
        ShaderCompilePipeline.ShaderModuleDesc module = new ShaderCompilePipeline.ShaderModuleDesc();
        module.source = VERTEX;
        module.type = ShaderDesc.ShaderType.SHADER_TYPE_VERTEX;
        ShaderCompilePipeline pipeline = new ShaderCompilePipeline("runtime-sm330");
        try {
            ShaderCompilePipeline.createShaderPipeline(pipeline, module, new ShaderCompilePipeline.Options());
            String source = new String(pipeline.crossCompile(module.type, ShaderDesc.Language.LANGUAGE_GLSL_SM330, null, false).data);
            assertTrue(source.matches("(?s).*uniform\\s+vertex_uniforms\\s*\\{.*"));
        } finally {
            ShaderCompilePipeline.destroyShaderPipeline(pipeline);
        }
    }

    @Test
    public void pagedSamplersUseTargetAppropriateLookups() throws Exception {
        // Paged samplers use separate 2D textures and the lookup syntax of the selected target.
        String fragment = """
            #version 140
            uniform sampler2DArray pages;
            uniform sampler2D ordinary;
            in vec3 uv;
            out vec4 color;
            void main() { color = texture(pages, uv) * texture(ordinary, uv.xy); }
            """;
        for (ShaderDesc.Language language : new ShaderDesc.Language[]{ShaderDesc.Language.LANGUAGE_GLSL_SM120, ShaderDesc.Language.LANGUAGE_GLSL_SM330}) {
            ShaderUtil.Common.GLSLCompileResult result = compile(fragment, ShaderDesc.ShaderType.SHADER_TYPE_FRAGMENT, language);
            assertArrayEquals(new String[]{"pages"}, result.arraySamplers);
            assertFalse(result.source.contains("sampler2DArray"));
            assertTrue(result.source, result.source.contains("sampler2D pages_0;"));
            assertTrue(result.source, result.source.contains("sampler2D pages_1;"));
            assertTrue(result.source, result.source.matches("(?s).*texture2DArray_pages\\(\\s*uv\\).*"));
            String lookup = language == ShaderDesc.Language.LANGUAGE_GLSL_SM330 ? "texture" : "texture2D";
            assertTrue(result.source.contains(lookup + "(pages_0,"));
            assertTrue(result.source.contains(lookup + "(ordinary,"));
            if (language == ShaderDesc.Language.LANGUAGE_GLSL_SM330) {
                assertFalse(result.source.contains("texture2D("));
                assertFalse(result.source.contains("gl_FragColor"));
            }
        }
    }

    @Test
    public void legacyProjectShaderCanTargetSm330() throws Exception {
        // Legacy project shader syntax is converted to valid SM330 inputs and plain uniforms.
        String vertex = """
            attribute vec4 position;
            uniform mat4 view_proj;
            void main() {
                gl_Position = view_proj * position;
            }
            """;
        ShaderUtil.Common.GLSLCompileResult result = compile(vertex,
            ShaderDesc.ShaderType.SHADER_TYPE_VERTEX, ShaderDesc.Language.LANGUAGE_GLSL_SM330);
        assertTrue(result.source.contains("#version 330"));
        assertTrue(result.source.contains("in vec4 position"));
        assertTrue(result.source.contains("uniform mat4 view_proj"));
        assertFalse(result.source.contains("attribute "));
    }

    // Verifies storage-buffer compute shaders produce DX12 bytecode and a root signature,
    // guarding against omitted buffer bindings and malformed root-signature separators.
    @Test
    public void runtimeDx12CompilesStorageBuffer() throws Exception {
        assumeTrue(Platform.getHostPlatform() == Platform.X86_64Win32);
        ShaderCompilePipeline.ShaderModuleDesc compute = new ShaderCompilePipeline.ShaderModuleDesc();
        compute.type = ShaderDesc.ShaderType.SHADER_TYPE_COMPUTE;
        compute.resourcePath = "/storage.cp";
        compute.source = """
            #version 430
            layout(local_size_x = 1) in;
            layout(std430, binding = 0) buffer Output { uint value; };
            void main() { value = gl_GlobalInvocationID.x; }
            """;
        assertDx12RuntimeShaders(new ShaderCompilePipeline.ShaderModuleDesc[] { compute }, 1);
    }

    // Verifies both graphics stages produce DX12 bytecode and a root signature even when
    // supplied in reverse order, guarding against unusable editor-built DX12 shaders.
    @Test
    public void runtimeDx12IncludesBytecodeAndRootSignature() throws Exception {
        assumeTrue(Platform.getHostPlatform() == Platform.X86_64Win32);
        ShaderCompilePipeline.ShaderModuleDesc vertex = new ShaderCompilePipeline.ShaderModuleDesc();
        vertex.type = ShaderDesc.ShaderType.SHADER_TYPE_VERTEX;
        vertex.resourcePath = "/dx12.vp";
        vertex.source = VERTEX;
        ShaderCompilePipeline.ShaderModuleDesc fragment = new ShaderCompilePipeline.ShaderModuleDesc();
        fragment.type = ShaderDesc.ShaderType.SHADER_TYPE_FRAGMENT;
        fragment.resourcePath = "/dx12.fp";
        fragment.source = """
            #version 140
            uniform fragment_uniforms { vec4 tint; };
            out vec4 color;
            void main() { color = tint; }
            """;
        assertDx12RuntimeShaders(new ShaderCompilePipeline.ShaderModuleDesc[] { fragment, vertex }, 2);
    }

    // Verifies compute shaders compile for DX12 with the full editor language list,
    // guarding against attempts to compile compute shaders for graphics-only languages.
    @Test
    public void runtimeDx12ComputeFiltersGraphicsOnlyLanguages() throws Exception {
        assumeTrue(Platform.getHostPlatform() == Platform.X86_64Win32);
        ShaderCompilePipeline.ShaderModuleDesc compute = new ShaderCompilePipeline.ShaderModuleDesc();
        compute.type = ShaderDesc.ShaderType.SHADER_TYPE_COMPUTE;
        compute.resourcePath = "/dx12.cp";
        compute.source = """
            #version 430
            layout(local_size_x = 1) in;
            layout(rgba32f, binding = 0) uniform image2D output_image;
            void main() { imageStore(output_image, ivec2(gl_GlobalInvocationID.xy), vec4(1.0)); }
            """;
        assertDx12RuntimeShaders(new ShaderCompilePipeline.ShaderModuleDesc[] { compute }, 1);
    }

    private static void assertDx12RuntimeShaders(ShaderCompilePipeline.ShaderModuleDesc[] modules, int stageCount) throws Exception {
        ShaderDesc.Language[] languages = {
            ShaderDesc.Language.LANGUAGE_GLSL_SM330,
            ShaderDesc.Language.LANGUAGE_GLES_SM300,
            ShaderDesc.Language.LANGUAGE_GLES_SM100,
            ShaderDesc.Language.LANGUAGE_GLSL_SM430,
            ShaderDesc.Language.LANGUAGE_SPIRV,
            ShaderDesc.Language.LANGUAGE_MSL_22,
            ShaderDesc.Language.LANGUAGE_HLSL_51
        };
        ShaderProgramBuilder.ShaderDescBuildResult result = ShaderProgramBuilderEditor.makeShaderDescWithVariants(
            "/dx12.spc", modules, languages, 0,
            Shaderc.ShaderPrecision.SHADER_PRECISION_MEDIUMP, Shaderc.ShaderPrecision.SHADER_PRECISION_HIGHP);
        assertNotNull(java.util.Arrays.toString(result.buildWarnings), result.shaderDesc);
        assertFalse(result.shaderDesc.getHlslRootSignature().isEmpty());
        int hlslCount = 0;
        int spirvCount = 0;
        for (ShaderDesc.Shader shader : result.shaderDesc.getShadersList()) {
            if (shader.getLanguage() == ShaderDesc.Language.LANGUAGE_HLSL_51) {
                ++hlslCount;
                assertTrue(shader.getSource().size() > 4);
                assertEquals("DXBC", shader.getSource().substring(0, 4).toStringUtf8());
            } else if (shader.getLanguage() == ShaderDesc.Language.LANGUAGE_SPIRV) {
                ++spirvCount;
            }
        }
        assertEquals(stageCount, hlslCount);
        assertEquals(stageCount, spirvCount);
    }
}
