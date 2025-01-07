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
import com.dynamo.bob.pipeline.Shaderc;
import com.dynamo.bob.pipeline.ShadercJni;
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
    protected long spirvContext                     = 0;
    protected ArrayList<ShaderModule> shaderModules = new ArrayList<>();
    protected Options options                       = null;

    private static String tintExe = null;
    private static String glslangExe = null;
    private static String spirvOptExe = null;

    public ShaderCompilePipeline(String pipelineName) throws IOException {
        this.pipelineName = pipelineName;
        if (tintExe == null)
            tintExe = Bob.getExe(Platform.getHostPlatform(), "tint");
        if (glslangExe == null)
            glslangExe = Bob.getExe(Platform.getHostPlatform(), "glslang");
        if (spirvOptExe == null)
            spirvOptExe = Bob.getExe(Platform.getHostPlatform(), "spirv-opt");
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

    private static byte[] remapTextureSamplers(ArrayList<Shaderc.ShaderResource> textures, String source) {
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
        for (Shaderc.ShaderResource texture : textures) {
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

    protected byte[] generateCrossCompiledShader(ShaderDesc.ShaderType shaderType, ShaderDesc.Language shaderLanguage, int versionOut) throws IOException, CompileExceptionError{

        long compiler = 0;

        if (shaderLanguage == ShaderDesc.Language.LANGUAGE_HLSL) {
            compiler = ShadercJni.NewShaderCompiler(this.spirvContext, Shaderc.ShaderLanguage.SHADER_LANGUAGE_HLSL.getValue());
        } else {
            compiler = ShadercJni.NewShaderCompiler(this.spirvContext, Shaderc.ShaderLanguage.SHADER_LANGUAGE_GLSL.getValue());
        }

        Shaderc.ShaderCompilerOptions opts = new Shaderc.ShaderCompilerOptions();
        opts.version               = versionOut;
        opts.entryPoint            = "main";
        opts.removeUnusedVariables = 1;
        opts.no420PackExtension    = 1;

        switch (shaderType) {
            case SHADER_TYPE_VERTEX -> opts.stage = Shaderc.ShaderStage.SHADER_STAGE_VERTEX;
            case SHADER_TYPE_FRAGMENT -> opts.stage = Shaderc.ShaderStage.SHADER_STAGE_FRAGMENT;
            case SHADER_TYPE_COMPUTE -> opts.stage = Shaderc.ShaderStage.SHADER_STAGE_COMPUTE;
        }

        if (shaderLanguage == ShaderDesc.Language.LANGUAGE_GLES_SM100 || shaderLanguage == ShaderDesc.Language.LANGUAGE_GLSL_SM120) {
            opts.glslEmitUboAsPlainUniforms = 1;
        }

        if (shaderLanguage == ShaderDesc.Language.LANGUAGE_GLES_SM100 || shaderLanguage == ShaderDesc.Language.LANGUAGE_GLES_SM300) {
            opts.glslEs = 1;
        }

        byte[] result = ShadercJni.Compile(this.spirvContext, compiler, opts);
        ShadercJni.DeleteShaderCompiler(compiler);
        return result;
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

        this.spirvContext = ShadercJni.NewShaderContext(FileUtils.readFileToByteArray(fileOutSpvOpt));
        this.spirvReflector = new SPIRVReflector(this.spirvContext);
        this.spirvFileOut = fileOutSpvOpt;
    }

    //////////////////////////
    // PUBLIC API
    //////////////////////////
    public byte[] crossCompile(ShaderDesc.ShaderType shaderType, ShaderDesc.Language shaderLanguage) throws IOException, CompileExceptionError {
        int version = shaderLanguageToVersion(shaderLanguage);

        if (shaderLanguage == ShaderDesc.Language.LANGUAGE_SPIRV) {
            return FileUtils.readFileToByteArray(this.spirvFileOut);
        } else if (shaderLanguage == ShaderDesc.Language.LANGUAGE_WGSL) {
            String shaderTypeStr = shaderTypeToSpirvStage(shaderType);
            String versionStr    = "v" + version;

            File fileCrossCompiled = File.createTempFile(this.pipelineName, "." + versionStr + "." + shaderTypeStr);
            FileUtil.deleteOnExit(fileCrossCompiled);

            generateWGSL(this.spirvFileOut.getAbsolutePath(), fileCrossCompiled.getAbsolutePath());
            return FileUtils.readFileToByteArray(fileCrossCompiled);
        } else if (canBeCrossCompiled(shaderLanguage)) {
            byte[] bytes = generateCrossCompiledShader(shaderType, shaderLanguage, version);

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

    public static void destroyShaderPipeline(ShaderCompilePipeline pipeline) {
        if (pipeline.spirvContext != 0) {
            ShadercJni.DeleteShaderContext(pipeline.spirvContext);
            pipeline.spirvContext = 0;
            pipeline.reset();
        }
    }
}
