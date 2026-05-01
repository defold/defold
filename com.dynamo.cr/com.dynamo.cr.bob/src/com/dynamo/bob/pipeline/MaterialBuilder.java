// Copyright 2020-2026 The Defold Foundation
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
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.fs.ResourceUtil;

import com.dynamo.bob.pipeline.ShaderUtil.VariantTextureArrayFallback;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.bob.Task;
import com.dynamo.graphics.proto.Graphics;
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

    // This is the uniform name we are looking for in the reflection:
    private static final String PBR_MATERIAL_NAME = "PbrMaterial";

    public static enum PbrParameterType
    {
        PBR_PARAMETER_TYPE_METALLIC_ROUGHNESS,
        PBR_PARAMETER_TYPE_SPECULAR_GLOSSINESS,
        PBR_PARAMETER_TYPE_CLEARCOAT,
        PBR_PARAMETER_TYPE_TRANSMISSION,
        PBR_PARAMETER_TYPE_IOR,
        PBR_PARAMETER_TYPE_SPECULAR,
        PBR_PARAMETER_TYPE_VOLUME,
        PBR_PARAMETER_TYPE_SHEEN,
        PBR_PARAMETER_TYPE_EMISSIVE_STRENGTH,
        PBR_PARAMETER_TYPE_IRIDESCENCE,
    }

    private static final HashMap<String, PbrParameterType> pbrStructNameToParameterType = new HashMap<>();
    static {
        pbrStructNameToParameterType.put("PbrMetallicRoughness", PbrParameterType.PBR_PARAMETER_TYPE_METALLIC_ROUGHNESS);
        pbrStructNameToParameterType.put("PbrSpecularGlossiness", PbrParameterType.PBR_PARAMETER_TYPE_SPECULAR_GLOSSINESS);
        pbrStructNameToParameterType.put("PbrClearCoat", PbrParameterType.PBR_PARAMETER_TYPE_CLEARCOAT);
        pbrStructNameToParameterType.put("PbrTransmission", PbrParameterType.PBR_PARAMETER_TYPE_TRANSMISSION);
        pbrStructNameToParameterType.put("PbrIor", PbrParameterType.PBR_PARAMETER_TYPE_IOR);
        pbrStructNameToParameterType.put("PbrSpecular", PbrParameterType.PBR_PARAMETER_TYPE_SPECULAR);
        pbrStructNameToParameterType.put("PbrVolume", PbrParameterType.PBR_PARAMETER_TYPE_VOLUME);
        pbrStructNameToParameterType.put("PbrSheen", PbrParameterType.PBR_PARAMETER_TYPE_SHEEN);
        pbrStructNameToParameterType.put("PbrEmissiveStrength", PbrParameterType.PBR_PARAMETER_TYPE_EMISSIVE_STRENGTH);
        pbrStructNameToParameterType.put("PbrIridescence", PbrParameterType.PBR_PARAMETER_TYPE_IRIDESCENCE);
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
        IResource shaderResourceOut = shaderResource.disableMinifyPath().changeExt(ShaderProgramBuilderBundle.EXT_OUT);

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

    private static void applyVariantTextureArray(MaterialDesc.Builder materialBuilder, ShaderDesc.Builder shaderBuilder) {
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

    private static void addPbrParameter(MaterialDesc.PbrParameters.Builder pbrParametersBuilder, PbrParameterType type) {
        switch(type) {
            case PBR_PARAMETER_TYPE_METALLIC_ROUGHNESS -> pbrParametersBuilder.setHasMetallicRoughness(true);
            case PBR_PARAMETER_TYPE_SPECULAR_GLOSSINESS -> pbrParametersBuilder.setHasSpecularGlossiness(true);
            case PBR_PARAMETER_TYPE_CLEARCOAT -> pbrParametersBuilder.setHasClearcoat(true);
            case PBR_PARAMETER_TYPE_TRANSMISSION -> pbrParametersBuilder.setHasTransmission(true);
            case PBR_PARAMETER_TYPE_IOR -> pbrParametersBuilder.setHasIor(true);
            case PBR_PARAMETER_TYPE_SPECULAR -> pbrParametersBuilder.setHasSpecular(true);
            case PBR_PARAMETER_TYPE_VOLUME -> pbrParametersBuilder.setHasVolume(true);
            case PBR_PARAMETER_TYPE_SHEEN -> pbrParametersBuilder.setHasSheen(true);
            case PBR_PARAMETER_TYPE_EMISSIVE_STRENGTH -> pbrParametersBuilder.setHasEmissiveStrength(true);
            case PBR_PARAMETER_TYPE_IRIDESCENCE -> pbrParametersBuilder.setHasIridescence(true);
        }
    }

    private static PbrParameterType[] getPbrParameters(Graphics.ShaderDesc.ShaderReflection shaderReflection) {
        ArrayList<PbrParameterType> params = new ArrayList<>();
        if (shaderReflection != null) {
            for (ShaderDesc.ResourceBinding resourceBinding : shaderReflection.getUniformBuffersList()) {
                ShaderDesc.ResourceType resourceType = resourceBinding.getType();
                ShaderDesc.ResourceTypeInfo resourceTypeInfo = shaderReflection.getTypes(resourceType.getTypeIndex());

                if (resourceTypeInfo.getName().equals(PBR_MATERIAL_NAME)) {
                    for (ShaderDesc.ResourceMember resourceMember : resourceTypeInfo.getMembersList()) {
                        ShaderDesc.ResourceTypeInfo resourceMemberTypeInfo = shaderReflection.getTypes(resourceMember.getType().getTypeIndex());
                        String resourceMemberTypeName = resourceMemberTypeInfo.getName();
                        if (pbrStructNameToParameterType.containsKey(resourceMemberTypeName)) {
                            params.add(pbrStructNameToParameterType.get(resourceMemberTypeName));
                        }
                    }
                }
            }

        }
        return params.toArray(new PbrParameterType[0]);
    }

    public static MaterialDesc.PbrParameters makePbrParametersProtoMessage(Graphics.ShaderDesc.ShaderReflection shaderReflection) {
        MaterialDesc.PbrParameters.Builder pbrParametersBuilder = MaterialDesc.PbrParameters.newBuilder();
        PbrParameterType[] parameters = getPbrParameters(shaderReflection);
        for (PbrParameterType parameter : parameters) {
            addPbrParameter(pbrParametersBuilder, parameter);
        }

        if (parameters.length > 0) {
            pbrParametersBuilder.setHasParameters(true);
            return pbrParametersBuilder.build();
        }
        return null;
    }

    private static void buildPbrParameters(MaterialDesc.Builder materialBuilder, ShaderDesc.Builder shaderBuilder) {
        MaterialDesc.PbrParameters builtPbrParameters = makePbrParametersProtoMessage(shaderBuilder.getReflection());
        if (builtPbrParameters != null) {
            materialBuilder.setPbrParameters(builtPbrParameters);
        }
    }

    // Auxiliary data is constructed here for the materialBuilder.
    // It is used for both the stand-alone invocation and during project building.
    private static MaterialDesc finalizeMaterial(MaterialDesc.Builder materialBuilder, ShaderDesc.Builder shaderBuilder) throws CompileExceptionError {
        migrateTexturesToSamplers(materialBuilder);
        buildVertexAttributes(materialBuilder);
        buildSamplers(materialBuilder);
        if (shaderBuilder != null) {
            applyVariantTextureArray(materialBuilder, shaderBuilder);
            buildPbrParameters(materialBuilder, shaderBuilder);
        }
        return materialBuilder.build();
    }

    @Override
    public void build(Task task) throws CompileExceptionError, IOException {
        IResource res = task.firstInput();
        IResource resShader = task.input(1);

        MaterialDesc.Builder materialBuilder = getSrcBuilder(res);

        BuilderUtil.checkResource(this.project, res, "vertex program", materialBuilder.getVertexProgram());
        BuilderUtil.checkResource(this.project, res, "fragment program", materialBuilder.getFragmentProgram());

        getShaderProgram(materialBuilder);
        IResource shaderResource = getShaderProgram(materialBuilder);

        materialBuilder.setProgram(ResourceUtil.minifyPathAndChangeExt(shaderResource.getPath(), ".spc"));
        materialBuilder.setVertexProgram("");
        materialBuilder.setFragmentProgram("");

        ShaderDesc.Builder shaderBuilder = ShaderDesc.newBuilder();
        shaderBuilder.mergeFrom(resShader.getContent());

        MaterialDesc materialDesc = finalizeMaterial(materialBuilder, shaderBuilder);

        task.output(0).setContent(materialDesc.toByteArray());
    }

    private static MaterialDesc.Sampler.Builder getSamplerBuilder(List<MaterialDesc.Sampler.Builder> samplerBuilders, String samplerName) {
        for (MaterialDesc.Sampler.Builder builder : samplerBuilders) {
            if (builder.getName().equals(samplerName)) {
                return builder;
            }
        }
        return null;
    }

    private static ShaderDesc.Builder getShaderBuilderFromResource(String path) {
        try {
            byte[] data = Files.readAllBytes(Paths.get(path));
            ShaderDesc.Builder shaderBuilder = ShaderDesc.newBuilder();
            shaderBuilder.mergeFrom(data);
            return shaderBuilder;
        } catch (Exception e) {
            return null;
        }
    }

    // Running standalone:
    // java -classpath $DYNAMO_HOME/share/java/bob-light.jar com.dynamo.bob.pipeline.MaterialBuilder <path-in.material> <path-in.spc> <path-out.materialc> <shader-name> <content-root>
    public static void main(String[] args) throws IOException, CompileExceptionError {

        System.setProperty("java.awt.headless", "true");

        String basedir = ".";
        String pathIn = args[0];
        // The .spc path is automatically passed in from waf, but it's currently not used since we derive the path from the material
        // String pathSpc = args[1];
        String pathOut = args[2];
        String shaderName = null;
        File fileIn = new File(pathIn);
        File fileOut = new File(pathOut);

        if (args.length >= 4) {
            shaderName = args[3];
        }

        if (args.length >= 5) {
            basedir = args[4];
        }

        try (Reader reader = new BufferedReader(new FileReader(pathIn));
             OutputStream output = new BufferedOutputStream(new FileOutputStream(pathOut))) {

            MaterialDesc.Builder materialBuilder = MaterialDesc.newBuilder();
            TextFormat.merge(reader, materialBuilder);

            if (shaderName == null) {
                shaderName = getShaderName(materialBuilder, ".spc");
            }

            // Construct the project-relative path based from the input material file
            Path basedirAbsolutePath = Paths.get(basedir).toAbsolutePath();
            Path shaderProgramProjectPath = Paths.get(fileIn.getAbsolutePath().replace(fileIn.getName(), shaderName));
            Path shaderProgramRelativePath = basedirAbsolutePath.relativize(shaderProgramProjectPath);
            Path shaderProgramBuildPath = Paths.get(fileOut.getAbsolutePath()).getParent().resolve(shaderName);
            String shaderProgramProjectStr = "/" + shaderProgramRelativePath.toString().replace("\\", "/");

            materialBuilder.setProgram(shaderProgramProjectStr);

            ShaderDesc.Builder shaderBuilder = getShaderBuilderFromResource(shaderProgramBuildPath.toString());

            MaterialDesc materialDesc = finalizeMaterial(materialBuilder, shaderBuilder);
            materialDesc.writeTo(output);
        }
    }
}
