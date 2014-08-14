package com.dynamo.bob;

import java.io.IOException;

import com.dynamo.bob.fs.IResource;

/**
 * Copy builder. This class is abstract. Inherit from this class
 * and add appropriate {@link BuilderParams}
 * @author Christian Murray
 *
 */
public abstract class CopyBuilder extends Builder<Void> {

    @Override
    public Task<Void> create(IResource input) {
        Task<Void> task = Task.<Void>newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addOutput(input.changeExt(params.outExt()))
                .build();
        return task;
    }

    @Override
    public void build(Task<Void> task) throws IOException {
        IResource in = task.getInputs().get(0);
        IResource out = task.getOutputs().get(0);
        out.setContent(in.getContent());
    }
}
