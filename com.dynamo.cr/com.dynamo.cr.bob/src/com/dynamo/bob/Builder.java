package com.dynamo.bob;

import java.io.IOException;
import java.security.MessageDigest;

import com.dynamo.bob.fs.IResource;

/**
 * Abstract builder class. Extent this class to create a builder
 * @author Christian Murray
 *
 * @param <T> currently not used. The idea is to pass data directly. TODO: Remove?
 */
public abstract class Builder<T> {

    protected BuilderParams params;
    protected Project project;
    public Builder() {
        params = getClass().getAnnotation(BuilderParams.class);
    }

    /**
     * Get builder annotation parameters
     * @return parameters
     */
    public BuilderParams getParams() {
        return params;
    }

    /**
     * Create a single input/output task
     * @param input input resource
     * @return new task with single input/output
     */
    protected Task<T> defaultTask(IResource input) {
        Task<T> task = Task.<T>newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addOutput(input.changeExt(params.outExt()))
                .build();
        return task;
    }

    /**
     * Create task from input
     * @param input input resource
     * @return new task
     * @throws IOException
     * @throws CompileExceptionError
     */
    public abstract Task<T> create(IResource input) throws IOException, CompileExceptionError;

    /**
     * Build task, ie compile
     * @param task task to build
     * @throws CompileExceptionError
     * @throws IOException
     */
    public abstract void build(Task<T> task) throws CompileExceptionError, IOException;

    /**
     * Add custom signature, eg command-line, etc
     * @param digest message digest to update
     */
    public void signature(MessageDigest digest) {

    }

    /**
     * Set project
     * @param project project to set
     */
    public void setProject(Project project) {
        this.project = project;
    }

}