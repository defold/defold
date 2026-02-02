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

import java.nio.charset.StandardCharsets;
import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import com.dynamo.bob.Bob;
import com.dynamo.bob.Platform;
import com.dynamo.bob.pipeline.ShaderUtil;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.pipeline.Shaderc;
import com.dynamo.bob.pipeline.ShadercJni;
import com.dynamo.bob.util.Exec;
import com.dynamo.bob.util.FileUtil;
import com.dynamo.bob.util.Exec.Result;
import com.dynamo.bob.util.MurmurHash;

import com.dynamo.graphics.proto.Graphics.ShaderDesc;

import org.apache.commons.io.FileUtils;

public class ShaderCompilePipeline {
    public static class Options {
        public boolean splitTextureSamplers;
        public ArrayList<String> defines = new ArrayList<>();
    }

    public static class ShaderModuleDesc {
        public String source;
        public String resourcePath;
        public ShaderDesc.ShaderType type;
    }

    protected static class ShaderModule {
        ShaderModuleDesc desc;
        public File spirvFile;
        protected SPIRVReflector spirvReflector = null;
        protected long spirvContext = 0;
        protected ShaderUtil.Common.GLSLShaderInfo shaderInfo;

        public ShaderModule(ShaderModuleDesc desc) {
            this.desc = desc;
        }
    }

    protected String pipelineName;
    protected ArrayList<ShaderModule> shaderModules = new ArrayList<>();
    protected Options options                       = null;

    private static String tintExe = null;
    private static String glslangExe = null;
    private static String spirvOptExe = null;
    private static String spirvCrossExe = null;

    public ShaderCompilePipeline(String pipelineName) throws IOException {
        this.pipelineName = pipelineName;
        tintExe = Bob.getHostExeOnce("tint", tintExe);
        glslangExe = Bob.getHostExeOnce("glslang", glslangExe);
        spirvOptExe = Bob.getHostExeOnce("spirv-opt", spirvOptExe);
    }

    protected void reset() {
        shaderModules.clear();
    }

    private static String ShaderTypeToSpirvStage(ShaderDesc.ShaderType shaderType) {
        return switch (shaderType) {
            case SHADER_TYPE_VERTEX -> "vert";
            case SHADER_TYPE_FRAGMENT -> "frag";
            case SHADER_TYPE_COMPUTE -> "comp";
        };
    }

    protected static Integer ShaderLanguageToVersion(ShaderDesc.Language shaderLanguage) {
        return switch (shaderLanguage) {
            case LANGUAGE_GLSL_SM120 -> 120;
            case LANGUAGE_GLES_SM100 -> 100;
            case LANGUAGE_GLES_SM300 -> 300;
            case LANGUAGE_GLSL_SM330 -> 330;
            case LANGUAGE_GLSL_SM430 -> 430;
            case LANGUAGE_HLSL_50    -> 50;
            case LANGUAGE_HLSL_51    -> 51;
            case LANGUAGE_MSL_22     -> 22;
            default -> 0;
        };
    }

    protected static int ToShadercShaderStageValue(ShaderDesc.ShaderType type) {
        return switch (type) {
            case SHADER_TYPE_VERTEX -> Shaderc.ShaderStage.SHADER_STAGE_VERTEX.getValue();
            case SHADER_TYPE_FRAGMENT -> Shaderc.ShaderStage.SHADER_STAGE_FRAGMENT.getValue();
            case SHADER_TYPE_COMPUTE -> Shaderc.ShaderStage.SHADER_STAGE_COMPUTE.getValue();
        };
    }

    protected static boolean ShaderLanguageIsGLSL(ShaderDesc.Language shaderLanguage) {
        return shaderLanguage == ShaderDesc.Language.LANGUAGE_GLSL_SM120 ||
               shaderLanguage == ShaderDesc.Language.LANGUAGE_GLES_SM100 ||
               shaderLanguage == ShaderDesc.Language.LANGUAGE_GLES_SM300 ||
               shaderLanguage == ShaderDesc.Language.LANGUAGE_GLSL_SM330 ||
               shaderLanguage == ShaderDesc.Language.LANGUAGE_GLSL_SM430;
    }

    protected static boolean CanBeCrossCompiled(ShaderDesc.Language shaderLanguage) {
        return ShaderLanguageIsGLSL(shaderLanguage) ||
               shaderLanguage == ShaderDesc.Language.LANGUAGE_WGSL ||
               shaderLanguage == ShaderDesc.Language.LANGUAGE_HLSL_50 ||
               shaderLanguage == ShaderDesc.Language.LANGUAGE_HLSL_51 ||
               shaderLanguage == ShaderDesc.Language.LANGUAGE_MSL_22;
    }

    private static byte[] RemapTextureSamplers(ArrayList<Shaderc.ShaderResource> textures, String source) {
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

    protected static String intermediateResourceToProjectPath(String message, String projectPath) {
        String[] knownFileExtensions = new String[] { "glsl", "spv", "wgsl", "hlsl" };
        String replacementRegex = String.format("/[^\\s:]+\\.(%s)(?=:)", String.join("|", knownFileExtensions));
        if (projectPath == null) {
            return message;
        }
        return message.replaceAll(replacementRegex, projectPath);
    }

    private static void checkResult(String resourcePath, Result result) throws CompileExceptionError {
        if (result.ret != 0) {
            String[] tokenizedResult = new String(result.stdOutErr).split(":", 2);
            String message = tokenizedResult[0];
            if(tokenizedResult.length != 1) {
                message = tokenizedResult[1];
            }
            message = intermediateResourceToProjectPath(message, resourcePath);
            throw new CompileExceptionError(message);
        }
    }

    protected static void generateWGSL(String resourcePath, String pathFileInSpv, String pathFileOutWGSL) throws IOException, CompileExceptionError {
        Result result = Exec.execResult(tintExe,
            "--format", "wgsl",
            "-o", pathFileOutWGSL,
            pathFileInSpv);
        checkResult(resourcePath, result);
    }

    private void generateSPIRv(String resourcePath, ShaderDesc.ShaderType shaderType, String pathFileInGLSL, String pathFileOutSpv) throws IOException, CompileExceptionError {
        ArrayList<String> args = new ArrayList<>();
        args.add(glslangExe);
        args.add("-w");
        args.add("--entry-point");
        args.add("main");
        args.add("--auto-map-bindings");
        args.add("--auto-map-locations");
        args.add("-Os");
        args.add("--resource-set-binding");
        args.add("frag");
        args.add("1");
        args.add("-S");
        args.add(ShaderTypeToSpirvStage(shaderType));
        args.add("-o");
        args.add(pathFileOutSpv);
        args.add("-V");
        for (String define : this.options.defines) {
            args.add("-D" + define);
        }
        args.add(pathFileInGLSL);

        Result result = Exec.execResult(args.toArray(new String[0]));
        checkResult(resourcePath, result);
    }

    private void generateSPIRvOptimized(String resourcePath, String pathFileInSpv, String pathFileOutSpvOpt) throws IOException, CompileExceptionError{
        // Run optimization pass on the result
        Result result = Exec.execResult(spirvOptExe,
            "-O",
            pathFileInSpv,
            "-o", pathFileOutSpvOpt);
        checkResult(resourcePath, result);
    }

    private ShaderModule getShaderModule(ShaderDesc.ShaderType shaderStage) {
        for (ShaderModule module : shaderModules) {
            if (module.desc.type == shaderStage) {
                return module;
            }
        }
        return null;
    }

    protected Shaderc.ShaderCompileResult generateCrossCompiledShader(ShaderDesc.ShaderType shaderType, ShaderDesc.Language shaderLanguage, int versionOut) {

        long compiler = 0;

        ShaderModule module = getShaderModule(shaderType);
        assert module != null;

        if (shaderLanguage == ShaderDesc.Language.LANGUAGE_HLSL_51 ||
            shaderLanguage == ShaderDesc.Language.LANGUAGE_HLSL_50) {
            compiler = ShadercJni.NewShaderCompiler(module.spirvContext, Shaderc.ShaderLanguage.SHADER_LANGUAGE_HLSL.getValue());
        } else if (shaderLanguage == ShaderDesc.Language.LANGUAGE_MSL_22) {
            compiler = ShadercJni.NewShaderCompiler(module.spirvContext, Shaderc.ShaderLanguage.SHADER_LANGUAGE_MSL.getValue());
        } else {
            compiler = ShadercJni.NewShaderCompiler(module.spirvContext, Shaderc.ShaderLanguage.SHADER_LANGUAGE_GLSL.getValue());
        }

        Shaderc.ShaderCompilerOptions opts = new Shaderc.ShaderCompilerOptions();
        opts.version               = versionOut;
        opts.entryPoint            = "main";
        opts.removeUnusedVariables = 1;
        opts.no420PackExtension    = 1;

        if (shaderLanguage == ShaderDesc.Language.LANGUAGE_GLES_SM100 || shaderLanguage == ShaderDesc.Language.LANGUAGE_GLSL_SM120) {
            opts.glslEmitUboAsPlainUniforms = 1;
        }

        if (shaderLanguage == ShaderDesc.Language.LANGUAGE_GLES_SM100 || shaderLanguage == ShaderDesc.Language.LANGUAGE_GLES_SM300) {
            opts.glslEs = 1;
        }

        Shaderc.ShaderCompileResult result = ShadercJni.Compile(module.spirvContext, compiler, opts);
        ShadercJni.DeleteShaderCompiler(compiler);
        return result;
    }

    protected void addShaderModule(ShaderModuleDesc desc) {
        shaderModules.add(new ShaderModule(desc));
    }

    protected void prepare() throws IOException, CompileExceptionError {
        if (this.shaderModules.size() == 0) {
            return;
        }

        ShaderModule vertexModule = null;
        ShaderModule fragmentModule = null;

        // Generate SPIR-V for each module
        for (ShaderModule module : this.shaderModules) {
            String baseName = this.pipelineName + "." + ShaderTypeToSpirvStage(module.desc.type);

            File fileInGLSL = File.createTempFile(baseName, ".glsl");
            FileUtil.deleteOnExit(fileInGLSL);

            String glsl = module.desc.source;
            if (this.options.splitTextureSamplers) {
                // We need to expand all combined samplers into texture + sampler due for certain targets (webgpu + dx12)
                glsl = ShaderUtil.ES2ToES3Converter.transformTextureUniforms(module.desc.source).output;
            }
            FileUtils.writeByteArrayToFile(fileInGLSL, glsl.getBytes());

            File fileOutSpv = File.createTempFile(baseName, ".spv");
            FileUtil.deleteOnExit(fileOutSpv);
            generateSPIRv(module.desc.resourcePath, module.desc.type, fileInGLSL.getAbsolutePath(), fileOutSpv.getAbsolutePath());

            // Generate an optimized version of the final .spv file
            File fileOutSpvOpt = File.createTempFile(this.pipelineName, ".optimized.spv");
            FileUtil.deleteOnExit(fileOutSpvOpt);
            generateSPIRvOptimized(module.desc.resourcePath, fileOutSpv.getAbsolutePath(), fileOutSpvOpt.getAbsolutePath());

            module.spirvFile = fileOutSpvOpt;
            module.spirvContext = ShadercJni.NewShaderContext(ToShadercShaderStageValue(module.desc.type), FileUtils.readFileToByteArray(fileOutSpvOpt));
            module.spirvReflector = new SPIRVReflector(module.spirvContext, module.desc.type);
            module.shaderInfo = ShaderUtil.Common.getShaderInfo(module.desc.source);

            if (module.desc.type == ShaderDesc.ShaderType.SHADER_TYPE_VERTEX) {
                vertexModule = module;
            } else if (module.desc.type == ShaderDesc.ShaderType.SHADER_TYPE_FRAGMENT) {
                fragmentModule = module;
            }
        }

        // Potentially post-fix the modules so they are compatible in runtime
        if (vertexModule != null && fragmentModule != null) {
            ArrayList<Long> mergedResources = new ArrayList<>();
            long compilerFs = remapOutputsAndInputs(vertexModule, fragmentModule);
            compilerFs = mergeResources(vertexModule, fragmentModule, compilerFs, mergedResources);

            // If we remapped the input/outputs or the resources, we need to re-generate the spir-v
            if (compilerFs != 0) {
                Shaderc.ShaderCompilerOptions opts = new Shaderc.ShaderCompilerOptions();
                opts.entryPoint = "main";

                Shaderc.ShaderCompileResult remappedSpv = ShadercJni.Compile(fragmentModule.spirvContext, compilerFs, opts);
                long remappedSpvContext = ShadercJni.NewShaderContext(ToShadercShaderStageValue(fragmentModule.desc.type), remappedSpv.data);

                ShadercJni.DeleteShaderCompiler(compilerFs);
                ShadercJni.DeleteShaderContext(fragmentModule.spirvContext);

                String baseName = this.pipelineName + "." + ShaderTypeToSpirvStage(fragmentModule.desc.type);
                File remappedSpvFile = File.createTempFile(baseName, ".remapped.spv");
                FileUtil.deleteOnExit(remappedSpvFile);
                FileUtils.writeByteArrayToFile(remappedSpvFile, remappedSpv.data);

                fragmentModule.spirvContext = remappedSpvContext;
                fragmentModule.spirvReflector = new SPIRVReflector(remappedSpvContext, fragmentModule.desc.type);
                fragmentModule.spirvFile = remappedSpvFile;

                // Update the reflection for the vertex module
                vertexModule.spirvReflector = new SPIRVReflector(vertexModule.spirvContext, vertexModule.desc.type);

                for (Long mergedResource : mergedResources) {
                    fragmentModule.spirvReflector.removeResourceByNameHash(mergedResource);
                }

                // TODO!
                // We can improve the "merge process" here by also merging the reflection data.
                // If two resources are merged, we don't need to keep the type data in both SPIRVReflectors.
                // But as this is a bit more complicated (we need to adjust resources indices and whatnot), this
                // can wait a bit until we are certain the merging works.
            }
        }
    }

    private long mergeResources(ShaderModule vertexModule, ShaderModule fragmentModule, long compiler, ArrayList<Long> mergedResources) throws CompileExceptionError {
        // Check the resources to see if we can merge resources from one stage to another
        int mergedStageFlags = Shaderc.ShaderStage.SHADER_STAGE_VERTEX.getValue() + Shaderc.ShaderStage.SHADER_STAGE_FRAGMENT.getValue();

        for (Shaderc.ShaderResource vsUbo : vertexModule.spirvReflector.getUBOs()) {
            for (Shaderc.ShaderResource fsUbo : fragmentModule.spirvReflector.getUBOs()) {

                if (SPIRVReflector.CanMergeResources(vertexModule.spirvReflector, fragmentModule.spirvReflector, vsUbo, fsUbo)) {
                    if (compiler == 0) {
                        compiler = ShadercJni.NewShaderCompiler(fragmentModule.spirvContext, Shaderc.ShaderLanguage.SHADER_LANGUAGE_SPIRV.getValue());
                    }
                    ShadercJni.SetResourceBinding(fragmentModule.spirvContext, compiler, vsUbo.nameHash, vsUbo.binding);
                    ShadercJni.SetResourceSet(fragmentModule.spirvContext, compiler, vsUbo.nameHash, vsUbo.set);
                    ShadercJni.SetResourceStageFlags(vertexModule.spirvContext, vsUbo.nameHash, mergedStageFlags);

                    mergedResources.add(vsUbo.nameHash);
                }
            }
        }

        for (Shaderc.ShaderResource vsTexture : vertexModule.spirvReflector.getTextures()) {
            for (Shaderc.ShaderResource fsTexture : fragmentModule.spirvReflector.getTextures()) {

                if (SPIRVReflector.CanMergeResources(vertexModule.spirvReflector, fragmentModule.spirvReflector, vsTexture, fsTexture)) {
                    if (compiler == 0) {
                        compiler = ShadercJni.NewShaderCompiler(fragmentModule.spirvContext, Shaderc.ShaderLanguage.SHADER_LANGUAGE_SPIRV.getValue());
                    }
                    ShadercJni.SetResourceBinding(fragmentModule.spirvContext, compiler, vsTexture.nameHash, vsTexture.binding);
                    ShadercJni.SetResourceSet(fragmentModule.spirvContext, compiler, vsTexture.nameHash, vsTexture.set);
                    ShadercJni.SetResourceStageFlags(vertexModule.spirvContext, vsTexture.nameHash, mergedStageFlags);

                    mergedResources.add(vsTexture.nameHash);
                }
            }
        }

        for (Shaderc.ShaderResource vsSsbo : vertexModule.spirvReflector.getSsbos()) {
            for (Shaderc.ShaderResource fsSsbo : fragmentModule.spirvReflector.getSsbos()) {

                if (SPIRVReflector.CanMergeResources(vertexModule.spirvReflector, fragmentModule.spirvReflector, vsSsbo, fsSsbo)) {
                    if (compiler == 0) {
                        compiler = ShadercJni.NewShaderCompiler(fragmentModule.spirvContext, Shaderc.ShaderLanguage.SHADER_LANGUAGE_SPIRV.getValue());
                    }
                    ShadercJni.SetResourceBinding(fragmentModule.spirvContext, compiler, vsSsbo.nameHash, vsSsbo.binding);
                    ShadercJni.SetResourceSet(fragmentModule.spirvContext, compiler, vsSsbo.nameHash, vsSsbo.set);
                    ShadercJni.SetResourceStageFlags(vertexModule.spirvContext, vsSsbo.nameHash, mergedStageFlags);

                    mergedResources.add(vsSsbo.nameHash);
                }
            }
        }

        return compiler;
    }

    private long remapOutputsAndInputs(ShaderModule vertexModule, ShaderModule fragmentModule) {
        long compiler = 0;
        // Check the inputs / output to see if we need to remap locations from vs module outputs -> inputs
        for (Shaderc.ShaderResource output : vertexModule.spirvReflector.getOutputs()) {
            for (Shaderc.ShaderResource input : fragmentModule.spirvReflector.getInputs()) {
                if (output.name.equals(input.name) && output.location != input.location) {
                    // Location mismatch!
                    if (compiler == 0) {
                        compiler = ShadercJni.NewShaderCompiler(fragmentModule.spirvContext, Shaderc.ShaderLanguage.SHADER_LANGUAGE_SPIRV.getValue());
                    }
                    ShadercJni.SetResourceLocation(fragmentModule.spirvContext, compiler, input.nameHash, output.location);
                }
            }
        }
        return compiler;
    }

    // This is only needed for compute shaders
    private static void injectHLSLNumWorkGroupsIdIntoReflection(ShaderModule module, int workGroupId) {
        Shaderc.ResourceType type = new Shaderc.ResourceType();
        type.useTypeIndex = true;

        // This is what the generated cbuffer looks like:
        // cbuffer SPIRV_Cross_NumWorkgroups : register(b3)
        // {
        //     uint3 SPIRV_Cross_NumWorkgroups_1_count : packoffset(c0);
        // };

        Shaderc.ShaderResource hLSLNumWorkGroupsId = new Shaderc.ShaderResource();
        hLSLNumWorkGroupsId.name             = "SPIRV_Cross_NumWorkgroups";
        hLSLNumWorkGroupsId.nameHash         = MurmurHash.hash64(hLSLNumWorkGroupsId.name);
        hLSLNumWorkGroupsId.instanceName     = null;
        hLSLNumWorkGroupsId.blockSize        = 12; // uint3 == 3 x uint32
        hLSLNumWorkGroupsId.binding          = (byte) workGroupId;
        hLSLNumWorkGroupsId.set              = 0;
        hLSLNumWorkGroupsId.stageFlags       = (byte) Shaderc.ShaderStage.SHADER_STAGE_COMPUTE.getValue();
        hLSLNumWorkGroupsId.type             = type;

        module.spirvReflector.addUBO(hLSLNumWorkGroupsId);
    }

    //////////////////////////
    // PUBLIC API
    //////////////////////////
    public Shaderc.ShaderCompileResult crossCompile(ShaderDesc.ShaderType shaderType, ShaderDesc.Language shaderLanguage) throws IOException, CompileExceptionError {
        int version = ShaderLanguageToVersion(shaderLanguage);

        ShaderModule module = getShaderModule(shaderType);
        assert module != null;

        if (shaderLanguage == ShaderDesc.Language.LANGUAGE_SPIRV) {
            // We have already produced SPIR-v for the input module, no need to crosscompile

            Shaderc.ShaderCompileResult result = new Shaderc.ShaderCompileResult();
            result.data = FileUtils.readFileToByteArray(module.spirvFile);
            return result;
        } else if (shaderLanguage == ShaderDesc.Language.LANGUAGE_WGSL) {
            // TODO: Move this into the crosscompile function, since we are actually crosscompiling.
            String shaderTypeStr = ShaderTypeToSpirvStage(shaderType);
            String versionStr    = "v" + version;

            File fileCrossCompiled = File.createTempFile(this.pipelineName, "." + versionStr + "." + shaderTypeStr);
            FileUtil.deleteOnExit(fileCrossCompiled);

            generateWGSL(module.desc.resourcePath, module.spirvFile.getAbsolutePath(), fileCrossCompiled.getAbsolutePath());

            Shaderc.ShaderCompileResult result = new Shaderc.ShaderCompileResult();
            result.data = FileUtils.readFileToByteArray(fileCrossCompiled);
            return result;
        } else if (CanBeCrossCompiled(shaderLanguage)) {
            Shaderc.ShaderCompileResult result = generateCrossCompiledShader(shaderType, shaderLanguage, version);

            if (!result.lastError.isEmpty()) {
                String excludeKey = shaderLanguage == ShaderDesc.Language.LANGUAGE_GLES_SM100 ? "exclude_gles_sm100" : null;
                String exclusionMessage = excludeKey != null ? "\nEnable the 'shader." + excludeKey + "' option in game.project if you don't intend to use this language." : "";
                throw new CompileExceptionError("Cross-compilation of shader type: " + shaderType + ", to language: " + shaderLanguage + " failed, reason: " + result.lastError + exclusionMessage);
            }

            // JG: spirv-cross renames samplers for GLSL based shaders, so we have to run a second pass to force renaming them back.
            //     There doesn't seem to be a simpler way to do this in spirv-cross from what I can understand.
            if (ShaderLanguageIsGLSL(shaderLanguage)) {
                result.data = RemapTextureSamplers(module.spirvReflector.getTextures(), new String(result.data));
            }

            // If we are cross-compiling a compute shader to HLSL, we need to inject an extra resource into the reflection,
            // because there is no gl_NumWorkGroups equivalent in HLSL
            if (shaderType == ShaderDesc.ShaderType.SHADER_TYPE_COMPUTE && result.hLSLNumWorkGroupsId >= 0) {
                injectHLSLNumWorkGroupsIdIntoReflection(module, result.hLSLNumWorkGroupsId);
            }

            return result;
        }

        throw new CompileExceptionError("Cannot crosscompile to shader language: " + shaderLanguage);
    }

    public Shaderc.HLSLRootSignature createRootSignature(ShaderDesc.Language shaderLanguage, List<Shaderc.ShaderCompileResult> shaders) {
        assert(shaderLanguage == ShaderDesc.Language.LANGUAGE_HLSL_51);
        Shaderc.ShaderCompileResult[] shaders_array = shaders.toArray(new Shaderc.ShaderCompileResult[0]);
        Shaderc.HLSLRootSignature result = ShadercJni.HLSLMergeRootSignatures(shaders_array);
        return result;
    }

    public SPIRVReflector getReflectionData(ShaderDesc.ShaderType shaderStage) {
        ShaderModule module = getShaderModule(shaderStage);
        assert module != null;
        return module.spirvReflector;
    }

    public static ShaderCompilePipeline createShaderPipeline(ShaderCompilePipeline pipeline, ShaderModuleDesc desc, Options options) throws IOException, CompileExceptionError {
        ArrayList<ShaderModuleDesc> descs = new ArrayList<>();
        descs.add(desc);
        return createShaderPipeline(pipeline, descs, options );
    }

    public static ShaderCompilePipeline createShaderPipeline(ShaderCompilePipeline pipeline, ArrayList<ShaderModuleDesc> descs, Options options) throws IOException, CompileExceptionError {
        pipeline.options = options;
        pipeline.reset();
        for (ShaderModuleDesc desc : descs) {
            pipeline.addShaderModule(desc);
        }
        pipeline.prepare();
        return pipeline;
    }

    public static void destroyShaderPipeline(ShaderCompilePipeline pipeline) {
        for (ShaderModule module : pipeline.shaderModules) {
            if (module.spirvContext != 0) {
                ShadercJni.DeleteShaderContext(module.spirvContext);
                module.spirvContext = 0;
            }
        }
        pipeline.reset();
    }
}
