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

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.pipeline.shader.ShaderCompilePipeline;
import com.dynamo.graphics.proto.Graphics;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;

import static com.dynamo.bob.pipeline.ShaderProgramBuilder.buildResultsToShaderDescBuildResults;

public class ShaderProgramBuilderEditor {
    static public ShaderUtil.Common.GLSLCompileResult buildGLSLVariantTextureArray(String resourcePath, String source, Graphics.ShaderDesc.ShaderType shaderType, Graphics.ShaderDesc.Language shaderLanguage, int maxPageCount) throws IOException, CompileExceptionError {
        ShaderCompilePipeline.ShaderModuleDesc module = new ShaderCompilePipeline.ShaderModuleDesc();
        module.source = source;
        module.type = shaderType;

        ArrayList<ShaderCompilePipeline.ShaderModuleDesc> shaderDescs = new ArrayList<>();
        shaderDescs.add(module);

        ShaderCompilePipeline.Options options = new ShaderCompilePipeline.Options();
        ShaderCompilePipeline pipeline;

        try {
            pipeline = ShaderProgramBuilder.newShaderPipeline(resourcePath, shaderDescs, options);
        } catch (Exception e) {
            // We don't have a graceful way to handle shader errors in the editor except for building/bundling
            return new ShaderUtil.Common.GLSLCompileResult(source);
        }

        byte[] result = pipeline.crossCompile(shaderType, shaderLanguage);
        String compiledSource = new String(result);

        ShaderUtil.Common.GLSLCompileResult variantCompileResult = ShaderUtil.VariantTextureArrayFallback.transform(compiledSource, maxPageCount);

        // If the variant transformation didn't do anything, we pass the original source but without array samplers
        if (variantCompileResult != null) {
            variantCompileResult.reflector = pipeline.getReflectionData(shaderType);
            return variantCompileResult;
        } else {
            return new ShaderUtil.Common.GLSLCompileResult(compiledSource, pipeline.getReflectionData(shaderType));
        }
    }

    static private boolean isCompatibleLanguage(Graphics.ShaderDesc.ShaderType shaderType, Graphics.ShaderDesc.Language shaderLanguage) {
        if (shaderType == Graphics.ShaderDesc.ShaderType.SHADER_TYPE_COMPUTE) {
            return switch (shaderLanguage) {
                case LANGUAGE_SPIRV, LANGUAGE_GLSL_SM430, LANGUAGE_PSSL, LANGUAGE_WGSL, LANGUAGE_HLSL -> true;
                default -> false;
            };
        } else {
            return true;
        }
    }

    // Called from editor for producing a ShaderDesc with a list of finalized shaders,
    // fully transformed from source to context shaders based on a list of languages
    static public ShaderProgramBuilder.ShaderDescBuildResult makeShaderDescWithVariants(
            String resourceOutputPath, ShaderCompilePipeline.ShaderModuleDesc[] shaderDescs,
            Graphics.ShaderDesc.Language[] shaderLanguages, int maxPageCount) throws IOException, CompileExceptionError {

        ShaderCompilePipeline.Options options = new ShaderCompilePipeline.Options();
        ShaderCompilePipeline pipeline;

        ArrayList<ShaderCompilePipeline.ShaderModuleDesc> shaderModuleDescsArrayList = new ArrayList<>(Arrays.asList(shaderDescs));

        try {
            pipeline = ShaderProgramBuilder.newShaderPipeline(resourceOutputPath, shaderModuleDescsArrayList, options);
        } catch (Exception e) {
            ShaderProgramBuilder.ShaderDescBuildResult res = new ShaderProgramBuilder.ShaderDescBuildResult();
            res.buildWarnings = new String[] { e.getMessage()};
            return res;
        }

        HashMap<Graphics.ShaderDesc.ShaderType, Boolean> shaderTypeKeys = new HashMap<>();
        ArrayList<ShaderProgramBuilder.ShaderBuildResult> shaderBuildResults = new ArrayList<>();

        for (Graphics.ShaderDesc.Language shaderLanguage : shaderLanguages) {
            for (ShaderCompilePipeline.ShaderModuleDesc shaderModule : shaderModuleDescsArrayList) {
                // Some languages might not be compatible with the shader type, e.g webgl and compute shaders
                if (!isCompatibleLanguage(shaderModule.type, shaderLanguage)) {
                    continue;
                }

                if (!shaderTypeKeys.containsKey(shaderModule.type)) {
                    shaderTypeKeys.put(shaderModule.type, true);
                }

                byte[] source = pipeline.crossCompile(shaderModule.type, shaderLanguage);
                boolean variantTextureArray = false;

                if (ShaderUtil.VariantTextureArrayFallback.isRequired(shaderLanguage)) {
                    ShaderUtil.Common.GLSLCompileResult variantCompileResult = ShaderUtil.VariantTextureArrayFallback.transform(new String(source), maxPageCount);
                    if (variantCompileResult != null && variantCompileResult.arraySamplers.length > 0) {
                        source = variantCompileResult.source.getBytes();
                        variantTextureArray = true;
                    }
                }

                Graphics.ShaderDesc.Shader.Builder builder = ShaderProgramBuilder.makeShaderBuilder(source, shaderLanguage, shaderModule.type);

                // Note: We are not doing builder.setVariantTextureArray(variantTextureArray); because calling that function
                //       will mark the field as being set, regardless of the value. We only want to mark the field if we need to.
                if (variantTextureArray) {
                    builder.setVariantTextureArray(true);
                }
                shaderBuildResults.add(new ShaderProgramBuilder.ShaderBuildResult(builder));
            }
        }

        ShaderProgramBuilder.ShaderCompileResult compileResult = new ShaderProgramBuilder.ShaderCompileResult();

        for(Graphics.ShaderDesc.ShaderType type : shaderTypeKeys.keySet()) {
            compileResult.reflectors.add(pipeline.getReflectionData(type));
        }

        compileResult.shaderBuildResults = shaderBuildResults;

        return buildResultsToShaderDescBuildResults(compileResult);
    }
}
