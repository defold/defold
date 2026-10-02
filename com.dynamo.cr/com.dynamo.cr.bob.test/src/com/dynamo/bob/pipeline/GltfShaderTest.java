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

import com.dynamo.graphics.proto.Graphics.ShaderDesc;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Set;
import java.util.stream.Collectors;
import org.junit.Before;
import org.junit.Test;
import static org.junit.Assert.*;

public class GltfShaderTest extends AbstractProtoBuilderTest {
    private Path repository;

    @Before
    public void loadBuiltins() throws Exception {
        addTestFiles();
        repository = Path.of("").toAbsolutePath();
        while (repository != null && !Files.isDirectory(repository.resolve("engine/engine/content/builtins"))) {
            repository = repository.getParent();
        }
        assertNotNull("Run this test from the repository checkout", repository);
        Path materials = repository.resolve("engine/engine/content/builtins/materials");
        try (var files = Files.list(materials)) {
            for (Path file : files.filter(p -> p.getFileName().toString().startsWith("gltf") || p.getFileName().toString().equals("skinning.glsl")).toList()) {
                addFile("/builtins/materials/" + file.getFileName(), Files.readString(file));
            }
        }
    }

    // Verifies all vertex variants link with PBR and expose only their required instance/skin inputs.
    @Test
    public void compilesVertexVariants() throws Exception {
        Path materials = repository.resolve("engine/engine/content/builtins/materials");
        for (String suffix : new String[] {"", "_instanced", "_skinned", "_skinned_instanced"}) {
            ShaderDesc shader = addAndBuildShaderDescs(
                new String[] {"/variant" + suffix + ".vp", "/variant" + suffix + ".fp"},
                new String[] {Files.readString(materials.resolve("gltf" + suffix + ".vp")), Files.readString(materials.resolve("gltf.fp"))},
                "/variant" + suffix + ".shbundle");
            Set<String> inputs = shader.getReflection().getInputsList().stream().map(i -> i.getName()).collect(Collectors.toSet());
            assertEquals(suffix.contains("instanced"), inputs.contains("mtx_world"));
            assertEquals(suffix.contains("skinned"), inputs.contains("bone_weights"));
            assertEquals(suffix.contains("skinned") && suffix.contains("instanced"), inputs.contains("animation_data"));
            assertTrue(inputs.contains("tangent"));
        }
    }

    // Verifies the shared variant compiles for mobile, Metal, WebGPU and DirectX adapters.
    @Test
    public void compilesPlatformVariants() throws Exception {
        Path materials = repository.resolve("engine/engine/content/builtins/materials");
        String[][] targets = {{"arm64-android", "opengles,vulkan"}, {"arm64-macos", "metal"},
                              {"wasm-web", "webgpu"}, {"x86_64-win32", "dx12"}};
        for (String[] target : targets) {
            IShaderCompiler.CompileOptions options = new IShaderCompiler.CompileOptions();
            options.shaderAdapters = target[1];
            var modules = new java.util.ArrayList<com.dynamo.bob.pipeline.shader.ShaderCompilePipeline.ShaderModuleDesc>();
            for (String file : new String[] {"gltf_skinned_instanced.vp", "gltf.fp"}) {
                var module = new com.dynamo.bob.pipeline.shader.ShaderCompilePipeline.ShaderModuleDesc();
                module.resourcePath = "/builtins/materials/" + file;
                module.type = file.endsWith(".vp") ? ShaderDesc.ShaderType.SHADER_TYPE_VERTEX : ShaderDesc.ShaderType.SHADER_TYPE_FRAGMENT;
                module.source = new ShaderPreprocessor(getProject(), module.resourcePath, Files.readString(materials.resolve(file)), null).getCompiledSource();
                modules.add(module);
            }
            var compiler = getProject().getShaderCompiler(com.dynamo.bob.Platform.get(target[0]));
            var result = compiler.compile(modules, "/cross_" + target[0] + ".spc", options);
            ShaderDesc shader = ShaderProgramBuilder.buildResultsToShaderDescBuildResults(result).shaderDesc;
            ShaderDesc.Language expected = switch (target[0]) {
                case "arm64-android" -> ShaderDesc.Language.LANGUAGE_SPIRV;
                case "arm64-macos" -> ShaderDesc.Language.LANGUAGE_MSL_22;
                case "wasm-web" -> ShaderDesc.Language.LANGUAGE_WGSL;
                default -> ShaderDesc.Language.LANGUAGE_HLSL_51;
            };
            assertTrue(target[0], shader.getShadersList().stream().anyMatch(s -> s.getLanguage() == expected));
        }
    }

    // Verifies procedural material/lighting reuse requires no glTF samplers, varyings or LightBuffer.
    @Test
    public void compilesIndependentLighting() throws Exception {
        String fragment = """
            #version 140
            #include "/builtins/materials/gltf_material.glsl"
            #include "/builtins/materials/gltf_lighting.glsl"
            out vec4 result;
            uniform custom_input { vec4 base; };
            void main() {
                PBRMaterial m;
                m.baseColor = base;
                m.emissive = vec3(0.0);
                m.metallic = 0.0;
                m.roughness = 0.5;
                m.occlusion = 1.0;
                m.alphaCutoff = 0.5;
                m.alphaMode = PBR_ALPHA_OPAQUE;
                m.doubleSided = false;
                m.unlit = false;
                PBRLighting light = empty_pbr_lighting();
                light.indirect = evaluate_pbr_ambient(get_material_info(m), vec3(1.0));
                result = vec4(composite_pbr_lighting(m, light, 1.0, 1.0), get_pbr_alpha(m));
            }
            """;
        ShaderDesc shader = addAndBuildShaderDescs(
            new String[] {"/procedural.vp", "/procedural.fp"},
            new String[] {"#version 140\nin vec4 position; void main() { gl_Position = position; }", fragment},
            "/procedural.shbundle");
        assertEquals(0, shader.getReflection().getTexturesCount());
        assertEquals(1, shader.getReflection().getUniformBuffersCount());
    }


}
