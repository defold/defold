package com.dynamo.bob.pipeline;

import java.io.PrintWriter;

import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.pipeline.ShaderProgramBuilder;

@BuilderParams(name = "VertexProgram", inExts = ".vp", outExt = ".vpc")
public class VertexProgramBuilder extends ShaderProgramBuilder {

    @Override
    public void writeExtraDirectives(PrintWriter writer) {
        writer.println("#ifndef GL_ES");
        writer.println("#define lowp");
        writer.println("#define mediump");
        writer.println("#define highp");
        writer.println("#endif");
    }

}
