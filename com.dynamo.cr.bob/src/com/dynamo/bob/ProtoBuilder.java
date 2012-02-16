package com.dynamo.bob;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.lang.reflect.Method;

import com.google.protobuf.GeneratedMessage;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;

public abstract class ProtoBuilder<B extends GeneratedMessage.Builder<B>> extends Builder<Void> {

    private ProtoParams protoParams;

    public ProtoBuilder() {
        protoParams = getClass().getAnnotation(ProtoParams.class);
    }

    protected B transform(B messageBuilder) throws IOException, CompileExceptionError {
        return messageBuilder;
    }

    @Override
    public Task<Void> create(IResource input) throws IOException {
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

        InputStreamReader reader = new InputStreamReader(new ByteArrayInputStream(task.input(0).getContent()));
        TextFormat.merge(reader, builder);
        reader.close();

        builder = transform(builder);

        Message msg = builder.build();
        ByteArrayOutputStream out = new ByteArrayOutputStream(4 * 1024);
        msg.writeTo(out);
        out.close();
        task.output(0).setContent(out.toByteArray());
    }

}
