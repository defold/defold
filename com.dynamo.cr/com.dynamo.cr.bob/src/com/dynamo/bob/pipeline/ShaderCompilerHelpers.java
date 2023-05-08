// Copyright 2020-2022 The Defold Foundation
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

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.File;
import java.io.PrintWriter;

import java.nio.CharBuffer;
import java.nio.charset.StandardCharsets;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.Locale;
import java.util.Scanner;
import java.util.regex.Pattern;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;

import com.dynamo.bob.Bob;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Platform;
import com.dynamo.bob.bundle.BundlerParams;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.IShaderCompiler;
import com.dynamo.bob.pipeline.ShaderUtil.ES2ToES3Converter;
import com.dynamo.bob.pipeline.ShaderUtil.SPIRVReflector;
import com.dynamo.bob.pipeline.ShaderUtil.Common;
import com.dynamo.bob.pipeline.ShaderProgramBuilder;
import com.dynamo.bob.util.Exec;
import com.dynamo.bob.util.Exec.Result;

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

        boolean gles =  shaderLanguage == ShaderDesc.Language.LANGUAGE_GLES_SM100 ||
                        shaderLanguage == ShaderDesc.Language.LANGUAGE_GLES_SM300;

        boolean gles3Standard = shaderLanguage == ShaderDesc.Language.LANGUAGE_GLSL_SM140 ||
                                shaderLanguage == ShaderDesc.Language.LANGUAGE_GLES_SM300;

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
            int version = shaderLanguage == ShaderDesc.Language.LANGUAGE_GLES_SM300 ? 300 : 140;
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

    static private ShaderDesc.ShaderDataType stringTypeToShaderType(String typeAsString)
    {
        switch(typeAsString)
        {
            case "int"         : return ShaderDesc.ShaderDataType.SHADER_TYPE_INT;
            case "uint"        : return ShaderDesc.ShaderDataType.SHADER_TYPE_UINT;
            case "float"       : return ShaderDesc.ShaderDataType.SHADER_TYPE_FLOAT;
            case "vec2"        : return ShaderDesc.ShaderDataType.SHADER_TYPE_VEC2;
            case "vec3"        : return ShaderDesc.ShaderDataType.SHADER_TYPE_VEC3;
            case "vec4"        : return ShaderDesc.ShaderDataType.SHADER_TYPE_VEC4;
            case "mat2"        : return ShaderDesc.ShaderDataType.SHADER_TYPE_MAT2;
            case "mat3"        : return ShaderDesc.ShaderDataType.SHADER_TYPE_MAT3;
            case "mat4"        : return ShaderDesc.ShaderDataType.SHADER_TYPE_MAT4;
            case "sampler2D"   : return ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER2D;
            case "sampler3D"   : return ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER3D;
            case "samplerCube" : return ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER_CUBE;
            default: break;
        }

        return ShaderDesc.ShaderDataType.SHADER_TYPE_UNKNOWN;
    }

    static public SPIRVCompileResult compileGLSLToSPIRV(String shaderSource, ES2ToES3Converter.ShaderType shaderType, String resourceOutput, String targetProfile, boolean isDebug, boolean soft_fail)  throws IOException, CompileExceptionError {
        SPIRVCompileResult res = new SPIRVCompileResult();

        Result result;
        File file_out_spv;

        if (shaderType == ES2ToES3Converter.ShaderType.COMPUTE_SHADER) {

            File file_in_compute = File.createTempFile(FilenameUtils.getName(resourceOutput), ".compute");
            file_in_compute.deleteOnExit();
            FileUtils.writeByteArrayToFile(file_in_compute, shaderSource.getBytes());

            file_out_spv = File.createTempFile(FilenameUtils.getName(resourceOutput), ".spv");
            file_out_spv.deleteOnExit();

            result = Exec.execResult(Bob.getExe(Platform.getHostPlatform(), "glslc"),
                    "-w",
                    "-fauto-bind-uniforms",
                    "-fauto-map-locations",
                    // "-std=" + es3Result.shaderVersion + es3Result.shaderProfile, // NOT SURE HERE
                    "-fshader-stage=compute",
                    "-o", file_out_spv.getAbsolutePath(),
                    file_in_compute.getAbsolutePath());
        } else {
            int version = 140;
            if(targetProfile.equals("es")) {
                version = 310;
            }

            // Convert to ES3 (or GL 140+)
            ES2ToES3Converter.Result es3Result = ES2ToES3Converter.transform(shaderSource, shaderType, targetProfile, version, true);

            // Update version for SPIR-V (GLES >= 310, Core >= 140)
            es3Result.shaderVersion = es3Result.shaderVersion.isEmpty() ? "0" : es3Result.shaderVersion;
            if(es3Result.shaderProfile.equals("es")) {
                es3Result.shaderVersion = Integer.parseInt(es3Result.shaderVersion) < 310 ? "310" : es3Result.shaderVersion;
            } else {
                es3Result.shaderVersion = Integer.parseInt(es3Result.shaderVersion) < 140 ? "140" : es3Result.shaderVersion;
            }

            // compile GLSL (ES3 or Desktop 140) to SPIR-V
            File file_in_glsl = File.createTempFile(FilenameUtils.getName(resourceOutput), ".glsl");
            file_in_glsl.deleteOnExit();
            FileUtils.writeByteArrayToFile(file_in_glsl, es3Result.output.getBytes());

            file_out_spv = File.createTempFile(FilenameUtils.getName(resourceOutput), ".spv");
            file_out_spv.deleteOnExit();

            String spirvShaderStage = (shaderType == ES2ToES3Converter.ShaderType.VERTEX_SHADER ? "vert" : "frag");
            result = Exec.execResult(Bob.getExe(Platform.getHostPlatform(), "glslc"),
                    "-w",
                    "-fauto-bind-uniforms",
                    "-fauto-map-locations",
                    "-std=" + es3Result.shaderVersion + es3Result.shaderProfile,
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
        file_out_spv_opt.deleteOnExit();

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
        file_out_refl.deleteOnExit();

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

        // Put all shader resources on a separate list that will be sorted by binding number later
        ArrayList<SPIRVReflector.Resource> resource_list = new ArrayList();

        // Generate a mapping of Uniform Set -> List of Bindings for that set
        // so that we can check for duplicate bindings.
        //
        // Set (0) |- Binding(0) |- U1
        //         |_ Binding(1) |- U2
        //                       |_ U3
        //
        // Set (1) |- Binding(0) |_ U4
        //                       |_ U5
        //
        HashMap<Integer,SetEntry> setBindingMap = new HashMap<Integer,SetEntry>();
        for (SPIRVReflector.UniformBlock ubo : reflector.getUniformBlocks()) {

            // We only support a 1-1 mapping between uniform blocks and uniforms.
            // I.e uniform blocks can't have more (or less) than one uniform in them.
            if (ubo.uniforms.size() > 1) {
                shaderIssues.add("More than one uniforms in uniform block '" + ubo.name + "'");
            } else if (ubo.uniforms.size() == 0) {
                shaderIssues.add("No uniforms found in uniform block '" + ubo.name + "'");
            } else {
                SetEntry setEntry = setBindingMap.get(ubo.set);
                if (setEntry == null) {
                    setEntry = new SetEntry();
                    setBindingMap.put(ubo.set, setEntry);
                }

                BindingEntry bindingEntry = setEntry.get(ubo.binding);
                if (bindingEntry == null) {
                    bindingEntry = new BindingEntry();
                    setEntry.put(ubo.binding, bindingEntry);
                }

                SPIRVReflector.Resource firstUniform = ubo.uniforms.get(0);
                SPIRVReflector.Resource uniform = new SPIRVReflector.Resource();
                uniform.name         = firstUniform.name;
                uniform.type         = firstUniform.type;
                uniform.elementCount = firstUniform.elementCount;
                uniform.binding      = ubo.binding;
                uniform.set          = ubo.set;
                bindingEntry.add(uniform);

                ShaderDesc.ShaderDataType type = Common.stringTypeToShaderType(uniform.type);

                int issue_count = shaderIssues.size();
                if (type == ShaderDesc.ShaderDataType.SHADER_TYPE_UNKNOWN) {
                    shaderIssues.add("Unsupported type for uniform '" + uniform.name + "'");
                }

                if (uniform.set > 1) {
                    shaderIssues.add("Unsupported set value for uniform '" + uniform.name + "', expected <= 1 but found " + uniform.set);
                }

                if (issue_count == shaderIssues.size()) {
                    resource_list.add(uniform);
                }
            }
        }

        for (SPIRVReflector.Resource img : reflector.getImages()) {
            SetEntry setEntry = setBindingMap.get(img.set);
            if (setEntry == null) {
                setEntry = new SetEntry();
                setBindingMap.put(img.set, setEntry);
            }

            BindingEntry bindingEntry = setEntry.get(img.binding);
            if (bindingEntry == null) {
                bindingEntry = new BindingEntry();
                setEntry.put(img.binding, bindingEntry);
            }

            resource_list.add(img);

            bindingEntry.add(img);
        }

        for (SPIRVReflector.Resource tex : reflector.getTextures()) {
            SetEntry setEntry = setBindingMap.get(tex.set);
            if (setEntry == null) {
                setEntry = new SetEntry();
                setBindingMap.put(tex.set, setEntry);
            }

            BindingEntry bindingEntry = setEntry.get(tex.binding);
            if (bindingEntry == null) {
                bindingEntry = new BindingEntry();
                setEntry.put(tex.binding, bindingEntry);
            }

            ShaderDesc.ShaderDataType type = Common.stringTypeToShaderType(tex.type);

            if (!Common.isShaderTypeTexture(type)) {
                shaderIssues.add("Unsupported type '" + tex.type + "'for texture sampler '" + tex.name + "'");
            } else {
                resource_list.add(tex);
            }

            bindingEntry.add(tex);
        }

        for (Map.Entry<Integer, SetEntry> sets : setBindingMap.entrySet()) {
            Integer setIndex = sets.getKey();
            SetEntry setEntry = sets.getValue();
            for (Map.Entry<Integer, BindingEntry> bindings : setEntry.entrySet()) {
                Integer bindingIndex = bindings.getKey();
                BindingEntry bindingEntry = bindings.getValue();
                if (bindingEntry.size() > 1) {
                    String duplicateList = "";
                    for (int i = 0; i < bindingEntry.size(); i++) {
                        duplicateList += bindingEntry.get(i).name;

                        if (i != bindingEntry.size() - 1) {
                            duplicateList += ", ";
                        }
                    }
                    shaderIssues.add("Uniforms '" + duplicateList + "' from set " + setIndex + " have the same binding " + bindingIndex);
                }
            }
        }

        // This is a soft-fail mechanism just to notify that the shaders won't work in runtime.
        // At some point we should probably throw a compilation error here so that the build fails.
        if (shaderIssues.size() > 0) {
            res.compile_warnings = shaderIssues;
            return res;
        }

        // Build vertex attributes (inputs)
        if (shaderType == ES2ToES3Converter.ShaderType.VERTEX_SHADER) {
            ArrayList<SPIRVReflector.Resource> attributes = reflector.getInputs();
            Collections.sort(attributes, new SortBindingsComparator());
            res.attributes = attributes;
        }

        // Sort shader resources by increasing binding number
        Collections.sort(resource_list, new SortBindingsComparator());

        res.resource_list = resource_list;
        res.source        = FileUtils.readFileToByteArray(file_out_spv);

        return res;
    }

    static private class BindingEntry extends ArrayList<SPIRVReflector.Resource> {}
    static private class SetEntry extends HashMap<Integer,BindingEntry> {}
    static private class SortBindingsComparator implements Comparator<SPIRVReflector.Resource> {
        public int compare(SPIRVReflector.Resource a, SPIRVReflector.Resource b) {
            return a.binding - b.binding;
        }
    }

    static public class SPIRVCompileResult
    {
        public byte[] source;
        public ArrayList<String> compile_warnings = new ArrayList<String>();
        public ArrayList<SPIRVReflector.Resource> attributes = new ArrayList<SPIRVReflector.Resource>();
        public ArrayList<SPIRVReflector.Resource> resource_list = new ArrayList<SPIRVReflector.Resource>();
    };

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

        // Note: No need to check for duplicates for vertex attributes,
        // the SPIR-V compiler will complain about it.
        for (SPIRVReflector.Resource input : compile_res.attributes) {
            ShaderDesc.ResourceBinding.Builder resourceBindingBuilder = ShaderDesc.ResourceBinding.newBuilder();
            resourceBindingBuilder.setName(input.name);
            resourceBindingBuilder.setType(Common.stringTypeToShaderType(input.type));
            resourceBindingBuilder.setSet(input.set);
            resourceBindingBuilder.setBinding(input.binding);
            builder.addAttributes(resourceBindingBuilder);
        }

        // Build uniforms
        for (SPIRVReflector.Resource res : compile_res.resource_list) {
            ShaderDesc.ResourceBinding.Builder resourceBindingBuilder = ShaderDesc.ResourceBinding.newBuilder();
            resourceBindingBuilder.setName(res.name);
            resourceBindingBuilder.setType(Common.stringTypeToShaderType(res.type));
            resourceBindingBuilder.setElementCount(res.elementCount);
            resourceBindingBuilder.setSet(res.set);
            resourceBindingBuilder.setBinding(res.binding);
            resourceBindingBuilder.setImageFormat(Common.stringFormatToTextureFormat(res.format));
            builder.addUniforms(resourceBindingBuilder);
        }

        return new ShaderProgramBuilder.ShaderBuildResult(builder);
    }
}
