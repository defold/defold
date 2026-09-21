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

import com.dynamo.bob.pipeline.shader.ShaderCompilePipeline;
import com.dynamo.graphics.proto.Graphics.ShaderDesc;
import org.junit.Test;

import static org.junit.Assert.*;

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
        for (ShaderDesc.Language language : new ShaderDesc.Language[]{ShaderDesc.Language.LANGUAGE_GLSL_SM120, ShaderDesc.Language.LANGUAGE_GLSL_SM330}) {
            var result = compile(VERTEX, ShaderDesc.ShaderType.SHADER_TYPE_VERTEX, language);
            assertTrue(result.source.contains(language == ShaderDesc.Language.LANGUAGE_GLSL_SM330 ? "#version 330" : "#version 120"));
            assertFalse(result.source, result.source.matches("(?s).*uniform\\s+\\w+\\s*\\{.*"));
            assertTrue(result.source, result.source.contains("uniform vertex_uniforms"));
            assertTrue(result.source.contains("offsets[2]"));
            var input = result.reflector.getInputs().get(0);
            assertEquals("position", input.name);
            assertEquals(0, input.location);
            assertEquals(ShaderDesc.ShaderDataType.SHADER_TYPE_VEC4, ShaderProgramBuilder.resourceTypeToShaderDataType(input.type));
            var ubo = result.reflector.getUBOs().get(0);
            assertTrue(result.source.contains("_" + ubo.id));
            var members = result.reflector.getTypes().stream().filter(t -> t.name.equals("vertex_uniforms")).findFirst().get().members;
            assertEquals("view_proj", members[0].name);
            assertEquals(ShaderDesc.ShaderDataType.SHADER_TYPE_MAT4, ShaderProgramBuilder.resourceTypeToShaderDataType(members[0].type));
            assertEquals("offsets", members[1].name);
            assertEquals(2, members[1].type.arraySize);
        }
    }

    @Test
    public void runtimeSm330RetainsUniformBlocks() throws Exception {
        var module = new ShaderCompilePipeline.ShaderModuleDesc();
        module.source = VERTEX;
        module.type = ShaderDesc.ShaderType.SHADER_TYPE_VERTEX;
        var pipeline = new ShaderCompilePipeline("runtime-sm330");
        try {
            ShaderCompilePipeline.createShaderPipeline(pipeline, module, new ShaderCompilePipeline.Options());
            String source = new String(pipeline.crossCompile(module.type, ShaderDesc.Language.LANGUAGE_GLSL_SM330).data);
            assertTrue(source.matches("(?s).*uniform\\s+vertex_uniforms\\s*\\{.*"));
        } finally {
            ShaderCompilePipeline.destroyShaderPipeline(pipeline);
        }
    }

    @Test
    public void pagedSamplersUseTargetAppropriateLookups() throws Exception {
        String fragment = """
            #version 140
            uniform sampler2DArray pages;
            uniform sampler2D ordinary;
            in vec3 uv;
            out vec4 color;
            void main() { color = texture(pages, uv) * texture(ordinary, uv.xy); }
            """;
        for (ShaderDesc.Language language : new ShaderDesc.Language[]{ShaderDesc.Language.LANGUAGE_GLSL_SM120, ShaderDesc.Language.LANGUAGE_GLSL_SM330}) {
            var result = compile(fragment, ShaderDesc.ShaderType.SHADER_TYPE_FRAGMENT, language);
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
        var result = compile("attribute vec4 position;\nuniform mat4 view_proj;\nvoid main() { gl_Position = view_proj * position; }",
            ShaderDesc.ShaderType.SHADER_TYPE_VERTEX, ShaderDesc.Language.LANGUAGE_GLSL_SM330);
        assertTrue(result.source.contains("#version 330"));
        assertTrue(result.source.contains("in vec4 position"));
        assertTrue(result.source.contains("uniform mat4 view_proj"));
        assertFalse(result.source.contains("attribute "));
    }
}
