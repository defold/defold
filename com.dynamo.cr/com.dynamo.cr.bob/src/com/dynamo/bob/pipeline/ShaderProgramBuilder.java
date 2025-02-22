// Copyright 2020-2025 The Defold Foundation
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

import com.dynamo.bob.*;
import com.dynamo.bob.fs.DefaultFileSystem;
import com.google.protobuf.ByteString;

import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.ShaderUtil.Common;
import com.dynamo.bob.util.MurmurHash;

import com.dynamo.bob.pipeline.shader.ShaderCompilePipeline;
import com.dynamo.bob.pipeline.shader.ShaderCompilePipelineLegacy;
import com.dynamo.bob.pipeline.shader.SPIRVReflector;

import com.dynamo.graphics.proto.Graphics.ShaderDesc;

@BuilderParams(name="ShaderProgramBuilder", inExts= {".shbundle", ".shbundlec"}, outExt=".spc")
public class ShaderProgramBuilder extends Builder {

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
        public ArrayList<SPIRVReflector>    reflectors = new ArrayList<>();
    }

    ArrayList<ShaderCompilePipeline.ShaderModuleDesc> modulesDescs = new ArrayList<>();
    ArrayList<ShaderPreprocessor> modulePreprocessors = new ArrayList<>();

    IShaderCompiler.CompileOptions compileOptions = new IShaderCompiler.CompileOptions();

    @Override
    public Task create(IResource input) throws IOException, CompileExceptionError {
        Task.TaskBuilder taskBuilder = Task.newBuilder(this)
                .setName(params.name())
                .addInput(input);

        ShaderProgramBuilderBundle.ModuleBundle modules = ShaderProgramBuilderBundle.ModuleBundle.load(input);
        for (String path : modules.getModules()) {
            IResource moduleInput = this.project.getResource(path);

            // Parse source for includes and add the include-nodes as inputs/dependencies to the shader
            String source = new String(moduleInput.getContent(), StandardCharsets.UTF_8);

            // SPIR-v tools cannot handle carriage return
            source = source.replace("\r", "");

            ShaderPreprocessor shaderPreprocessor = new ShaderPreprocessor(this.project, input.getPath(), source);
            String[] includes = shaderPreprocessor.getIncludes();

            for (String includePath : includes) {
                taskBuilder.addInput(this.project.getResource(includePath));
            }

            ShaderCompilePipeline.ShaderModuleDesc moduleDesc = new ShaderCompilePipeline.ShaderModuleDesc();
            moduleDesc.type = parseShaderTypeFromPath(moduleInput.getPath());
            moduleDesc.resourcePath = path;

            modulesDescs.add(moduleDesc);
            modulePreprocessors.add(shaderPreprocessor);
        }

        compileOptions = modules.getCompileOptions();

        String platformString = this.project.getPlatformStrings()[0];

        // Include the spir-v flag into the cache key so we can invalidate the output results accordingly
        // NOTE: We include the platform string as well for the same reason as spirv, but it doesn't seem to work correctly.
        //       Keeping the build folder and rebuilding for a different platform _should_ invalidate the cache, but it doesn't.
        //       Needs further investigation!
        String shaderCacheKey = String.format("output_spirv=%s;output_hlsl=%s;output_wgsl=%s;platform_key=%s",
                getOutputSpirvFlag(), getOutputHlslFlag(), getOutputWGSLFlag(),  platformString);

        taskBuilder.addOutput(input.changeExt(params.outExt()));
        taskBuilder.addExtraCacheKey(shaderCacheKey);

        return taskBuilder.build();
    }

    @Override
    public void build(Task task) throws IOException, CompileExceptionError {
        String resourceOutputPath = task.getOutputs().get(0).getPath();

        String platform = this.project.getPlatformStrings()[0];
        Platform platformKey = Platform.get(platform);
        if(platformKey == null) {
            throw new CompileExceptionError("Unknown platform for shader program '" + resourceOutputPath + "'': " + platform);
        }

        for (int i = 0; i < this.modulesDescs.size(); i++) {
            this.modulesDescs.get(i).source = this.modulePreprocessors.get(i).getCompiledSource();
        }

        if (getOutputHlslFlag()) {
            compileOptions.forceIncludeShaderLanguages.add(ShaderDesc.Language.LANGUAGE_HLSL);
        }
        if (getOutputSpirvFlag()) {
            compileOptions.forceIncludeShaderLanguages.add(ShaderDesc.Language.LANGUAGE_SPIRV);
        }
        if (getOutputWGSLFlag()) {
            compileOptions.forceIncludeShaderLanguages.add(ShaderDesc.Language.LANGUAGE_WGSL);
        }
        if (getOutputGLSLESFlag(100)) {
            compileOptions.forceIncludeShaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLES_SM100);
        }
        if (getOutputGLSLESFlag(300)) {
            compileOptions.forceIncludeShaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLES_SM300);
        }
        if (getOutputGLSLFlag(120)) {
            compileOptions.forceIncludeShaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLSL_SM120);
        }
        if (getOutputGLSLFlag(330)) {
            compileOptions.forceIncludeShaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLSL_SM330);
        }
        if (getOutputGLSLFlag(430)) {
            compileOptions.forceIncludeShaderLanguages.add(ShaderDesc.Language.LANGUAGE_GLSL_SM430);
        }

        IShaderCompiler shaderCompiler              = project.getShaderCompiler(platformKey);
        ShaderCompileResult shaderCompilerResult    = shaderCompiler.compile(this.modulesDescs, resourceOutputPath, compileOptions);
        ShaderDescBuildResult shaderDescBuildResult = buildResultsToShaderDescBuildResults(shaderCompilerResult);

        handleShaderDescBuildResult(shaderDescBuildResult, resourceOutputPath);

        task.output(0).setContent(shaderDescBuildResult.shaderDesc.toByteArray());
    }

    private boolean getOutputShaderFlag(String projectOption, String projectProperty) {
        boolean fromProjectOptions    = this.project.option(projectOption, "false").equals("true");
        boolean fromProjectProperties = this.project.getProjectProperties().getBooleanValue("shader", projectProperty, false);
        return fromProjectOptions || fromProjectProperties;
    }

    private boolean getOutputSpirvFlag() { return getOutputShaderFlag("output-spirv", "output_spirv"); }
    private boolean getOutputHlslFlag() { return getOutputShaderFlag("output-hlsl", "output_hlsl"); }
    private boolean getOutputWGSLFlag() { return getOutputShaderFlag("output-wgsl", "output_wgsl"); }
    private boolean getOutputGLSLESFlag(int version) { return getOutputShaderFlag("output-glsles" + version, "output_glsl_es" + version); }
    private boolean getOutputGLSLFlag(int version) { return getOutputShaderFlag("output-glsl" + version, "output_glsl" + version); }

    static public ShaderDescBuildResult buildResultsToShaderDescBuildResults(ShaderCompileResult shaderCompileresult) throws CompileExceptionError {
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

        shaderDescBuilder.setReflection(makeShaderReflectionBuilder(shaderCompileresult.reflectors));

        shaderDescBuildResult.shaderDesc = shaderDescBuilder.build();

        return shaderDescBuildResult;
    }

    static private void handleShaderDescBuildResult(ShaderDescBuildResult result, String resourceOutputPath) throws CompileExceptionError {
        if (result.buildWarnings != null) {
            for(String warningStr : result.buildWarnings) {
                System.err.println(warningStr);
            }
            throw new CompileExceptionError("Errors when producing output " + resourceOutputPath);
        }
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

    static public ShaderDesc.ResourceType.Builder getResourceTypeBuilder(Shaderc.ResourceType type, int typeMemberOffset) throws CompileExceptionError {
        ShaderDesc.ResourceType.Builder resourceTypeBuilder = ShaderDesc.ResourceType.newBuilder();
        resourceTypeBuilder.setUseTypeIndex(type.useTypeIndex);

        if (type.useTypeIndex) {
            resourceTypeBuilder.setTypeIndex(type.typeIndex + typeMemberOffset);
        } else {
            ShaderDesc.ShaderDataType shaderType = resourceTypeToShaderDataType(type);
            resourceTypeBuilder.setShaderType(shaderType);
        }
        return resourceTypeBuilder;
    }

    static public ShaderDesc.ResourceBinding.Builder SPIRVResourceToResourceBindingBuilder(Shaderc.ShaderResource res, int stageFlags, int typeMemberOffset) throws CompileExceptionError {
        ShaderDesc.ResourceBinding.Builder resourceBindingBuilder = ShaderDesc.ResourceBinding.newBuilder();
        ShaderDesc.ResourceType.Builder typeBuilder = getResourceTypeBuilder(res.type, typeMemberOffset);
        resourceBindingBuilder.setType(typeBuilder);
        resourceBindingBuilder.setName(res.name);
        resourceBindingBuilder.setNameHash(res.nameHash);
        resourceBindingBuilder.setId(res.id);
        resourceBindingBuilder.setSet(res.set);
        resourceBindingBuilder.setBinding(res.binding);
        resourceBindingBuilder.setStageFlags(stageFlags);

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

    static private ShaderDesc.ShaderReflection.Builder makeShaderReflectionBuilder(ArrayList<SPIRVReflector> reflectors) throws CompileExceptionError {
        ShaderDesc.ShaderReflection.Builder builder = ShaderDesc.ShaderReflection.newBuilder();

        for (SPIRVReflector reflector : reflectors) {
            int stageFlags = 0;
            switch(reflector.getShaderStage()) {
                case SHADER_TYPE_VERTEX -> stageFlags = 1;
                case SHADER_TYPE_FRAGMENT -> stageFlags = 2;
                case SHADER_TYPE_COMPUTE -> stageFlags = 4;
            }
            fillShaderReflectionBuilder(builder, reflector, stageFlags);
        }
        return builder;
    }

    static private void fillShaderReflectionBuilder(ShaderDesc.ShaderReflection.Builder builder, SPIRVReflector reflector, int stageFlags) throws CompileExceptionError {
        ArrayList<Shaderc.ShaderResource> inputs    = reflector.getInputs();
        ArrayList<Shaderc.ShaderResource> outputs   = reflector.getOutputs();
        ArrayList<Shaderc.ShaderResource> ubos      = reflector.getUBOs();
        ArrayList<Shaderc.ShaderResource> ssbos     = reflector.getSsbos();
        ArrayList<Shaderc.ShaderResource> textures  = reflector.getTextures();
        ArrayList<Shaderc.ResourceTypeInfo> types   = reflector.getTypes();

        int typeMemberOffset = builder.getTypesCount();

        HashMap<Integer, Integer> idToTextureIndex = new HashMap<>();
        ResolveSamplerIndices(textures, idToTextureIndex);

        for (Shaderc.ShaderResource input : inputs) {
            ShaderDesc.ResourceBinding.Builder resourceBindingBuilder = SPIRVResourceToResourceBindingBuilder(input, stageFlags, typeMemberOffset);
            resourceBindingBuilder.setBinding(input.location);
            builder.addInputs(resourceBindingBuilder);
        }

        for (Shaderc.ShaderResource output : outputs) {
            ShaderDesc.ResourceBinding.Builder resourceBindingBuilder = SPIRVResourceToResourceBindingBuilder(output, stageFlags, typeMemberOffset);
            resourceBindingBuilder.setBinding(output.location);
            builder.addOutputs(resourceBindingBuilder);
        }

        for (Shaderc.ShaderResource ubo : ubos) {
            ShaderDesc.ResourceBinding.Builder resourceBindingBuilder = SPIRVResourceToResourceBindingBuilder(ubo, stageFlags, typeMemberOffset);
            builder.addUniformBuffers(resourceBindingBuilder);
        }

        for (Shaderc.ShaderResource ssbo : ssbos) {
            ShaderDesc.ResourceBinding.Builder resourceBindingBuilder = SPIRVResourceToResourceBindingBuilder(ssbo, stageFlags, typeMemberOffset);
            builder.addStorageBuffers(resourceBindingBuilder);
        }

        for (Shaderc.ShaderResource texture : textures) {
            ShaderDesc.ResourceBinding.Builder resourceBindingBuilder = SPIRVResourceToResourceBindingBuilder(texture, stageFlags, typeMemberOffset);

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

                ShaderDesc.ResourceType.Builder typeBuilder = getResourceTypeBuilder(member.type, typeMemberOffset);
                typeMemberBuilder.setType(typeBuilder);
                typeMemberBuilder.setName(member.name);
                typeMemberBuilder.setNameHash(MurmurHash.hash64(member.name));
                typeMemberBuilder.setElementCount(member.type.arraySize);
                typeMemberBuilder.setOffset(member.offset);

                resourceTypeInfoBuilder.addMembers(typeMemberBuilder);
            }

            builder.addTypes(resourceTypeInfoBuilder);
        }
    }

    private static ShaderDesc.ShaderType parseShaderTypeFromPath(String path) throws CompileExceptionError {
        if (path.endsWith(".fp")) {
            return ShaderDesc.ShaderType.SHADER_TYPE_FRAGMENT;
        } else if (path.endsWith(".vp")) {
            return ShaderDesc.ShaderType.SHADER_TYPE_VERTEX;
        } else if (path.endsWith(".cp")) {
            return ShaderDesc.ShaderType.SHADER_TYPE_COMPUTE;
        }
        throw new CompileExceptionError("Unknown shader type.%n for path" + path);
    }

    static public ShaderCompilePipeline newShaderPipeline(String resourcePath, ArrayList<ShaderCompilePipeline.ShaderModuleDesc> shaderDescs, ShaderCompilePipeline.Options options) throws IOException, CompileExceptionError {
        ArrayList<ShaderCompilePipeline.ShaderModuleDesc> oldShaders = new ArrayList<>();
        ArrayList<ShaderCompilePipeline.ShaderModuleDesc> newShaders = new ArrayList<>();

        for (ShaderCompilePipeline.ShaderModuleDesc shaderDesc : shaderDescs) {
            Common.GLSLShaderInfo shaderInfo = Common.getShaderInfo(shaderDesc.source);
            if (shaderInfo == null) {
                oldShaders.add(shaderDesc);
            } else {
                newShaders.add(shaderDesc);
            }
        }

        if (newShaders.size() > 0 && oldShaders.size() > 0) {
            System.out.println("Warning: Mixing old shaders with new shaders is slow. Consider migrating old shaders to using the new shader pipeline.");

            ArrayList<ShaderCompilePipeline.ShaderModuleDesc> newDescs = new ArrayList<>(newShaders);
            for (ShaderCompilePipeline.ShaderModuleDesc old : oldShaders) {
                ShaderUtil.ES2ToES3Converter.Result transformResult = ShaderUtil.ES2ToES3Converter.transform(old.source, old.type, "", 140, true, options.splitTextureSamplers);
                ShaderCompilePipeline.ShaderModuleDesc transformedDesc = new ShaderCompilePipeline.ShaderModuleDesc();
                transformedDesc.type = old.type;
                transformedDesc.source = transformResult.output;
                newDescs.add(transformedDesc);
            }
            shaderDescs = newDescs;
        }

        if (newShaders.size() > 0) {
            return ShaderCompilePipeline.createShaderPipeline(new ShaderCompilePipeline(resourcePath), shaderDescs, options);
        } else {
            return ShaderCompilePipeline.createShaderPipeline(new ShaderCompilePipelineLegacy(resourcePath), shaderDescs, options);
        }
    }

    static public ShaderDesc.Shader.Builder makeShaderBuilder(byte[] source, ShaderDesc.Language language, ShaderDesc.ShaderType type) {
        ShaderDesc.Shader.Builder builder = ShaderDesc.Shader.newBuilder();
        builder.setLanguage(language);
        builder.setShaderType(type);
        builder.setSource(ByteString.copyFrom(source));
        return builder;
    }

    private static String GetShaderSource(Project project, String path) throws IOException, CompileExceptionError {
        try (BufferedInputStream is = new BufferedInputStream(new FileInputStream(path))) {
            byte[] inBytes = new byte[is.available()];
            is.read(inBytes);

            String source = new String(inBytes, StandardCharsets.UTF_8);
            source = source.replace("\r", "");

            ShaderPreprocessor shaderPreprocessor = new ShaderPreprocessor(project, path, source);
            return shaderPreprocessor.getCompiledSource();
        }
    }

    private static ShaderCompilePipeline.ShaderModuleDesc GetShaderDesc(Project project, String path) throws IOException, CompileExceptionError {
        ShaderDesc.ShaderType shaderType = parseShaderTypeFromPath(path);
        ShaderCompilePipeline.ShaderModuleDesc desc = new ShaderCompilePipeline.ShaderModuleDesc();
        desc.type = shaderType;
        desc.source = GetShaderSource(project, path);
        return desc;
    }

    // Running standalone:
    // java -classpath $DYNAMO_HOME/share/java/bob-light.jar com.dynamo.bob.pipeline.ShaderProgramBuilder <path-in.fp|vp|cp> <path-out.fpc|vpc|cpc> <platform>
    public static void main(String[] args) throws IOException, CompileExceptionError {
        System.setProperty("java.awt.headless", "true");
        ShaderProgramBuilder builder = new ShaderProgramBuilder();

        Project project = new Project(new DefaultFileSystem());
        project.scanJavaClasses();
        builder.setProject(project);

        if (args.length < 3) {
            System.err.println("Unable to build shader - no platform passed in.%n");
            return;
        }

        ArrayList<ShaderCompilePipeline.ShaderModuleDesc> modules = new ArrayList<>();

        String outputPath, platform;

        if (args.length == 3) {
            modules.add(GetShaderDesc(project, args[0]));
            outputPath   = args[1];
            platform     = args[2];
        } else {
            modules.add(GetShaderDesc(project, args[0]));
            modules.add(GetShaderDesc(project, args[1]));
            outputPath   = args[2];
            platform     = args[3];
        }

        assert platform != null;
        Platform outputPlatform = Platform.get(platform);
        IShaderCompiler shaderCompiler = project.getShaderCompiler(outputPlatform);
        if (shaderCompiler == null) {
            System.err.printf("Unable to build shader - no shader compiler found.%n");
            return;
        }

        try (BufferedOutputStream os = new BufferedOutputStream(new FileOutputStream(outputPath))) {

            IShaderCompiler.CompileOptions compileOptions = new IShaderCompiler.CompileOptions();
            compileOptions.forceIncludeShaderLanguages.add(ShaderDesc.Language.LANGUAGE_HLSL);
            compileOptions.forceIncludeShaderLanguages.add(ShaderDesc.Language.LANGUAGE_SPIRV);
            compileOptions.forceIncludeShaderLanguages.add(ShaderDesc.Language.LANGUAGE_WGSL);

            ShaderCompileResult shaderCompilerResult = shaderCompiler.compile(modules, outputPath, compileOptions);
            ShaderDescBuildResult shaderDescResult = buildResultsToShaderDescBuildResults(shaderCompilerResult);
            shaderDescResult.shaderDesc.writeTo(os);
        }
    }
}
