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
import java.util.HashSet;
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
    private static final String[] PBR_COMMON_MEMBERS = {
        "pbrAlphaCutoffAndDoubleSidedAndIsUnlit",
        "pbrCommonTextures",
        "pbrEmissiveFactorAndStrength",
        "pbrNormalScaleAndOcclusionStrength",
    };

    public static enum PbrParameterType
    {
        PBR_PARAMETER_TYPE_METALLIC_ROUGHNESS("PbrMetallicRoughness", "baseColorFactor", "metallicAndRoughnessFactor", "metallicRoughnessTextures"),
        PBR_PARAMETER_TYPE_SPECULAR_GLOSSINESS("PbrSpecularGlossiness", "diffuseFactor", "specularAndSpecularGlossinessFactor", "specularGlossinessTextures"),
        PBR_PARAMETER_TYPE_CLEARCOAT("PbrClearCoat", "clearCoatAndClearCoatRoughnessFactor", "clearCoatTextures"),
        PBR_PARAMETER_TYPE_TRANSMISSION("PbrTransmission", "transmissionFactor", "transmissionTextures"),
        PBR_PARAMETER_TYPE_IOR("PbrIor", "ior"),
        PBR_PARAMETER_TYPE_SPECULAR("PbrSpecular", "specularColorAndSpecularFactor", "specularTextures"),
        PBR_PARAMETER_TYPE_VOLUME("PbrVolume", "thicknessFactorAndAttenuationColor", "attenuationDistance", "volumeTextures"),
        PBR_PARAMETER_TYPE_SHEEN("PbrSheen", "sheenColorAndRoughnessFactor", "sheenTextures"),
        PBR_PARAMETER_TYPE_EMISSIVE_STRENGTH("PbrEmissiveStrength", "emissiveStrength"),
        PBR_PARAMETER_TYPE_IRIDESCENCE("PbrIridescence", "iridescenceFactorAndIorAndThicknessMinMax", "iridescenceTextures");

        private final String structName;
        private final String memberName;
        private final String[] members;

        PbrParameterType(String structName, String... members) {
            this.structName = structName;
            this.memberName = "p" + structName.substring(1);
            this.members = members;
        }
    }

    private static final HashMap<String, PbrParameterType> pbrStructNameToParameterType = new HashMap<>();
    static {
        for (PbrParameterType type : PbrParameterType.values()) {
            pbrStructNameToParameterType.put(type.structName, type);
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

    private static CompileExceptionError invalidPbrLayout(String message) {
        return new CompileExceptionError("Invalid PbrMaterial uniform layout: " + message);
    }

    private static void validatePbrVec4(ShaderDesc.ResourceMember member, String name, int offset, String owner) throws CompileExceptionError {
        if (!member.getName().equals(name) || member.getType().getUseTypeIndex() ||
                member.getType().getShaderType() != ShaderDesc.ShaderDataType.SHADER_TYPE_VEC4 ||
                member.getElementCount() > 1 || member.getOffset() != offset) {
            throw invalidPbrLayout(owner + " must contain vec4 " + name + " at byte offset " + offset + ".");
        }
    }

    private static ShaderDesc.ResourceTypeInfo getPbrStruct(ShaderDesc.ShaderReflection reflection, ShaderDesc.ResourceType type, String name) throws CompileExceptionError {
        if (!type.getUseTypeIndex() || type.getTypeIndex() < 0 || type.getTypeIndex() >= reflection.getTypesCount()) {
            throw invalidPbrLayout(name + " must be a PBR struct.");
        }
        return reflection.getTypes(type.getTypeIndex());
    }

    private static void validatePbrMaterial(ShaderDesc.ShaderReflection reflection, ShaderDesc.ResourceBinding resource,
            ShaderDesc.ResourceTypeInfo material, MaterialDesc.PbrParameters.Builder parameters) throws CompileExceptionError {
        int commonSize = PBR_COMMON_MEMBERS.length * 16;
        if (material.getMembersCount() < PBR_COMMON_MEMBERS.length || resource.getBlockSize() < commonSize || resource.getElementCount() > 1) {
            throw invalidPbrLayout("PbrMaterial must begin with the four common vec4 fields and cannot be an array.");
        }
        for (int i = 0; i < PBR_COMMON_MEMBERS.length; ++i) {
            validatePbrVec4(material.getMembers(i), PBR_COMMON_MEMBERS[i], i * 16, PBR_MATERIAL_NAME);
        }

        HashSet<PbrParameterType> seen = new HashSet<>();
        ArrayList<int[]> ranges = new ArrayList<>();
        for (int i = PBR_COMMON_MEMBERS.length; i < material.getMembersCount(); ++i) {
            ShaderDesc.ResourceMember member = material.getMembers(i);
            ShaderDesc.ResourceTypeInfo struct = getPbrStruct(reflection, member.getType(), member.getName());
            PbrParameterType parameter = pbrStructNameToParameterType.get(struct.getName());
            if (parameter == null) {
                throw invalidPbrLayout("Unsupported channel " + member.getName() + " of type " + struct.getName() + ".");
            }
            if (!seen.add(parameter)) {
                throw invalidPbrLayout("Duplicate channel " + struct.getName() + ".");
            }
            if (!member.getName().equals(parameter.memberName) || member.getElementCount() > 1) {
                throw invalidPbrLayout("Use a single " + parameter.structName + " " + parameter.memberName + " channel.");
            }
            if (struct.getMembersCount() != parameter.members.length) {
                throw invalidPbrLayout(parameter.structName + " must contain exactly " + parameter.members.length + " vec4 fields.");
            }
            for (int j = 0; j < parameter.members.length; ++j) {
                validatePbrVec4(struct.getMembers(j), parameter.members[j], j * 16, parameter.structName);
            }

            int offset = member.getOffset();
            int size = parameter.members.length * 16;
            if (offset < commonSize || offset % 16 != 0 || offset > resource.getBlockSize() - size) {
                throw invalidPbrLayout(parameter.memberName + " must be aligned to 16 bytes and fit inside the uniform block.");
            }
            for (int[] range : ranges) {
                if (offset < range[1] && offset + size > range[0]) {
                    throw invalidPbrLayout(parameter.memberName + " overlaps another channel.");
                }
            }
            ranges.add(new int[] { offset, offset + size });
            addPbrParameter(parameters, parameter);
        }
    }

    public static MaterialDesc.PbrParameters makePbrParametersProtoMessage(Graphics.ShaderDesc.ShaderReflection shaderReflection) throws CompileExceptionError {
        if (shaderReflection == null) {
            return null;
        }
        MaterialDesc.PbrParameters.Builder parameters = MaterialDesc.PbrParameters.newBuilder();
        for (ShaderDesc.ResourceBinding resource : shaderReflection.getUniformBuffersList()) {
            if (!resource.getType().getUseTypeIndex()) {
                continue;
            }
            ShaderDesc.ResourceTypeInfo material = getPbrStruct(shaderReflection, resource.getType(), resource.getName());
            if (material.getName().equals(PBR_MATERIAL_NAME)) {
                validatePbrMaterial(shaderReflection, resource, material, parameters);
                parameters.setHasParameters(true);
            }
        }
        return parameters.getHasParameters() ? parameters.build() : null;
    }

    private static void buildPbrParameters(MaterialDesc.Builder materialBuilder, ShaderDesc.Builder shaderBuilder) throws CompileExceptionError {
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
