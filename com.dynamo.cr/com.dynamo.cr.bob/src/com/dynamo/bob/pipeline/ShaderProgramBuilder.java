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
import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.FileInputStream;
import java.io.FileOutputStream;

import java.nio.charset.StandardCharsets;

import java.util.ArrayList;

import org.apache.commons.cli.CommandLine;
import org.apache.commons.cli.CommandLineParser;
import org.apache.commons.cli.Options;
import org.apache.commons.cli.ParseException;
import org.apache.commons.cli.PosixParser;

import com.google.protobuf.ByteString;

import com.dynamo.bob.Builder;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Task;
import com.dynamo.bob.Platform;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.ShaderUtil.Common;
import com.dynamo.bob.util.MurmurHash;

import com.dynamo.bob.pipeline.shader.ShaderCompilePipeline;
import com.dynamo.bob.pipeline.shader.ShaderCompilePipelineLegacy;
import com.dynamo.bob.pipeline.shader.SPIRVReflector;

import com.dynamo.graphics.proto.Graphics.ShaderDesc;

public abstract class ShaderProgramBuilder extends Builder<ShaderPreprocessor> {

    static public class ShaderBuildResult {
        public ShaderDesc.Shader.Builder shaderBuilder;
        public String[]                  buildWarnings;

        public ShaderBuildResult(ShaderDesc.Shader.Builder fromBuilder) {
            this.shaderBuilder = fromBuilder;
        }
    }

    static public class ShaderDescBuildResult {
        public ShaderDesc shaderDesc;
        public String[]   buildWarnings;
    }

    @Override
    public Task<ShaderPreprocessor> create(IResource input) throws IOException, CompileExceptionError {

        Task.TaskBuilder<ShaderPreprocessor> taskBuilder = Task.<ShaderPreprocessor>newBuilder(this)
            .setName(params.name())
            .addInput(input);

        // Parse source for includes and add the include nodes as inputs/dependancies to the shader
        String source = new String(input.getContent(), StandardCharsets.UTF_8);
        ShaderPreprocessor shaderPreprocessor = new ShaderPreprocessor(this.project, input.getPath(), source);
        String[] includes = shaderPreprocessor.getIncludes();

        for (String path : includes) {
            taskBuilder.addInput(this.project.getResource(path));
        }

        // Include the spir-v flag into the cache key so we can invalidate the output results accordingly
        String shaderCacheKey = "output_spirv=" + getOutputSpirvFlag();

        taskBuilder.addOutput(input.changeExt(params.outExt()));
        taskBuilder.setData(shaderPreprocessor);
        taskBuilder.addExtraCacheKey(shaderCacheKey);

        return taskBuilder.build();
    }

    private boolean getOutputSpirvFlag() {
        boolean fromProjectOptions    = this.project.option("output-spirv", "false").equals("true");
        boolean fromProjectProperties = this.project.getProjectProperties().getBooleanValue("shader", "output_spirv", false);
        return fromProjectOptions || fromProjectProperties;
    }

    static public ShaderDescBuildResult buildResultsToShaderDescBuildResults(ArrayList<ShaderBuildResult> shaderBuildResults, ShaderDesc.ShaderType shaderType) {

        ShaderDescBuildResult shaderDescBuildResult = new ShaderDescBuildResult();
        ShaderDesc.Builder shaderDescBuilder = ShaderDesc.newBuilder();

        for (ShaderBuildResult shaderBuildResult : shaderBuildResults) {
            if (shaderBuildResult != null) {
                if (shaderBuildResult.buildWarnings != null) {
                    shaderDescBuildResult.buildWarnings = shaderBuildResult.buildWarnings;
                    return shaderDescBuildResult;
                }

                shaderDescBuilder.addShaders(shaderBuildResult.shaderBuilder);
            }
        }

        shaderDescBuilder.setShaderType(shaderType);
        shaderDescBuildResult.shaderDesc = shaderDescBuilder.build();

        return shaderDescBuildResult;
    }

    public ShaderDescBuildResult makeShaderDesc(String resourceOutputPath, ShaderPreprocessor shaderPreprocessor, ShaderDesc.ShaderType shaderType, String platform, boolean outputSpirv) throws IOException, CompileExceptionError {
        Platform platformKey = Platform.get(platform);
        if(platformKey == null) {
            throw new CompileExceptionError("Unknown platform for shader program '" + resourceOutputPath + "'': " + platform);
        }

        String finalShaderSource                          = shaderPreprocessor.getCompiledSource();
        IShaderCompiler shaderCompiler                    = project.getShaderCompiler(platformKey);
        ArrayList<ShaderBuildResult> shaderCompilerResult = shaderCompiler.compile(finalShaderSource, shaderType, resourceOutputPath, outputSpirv);
        return buildResultsToShaderDescBuildResults(shaderCompilerResult, shaderType);
    }

    static private void handleShaderDescBuildResult(ShaderDescBuildResult result, String resourceOutputPath) throws CompileExceptionError {
        if (result.buildWarnings != null) {
            for(String warningStr : result.buildWarnings) {
                System.err.println(warningStr);
            }
            throw new CompileExceptionError("Errors when producing output " + resourceOutputPath);
        }
    }

    public ShaderDesc getCompiledShaderDesc(Task<ShaderPreprocessor> task, ShaderDesc.ShaderType shaderType) throws IOException, CompileExceptionError {
        ShaderPreprocessor shaderPreprocessor = task.getData();
        boolean outputSpirv                   = getOutputSpirvFlag();
        String resourceOutputPath             = task.getOutputs().get(0).getPath();

        ShaderDescBuildResult shaderDescBuildResult = makeShaderDesc(resourceOutputPath, shaderPreprocessor,
            shaderType, this.project.getPlatformStrings()[0], outputSpirv);

        handleShaderDescBuildResult(shaderDescBuildResult, resourceOutputPath);

        return shaderDescBuildResult.shaderDesc;
    }

    private CommandLine getShaderCommandLineOptions(String[] args) {
        Options options = new Options();
        options.addOption("p", "platform", true, "Platform");
        options.addOption(null, "variant", true, "Specify debug or release");
        CommandLineParser parser = new PosixParser();
        CommandLine cmd = null;
        try {
            cmd = parser.parse(options, args);
        } catch (ParseException e) {
            System.err.println(e.getMessage());
            System.exit(5);
        }
        return cmd;
    }

    public Platform getPlatformFromCommandLine(String[] args) throws CompileExceptionError {
        CommandLine cmd = getShaderCommandLineOptions(args);
        String platformName = cmd.getOptionValue("platform", "");
        Platform platform = Platform.get(platformName);
        if (platform == null) {
            throw new CompileExceptionError(String.format("Invalid platform '%s'\n", platformName));
        }
        return platform;
    }

    static public ShaderCompilePipeline getShaderPipelineFromShaderSource(ShaderDesc.ShaderType type, String resourcePath, String shaderSource) throws IOException, CompileExceptionError {
        ShaderCompilePipeline pipeline;
        Common.GLSLShaderInfo shaderInfo = Common.getShaderInfo(shaderSource);

        if (shaderInfo == null) {
            pipeline = new ShaderCompilePipelineLegacy(resourcePath);
        } else {
            pipeline = new ShaderCompilePipeline(resourcePath);
        }

        return ShaderCompilePipeline.createShaderPipeline(pipeline, shaderSource, type);
    }

    static private int getTypeIndex(ArrayList<SPIRVReflector.ResourceType> types, String typeName) {
        for (int i=0; i < types.size(); i++) {
            if (types.get(i).key.equals(typeName)) {
                return i;
            }
        }
        return -1;
    }

    static public ShaderProgramBuilder.ShaderBuildResult makeShaderBuilderFromGLSLSource(String source, ShaderDesc.Language shaderLanguage) throws IOException {
        ShaderDesc.Shader.Builder builder = ShaderDesc.Shader.newBuilder();
        builder.setLanguage(shaderLanguage);
        builder.setSource(ByteString.copyFrom(source, "UTF-8"));
        return new ShaderProgramBuilder.ShaderBuildResult(builder);
    }

    static public ShaderDesc.ResourceType.Builder getResourceTypeBuilder(ArrayList<SPIRVReflector.ResourceType> types, String typeName) {
        ShaderDesc.ResourceType.Builder resourceTypeBuilder = ShaderDesc.ResourceType.newBuilder();
        ShaderDesc.ShaderDataType knownType = Common.stringTypeToShaderType(typeName);

        if (knownType == ShaderDesc.ShaderDataType.SHADER_TYPE_UNKNOWN) {
            resourceTypeBuilder.setTypeIndex(getTypeIndex(types, typeName));
            resourceTypeBuilder.setUseTypeIndex(true);
        } else {
            resourceTypeBuilder.setShaderType(knownType);
            resourceTypeBuilder.setUseTypeIndex(false);
        }

        return resourceTypeBuilder;
    }

    static public ShaderDesc.ResourceBinding.Builder SPIRVResourceToResourceBindingBuilder(ArrayList<SPIRVReflector.ResourceType> types, SPIRVReflector.Resource res) {
        ShaderDesc.ResourceBinding.Builder resourceBindingBuilder = ShaderDesc.ResourceBinding.newBuilder();
        ShaderDesc.ResourceType.Builder typeBuilder = getResourceTypeBuilder(types, res.type);
        resourceBindingBuilder.setType(typeBuilder);
        resourceBindingBuilder.setName(res.name);
        resourceBindingBuilder.setNameHash(MurmurHash.hash64(res.name));
        resourceBindingBuilder.setSet(res.set);
        resourceBindingBuilder.setBinding(res.binding);
        resourceBindingBuilder.setBlockSize(res.blockSize);
        return resourceBindingBuilder;
    }

    static public ShaderDesc.Shader.Builder makeShaderBuilder(ShaderDesc.Language language, byte[] source, SPIRVReflector reflector) {
        ShaderDesc.Shader.Builder builder = ShaderDesc.Shader.newBuilder();
        builder.setLanguage(language);
        builder.setSource(ByteString.copyFrom(source));

        if (reflector != null) {
            ArrayList<SPIRVReflector.Resource> inputs    = reflector.getInputs();
            ArrayList<SPIRVReflector.Resource> outputs   = reflector.getOutputs();
            ArrayList<SPIRVReflector.Resource> ubos      = reflector.getUBOs();
            ArrayList<SPIRVReflector.Resource> ssbos     = reflector.getSsbos();
            ArrayList<SPIRVReflector.Resource> textures  = reflector.getTextures();
            ArrayList<SPIRVReflector.ResourceType> types = reflector.getTypes();

            for (SPIRVReflector.Resource input : inputs) {
                ShaderDesc.ResourceBinding.Builder resourceBindingBuilder = SPIRVResourceToResourceBindingBuilder(types, input);
                builder.addInputs(resourceBindingBuilder);
            }

            for (SPIRVReflector.Resource output : outputs) {
                ShaderDesc.ResourceBinding.Builder resourceBindingBuilder = SPIRVResourceToResourceBindingBuilder(types, output);
                builder.addOutputs(resourceBindingBuilder);
            }

            for (SPIRVReflector.Resource ubo : ubos) {
                ShaderDesc.ResourceBinding.Builder resourceBindingBuilder = SPIRVResourceToResourceBindingBuilder(types, ubo);
                builder.addUniformBuffers(resourceBindingBuilder);
            }

            for (SPIRVReflector.Resource ssbo : ssbos) {
                ShaderDesc.ResourceBinding.Builder resourceBindingBuilder = SPIRVResourceToResourceBindingBuilder(types, ssbo);
                builder.addStorageBuffers(resourceBindingBuilder);
            }

            for (SPIRVReflector.Resource texture : textures) {
                ShaderDesc.ResourceBinding.Builder resourceBindingBuilder = SPIRVResourceToResourceBindingBuilder(types, texture);
                builder.addTextures(resourceBindingBuilder);
            }

            for (SPIRVReflector.ResourceType type : types) {
                ShaderDesc.ResourceTypeInfo.Builder resourceTypeInfoBuilder = ShaderDesc.ResourceTypeInfo.newBuilder();

                resourceTypeInfoBuilder.setName(type.name);
                resourceTypeInfoBuilder.setNameHash(MurmurHash.hash64(type.name));

                for (SPIRVReflector.ResourceMember member : type.members) {
                    ShaderDesc.ResourceMember.Builder typeMemberBuilder = ShaderDesc.ResourceMember.newBuilder();

                    ShaderDesc.ResourceType.Builder typeBuilder = getResourceTypeBuilder(types, member.type);
                    typeMemberBuilder.setType(typeBuilder);
                    typeMemberBuilder.setName(member.name);
                    typeMemberBuilder.setNameHash(MurmurHash.hash64(member.name));
                    typeMemberBuilder.setElementCount(member.elementCount);
                    typeMemberBuilder.setOffset(member.offset);

                    resourceTypeInfoBuilder.addMembers(typeMemberBuilder);
                }

                builder.addTypes(resourceTypeInfoBuilder);
            }
        }

        return builder;
    }

    public void BuildShader(String[] args, ShaderDesc.ShaderType shaderType, IShaderCompiler shaderCompiler) throws IOException, CompileExceptionError {
        if (shaderCompiler == null) {
            System.err.printf("Unable to build shader %s - no shader compiler found.%n", args[0]);
            return;
        }

        try (BufferedInputStream is = new BufferedInputStream(new FileInputStream(args[0]));
            BufferedOutputStream os = new BufferedOutputStream(new FileOutputStream(args[1]))) {

            byte[] inBytes = new byte[is.available()];
            is.read(inBytes);

            String source                         = new String(inBytes, StandardCharsets.UTF_8);
            ShaderPreprocessor shaderPreprocessor = new ShaderPreprocessor(this.project, args[0], source);
            String finalShaderSource              = shaderPreprocessor.getCompiledSource();

            ArrayList<ShaderBuildResult> shaderCompilerResult = shaderCompiler.compile(finalShaderSource, shaderType, args[1], true);
            ShaderDescBuildResult shaderDescResult = buildResultsToShaderDescBuildResults(shaderCompilerResult, shaderType);

            shaderDescResult.shaderDesc.writeTo(os);
        }
    }
}
