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
    public static class Options {
        public boolean splitTextureSamplers;
    }

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
    protected Options options                       = null;

    private static String tintExe = null;
    private static String glslangExe = null;
    private static String spirvOptExe = null;
    private static String spirvCrossExe = null;

    public ShaderCompilePipeline(String pipelineName) throws IOException {
        this.pipelineName = pipelineName;
        if (this.tintExe == null) this.tintExe = Bob.getExe(Platform.getHostPlatform(), "tint");
        if (this.glslangExe == null) this.glslangExe = Bob.getExe(Platform.getHostPlatform(), "glslang");
        if (this.spirvOptExe == null) this.spirvOptExe = Bob.getExe(Platform.getHostPlatform(), "spirv-opt");
        if (this.spirvCrossExe == null) this.spirvCrossExe = Bob.getExe(Platform.getHostPlatform(), "spirv-cross");
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

    protected static boolean shaderLanguageIsGLSL(ShaderDesc.Language shaderLanguage) {
        return shaderLanguage == ShaderDesc.Language.LANGUAGE_GLSL_SM120 ||
               shaderLanguage == ShaderDesc.Language.LANGUAGE_GLSL_SM140 ||
               shaderLanguage == ShaderDesc.Language.LANGUAGE_GLES_SM100 ||
               shaderLanguage == ShaderDesc.Language.LANGUAGE_GLES_SM300 ||
               shaderLanguage == ShaderDesc.Language.LANGUAGE_GLSL_SM330 ||
               shaderLanguage == ShaderDesc.Language.LANGUAGE_GLSL_SM430;
    }

    protected static boolean canBeCrossCompiled(ShaderDesc.Language shaderLanguage) {
        return shaderLanguageIsGLSL(shaderLanguage) || shaderLanguage == ShaderDesc.Language.LANGUAGE_WGSL;
    }

    private static byte[] remapTextureSamplers(ArrayList<SPIRVReflector.Resource> textures, String source) {
        // Textures are remapped via spirv-cross according to:
        //   SPIRV_Cross_Combined<TEXTURE_NAME><SAMPLER_NAME>
        //
        // Due to WGSL, before we pass the source into crosscompilation, we 'expand' samplers into texture + sampler:
        //   uniform sampler2D my_texture;
        // which becomes:
        //   uniform texture2D my_texture;
        //   uniform sampler   my_texture_separated;
        //
        // So these two rules together then becomes:
        //   uniform sampler2D my_texture; ==> SPIRV_Cross_Combinedsampler_2dsampler_2d_separated
        //
        // Even without the separation of texture/sampler, we will still need to rename the texture in the source
        // due to how spirv-cross works.
        for (SPIRVReflector.Resource texture : textures) {
            String spirvCrossSamplerName = String.format("SPIRV_Cross_Combined%s%s_separated", texture.name, texture.name);
            source = source.replaceAll(spirvCrossSamplerName, texture.name);
        }
        return source.getBytes(StandardCharsets.UTF_8);
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
        Result result = Exec.execResult(tintExe,
            "--format", "wgsl",
            "-o", pathFileOutWGSL,
            pathFileInSpv);
        checkResult(result);
    }

    private void generateSPIRv(ShaderDesc.ShaderType shaderType, String pathFileInGLSL, String pathFileOutSpv) throws IOException, CompileExceptionError {
        Result result = Exec.execResult(glslangExe,
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
        Result result = Exec.execResult(spirvOptExe,
            "-O",
            pathFileInSpv,
            "-o", pathFileOutSpvOpt);
        checkResult(result);
    }

    private void generateSPIRvReflection(String pathFileInSpv, String pathFileOutSpvReflection) throws IOException, CompileExceptionError{
        Result result = Exec.execResult(spirvCrossExe,
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
        args.add(spirvCrossExe);
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

            String glsl = module.source;
            if (this.options.splitTextureSamplers) {
                // We need to expand all combined samplers into texture + sampler due for certain targets (webgpu + dx12)
                glsl = ShaderUtil.ES2ToES3Converter.transformTextureUniforms(module.source).output;
            }
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

            byte[] bytes = FileUtils.readFileToByteArray(fileCrossCompiled);

            // JG: spirv-cross renames samplers for GLSL based shaders, so we have to run a second pass to force renaming them back.
            //     There doesn't seem to be a simpler way to do this in spirv-cross from what I can understand.
            if (shaderLanguageIsGLSL(shaderLanguage)) {
                bytes = remapTextureSamplers(this.spirvReflector.getTextures(), new String(bytes));
            }

            return bytes;
        }

        throw new CompileExceptionError("Cannot crosscompile to shader language: " + shaderLanguage);
    }

    public SPIRVReflector getReflectionData() {
        return this.spirvReflector;
    }

    public static ShaderCompilePipeline createShaderPipeline(ShaderCompilePipeline pipeline, String source, ShaderDesc.ShaderType type, Options options) throws IOException, CompileExceptionError {
        pipeline.options = options;
        pipeline.reset();
        pipeline.addShaderModule(source, type);
        pipeline.prepare();
        return pipeline;
    }
}
