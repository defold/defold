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

import java.io.*;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.List;

import com.dynamo.bob.fs.IResource;

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

    public static String getShaderName(String vertexProgram, String fragmentProgram, int maxPageCount, String ext) {
        long shaderProgramHash = MurmurHash.hash64(vertexProgram + fragmentProgram + maxPageCount);
        return String.format("shader_%d%s", shaderProgramHash, ext);
    }

    private static String getShaderName(MaterialDesc.Builder fromBuilder, String ext) {
        return getShaderName(fromBuilder.getVertexProgram(), fromBuilder.getFragmentProgram(), fromBuilder.getMaxPageCount(), ext);
    }

    private IResource getShaderProgram(MaterialDesc.Builder fromBuilder) {
        String shaderPath = getShaderName(fromBuilder, ShaderProgramBuilderBundle.EXT);
        return this.project.getResource(shaderPath);
    }

    @Override
    public Task create(IResource input) throws IOException, CompileExceptionError {
        MaterialDesc.Builder materialBuilder = getSrcBuilder(input);

        // The material should depend on the finally built shader resource file
        // that is a combination of one or more shader modules
        IResource shaderResource = getShaderProgram(materialBuilder);
        IResource shaderResourceOut = shaderResource.changeExt(ShaderProgramBuilderBundle.EXT_OUT);

        IShaderCompiler.CompileOptions compileOptions = new IShaderCompiler.CompileOptions();
        compileOptions.maxPageCount = materialBuilder.getMaxPageCount();

        ShaderProgramBuilderBundle.ModuleBundle modules = ShaderProgramBuilderBundle.createBundle();
        modules.addModule(materialBuilder.getVertexProgram());
        modules.addModule(materialBuilder.getFragmentProgram());
        modules.setCompileOptions(compileOptions);

        shaderResourceOut.setContent(modules.toByteArray());

        Task.TaskBuilder materialTaskBuilder = Task.newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addOutput(input.changeExt(params.outExt()));

        createSubTask(shaderResourceOut, materialTaskBuilder);

        return materialTaskBuilder.build();
    }

    private void applyVariantTextureArray(MaterialDesc.Builder materialBuilder, ShaderDesc.Builder shaderBuilder) {
        for (ShaderDesc.Shader shader : shaderBuilder.getShadersList()) {
            if (shader.getVariantTextureArray()) {
                for (ShaderDesc.ResourceBinding resourceBinding : shaderBuilder.getReflection().getTexturesList()) {
                    if (resourceBinding.getType().getShaderType() == ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER2D_ARRAY ||
                            resourceBinding.getType().getShaderType() == ShaderDesc.ShaderDataType.SHADER_TYPE_TEXTURE2D_ARRAY) {

                        MaterialDesc.Sampler.Builder samplerBuilder = getSamplerBuilder(materialBuilder.getSamplersBuilderList(), resourceBinding.getName());

                        if (samplerBuilder != null) {
                            for (int i = 0; i < materialBuilder.getMaxPageCount(); i++) {
                                samplerBuilder.addNameIndirections(MurmurHash.hash64(VariantTextureArrayFallback.samplerNameToSliceSamplerName(resourceBinding.getName(), i)));
                            }
                        }
                    }
                }
                break;
            }
        }
    }

    @Override
    public void build(Task task) throws CompileExceptionError, IOException {
        IResource res = task.firstInput();
        IResource resShader = task.input(1);

        MaterialDesc.Builder materialBuilder = getSrcBuilder(res);

        BuilderUtil.checkResource(this.project, res, "vertex program", materialBuilder.getVertexProgram());
        BuilderUtil.checkResource(this.project, res, "fragment program", materialBuilder.getFragmentProgram());

        migrateTexturesToSamplers(materialBuilder);
        buildVertexAttributes(materialBuilder);
        buildSamplers(materialBuilder);

        getShaderProgram(materialBuilder);
        IResource shaderResource = getShaderProgram(materialBuilder);

        materialBuilder.setProgram("/" + BuilderUtil.replaceExt(shaderResource.getPath(), ".spc"));

        ShaderDesc.Builder shaderBuilder = ShaderDesc.newBuilder();
        shaderBuilder.mergeFrom(resShader.getContent());

        applyVariantTextureArray(materialBuilder, shaderBuilder);

        MaterialDesc materialDesc = materialBuilder.build();
        task.output(0).setContent(materialDesc.toByteArray());
    }

    private MaterialDesc.Sampler.Builder getSamplerBuilder(List<MaterialDesc.Sampler.Builder> samplerBuilders, String samplerName) {
        for (MaterialDesc.Sampler.Builder builder : samplerBuilders) {
            if (builder.getName().equals(samplerName)) {
                return builder;
            }
        }
        return null;
    }

    // Running standalone:
    // java -classpath $DYNAMO_HOME/share/java/bob-light.jar com.dynamo.bob.pipeline.MaterialBuilder <path-in.material> <path-out.materialc> <content-root>
    public static void main(String[] args) throws IOException, CompileExceptionError {

        System.setProperty("java.awt.headless", "true");

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
                shaderName = getShaderName(materialBuilder, ".spc");
            }

            // Construct the project-relative path based from the input material file
            Path basedirAbsolutePath = Paths.get(basedir).toAbsolutePath();
            Path shaderProgramProjectPath = Paths.get(fileIn.getAbsolutePath().replace(fileIn.getName(), shaderName));
            Path shaderProgramRelativePath = basedirAbsolutePath.relativize(shaderProgramProjectPath);
            String shaderProgramProjectStr = "/" + shaderProgramRelativePath.toString().replace("\\", "/");

            materialBuilder.setProgram(shaderProgramProjectStr);

            migrateTexturesToSamplers(materialBuilder);

            buildVertexAttributes(materialBuilder);
            buildSamplers(materialBuilder);

            MaterialDesc materialDesc = materialBuilder.build();
            materialDesc.writeTo(output);
        }
    }
}
