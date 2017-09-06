package com.dynamo.bob.pipeline;

import java.io.PrintWriter;

import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.pipeline.ShaderProgramBuilder;

@BuilderParams(name = "FragmentProgram", inExts = ".fp", outExt = ".fpc")
public class FragmentProgramBuilder extends ShaderProgramBuilder {

    @Override
    public void writeExtraDirectives(PrintWriter writer) {
        writer.println("#ifdef GL_ES");
        writer.println("precision mediump float;");
        writer.println("#endif");
        writer.println("#ifndef GL_ES");
        writer.println("#define lowp");
        writer.println("#define mediump");
        writer.println("#define highp");
        writer.println("#endif");
    }

}
