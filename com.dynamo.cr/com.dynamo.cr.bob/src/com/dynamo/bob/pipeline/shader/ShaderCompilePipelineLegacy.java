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

package com.dynamo.bob.pipeline.shader;

import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;

import com.dynamo.bob.Bob;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Platform;
import com.dynamo.bob.fs.IResource;
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

    protected class ShaderModuleLegacy extends ShaderCompilePipeline.ShaderModule {
        public SPIRVCompileResult spirvResult;

        public ShaderModuleLegacy(String source, ShaderDesc.ShaderType type) {
            super(source, type);
        }
    }

	public ShaderCompilePipelineLegacy(String pipelineName) {
        super(pipelineName);
    }

    static private void checkResult(String result_string, IResource resource, String resourceOutput) throws CompileExceptionError {
        if (result_string != null ) {
            if(resource != null) {
                throw new CompileExceptionError(resource, 0, result_string);
            } else {
                throw new CompileExceptionError(resourceOutput + ":" + result_string, null);
            }
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

    static private String compileSPIRVToWGSL(byte[] shaderSource, String resourceOutput)  throws IOException, CompileExceptionError {
        File file_in_spv = File.createTempFile(FilenameUtils.getName(resourceOutput), ".spv");
        FileUtil.deleteOnExit(file_in_spv);
        FileUtils.writeByteArrayToFile(file_in_spv, shaderSource);

        File file_out_wgsl = File.createTempFile(FilenameUtils.getName(resourceOutput), ".wgsl");
        FileUtil.deleteOnExit(file_out_wgsl);
        generateWGSL(file_in_spv.getAbsolutePath(), file_out_wgsl.getAbsolutePath());
        return FileUtils.readFileToString(file_out_wgsl);
    }

    static private SPIRVCompileResult compileGLSLToSPIRV(String shaderSource, ShaderDesc.ShaderType shaderType, String resourceOutput, String targetProfile, boolean isDebug, boolean soft_fail)  throws IOException, CompileExceptionError {
        SPIRVCompileResult res = new SPIRVCompileResult();

        Exec.Result result;
        File file_out_spv;

        if (shaderType == ShaderDesc.ShaderType.SHADER_TYPE_COMPUTE) {

            int version = 430;

            ShaderUtil.ES2ToES3Converter.Result es3Result = ShaderUtil.ES2ToES3Converter.transform(shaderSource, shaderType, targetProfile, version, true);

            File file_in_compute = File.createTempFile(FilenameUtils.getName(resourceOutput), ".cp");
            FileUtil.deleteOnExit(file_in_compute);
            FileUtils.writeByteArrayToFile(file_in_compute, es3Result.output.getBytes());

            file_out_spv = File.createTempFile(FilenameUtils.getName(resourceOutput), ".spv");
            FileUtil.deleteOnExit(file_out_spv);

            result = Exec.execResult(Bob.getExe(Platform.getHostPlatform(), "glslang"),
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
                ShaderUtil.ES2ToES3Converter.Result es3Result = ShaderUtil.ES2ToES3Converter.transform(shaderSource, shaderType, targetProfile, version, true);

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
            result = Exec.execResult(Bob.getExe(Platform.getHostPlatform(), "glslang"),
                    "-w",
                    "-V",
                    "--entry-point", "main",
                    "--auto-map-bindings",
                    "--auto-map-locations",
                    "--resource-set-binding", "frag", "1",
                    "-Os",
                    "-S", spirvShaderStage,
                    "-o", file_out_spv.getAbsolutePath(),
                    file_in_glsl.getAbsolutePath());
        }

        String resultString = getResultString(result);
        if (soft_fail && resultString != null) {
            res.compile_warnings.add("\nCompatability issue: " + resultString);
            return res;
        } else {
            checkResult(resultString, null, resourceOutput);
        }

        File file_out_spv_opt = File.createTempFile(FilenameUtils.getName(resourceOutput), ".spv");
        FileUtil.deleteOnExit(file_out_spv_opt);

        // Run optimization pass
        result = Exec.execResult(Bob.getExe(Platform.getHostPlatform(), "spirv-opt"),
                "-O",
                file_out_spv.getAbsolutePath(),
                "-o", file_out_spv_opt.getAbsolutePath());

        resultString = getResultString(result);
        if (soft_fail && resultString != null) {
            res.compile_warnings.add("\nOptimization pass failed: " + resultString);
            return res;
        } else {
            checkResult(resultString, null, resourceOutput);
        }

        // Generate reflection data
        File file_out_refl = File.createTempFile(FilenameUtils.getName(resourceOutput), ".json");
        FileUtil.deleteOnExit(file_out_refl);

        result = Exec.execResult(Bob.getExe(Platform.getHostPlatform(), "spirv-cross"),
                file_out_spv_opt.getAbsolutePath(),
                "--output",file_out_refl.getAbsolutePath(),
                "--reflect");

        resultString = getResultString(result);
        if (soft_fail && resultString != null) {
            res.compile_warnings.add("\nUnable to get reflection data: " + resultString);
            return res;
        } else {
            checkResult(resultString, null, resourceOutput);
        }

        String result_json = FileUtils.readFileToString(file_out_refl, StandardCharsets.UTF_8);

        res.reflector = new SPIRVReflector(result_json);
        res.source = FileUtils.readFileToByteArray(file_out_spv);

        return res;
    }

    private ShaderModuleLegacy getShaderModule(ShaderDesc.ShaderType shaderType) {
        for (ShaderModule m : this.shaderModules) {
            if (m.type == shaderType && m instanceof ShaderModuleLegacy) {
                return (ShaderModuleLegacy) m;
            }
        }
        return null;
    }

    @Override
    protected void addShaderModule(String source, ShaderDesc.ShaderType type) {
        shaderModules.add(new ShaderModuleLegacy(source, type));
    }

    @Override
    protected void prepare() throws IOException, CompileExceptionError {
        // Generate spirv for each shader module so we can utilize the reflection from it
        for (ShaderModule module : this.shaderModules) {
            SPIRVCompileResult result = compileGLSLToSPIRV(module.source, module.type, this.pipelineName, "", false, false);

            ShaderModuleLegacy moduleLegacy = (ShaderModuleLegacy) module;
            moduleLegacy.spirvResult = result;
            this.spirvReflector = moduleLegacy.spirvResult.reflector;
        }
    }

    @Override
    public byte[] crossCompile(ShaderDesc.ShaderType shaderType, ShaderDesc.Language shaderLanguage) throws CompileExceptionError, IOException {
        ShaderModuleLegacy module = getShaderModule(shaderType);
        if (module == null) {
            throw new CompileExceptionError("No module found for " + shaderType);
        }

        this.spirvReflector = module.spirvResult.reflector;

        if (shaderLanguage == ShaderDesc.Language.LANGUAGE_SPIRV) {
            return module.spirvResult.source;
        } else if(shaderLanguage == ShaderDesc.Language.LANGUAGE_WGSL) {
            String result = compileSPIRVToWGSL(module.spirvResult.source, this.pipelineName);
            return result.getBytes();
        } else if (canBeCrossCompiled(shaderLanguage)) {
            String result = ShaderUtil.Common.compileGLSL(module.source, shaderType, shaderLanguage, false, false);
            return result.getBytes();
        }

        throw new CompileExceptionError("Unsupported shader language: " + shaderLanguage);
    }
}
