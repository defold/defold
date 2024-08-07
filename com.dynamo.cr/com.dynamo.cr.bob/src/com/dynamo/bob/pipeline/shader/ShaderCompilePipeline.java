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

import java.nio.charset.StandardCharsets;
import java.io.File;
import java.io.IOException;
import java.util.ArrayList;

import com.dynamo.bob.Bob;
import com.dynamo.bob.Platform;
import com.dynamo.bob.pipeline.ShaderUtil;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.util.Exec;
import com.dynamo.bob.util.FileUtil;
import com.dynamo.bob.util.Exec.Result;

import com.dynamo.graphics.proto.Graphics.ShaderDesc;

import org.apache.commons.io.FileUtils;

public class ShaderCompilePipeline {
    protected static class ShaderModule {
        public String                source;
        public ShaderDesc.ShaderType type;
        public File                  spirvFile;

        public ShaderModule(String source, ShaderDesc.ShaderType type) {
            this.source = source;
            this.type = type;
        }
    }

    protected String pipelineName;
    protected File spirvFileOut                     = null;
    protected SPIRVReflector spirvReflector         = null;
    protected ArrayList<ShaderModule> shaderModules = new ArrayList<>();

    public ShaderCompilePipeline(String pipelineName) {
        this.pipelineName = pipelineName;
    }

    protected void reset() {
        spirvFileOut = null;
        spirvReflector = null;
        shaderModules.clear();
    }

    private static String shaderTypeToSpirvStage(ShaderDesc.ShaderType shaderType) {
        return switch (shaderType) {
            case SHADER_TYPE_VERTEX -> "vert";
            case SHADER_TYPE_FRAGMENT -> "frag";
            case SHADER_TYPE_COMPUTE -> "comp";
        };
    }

    private static int shaderLanguageToVersion(ShaderDesc.Language shaderLanguage) {
        return switch (shaderLanguage) {
            case LANGUAGE_GLSL_SM120 -> 120;
            case LANGUAGE_GLSL_SM140 -> 140;
            case LANGUAGE_GLES_SM100 -> 100;
            case LANGUAGE_GLES_SM300 -> 300;
            case LANGUAGE_GLSL_SM330 -> 330;
            case LANGUAGE_GLSL_SM430 -> 430;
            default -> 0;
        };
    }

    protected static boolean canBeCrossCompiled(ShaderDesc.Language shaderLanguage) {
        return shaderLanguage == ShaderDesc.Language.LANGUAGE_GLSL_SM120 ||
               shaderLanguage == ShaderDesc.Language.LANGUAGE_GLSL_SM140 ||
               shaderLanguage == ShaderDesc.Language.LANGUAGE_GLES_SM100 ||
               shaderLanguage == ShaderDesc.Language.LANGUAGE_GLES_SM300 ||
               shaderLanguage == ShaderDesc.Language.LANGUAGE_GLSL_SM330 ||
               shaderLanguage == ShaderDesc.Language.LANGUAGE_GLSL_SM430 ||
               shaderLanguage == ShaderDesc.Language.LANGUAGE_WGSL;
    }

    private static void checkResult(Result result) throws CompileExceptionError {
        if (result.ret != 0) {
            String[] tokenizedResult = new String(result.stdOutErr).split(":", 2);
            String message = tokenizedResult[0];
            if(tokenizedResult.length != 1) {
                message = tokenizedResult[1];
            }

            throw new CompileExceptionError(message);
        }
    }

    protected static void generateWGSL(String pathFileInSpv, String pathFileOutWGSL) throws IOException, CompileExceptionError {
        Result result = Exec.execResult(Bob.getExe(Platform.getHostPlatform(), "tint"),
            "--format", "wgsl",
            "-o", pathFileOutWGSL,
            pathFileInSpv);
        checkResult(result);
    }

    private void generateSPIRv(ShaderDesc.ShaderType shaderType, String pathFileInGLSL, String pathFileOutSpv) throws IOException, CompileExceptionError {
        Result result = Exec.execResult(Bob.getExe(Platform.getHostPlatform(), "glslang"),
            "-w",
            "--entry-point", "main",
            "--auto-map-bindings",
            "--auto-map-locations",
            "-Os",
            "--resource-set-binding", "frag", "1",
            "-S", shaderTypeToSpirvStage(shaderType),
            "-o", pathFileOutSpv,
            "-V",
            pathFileInGLSL);
        checkResult(result);
    }

    private void generateSPIRvOptimized(String pathFileInSpv, String pathFileOutSpvOpt) throws IOException, CompileExceptionError{
        // Run optimization pass on the result
        Result result = Exec.execResult(Bob.getExe(Platform.getHostPlatform(), "spirv-opt"),
            "-O",
            pathFileInSpv,
            "-o", pathFileOutSpvOpt);
        checkResult(result);
    }

    private void generateSPIRvReflection(String pathFileInSpv, String pathFileOutSpvReflection) throws IOException, CompileExceptionError{
        Result result = Exec.execResult(Bob.getExe(Platform.getHostPlatform(), "spirv-cross"),
            pathFileInSpv,
            "--entry", "main",
            "--output", pathFileOutSpvReflection,
            "--reflect");
        checkResult(result);
    }

    private void generateCrossCompiledShader(ShaderDesc.ShaderType shaderType, ShaderDesc.Language shaderLanguage, String pathFileInSpv, String pathFileOut, int versionOut) throws IOException, CompileExceptionError{
        if(shaderLanguage == ShaderDesc.Language.LANGUAGE_WGSL) {
            generateWGSL(pathFileInSpv, pathFileOut);
            return;
        }

        ArrayList<String> args = new ArrayList<>();
        args.add(Bob.getExe(Platform.getHostPlatform(), "spirv-cross"));
        args.add(pathFileInSpv);
        args.add("--version");
        args.add(String.valueOf(versionOut));
        args.add("--output");
        args.add(pathFileOut);
        args.add("--entry");
        args.add("main");
        args.add("--stage");
        args.add(shaderTypeToSpirvStage(shaderType));
        args.add("--remove-unused-variables");
        args.add("--no-420pack-extension");

        if (shaderLanguage == ShaderDesc.Language.LANGUAGE_GLES_SM100 || shaderLanguage == ShaderDesc.Language.LANGUAGE_GLSL_SM120) {
            args.add("--glsl-emit-ubo-as-plain-uniforms");
        }

        if (shaderLanguage == ShaderDesc.Language.LANGUAGE_GLES_SM100 || shaderLanguage == ShaderDesc.Language.LANGUAGE_GLES_SM300) {
            args.add("--es");
        }

        Result result = Exec.execResult(args.toArray(new String[0]));
        checkResult(result);
    }

    protected void addShaderModule(String source, ShaderDesc.ShaderType type) {
        shaderModules.add(new ShaderModule(source, type));
    }

    protected void prepare() throws IOException, CompileExceptionError {
        if (this.shaderModules.size() == 0) {
            return;
        }

        // 1. Generate SPIR-V for each module that can be linked afterwards
        for (ShaderModule module : this.shaderModules) {
            String baseName = this.pipelineName + "." + shaderTypeToSpirvStage(module.type);

            File fileInGLSL = File.createTempFile(baseName, ".glsl");
            FileUtil.deleteOnExit(fileInGLSL);
            String glsl = ShaderUtil.Common.compileGLSL(module.source, module.type, ShaderDesc.Language.LANGUAGE_GLSL_SM430, false, true);
            FileUtils.writeByteArrayToFile(fileInGLSL, glsl.getBytes());

            File fileOutSpv = File.createTempFile(baseName, ".spv");
            FileUtil.deleteOnExit(fileOutSpv);
            generateSPIRv(module.type, fileInGLSL.getAbsolutePath(), fileOutSpv.getAbsolutePath());
            module.spirvFile = fileOutSpv;
        }

        // 2. TODO: link all the shader modules together. For now we only need to support single modules
        File fileOutSpvLinked = this.shaderModules.get(0).spirvFile;

        // 3. Generate an optimized version of the final .spv file
        File fileOutSpvOpt = File.createTempFile(this.pipelineName, ".optimized.spv");
        FileUtil.deleteOnExit(fileOutSpvOpt);
        generateSPIRvOptimized(fileOutSpvLinked.getAbsolutePath(), fileOutSpvOpt.getAbsolutePath());

        // 4. Generate the reflection data from the final .spv file
        File fileOutSpvReflection = File.createTempFile(this.pipelineName, ".reflection.json");
        FileUtil.deleteOnExit(fileOutSpvReflection);

        generateSPIRvReflection(fileOutSpvOpt.getAbsolutePath(), fileOutSpvReflection.getAbsolutePath());

        // 4. Finalize output
        this.spirvReflector = new SPIRVReflector(FileUtils.readFileToString(fileOutSpvReflection, StandardCharsets.UTF_8));
        this.spirvFileOut = fileOutSpvOpt;
    }

    //////////////////////////
    // PUBLIC API
    //////////////////////////
    public byte[] crossCompile(ShaderDesc.ShaderType shaderType, ShaderDesc.Language shaderLanguage) throws IOException, CompileExceptionError {
        if (shaderLanguage == ShaderDesc.Language.LANGUAGE_SPIRV) {
            return FileUtils.readFileToByteArray(this.spirvFileOut);
        } else if (canBeCrossCompiled(shaderLanguage)) {
            int version          = shaderLanguageToVersion(shaderLanguage);
            String shaderTypeStr = shaderTypeToSpirvStage(shaderType);
            String versionStr    = "v" + version;

            File fileCrossCompiled = File.createTempFile(this.pipelineName, "." + versionStr + "." + shaderTypeStr);
            FileUtil.deleteOnExit(fileCrossCompiled);

            generateCrossCompiledShader(shaderType, shaderLanguage, this.spirvFileOut.getAbsolutePath(), fileCrossCompiled.getAbsolutePath(), version);

            return FileUtils.readFileToByteArray(fileCrossCompiled);
        }

        throw new CompileExceptionError("Cannot crosscompile to shader language: " + shaderLanguage);
    }

    public SPIRVReflector getReflectionData() {
        return this.spirvReflector;
    }

    public static ShaderCompilePipeline createShaderPipeline(ShaderCompilePipeline pipeline, String source, ShaderDesc.ShaderType type) throws IOException, CompileExceptionError {
        pipeline.reset();
        pipeline.addShaderModule(source, type);
        pipeline.prepare();
        return pipeline;
    }
}
