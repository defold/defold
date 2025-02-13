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

import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Platform;
import com.dynamo.bob.pipeline.shader.ShaderCompilePipeline;
import com.dynamo.graphics.proto.Graphics.ShaderDesc;

public class ShaderCompilers {

    public static class CommonShaderCompiler implements IShaderCompiler {
        private final Platform platform;

        public CommonShaderCompiler(Platform platform) {
            this.platform = platform;
        }

        private ArrayList<ShaderDesc.Language> getPlatformShaderLanguages(boolean isComputeType, boolean outputSpirv, boolean outputWGLS, boolean outputHLSL) {
            ArrayList<ShaderDesc.Language> shaderLanguages = new ArrayList<>();
            boolean spirvSupported = true;
            boolean hlslSupported = platform == Platform.X86_64Win32;

            if (platform == Platform.Arm64MacOS ||
                platform == Platform.X86_64MacOS) {
                if (!isComputeType) {
                    shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLSL_SM330);
                }
            }
            else
            if (platform == Platform.X86Win32 ||
                platform == Platform.X86_64Win32 ||
                platform == Platform.X86Linux ||
                platform == Platform.Arm64Linux ||
                platform == Platform.X86_64Linux) {
                    if (isComputeType) {
                        shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLSL_SM430);
                    } else {
                        shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLSL_SM330);
                    }
            }
            else
            if (platform == Platform.Arm64Ios ||
                platform == Platform.X86_64Ios) {
                    if (!isComputeType) {
                        shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLES_SM300);
                    }
            }
            else
            if (platform == Platform.Armv7Android ||
                platform == Platform.Arm64Android) {
                    if (!isComputeType) {
                        shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLES_SM300);
                        shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLES_SM100);
                    }
            }
            else
            if (platform == Platform.JsWeb ||
                platform == Platform.WasmWeb) {
                    if (!isComputeType) {
                        shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLES_SM300);
                        shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLES_SM100);
                    }
                    if (outputWGLS)
                        shaderLanguages.add(ShaderDesc.Language.LANGUAGE_WGSL);
                    spirvSupported = false;
            }
            else
            if (platform == Platform.Arm64NX64) {
                outputSpirv = true;
            }
            else {
                return null;
            }

            if (spirvSupported && outputSpirv) {
                shaderLanguages.add(ShaderDesc.Language.LANGUAGE_SPIRV);
            }

            if (hlslSupported && outputHLSL) {
                shaderLanguages.add(ShaderDesc.Language.LANGUAGE_HLSL);
            }

            return shaderLanguages;
        }

        private static void validateModules(ArrayList<ShaderCompilePipeline.ShaderModuleDesc> descs) throws CompileExceptionError{
            if (descs.isEmpty())
                throw new CompileExceptionError("No shader modules");

            int vsCount=0, fsCount=0, computeCount=0;
            for (ShaderCompilePipeline.ShaderModuleDesc desc : descs) {
                switch(desc.type) {
                    case SHADER_TYPE_COMPUTE -> computeCount++;
                    case SHADER_TYPE_VERTEX -> vsCount++;
                    case SHADER_TYPE_FRAGMENT -> fsCount++;
                }
            }

            if (computeCount > 0 && (vsCount > 0 || fsCount > 0))
                throw new CompileExceptionError("Can't match compute with graphics modules");
        }

        public ShaderProgramBuilder.ShaderCompileResult compile(ArrayList<ShaderCompilePipeline.ShaderModuleDesc> shaderModules, String resourceOutputPath, CompileOptions compileOptions) throws IOException, CompileExceptionError {

            boolean isComputeType = shaderModules.get(0).type == ShaderDesc.ShaderType.SHADER_TYPE_COMPUTE;
            boolean outputSpirv = false;
            boolean outputHLSL = false;
            boolean outputWGSL = false;

            ShaderCompilePipeline.Options opts = new ShaderCompilePipeline.Options();
            opts.splitTextureSamplers = compileOptions.forceSplitSamplers;

            for (ShaderDesc.Language shaderLanguage : compileOptions.forceIncludeShaderLanguages) {
                opts.splitTextureSamplers |= shaderLanguage == ShaderDesc.Language.LANGUAGE_HLSL || shaderLanguage == ShaderDesc.Language.LANGUAGE_WGSL;
                outputSpirv |= shaderLanguage == ShaderDesc.Language.LANGUAGE_SPIRV;
                outputHLSL |= shaderLanguage == ShaderDesc.Language.LANGUAGE_HLSL;
                outputWGSL |= shaderLanguage == ShaderDesc.Language.LANGUAGE_WGSL;
            }

            ShaderCompilePipeline pipeline = ShaderProgramBuilder.newShaderPipeline(resourceOutputPath, shaderModules, opts);
            ArrayList<ShaderProgramBuilder.ShaderBuildResult> shaderBuildResults = new ArrayList<>();

            validateModules(shaderModules);

            ArrayList<ShaderDesc.Language> shaderLanguages = getPlatformShaderLanguages(isComputeType, outputSpirv, outputWGSL, outputHLSL);
            assert shaderLanguages != null;

            // Used for tests, merge in potentially unsupported languages here.
            for (ShaderDesc.Language shaderLanguage : compileOptions.forceIncludeShaderLanguages) {
                if (!shaderLanguages.contains(shaderLanguage)) {
                    shaderLanguages.add(shaderLanguage);
                }
            }

            HashMap<ShaderDesc.ShaderType, Boolean> shaderTypeKeys = new HashMap<>();

            for (ShaderDesc.Language shaderLanguage : shaderLanguages) {

                boolean arrayTextureFallbackRequired = ShaderUtil.VariantTextureArrayFallback.isRequired(shaderLanguage);

                for (ShaderCompilePipeline.ShaderModuleDesc shaderModule : shaderModules) {

                    boolean variantTextureArray = false;
                    byte[] crossCompileResult = pipeline.crossCompile(shaderModule.type, shaderLanguage);

                    if (!shaderTypeKeys.containsKey(shaderModule.type)) {
                        shaderTypeKeys.put(shaderModule.type, true);
                    }

                    if (arrayTextureFallbackRequired) {
                        ShaderUtil.Common.GLSLCompileResult variantCompileResult = ShaderUtil.VariantTextureArrayFallback.transform(new String(crossCompileResult), compileOptions.maxPageCount);
                        if (variantCompileResult != null && variantCompileResult.arraySamplers.length > 0) {
                            crossCompileResult = variantCompileResult.source.getBytes();
                            variantTextureArray = true;
                        }
                    }

                    ShaderDesc.Shader.Builder builder = ShaderProgramBuilder.makeShaderBuilder(crossCompileResult, shaderLanguage, shaderModule.type);
                    shaderBuildResults.add(new ShaderProgramBuilder.ShaderBuildResult(builder));

                    if (variantTextureArray) {
                        builder.setVariantTextureArray(true);
                    }
                }
            }

            ShaderProgramBuilder.ShaderCompileResult compileResult = new ShaderProgramBuilder.ShaderCompileResult();
            compileResult.shaderBuildResults = shaderBuildResults;

            for(ShaderDesc.ShaderType type : shaderTypeKeys.keySet()) {
                compileResult.reflectors.add(pipeline.getReflectionData(type));
            }

            ShaderCompilePipeline.destroyShaderPipeline(pipeline);

            return compileResult;
        }
    }

    public static IShaderCompiler getCommonShaderCompiler(Platform platform) {
        return new CommonShaderCompiler(platform);
    }
}
