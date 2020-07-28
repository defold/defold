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

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.lang.reflect.Method;

import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.ProtoUtil;
import com.google.protobuf.GeneratedMessage;
import com.google.protobuf.Message;

public abstract class ProtoBuilder<B extends GeneratedMessage.Builder<B>> extends Builder<Void> {

    private ProtoParams protoParams;

    public ProtoBuilder() {
        protoParams = getClass().getAnnotation(ProtoParams.class);
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
