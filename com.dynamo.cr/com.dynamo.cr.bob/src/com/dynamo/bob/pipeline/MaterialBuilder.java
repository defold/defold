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
import com.dynamo.bob.pipeline.ShaderUtil.ES2Variants;
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

    private ShaderDesc.Shader getTextureArrayShader(ShaderDesc shaderDesc) {
        for (int i=0; i < shaderDesc.getShadersCount(); i++) {
            ShaderDesc.Shader shader = shaderDesc.getShaders(i);

            // TODO paged-atlas: This should be != es2 shader languages
            if (shader.getLanguage() == ShaderDesc.Language.LANGUAGE_SPIRV) {
                continue;
            }

            for (ShaderDesc.ResourceBinding binding : shader.getUniformsList()) {
                if (binding.getType() == ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER2D_ARRAY) {
                    return shader;
                }
            }
        }

        return null;
    }

    private IResource makeVariantTextureArrays(String resourcePath, ES2ToES3Converter.ShaderType shaderType, int maxPageCount, HashMap<String, long[]> samplerIndirectionMap) throws IOException, CompileExceptionError {
        IResource shaderResource = this.project.getResource(resourcePath);
        ShaderDesc shaderDesc = null;
        String shaderFileExt = null;

        if (shaderType == ES2ToES3Converter.ShaderType.VERTEX_SHADER) {
            shaderDesc    = getShaderDesc(new VertexProgramBuilder(), shaderResource, shaderType);
            shaderFileExt = "vpc";
        } else {
            shaderDesc    = getShaderDesc(new FragmentProgramBuilder(), shaderResource, shaderType);
            shaderFileExt = "fpc";
        }

        ShaderDesc.Shader shaderWithArraySamplers = getTextureArrayShader(shaderDesc);
        if (shaderWithArraySamplers == null) {
            return null;
        }

        IResource variantResource = shaderResource.changeExt(String.format(TextureArrayFilenameVariantFormat, maxPageCount, shaderFileExt));

        ShaderDesc.Builder variantShaderDescBuilder = ShaderDesc.newBuilder();
        variantShaderDescBuilder.addAllShaders(shaderDesc.getShadersList());

        Common.GLSLCompileResult sourceVariantTextureArray = ES2Variants.variantTextureArrayFallback(new String(shaderResource.getContent()), maxPageCount);

        if (sourceVariantTextureArray != null) {
            ShaderDesc.Shader.Builder builder = ShaderDesc.Shader.newBuilder();
            Common.GLSLCompileResult compileResult = ShaderProgramBuilder.compileGLSL(sourceVariantTextureArray.source,
                ES2ToES3Converter.ShaderType.FRAGMENT_SHADER,
                shaderWithArraySamplers.getLanguage(), false);

            builder.setLanguage(shaderWithArraySamplers.getLanguage());
            builder.setSource(ByteString.copyFrom(compileResult.source, "UTF-8"));
            builder.setVariantTextureArray(true);

            for (String samplerName : sourceVariantTextureArray.arraySamplers) {
                ShaderDesc.ResourceBinding.Builder resourceBindingBuilder = ShaderDesc.ResourceBinding.newBuilder();
                resourceBindingBuilder.setName(samplerName);
                resourceBindingBuilder.setType(ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER2D_ARRAY);
                resourceBindingBuilder.setElementCount(1);
                resourceBindingBuilder.setSet(0);
                resourceBindingBuilder.setBinding(0);
                builder.addUniforms(resourceBindingBuilder);

                // Create samplerName -> list of indirection hashes
                if (samplerIndirectionMap.get(samplerName) == null) {
                    long[] samplerIndirectionEntry = new long[maxPageCount];
                    for (int i=0; i < maxPageCount; i++) {
                        samplerIndirectionEntry[i] = MurmurHash.hash64(ES2Variants.variantSamplerToSliceSampler(samplerName, i));
                    }
                    samplerIndirectionMap.put(samplerName, samplerIndirectionEntry);
                }
            }

            variantShaderDescBuilder.addShaders(builder.build());
        }

        variantResource.setContent(variantShaderDescBuilder.build().toByteArray());


        return variantResource;
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

        IResource vertexVariantTextureArrays   = makeVariantTextureArrays(materialBuilder.getVertexProgram(), ES2ToES3Converter.ShaderType.VERTEX_SHADER, materialBuilder.getMaxPageCount(), samplerIndirectionMap);
        IResource fragmentVariantTextureArrays = makeVariantTextureArrays(materialBuilder.getFragmentProgram(), ES2ToES3Converter.ShaderType.FRAGMENT_SHADER, materialBuilder.getMaxPageCount(), samplerIndirectionMap);

        if (vertexVariantTextureArrays != null) {
            BuilderUtil.checkResource(this.project, res, "vertex program", vertexVariantTextureArrays.getPath());
            materialBuilder.setVertexProgram(BuilderUtil.replaceExt(materialBuilder.getVertexProgram(), ".vp", String.format(TextureArrayFilenameVariantFormat, materialBuilder.getMaxPageCount(), "vpc")));
        } else {
            BuilderUtil.checkResource(this.project, res, "vertex program", materialBuilder.getVertexProgram());
            materialBuilder.setVertexProgram(BuilderUtil.replaceExt(materialBuilder.getVertexProgram(), ".vp", ".vpc"));
        }

        if (fragmentVariantTextureArrays != null) {
            BuilderUtil.checkResource(this.project, res, "fragment program", fragmentVariantTextureArrays.getPath());
            materialBuilder.setFragmentProgram(BuilderUtil.replaceExt(materialBuilder.getFragmentProgram(), ".fp", String.format(TextureArrayFilenameVariantFormat, materialBuilder.getMaxPageCount(), "fpc")));
        } else {
            BuilderUtil.checkResource(this.project, res, "fragment program", materialBuilder.getFragmentProgram());
            materialBuilder.setFragmentProgram(BuilderUtil.replaceExt(materialBuilder.getFragmentProgram(), ".fp", ".fpc"));
        }

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
