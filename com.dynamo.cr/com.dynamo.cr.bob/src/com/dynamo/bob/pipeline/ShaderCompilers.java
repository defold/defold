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

import java.io.IOException;
import java.util.ArrayList;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Platform;
import com.dynamo.graphics.proto.Graphics.ShaderDesc;

import com.dynamo.bob.pipeline.shader.ShaderCompilePipeline;

public class ShaderCompilers {

    public static class CommonShaderCompiler implements IShaderCompiler {
        private final Platform platform;

        public CommonShaderCompiler(Platform platform) {
            this.platform = platform;
        }

        private ArrayList<ShaderDesc.Language> getPlatformShaderLanguages(ShaderDesc.ShaderType shaderType, boolean outputSpirvRequested) {
            ArrayList<ShaderDesc.Language> shaderLanguages = new ArrayList<>();
            boolean spirvSupported = true;

            switch(platform) {
                case Arm64MacOS:
                case X86_64MacOS: {
                    if (shaderType != ShaderDesc.ShaderType.SHADER_TYPE_COMPUTE) {
                        shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLSL_SM330);
                    }
                } break;
                case X86Win32:
                case X86_64Win32:
                case X86Linux:
                case X86_64Linux: {
                    if (shaderType == ShaderDesc.ShaderType.SHADER_TYPE_COMPUTE) {
                        shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLSL_SM430);
                    } else {
                        shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLSL_SM140);
                    }
                } break;
                case Arm64Ios:
                case X86_64Ios: {
                    if (shaderType != ShaderDesc.ShaderType.SHADER_TYPE_COMPUTE) {
                        shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLES_SM300);
                    }
                } break;
                case Armv7Android:
                case Arm64Android: {
                    if (shaderType != ShaderDesc.ShaderType.SHADER_TYPE_COMPUTE) {
                        shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLES_SM300);
                        shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLES_SM100);
                    }
                } break;
                case JsWeb:
                case WasmWeb: {
                    if (shaderType != ShaderDesc.ShaderType.SHADER_TYPE_COMPUTE) {
                        shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLES_SM300);
                        shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLES_SM100);
                    }
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
            return shaderLanguages;
        }

        public ArrayList<ShaderProgramBuilder.ShaderBuildResult> compile(String shaderSource, ShaderDesc.ShaderType shaderType, String resourceOutputPath, boolean outputSpirv) throws IOException, CompileExceptionError {
            ShaderCompilePipeline pipeline = ShaderProgramBuilder.getShaderPipelineFromShaderSource(shaderType, resourceOutputPath, shaderSource);
            ArrayList<ShaderProgramBuilder.ShaderBuildResult> shaderBuildResults = new ArrayList<>();
            ArrayList<ShaderDesc.Language> shaderLanguages = getPlatformShaderLanguages(shaderType, outputSpirv);

            assert shaderLanguages != null;
            for (ShaderDesc.Language shaderLanguage : shaderLanguages) {
                ShaderDesc.Shader.Builder builder = ShaderProgramBuilder.makeShaderBuilder(shaderLanguage, pipeline.crossCompile(shaderType, shaderLanguage), pipeline.getReflectionData());
                shaderBuildResults.add(new ShaderProgramBuilder.ShaderBuildResult(builder));
            }

            return shaderBuildResults;
        }
    }

    public static IShaderCompiler getCommonShaderCompiler(Platform platform) {
        return new CommonShaderCompiler(platform);
    }
}
