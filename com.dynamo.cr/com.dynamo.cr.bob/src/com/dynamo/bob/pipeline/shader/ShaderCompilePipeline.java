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

import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.nio.file.Path;
import java.io.File;
import java.io.IOException;
import java.util.List;
import java.util.ArrayList;

import java.util.HashMap;
import java.util.Map;

import com.dynamo.bob.Bob;
import com.dynamo.bob.Platform;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.util.Exec;
import com.dynamo.bob.util.FileUtil;
import com.dynamo.bob.util.Exec.Result;
import com.dynamo.bob.pipeline.ShaderUtil.SPIRVReflector;

import com.dynamo.graphics.proto.Graphics.ShaderDesc;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;

public class ShaderCompilePipeline {

    private class ShaderModule {
        public String                source;
        public ShaderDesc.ShaderType type;
        public File                  spirvFile;

        public ShaderModule(String source, ShaderDesc.ShaderType type) {
            this.source = source;
            this.type = type;
        }
    };

    private String pipelineName                   = null;
    private File spirvFileOut                     = null;
    private SPIRVReflector spirvReflector         = null;
    private ArrayList<ShaderModule> shaderModules = new ArrayList<ShaderModule>();

    public ShaderCompilePipeline(String pipelineName) {
        this.pipelineName = pipelineName;
    }

    public static ShaderCompilePipeline createShaderPipelineGraphics(String pipelineName, String vertexShaderSource, String fragmentShaderSource) throws IOException, CompileExceptionError {
        ShaderCompilePipeline pipeline = new ShaderCompilePipeline(pipelineName);
        pipeline.addShaderModule(vertexShaderSource, ShaderDesc.ShaderType.SHADER_TYPE_VERTEX);
        pipeline.addShaderModule(fragmentShaderSource, ShaderDesc.ShaderType.SHADER_TYPE_FRAGMENT);
        pipeline.prepare();
        return pipeline;
    }

    public static ShaderCompilePipeline createShaderPipeline(String pipelineName, String source, ShaderDesc.ShaderType type) throws IOException, CompileExceptionError {
        ShaderCompilePipeline pipeline = new ShaderCompilePipeline(pipelineName);
        pipeline.addShaderModule(source, type);
        pipeline.prepare();
        return pipeline;
    }

    public static ShaderCompilePipeline createShaderPipelineCompute(String pipelineName, String computeShaderSource) throws IOException, CompileExceptionError {
        return createShaderPipeline(pipelineName, computeShaderSource, ShaderDesc.ShaderType.SHADER_TYPE_COMPUTE);
    }

    private void addShaderModule(String source, ShaderDesc.ShaderType type) {
        shaderModules.add(new ShaderModule(source, type));
    }

    private static ShaderDesc.ShaderType pathToShaderType(String path) throws CompileExceptionError {
        if (path.endsWith(".vp")) {
            return ShaderDesc.ShaderType.SHADER_TYPE_VERTEX;
        } else if (path.endsWith(".fp")) {
            return ShaderDesc.ShaderType.SHADER_TYPE_FRAGMENT;
        } else if (path.endsWith(".cp")) {
            return ShaderDesc.ShaderType.SHADER_TYPE_COMPUTE;
        }
        throw new CompileExceptionError("Unable to deduce shader type from path: " + path);
    }

    private static String shaderTypeToSpirvStage(ShaderDesc.ShaderType shaderType) throws CompileExceptionError {
        switch (shaderType) {
            case SHADER_TYPE_VERTEX:
                return "vert";
            case SHADER_TYPE_FRAGMENT:
                return "frag";
            case SHADER_TYPE_COMPUTE:
                return "compute";
        }
        throw new CompileExceptionError("Unknown shader type: " + shaderType);
    }

    private static int shaderLanguageToVersion(ShaderDesc.Language shaderLanguage) {
        switch (shaderLanguage) {
            case LANGUAGE_GLSL_SM120:
                return 120;
            case LANGUAGE_GLSL_SM140:
                return 140;
            case LANGUAGE_GLES_SM100:
                return 100;
            case LANGUAGE_GLES_SM300:
                return 300;
            case LANGUAGE_GLSL_SM430:
                return 430;
        }
        return 0;
    }

    private static ShaderDesc.Language shaderLanguageStrToEnum(String fromString) throws CompileExceptionError {
        if (fromString.equals("100")) {
            return ShaderDesc.Language.LANGUAGE_GLES_SM100;
        } else if (fromString.equals("120")) {
            return ShaderDesc.Language.LANGUAGE_GLSL_SM120;
        } else if (fromString.equals("140")) {
            return ShaderDesc.Language.LANGUAGE_GLSL_SM140;
        } else if (fromString.equals("300")) {
            return ShaderDesc.Language.LANGUAGE_GLES_SM300;
        } else if (fromString.equals("430")) {
            return ShaderDesc.Language.LANGUAGE_GLSL_SM430;
        } else if (fromString.equals("spv")) {
            return ShaderDesc.Language.LANGUAGE_SPIRV;
        }
        throw new CompileExceptionError("Unknown shader language: " + fromString);
    }

    private static boolean canBeCrossCompiled(ShaderDesc.Language shaderLanguage) {
        return shaderLanguage == ShaderDesc.Language.LANGUAGE_GLSL_SM120 ||
               shaderLanguage == ShaderDesc.Language.LANGUAGE_GLSL_SM140 ||
               shaderLanguage == ShaderDesc.Language.LANGUAGE_GLES_SM100 ||
               shaderLanguage == ShaderDesc.Language.LANGUAGE_GLES_SM300 ||
               shaderLanguage == ShaderDesc.Language.LANGUAGE_GLSL_SM430;
    }

    private static void checkResult(Result result) throws CompileExceptionError {
        if (result.ret != 0) {
            String[] tokenizedResult = new String(result.stdOutErr).split(":", 2);
            String message = tokenizedResult[0];
            if(tokenizedResult.length != 1) {
                message = tokenizedResult[1];
            }

            String err = new String(result.stdOutErr);

            System.out.println("Error: " + err);
            throw new CompileExceptionError(message);
        }
    }

    private void generateSPIRv(ShaderDesc.ShaderType shaderType, String fullShaderSource, String pathFileInGLSL, String pathFileOutSpv) throws IOException, CompileExceptionError {
        Result result = Exec.execResult(Bob.getExe(Platform.getHostPlatform(), "glslc"),
            "-w",
            "-fauto-bind-uniforms",
            "-fauto-map-locations",
            "-std=140", // TODO
            // "-std=" + shaderVersionStr + shaderProfileStr,
            "-fshader-stage=" + shaderTypeToSpirvStage(shaderType),
            "-o", pathFileOutSpv,
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
            "--output", pathFileOutSpvReflection,
            "--reflect");
        checkResult(result);
    }

    private void generateCrossCompiledShader(ShaderDesc.ShaderType shaderType, ShaderDesc.Language shaderLanguage, String pathFileInSpv, String pathFileOut, int versionOut) throws IOException, CompileExceptionError{

        ArrayList<String> options = new ArrayList<String>();
        options.add("--remove-unused-variables");

        if (shaderLanguage == ShaderDesc.Language.LANGUAGE_GLES_SM100 || shaderLanguage == ShaderDesc.Language.LANGUAGE_GLSL_SM120) {
            options.add("--glsl-emit-ubo-as-plain-uniforms");
        }

        if (shaderLanguage == ShaderDesc.Language.LANGUAGE_GLES_SM100 || shaderLanguage == ShaderDesc.Language.LANGUAGE_GLES_SM300) {
            options.add("--es");
        }

        String optionsStr = String.join(" ", list);

        Result result = Exec.execResult(Bob.getExe(Platform.getHostPlatform(), "spirv-cross"),
            pathFileInSpv,
            "--version", String.valueOf(versionOut),
            "--output", pathFileOut,
            "--stage", shaderTypeToSpirvStage(shaderType),
            optionsStr
        );
        checkResult(result);
    }

    private void linkSPIRvModules(ArrayList<ShaderModule> shaderModules, String pathFileOutSpvLinked) throws IOException, CompileExceptionError{
        String allFiles = "";
        for (ShaderModule s : shaderModules) {
            allFiles += s.spirvFile.getAbsolutePath() + " ";
        }

        Result result = Exec.execResult(Bob.getExe(Platform.getHostPlatform(), "spirv-link"),
            allFiles,
            "--output", pathFileOutSpvLinked);
        checkResult(result);
    }

    private void prepare() throws IOException, CompileExceptionError {
        if (this.shaderModules.size() == 0) {
            return;
        }

        // 1. Generate SPIR-V for each module that can be linked afterwards
        for (ShaderModule module : this.shaderModules) {
            String baseName = this.pipelineName + "." + shaderTypeToSpirvStage(module.type);

            File fileInGLSL = File.createTempFile(baseName, ".glsl");
            FileUtil.deleteOnExit(fileInGLSL);
            FileUtils.writeByteArrayToFile(fileInGLSL, module.source.getBytes());

            File fileOutSpv = File.createTempFile(baseName, ".spv");
            FileUtil.deleteOnExit(fileOutSpv);

            generateSPIRv(module.type, module.source, fileInGLSL.getAbsolutePath(), fileOutSpv.getAbsolutePath());

            module.spirvFile = fileOutSpv;
        }

        // Link step is only needed if there is more than one module
        File fileOutSpvLinked = null;
        if (this.shaderModules.size() == 1) {
            fileOutSpvLinked = this.shaderModules.get(0).spirvFile;
        } else {
            // 2. Link all the .spv files together
            fileOutSpvLinked = File.createTempFile(this.pipelineName, ".linked.spv");
            FileUtil.deleteOnExit(fileOutSpvLinked);
            linkSPIRvModules(this.shaderModules, fileOutSpvLinked.getAbsolutePath());
        }

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

    // java -classpath $DYNAMO_HOME/share/java/bob-light.jar com.dynamo.bob.pipeline.ShaderCompilePipeline <path-in.stage> <path-out.stagec>
    // where .stage is either .vp for vertex shaders, .fp for fragment shaders or .cp for compute shaders
    public static void main(String[] args) throws IOException, CompileExceptionError {
        System.setProperty("java.awt.headless", "true");

        if (args.length < 2) {
            System.out.println("Usage: java -classpath $DYNAMO_HOME/share/java/bob-light.jar com.dynamo.bob.pipeline.ShaderCompilePipeline <path-in.stage> <path-out.stage> [shader-language]");
            System.exit(1);
        }

        /*
        String shaderIn  = args[0];
        String shaderOut = args[1];
        String shaderLanguage = null;

        if (args.length > 2) {
            shaderLanguage = args[2];
        }

        ShaderDesc.ShaderType shaderType = pathToShaderType(shaderIn);

        List<String> lines = Files.readAllLines(Paths.get(shaderIn));
        String content     = String.join("\n", lines);

        ShaderCompilePipeline pipeline = createShaderPipelineGraphics("ShaderCompilePipelineBuilder", String vertexShaderSource, String fragmentShaderSource);

        if (shaderLanguage == null) {
            pipeline.addOutputLanguage(ShaderDesc.Language.LANGUAGE_GLES_SM100);
            pipeline.addOutputLanguage(ShaderDesc.Language.LANGUAGE_GLSL_SM120);
            pipeline.addOutputLanguage(ShaderDesc.Language.LANGUAGE_GLSL_SM140);
            pipeline.addOutputLanguage(ShaderDesc.Language.LANGUAGE_GLES_SM300);
            pipeline.addOutputLanguage(ShaderDesc.Language.LANGUAGE_GLSL_SM430);
            pipeline.addOutputLanguage(ShaderDesc.Language.LANGUAGE_SPIRV);
        } else {
            pipeline.addOutputLanguage(shaderLanguageStrToEnum(shaderLanguage));
        }
        pipeline.compile(shaderType, content, shaderIn);
        */
    }
}
