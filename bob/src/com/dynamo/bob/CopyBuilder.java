package com.dynamo.bob;

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
    public void build(Task<Void> task) {
        IResource in = task.getInputs().get(0);
        IResource out = task.getOutputs().get(0);
        out.setContent(in.getContent());
    }
}
