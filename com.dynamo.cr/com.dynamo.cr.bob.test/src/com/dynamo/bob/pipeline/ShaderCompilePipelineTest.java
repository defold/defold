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

import static com.dynamo.bob.pipeline.ShaderProgramBuilder.resourceTypeToShaderDataType;
import static org.junit.Assert.*;

import java.io.IOException;
import java.util.ArrayList;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.pipeline.shader.ShaderCompilePipelineLegacy;
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

    private ArrayList<ShaderCompilePipeline.ShaderModuleDesc> toShaderDescs(String vsShader, String fsShader) {
        ShaderCompilePipeline.ShaderModuleDesc vsDesc = new ShaderCompilePipeline.ShaderModuleDesc();
        vsDesc.source = vsShader;
        vsDesc.type = ShaderDesc.ShaderType.SHADER_TYPE_VERTEX;

        ShaderCompilePipeline.ShaderModuleDesc fsDesc = new ShaderCompilePipeline.ShaderModuleDesc();
        fsDesc.source = fsShader;
        fsDesc.type = ShaderDesc.ShaderType.SHADER_TYPE_FRAGMENT;

        ArrayList<ShaderCompilePipeline.ShaderModuleDesc> shaderModuleDescs = new ArrayList<>();
        shaderModuleDescs.add(vsDesc);
        shaderModuleDescs.add(fsDesc);
        return shaderModuleDescs;
    }

    @Test
    public void testRemapResourceBindings() throws Exception {
        String vsShader =
                """
                #version 140
                in highp vec4 position;
                in mediump vec2 texcoord0;
                out mediump vec2 var_texcoord0;
                uniform vs_uniforms { highp mat4 view_proj; };
                uniform shared_uniforms { vec4 tint; };  
                uniform sampler2D shared_texture;
                void main()
                {
                    vec4 shared_color = texture(shared_texture, vec2(0.0));
                    gl_Position = view_proj * vec4(position.xyz + tint.x + shared_color.x, 1.0);
                    var_texcoord0 = texcoord0;
                }
                """;

        String fsShader =
                """
                #version 140
                in mediump vec2 var_texcoord0;
                out vec4 out_fragColor;
                uniform shared_uniforms { vec4 tint; };
                uniform sampler2D shared_texture;
                void main()
                {
                    out_fragColor = tint + texture(shared_texture, var_texcoord0.st);
                }
                """;

        ArrayList<ShaderCompilePipeline.ShaderModuleDesc> shaderModuleDescs = toShaderDescs(vsShader, fsShader);

        ShaderCompilePipeline pipeline = new ShaderCompilePipeline("testRemapping");
        ShaderCompilePipeline.createShaderPipeline(pipeline, shaderModuleDescs, new ShaderCompilePipeline.Options());

        SPIRVReflector reflectorVs = pipeline.getReflectionData(ShaderDesc.ShaderType.SHADER_TYPE_VERTEX);
        SPIRVReflector reflectorFs = pipeline.getReflectionData(ShaderDesc.ShaderType.SHADER_TYPE_FRAGMENT);

        // "shared_uniforms" have the same name and type, and can thus be merged. This means that
        // the resource will NOT be available from the reflection data in the fragment shader.
        Shaderc.ShaderResource sharedUBOvs = getShaderResource(reflectorVs, "shared_uniforms");
        assertNotNull(sharedUBOvs);
        Shaderc.ShaderResource sharedUBOfs = getShaderResource(reflectorFs, "shared_uniforms");
        assertNull(sharedUBOfs);

        // Shared resources have "visibility" in both modules, meaning that the shader stage flags
        // have both fragment and vertex stage flags.
        int combinedShaderStages = Shaderc.ShaderStage.SHADER_STAGE_VERTEX.getValue() + Shaderc.ShaderStage.SHADER_STAGE_FRAGMENT.getValue();
        assertEquals(combinedShaderStages,sharedUBOvs.stageFlags);

        Shaderc.ShaderResource sharedTextureVs = getShaderResource(reflectorVs, "shared_texture");
        assertNotNull(sharedTextureVs);
        Shaderc.ShaderResource sharedTextureFs = getShaderResource(reflectorFs, "shared_texture");
        assertNull(sharedTextureFs);

        assertEquals(combinedShaderStages, sharedTextureVs.stageFlags);

        ShaderCompilePipeline.destroyShaderPipeline(pipeline);
    }

    @Test
    public void testAreTypesEqual() throws IOException, CompileExceptionError {
        String vsShader =
                """
                #version 430
                in highp vec4 position;
                struct nested
                {
                    float nested;
                };
                uniform equal
                {
                    float value;
                    nested value_nested;
                };
                uniform not_equal
                {
                    vec4 tint;
                    vec4 tint_not_in_fs;
                };
                struct not_equal_nested
                {
                    float nested;
                    float not_in_fs;
                };
                uniform not_equal_two
                {
                    not_equal_nested value_nested_two;
                };
                void main()
                {
                    gl_Position = vec4(position.xyz + tint.xyz + tint_not_in_fs.xyz, 1.0 + value + value_nested_two.nested);
                }
                """;

        String fsShader =
                """
                #version 430
                out vec4 out_fragColor;
               
                struct nested
                {
                    float nested;
                };
                uniform equal
                {
                    float value;
                    nested value_nested;
                };
                uniform not_equal
                {
                    vec4 tint;
                };
                struct not_equal_nested
                {
                    float nested;
                };
                uniform not_equal_two
                {
                    not_equal_nested value_nested_two;
                };
                void main()
                {
                    out_fragColor = tint;
                    out_fragColor.a = value + value_nested_two.nested;
                }
                """;

        ArrayList<ShaderCompilePipeline.ShaderModuleDesc> shaderModuleDescs = toShaderDescs(vsShader, fsShader);

        ShaderCompilePipeline pipeline = new ShaderCompilePipeline("testCompareTypes");
        ShaderCompilePipeline.createShaderPipeline(pipeline, shaderModuleDescs, new ShaderCompilePipeline.Options());

        SPIRVReflector reflectorVs = pipeline.getReflectionData(ShaderDesc.ShaderType.SHADER_TYPE_VERTEX);
        SPIRVReflector reflectorFs = pipeline.getReflectionData(ShaderDesc.ShaderType.SHADER_TYPE_FRAGMENT);

        assertTrue(SPIRVReflector.AreResourceTypesEqual(reflectorVs, reflectorFs, "equal"));
        assertFalse(SPIRVReflector.AreResourceTypesEqual(reflectorVs, reflectorFs, "not_equal"));
        assertFalse(SPIRVReflector.AreResourceTypesEqual(reflectorVs, reflectorFs, "not_equal_two"));

        // The "equal" ubos should be merged, which means that it will only be part of the VS reflection and not the FS
        Shaderc.ShaderResource equalUboA = getShaderResource(reflectorVs, "equal");
        Shaderc.ShaderResource equalUboB = getShaderResource(reflectorFs, "equal");
        assert equalUboA != null;
        assert equalUboB == null;

        int combinedShaderStages = Shaderc.ShaderStage.SHADER_STAGE_VERTEX.getValue() + Shaderc.ShaderStage.SHADER_STAGE_FRAGMENT.getValue();
        assertEquals(combinedShaderStages, equalUboA.stageFlags);
    }

    private Shaderc.ShaderResource getShaderResource(SPIRVReflector reflector, String name) {
        for (Shaderc.ShaderResource input : reflector.getInputs()) {
            if (input.name.equals(name))
                return input;
        }
        for (Shaderc.ShaderResource output : reflector.getOutputs()) {
            if (output.name.equals(name))
                return output;
        }
        for (Shaderc.ShaderResource ubo : reflector.getUBOs()) {
            if (ubo.name.equals(name))
                return ubo;
        }
        for (Shaderc.ShaderResource tex : reflector.getTextures()) {
            if (tex.name.equals(name))
                return tex;
        }
        for (Shaderc.ShaderResource ssbo : reflector.getSsbos()) {
            if (ssbo.name.equals(name))
                return ssbo;
        }
        return null;
    }

    @Test
    public void testRemapInputOutputs() throws Exception {
        String vsShader =
                 """
                #version 140
                in vec3 position;
                in vec2 texcoord0;
                in vec4 color;
                out vec2 var_texcoord0;
                out vec4 var_color;
                out vec3 var_position;          
                void main()
                {
                    var_position = position;
                    var_texcoord0 = texcoord0;
                    var_color = vec4(color.rgb * color.a, color.a);
                    gl_Position = vec4(position.xyz, 1.0);
                }                    
                """;

        String fsShader =
                """
                #version 140
                in vec2 var_texcoord0;
                in vec3 var_position;
                in vec4 var_color;
                out vec4 colorOut;
                void main()
                {
                    colorOut = vec4(var_texcoord0.st, 0, 0) + vec4(var_position, 0) + var_color;
                }
                """;

        ArrayList<ShaderCompilePipeline.ShaderModuleDesc> shaderModuleDescs = toShaderDescs(vsShader, fsShader);

        ShaderCompilePipeline pipeline = new ShaderCompilePipeline("testRemapping");
        ShaderCompilePipeline.createShaderPipeline(pipeline, shaderModuleDescs, new ShaderCompilePipeline.Options());

        SPIRVReflector reflectorVs = pipeline.getReflectionData(ShaderDesc.ShaderType.SHADER_TYPE_VERTEX);
        SPIRVReflector reflectorFs = pipeline.getReflectionData(ShaderDesc.ShaderType.SHADER_TYPE_FRAGMENT);

        for (Shaderc.ShaderResource output : reflectorVs.getOutputs()) {
            Shaderc.ShaderResource other = null;

            for (Shaderc.ShaderResource input : reflectorFs.getInputs()) {
                if (output.name.equals(input.name)) {
                    other = input;
                    break;
                }
            }

            assertNotNull(other);
            assertEquals(output.location, other.location);
        }

        ShaderCompilePipeline.destroyShaderPipeline(pipeline);
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
            pipelineFragment.crossCompile(fsDesc.type, ShaderDesc.Language.LANGUAGE_GLES_SM100);
        } catch (CompileExceptionError e) {
            didException = true;
        }
        assertTrue(didException);
        ShaderCompilePipeline.destroyShaderPipeline(pipelineFragment);
    }

    @Test
    public void testExtraDefines() throws Exception {
        String fsShader =
               """
               #version 140
               out vec4 color;
               void main() {
               #ifdef TEST_DEFINE
                    color = vec4(1.0);
               #else
                    color = vec4(0.25);
               #endif
               }
               """;

        ShaderCompilePipeline.ShaderModuleDesc fsDesc = new ShaderCompilePipeline.ShaderModuleDesc();
        fsDesc.source = fsShader;
        fsDesc.type = ShaderDesc.ShaderType.SHADER_TYPE_FRAGMENT;

        ShaderCompilePipeline.Options options = new ShaderCompilePipeline.Options();
        options.defines.add("TEST_DEFINE");

        ShaderCompilePipeline pipelineFragment = new ShaderCompilePipeline("testFragment");
        ShaderCompilePipeline.createShaderPipeline(pipelineFragment, fsDesc, options);

        Shaderc.ShaderCompileResult compileResult = pipelineFragment.crossCompile(ShaderDesc.ShaderType.SHADER_TYPE_FRAGMENT, ShaderDesc.Language.LANGUAGE_GLSL_SM330);
        String compiledStr = new String(compileResult.data);

        assertTrue(compiledStr.contains("color = vec4(1.0);"));
        ShaderCompilePipeline.destroyShaderPipeline(pipelineFragment);

        String fsShaderLegacy =
                """
                void main() {
                #ifdef TEST_DEFINE
                     gl_FragColor = vec4(1.0);
                #else
                     gl_FragColor = vec4(0.25);
                #endif
                }
                """;

        ShaderCompilePipeline.ShaderModuleDesc fsDescLegacy = new ShaderCompilePipeline.ShaderModuleDesc();
        fsDescLegacy.source = fsShaderLegacy;
        fsDescLegacy.type = ShaderDesc.ShaderType.SHADER_TYPE_FRAGMENT;
        ShaderCompilePipelineLegacy pipelineFragmentLegacy = new ShaderCompilePipelineLegacy("testFragment");
        ShaderCompilePipeline.createShaderPipeline(pipelineFragmentLegacy, fsDescLegacy, new ShaderCompilePipeline.Options());

        compileResult = pipelineFragmentLegacy.crossCompile(ShaderDesc.ShaderType.SHADER_TYPE_FRAGMENT, ShaderDesc.Language.LANGUAGE_GLSL_SM330);
        compiledStr = new String(compileResult.data);

        assertTrue(compiledStr.contains("_DMENGINE_GENERATED_gl_FragColor_0 = vec4(1.0);"));
        ShaderCompilePipeline.destroyShaderPipeline(pipelineFragmentLegacy);
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
