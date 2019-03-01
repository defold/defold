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
import java.util.Locale;
import java.util.regex.Pattern;
import java.util.Iterator;

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
import com.dynamo.bob.util.Exec;
import com.dynamo.bob.util.Exec.Result;
import com.dynamo.graphics.proto.Graphics.ShaderDesc;
import com.google.protobuf.ByteString;

import org.codehaus.jackson.JsonNode;
import org.codehaus.jackson.map.ObjectMapper;

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

    private void compileMSL(ShaderDesc.Shader.Builder builder, File spirvFile, String spirvShaderStage, IResource resource, String resourceOutput, boolean isDebug, boolean soft_fail)  throws IOException, CompileExceptionError {

        File file_out_msl = File.createTempFile(FilenameUtils.getName(resourceOutput), ".msl");
        file_out_msl.deleteOnExit();

        Result result = Exec.execResult(Bob.getExe(Platform.getHostPlatform(), "spirv-cross"),
                spirvFile.getAbsolutePath(),
                "--output", file_out_msl.getAbsolutePath(),
                "--msl",
                "--stage", spirvShaderStage
                );
        String result_string = getResultString(result);
        if (soft_fail && result_string != null)
        {
            System.err.println("\nWarning! Compatability issue: " + result_string);
            return;
        }
        else
        {
            checkResult(result_string, resource, resourceOutput);
        }

        byte[] msl_data = FileUtils.readFileToByteArray(file_out_msl);
        builder.setSource(ByteString.copyFrom(msl_data));
    }

    private ShaderDesc.Shader.Builder compileGLSLToSPIRV(ByteArrayInputStream is, ES2ToES3Converter.ShaderType shaderType, ShaderDesc.Language targetLanguage, IResource resource, String resourceOutput, String targetProfile, boolean isDebug, boolean soft_fail)  throws IOException, CompileExceptionError {
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

        File file_out_refl = File.createTempFile(FilenameUtils.getName(resourceOutput), ".relf");
        file_out_refl.deleteOnExit();

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
        if (soft_fail && result_string != null)
        {
            System.err.println("\nWarning! Compatability issue: " + result_string);
            return null;
        }
        else
        {
            checkResult(result_string, resource, resourceOutput);
        }

        // Transpile to target language or output SPIRV
        ShaderDesc.Shader.Builder builder = ShaderDesc.Shader.newBuilder();
        builder.setLanguage(targetLanguage);
        switch(targetLanguage) {
			case LANGUAGE_MSL:
			    compileMSL(builder, file_out_spv, spirvShaderStage, resource, resourceOutput, isDebug, soft_fail);
			break;

			case LANGUAGE_SPIRV:
				byte[] spv_data = FileUtils.readFileToByteArray(file_out_spv);
				builder.setBinary(ByteString.copyFrom(spv_data));
			break;

			case LANGUAGE_GLSL:
                throw new CompileExceptionError("GLSL not implemented as SPIRV target language", null);

			default:
                throw new CompileExceptionError("Unknown compiler target languge", null);
        }

        // Get reflection data
        // TODO: error on fail?
        result = Exec.execResult(Bob.getExe(Platform.getHostPlatform(), "spirv-cross"),
            file_out_spv.getAbsolutePath(),
            "--output",file_out_refl.getAbsolutePath(),
            "--reflect");

        ObjectMapper om = new ObjectMapper();
        JsonNode tree = om.readTree(file_out_refl);
        JsonNode uboNode = tree.get("ubos");
        JsonNode texturesNode = tree.get("textures");
        JsonNode typesNode = tree.get("types");

        int uniformIndex = 0;

        if (uboNode != null) {
            Iterator<JsonNode> uniformBlockIt = uboNode.getElements();
            while (uniformBlockIt.hasNext()) {
                JsonNode uniformBlock = uniformBlockIt.next();

                System.out.println("UBO [" + uniformBlock.get("name").asText() + "]");
                System.out.println(" type       : " + uniformBlock.get("type").asText());
                System.out.println(" block_size : " + uniformBlock.get("block_size").asInt());
                System.out.println(" set        : " + uniformBlock.get("set").asInt());
                System.out.println(" binding    : " + uniformBlock.get("binding").asInt());

                ShaderDesc.UniformBlock.Builder uniformBlockBuilder = ShaderDesc.UniformBlock.newBuilder();

                uniformBlockBuilder.setName(uniformBlock.get("name").asText());
                uniformBlockBuilder.setSet(uniformBlock.get("set").asInt());
                uniformBlockBuilder.setBinding(uniformBlock.get("binding").asInt());

                JsonNode typeNode = typesNode.get(uniformBlock.get("type").asText());
                JsonNode membersNode = typeNode.get("members");

                for (Iterator<JsonNode> iter = membersNode.getElements(); iter.hasNext(); uniformIndex++) {
                    JsonNode uniformNode = iter.next();
                    String uniformNodeName = uniformNode.get("name").asText();
                    String uniformNodeType = uniformNode.get("type").asText();
                    int uniformNodeOffset  = uniformNode.get("offset").asInt();

                    System.out.println("  Uniform [" + uniformNodeName + "]");
                    System.out.println("    Type   :" + uniformNodeType);
                    System.out.println("    Offset :" + Integer.toString(uniformNodeOffset));

                    ShaderDesc.Uniform.Builder uniformBuilder = ShaderDesc.Uniform.newBuilder();
                    ShaderDesc.UniformType uniformType = ShaderDesc.UniformType.UNIFORM_TYPE_UNKNOWN;

                    if (uniformNodeType.equals("vec2")) {
                        uniformType = ShaderDesc.UniformType.UNIFORM_TYPE_VEC2;
                    } else if (uniformNodeType.equals("vec3")) {
                        uniformType = ShaderDesc.UniformType.UNIFORM_TYPE_VEC3;
                    } else if (uniformNodeType.equals("vec4")) {
                        uniformType = ShaderDesc.UniformType.UNIFORM_TYPE_VEC4;
                    } else if (uniformNodeType.equals("mat2")) {
                        uniformType = ShaderDesc.UniformType.UNIFORM_TYPE_MAT2;
                    } else if (uniformNodeType.equals("mat3")) {
                        uniformType = ShaderDesc.UniformType.UNIFORM_TYPE_MAT3;
                    } else if (uniformNodeType.equals("mat4")) {
                        uniformType = ShaderDesc.UniformType.UNIFORM_TYPE_MAT4;
                    }

                    uniformBlockBuilder.addUniformIndices(uniformIndex);
                    uniformBuilder.setName(uniformNodeName);
                    uniformBuilder.setType(uniformType);
                    uniformBuilder.setOffset(uniformNodeOffset);
                    builder.addUniforms(uniformBuilder);
                }

                builder.addUniformBlocks(uniformBlockBuilder);
            }
        }

        if (texturesNode != null) {
            for (Iterator<JsonNode> iter = texturesNode.getElements(); iter.hasNext();) {
                JsonNode textureNode   = iter.next();
                String textureNodeType = textureNode.get("type").asText();
                String textureNodeName = textureNode.get("name").asText();
                int textureNodeSet     = textureNode.get("set").asInt();
                int textureNodeBinding = textureNode.get("binding").asInt();

                System.out.println("  Texture [" + textureNodeName + "]");
                System.out.println("    Type    :" + textureNodeType);
                System.out.println("    Set     :" + Integer.toString(textureNodeSet));
                System.out.println("    Binding :" + Integer.toString(textureNodeBinding));

                ShaderDesc.Uniform.Builder uniformBuilder = ShaderDesc.Uniform.newBuilder();
                ShaderDesc.UniformType uniformType = ShaderDesc.UniformType.UNIFORM_TYPE_SAMPLER2D;

                if (!textureNodeType.equals("sampler2D"))
                {
                    throw new CompileExceptionError("Unsupported texture type: " + textureNodeType, null);
                }

                uniformBuilder.setName(textureNodeName);
                uniformBuilder.setType(uniformType);
                uniformBuilder.setSet(textureNodeSet);
                uniformBuilder.setBinding(textureNodeBinding);
                builder.addUniforms(uniformBuilder);
            }
        }

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
                    ShaderDesc.Shader.Builder builder = compileGLSLToSPIRV(is, shaderType, ShaderDesc.Language.LANGUAGE_SPIRV, resource, resourceOutput, "", isDebug, soft_fail);
                    if (builder != null)
                    {
                        shaderDescBuilder.addShaders(builder);
                    }
        		}
                break;

        		case X86Win32:
        		case X86_64Win32:
        			shaderDescBuilder.addShaders(tranformGLSL(is, resource, resourceOutput, platform, isDebug));
        		break;

        		case X86Linux:
        		case X86_64Linux:
        			shaderDescBuilder.addShaders(tranformGLSL(is, resource, resourceOutput, platform, isDebug));
        		break;

        		case Armv7Darwin:
        		case Arm64Darwin:
        		{
        			shaderDescBuilder.addShaders(tranformGLSL(is, resource, resourceOutput, platform, isDebug));
                    is.reset();
                    ShaderDesc.Shader.Builder builder = compileGLSLToSPIRV(is, shaderType, ShaderDesc.Language.LANGUAGE_MSL, resource, resourceOutput, "es", isDebug, soft_fail);
                    if (builder != null)
                    {
                        shaderDescBuilder.addShaders(builder);
                    }
        		}
     			break;

        		case Armv7Android:
        			shaderDescBuilder.addShaders(tranformGLSL(is, resource, resourceOutput, platform, isDebug));
                    is.reset();
                    ShaderDesc.Shader.Builder builder = compileGLSLToSPIRV(is, shaderType, ShaderDesc.Language.LANGUAGE_SPIRV, resource, resourceOutput, "", isDebug, soft_fail);
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
                    System.err.println("Unsupported platform: " + platformKey);
                break;
         	}
        }
        else
        {
            System.err.println("Unknown platform: " + platform);
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
