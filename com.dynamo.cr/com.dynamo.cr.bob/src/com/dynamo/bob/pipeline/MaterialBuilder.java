// Copyright 2020-2023 The Defold Foundation
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
import java.io.ByteArrayInputStream;
import java.io.InputStreamReader;

import java.util.HashMap;
import java.util.Map;

import com.dynamo.bob.Bob;
import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Project;
import com.dynamo.bob.Task;
import com.dynamo.bob.Task.TaskBuilder;
import com.dynamo.bob.fs.IResource;

import com.dynamo.bob.pipeline.ShaderPreprocessor;
import com.dynamo.bob.pipeline.ShaderUtil.Common;
import com.dynamo.bob.pipeline.ShaderUtil.VariantTextureArrayFallback;
import com.dynamo.bob.pipeline.ShaderUtil.ES2ToES3Converter;
import com.dynamo.graphics.proto.Graphics.ShaderDesc;
import com.dynamo.render.proto.Material.MaterialDesc;
import com.dynamo.bob.util.MurmurHash;

import com.google.protobuf.ByteString;
import com.google.protobuf.TextFormat;

@BuilderParams(name = "Material", inExts = {".material"}, outExt = ".materialc")
public class MaterialBuilder extends Builder<Void>  {

    private static final String TextureArrayFilenameVariantFormat = "_max_pages_%d.%s";

    private ShaderDesc getShaderDesc(ShaderProgramBuilder builder, IResource resource, ES2ToES3Converter.ShaderType shaderType) throws IOException, CompileExceptionError {
        builder.setProject(this.project);
        Task<ShaderPreprocessor> task = builder.create(resource);
        return builder.getCompiledShaderDesc(task, shaderType);
    }

    private ShaderDesc.Language findTextureArrayShaderLanguage(ShaderDesc shaderDesc) {
        ShaderDesc.Language selected = null;
        for (int i=0; i < shaderDesc.getShadersCount(); i++) {
            ShaderDesc.Shader shader = shaderDesc.getShaders(i);
            if (VariantTextureArrayFallback.tryBuildVariant(shader.getLanguage())) {
                assert(selected == null);
                selected = shader.getLanguage();
            }
        }

        return selected;
    }

    private class ShaderProgramPaths {
        public String buildPath;
        public String projectPath;

        public ShaderProgramPaths(String buildAndProjectPath) {
            this.buildPath   = buildAndProjectPath;
            this.projectPath = buildAndProjectPath;
        }

        public ShaderProgramPaths(String buildPath, String projectPath) {
            this.buildPath   = buildPath;
            this.projectPath = projectPath;
        }
    }

    private ShaderProgramPaths getShaderProgramPath(String shaderResourcePath, ES2ToES3Converter.ShaderType shaderType, int maxPageCount, HashMap<String, long[]> samplerNameToIndirectHashes) throws CompileExceptionError, IOException {
        IResource shaderResource = this.project.getResource(shaderResourcePath);
        ShaderDesc shaderDesc = null;
        String shaderFileOutExt = null;
        String shaderFileInExt = null;

        if (shaderType == ES2ToES3Converter.ShaderType.VERTEX_SHADER) {
            shaderDesc       = getShaderDesc(new VertexProgramBuilder(), shaderResource, shaderType);
            shaderFileOutExt = "vpc";
            shaderFileInExt  = "vp";
        } else {
            shaderDesc       = getShaderDesc(new FragmentProgramBuilder(), shaderResource, shaderType);
            shaderFileOutExt = "fpc";
            shaderFileInExt  = "fp";
        }

        ShaderDesc.Language shaderLanguage = findTextureArrayShaderLanguage(shaderDesc);
        if (shaderLanguage == null) {
            return new ShaderProgramPaths(shaderResourcePath);
        }

        String shaderInputSource = new String(shaderResource.getContent());

        // Taken from ShaderProgramBuilder.java
        boolean isDebug = (this.project.hasOption("debug") || (this.project.option("variant", Bob.VARIANT_RELEASE) != Bob.VARIANT_RELEASE));
        Common.GLSLCompileResult variantCompileResult = ShaderProgramBuilder.buildGLSLVariantTextureArray(shaderInputSource, shaderType, shaderLanguage, isDebug, maxPageCount);
        if (variantCompileResult.arraySamplers == null) {
            return new ShaderProgramPaths(shaderResourcePath);
        }

        ShaderProgramBuilder.ShaderBuildResult variantBuildResult = ShaderProgramBuilder.makeShaderBuilderFromGLSLSource(variantCompileResult.source, shaderLanguage);

        // If we didn't get a build result, we don't need a variant for texture arrays
        if (variantBuildResult.buildWarnings != null) {
            for(String warningStr : variantBuildResult.buildWarnings) {
                System.err.println(warningStr);
            }
            throw new CompileExceptionError("Errors when producing texture array variant output " + shaderResourcePath);
        }

        IResource variantResource = shaderResource.changeExt(String.format(TextureArrayFilenameVariantFormat, maxPageCount, shaderFileOutExt));

        // Transfer already built shaders to this variant shader
        ShaderDesc.Builder variantShaderDescBuilder = ShaderDesc.newBuilder();
        variantShaderDescBuilder.addAllShaders(shaderDesc.getShadersList());
        variantShaderDescBuilder.addShaders(variantBuildResult.shaderBuilder);
        variantResource.setContent(variantShaderDescBuilder.build().toByteArray());

        for (String samplerName : variantCompileResult.arraySamplers) {
            // Create samplerName -> list of indirection hashes
            if (samplerNameToIndirectHashes.get(samplerName) == null) {
                long[] samplerIndirectionEntry = new long[maxPageCount];
                for (int i=0; i < maxPageCount; i++) {
                    samplerIndirectionEntry[i] = MurmurHash.hash64(VariantTextureArrayFallback.samplerNameToSliceSamplerName(samplerName, i));
                }
                samplerNameToIndirectHashes.put(samplerName, samplerIndirectionEntry);
            }
        }

        String buildOutputPath = variantResource.getPath();
        String projectPath     = BuilderUtil.replaceExt(shaderResourcePath, "." + shaderFileInExt, String.format(TextureArrayFilenameVariantFormat, maxPageCount, shaderFileInExt));
        return new ShaderProgramPaths(buildOutputPath, projectPath);
    }

    @Override
    public Task<Void> create(IResource input) throws IOException, CompileExceptionError {

        Task.TaskBuilder<Void> taskBuilder = Task.<Void>newBuilder(this)
            .setName(params.name())
            .addInput(input);

        ByteArrayInputStream is              = new ByteArrayInputStream(input.getContent());
        InputStreamReader reader             = new InputStreamReader(is);
        MaterialDesc.Builder materialBuilder = MaterialDesc.newBuilder();
        TextFormat.merge(reader, materialBuilder);

        taskBuilder.addOutput(input.changeExt(params.outExt()));

        return taskBuilder.build();
    }

    @Override
    public void build(Task<Void> task) throws CompileExceptionError, IOException {

        IResource res                        = task.input(0);
        ByteArrayInputStream is              = new ByteArrayInputStream(res.getContent());
        InputStreamReader reader             = new InputStreamReader(is);
        MaterialDesc.Builder materialBuilder = MaterialDesc.newBuilder();
        TextFormat.merge(reader, materialBuilder);

        HashMap<String, long[]> samplerIndirectionMap = new HashMap<String, long[]>();

        ShaderProgramPaths vertexShaderPaths   = getShaderProgramPath(materialBuilder.getVertexProgram(), ES2ToES3Converter.ShaderType.VERTEX_SHADER, materialBuilder.getMaxPageCount(), samplerIndirectionMap);
        ShaderProgramPaths fragmentShaderPaths = getShaderProgramPath(materialBuilder.getFragmentProgram(), ES2ToES3Converter.ShaderType.FRAGMENT_SHADER, materialBuilder.getMaxPageCount(), samplerIndirectionMap);

        BuilderUtil.checkResource(this.project, res, "vertex program", vertexShaderPaths.buildPath);
        materialBuilder.setVertexProgram(BuilderUtil.replaceExt(vertexShaderPaths.projectPath, ".vp", ".vpc"));

        BuilderUtil.checkResource(this.project, res, "fragment program", fragmentShaderPaths.buildPath);
        materialBuilder.setFragmentProgram(BuilderUtil.replaceExt(fragmentShaderPaths.projectPath, ".fp", ".fpc"));

        for (int i=0; i < materialBuilder.getSamplersCount(); i++) {

            MaterialDesc.Sampler materialSampler = materialBuilder.getSamplers(i);
            long[] samplerIndirectionTable = samplerIndirectionMap.get(materialSampler.getName());

            if (samplerIndirectionTable != null) {
                MaterialDesc.Sampler.Builder samplerBuilder = MaterialDesc.Sampler.newBuilder(materialSampler);

                for (int j=0; j < samplerIndirectionTable.length; j++) {
                    samplerBuilder.addNameIndirections(samplerIndirectionTable[j]);
                }

                materialBuilder.setSamplers(i, samplerBuilder.build());
            }
        }

        MaterialDesc materialDesc = materialBuilder.build();
        task.output(0).setContent(materialDesc.toByteArray());
    }
}
