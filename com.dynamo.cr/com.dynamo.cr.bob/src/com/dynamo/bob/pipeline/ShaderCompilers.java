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

        private ArrayList<ShaderDesc.Language> getPlatformShaderLanguages(boolean isComputeType, boolean outputSpirvRequested, boolean outputWGSLRequested) {
            ArrayList<ShaderDesc.Language> shaderLanguages = new ArrayList<>();
            boolean spirvSupported = true;
            boolean hlslSupported = platform == Platform.X86_64Win32;

            switch(platform) {
                case Arm64MacOS:
                case X86_64MacOS: {
                    if (!isComputeType) {
                        shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLSL_SM330);
                    }
                } break;
                case X86Win32:
                case X86_64Win32:
                case X86Linux:
                case Arm64Linux:
                case X86_64Linux: {
                    if (isComputeType) {
                        shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLSL_SM430);
                    } else {
                        shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLSL_SM140);
                    }
                } break;
                case Arm64Ios:
                case X86_64Ios: {
                    if (!isComputeType) {
                        shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLES_SM300);
                    }
                } break;
                case Armv7Android:
                case Arm64Android: {
                    if (!isComputeType) {
                        shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLES_SM300);
                        shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLES_SM100);
                    }
                } break;
                case JsWeb:
                case WasmWeb: {
                    if (!isComputeType) {
                        shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLES_SM300);
                        shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLES_SM100);
                    }
                    if (outputWGSLRequested)
                        shaderLanguages.add(ShaderDesc.Language.LANGUAGE_WGSL);
                    spirvSupported = false;
                } break;
                case Arm64NX64:
                    outputSpirvRequested = true;
                    break;
                default: return null;
            }

            if (spirvSupported && outputSpirvRequested) {
                shaderLanguages.add(ShaderDesc.Language.LANGUAGE_SPIRV);
            }

            if (hlslSupported && outputHlslRequested) {
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

        public ShaderProgramBuilder.ShaderCompileResult compile(ArrayList<ShaderCompilePipeline.ShaderModuleDesc> shaderModules, String resourceOutputPath, boolean outputSpirv, boolean outputWGSL) throws IOException, CompileExceptionError {

            ShaderCompilePipeline.Options opts = new ShaderCompilePipeline.Options();
            opts.splitTextureSamplers = outputWGSL || outputHlsl;

            ShaderCompilePipeline pipeline = ShaderProgramBuilder.newShaderPipeline(resourceOutputPath, shaderModules, opts);
            ArrayList<ShaderProgramBuilder.ShaderBuildResult> shaderBuildResults = new ArrayList<>();

            ArrayList<ShaderDesc.Language> shaderLanguages = getPlatformShaderLanguages(shaderType, outputSpirv, outputHlsl, outputWGSL);

            validateModules(shaderModules);

            boolean isComputeType = shaderModules.get(0).type == ShaderDesc.ShaderType.SHADER_TYPE_COMPUTE;
            ArrayList<ShaderDesc.Language> shaderLanguages = getPlatformShaderLanguages(isComputeType, outputSpirv, outputWGSL);
            assert shaderLanguages != null;

            HashMap<ShaderDesc.ShaderType, Boolean> shaderTypeKeys = new HashMap<>();

            for (ShaderDesc.Language shaderLanguage : shaderLanguages) {
                for (ShaderCompilePipeline.ShaderModuleDesc shaderModule : shaderModules) {
                    byte[] crossCompileResult = pipeline.crossCompile(shaderModule.type, shaderLanguage);
                    ShaderDesc.Shader.Builder builder = ShaderProgramBuilder.makeShaderBuilder(crossCompileResult, shaderLanguage, shaderModule.type);
                    shaderBuildResults.add(new ShaderProgramBuilder.ShaderBuildResult(builder));

                    if (!shaderTypeKeys.containsKey(shaderModule.type)) {
                        shaderTypeKeys.put(shaderModule.type, true);
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
