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
import com.google.protobuf.ByteString;

import com.dynamo.graphics.proto.Graphics.ShaderDesc;

public abstract class ShaderProgramBuilder extends Builder<Void> {


    @Override
    public void build(Task<Void> task) throws IOException, CompileExceptionError {
        IResource in = task.getInputs().get(0);
        try (ByteArrayInputStream is = new ByteArrayInputStream(in.getContent())) {
            boolean isDebug = (project.hasOption("debug") || (project.option("variant", Bob.VARIANT_RELEASE) != Bob.VARIANT_RELEASE));
            
            ShaderDesc shaderDesc = compile(is, in, task.getOutputs().get(0).getPath(), project.getPlatformStrings()[0], isDebug);
            task.output(0).setContent(shaderDesc.toByteArray());
        }
    }

    private static ShaderDesc.Shader.Builder tranformGLSL(ByteArrayInputStream is, IResource resource, String resourceOutput, String platform, boolean isDebug)  throws IOException, CompileExceptionError {
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
        // Write extra directives
        switch(FilenameUtils.getExtension(resourceOutput)) {
            case "vpc":
                writer.println("#ifndef GL_ES");
                writer.println("#define lowp");
                writer.println("#define mediump");
                writer.println("#define highp");
                writer.println("#endif");
                break;
            case "fpc":
                writer.println("#ifdef GL_ES");
                writer.println("precision mediump float;");
                writer.println("#endif");
                writer.println("#ifndef GL_ES");
                writer.println("#define lowp");
                writer.println("#define mediump");
                writer.println("#define highp");
                writer.println("#endif");
                break;
        }
        writer.println();

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

    static private void checkResult(Result r, IResource resource, String resourceOutput) throws CompileExceptionError {
        if (r.ret != 0 ) {
            String[] noTFN = new String(r.stdOutErr).split(":", 2);
            String message = noTFN[0];
            if(noTFN.length != 1) {
                message = noTFN[1];
            }
            if(resource != null) {
                throw new CompileExceptionError(resource, 0, message);
            } else {
                throw new CompileExceptionError(resourceOutput + ":" + message, null);
            }
        }
    	
    }

    private static void compileMSL(ShaderDesc.Shader.Builder builder, File spirvFile, String spirvShaderStage, IResource resource, String resourceOutput, String platform, boolean isDebug)  throws IOException, CompileExceptionError {

        File file_out_msl = File.createTempFile(FilenameUtils.getName(resourceOutput), ".msl");
        file_out_msl.deleteOnExit();

        Result result = Exec.execResult(Bob.getExe(Platform.getHostPlatform(), "spirv-cross"),
        		spirvFile.getAbsolutePath(),
        		"--output", file_out_msl.getAbsolutePath(),
                "--msl",
                "--stage", spirvShaderStage
                );
        checkResult(result, resource, resourceOutput);
        
        byte[] msl_data = FileUtils.readFileToByteArray(file_out_msl);
        builder.setSource(ByteString.copyFrom(msl_data));
    }
    
    private static ShaderDesc.Shader.Builder compileSPIRV(ByteArrayInputStream is, ShaderDesc.Language targetLanguage, IResource resource, String resourceOutput, String platform, boolean isDebug)  throws IOException, CompileExceptionError {
        InputStreamReader isr = new InputStreamReader(is);

        // Convert to ES3 (or GL 140+)
        CharBuffer source = CharBuffer.allocate(is.available());
        isr.read(source);
        source.flip();
        ES2ToES3Converter.ShaderType shaderType = FilenameUtils.getExtension(resourceOutput).equals("vpc") ? ES2ToES3Converter.ShaderType.VERTEX_SHADER : ES2ToES3Converter.ShaderType.FRAGMENT_SHADER;
        String spirvShaderStage = (shaderType == ES2ToES3Converter.ShaderType.VERTEX_SHADER ? "vert" : "frag");
        
        // TODO: Add optional glslangValidator step here to ensure shader is ES2 or Desktop X compliant?
        
        ES2ToES3Converter.Result es3Result = ES2ToES3Converter.transform(source.toString(), shaderType, platform.contains("x86") ? "" : "es");

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
        
        Result result = Exec.execResult(Bob.getExe(Platform.getHostPlatform(), "glslc"),
                "-w",
                "-fauto-bind-uniforms",
                "-fauto-map-locations",
                "-std=" + es3Result.shaderVersion + es3Result.shaderProfile,
                "-fshader-stage=" + spirvShaderStage,
                "-o", file_out_spv.getAbsolutePath(),
                file_in_glsl.getAbsolutePath()
        		);
        checkResult(result, resource, resourceOutput);

        // Transpile to target language or output SPIRV 
        ShaderDesc.Shader.Builder builder = ShaderDesc.Shader.newBuilder();
        builder.setLanguage(targetLanguage);
        switch(targetLanguage) {
			case LANGUAGE_MSL:
			    compileMSL(builder, file_out_spv, spirvShaderStage, resource, resourceOutput, platform, isDebug);
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
        
        return builder;
    }        
        
    public static ShaderDesc compile(ByteArrayInputStream is, IResource resource, String resourceOutput, String platform, boolean isDebug) throws IOException, CompileExceptionError {
        ShaderDesc.Builder shaderDescBuilder = ShaderDesc.newBuilder();

        // Always build GLSL (for now)
        {
        	ShaderDesc.Shader.Builder builder = tranformGLSL(is, resource, resourceOutput, platform, isDebug);
            shaderDescBuilder.addShaders(builder);
            is.reset();
        }

        // Build platform specific shader targets (e.g SPIRV, MSL, ..)
        Platform platformKey = Platform.get(platform);
        if(platformKey != null) {
            ShaderDesc.Shader.Builder builder = null; 
         	switch(platformKey) {
        		case X86_64Darwin:
        		case Arm64Darwin:
                    builder = compileSPIRV(is, ShaderDesc.Language.LANGUAGE_MSL, resource, resourceOutput, platform, isDebug);
     			break;

     			default:
     				// Disabled for now..
                    // builder = ShaderDesc.Shader.Builder builder = compileSPIRV(is, ShaderDesc.Language.LANGUAGE_SPIRV, resource, resourceOutput, platform, isDebug);
                break;
         	}
         	if(builder != null) {
                shaderDescBuilder.addShaders(builder);
         	}
        }

        return shaderDescBuilder.build();
    }

    public static void main(String[] args) throws IOException, CompileExceptionError {
        System.setProperty("java.awt.headless", "true");

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

            ShaderDesc shaderDesc = compile(bais, null, args[1], cmd.getOptionValue("platform", ""), cmd.getOptionValue("variant", "").equals("debug") ? true : false);
            shaderDesc.writeTo(os);
            os.close();
        }
    }

}
