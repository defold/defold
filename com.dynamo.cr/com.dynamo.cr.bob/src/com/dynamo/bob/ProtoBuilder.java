package com.dynamo.bob;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.lang.reflect.Method;

import com.dynamo.bob.pipeline.ProtoUtil;
import com.google.protobuf.GeneratedMessage;
import com.google.protobuf.Message;

public abstract class ProtoBuilder<B extends GeneratedMessage.Builder<B>> extends Builder<Void> {

    private ProtoParams protoParams;

    public ProtoBuilder() {
        protoParams = getClass().getAnnotation(ProtoParams.class);
    }

    protected B transform(IResource resource, B messageBuilder) throws IOException, CompileExceptionError {
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
        builder = transform(task.input(0), builder);

        Message msg = builder.build();
        ByteArrayOutputStream out = new ByteArrayOutputStream(4 * 1024);
        msg.writeTo(out);
        out.close();
        task.output(0).setContent(out.toByteArray());
    }

}
