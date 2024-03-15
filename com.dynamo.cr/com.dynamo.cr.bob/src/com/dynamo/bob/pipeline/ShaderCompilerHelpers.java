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

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.File;
import java.io.PrintWriter;

import java.nio.charset.StandardCharsets;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.Collections;
import java.util.Locale;
import java.util.Scanner;
import java.util.regex.Pattern;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;

import com.dynamo.bob.Bob;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Platform;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.ShaderUtil.ES2ToES3Converter;
import com.dynamo.bob.pipeline.ShaderUtil.SPIRVReflector;
import com.dynamo.bob.pipeline.ShaderUtil.Common;
import com.dynamo.bob.pipeline.ShaderProgramBuilder;
import com.dynamo.bob.util.Exec;
import com.dynamo.bob.util.FileUtil;
import com.dynamo.bob.util.Exec.Result;
import com.dynamo.bob.util.MurmurHash;

import com.dynamo.graphics.proto.Graphics.ShaderDesc;
import com.google.protobuf.ByteString;

public class ShaderCompilerHelpers {
	public static String compileGLSL(String shaderSource, ES2ToES3Converter.ShaderType shaderType, ShaderDesc.Language shaderLanguage, boolean isDebug) throws IOException, CompileExceptionError {

        ByteArrayOutputStream os = new ByteArrayOutputStream();
        PrintWriter writer = new PrintWriter(os);

        // Write directives from shader.
        String line = null;
        String firstNonDirectiveLine = null;
        int directiveLineCount = 0;

        Pattern directiveLinePattern = Pattern.compile("^\\s*(#|//).*");
        Scanner scanner = new Scanner(shaderSource);

        while (scanner.hasNextLine()) {
            line = scanner.nextLine();
            if (line.isEmpty() || directiveLinePattern.matcher(line).find()) {
                writer.println(line);
                ++directiveLineCount;
            } else {
                firstNonDirectiveLine = line;
                break;
            }
        }

        if (directiveLineCount != 0) {
            writer.println();
        }

        int version;
        boolean gles3Standard;
        boolean gles;

        if (shaderLanguage == ShaderDesc.Language.LANGUAGE_GLSL_SM430) {
            version       = 430;
            gles          = false;
            gles3Standard = true;
        } else {
            gles = shaderLanguage == ShaderDesc.Language.LANGUAGE_GLES_SM100 ||
                   shaderLanguage == ShaderDesc.Language.LANGUAGE_GLES_SM300;

            gles3Standard = shaderLanguage == ShaderDesc.Language.LANGUAGE_GLSL_SM140 ||
                            shaderLanguage == ShaderDesc.Language.LANGUAGE_GLES_SM300;

            version = shaderLanguage == ShaderDesc.Language.LANGUAGE_GLES_SM300 ? 300 : 140;

            // Write our directives.
            if (shaderLanguage == ShaderDesc.Language.LANGUAGE_GLES_SM100) {
                // Normally, the ES2ToES3Converter would do this
                writer.println("precision mediump float;");
            }

            if (!gles) {
                writer.println("#ifndef GL_ES");
                writer.println("#define lowp");
                writer.println("#define mediump");
                writer.println("#define highp");
                writer.println("#endif");
                writer.println();
            }
        }

        // We want "correct" line numbers from the GLSL compiler.
        //
        // Some Android devices don't like setting #line to something below 1,
        // see JIRA issue: DEF-1786.
        // We still want to have correct line reporting on most devices so
        // only output the "#line N" directive in debug builds.
        if (isDebug) {
            writer.printf(Locale.ROOT, "#line %d", directiveLineCount);
            writer.println();
        }

        // Write the first non-directive line from above.
        if (firstNonDirectiveLine != null) {
            writer.println(firstNonDirectiveLine);
        }

        // Write the remaining lines from the shader.
        while (scanner.hasNextLine()) {
            line = scanner.nextLine();
            writer.println(line);
        }
        scanner.close();
        writer.flush();

        String source = source = os.toString().replace("\r", "");

        if (gles3Standard) {
            ES2ToES3Converter.Result es3Result = ES2ToES3Converter.transform(source, shaderType, gles ? "es" : "", version, false);
            source = es3Result.output;
        }
        return source;
    }

    static public ShaderProgramBuilder.ShaderBuildResult makeShaderBuilderFromGLSLSource(String source, ShaderDesc.Language shaderLanguage) throws IOException {
        ShaderDesc.Shader.Builder builder = ShaderDesc.Shader.newBuilder();
        builder.setLanguage(shaderLanguage);
        builder.setSource(ByteString.copyFrom(source, "UTF-8"));
        return new ShaderProgramBuilder.ShaderBuildResult(builder);
    }

	static public ShaderProgramBuilder.ShaderBuildResult buildGLSL(String source, ES2ToES3Converter.ShaderType shaderType, ShaderDesc.Language shaderLanguage, boolean isDebug)  throws IOException, CompileExceptionError {
        String glslSource = compileGLSL(source, shaderType, shaderLanguage, isDebug);
        return makeShaderBuilderFromGLSLSource(glslSource, shaderLanguage);
    }

    static public String getResultString(Result r)
    {
        if (r.ret != 0 ) {
            String[] tokenizedResult = new String(r.stdOutErr).split(":", 2);
            String message = tokenizedResult[0];
            if(tokenizedResult.length != 1) {
                message = tokenizedResult[1];
            }
            return message;
        }
        return null;
    }

    static public void checkResult(String result_string, IResource resource, String resourceOutput) throws CompileExceptionError {
        if (result_string != null ) {
            if(resource != null) {
                throw new CompileExceptionError(resource, 0, result_string);
            } else {
                throw new CompileExceptionError(resourceOutput + ":" + result_string, null);
            }
        }
    }

    static public SPIRVCompileResult compileGLSLToSPIRV(String shaderSource, ES2ToES3Converter.ShaderType shaderType, String resourceOutput, String targetProfile, boolean isDebug, boolean soft_fail)  throws IOException, CompileExceptionError {
        SPIRVCompileResult res = new SPIRVCompileResult();

        Result result;
        File file_out_spv;

        if (shaderType == ES2ToES3Converter.ShaderType.COMPUTE_SHADER) {

            int version = 430;

            ES2ToES3Converter.Result es3Result = ES2ToES3Converter.transform(shaderSource, shaderType, targetProfile, version, true);

            File file_in_compute = File.createTempFile(FilenameUtils.getName(resourceOutput), ".cp");
            FileUtil.deleteOnExit(file_in_compute);
            FileUtils.writeByteArrayToFile(file_in_compute, es3Result.output.getBytes());

            file_out_spv = File.createTempFile(FilenameUtils.getName(resourceOutput), ".spv");
            FileUtil.deleteOnExit(file_out_spv);

            result = Exec.execResult(Bob.getExe(Platform.getHostPlatform(), "glslc"),
                    "-w",
                    "-fauto-bind-uniforms",
                    "-fauto-map-locations",
                    // JG: Do we need to pass in -std flag?
                    "-fshader-stage=compute",
                    "-o", file_out_spv.getAbsolutePath(),
                    file_in_compute.getAbsolutePath());
        } else {
            int version = 140;
            if(targetProfile.equals("es")) {
                version = 310;
            }

            Common.GLSLShaderInfo shaderInfo = Common.getShaderInfo(shaderSource);
            String shaderVersionStr = null;
            String shaderProfileStr = "";

            // If the shader already has a version, we expect it to be already written in valid GLSL for that version
            if (shaderInfo != null && shaderInfo.version >= version) {
                shaderVersionStr = Integer.toString(shaderInfo.version);
                shaderProfileStr = shaderInfo.profile;
            } else {
                // Convert to ES3 (or GL 140+)
                ES2ToES3Converter.Result es3Result = ES2ToES3Converter.transform(shaderSource, shaderType, targetProfile, version, true);

                // Update version for SPIR-V (GLES >= 310, Core >= 140)
                es3Result.shaderVersion = es3Result.shaderVersion.isEmpty() ? "0" : es3Result.shaderVersion;
                if(es3Result.shaderProfile.equals("es")) {
                    es3Result.shaderVersion = Integer.parseInt(es3Result.shaderVersion) < 310 ? "310" : es3Result.shaderVersion;
                } else {
                    es3Result.shaderVersion = Integer.parseInt(es3Result.shaderVersion) < 140 ? "140" : es3Result.shaderVersion;
                }

                shaderVersionStr = es3Result.shaderVersion;
                shaderProfileStr = es3Result.shaderProfile;
                shaderSource     = es3Result.output;
            }

            // compile GLSL (ES3 or Desktop 140) to SPIR-V
            File file_in_glsl = File.createTempFile(FilenameUtils.getName(resourceOutput), ".glsl");
            FileUtil.deleteOnExit(file_in_glsl);
            FileUtils.writeByteArrayToFile(file_in_glsl, shaderSource.getBytes());

            file_out_spv = File.createTempFile(FilenameUtils.getName(resourceOutput), ".spv");
            FileUtil.deleteOnExit(file_out_spv);

            String spirvShaderStage = (shaderType == ES2ToES3Converter.ShaderType.VERTEX_SHADER ? "vert" : "frag");
            result = Exec.execResult(Bob.getExe(Platform.getHostPlatform(), "glslc"),
                    "-w",
                    "-fauto-bind-uniforms",
                    "-fauto-map-locations",
                    "-std=" + shaderVersionStr + shaderProfileStr,
                    "-fshader-stage=" + spirvShaderStage,
                    "-o", file_out_spv.getAbsolutePath(),
                    file_in_glsl.getAbsolutePath());
        }

        String result_string = getResultString(result);
        if (soft_fail && result_string != null) {
            res.compile_warnings.add("\nCompatability issue: " + result_string);
            return res;
        } else {
            checkResult(result_string, null, resourceOutput);
        }

        File file_out_spv_opt = File.createTempFile(FilenameUtils.getName(resourceOutput), ".spv");
        FileUtil.deleteOnExit(file_out_spv_opt);

        // Run optimization pass
        result = Exec.execResult(Bob.getExe(Platform.getHostPlatform(), "spirv-opt"),
            "-O",
            file_out_spv.getAbsolutePath(),
            "-o", file_out_spv_opt.getAbsolutePath());

        result_string = getResultString(result);
        if (soft_fail && result_string != null) {
            res.compile_warnings.add("\nOptimization pass failed: " + result_string);
            return res;
        } else {
            checkResult(result_string, null, resourceOutput);
        }

        // Generate reflection data
        File file_out_refl = File.createTempFile(FilenameUtils.getName(resourceOutput), ".json");
        FileUtil.deleteOnExit(file_out_refl);

        result = Exec.execResult(Bob.getExe(Platform.getHostPlatform(), "spirv-cross"),
            file_out_spv_opt.getAbsolutePath(),
            "--output",file_out_refl.getAbsolutePath(),
            "--reflect");

        result_string = getResultString(result);
        if (soft_fail && result_string != null) {
            res.compile_warnings.add("\nUnable to get reflection data: " + result_string);
            return res;
        } else {
            checkResult(result_string, null, resourceOutput);
        }

        String result_json             = FileUtils.readFileToString(file_out_refl, StandardCharsets.UTF_8);
        SPIRVReflector reflector       = new SPIRVReflector(result_json);
        ArrayList<String> shaderIssues = new ArrayList<String>();

        // This is a soft-fail mechanism just to notify that the shaders won't work in runtime.
        // At some point we should probably throw a compilation error here so that the build fails.
        if (shaderIssues.size() > 0) {
            res.compile_warnings = shaderIssues;
            return res;
        }

        res.inputs    = reflector.getInputs();
        res.outputs   = reflector.getOutputs();
        res.ubos      = reflector.getUBOs();
        res.ssbos     = reflector.getSsbos();
        res.textures  = reflector.getTextures();
        res.types     = reflector.getTypes();
        res.source    = FileUtils.readFileToByteArray(file_out_spv);

        Collections.sort(res.inputs, new SortBindingsComparator());
        Collections.sort(res.outputs, new SortBindingsComparator());
        Collections.sort(res.ubos, new SortBindingsComparator());
        Collections.sort(res.ssbos, new SortBindingsComparator());
        Collections.sort(res.textures, new SortBindingsComparator());

        return res;
    }

    static private class SortBindingsComparator implements Comparator<SPIRVReflector.Resource> {
        public int compare(SPIRVReflector.Resource a, SPIRVReflector.Resource b) {
            return a.binding - b.binding;
        }
    }

    static public class SPIRVCompileResult
    {
        public byte[] source;
        public ArrayList<String> compile_warnings           = new ArrayList<String>();
        public ArrayList<SPIRVReflector.Resource> inputs    = new ArrayList<SPIRVReflector.Resource>();
        public ArrayList<SPIRVReflector.Resource> outputs   = new ArrayList<SPIRVReflector.Resource>();
        public ArrayList<SPIRVReflector.Resource> ubos      = new ArrayList<SPIRVReflector.Resource>();
        public ArrayList<SPIRVReflector.Resource> ssbos     = new ArrayList<SPIRVReflector.Resource>();
        public ArrayList<SPIRVReflector.Resource> textures  = new ArrayList<SPIRVReflector.Resource>();
        public ArrayList<SPIRVReflector.ResourceType> types = new ArrayList<SPIRVReflector.ResourceType>();
    };

    static private int getTypeIndex(ArrayList<SPIRVReflector.ResourceType> types, String typeName) {
        for (int i=0; i < types.size(); i++) {
            if (types.get(i).key.equals(typeName)) {
                return i;
            }
        }
        return -1;
    }

    static private ShaderDesc.ResourceType.Builder getResourceTypeBuilder(ArrayList<SPIRVReflector.ResourceType> types, String typeName) {
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

    static private ShaderDesc.ResourceBinding.Builder SPIRVResourceToResourceBindingBuilder(ArrayList<SPIRVReflector.ResourceType> types, SPIRVReflector.Resource res) {
        ShaderDesc.ResourceBinding.Builder resourceBindingBuilder = ShaderDesc.ResourceBinding.newBuilder();
        ShaderDesc.ResourceType.Builder typeBuilder = getResourceTypeBuilder(types, res.type);
        resourceBindingBuilder.setType(typeBuilder);
        resourceBindingBuilder.setName(res.name);
        resourceBindingBuilder.setNameHash(MurmurHash.hash64(res.name));
        resourceBindingBuilder.setSet(res.set);
        resourceBindingBuilder.setBinding(res.binding);
        resourceBindingBuilder.setBlockSize(res.blockSize);
        return resourceBindingBuilder;
    }

    static public ShaderProgramBuilder.ShaderBuildResult buildSpirvFromGLSL(String source, ES2ToES3Converter.ShaderType shaderType, String resourceOutputPath, String targetProfile, boolean isDebug, boolean soft_fail)  throws IOException, CompileExceptionError {
        source = Common.stripComments(source);
        SPIRVCompileResult compile_res = compileGLSLToSPIRV(source, shaderType, resourceOutputPath, targetProfile, isDebug, soft_fail);

        if (compile_res.compile_warnings.size() > 0)
        {
            return new ShaderProgramBuilder.ShaderBuildResult(compile_res.compile_warnings);
        }

        ShaderDesc.Shader.Builder builder = ShaderDesc.Shader.newBuilder();
        builder.setLanguage(ShaderDesc.Language.LANGUAGE_SPIRV);
        builder.setSource(ByteString.copyFrom(compile_res.source));

        for (SPIRVReflector.Resource input : compile_res.inputs) {
            ShaderDesc.ResourceBinding.Builder resourceBindingBuilder = SPIRVResourceToResourceBindingBuilder(compile_res.types, input);
            builder.addInputs(resourceBindingBuilder);
        }

        for (SPIRVReflector.Resource output : compile_res.outputs) {
            ShaderDesc.ResourceBinding.Builder resourceBindingBuilder = SPIRVResourceToResourceBindingBuilder(compile_res.types, output);
            builder.addOutputs(resourceBindingBuilder);
        }

        for (SPIRVReflector.Resource ubo : compile_res.ubos) {
            ShaderDesc.ResourceBinding.Builder resourceBindingBuilder = SPIRVResourceToResourceBindingBuilder(compile_res.types, ubo);
            builder.addUniformBuffers(resourceBindingBuilder);
        }

        for (SPIRVReflector.Resource ssbo : compile_res.ssbos) {
            ShaderDesc.ResourceBinding.Builder resourceBindingBuilder = SPIRVResourceToResourceBindingBuilder(compile_res.types, ssbo);
            builder.addStorageBuffers(resourceBindingBuilder);
        }

        for (SPIRVReflector.Resource texture : compile_res.textures) {
            ShaderDesc.ResourceBinding.Builder resourceBindingBuilder = SPIRVResourceToResourceBindingBuilder(compile_res.types, texture);
            builder.addTextures(resourceBindingBuilder);
        }

        for (SPIRVReflector.ResourceType type : compile_res.types) {
            ShaderDesc.ResourceTypeInfo.Builder resourceTypeInfoBuilder = ShaderDesc.ResourceTypeInfo.newBuilder();

            resourceTypeInfoBuilder.setName(type.name);
            resourceTypeInfoBuilder.setNameHash(MurmurHash.hash64(type.name));

            for (SPIRVReflector.ResourceMember member : type.members) {
                ShaderDesc.ResourceMember.Builder typeMemberBuilder = ShaderDesc.ResourceMember.newBuilder();

                ShaderDesc.ResourceType.Builder typeBuilder = getResourceTypeBuilder(compile_res.types, member.type);
                typeMemberBuilder.setType(typeBuilder);
                typeMemberBuilder.setName(member.name);
                typeMemberBuilder.setNameHash(MurmurHash.hash64(member.name));
                typeMemberBuilder.setElementCount(member.elementCount);

                resourceTypeInfoBuilder.addMembers(typeMemberBuilder);
            }

            builder.addTypes(resourceTypeInfoBuilder);
        }

        return new ShaderProgramBuilder.ShaderBuildResult(builder);
    }
}
