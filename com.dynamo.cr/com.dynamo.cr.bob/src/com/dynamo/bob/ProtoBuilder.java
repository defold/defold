// Copyright 2020-2022 The Defold Foundation
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
import java.util.Map;

import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.ProtoUtil;
import com.google.protobuf.GeneratedMessage;
import com.google.protobuf.Message;

public abstract class ProtoBuilder<B extends GeneratedMessage.Builder<B>> extends Builder<Void> {

    private ProtoParams protoParams;

    private static Map<String, Class<? extends GeneratedMessage>> extToMessageClass = new HashMap<String, Class<? extends GeneratedMessage>>();

    public ProtoBuilder() {
        protoParams = getClass().getAnnotation(ProtoParams.class);

        BuilderParams builderParams = getClass().getAnnotation(BuilderParams.class);
        extToMessageClass.put(builderParams.outExt(), protoParams.messageClass());
    }

    static public void addMessageClass(String ext, Class<? extends GeneratedMessage> klass) {
        extToMessageClass.put(ext, klass);
    }

    static public Class<? extends GeneratedMessage> getMessageClassFromExt(String ext) {
        return extToMessageClass.get(ext);
    }

    static public boolean supportsType(String ext) {
        Class<? extends GeneratedMessage> klass = getMessageClassFromExt(ext);
        return klass != null;
    }

    static public GeneratedMessage.Builder<?> newBuilder(String ext) throws CompileExceptionError {
        Class<? extends GeneratedMessage> klass = getMessageClassFromExt(ext);
        if (klass != null) {
            GeneratedMessage.Builder<?> builder;
            try {
                Method newBuilder = klass.getDeclaredMethod("newBuilder");
                return (GeneratedMessage.Builder<?>) newBuilder.invoke(null);
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

    @Override
    public Task<Void> create(IResource input) throws IOException, CompileExceptionError {
        return defaultTask(input);
    }

    @SuppressWarnings("unchecked")
    @Override
    public void build(Task<Void> task) throws CompileExceptionError,
            IOException {

        B builder;
        try {
            Method newBuilder = protoParams.messageClass().getDeclaredMethod("newBuilder");
            builder = (B) newBuilder.invoke(null);
        } catch (Exception e) {
            throw new RuntimeException(e);
        }

        ProtoUtil.merge(task.input(0), builder);
        builder = transform(task, task.input(0), builder);

        Message msg = builder.build();
        ByteArrayOutputStream out = new ByteArrayOutputStream(4 * 1024);
        msg.writeTo(out);
        out.close();
        task.output(0).setContent(out.toByteArray());
    }

}
