// Copyright 2020-2024 The Defold Foundation
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
import java.util.List;

import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.BuilderUtil;
import com.dynamo.bob.pipeline.ProtoUtil;
import com.dynamo.proto.DdfExtensions;
import com.google.protobuf.DescriptorProtos;
import com.google.protobuf.Descriptors;
import com.google.protobuf.GeneratedMessageV3;
import com.google.protobuf.MessageOrBuilder;

/**
 * Abstract builder class. Extend this class to create a builder
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
    protected Task<T> defaultTask(IResource input) throws CompileExceptionError, IOException {
        Task.TaskBuilder<T> taskBuilder = Task.<T>newBuilder(this)
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
    protected Task<?> createSubTask(IResource input, Class<? extends Builder<?>> builderClass, Task.TaskBuilder<?> builder) throws CompileExceptionError {
        Task<?> subTask = project.createTask(input, builderClass);
        builder.addInputsFromOutputs(subTask);
        return subTask;
    }

    /**
     * Create a task whose outputs will be added to the current task's inputs.
     * @param input input resource
     * @param builder current task builder
     * @return new subtask with single input/output
     */
    protected Task<?> createSubTask(IResource input, Task.TaskBuilder<?> builder) throws CompileExceptionError {
        Task<?> subTask = project.createTask(input);
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
    protected Task<?> createSubTask(String inputPath, String field, Task.TaskBuilder<?> builder) throws CompileExceptionError {
        IResource res = BuilderUtil.checkResource(project, builder.firstInput(), field, inputPath);
        Task<?> subTask = project.createTask(res);
        builder.addInputsFromOutputs(subTask);
        return subTask;
    }

    /**
     * Scan proto message and create a sub-task for each resource in it
     * @param builder message or builder of the file that should be scanned
     * @param taskBuilder the builder where result should be applied to
     */
    protected void createSubTasks(MessageOrBuilder builder, Task.TaskBuilder<Void> taskBuilder) throws CompileExceptionError {
        List<Descriptors.FieldDescriptor> fields = builder.getDescriptorForType().getFields();
        for (Descriptors.FieldDescriptor fieldDescriptor : fields) {
            DescriptorProtos.FieldOptions options = fieldDescriptor.getOptions();
            Descriptors.FieldDescriptor resourceDesc = DdfExtensions.resource.getDescriptor();
            boolean isResource = (Boolean) options.getField(resourceDesc);
            Object value = builder.getField(fieldDescriptor);
            if (value instanceof List) {
                @SuppressWarnings("unchecked")
                List<Object> list = (List<Object>) value;
                for (Object v : list) {
                    if (isResource && v instanceof String) {
                        createSubTask((String) v, fieldDescriptor.getName(), taskBuilder);
                    } else if (v instanceof MessageOrBuilder) {
                        createSubTasks((MessageOrBuilder) v, taskBuilder);
                    }
                }
            } else if (isResource && value instanceof String) {
                boolean isOptional = fieldDescriptor.isOptional();
                String resValue =  (String) value;
                // We don't require optional fields to be filled
                // if such a field has no value - just ignore it
                if (isOptional && resValue.isEmpty()) {
                    continue;
                }
                createSubTask(resValue, fieldDescriptor.getName(), taskBuilder);
            } else if (value instanceof MessageOrBuilder) {
                createSubTasks((MessageOrBuilder) value, taskBuilder);
            }
        }
    }

    /**
     * Scan proto message and create a sub-task for each resource in it
     * @param input resource that should be scanned
     * @param taskBuilder the builder where result should be applied to
     */
    protected void createSubTasks(IResource input, Task.TaskBuilder<Void> taskBuilder) throws CompileExceptionError, IOException {
        GeneratedMessageV3.Builder builder = ProtoBuilder.newBuilder(params.outExt());
        ProtoUtil.merge(input, builder);
        createSubTasks(builder, taskBuilder);
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