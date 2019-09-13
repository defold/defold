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
import com.dynamo.bob.util.MurmurHash;
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

    abstract void writeExtraDirectives(PrintWriter writer);

    private ShaderDesc.Shader.Builder tranformGLSL(ByteArrayInputStream is, IResource resource, String resourceOutput, String platform, boolean isDebug)  throws IOException, CompileExceptionError {
        InputStreamReader isr = new InputStreamReader(is);
        BufferedReader reader = new BufferedReader(isr);
        ByteArrayOutputStream os = new ByteArrayOutputStream(is.available() + 128 * 16);
        PrintWriter writer = new PrintWriter(os);

        // Write directives from shader.
        String line = null;
        String firstNonDirectiveLine = null;
        int directiveLineCount = 0;

        Pattern directiveLinePattern = Pattern.compile("^\\s*(#|//).*");
        while ((line = reader.readLine()) != null) {
            if (line.isEmpty() || directiveLinePattern.matcher(line).find()) {
                writer.println(line);
                ++directiveLineCount;
            } else {
                firstNonDirectiveLine = line;
                break;
            }
        }

        // Write our directives.
        if (directiveLineCount != 0) {
            writer.println();
        }

        writeExtraDirectives(writer);

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
        while ((line = reader.readLine()) != null) {
            writer.println(line);
        }
        writer.flush();

        // Always write the glsl source (for now)
        ShaderDesc.Shader.Builder builder = ShaderDesc.Shader.newBuilder();
        builder.setLanguage(ShaderDesc.Language.LANGUAGE_GLSL);
        builder.setSource(ByteString.copyFrom(os.toString(), "UTF-8"));

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

    private ShaderDesc.Shader.Builder compileGLSLToSPIRV(ByteArrayInputStream is, ES2ToES3Converter.ShaderType shaderType, IResource resource, String resourceOutput, String targetProfile, boolean isDebug, boolean soft_fail)  throws IOException, CompileExceptionError {
        InputStreamReader isr = new InputStreamReader(is);

        // Convert to ES3 (or GL 140+)
        CharBuffer source = CharBuffer.allocate(is.available());
        isr.read(source);
        source.flip();

        // TODO: Add optional glslangValidator step here to ensure shader is ES2 or Desktop X compliant?

        ES2ToES3Converter.Result es3Result = ES2ToES3Converter.transform(source.toString(), shaderType, targetProfile);

        // TODO: Add optional glslangValidator step here to ensure shader is ES3 or Desktop X compliant?

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
            System.err.println("\nWarning! Compatability issue: " + result_string);
            return null;
        } else {
            checkResult(result_string, resource, resourceOutput);
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
            System.err.println("\nWarning! Unable to get reflection data: " + result_string);
            return null;
        } else {
            checkResult(result_string, resource, resourceOutput);
        }

        String result_json = FileUtils.readFileToString(file_out_refl, StandardCharsets.UTF_8);
        SPIRVReflector reflector = new SPIRVReflector(result_json);

        // A map of Uniform Set -> List of Bindings for that set
        HashMap<Integer,ArrayList<Integer>> setBindingMap = new HashMap<Integer,ArrayList<Integer>>();

        ShaderDesc.Shader.Builder builder = ShaderDesc.Shader.newBuilder();
        int shaderIssues = 0;

        // Process all uniform blocks
        // NOTE: Currently, we only support a 1-1 mapping between blocks and uniforms, since this is
        // what the shader elevation does (due to simplifiation). While a custom GLSL3+ shader could have
        // uniform blocks that contain several uniforms, we don't have that setup in our shader output
        // or in any of the graphics systems, so they will fail here.
        for (SPIRVReflector.UniformBlock ubo : reflector.getUniformBlocks()) {
            ArrayList<Integer> bindingsEntry = setBindingMap.get(ubo.set);

            if (bindingsEntry == null) {
                bindingsEntry = new ArrayList<Integer>();
                setBindingMap.put(ubo.set, bindingsEntry);
            }

            boolean notExactlyOneUniformIssue = ubo.uniforms.size() != 1;
            boolean bindingsExistIssuse       = bindingsEntry.contains(ubo.binding);

            if (notExactlyOneUniformIssue || bindingsExistIssuse) {
                if (bindingsExistIssuse) {
                    System.err.println("\nDuplicate binding found for uniform '" + ubo.uniforms.get(0).name + "'");
                    shaderIssues++;
                }

                if (notExactlyOneUniformIssue) {
                    System.err.println("\nNot exactly one uniform in uniform block '" + ubo.name + "'");
                    shaderIssues++;
                }
            } else {
                SPIRVReflector.Resource uniform = ubo.uniforms.get(0);
                ShaderDesc.ResourceBinding.Builder resourceBindingBuilder = ShaderDesc.ResourceBinding.newBuilder();
                resourceBindingBuilder.setName(MurmurHash.hash64(uniform.name));
                resourceBindingBuilder.setType(stringTypeToShaderType(uniform.type));
                resourceBindingBuilder.setSet(uniform.set);
                resourceBindingBuilder.setBinding(uniform.binding);
                builder.addUniforms(resourceBindingBuilder);
            }

            bindingsEntry.add(ubo.binding);
        }

        // Process all texture entries
        for (SPIRVReflector.Resource tex : reflector.getTextures()) {
            ArrayList<Integer> bindingsEntry = setBindingMap.get(tex.set);
            ShaderDesc.ShaderDataType type = stringTypeToShaderType(tex.type);

            if (bindingsEntry == null) {
                bindingsEntry = new ArrayList<Integer>();
                setBindingMap.put(tex.set, bindingsEntry);
            }

            boolean bindingsExistIssuse  = bindingsEntry.contains(tex.binding);
            boolean unsupportedTypeIssue = type != ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER2D;

            if (bindingsExistIssuse || unsupportedTypeIssue) {
                if (bindingsExistIssuse) {
                    System.err.println("\nDuplicate binding found for texture sampler '" + tex.name + "'");
                    shaderIssues++;
                }

                if (unsupportedTypeIssue) {
                    System.err.println("\nUnsupported type for texture sampler '" + tex.name + "'");
                    shaderIssues++;
                }
            } else {
                ShaderDesc.ResourceBinding.Builder resourceBindingBuilder = ShaderDesc.ResourceBinding.newBuilder();
                resourceBindingBuilder.setName(MurmurHash.hash64(tex.name));
                resourceBindingBuilder.setType(type);
                resourceBindingBuilder.setSet(tex.set);
                resourceBindingBuilder.setBinding(tex.binding);
                builder.addUniforms(resourceBindingBuilder);
            }

            bindingsEntry.add(tex.binding);
        }

        // Process all vertex attributes (inputs)
        if (shaderType == ES2ToES3Converter.ShaderType.VERTEX_SHADER){
            ArrayList<Integer> locationsList = new ArrayList<Integer>();
            for (SPIRVReflector.Resource input : reflector.getInputs()) {
                if (locationsList.contains(input.binding)) {
                    System.err.println("\nDuplicate input attributes found for input '" + input.name + "'");
                    shaderIssues++;
                } else {
                    ShaderDesc.ResourceBinding.Builder resourceBindingBuilder = ShaderDesc.ResourceBinding.newBuilder();
                    resourceBindingBuilder.setName(MurmurHash.hash64(input.name));
                    resourceBindingBuilder.setType(stringTypeToShaderType(input.type));
                    resourceBindingBuilder.setSet(input.set);
                    resourceBindingBuilder.setBinding(input.binding);
                    builder.addAttributes(resourceBindingBuilder);
                }

                locationsList.add(input.binding);
            }
        }

        // This is a soft-fail mechanism just to notify that the shaders won't work in runtime.
        if (shaderIssues != 0) {
            String resourcePath = resourceOutput;

            if (resource != null)
                resourcePath = resource.getPath();

            System.err.println("\nWarning! Found " + shaderIssues + " issues when compiling '" + resourcePath + "' to SPIR-V.");
            return null;
        }

        builder.setLanguage(ShaderDesc.Language.LANGUAGE_SPIRV);
        byte[] spv_data = FileUtils.readFileToByteArray(file_out_spv);
        builder.setSource(ByteString.copyFrom(spv_data));

        return builder;
    }

    public ShaderDesc compile(ByteArrayInputStream is, ES2ToES3Converter.ShaderType shaderType, IResource resource, String resourceOutput, String platform, boolean isDebug, boolean soft_fail) throws IOException, CompileExceptionError {
        ShaderDesc.Builder shaderDescBuilder = ShaderDesc.newBuilder();

        // Build platform specific shader targets (e.g SPIRV, MSL, ..)
        Platform platformKey = Platform.get(platform);
        if(platformKey != null) {
            switch(platformKey) {
                case X86Darwin:
                case X86_64Darwin:
                {
                    shaderDescBuilder.addShaders(tranformGLSL(is, resource, resourceOutput, platform, isDebug));
                    is.reset();
                    ShaderDesc.Shader.Builder builder = compileGLSLToSPIRV(is, shaderType, resource, resourceOutput, "", isDebug, soft_fail);
                    if (builder != null)
                    {
                        shaderDescBuilder.addShaders(builder);
                    }
                }
                break;

                case X86Win32:
                case X86_64Win32:
                {
                    shaderDescBuilder.addShaders(tranformGLSL(is, resource, resourceOutput, platform, isDebug));
                    is.reset();
                    ShaderDesc.Shader.Builder builder = compileGLSLToSPIRV(is, shaderType, resource, resourceOutput, "", isDebug, soft_fail);
                    if (builder != null)
                    {
                        shaderDescBuilder.addShaders(builder);
                    }
                }
                break;

                case X86Linux:
                case X86_64Linux:
                {
                    shaderDescBuilder.addShaders(tranformGLSL(is, resource, resourceOutput, platform, isDebug));
                    is.reset();
                    ShaderDesc.Shader.Builder builder = compileGLSLToSPIRV(is, shaderType, resource, resourceOutput, "", isDebug, soft_fail);
                    if (builder != null)
                    {
                        shaderDescBuilder.addShaders(builder);
                    }
                }
                break;

                case Armv7Darwin:
                case Arm64Darwin:
                case X86_64Ios:
                {
                    shaderDescBuilder.addShaders(tranformGLSL(is, resource, resourceOutput, platform, isDebug));
                    is.reset();
                    ShaderDesc.Shader.Builder builder = compileGLSLToSPIRV(is, shaderType, resource, resourceOutput, "es", isDebug, soft_fail);
                    if (builder != null)
                    {
                        shaderDescBuilder.addShaders(builder);
                    }
                }
                break;

                case Armv7Android:
                case Arm64Android:
                    shaderDescBuilder.addShaders(tranformGLSL(is, resource, resourceOutput, platform, isDebug));
                    is.reset();
                    ShaderDesc.Shader.Builder builder = compileGLSLToSPIRV(is, shaderType, resource, resourceOutput, "", isDebug, soft_fail);
                    if (builder != null)
                    {
                        shaderDescBuilder.addShaders(builder);
                    }
                break;

                case JsWeb:
                case WasmWeb:
                    shaderDescBuilder.addShaders(tranformGLSL(is, resource, resourceOutput, platform, isDebug));
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

           ShaderDesc shaderDesc = compile(bais, shaderType, null, args[1], cmd.getOptionValue("platform", ""), cmd.getOptionValue("variant", "").equals("debug") ? true : false, false);
           shaderDesc.writeTo(os);
           os.close();
       }
    }

}
