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

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.nio.CharBuffer;
import java.nio.charset.StandardCharsets;
import java.util.Locale;
import java.util.regex.Pattern;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;
import java.util.Collections;
import java.util.Comparator;
import java.util.Scanner;

import org.apache.commons.cli.CommandLine;
import org.apache.commons.cli.CommandLineParser;
import org.apache.commons.cli.Options;
import org.apache.commons.cli.ParseException;
import org.apache.commons.cli.PosixParser;
import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;

import com.dynamo.bob.Bob;
import com.dynamo.bob.Builder;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Platform;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.ShaderUtil.ES2ToES3Converter;
import com.dynamo.bob.pipeline.ShaderUtil.SPIRVReflector;
import com.dynamo.bob.util.Exec;
import com.dynamo.bob.util.Exec.Result;
import com.dynamo.graphics.proto.Graphics.ShaderDesc;
import com.google.protobuf.ByteString;

public abstract class ShaderProgramBuilder extends Builder<Void> {

    @Override
    public Task<Void> create(IResource input) throws IOException, CompileExceptionError {

        Task.TaskBuilder<Void> taskBuilder = Task.<Void>newBuilder(this)
            .setName(params.name())
            .addInput(input);
        taskBuilder.addOutput(input.changeExt(params.outExt()));

        return taskBuilder.build();
    }

    static public String compileGLSL(String shaderSource, ES2ToES3Converter.ShaderType shaderType, ShaderDesc.Language shaderLanguage,
                            String resourceOutput, boolean isDebug)  throws IOException, CompileExceptionError {

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

    private ShaderDesc.Shader.Builder buildGLSL(ByteArrayInputStream is, ES2ToES3Converter.ShaderType shaderType, ShaderDesc.Language shaderLanguage,
                                IResource resource, String resourceOutput, boolean isDebug)  throws IOException, CompileExceptionError {
        ShaderDesc.Shader.Builder builder = ShaderDesc.Shader.newBuilder();
        builder.setLanguage(shaderLanguage);

        int n = is.available();
        byte[] bytes = new byte[n];
        is.read(bytes, 0, n);
        String source = new String(bytes, StandardCharsets.UTF_8);

        String transformedSource = compileGLSL(source, shaderType, shaderLanguage, resourceOutput, isDebug);

        builder.setSource(ByteString.copyFrom(transformedSource, "UTF-8"));
        return builder;
    }

    static private String getResultString(Result r)
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

    static private void checkResult(String result_string, IResource resource, String resourceOutput) throws CompileExceptionError {
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

    static private boolean isShaderTypeTexture(ShaderDesc.ShaderDataType data_type)
    {
        return data_type == ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER_CUBE ||
               data_type == ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER2D ||
               data_type == ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER3D;
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

    static public SPIRVCompileResult compileGLSLToSPIRV(String shaderSource, ES2ToES3Converter.ShaderType shaderType, String resourceOutput, String targetProfile, boolean isDebug, boolean soft_fail)  throws IOException, CompileExceptionError {
        SPIRVCompileResult res = new SPIRVCompileResult();

        int version = 140;
        if(targetProfile.equals("es"))
            version = 310;

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

        File file_out_spv = File.createTempFile(FilenameUtils.getName(resourceOutput), ".spv");
        file_out_spv.deleteOnExit();

        String spirvShaderStage = (shaderType == ES2ToES3Converter.ShaderType.VERTEX_SHADER ? "vert" : "frag");

        Result result = Exec.execResult(Bob.getExe(Platform.getHostPlatform(), "glslc"),
                "-w",
                "-fauto-bind-uniforms",
                "-fauto-map-locations",
                "-std=" + es3Result.shaderVersion + es3Result.shaderProfile,
                "-fshader-stage=" + spirvShaderStage,
                "-o", file_out_spv.getAbsolutePath(),
                file_in_glsl.getAbsolutePath()
                );

        String result_string = getResultString(result);
        if (soft_fail && result_string != null) {
            res.compile_warnings.add("\nCompatability issue: " + result_string);
            return res;
        } else {
            checkResult(result_string, null, resourceOutput);
        }

        // Generate reflection data
        File file_out_refl = File.createTempFile(FilenameUtils.getName(resourceOutput), ".json");
        file_out_refl.deleteOnExit();

        result = Exec.execResult(Bob.getExe(Platform.getHostPlatform(), "spirv-cross"),
            file_out_spv.getAbsolutePath(),
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

                ShaderDesc.ShaderDataType type = stringTypeToShaderType(uniform.type);

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

            ShaderDesc.ShaderDataType type = stringTypeToShaderType(tex.type);
            if (!isShaderTypeTexture(type)) {
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

    static private ShaderDesc.Shader.Builder buildSpirvFromGLSL(ByteArrayInputStream is, ES2ToES3Converter.ShaderType shaderType, IResource resource, String resourceOutput, String targetProfile, boolean isDebug, boolean soft_fail)  throws IOException, CompileExceptionError {
        InputStreamReader isr = new InputStreamReader(is);
        CharBuffer source = CharBuffer.allocate(is.available());
        isr.read(source);
        source.flip();

        SPIRVCompileResult compile_res = compileGLSLToSPIRV(source.toString(), shaderType, resourceOutput, targetProfile, isDebug, soft_fail);

        if (compile_res.compile_warnings.size() > 0)
        {
            String resourcePath = resourceOutput;

            if (resource != null)
            {
                resourcePath = resource.getPath();
            }

            System.err.println("\nWarning! Found " + compile_res.compile_warnings.size() + " issues when compiling '" + resourcePath + "' to SPIR-V:");
            for (String issueStr : compile_res.compile_warnings) {
                System.err.println("  " + issueStr);
            }

            return null;
        }

        ShaderDesc.Shader.Builder builder = ShaderDesc.Shader.newBuilder();
        builder.setLanguage(ShaderDesc.Language.LANGUAGE_SPIRV);
        builder.setSource(ByteString.copyFrom(compile_res.source));

        // Note: No need to check for duplicates for vertex attributes,
        // the SPIR-V compiler will complain about it.
        for (SPIRVReflector.Resource input : compile_res.attributes) {
            ShaderDesc.ResourceBinding.Builder resourceBindingBuilder = ShaderDesc.ResourceBinding.newBuilder();
            resourceBindingBuilder.setName(input.name);
            resourceBindingBuilder.setType(stringTypeToShaderType(input.type));
            resourceBindingBuilder.setSet(input.set);
            resourceBindingBuilder.setBinding(input.binding);
            builder.addAttributes(resourceBindingBuilder);
        }

        // Build uniforms
        for (SPIRVReflector.Resource res : compile_res.resource_list) {
            ShaderDesc.ResourceBinding.Builder resourceBindingBuilder = ShaderDesc.ResourceBinding.newBuilder();
            resourceBindingBuilder.setName(res.name);
            resourceBindingBuilder.setType(stringTypeToShaderType(res.type));
            resourceBindingBuilder.setElementCount(res.elementCount);
            resourceBindingBuilder.setSet(res.set);
            resourceBindingBuilder.setBinding(res.binding);
            builder.addUniforms(resourceBindingBuilder);
        }

        return builder;
    }

    public ShaderDesc compile(ByteArrayInputStream is, ES2ToES3Converter.ShaderType shaderType, IResource resource, String resourceOutput, String platform, boolean isDebug, boolean outputSpirv, boolean soft_fail) throws IOException, CompileExceptionError {
        ShaderDesc.Builder shaderDescBuilder = ShaderDesc.newBuilder();

        // Build platform specific shader targets (e.g SPIRV, MSL, ..)
        Platform platformKey = Platform.get(platform);
        if(platformKey != null) {
            switch(platformKey) {
                case X86_64MacOS:
                {
                    shaderDescBuilder.addShaders(buildGLSL(is, shaderType, ShaderDesc.Language.LANGUAGE_GLSL_SM140, resource, resourceOutput, isDebug));
                    is.reset();

                    if (outputSpirv)
                    {
                        ShaderDesc.Shader.Builder builder = buildSpirvFromGLSL(is, shaderType, resource, resourceOutput, "", isDebug, soft_fail);
                        if (builder != null)
                        {
                            shaderDescBuilder.addShaders(builder);
                        }
                    }
                }
                break;

                case X86Win32:
                case X86_64Win32:
                {
                    shaderDescBuilder.addShaders(buildGLSL(is, shaderType, ShaderDesc.Language.LANGUAGE_GLSL_SM140, resource, resourceOutput, isDebug));
                    is.reset();

                    if (outputSpirv)
                    {
                        ShaderDesc.Shader.Builder builder = buildSpirvFromGLSL(is, shaderType, resource, resourceOutput, "", isDebug, soft_fail);
                        if (builder != null)
                        {
                            shaderDescBuilder.addShaders(builder);
                        }
                    }
                }
                break;

                case X86Linux:
                case X86_64Linux:
                {
                    shaderDescBuilder.addShaders(buildGLSL(is, shaderType, ShaderDesc.Language.LANGUAGE_GLSL_SM140, resource, resourceOutput, isDebug));
                    is.reset();
                    if (outputSpirv)
                    {
                        ShaderDesc.Shader.Builder builder = buildSpirvFromGLSL(is, shaderType, resource, resourceOutput, "", isDebug, soft_fail);
                        if (builder != null)
                        {
                            shaderDescBuilder.addShaders(builder);
                        }
                    }
                }
                break;

                case Arm64Ios:
                case X86_64Ios:
                {
                    shaderDescBuilder.addShaders(buildGLSL(is, shaderType, ShaderDesc.Language.LANGUAGE_GLES_SM300, resource, resourceOutput, isDebug));
                    is.reset();
                    if (outputSpirv)
                    {
                        ShaderDesc.Shader.Builder builder = buildSpirvFromGLSL(is, shaderType, resource, resourceOutput, "es", isDebug, soft_fail);
                        if (builder != null)
                        {
                            shaderDescBuilder.addShaders(builder);
                        }
                    }
                }
                break;

                case Armv7Android:
                case Arm64Android:
                {
                    shaderDescBuilder.addShaders(buildGLSL(is, shaderType, ShaderDesc.Language.LANGUAGE_GLES_SM300, resource, resourceOutput, isDebug));
                    is.reset();
                    shaderDescBuilder.addShaders(buildGLSL(is, shaderType, ShaderDesc.Language.LANGUAGE_GLES_SM100, resource, resourceOutput, isDebug));
                    is.reset();
                    if (outputSpirv)
                    {
                        ShaderDesc.Shader.Builder builder = buildSpirvFromGLSL(is, shaderType, resource, resourceOutput, "", isDebug, soft_fail);
                        if (builder != null)
                        {
                            shaderDescBuilder.addShaders(builder);
                        }
                    }
                }
                break;

                case JsWeb:
                case WasmWeb:
                    shaderDescBuilder.addShaders(buildGLSL(is, shaderType, ShaderDesc.Language.LANGUAGE_GLES_SM300, resource, resourceOutput, isDebug));
                    is.reset();
                    shaderDescBuilder.addShaders(buildGLSL(is, shaderType, ShaderDesc.Language.LANGUAGE_GLES_SM100, resource, resourceOutput, isDebug));
                    is.reset();
                break;

                case Arm64NX64:
                {
                    shaderDescBuilder.addShaders(buildGLSL(is, shaderType, ShaderDesc.Language.LANGUAGE_GLSL_SM140, resource, resourceOutput, isDebug));
                    is.reset();
                    ShaderDesc.Shader.Builder builder = buildSpirvFromGLSL(is, shaderType, resource, resourceOutput, "", isDebug, soft_fail);
                    if (builder != null)
                    {
                        shaderDescBuilder.addShaders(builder);
                    }
                }
                break;

                default:
                    System.err.println("Unsupported platform for shader program builder: " + platformKey);
                break;
            }
        }
        else
        {
            System.err.println("Unknown platform for shader program builder: " + platform);
        }

        return shaderDescBuilder.build();
    }

    public void BuildShader(String[] args, ES2ToES3Converter.ShaderType shaderType) throws IOException, CompileExceptionError {
        try (BufferedInputStream is = new BufferedInputStream(new FileInputStream(args[0]));
            BufferedOutputStream os = new BufferedOutputStream(new FileOutputStream(args[1]))) {

           Options options = new Options();
           options.addOption("p", "platform", true, "Platform");
           options.addOption(null, "variant", true, "Specify debug or release");
           CommandLineParser parser = new PosixParser();
           CommandLine cmd = null;
           try {
               cmd = parser.parse(options, args);
           } catch (ParseException e) {
               System.err.println(e.getMessage());
               System.exit(5);
           }

           byte[] inBytes = new byte[is.available()];
           is.read(inBytes);
           ByteArrayInputStream bais = new ByteArrayInputStream(inBytes);

           boolean outputSpirv = true;
           ShaderDesc shaderDesc = compile(bais, shaderType, null, args[1], cmd.getOptionValue("platform", ""), cmd.getOptionValue("variant", "").equals("debug") ? true : false, outputSpirv, false);
           shaderDesc.writeTo(os);
           os.close();
       }
    }

}
