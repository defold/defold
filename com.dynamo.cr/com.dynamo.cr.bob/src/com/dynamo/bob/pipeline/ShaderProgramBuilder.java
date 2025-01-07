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
import java.util.HashMap;

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

public abstract class ShaderProgramBuilder extends Builder {

    ShaderPreprocessor shaderPreprocessor;

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

    static public class ShaderCompileResult {
        public ArrayList<ShaderBuildResult> shaderBuildResults;
        public SPIRVReflector               reflector;
    }

    @Override
    public Task create(IResource input) throws IOException, CompileExceptionError {

        Task.TaskBuilder<ShaderPreprocessor> taskBuilder = Task.<ShaderPreprocessor>newBuilder(this)
            .setName(params.name())
            .addInput(input);

        // Parse source for includes and add the include nodes as inputs/dependancies to the shader
        String source = new String(input.getContent(), StandardCharsets.UTF_8);

        // SPIR-v tools cannot handle carriage return
        source = source.replace("\r", "");

        shaderPreprocessor = new ShaderPreprocessor(this.project, input.getPath(), source);
        String[] includes = shaderPreprocessor.getIncludes();

        for (String path : includes) {
            taskBuilder.addInput(this.project.getResource(path));
        }

        String platformString = this.project.getPlatformStrings()[0];

        // Include the spir-v flag into the cache key so we can invalidate the output results accordingly
        // NOTE: We include the platform string as well for the same reason as spirv, but it doesn't seem to work correctly.
        //       Keeping the build folder and rebuilding for a different platform _should_ invalidate the cache, but it doesn't.
        //       Needs further investigation!
        String shaderCacheKey = String.format("output_spirv=%s;output_wgsl=%s;platform_key=%s", getOutputSpirvFlag(), getOutputWGSLFlag(), platformString);

        taskBuilder.addOutput(input.changeExt(params.outExt()));
        taskBuilder.addExtraCacheKey(shaderCacheKey);

        return taskBuilder.build();
    }

    private boolean getOutputSpirvFlag() {
        boolean fromProjectOptions    = this.project.option("output-spirv", "false").equals("true");
        boolean fromProjectProperties = this.project.getProjectProperties().getBooleanValue("shader", "output_spirv", false);
        return fromProjectOptions || fromProjectProperties;
    }

    private boolean getOutputWGSLFlag() {
        boolean fromProjectOptions    = this.project.option("output-wgsl", "false").equals("true");
        boolean fromProjectProperties = this.project.getProjectProperties().getBooleanValue("shader", "output_wgsl", false);
        return fromProjectOptions || fromProjectProperties;
    }

    static public ShaderDescBuildResult buildResultsToShaderDescBuildResults(ShaderCompileResult shaderCompileresult, ShaderDesc.ShaderType shaderType) throws CompileExceptionError {

        ShaderDescBuildResult shaderDescBuildResult = new ShaderDescBuildResult();
        ShaderDesc.Builder shaderDescBuilder = ShaderDesc.newBuilder();

        for (ShaderBuildResult shaderBuildResult : shaderCompileresult.shaderBuildResults) {
            if (shaderBuildResult != null) {
                if (shaderBuildResult.buildWarnings != null) {
                    shaderDescBuildResult.buildWarnings = shaderBuildResult.buildWarnings;
                    return shaderDescBuildResult;
                }

                shaderDescBuilder.addShaders(shaderBuildResult.shaderBuilder);
            }
        }

        shaderDescBuilder.setShaderType(shaderType);
        shaderDescBuilder.setReflection(makeShaderReflectionBuilder(shaderCompileresult.reflector).build());

        shaderDescBuildResult.shaderDesc = shaderDescBuilder.build();

        return shaderDescBuildResult;
    }

    public ShaderDescBuildResult makeShaderDesc(String resourceOutputPath, ShaderPreprocessor shaderPreprocessor, ShaderDesc.ShaderType shaderType, String platform, boolean outputSpirv, boolean outputWGSL) throws IOException, CompileExceptionError {
        Platform platformKey = Platform.get(platform);
        if(platformKey == null) {
            throw new CompileExceptionError("Unknown platform for shader program '" + resourceOutputPath + "'': " + platform);
        }

        String finalShaderSource                 = shaderPreprocessor.getCompiledSource();
        IShaderCompiler shaderCompiler           = project.getShaderCompiler(platformKey);
        ShaderCompileResult shaderCompilerResult = shaderCompiler.compile(finalShaderSource, shaderType, resourceOutputPath, outputSpirv, outputWGSL);
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

    public ShaderDesc getCompiledShaderDesc(Task task, ShaderDesc.ShaderType shaderType) throws IOException, CompileExceptionError {
        boolean outputSpirv                   = getOutputSpirvFlag();
        boolean outputWGSL                    = getOutputWGSLFlag();
        String resourceOutputPath             = task.getOutputs().get(0).getPath();

        ShaderDescBuildResult shaderDescBuildResult = makeShaderDesc(resourceOutputPath, shaderPreprocessor, shaderType, this.project.getPlatformStrings()[0], outputSpirv, outputWGSL);

        handleShaderDescBuildResult(shaderDescBuildResult, resourceOutputPath);

        return shaderDescBuildResult.shaderDesc;
    }

    static public ShaderCompilePipeline newShaderPipelineFromShaderSource(ShaderDesc.ShaderType type, String resourcePath, String shaderSource, ShaderCompilePipeline.Options options) throws IOException, CompileExceptionError {
        ShaderCompilePipeline pipeline;
        Common.GLSLShaderInfo shaderInfo = Common.getShaderInfo(shaderSource);

        if (shaderInfo == null) {
            pipeline = new ShaderCompilePipelineLegacy(resourcePath);
        } else {
            pipeline = new ShaderCompilePipeline(resourcePath);
        }

        return ShaderCompilePipeline.createShaderPipeline(pipeline, shaderSource, type, options);
    }

    static public ShaderProgramBuilder.ShaderBuildResult makeShaderBuilderFromGLSLSource(String source, ShaderDesc.Language shaderLanguage) throws IOException {
        ShaderDesc.Shader.Builder builder = ShaderDesc.Shader.newBuilder();
        builder.setLanguage(shaderLanguage);
        builder.setSource(ByteString.copyFrom(source, "UTF-8"));
        return new ShaderProgramBuilder.ShaderBuildResult(builder);
    }

    static private ShaderDesc.ShaderDataType TextureToShaderDataType(Shaderc.ResourceType type) throws CompileExceptionError {
        if (type.baseType == Shaderc.BaseType.BASE_TYPE_SAMPLED_IMAGE) {
            switch (type.dimensionType) {
                case DIMENSION_TYPE_2D -> {
                    if (type.imageIsArrayed) {
                        return ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER2D_ARRAY;
                    } else {
                        return ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER2D;
                    }
                }
                case DIMENSION_TYPE_3D -> {
                    return ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER3D;
                }
                case DIMENSION_TYPE_CUBE -> {
                    return ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER_CUBE;
                }
            }
        } else if (type.baseType == Shaderc.BaseType.BASE_TYPE_SAMPLER) {
            return ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER;
        } else if (type.baseType == Shaderc.BaseType.BASE_TYPE_IMAGE) {
            if (type.dimensionType == Shaderc.DimensionType.DIMENSION_TYPE_2D) {
                if (type.imageIsStorage) {
                    if (type.imageStorageType == Shaderc.ImageStorageType.IMAGE_STORAGE_TYPE_RGBA32F) {
                        return ShaderDesc.ShaderDataType.SHADER_TYPE_IMAGE2D;
                    } else if (type.imageStorageType == Shaderc.ImageStorageType.IMAGE_STORAGE_TYPE_RGBA8UI) {
                        return ShaderDesc.ShaderDataType.SHADER_TYPE_UIMAGE2D;
                    }
                } else if (type.imageIsArrayed) {
                    return ShaderDesc.ShaderDataType.SHADER_TYPE_TEXTURE2D_ARRAY;
                } else if (type.imageBaseType == Shaderc.BaseType.BASE_TYPE_UINT32) {
                    return ShaderDesc.ShaderDataType.SHADER_TYPE_UTEXTURE2D;
                } else if (type.imageBaseType == Shaderc.BaseType.BASE_TYPE_FP32) {
                    return ShaderDesc.ShaderDataType.SHADER_TYPE_TEXTURE2D;
                }
            } else if (type.dimensionType == Shaderc.DimensionType.DIMENSION_TYPE_CUBE) {
                return ShaderDesc.ShaderDataType.SHADER_TYPE_TEXTURE_CUBE;
            }
        }
        throw new CompileExceptionError("Unsupported shader type " + type);
    }

    static private ShaderDesc.ShaderDataType DataTypeToShaderDataType(Shaderc.ResourceType type) throws CompileExceptionError{
        if (type.baseType == Shaderc.BaseType.BASE_TYPE_FP32) {
            if (type.vectorSize == 1) {
                return ShaderDesc.ShaderDataType.SHADER_TYPE_FLOAT;
            } else if (type.vectorSize == 2) {
                if (type.columnCount == 2) {
                    return ShaderDesc.ShaderDataType.SHADER_TYPE_MAT2;
                } else if (type.columnCount == 1) {
                    return ShaderDesc.ShaderDataType.SHADER_TYPE_VEC2;
                }
            } else if (type.vectorSize == 3) {
                if (type.columnCount == 3) {
                    return ShaderDesc.ShaderDataType.SHADER_TYPE_MAT3;
                } else if (type.columnCount == 1) {
                    return ShaderDesc.ShaderDataType.SHADER_TYPE_VEC3;
                }
            } else if (type.vectorSize == 4) {
                if (type.columnCount == 4) {
                    return ShaderDesc.ShaderDataType.SHADER_TYPE_MAT4;
                } else if (type.columnCount == 1) {
                    return ShaderDesc.ShaderDataType.SHADER_TYPE_VEC4;
                }
            }
        }  else if (type.baseType == Shaderc.BaseType.BASE_TYPE_INT32) {
            if (type.columnCount == 1) {
                return ShaderDesc.ShaderDataType.SHADER_TYPE_INT;
            }
        } else if (type.baseType == Shaderc.BaseType.BASE_TYPE_UINT32) {
            if (type.columnCount == 1) {
                switch (type.vectorSize) {
                    case 1: return ShaderDesc.ShaderDataType.SHADER_TYPE_UINT;
                    case 2: return ShaderDesc.ShaderDataType.SHADER_TYPE_UVEC2;
                    case 3: return ShaderDesc.ShaderDataType.SHADER_TYPE_UVEC3;
                    case 4: return ShaderDesc.ShaderDataType.SHADER_TYPE_UVEC4;
                }
            }
        }

        String errorMsg = "";
        errorMsg += "BaseType: " + type.baseType + "\n";
        errorMsg += "Columncount: " + type.columnCount + "\n";
        errorMsg += "DimensionType: " + type.dimensionType + "\n";
        errorMsg += "ArraySize: " +  type.arraySize + "\n";
        throw new CompileExceptionError("Unsupported shader type:\n" + errorMsg);
    }

    public static ShaderDesc.ShaderDataType resourceTypeToShaderDataType(Shaderc.ResourceType type) throws CompileExceptionError {
        if (type.baseType == Shaderc.BaseType.BASE_TYPE_SAMPLED_IMAGE ||
            type.baseType == Shaderc.BaseType.BASE_TYPE_SAMPLER ||
            type.baseType == Shaderc.BaseType.BASE_TYPE_IMAGE) {
            return TextureToShaderDataType(type);
        } else {
            return DataTypeToShaderDataType(type);
        }
    }

    static public ShaderDesc.ResourceType.Builder getResourceTypeBuilder(Shaderc.ResourceType type) throws CompileExceptionError {
        ShaderDesc.ResourceType.Builder resourceTypeBuilder = ShaderDesc.ResourceType.newBuilder();
        resourceTypeBuilder.setUseTypeIndex(type.useTypeIndex);

        if (type.useTypeIndex) {
            resourceTypeBuilder.setTypeIndex(type.typeIndex);
        } else {
            ShaderDesc.ShaderDataType shaderType = resourceTypeToShaderDataType(type);
            resourceTypeBuilder.setShaderType(shaderType);
        }
        return resourceTypeBuilder;
    }

    static public ShaderDesc.ResourceBinding.Builder SPIRVResourceToResourceBindingBuilder(Shaderc.ShaderResource res) throws CompileExceptionError {
        ShaderDesc.ResourceBinding.Builder resourceBindingBuilder = ShaderDesc.ResourceBinding.newBuilder();
        ShaderDesc.ResourceType.Builder typeBuilder = getResourceTypeBuilder(res.type);
        resourceBindingBuilder.setType(typeBuilder);
        resourceBindingBuilder.setName(res.name);
        resourceBindingBuilder.setNameHash(res.nameHash);
        resourceBindingBuilder.setId(res.id);
        resourceBindingBuilder.setSet(res.set);
        resourceBindingBuilder.setBinding(res.binding);

        if (res.blockSize != 0) {
            resourceBindingBuilder.setBlockSize(res.blockSize);
        }

        if (res.instanceName != null) {
            resourceBindingBuilder.setInstanceName(res.instanceName);
            resourceBindingBuilder.setInstanceNameHash(res.instanceNameHash);
        }

        return resourceBindingBuilder;
    }

    static private void ResolveSamplerIndices(ArrayList<Shaderc.ShaderResource> textures, HashMap<Integer, Integer> idToTextureIndexMap) {
        for (int i=0; i < textures.size(); i++) {
            Shaderc.ShaderResource texture = textures.get(i);

            if (texture.type.baseType != Shaderc.BaseType.BASE_TYPE_SAMPLER) {
                String constructedSamplerName = texture.name + "_separated";

                for (Shaderc.ShaderResource other : textures) {
                    if (other.name.equals(constructedSamplerName)) {
                        idToTextureIndexMap.put(other.id, i);
                    }
                }
            }
        }
    }

    static private ShaderDesc.ShaderReflection.Builder makeShaderReflectionBuilder(SPIRVReflector reflector) throws CompileExceptionError {
        ShaderDesc.ShaderReflection.Builder builder = ShaderDesc.ShaderReflection.newBuilder();

        ArrayList<Shaderc.ShaderResource> inputs    = reflector.getInputs();
        ArrayList<Shaderc.ShaderResource> outputs   = reflector.getOutputs();
        ArrayList<Shaderc.ShaderResource> ubos      = reflector.getUBOs();
        ArrayList<Shaderc.ShaderResource> ssbos     = reflector.getSsbos();
        ArrayList<Shaderc.ShaderResource> textures  = reflector.getTextures();
        ArrayList<Shaderc.ResourceTypeInfo> types   = reflector.getTypes();

        HashMap<Integer, Integer> idToTextureIndex = new HashMap<>();
        ResolveSamplerIndices(textures, idToTextureIndex);

        for (Shaderc.ShaderResource input : inputs) {
            ShaderDesc.ResourceBinding.Builder resourceBindingBuilder = SPIRVResourceToResourceBindingBuilder(input);
            resourceBindingBuilder.setBinding(input.location);
            builder.addInputs(resourceBindingBuilder);
        }

        for (Shaderc.ShaderResource output : outputs) {
            ShaderDesc.ResourceBinding.Builder resourceBindingBuilder = SPIRVResourceToResourceBindingBuilder(output);
            resourceBindingBuilder.setBinding(output.location);
            builder.addOutputs(resourceBindingBuilder);
        }

        for (Shaderc.ShaderResource ubo : ubos) {
            ShaderDesc.ResourceBinding.Builder resourceBindingBuilder = SPIRVResourceToResourceBindingBuilder(ubo);
            builder.addUniformBuffers(resourceBindingBuilder);
        }

        for (Shaderc.ShaderResource ssbo : ssbos) {
            ShaderDesc.ResourceBinding.Builder resourceBindingBuilder = SPIRVResourceToResourceBindingBuilder(ssbo);
            builder.addStorageBuffers(resourceBindingBuilder);
        }

        for (Shaderc.ShaderResource texture : textures) {
            ShaderDesc.ResourceBinding.Builder resourceBindingBuilder = SPIRVResourceToResourceBindingBuilder(texture);

            Integer textureIndex = idToTextureIndex.get(texture.id);
            if (textureIndex != null) {
                resourceBindingBuilder.setSamplerTextureIndex(textureIndex);
            }

            builder.addTextures(resourceBindingBuilder);
        }

        for (Shaderc.ResourceTypeInfo type : types) {
            ShaderDesc.ResourceTypeInfo.Builder resourceTypeInfoBuilder = ShaderDesc.ResourceTypeInfo.newBuilder();

            resourceTypeInfoBuilder.setName(type.name);
            resourceTypeInfoBuilder.setNameHash(MurmurHash.hash64(type.name));

            for (Shaderc.ResourceMember member : type.members) {
                ShaderDesc.ResourceMember.Builder typeMemberBuilder = ShaderDesc.ResourceMember.newBuilder();

                ShaderDesc.ResourceType.Builder typeBuilder = getResourceTypeBuilder(member.type);
                typeMemberBuilder.setType(typeBuilder);
                typeMemberBuilder.setName(member.name);
                typeMemberBuilder.setNameHash(MurmurHash.hash64(member.name));
                typeMemberBuilder.setElementCount(member.type.arraySize);
                typeMemberBuilder.setOffset(member.offset);

                resourceTypeInfoBuilder.addMembers(typeMemberBuilder);
            }

            builder.addTypes(resourceTypeInfoBuilder);
        }
        return builder;
    }

    static public ShaderDesc.Shader.Builder makeShaderBuilder(ShaderDesc.Language language, byte[] source) {
        ShaderDesc.Shader.Builder builder = ShaderDesc.Shader.newBuilder();
        builder.setLanguage(language);
        builder.setSource(ByteString.copyFrom(source));
        return builder;
    }

    public void BuildShader(String[] args, ShaderDesc.ShaderType shaderType) throws IOException, CompileExceptionError {

        if (args.length < 3) {
            System.err.println("Unable to build shader %s - no platform passed in.%n");
            return;
        }

        Platform outputPlatform = Platform.get(args[2]);
        IShaderCompiler shaderCompiler = project.getShaderCompiler(outputPlatform);

        if (shaderCompiler == null) {
            System.err.printf("Unable to build shader %s - no shader compiler found.%n", args[0]);
            return;
        }

        try (BufferedInputStream is = new BufferedInputStream(new FileInputStream(args[0]));
            BufferedOutputStream os = new BufferedOutputStream(new FileOutputStream(args[1]))) {

            byte[] inBytes = new byte[is.available()];
            is.read(inBytes);

            String source = new String(inBytes, StandardCharsets.UTF_8);

            // SPIR-v tools cannot handle carriage return
            source = source.replace("\r", "");

            ShaderPreprocessor shaderPreprocessor = new ShaderPreprocessor(this.project, args[0], source);
            String finalShaderSource              = shaderPreprocessor.getCompiledSource();

            ShaderCompileResult shaderCompilerResult = shaderCompiler.compile(finalShaderSource, shaderType, args[1], true, true);
            ShaderDescBuildResult shaderDescResult = buildResultsToShaderDescBuildResults(shaderCompilerResult, shaderType);

            shaderDescResult.shaderDesc.writeTo(os);
        }
    }
}
