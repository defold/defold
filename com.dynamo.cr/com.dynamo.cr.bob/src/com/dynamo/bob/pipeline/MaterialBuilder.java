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

import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;

import com.dynamo.bob.ProtoParams;
import com.dynamo.bob.pipeline.ShaderUtil.Common;
import com.dynamo.bob.pipeline.ShaderUtil.VariantTextureArrayFallback;
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

    private static class ShaderProgramBuildContext {
        public String                buildPath;
        public String                projectPath;
        public IResource             resource;
        public ShaderDesc.ShaderType type;
        public ShaderDesc            desc;

        // Variant specific state
        public boolean hasTextureArrayVariant;
        public String[] arraySamplers = new String[0];
    }

    private ShaderDesc getShaderDesc(IResource resource, ShaderProgramBuilder builder, ShaderDesc.ShaderType shaderType) throws IOException, CompileExceptionError {
        builder.setProject(this.project);
        Task<ShaderPreprocessor> task = builder.create(resource);
        return builder.getCompiledShaderDesc(task, shaderType);
    }

    private void validateSpirvShaders(ShaderProgramBuildContext vertexBuildContext, ShaderProgramBuildContext fragmentBuildContext) throws CompileExceptionError {
        for (ShaderDesc.ResourceBinding input : fragmentBuildContext.desc.getReflection().getInputsList()) {
            boolean input_found = false;
            for (ShaderDesc.ResourceBinding output : vertexBuildContext.desc.getReflection().getOutputsList()) {
                if (output.getNameHash() == input.getNameHash()) {
                    input_found = true;
                    break;
                }
            }
            if (!input_found) {
                throw new CompileExceptionError(String.format("Input of fragment shader '%s' not written by vertex shader", input.getName()));
            }
        }
    }

    private ShaderDesc.Shader getTextureArrayShader(ShaderDesc desc) {
        for (int i=0; i < desc.getShadersCount(); i++) {
            ShaderDesc.Shader shader = desc.getShaders(i);
            if (VariantTextureArrayFallback.isRequired(shader.getLanguage())) {
                return shader;
            }
        }
        return null;
    }

    private void applyVariantTextureArray(MaterialDesc.Builder materialBuilder, ShaderProgramBuildContext ctx, String inExt, String outExt) throws IOException, CompileExceptionError {

        ShaderDesc.Shader shader = getTextureArrayShader(ctx.desc);
        if (shader == null) {
            return;
        }

        int maxPageCount = materialBuilder.getMaxPageCount();
        String shaderSource = new String(shader.getSource().toByteArray());

        Common.GLSLCompileResult variantCompileResult = VariantTextureArrayFallback.transform(shaderSource, maxPageCount);
        if (variantCompileResult == null || variantCompileResult.arraySamplers.length == 0) {
            return;
        }

        ShaderProgramBuilder.ShaderBuildResult variantBuildResult = ShaderProgramBuilder.makeShaderBuilderFromGLSLSource(variantCompileResult.source, shader.getLanguage());
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
        variantShaderDescBuilder.setReflection(ctx.desc.getReflection());
        variantShaderDescBuilder.setShaderType(ctx.desc.getShaderType());
        variantResource.setContent(variantShaderDescBuilder.build().toByteArray());

        ctx.buildPath             = variantResource.getPath();
        ctx.projectPath           = BuilderUtil.replaceExt(ctx.projectPath, "." + inExt, String.format(TextureArrayFilenameVariantFormat, maxPageCount, inExt));
        ctx.arraySamplers         = variantCompileResult.arraySamplers;
        ctx.hasTextureArrayVariant = true;
    }

    private ShaderProgramBuildContext makeShaderProgramBuildContext(MaterialDesc.Builder materialBuilder, String shaderResourcePath) throws CompileExceptionError, IOException {
        IResource shaderResource = this.project.getResource(shaderResourcePath);
        String shaderFileInExt   = FilenameUtils.getExtension(shaderResourcePath);
        String shaderFileOutExt  = shaderFileInExt + "c";

        ShaderDesc.ShaderType shaderType;
        ShaderProgramBuilder shaderBuilder;

        if (shaderFileInExt.equals("vp")) {
            shaderType    = ShaderDesc.ShaderType.SHADER_TYPE_VERTEX;
            shaderBuilder = new VertexProgramBuilder();
        } else {
            shaderType    = ShaderDesc.ShaderType.SHADER_TYPE_FRAGMENT;
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
        if (!(vertexBuildContext.hasTextureArrayVariant || fragmentBuildContext.hasTextureArrayVariant)) {
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
        return defaultTask(input);
    }

    private static void buildVertexAttributes(MaterialDesc.Builder materialBuilder) throws CompileExceptionError {
        for (int i=0; i < materialBuilder.getAttributesCount(); i++) {
            VertexAttribute materialAttribute = materialBuilder.getAttributes(i);
            materialBuilder.setAttributes(i, GraphicsUtil.buildVertexAttribute(materialAttribute, materialAttribute));
        }
    }

    private static void buildSamplers(MaterialDesc.Builder materialBuilder) throws CompileExceptionError {
        for (int i=0; i < materialBuilder.getSamplersCount(); i++) {
            MaterialDesc.Sampler sampler = materialBuilder.getSamplers(i);
            materialBuilder.setSamplers(i, GraphicsUtil.buildSampler(sampler));
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
