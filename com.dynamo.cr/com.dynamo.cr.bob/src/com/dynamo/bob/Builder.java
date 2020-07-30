// Copyright 2020 The Defold Foundation
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
// 
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
// 
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

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