// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
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
import java.security.NoSuchAlgorithmException;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;

import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.BuilderUtil;

/**
 * Abstract builder class. Extend this class to create a builder
 * @author Christian Murray
 */
public abstract class Builder {

    private static Map<Class<?>,  byte[]> classToParamsDigest = new HashMap<Class<?>,  byte[]>();

    public static void addParamsDigest(Class<?> klass, Map<String, String> options, BuilderParams builderParams) throws NoSuchAlgorithmException {
        String[] params = builderParams.paramsForSignature();
        if (params.length == 0) {
            return;
        }
        MessageDigest digest = MessageDigest.getInstance("SHA1");
        Arrays.sort(params);
        for (String param: params) {
            if (options.containsKey(param)) {
                digest.update(param.getBytes());
                String value = options.get(param);
                if (value != null && !value.isEmpty()) {
                    digest.update(value.getBytes());
                }
            }
        }
        classToParamsDigest.put(klass, digest.digest());
    }

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
    protected Task defaultTask(IResource input) {
        Task.TaskBuilder taskBuilder = Task.newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addOutput(input.changeExt(params.outExt()));
        return taskBuilder.build();
    }

    /**
     * Create a task whose outputs will be added to the current task's inputs.
     * @param input input resource
     * @param builderClass class to build resource with
     * @param builder current task builder
     * @return new subtask with single input/output
     */
    protected Task createSubTask(IResource input, Class<? extends Builder> builderClass, Task.TaskBuilder builder) throws CompileExceptionError {
        Task subTask = project.createTask(input, builderClass);
        builder.addInputsFromOutputs(subTask);
        return subTask;
    }

    /**
     * Create a task whose outputs will be added to the current task's inputs.
     * @param input input resource
     * @param builder current task builder
     * @return new subtask with single input/output
     */
    protected Task createSubTask(IResource input, Task.TaskBuilder builder) throws CompileExceptionError {
        Task subTask = project.createTask(input);
        if (subTask == null) {
            throw new CompileExceptionError(input,
                    0,
                    String.format("Failed to create build task for '%s'", input.getPath()));
        }
        builder.addInputsFromOutputs(subTask);
        return subTask;
    }

    /**
     * Create a task whose outputs will be added to the current task's inputs.
     * @param inputPath input path to resource
     * @param field where specified path to the resource
     * @param builder current task builder
     * @return new subtask with single input/output
     */
    protected Task createSubTask(String inputPath, String field, Task.TaskBuilder builder) throws CompileExceptionError {
        IResource res = BuilderUtil.checkResource(project, builder.firstInput(), field, inputPath);
        Task subTask = project.createTask(res);
        builder.addInputsFromOutputs(subTask);
        return subTask;
    }

    /**
     * Create task from input
     * @param input input resource
     * @return new task
     * @throws IOException
     * @throws CompileExceptionError
     */
    public abstract Task create(IResource input) throws IOException, CompileExceptionError;

    /**
     * Build task, ie compile
     * @param task task to build
     * @throws CompileExceptionError
     * @throws IOException
     */
    public abstract void build(Task task) throws CompileExceptionError, IOException;

    /**
     * Add custom signature, eg command-line, etc
     * @param digest message digest to update
     */
    public void signature(MessageDigest digest) {
        byte[] paramsDigest = classToParamsDigest.get(this.getClass());
        if (paramsDigest != null) {
            digest.update(paramsDigest);
        }
    }

    /**
     * Set project
     * @param project project to set
     */
    public void setProject(Project project) {
        this.project = project;
    }

    public boolean isGameProjectBuilder() {
        return false;
    }

    public void clearState() {
    }
}