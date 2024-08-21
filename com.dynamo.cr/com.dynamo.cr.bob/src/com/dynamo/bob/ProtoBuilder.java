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

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.lang.reflect.Method;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.ProtoUtil;
import com.dynamo.proto.DdfExtensions;
import com.google.protobuf.DescriptorProtos;
import com.google.protobuf.Descriptors;
import com.google.protobuf.GeneratedMessageV3;
import com.google.protobuf.MessageOrBuilder;
import com.google.protobuf.Message;

public abstract class ProtoBuilder<B extends GeneratedMessageV3.Builder<B>> extends Builder<Void> {

    private ProtoParams protoParams;
    private HashMap<IResource, B> messageBuilders = new HashMap<>();

    private static Map<String, Class<? extends GeneratedMessageV3>> extToMessageClass = new HashMap<String, Class<? extends GeneratedMessageV3>>();

    public ProtoBuilder() {
        protoParams = getClass().getAnnotation(ProtoParams.class);

        BuilderParams builderParams = getClass().getAnnotation(BuilderParams.class);
        extToMessageClass.put(builderParams.outExt(), protoParams.messageClass());
    }

    static public void addMessageClass(String ext, Class<? extends GeneratedMessageV3> klass) {
        extToMessageClass.put(ext, klass);
    }

    static public Class<? extends GeneratedMessageV3> getMessageClassFromExt(String ext) {
        return extToMessageClass.get(ext);
    }

    static public boolean supportsType(String ext) {
        Class<? extends GeneratedMessageV3> klass = getMessageClassFromExt(ext);
        return klass != null;
    }

    static public GeneratedMessageV3.Builder<?> newBuilder(String ext) throws CompileExceptionError {
        Class<? extends GeneratedMessageV3> klass = getMessageClassFromExt(ext);
        if (klass != null) {
            GeneratedMessageV3.Builder<?> builder;
            try {
                Method newBuilder = klass.getDeclaredMethod("newBuilder");
                return (GeneratedMessageV3.Builder<?>) newBuilder.invoke(null);
            } catch(Exception e) {
                throw new RuntimeException(e);
            }
        } else {
            throw new CompileExceptionError(null, -1, "No proto message class mapping for " + ext);
        }
    }

    protected B transform(Task<Void> task, IResource resource, B messageBuilder) throws IOException, CompileExceptionError {
        return messageBuilder;
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

    protected B getMessageBuilder(IResource input) throws IOException, CompileExceptionError {
        B messageBuilder = messageBuilders.get(input);
        if (messageBuilder != null) {
            return messageBuilder;
        }
        try {
            Method newBuilder = protoParams.messageClass().getDeclaredMethod("newBuilder");
            messageBuilder = (B) newBuilder.invoke(null);
        } catch (Exception e) {
            throw new RuntimeException(e);
        }

        ProtoUtil.merge(input, messageBuilder);
        messageBuilders.put(input, messageBuilder);
        return messageBuilder;
    }

    @Override
    public Task<Void> create(IResource input) throws IOException, CompileExceptionError {
        Task.TaskBuilder<Void> taskBuilder = Task.<Void>newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addOutput(input.changeExt(params.outExt()));
        createSubTasks(getMessageBuilder(input), taskBuilder);
        return taskBuilder.build();
    }

    @SuppressWarnings("unchecked")
    @Override
    public void build(Task<Void> task) throws CompileExceptionError,
            IOException {

        B builder = getMessageBuilder(task.firstInput());
        builder = transform(task, task.firstInput(), builder);

        Message msg = builder.build();
        ByteArrayOutputStream out = new ByteArrayOutputStream(4 * 1024);
        msg.writeTo(out);
        out.close();
        task.output(0).setContent(out.toByteArray());
    }

}
