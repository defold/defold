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

package com.dynamo.bob.pipeline.shader;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;

import com.dynamo.bob.Bob;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Platform;
import com.dynamo.bob.pipeline.Shaderc;
import com.dynamo.bob.pipeline.ShadercJni;
import com.dynamo.bob.util.Exec;
import com.dynamo.bob.util.FileUtil;
import com.dynamo.bob.pipeline.ShaderUtil;
import com.dynamo.graphics.proto.Graphics.ShaderDesc;
import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;

public class ShaderCompilePipelineLegacy extends ShaderCompilePipeline {

    static private class SPIRVCompileResult {
        public byte[] source;
        public ArrayList<String> compile_warnings = new ArrayList<String>();
        public SPIRVReflector reflector;
    }

    protected static class ShaderModuleLegacy extends ShaderCompilePipeline.ShaderModule {
        public SPIRVCompileResult spirvResult;

        public ShaderModuleLegacy(ShaderModuleDesc desc) {
            super(desc);
        }
    }

    private static String glslangExe = null;
    private static String spirvOptExe = null;

    public ShaderCompilePipelineLegacy(String pipelineName) throws IOException {
        super(pipelineName);
        if (glslangExe == null) glslangExe = Bob.getExe(Platform.getHostPlatform(), "glslang");
        if (spirvOptExe == null) spirvOptExe = Bob.getExe(Platform.getHostPlatform(), "spirv-opt");
    }

    static private void checkResult(String resourcePath, String result_string) throws CompileExceptionError {
        if (result_string != null ) {
            String messagePrefixedWithProjectPath = intermediateResourceToProjectPath(result_string, resourcePath);
            throw new CompileExceptionError(messagePrefixedWithProjectPath, null);
        }
    }

    static private String getResultString(Exec.Result r)
    {
        if (r.ret != 0 ) {
            String[] tokenizedResult = new String(r.stdOutErr).split(":", 2);
            String message = tokenizedResult[0];
            if(tokenizedResult.length != 1) {
                message = tokenizedResult[1];
            }
            return message;
        }
        return null;
    }

    static private String compileSPIRVToWGSL(String resourcePath, byte[] shaderSource, String resourceOutput)  throws IOException, CompileExceptionError {
        File file_in_spv = File.createTempFile(FilenameUtils.getName(resourceOutput), ".spv");
        FileUtil.deleteOnExit(file_in_spv);
        FileUtils.writeByteArrayToFile(file_in_spv, shaderSource);

        File file_out_wgsl = File.createTempFile(FilenameUtils.getName(resourceOutput), ".wgsl");
        FileUtil.deleteOnExit(file_out_wgsl);
        generateWGSL(resourcePath, file_in_spv.getAbsolutePath(), file_out_wgsl.getAbsolutePath());
        return FileUtils.readFileToString(file_out_wgsl);
    }

    private SPIRVCompileResult compileGLSLToSPIRV(ShaderModuleLegacy moduleLegacy, String resourceOutput, String targetProfile, boolean softFail, boolean splitTextureSamplers)  throws IOException, CompileExceptionError {

        SPIRVCompileResult res = new SPIRVCompileResult();

        Exec.Result result;
        File file_out_spv;

        String shaderSource = moduleLegacy.desc.source;
        ShaderDesc.ShaderType shaderType = moduleLegacy.desc.type;

        if (shaderType == ShaderDesc.ShaderType.SHADER_TYPE_COMPUTE) {

            int version = 430;

            ShaderUtil.ES2ToES3Converter.Result es3Result = ShaderUtil.ES2ToES3Converter.transform(shaderSource, shaderType, targetProfile, version, true, splitTextureSamplers);

            File file_in_compute = File.createTempFile(FilenameUtils.getName(resourceOutput), ".cp");
            FileUtil.deleteOnExit(file_in_compute);
            FileUtils.writeByteArrayToFile(file_in_compute, es3Result.output.getBytes());

            file_out_spv = File.createTempFile(FilenameUtils.getName(resourceOutput), ".spv");
            FileUtil.deleteOnExit(file_out_spv);

            result = Exec.execResult(glslangExe,
                    "-w",
                    "-V",
                    "--entry-point", "main",
                    "--auto-map-bindings",
                    "--auto-map-locations",
                    "-Os",
                    "-S", "comp",
                    "-o", file_out_spv.getAbsolutePath(),
                    file_in_compute.getAbsolutePath());
        } else {
            int version = 140;
            if(targetProfile.equals("es")) {
                version = 310;
            }

            ShaderUtil.Common.GLSLShaderInfo shaderInfo = ShaderUtil.Common.getShaderInfo(shaderSource);

            // If the shader already has a version, we expect it to be already written in valid GLSL for that version
            if (shaderInfo == null) {
                // Convert to ES3 (or GL 140+)
                ShaderUtil.ES2ToES3Converter.Result es3Result = ShaderUtil.ES2ToES3Converter.transform(shaderSource, shaderType, targetProfile, version, true, splitTextureSamplers);

                // Update version for SPIR-V (GLES >= 310, Core >= 140)
                es3Result.shaderVersion = es3Result.shaderVersion.isEmpty() ? "0" : es3Result.shaderVersion;
                if(es3Result.shaderProfile.equals("es")) {
                    es3Result.shaderVersion = Integer.parseInt(es3Result.shaderVersion) < 310 ? "310" : es3Result.shaderVersion;
                } else {
                    es3Result.shaderVersion = Integer.parseInt(es3Result.shaderVersion) < 140 ? "140" : es3Result.shaderVersion;
                }

                shaderSource = es3Result.output;
            }

            // compile GLSL (ES3 or Desktop 140) to SPIR-V
            File file_in_glsl = File.createTempFile(FilenameUtils.getName(resourceOutput), ".glsl");
            FileUtil.deleteOnExit(file_in_glsl);
            FileUtils.writeByteArrayToFile(file_in_glsl, shaderSource.getBytes());

            file_out_spv = File.createTempFile(FilenameUtils.getName(resourceOutput), ".spv");
            FileUtil.deleteOnExit(file_out_spv);

            String spirvShaderStage = (shaderType == ShaderDesc.ShaderType.SHADER_TYPE_VERTEX ? "vert" : "frag");

            ArrayList<String> glslangArgs = new ArrayList<>();
            glslangArgs.add(glslangExe);
            glslangArgs.add("-w");
            glslangArgs.add("--entry-point");
            glslangArgs.add("main");
            glslangArgs.add("--auto-map-bindings");
            glslangArgs.add("--auto-map-locations");
            glslangArgs.add("-Os");
            glslangArgs.add("--resource-set-binding");
            glslangArgs.add("frag");
            glslangArgs.add("1");
            glslangArgs.add("-S");
            glslangArgs.add(spirvShaderStage);
            glslangArgs.add("-o");
            glslangArgs.add(file_out_spv.getAbsolutePath());
            glslangArgs.add("-V");
            for (String define : this.options.defines) {
                glslangArgs.add("-D" + define);
            }
            glslangArgs.add(file_in_glsl.getAbsolutePath());

            result = Exec.execResult(glslangArgs.toArray(new String[0]));
        }

        String resultString = getResultString(result);
        if (softFail && resultString != null) {
            res.compile_warnings.add("\nCompatability issue: " + resultString);
            return res;
        } else {
            checkResult(moduleLegacy.desc.resourcePath, resultString);
        }

        File file_out_spv_opt = File.createTempFile(FilenameUtils.getName(resourceOutput), ".spv");
        FileUtil.deleteOnExit(file_out_spv_opt);

        // Run optimization pass
        result = Exec.execResult(spirvOptExe,
                "-O",
                file_out_spv.getAbsolutePath(),
                "-o", file_out_spv_opt.getAbsolutePath());

        resultString = getResultString(result);
        if (softFail && resultString != null) {
            res.compile_warnings.add("\nOptimization pass failed: " + resultString);
            return res;
        } else {
            checkResult(moduleLegacy.desc.resourcePath, resultString);
        }

        moduleLegacy.spirvContext = ShadercJni.NewShaderContext(ToShadercShaderStageValue(moduleLegacy.desc.type), FileUtils.readFileToByteArray(file_out_spv));

        res.reflector = new SPIRVReflector(moduleLegacy.spirvContext, shaderType);
        res.source = FileUtils.readFileToByteArray(file_out_spv);

        return res;
    }

    private ShaderModuleLegacy getShaderModule(ShaderDesc.ShaderType shaderType) {
        for (ShaderModule m : this.shaderModules) {
            if (m.desc.type == shaderType && m instanceof ShaderModuleLegacy) {
                return (ShaderModuleLegacy) m;
            }
        }
        return null;
    }

    @Override
    protected void addShaderModule(ShaderModuleDesc desc) {
        shaderModules.add(new ShaderModuleLegacy(desc));
    }

    @Override
    protected void prepare() throws IOException, CompileExceptionError {
        // Generate spirv for each shader module so we can utilize the reflection from it
        for (ShaderModule module : this.shaderModules) {
            ShaderModuleLegacy moduleLegacy = (ShaderModuleLegacy) module;

            moduleLegacy.spirvResult = compileGLSLToSPIRV(moduleLegacy, this.pipelineName, "", false, this.options.splitTextureSamplers);
            moduleLegacy.spirvReflector = moduleLegacy.spirvResult.reflector;
        }
    }

    @Override
    public Shaderc.ShaderCompileResult crossCompile(ShaderDesc.ShaderType shaderType, ShaderDesc.Language shaderLanguage) throws CompileExceptionError, IOException {
        ShaderModuleLegacy module = getShaderModule(shaderType);
        if (module == null) {
            throw new CompileExceptionError("No module found for " + shaderType);
        }

        if (shaderLanguage == ShaderDesc.Language.LANGUAGE_SPIRV) {
            Shaderc.ShaderCompileResult result = new Shaderc.ShaderCompileResult();
            result.data = module.spirvResult.source;
            return result;
        } else if(shaderLanguage == ShaderDesc.Language.LANGUAGE_WGSL) {
            String compileResult = compileSPIRVToWGSL(module.desc.resourcePath, module.spirvResult.source, this.pipelineName);

            Shaderc.ShaderCompileResult result = new Shaderc.ShaderCompileResult();
            result.data = compileResult.getBytes();
            return result;
        } else if(shaderLanguage == ShaderDesc.Language.LANGUAGE_HLSL_51 ||
                  shaderLanguage == ShaderDesc.Language.LANGUAGE_HLSL_50 ||
                  shaderLanguage == ShaderDesc.Language.LANGUAGE_MSL_22) {
            Shaderc.ShaderCompileResult result = this.generateCrossCompiledShader(shaderType, shaderLanguage, this.ShaderLanguageToVersion(shaderLanguage));
            if (result.data == null) {
                throw new CompileExceptionError("Cannot cross-compile shader of type: " + shaderType + ", to language: " + shaderLanguage + ", reason: " + result.lastError);
            }
            return result;
        } else if (CanBeCrossCompiled(shaderLanguage)) {
            String compileResult = ShaderUtil.Common.compileGLSL(module.desc.source, shaderType, shaderLanguage, false, false, this.options.splitTextureSamplers);
            Shaderc.ShaderCompileResult result = new Shaderc.ShaderCompileResult();
            result.data = compileResult.getBytes();
            return result;
        }

        throw new CompileExceptionError("Unsupported shader language: " + shaderLanguage);
    }
}
