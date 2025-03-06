// Copyright 2020-2025 The Defold Foundation
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

import static com.dynamo.bob.pipeline.ShaderProgramBuilder.resourceTypeToShaderDataType;
import static org.junit.Assert.*;

import java.io.IOException;
import java.util.ArrayList;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.pipeline.shader.ShaderCompilePipelineLegacy;
import com.dynamo.bob.util.Exec;
import org.junit.Test;

import com.dynamo.bob.pipeline.shader.ShaderCompilePipeline;
import com.dynamo.bob.pipeline.shader.SPIRVReflector;
import com.dynamo.graphics.proto.Graphics.ShaderDesc;

public class ShaderCompilePipelineTest {

    ArrayList<ShaderDesc.Language> allLanguages = new ArrayList<>();

    public ShaderCompilePipelineTest() {
        allLanguages.add(ShaderDesc.Language.LANGUAGE_GLES_SM100);
        allLanguages.add(ShaderDesc.Language.LANGUAGE_GLES_SM300);
        allLanguages.add(ShaderDesc.Language.LANGUAGE_GLSL_SM120);
        allLanguages.add(ShaderDesc.Language.LANGUAGE_GLSL_SM330);
        allLanguages.add(ShaderDesc.Language.LANGUAGE_GLSL_SM430);
        allLanguages.add(ShaderDesc.Language.LANGUAGE_SPIRV);
    }

    @Test
    public void testSimple() throws Exception {

        String vsShader =
                """
                #version 140
                in vec4 position;
                uniform uniforms
                {
                    mat4 view;
                    mat4 proj;
                };
                void main() {
                    gl_Position = proj * view * position;
                }
                """;

        ShaderCompilePipeline.ShaderModuleDesc vsDesc = new ShaderCompilePipeline.ShaderModuleDesc();
        vsDesc.type = ShaderDesc.ShaderType.SHADER_TYPE_VERTEX;
        vsDesc.source = vsShader;

        ShaderCompilePipeline pipelineVertex = new ShaderCompilePipeline("testSimpleVertex");
        ShaderCompilePipeline.createShaderPipeline(pipelineVertex, vsDesc, new ShaderCompilePipeline.Options());

        SPIRVReflector reflector                  = pipelineVertex.getReflectionData(ShaderDesc.ShaderType.SHADER_TYPE_VERTEX);
        ArrayList<Shaderc.ShaderResource> inputs  = reflector.getInputs();
        ArrayList<Shaderc.ShaderResource> ubos    = reflector.getUBOs();
        ArrayList<Shaderc.ResourceTypeInfo> types = reflector.getTypes();

        assertEquals("position", inputs.get(0).name);
        assertEquals("uniforms", ubos.get(0).name);
        assertEquals("uniforms", types.get(0).name);

        assertEquals("view", types.get(0).members[0].name);
        assertEquals(ShaderDesc.ShaderDataType.SHADER_TYPE_MAT4, resourceTypeToShaderDataType(types.get(0).members[0].type));

        assertEquals("proj", types.get(0).members[1].name);
        assertEquals(ShaderDesc.ShaderDataType.SHADER_TYPE_MAT4, resourceTypeToShaderDataType(types.get(0).members[1].type));

        for (ShaderDesc.Language l : allLanguages) {
            pipelineVertex.crossCompile(ShaderDesc.ShaderType.SHADER_TYPE_VERTEX, l);
        }

        ShaderCompilePipeline.destroyShaderPipeline(pipelineVertex);

        String fsShader =
                """
                #version 140
                out vec4 color;
                uniform fs_uniforms
                {
                    vec4 tint;
                    float value;
                    vec4 value_array[4];
                    mat4 value_mat;
                };
                void main() {
                    color = vec4(1.0) + tint + value_array[2];
                }
                """;
        ShaderCompilePipeline.ShaderModuleDesc fsDesc = new ShaderCompilePipeline.ShaderModuleDesc();
        fsDesc.source = fsShader;
        fsDesc.type = ShaderDesc.ShaderType.SHADER_TYPE_FRAGMENT;

        ShaderCompilePipeline pipelineFragment = new ShaderCompilePipeline("testSimpleFragment");
        ShaderCompilePipeline.createShaderPipeline(pipelineFragment, fsDesc, new ShaderCompilePipeline.Options());

        reflector = pipelineFragment.getReflectionData(ShaderDesc.ShaderType.SHADER_TYPE_FRAGMENT);
        ubos      = reflector.getUBOs();
        types     = reflector.getTypes();

        assertEquals("fs_uniforms", ubos.get(0).name);
        assertEquals("fs_uniforms", types.get(0).name);

        assertEquals("tint", types.get(0).members[0].name);
        assertEquals(ShaderDesc.ShaderDataType.SHADER_TYPE_VEC4, resourceTypeToShaderDataType(types.get(0).members[0].type));

        assertEquals("value", types.get(0).members[1].name);
        assertEquals(ShaderDesc.ShaderDataType.SHADER_TYPE_FLOAT, resourceTypeToShaderDataType(types.get(0).members[1].type));

        assertEquals("value_array", types.get(0).members[2].name);
        assertEquals(ShaderDesc.ShaderDataType.SHADER_TYPE_VEC4, resourceTypeToShaderDataType(types.get(0).members[2].type));
        assertEquals(4, types.get(0).members[2].type.arraySize);

        assertEquals("value_mat", types.get(0).members[3].name);
        assertEquals(ShaderDesc.ShaderDataType.SHADER_TYPE_MAT4, resourceTypeToShaderDataType(types.get(0).members[3].type));

        for (ShaderDesc.Language l : allLanguages) {
            pipelineFragment.crossCompile(ShaderDesc.ShaderType.SHADER_TYPE_FRAGMENT, l);
        }

        ShaderCompilePipeline.destroyShaderPipeline(pipelineFragment);
    }

    @Test
    public void testFailingCrossCompilation() throws IOException, CompileExceptionError {
        String fsShader =
                """
                #version 140
                uniform sampler2D sampler_2d;
                out vec4 color;
                void main()
                {
                    ivec2 sampler_size = textureSize(sampler_2d, 0);
                    color = vec4( float(sampler_size.x), float(sampler_size.y), 0, 1);
                }
                """;

        ShaderCompilePipeline.ShaderModuleDesc fsDesc = new ShaderCompilePipeline.ShaderModuleDesc();
        fsDesc.source = fsShader;
        fsDesc.type = ShaderDesc.ShaderType.SHADER_TYPE_FRAGMENT;

        ShaderCompilePipeline pipelineFragment = new ShaderCompilePipeline("testFragment");
        ShaderCompilePipeline.createShaderPipeline(pipelineFragment, fsDesc, new ShaderCompilePipeline.Options());

        boolean didException = false;
        try {
            byte[] bytes = pipelineFragment.crossCompile(fsDesc.type, ShaderDesc.Language.LANGUAGE_GLES_SM100);
        } catch (CompileExceptionError e) {
            didException = true;
        }
        assertTrue(didException);
        ShaderCompilePipeline.destroyShaderPipeline(pipelineFragment);
    }

    @Test
    public void testUnusedResources() throws Exception {
        String fsShader =
                """
                #version 140
                in vec4 in_used;
                in vec4 in_unused_1;
                in vec4 in_unused_2;
                in vec4 in_unused_3;
                out vec4 color;
                out vec4 color2;
                uniform fs_uniforms
                {
                    vec4 tint;
                };
                uniform fs_unused
                {
                    vec4 tint_unused;
                };
                void main() {
                    color = vec4(1.0) + tint + in_used;
                }
                """;

        ShaderCompilePipeline.ShaderModuleDesc fsDesc = new ShaderCompilePipeline.ShaderModuleDesc();
        fsDesc.source = fsShader;
        fsDesc.type = ShaderDesc.ShaderType.SHADER_TYPE_FRAGMENT;

        ShaderCompilePipeline pipelineFragment = new ShaderCompilePipeline("testFragment");
        ShaderCompilePipeline.createShaderPipeline(pipelineFragment, fsDesc, new ShaderCompilePipeline.Options());

        SPIRVReflector reflector                    = pipelineFragment.getReflectionData(ShaderDesc.ShaderType.SHADER_TYPE_FRAGMENT);
        ArrayList<Shaderc.ShaderResource> inputs    = reflector.getInputs();
        ArrayList<Shaderc.ShaderResource> outputs   = reflector.getOutputs();
        ArrayList<Shaderc.ShaderResource> ubos      = reflector.getUBOs();

        assertEquals(1, inputs.size());
        // Outputs are not optimized away, even if they are not written to by the shader
        assertEquals(2, outputs.size());
        assertEquals(1, ubos.size());
        assertEquals("fs_uniforms", ubos.get(0).name);

        ShaderCompilePipeline.destroyShaderPipeline(pipelineFragment);

        // Do the same test, but with the legacy pipeline
        String fsShaderLegacy =
                """
                varying vec4 in_used;
                varying vec4 in_unused_1;
                varying vec4 in_unused_2;
                varying vec4 in_unused_3;
                uniform vec4 tint;
                uniform vec4 uniform_unused;
                void main() {
                    gl_FragColor = vec4(1.0) + tint + in_used;
                }
                """;

        ShaderCompilePipeline.ShaderModuleDesc fsDescLegacy = new ShaderCompilePipeline.ShaderModuleDesc();
        fsDescLegacy.source = fsShaderLegacy;
        fsDescLegacy.type = ShaderDesc.ShaderType.SHADER_TYPE_FRAGMENT;

        ShaderCompilePipelineLegacy pipelineFragmentLegacy = new ShaderCompilePipelineLegacy("testFragment");
        ShaderCompilePipeline.createShaderPipeline(pipelineFragmentLegacy, fsDescLegacy, new ShaderCompilePipeline.Options());

        reflector = pipelineFragmentLegacy.getReflectionData(ShaderDesc.ShaderType.SHADER_TYPE_FRAGMENT);
        inputs    = reflector.getInputs();
        outputs   = reflector.getOutputs();
        ubos      = reflector.getUBOs();

        ArrayList<Shaderc.ResourceTypeInfo> types = reflector.getTypes();

        assertEquals(1, inputs.size());
        assertEquals(1, outputs.size());
        assertEquals(1, ubos.size());
        assertEquals("_DMENGINE_GENERATED_UB_FS_0", ubos.get(0).name);
        assertEquals("tint", types.get(0).members[0].name);

        ShaderCompilePipeline.destroyShaderPipeline(pipelineFragmentLegacy);
    }
}
