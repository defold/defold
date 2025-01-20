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

import com.dynamo.bob.util.ComponentsCounter;
import com.dynamo.render.proto.Font;
import com.dynamo.render.proto.Material;
import org.apache.commons.io.FilenameUtils;

import java.io.*;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;
import java.util.List;

import com.dynamo.bob.fs.IResource;

import com.dynamo.bob.pipeline.ShaderUtil.Common;
import com.dynamo.bob.pipeline.ShaderUtil.VariantTextureArrayFallback;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.bob.Task;
import com.dynamo.graphics.proto.Graphics.ShaderDesc;
import com.dynamo.graphics.proto.Graphics.VertexAttribute;
import com.dynamo.render.proto.Material.MaterialDesc;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.ProtoBuilder;
import com.dynamo.bob.ProtoParams;

// For tests
import com.google.protobuf.TextFormat;

@ProtoParams(srcClass = MaterialDesc.class, messageClass = MaterialDesc.class)
@BuilderParams(name = "Material", inExts = {".material"}, outExt = ".materialc")
public class MaterialBuilder extends ProtoBuilder<MaterialDesc.Builder> {

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
        Task task = builder.create(resource);
        return null;
        // return builder.getCompiledShaderDesc(task, shaderType);
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

        /*
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
        */
    }

    private ShaderProgramBuildContext makeShaderProgramBuildContext(MaterialDesc.Builder materialBuilder, String shaderResourcePath) throws CompileExceptionError, IOException {
        IResource shaderResource = this.project.getResource(shaderResourcePath);
        String shaderFileInExt   = FilenameUtils.getExtension(shaderResourcePath);
        String shaderFileOutExt  = shaderFileInExt + "c";

        ShaderDesc.ShaderType shaderType;
        ShaderProgramBuilder shaderBuilder = new ShaderProgramBuilder();

        if (shaderFileInExt.equals("vp")) {
            shaderType = ShaderDesc.ShaderType.SHADER_TYPE_VERTEX;
        } else {
            shaderType = ShaderDesc.ShaderType.SHADER_TYPE_FRAGMENT;
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
        Set<String> mergedArraySamplers = new HashSet<>();
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

    private static String getShaderPath(MaterialDesc.Builder fromBuilder, String ext) {
        long shaderProgramHash = MurmurHash.hash64(fromBuilder.getVertexProgram() + fromBuilder.getFragmentProgram());
        return String.format("shader_%d%s", shaderProgramHash, ext);
    }

    private IResource getShaderProgram(MaterialDesc.Builder fromBuilder) {
        String shaderPath = getShaderPath(fromBuilder, ShaderProgramBuilderBundle.EXT);
        return this.project.getResource(shaderPath);
    }

    @Override
    public Task create(IResource input) throws IOException, CompileExceptionError {
        MaterialDesc.Builder materialBuilder = getSrcBuilder(input);

        // The material should depend on the finally built shader resource file
        // that is a combination of one or more shader modules
        IResource shaderResource = getShaderProgram(materialBuilder);
        IResource shaderResourceOut = shaderResource.changeExt(ShaderProgramBuilderBundle.EXT_OUT);

        System.out.print("MaterialBuilder.create " + input.getPath());

        ShaderProgramBuilderBundle.ModuleBundle modules = ShaderProgramBuilderBundle.createBundle();
        modules.add(materialBuilder.getVertexProgram());
        modules.add(materialBuilder.getFragmentProgram());
        shaderResourceOut.setContent(modules.toByteArray());

        Task.TaskBuilder materialTaskBuilder = Task.newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addOutput(input.changeExt(params.outExt()));

        createSubTask(shaderResourceOut, materialTaskBuilder);

        return materialTaskBuilder.build();
    }

    @Override
    public void build(Task task) throws CompileExceptionError, IOException {
        IResource res = task.firstInput();
        MaterialDesc.Builder materialBuilder = getSrcBuilder(res);

        System.out.print("MaterialBuilder.build: " + res.getPath());

        migrateTexturesToSamplers(materialBuilder);
        buildVertexAttributes(materialBuilder);
        buildSamplers(materialBuilder);

        IResource shaderResource = getShaderProgram(materialBuilder);
        IResource shaderResourceOut = shaderResource.changeExt(ShaderProgramBuilderBundle.EXT_OUT);

        BuilderUtil.checkResource(this.project, res, "shader program", shaderResourceOut.getPath());
        materialBuilder.setProgram("/" + BuilderUtil.replaceExt(shaderResource.getPath(), ".spc"));

        MaterialDesc materialDesc = materialBuilder.build();
        task.output(0).setContent(materialDesc.toByteArray());

        /*
        ShaderProgramBuildContext vertexBuildContext   = makeShaderProgramBuildContext(materialBuilder, materialBuilder.getVertexProgram());
        ShaderProgramBuildContext fragmentBuildContext = makeShaderProgramBuildContext(materialBuilder, materialBuilder.getFragmentProgram());
        validateSpirvShaders(vertexBuildContext, fragmentBuildContext);
        */

        //applyShaderProgramBuildContexts(materialBuilder, vertexBuildContext, fragmentBuildContext);

        //BuilderUtil.checkResource(this.project, res, "vertex program", vertexBuildContext.buildPath);
        //BuilderUtil.checkResource(this.project, res, "fragment program", fragmentBuildContext.buildPath);
        //materialBuilder.setVertexProgram(BuilderUtil.replaceExt(vertexBuildContext.projectPath, ".vp", ".vpc"));
        //materialBuilder.setFragmentProgram(BuilderUtil.replaceExt(fragmentBuildContext.projectPath, ".fp", ".fpc"));

        // IResource variantResource = ctx.resource.changeExt(String.format(TextureArrayFilenameVariantFormat, maxPageCount, outExt));

        //IResource fsShader = this.project.getResource(materialBuilder.getFragmentProgram());
        //IResource vsShader = this.project.getResource(materialBuilder.getVertexProgram());

        /*
        long shaderProgramHash = MurmurHash.hash64(materialBuilder.getVertexProgram() + materialBuilder.getFragmentProgram());

        migrateTexturesToSamplers(materialBuilder);
        buildVertexAttributes(materialBuilder);
        buildSamplers(materialBuilder);

        MaterialDesc materialDesc = materialBuilder.build();
        task.output(0).setContent(materialDesc.toByteArray());
        */
    }

    // Running standalone:
    // java -classpath $DYNAMO_HOME/share/java/bob-light.jar com.dynamo.bob.pipeline.MaterialBuilder <path-in.material> <path-out.materialc> <content-root>
    public static void main(String[] args) throws IOException, CompileExceptionError {

        System.setProperty("java.awt.headless", "true");

        System.out.println("Material-builder: " + args[0]);

        String basedir = ".";
        String pathIn = args[0];
        String pathOut = args[1];
        String shaderName = null;
        File fileIn = new File(pathIn);

        if (args.length >= 3) {
            shaderName = args[2];
        }

        if (args.length >= 4) {
            basedir = args[3];
        }

        try (Reader reader = new BufferedReader(new FileReader(pathIn));
             OutputStream output = new BufferedOutputStream(new FileOutputStream(pathOut))) {

            MaterialDesc.Builder materialBuilder = MaterialDesc.newBuilder();
            TextFormat.merge(reader, materialBuilder);

            // TODO: We should probably add the other things that materialbuilder does, but for now this is the minimal
            //       amount of work to get the tests working.

            if (shaderName == null) {
                shaderName = getShaderPath(materialBuilder, ".spc");
            }

            long shaderProgramHash = MurmurHash.hash64(materialBuilder.getVertexProgram() + materialBuilder.getFragmentProgram());
            System.out.println(materialBuilder.getVertexProgram() + ", " + materialBuilder.getFragmentProgram());
            System.out.println("Hash: " + shaderProgramHash);

            // Construct the project-relative path based from the input material file
            Path basedirAbsolutePath = Paths.get(basedir).toAbsolutePath();
            Path shaderProgramProjectPath = Paths.get(fileIn.getAbsolutePath().replace(fileIn.getName(), shaderName));
            Path shaderProgramRelativePath = basedirAbsolutePath.relativize(shaderProgramProjectPath);
            String shaderProgramProjectStr = "/" + shaderProgramRelativePath.toString().replace("\\", "/");

            materialBuilder.setProgram(shaderProgramProjectStr);

            //materialBuilder.setProgram("/" + BuilderUtil.replaceExt(shaderPath, ".spc"));

            System.out.println("Shader path: " + materialBuilder.getProgram());

            migrateTexturesToSamplers(materialBuilder);

            buildVertexAttributes(materialBuilder);
            buildSamplers(materialBuilder);

            MaterialDesc materialDesc = materialBuilder.build();
            materialDesc.writeTo(output);
        }
    }
}
