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
import com.dynamo.bob.pipeline.IShaderCompiler;
import com.dynamo.bob.pipeline.ShaderCompilerHelpers;
import com.dynamo.bob.pipeline.ShaderProgramBuilder;
import com.dynamo.bob.pipeline.ShaderUtil.ES2ToES3Converter;
import com.dynamo.graphics.proto.Graphics.ShaderDesc;

public class ShaderCompilers {
    public static IShaderCompiler getCommonShaderCompiler(Platform platform) {
        switch(platform) {
            case Arm64MacOS:
            case X86_64MacOS:
                return new MacOSShaderCompiler();
            case X86Win32:
            case X86_64Win32:
                return new Win32ShaderCompiler();
            case X86Linux:
            case X86_64Linux:
                return new LinuxShaderCompiler();
            case Arm64Ios:
            case X86_64Ios:
                return new IOSShaderCompiler();
            case Armv7Android:
            case Arm64Android:
                return new AndroidShaderCompiler();
            case JsWeb:
            case WasmWeb:
                return new WebShaderCompiler();
            case Arm64NX64:
                return new NXShaderCompiler();
            default:break;
        }
        return null;
    }

    // Generate a shader desc struct that consists of either the built shader desc, or a list of compile warnings/errors
    public static ArrayList<ShaderProgramBuilder.ShaderBuildResult> getBaseShaderBuildResults(String resourceOutputPath, String fullShaderSource,
            ES2ToES3Converter.ShaderType shaderType, ShaderDesc.Language[] shaderLanguages,
            String spirvTargetProfile, boolean isDebug, boolean softFail) throws IOException, CompileExceptionError {

        ArrayList<ShaderProgramBuilder.ShaderBuildResult> shaderBuildResults = new ArrayList<ShaderProgramBuilder.ShaderBuildResult>();

        for (ShaderDesc.Language shaderLanguage : shaderLanguages) {
            if (shaderLanguage == ShaderDesc.Language.LANGUAGE_SPIRV) {
                shaderBuildResults.add(ShaderCompilerHelpers.buildSpirvFromGLSL(fullShaderSource, shaderType, resourceOutputPath, spirvTargetProfile, isDebug, softFail));
            } else {
                shaderBuildResults.add(ShaderCompilerHelpers.buildGLSL(fullShaderSource, shaderType, shaderLanguage, isDebug));
            }
        }

        return shaderBuildResults;
    }

    public static class MacOSShaderCompiler implements IShaderCompiler {
        public ArrayList<ShaderProgramBuilder.ShaderBuildResult> compile(String shaderSource, ES2ToES3Converter.ShaderType shaderType, String resourceOutputPath, String resourceOutput, boolean isDebug, boolean outputSpirv, boolean softFail) throws IOException, CompileExceptionError {
            ArrayList<ShaderDesc.Language> shaderLanguages = new ArrayList<ShaderDesc.Language>();

            // Compute shaders not supported on osx for OpenGL
            if (shaderType != ES2ToES3Converter.ShaderType.COMPUTE_SHADER) {
                shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLSL_SM140);
            }

            if (outputSpirv)
            {
                shaderLanguages.add(ShaderDesc.Language.LANGUAGE_SPIRV);
            }

            return getBaseShaderBuildResults(resourceOutputPath, shaderSource, shaderType, shaderLanguages.toArray(new ShaderDesc.Language[0]), "", isDebug, softFail);
        }
    }

    public static class Win32ShaderCompiler implements IShaderCompiler {
        public ArrayList<ShaderProgramBuilder.ShaderBuildResult> compile(String shaderSource, ES2ToES3Converter.ShaderType shaderType, String resourceOutputPath, String resourceOutput, boolean isDebug, boolean outputSpirv, boolean softFail) throws IOException, CompileExceptionError {
            ArrayList<ShaderDesc.Language> shaderLanguages = new ArrayList<ShaderDesc.Language>();

            if (shaderType == ES2ToES3Converter.ShaderType.COMPUTE_SHADER) {
                shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLSL_SM430);
            } else {
                shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLSL_SM140);
            }

            if (outputSpirv)
            {
                shaderLanguages.add(ShaderDesc.Language.LANGUAGE_SPIRV);
            }

            return getBaseShaderBuildResults(resourceOutputPath, shaderSource, shaderType, shaderLanguages.toArray(new ShaderDesc.Language[0]), "", isDebug, softFail);
        }
    }

    public static class LinuxShaderCompiler implements IShaderCompiler {
        public ArrayList<ShaderProgramBuilder.ShaderBuildResult> compile(String shaderSource, ES2ToES3Converter.ShaderType shaderType, String resourceOutputPath, String resourceOutput, boolean isDebug, boolean outputSpirv, boolean softFail) throws IOException, CompileExceptionError {
            ArrayList<ShaderDesc.Language> shaderLanguages = new ArrayList<ShaderDesc.Language>();

            if (shaderType == ES2ToES3Converter.ShaderType.COMPUTE_SHADER) {
                shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLSL_SM430);
            } else {
                shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLSL_SM140);
            }

            if (outputSpirv)
            {
                shaderLanguages.add(ShaderDesc.Language.LANGUAGE_SPIRV);
            }

            return getBaseShaderBuildResults(resourceOutputPath, shaderSource, shaderType, shaderLanguages.toArray(new ShaderDesc.Language[0]), "", isDebug, softFail);
        }
    }

    public static class IOSShaderCompiler implements IShaderCompiler {
        public ArrayList<ShaderProgramBuilder.ShaderBuildResult> compile(String shaderSource, ES2ToES3Converter.ShaderType shaderType, String resourceOutputPath, String resourceOutput, boolean isDebug, boolean outputSpirv, boolean softFail) throws IOException, CompileExceptionError {
            ArrayList<ShaderDesc.Language> shaderLanguages = new ArrayList<ShaderDesc.Language>();

            if (shaderType != ES2ToES3Converter.ShaderType.COMPUTE_SHADER) {
                shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLES_SM300);

                if (outputSpirv)
                {
                    shaderLanguages.add(ShaderDesc.Language.LANGUAGE_SPIRV);
                }
            }

            return getBaseShaderBuildResults(resourceOutputPath, shaderSource, shaderType, shaderLanguages.toArray(new ShaderDesc.Language[0]), "es", isDebug, softFail);
        }
    }

    public static class AndroidShaderCompiler implements IShaderCompiler {
        public ArrayList<ShaderProgramBuilder.ShaderBuildResult> compile(String shaderSource, ES2ToES3Converter.ShaderType shaderType, String resourceOutputPath, String resourceOutput, boolean isDebug, boolean outputSpirv, boolean softFail) throws IOException, CompileExceptionError {
            ArrayList<ShaderDesc.Language> shaderLanguages = new ArrayList<ShaderDesc.Language>();

            if (shaderType != ES2ToES3Converter.ShaderType.COMPUTE_SHADER) {
                shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLES_SM300);
                shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLES_SM100);

                if (outputSpirv)
                {
                    shaderLanguages.add(ShaderDesc.Language.LANGUAGE_SPIRV);
                }
            }

            return getBaseShaderBuildResults(resourceOutputPath, shaderSource, shaderType, shaderLanguages.toArray(new ShaderDesc.Language[0]), "", isDebug, softFail);
        }
    }

    public static class WebShaderCompiler implements IShaderCompiler {
        public ArrayList<ShaderProgramBuilder.ShaderBuildResult> compile(String shaderSource, ES2ToES3Converter.ShaderType shaderType, String resourceOutputPath, String resourceOutput, boolean isDebug, boolean outputSpirv, boolean softFail) throws IOException, CompileExceptionError {
            ArrayList<ShaderDesc.Language> shaderLanguages = new ArrayList<ShaderDesc.Language>();

            if (shaderType != ES2ToES3Converter.ShaderType.COMPUTE_SHADER) {
                shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLES_SM300);
                shaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLES_SM100);
            }

            return getBaseShaderBuildResults(resourceOutputPath, shaderSource, shaderType, shaderLanguages.toArray(new ShaderDesc.Language[0]), "", isDebug, softFail);
        }
    }

    public static class NXShaderCompiler implements IShaderCompiler {
        public ArrayList<ShaderProgramBuilder.ShaderBuildResult> compile(String shaderSource, ES2ToES3Converter.ShaderType shaderType, String resourceOutputPath, String resourceOutput, boolean isDebug, boolean outputSpirv, boolean softFail) throws IOException, CompileExceptionError {
            ArrayList<ShaderDesc.Language> shaderLanguages = new ArrayList<ShaderDesc.Language>();
            shaderLanguages.add(ShaderDesc.Language.LANGUAGE_SPIRV);
            return getBaseShaderBuildResults(resourceOutputPath, shaderSource, shaderType, shaderLanguages.toArray(new ShaderDesc.Language[0]), "", isDebug, softFail);
        }
    }
}
