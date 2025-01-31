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

        private ArrayList<ShaderDesc.Language> getPlatformShaderLanguages(ShaderDesc.ShaderType shaderType, boolean outputSpirvRequested, boolean outputHlslRequested, boolean outputWGSLRequested) {
            ArrayList<ShaderDesc.Language> shaderLanguages = new ArrayList<>();
            boolean spirvSupported = true;
            boolean hlslSupported = platform == Platform.X86_64Win32;

            if (platform == Platform.Arm64MacOS ||
                platform == Platform.X86_64MacOS) {
                if (shaderType != ShaderDesc.ShaderType.SHADER_TYPE_COMPUTE) {
                    shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLSL_SM330);
                }
            }
            else
            if (platform == Platform.X86Win32 ||
                platform == Platform.X86_64Win32 ||
                platform == Platform.X86Linux ||
                platform == Platform.Arm64Linux ||
                platform == Platform.X86_64Linux) {
                    if (shaderType == ShaderDesc.ShaderType.SHADER_TYPE_COMPUTE) {
                        shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLSL_SM430);
                    } else {
                        shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLSL_SM140);
                    }
            }
            else
            if (platform == Platform.Arm64Ios ||
                platform == Platform.X86_64Ios) {
                    if (shaderType != ShaderDesc.ShaderType.SHADER_TYPE_COMPUTE) {
                        shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLES_SM300);
                    }
            }
            else
            if (platform == Platform.Armv7Android ||
                platform == Platform.Arm64Android) {
                    if (shaderType != ShaderDesc.ShaderType.SHADER_TYPE_COMPUTE) {
                        shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLES_SM300);
                        shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLES_SM100);
                    }
            }
            else
            if (platform == Platform.JsWeb ||
                platform == Platform.WasmWeb) {
                    if (shaderType != ShaderDesc.ShaderType.SHADER_TYPE_COMPUTE) {
                        shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLES_SM300);
                        shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLES_SM100);
                    }
                    if (outputWGSLRequested)
                        shaderLanguages.add(ShaderDesc.Language.LANGUAGE_WGSL);
                    spirvSupported = false;
            }
            else
            if (platform == Platform.Arm64NX64) {
                    outputSpirvRequested = true;
            }
            else {
                return null;
            }

            if (spirvSupported && outputSpirvRequested) {
                shaderLanguages.add(ShaderDesc.Language.LANGUAGE_SPIRV);
            }

            if (hlslSupported && outputHlslRequested) {
                shaderLanguages.add(ShaderDesc.Language.LANGUAGE_HLSL);
            }

            return shaderLanguages;
        }

        public ShaderProgramBuilder.ShaderCompileResult compile(String shaderSource, ShaderDesc.ShaderType shaderType, String resourceOutputPath, boolean outputSpirv, boolean outputHlsl, boolean outputWGSL) throws IOException, CompileExceptionError {

            ShaderCompilePipeline.Options opts = new ShaderCompilePipeline.Options();
            opts.splitTextureSamplers = outputWGSL || outputHlsl;

            ShaderCompilePipeline pipeline = ShaderProgramBuilder.newShaderPipelineFromShaderSource(shaderType, resourceOutputPath, shaderSource, opts);
            ArrayList<ShaderProgramBuilder.ShaderBuildResult> shaderBuildResults = new ArrayList<>();
            ArrayList<ShaderDesc.Language> shaderLanguages = getPlatformShaderLanguages(shaderType, outputSpirv, outputHlsl, outputWGSL);

            assert shaderLanguages != null;
            for (ShaderDesc.Language shaderLanguage : shaderLanguages) {
                byte[] shaderBytes = pipeline.crossCompile(shaderType, shaderLanguage);
                ShaderDesc.Shader.Builder builder = ShaderProgramBuilder.makeShaderBuilder(shaderLanguage, shaderBytes);
                shaderBuildResults.add(new ShaderProgramBuilder.ShaderBuildResult(builder));
            }

            ShaderProgramBuilder.ShaderCompileResult compileResult = new ShaderProgramBuilder.ShaderCompileResult();
            compileResult.shaderBuildResults = shaderBuildResults;
            compileResult.reflector = pipeline.getReflectionData();

            ShaderCompilePipeline.destroyShaderPipeline(pipeline);

            return compileResult;
        }
    }

    public static IShaderCompiler getCommonShaderCompiler(Platform platform) {
        return new CommonShaderCompiler(platform);
    }
}
