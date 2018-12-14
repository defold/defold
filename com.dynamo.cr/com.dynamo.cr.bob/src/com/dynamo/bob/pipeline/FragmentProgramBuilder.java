package com.dynamo.bob.pipeline;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.PrintWriter;

import com.dynamo.bob.Bob;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.ShaderUtil.ES2ToES3Converter;
import com.dynamo.graphics.proto.Graphics.ShaderDesc;

@BuilderParams(name = "FragmentProgram", inExts = ".fp", outExt = ".fpc")
public class FragmentProgramBuilder extends ShaderProgramBuilder {

    private static final ES2ToES3Converter.ShaderType SHADER_TYPE = ES2ToES3Converter.ShaderType.FRAGMENT_SHADER;

    @Override
    public void build(Task<Void> task) throws IOException, CompileExceptionError {
        IResource in = task.getInputs().get(0);
        try (ByteArrayInputStream is = new ByteArrayInputStream(in.getContent())) {
            boolean isDebug = (project.hasOption("debug") || (project.option("variant", Bob.VARIANT_RELEASE) != Bob.VARIANT_RELEASE));

            ShaderDesc shaderDesc = compile(is, SHADER_TYPE, in, task.getOutputs().get(0).getPath(), project.getPlatformStrings()[0], isDebug);
            task.output(0).setContent(shaderDesc.toByteArray());
        }
    }

    public void writeExtraDirectives(PrintWriter writer)
    {
        writer.println("#ifdef GL_ES");
        writer.println("precision mediump float;");
        writer.println("#endif");
        writer.println("#ifndef GL_ES");
        writer.println("#define lowp");
        writer.println("#define mediump");
        writer.println("#define highp");
        writer.println("#endif");
        writer.println();
    }

    public static void main(String[] args) throws IOException, CompileExceptionError {
        System.setProperty("java.awt.headless", "true");
        FragmentProgramBuilder builder = new FragmentProgramBuilder();
        builder.BuildShader(args, SHADER_TYPE);
    }

}
