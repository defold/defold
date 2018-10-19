package com.dynamo.bob.pipeline;

import java.io.IOException;


import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.ShaderProgramBuilder;

@BuilderParams(name = "FragmentProgram", inExts = ".fp", outExt = ".fpc")
public class FragmentProgramBuilder extends ShaderProgramBuilder {

    public Task<Void> create(IResource input) throws IOException, CompileExceptionError {

        Task.TaskBuilder<Void> taskBuilder = Task.<Void>newBuilder(this)
            .setName(params.name())
            .addInput(input);
        taskBuilder.addOutput(input.changeExt(params.outExt()));

        return taskBuilder.build();
    }

    public static void main(String[] args) throws IOException, CompileExceptionError {
        ShaderProgramBuilder.main(args);
    }

}
