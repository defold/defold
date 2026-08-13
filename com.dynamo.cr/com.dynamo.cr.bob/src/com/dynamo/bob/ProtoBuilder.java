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

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;

import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.ProtoUtil;
import com.dynamo.proto.DdfExtensions;
import com.google.protobuf.DescriptorProtos;
import com.google.protobuf.Descriptors;
import com.google.protobuf.GeneratedMessageV3;
import com.google.protobuf.MessageOrBuilder;
import com.google.protobuf.Message;

public abstract class ProtoBuilder<B extends GeneratedMessageV3.Builder<B>> extends Builder {

    private ProtoParams protoParams;
    private HashMap<IResource, B> srcBuilders = new HashMap<>();
    private HashMap<IResource, Message> srcMessages = new HashMap<>();

    private static Map<String, Class<? extends GeneratedMessageV3>> extToMessageClass = new HashMap<String, Class<? extends GeneratedMessageV3>>();
    private static Map<Class<? extends GeneratedMessageV3>,  byte[]> classToProtoDigest = new HashMap<Class<? extends GeneratedMessageV3>,  byte[]>();

    public ProtoBuilder() {
        protoParams = getClass().getAnnotation(ProtoParams.class);

        BuilderParams builderParams = getClass().getAnnotation(BuilderParams.class);
        extToMessageClass.put(builderParams.outExt(), protoParams.messageClass());
    }

    public static void addProtoDigest(Class<? extends GeneratedMessageV3> klass) throws NoSuchAlgorithmException {
        if (classToProtoDigest.get(klass) == null) {
            MessageDigest digest = MessageDigest.getInstance("SHA1");
            digest.update(klass.getName().getBytes(StandardCharsets.UTF_8));
            try {
                Descriptors.Descriptor descriptor = (Descriptors.Descriptor) klass.getMethod("getDescriptor").invoke(null);
                digest.update(descriptor.getFullName().getBytes(StandardCharsets.UTF_8));

                // Hash complete, deterministic file descriptors instead of a subset of
                // message fields. This includes oneofs, enums, cardinality, custom DDF
                // options, and the transitive schemas used by source-only payloads.
                TreeMap<String, Descriptors.FileDescriptor> files = new TreeMap<>();
                collectFileDescriptors(descriptor.getFile(), files);
                for (Descriptors.FileDescriptor file : files.values()) {
                    digest.update(file.toProto().toByteArray());
                }
            } catch (ReflectiveOperationException e) {
                throw new RuntimeException("Failed to retrieve descriptor from protobuf class", e);
            }
            classToProtoDigest.put(klass, digest.digest());
        }
    }

    private static void collectFileDescriptors(Descriptors.FileDescriptor file,
                                               Map<String, Descriptors.FileDescriptor> files) {
        if (files.putIfAbsent(file.getName(), file) != null) {
            return;
        }
        for (Descriptors.FileDescriptor dependency : file.getDependencies()) {
            collectFileDescriptors(dependency, files);
        }
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

    static public GeneratedMessageV3.Builder newBuilder(String ext) throws CompileExceptionError {
        Class<? extends GeneratedMessageV3> klass = getMessageClassFromExt(ext);
        if (klass != null) {
            GeneratedMessageV3.Builder builder;
            try {
                Method newBuilder = klass.getDeclaredMethod("newBuilder");
                return (GeneratedMessageV3.Builder) newBuilder.invoke(null);
            } catch(Exception e) {
                throw new RuntimeException(e);
            }
        } else {
            throw new CompileExceptionError(null, -1, "No proto message class mapping for " + ext);
        }
    }

    protected B transform(Task task, IResource resource, B messageBuilder) throws IOException, CompileExceptionError {
        return messageBuilder;
    }

    /**
     * Scan proto message and create a sub-task for each resource in it
     * @param builder message or builder of the file that should be scanned
     * @param taskBuilder the builder where result should be applied to
     */
    protected void createSubTasks(MessageOrBuilder builder, Task.TaskBuilder taskBuilder) throws CompileExceptionError {
        List<Descriptors.FieldDescriptor> fields = builder.getDescriptorForType().getFields();
        for (Descriptors.FieldDescriptor fieldDescriptor : fields) {
            if (fieldDescriptor.getContainingOneof() != null && !builder.hasField(fieldDescriptor)) {
                continue;
            }
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

    // This used to parse the main input resource ('firstInput()' or 'input.get(0)') and then reuse it on all the stages.
    protected B getSrcBuilder(IResource input) throws IOException, CompileExceptionError {
        B srcBuilder = srcBuilders.get(input);
        if (srcBuilder != null) {
            return srcBuilder;
        }

        Message srcMessage = srcMessages.get(input);
        if (srcMessage != null) {
            srcBuilder = (B) srcMessage.toBuilder();
            srcBuilders.put(input, srcBuilder);
            return srcBuilder;
        }

        try {
            Method newBuilder = protoParams.srcClass().getDeclaredMethod("newBuilder");
            srcBuilder = (B) newBuilder.invoke(null);
        } catch (Exception e) {
            throw new RuntimeException(e);
        }

        mergeSrc(input, srcBuilder);
        srcBuilders.put(input, srcBuilder);
        return srcBuilder;
    }

    public void setSrcMessage(IResource input, Message srcMessage) throws CompileExceptionError {
        if (!protoParams.srcClass().isInstance(srcMessage)) {
            throw new CompileExceptionError(input, 0,
                    "Injected protobuf source has type '" + srcMessage.getDescriptorForType().getFullName()
                            + "', expected '" + protoParams.srcClass().getName() + "'");
        }

        Message previousMessage = srcMessages.putIfAbsent(input, srcMessage);
        if (previousMessage != null && !previousMessage.equals(srcMessage)) {
            throw new CompileExceptionError(input, 0,
                    "Conflicting injected protobuf sources for generated resource '" + input.getPath() + "'");
        }
    }

    protected void mergeSrc(IResource input, B srcBuilder) throws IOException, CompileExceptionError {
        ProtoUtil.merge(input, srcBuilder);
    }

    @Override
    public Task create(IResource input) throws IOException, CompileExceptionError {
        Task.TaskBuilder taskBuilder = Task.newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addOutput(input.changeExt(params.outExt()));
        createSubTasks(getSrcBuilder(input), taskBuilder);
        return taskBuilder.build();
    }

    @SuppressWarnings("unchecked")
    @Override
    public void build(Task task) throws CompileExceptionError,
            IOException {

        B builder = getSrcBuilder(task.firstInput());
        builder = transform(task, task.firstInput(), builder);

        Message msg = builder.build();
        ByteArrayOutputStream out = new ByteArrayOutputStream(4 * 1024);
        msg.writeTo(out);
        out.close();
        task.output(0).setContent(out.toByteArray());
    }

    @Override
    public void clearState() {
        super.clearState();
        srcBuilders = null;
        srcMessages = null;
    }

    // Update digest with the signature of the output proto format
    @Override
    public void signature(MessageDigest digest) {
        super.signature(digest);
        byte[] protoDigest = classToProtoDigest.get(protoParams.messageClass());
        if (protoDigest != null) {
            digest.update(protoDigest);
        }
        if (protoParams.srcClass() != protoParams.messageClass()) {
            byte[] srcProtoDigest = classToProtoDigest.get(protoParams.srcClass());
            if (srcProtoDigest != null) {
                digest.update(srcProtoDigest);
            }
        }
    }
}
