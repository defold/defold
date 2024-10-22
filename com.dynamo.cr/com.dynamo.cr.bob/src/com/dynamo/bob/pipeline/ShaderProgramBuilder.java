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

@BuilderParams(name="ShaderProgramBuilder", inExts=".shbundlec", outExt=".spc")
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
        public SPIRVReflector               reflector;
    }

    ArrayList<ShaderCompilePipeline.ShaderModuleDesc> modulesDescs = new ArrayList<>();
    ArrayList<ShaderPreprocessor> modulePreprocessors = new ArrayList<>();

    @Override
    public Task create(IResource input) throws IOException, CompileExceptionError {
        Task.TaskBuilder taskBuilder = Task.newBuilder(this)
                .setName(params.name())
                .addInput(input);

        ShaderProgramBuilderBundle.ModuleBundle modules = ShaderProgramBuilderBundle.ModuleBundle.load(input);
        for (String path : modules.get()) {
            IResource moduleInput = this.project.getResource(path);

            // Parse source for includes and add the include nodes as inputs/dependancies to the shader
            String source = new String(moduleInput.getContent(), StandardCharsets.UTF_8);
            ShaderPreprocessor shaderPreprocessor = new ShaderPreprocessor(this.project, input.getPath(), source);
            String[] includes = shaderPreprocessor.getIncludes();

            for (String includePath : includes) {
                taskBuilder.addInput(this.project.getResource(includePath));
            }

            ShaderCompilePipeline.ShaderModuleDesc moduleDesc = new ShaderCompilePipeline.ShaderModuleDesc();
            moduleDesc.type = parseShaderTypeFromPath(moduleInput.getPath());

            modulesDescs.add(moduleDesc);
            modulePreprocessors.add(shaderPreprocessor);
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

    @Override
    public void build(Task task) throws IOException, CompileExceptionError {
        boolean outputSpirv = getOutputSpirvFlag();
        boolean outputWGSL  = getOutputWGSLFlag();

        String resourceOutputPath = task.getOutputs().get(0).getPath();

        String platform = this.project.getPlatformStrings()[0];
        Platform platformKey = Platform.get(platform);
        if(platformKey == null) {
            throw new CompileExceptionError("Unknown platform for shader program '" + resourceOutputPath + "'': " + platform);
        }

        for (int i = 0; i < this.modulesDescs.size(); i++) {
            this.modulesDescs.get(i).source = this.modulePreprocessors.get(i).getCompiledSource();
        }
        IShaderCompiler shaderCompiler              = project.getShaderCompiler(platformKey);
        ShaderCompileResult shaderCompilerResult    = shaderCompiler.compile(this.modulesDescs, resourceOutputPath, outputSpirv, outputWGSL);
        ShaderDescBuildResult shaderDescBuildResult = buildResultsToShaderDescBuildResults(shaderCompilerResult);

        handleShaderDescBuildResult(shaderDescBuildResult, resourceOutputPath);

        task.output(0).setContent(shaderDescBuildResult.shaderDesc.toByteArray());
    }

    static private void handleShaderDescBuildResult(ShaderDescBuildResult result, String resourceOutputPath) throws CompileExceptionError {
        if (result.buildWarnings != null) {
            for(String warningStr : result.buildWarnings) {
                System.err.println(warningStr);
            }
            throw new CompileExceptionError("Errors when producing output " + resourceOutputPath);
        }
    }

    static public ShaderDescBuildResult buildResultsToShaderDescBuildResults(ShaderCompileResult shaderCompileresult) {

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

        //shaderDescBuilder.setShaderType(shaderType);
        shaderDescBuilder.setReflection(makeShaderReflectionBuilder(shaderCompileresult.reflector).build());

        shaderDescBuildResult.shaderDesc = shaderDescBuilder.build();

        return shaderDescBuildResult;
    }

    static private void ResolveSamplerIndices(ArrayList<SPIRVReflector.Resource> textures) {
        for (int i=0; i < textures.size(); i++) {
            SPIRVReflector.Resource texture = textures.get(i);
            // Look for a matching sampler resource
            if (ShaderUtil.Common.stringTypeToShaderType(texture.type) != ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER) {
                // TODO: the extension should be a constant
                String constructedSamplerName = texture.name + "_separated";
                for (SPIRVReflector.Resource other : textures) {
                    if (other.name.equals(constructedSamplerName)) {
                        other.textureIndex = i;
                    }
                }
            }
        }
    }

    static private int getTypeIndex(ArrayList<SPIRVReflector.ResourceType> types, String typeName) {
        for (int i=0; i < types.size(); i++) {
            if (types.get(i).key.equals(typeName)) {
                return i;
            }
        }
        return -1;
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
        if (res.blockSize != null)
            resourceBindingBuilder.setBlockSize(res.blockSize);
        else if (res.textureIndex != null)
            resourceBindingBuilder.setSamplerTextureIndex(res.textureIndex);
        return resourceBindingBuilder;
    }

    static private ShaderDesc.ShaderReflection.Builder makeShaderReflectionBuilder(SPIRVReflector reflector) {
        ShaderDesc.ShaderReflection.Builder builder = ShaderDesc.ShaderReflection.newBuilder();

        ArrayList<SPIRVReflector.Resource> inputs    = reflector.getInputs();
        ArrayList<SPIRVReflector.Resource> outputs   = reflector.getOutputs();
        ArrayList<SPIRVReflector.Resource> ubos      = reflector.getUBOs();
        ArrayList<SPIRVReflector.Resource> ssbos     = reflector.getSsbos();
        ArrayList<SPIRVReflector.Resource> textures  = reflector.getTextures();
        ArrayList<SPIRVReflector.ResourceType> types = reflector.getTypes();

        ResolveSamplerIndices(textures);

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
        return builder;
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

    static public ShaderCompilePipeline newShaderPipeline(String resourcePath, ArrayList<ShaderCompilePipeline.ShaderModuleDesc> shaderDescs, ShaderCompilePipeline.Options options) throws IOException, CompileExceptionError {
        // Validate that all the shaders can use the same pipeline (TODO: Can we mix and match?)
        int countNewPipeline = 0, countOldPipeline = 0;
        for (ShaderCompilePipeline.ShaderModuleDesc shaderDesc : shaderDescs) {
            Common.GLSLShaderInfo shaderInfo = Common.getShaderInfo(shaderDesc.source);
            if (shaderInfo == null) {
                countOldPipeline++;
            } else
            {
                countNewPipeline++;
            }
        }

        if (countNewPipeline > 0 && countOldPipeline > 0) {
            throw new CompileExceptionError("Unable to mix old shader layout with new shader layout.");
        } else if (countNewPipeline > 0) {
            return ShaderCompilePipeline.createShaderPipeline(new ShaderCompilePipeline(resourcePath), shaderDescs, options);
        } else {
            return ShaderCompilePipeline.createShaderPipeline(new ShaderCompilePipelineLegacy(resourcePath), shaderDescs, options);
        }
    }

    static public ShaderDesc.ShaderModule.Builder makeShaderModuleBuilder(ShaderDesc.ShaderType type, byte[] source) {
        ShaderDesc.ShaderModule.Builder builder = ShaderDesc.ShaderModule.newBuilder();
        builder.setShaderType(type);
        builder.setSource(ByteString.copyFrom(source));
        return builder;
    }

    static public ShaderDesc.Shader.Builder makeShaderBuilder(ShaderDesc.Language language, ArrayList<ShaderDesc.ShaderModule> moduleBuilders) {
        ShaderDesc.Shader.Builder builder = ShaderDesc.Shader.newBuilder();
        builder.setLanguage(language);
        builder.addAllModules(moduleBuilders);
        return builder;
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
            System.err.println("Unable to build shader %s - no platform passed in.%n");
            return;
        }

        String resourcePath = args[0];
        String outputPath   = args[1];
        String platform     = args[2];

        ShaderDesc.ShaderType shaderType = parseShaderTypeFromPath(resourcePath);

        Platform outputPlatform = Platform.get(platform);
        IShaderCompiler shaderCompiler = project.getShaderCompiler(outputPlatform);

        if (shaderCompiler == null) {
            System.err.printf("Unable to build shader %s - no shader compiler found.%n", resourcePath);
            return;
        }

        try (BufferedInputStream is = new BufferedInputStream(new FileInputStream(resourcePath));
            BufferedOutputStream os = new BufferedOutputStream(new FileOutputStream(outputPath))) {

            byte[] inBytes = new byte[is.available()];
            is.read(inBytes);

            String source                         = new String(inBytes, StandardCharsets.UTF_8);
            ShaderPreprocessor shaderPreprocessor = new ShaderPreprocessor(project, resourcePath, source);
            String finalShaderSource              = shaderPreprocessor.getCompiledSource();

            //ShaderCompileResult shaderCompilerResult = shaderCompiler.compile(finalShaderSource, shaderType, outputPath, true, true);
            //ShaderDescBuildResult shaderDescResult = buildResultsToShaderDescBuildResults(shaderCompilerResult, shaderType);
            //shaderDescResult.shaderDesc.writeTo(os);
        }
    }
}

/*
public abstract class ShaderProgramBuilder extends Builder {

    ShaderPreprocessor shaderPreprocessor;

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

    public ShaderDesc getCompiledShaderDesc(Task task, ShaderDesc.ShaderType shaderType) throws IOException, CompileExceptionError {
        boolean outputSpirv                   = getOutputSpirvFlag();
        boolean outputWGSL                    = getOutputWGSLFlag();
        String resourceOutputPath             = task.getOutputs().get(0).getPath();
        ShaderDescBuildResult shaderDescBuildResult = makeShaderDesc(resourceOutputPath, shaderPreprocessor, shaderType, this.project.getPlatformStrings()[0], outputSpirv, outputWGSL);
        handleShaderDescBuildResult(shaderDescBuildResult, resourceOutputPath);
        return shaderDescBuildResult.shaderDesc;
    }
}
*/
