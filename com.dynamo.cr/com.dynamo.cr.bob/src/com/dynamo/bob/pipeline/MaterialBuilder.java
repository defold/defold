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

import org.apache.commons.io.FilenameUtils;

import java.io.IOException;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;
import java.util.List;

import com.dynamo.bob.Bob;
import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Task;
import com.dynamo.bob.Task.TaskBuilder;
import com.dynamo.bob.fs.IResource;

import com.dynamo.bob.pipeline.ShaderPreprocessor;
import com.dynamo.bob.pipeline.ShaderCompilerHelpers;
import com.dynamo.bob.ProtoParams;
import com.dynamo.bob.pipeline.ShaderUtil.Common;
import com.dynamo.bob.pipeline.ShaderUtil.VariantTextureArrayFallback;
import com.dynamo.bob.pipeline.ShaderUtil.ES2ToES3Converter;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.graphics.proto.Graphics.ShaderDesc;
import com.dynamo.graphics.proto.Graphics.VertexAttribute;
import com.dynamo.render.proto.Material.MaterialDesc;

// For tests
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.BufferedOutputStream;
import java.io.BufferedReader;
import java.io.Reader;
import java.io.OutputStream;
import com.google.protobuf.TextFormat;

@ProtoParams(srcClass = MaterialDesc.class, messageClass = MaterialDesc.class)
@BuilderParams(name = "Material", inExts = {".material"}, outExt = ".materialc")
public class MaterialBuilder extends Builder<Void>  {

    private static final String TextureArrayFilenameVariantFormat = "_max_pages_%d.%s";

    private class ShaderProgramBuildContext {
        public String                       buildPath;
        public String                       projectPath;
        public IResource                    resource;
        public ES2ToES3Converter.ShaderType type;
        public ShaderDesc                   desc;

        // Variant specific state
        public boolean hasVertexArrayVariant;
        public String[] arraySamplers = new String[0];
    }

    private ShaderDesc getShaderDesc(IResource resource, ShaderProgramBuilder builder, ES2ToES3Converter.ShaderType shaderType) throws IOException, CompileExceptionError {
        builder.setProject(this.project);
        Task<ShaderPreprocessor> task = builder.create(resource);
        return builder.getCompiledShaderDesc(task, shaderType);
    }

    private ShaderDesc.Language findTextureArrayShaderLanguage(ShaderDesc shaderDesc) {
        ShaderDesc.Language selected = null;
        for (int i=0; i < shaderDesc.getShadersCount(); i++) {
            ShaderDesc.Shader shader = shaderDesc.getShaders(i);
            if (VariantTextureArrayFallback.isRequired(shader.getLanguage())) {
                assert(selected == null);
                selected = shader.getLanguage();
            }
        }

        return selected;
    }

    private ShaderDesc.Shader getSpirvShader(ShaderDesc shaderDesc) {
        for (int i=0; i < shaderDesc.getShadersCount(); i++) {
            ShaderDesc.Shader shader = shaderDesc.getShaders(i);
            if (shader.getLanguage() == ShaderDesc.Language.LANGUAGE_SPIRV) {
                return shader;
            }
        }
        return null;
    }

    private void validateSpirvShaders(ShaderProgramBuildContext vertexBuildContext, ShaderProgramBuildContext fragmentBuildContext) throws CompileExceptionError {
        ShaderDesc.Shader spirvVertex = getSpirvShader(vertexBuildContext.desc);
        if (spirvVertex == null) {
            return;
        }
        ShaderDesc.Shader spirvFragment = getSpirvShader(fragmentBuildContext.desc);

        for (ShaderDesc.ResourceBinding input : spirvFragment.getInputsList()) {
            boolean input_found = false;
            for (ShaderDesc.ResourceBinding output : spirvVertex.getOutputsList()) {
                if (output.getNameHash() == input.getNameHash()) {
                    input_found = true;

                    if (input.getBinding() != output.getBinding()) {
                        throw new CompileExceptionError(
                            String.format("Location mismatch for fragment shader input '%s': The vertex shader specifies the input at location %d, and location %d in the fragment shader.",
                            input.getName(),
                            output.getBinding(),
                            input.getBinding()));
                    }
                    break;
                }
            }
            if (!input_found) {
                throw new CompileExceptionError(String.format("Input of fragment shader '%s' not written by vertex shader", input.getName()));
            }
        }
    }

    private void applyVariantTextureArray(MaterialDesc.Builder materialBuilder, ShaderProgramBuildContext ctx, String inExt, String outExt) throws IOException, CompileExceptionError {

        ShaderDesc.Language shaderLanguage = findTextureArrayShaderLanguage(ctx.desc);
        if (shaderLanguage == null) {
            return;
        }

        String shaderInputSource = new String(ctx.resource.getContent());

        int maxPageCount = materialBuilder.getMaxPageCount();

        // Taken from ShaderProgramBuilder.java
        boolean isDebug = (this.project.hasOption("debug") || (this.project.option("variant", Bob.VARIANT_RELEASE) != Bob.VARIANT_RELEASE));
        Common.GLSLCompileResult variantCompileResult = ShaderProgramBuilder.buildGLSLVariantTextureArray(shaderInputSource, ctx.type, shaderLanguage, isDebug, maxPageCount);

        // No array samplers, we can use original source
        if (variantCompileResult.arraySamplers.length == 0) {
            return;
        }

        ShaderProgramBuilder.ShaderBuildResult variantBuildResult = ShaderCompilerHelpers.makeShaderBuilderFromGLSLSource(variantCompileResult.source, shaderLanguage);

        // JG: AAaah this should not be here, but we need to know of the parsed array samplers for building the indirection map..
        variantBuildResult.shaderBuilder.setVariantTextureArray(true);

        if (variantBuildResult.buildWarnings != null) {
            for(String warningStr : variantBuildResult.buildWarnings) {
                System.err.println(warningStr);
            }
            throw new CompileExceptionError("Errors when producing texture array variant output " + ctx.projectPath);
        }

        IResource variantResource = ctx.resource.changeExt(String.format(TextureArrayFilenameVariantFormat, maxPageCount, outExt));

        // Transfer already built shaders to this variant shader
        ShaderDesc.Builder variantShaderDescBuilder = ShaderDesc.newBuilder();
        variantShaderDescBuilder.addAllShaders(ctx.desc.getShadersList());
        variantShaderDescBuilder.addShaders(variantBuildResult.shaderBuilder);
        variantResource.setContent(variantShaderDescBuilder.build().toByteArray());

        ctx.buildPath             = variantResource.getPath();
        ctx.projectPath           = BuilderUtil.replaceExt(ctx.projectPath, "." + inExt, String.format(TextureArrayFilenameVariantFormat, maxPageCount, inExt));
        ctx.arraySamplers         = variantCompileResult.arraySamplers;
        ctx.hasVertexArrayVariant = true;
    }

    private ShaderProgramBuildContext makeShaderProgramBuildContext(MaterialDesc.Builder materialBuilder, String shaderResourcePath) throws CompileExceptionError, IOException {
        IResource shaderResource = this.project.getResource(shaderResourcePath);
        String shaderFileInExt   = FilenameUtils.getExtension(shaderResourcePath);
        String shaderFileOutExt  = shaderFileInExt + "c";

        ES2ToES3Converter.ShaderType shaderType;
        ShaderProgramBuilder shaderBuilder;

        if (shaderFileInExt.equals("vp")) {
            shaderType    = ES2ToES3Converter.ShaderType.VERTEX_SHADER;
            shaderBuilder = new VertexProgramBuilder();
        } else {
            shaderType    = ES2ToES3Converter.ShaderType.FRAGMENT_SHADER;
            shaderBuilder = new FragmentProgramBuilder();
        }

        ShaderDesc shaderDesc = getShaderDesc(shaderResource, shaderBuilder, shaderType);

        ShaderProgramBuildContext ctx = new ShaderProgramBuildContext();
        ctx.buildPath   = shaderResourcePath;
        ctx.projectPath = shaderResourcePath;
        ctx.resource    = shaderResource;
        ctx.type        = shaderType;
        ctx.desc        = shaderDesc;

        applyVariantTextureArray(materialBuilder, ctx, shaderFileInExt, shaderFileOutExt);

        return ctx;
    }

    private void applyShaderProgramBuildContexts(MaterialDesc.Builder materialBuilder, ShaderProgramBuildContext vertexBuildContext, ShaderProgramBuildContext fragmentBuildContext) {
        if (!vertexBuildContext.hasVertexArrayVariant || fragmentBuildContext.hasVertexArrayVariant) {
            return;
        }

        // Generate indirection table for all material samplers based on the result of the shader build context
        Set mergedArraySamplers = new HashSet();
        mergedArraySamplers.addAll(Arrays.asList(vertexBuildContext.arraySamplers));
        mergedArraySamplers.addAll(Arrays.asList(fragmentBuildContext.arraySamplers));

        for (int i=0; i < materialBuilder.getSamplersCount(); i++) {
            MaterialDesc.Sampler materialSampler = materialBuilder.getSamplers(i);

            if (mergedArraySamplers.contains(materialSampler.getName())) {
                MaterialDesc.Sampler.Builder samplerBuilder = MaterialDesc.Sampler.newBuilder(materialSampler);

                for (int j = 0; j < materialBuilder.getMaxPageCount(); j++) {
                    samplerBuilder.addNameIndirections(MurmurHash.hash64(VariantTextureArrayFallback.samplerNameToSliceSamplerName(materialSampler.getName(), j)));
                }

                materialBuilder.setSamplers(i, samplerBuilder.build());
            }
        }

        // This value is not needed in the engine specifically for validation in bob
        // I.e we need to set this to zero to check if the combination of material and atlas is correct.
        if (mergedArraySamplers.size() == 0) {
            materialBuilder.setMaxPageCount(0);
        }
    }

    private static void migrateTexturesToSamplers(MaterialDesc.Builder materialBuilder) {
        List<MaterialDesc.Sampler> samplers = materialBuilder.getSamplersList();

        for (String texture : materialBuilder.getTexturesList()) {
            // Cannot migrate textures that are already in samplers
            if (samplers.stream().anyMatch(sampler -> sampler.getName().equals(texture))) {
                continue;
            }

            MaterialDesc.Sampler.Builder samplerBuilder = MaterialDesc.Sampler.newBuilder();
            samplerBuilder.setName(texture);
            samplerBuilder.setWrapU(MaterialDesc.WrapMode.WRAP_MODE_CLAMP_TO_EDGE);
            samplerBuilder.setWrapV(MaterialDesc.WrapMode.WRAP_MODE_CLAMP_TO_EDGE);
            samplerBuilder.setFilterMin(MaterialDesc.FilterModeMin.FILTER_MODE_MIN_LINEAR);
            samplerBuilder.setFilterMag(MaterialDesc.FilterModeMag.FILTER_MODE_MAG_LINEAR);
            materialBuilder.addSamplers(samplerBuilder);
        }

        materialBuilder.clearTextures();
    }

    @Override
    public Task<Void> create(IResource input) throws IOException, CompileExceptionError {
        TaskBuilder<Void> task = Task.<Void> newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addOutput(input.changeExt(params.outExt()));

        MaterialDesc.Builder materialBuilder = MaterialDesc.newBuilder();
        ProtoUtil.merge(input, materialBuilder);

        IResource vertexProgramOutputResource   = this.project.getResource(materialBuilder.getVertexProgram()).changeExt(".vpc");
        IResource fragmentProgramOutputResource = this.project.getResource(materialBuilder.getFragmentProgram()).changeExt(".fpc");

        task.addInput(vertexProgramOutputResource);
        task.addInput(fragmentProgramOutputResource);

        migrateTexturesToSamplers(materialBuilder);

        for (MaterialDesc.Sampler materialSampler : materialBuilder.getSamplersList()) {
            String texture = materialSampler.getTexture();
            if (texture.isEmpty())
                continue;
            IResource res = BuilderUtil.checkResource(this.project, input, "texture", texture);
            Task<?> embedTask = this.project.createTask(res);
            if (embedTask == null) {
                throw new CompileExceptionError(input, 0, String.format("Failed to create build task for component '%s'", res.getPath()));
            }
        }

        return task.build();
    }

    private static void buildVertexAttributes(MaterialDesc.Builder materialBuilder) throws CompileExceptionError {
        for (int i=0; i < materialBuilder.getAttributesCount(); i++) {
            VertexAttribute materialAttribute = materialBuilder.getAttributes(i);
            materialBuilder.setAttributes(i, GraphicsUtil.buildVertexAttribute(materialAttribute, materialAttribute));
        }
    }

    private static void buildSamplers(MaterialDesc.Builder materialBuilder) throws CompileExceptionError {
        for (int i=0; i < materialBuilder.getSamplersCount(); i++) {
            MaterialDesc.Sampler materialSampler = materialBuilder.getSamplers(i);

            MaterialDesc.Sampler.Builder samplerBuilder = MaterialDesc.Sampler.newBuilder(materialSampler);
            samplerBuilder.setNameHash(MurmurHash.hash64(samplerBuilder.getName()));

            String texture = materialSampler.getTexture();
            if (!texture.isEmpty()) {
                samplerBuilder.setTexture(ProtoBuilders.replaceTextureName(texture));
            }

            materialBuilder.setSamplers(i, samplerBuilder.build());
        }
    }

    @Override
    public void build(Task<Void> task) throws CompileExceptionError, IOException {
        IResource res                        = task.input(0);
        MaterialDesc.Builder materialBuilder = MaterialDesc.newBuilder();
        ProtoUtil.merge(task.input(0), materialBuilder);

        ShaderProgramBuildContext vertexBuildContext   = makeShaderProgramBuildContext(materialBuilder, materialBuilder.getVertexProgram());
        ShaderProgramBuildContext fragmentBuildContext = makeShaderProgramBuildContext(materialBuilder, materialBuilder.getFragmentProgram());

        validateSpirvShaders(vertexBuildContext, fragmentBuildContext);

        applyShaderProgramBuildContexts(materialBuilder, vertexBuildContext, fragmentBuildContext);

        BuilderUtil.checkResource(this.project, res, "vertex program", vertexBuildContext.buildPath);
        materialBuilder.setVertexProgram(BuilderUtil.replaceExt(vertexBuildContext.projectPath, ".vp", ".vpc"));

        BuilderUtil.checkResource(this.project, res, "fragment program", fragmentBuildContext.buildPath);
        materialBuilder.setFragmentProgram(BuilderUtil.replaceExt(fragmentBuildContext.projectPath, ".fp", ".fpc"));

        migrateTexturesToSamplers(materialBuilder);
        buildVertexAttributes(materialBuilder);
        buildSamplers(materialBuilder);

        MaterialDesc materialDesc = materialBuilder.build();
        task.output(0).setContent(materialDesc.toByteArray());
    }

    // Running standalone:
    // java -classpath $DYNAMO_HOME/share/java/bob-light.jar com.dynamo.bob.pipeline.MaterialBuilder <path-in.material> <path-out.materialc>
    public static void main(String[] args) throws IOException, CompileExceptionError {

        System.setProperty("java.awt.headless", "true");

        Reader reader       = new BufferedReader(new FileReader(args[0]));
        OutputStream output = new BufferedOutputStream(new FileOutputStream(args[1]));

        try {
            MaterialDesc.Builder materialBuilder = MaterialDesc.newBuilder();
            TextFormat.merge(reader, materialBuilder);

            // TODO: We should probably add the other things that materialbuilder does, but for now this is the minimal
            //       amount of work to get the tests working.
            materialBuilder.setVertexProgram(BuilderUtil.replaceExt(materialBuilder.getVertexProgram(), ".vp", ".vpc"));
            materialBuilder.setFragmentProgram(BuilderUtil.replaceExt(materialBuilder.getFragmentProgram(), ".fp", ".fpc"));

            migrateTexturesToSamplers(materialBuilder);

            buildVertexAttributes(materialBuilder);
            buildSamplers(materialBuilder);

            MaterialDesc materialDesc = materialBuilder.build();
            materialDesc.writeTo(output);
        } finally {
            reader.close();
            output.close();
        }
    }
}
