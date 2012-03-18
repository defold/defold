package com.dynamo.bob.pipeline;

import java.io.IOException;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.IResource;
import com.google.protobuf.Message.Builder;
import com.google.protobuf.TextFormat;

public class ProtoUtil {

    public static void merge(IResource input, Builder builder) throws IOException, CompileExceptionError {
        try {
            TextFormat.merge(new String(input.getContent()), builder);
        } catch (TextFormat.ParseException e) {
            throw new CompileExceptionError(input, e.getMessage(), e);
        }
    }
}
