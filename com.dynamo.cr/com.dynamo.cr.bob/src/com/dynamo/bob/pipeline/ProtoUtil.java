package com.dynamo.bob.pipeline;

import java.io.IOException;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.fs.IResource;
import com.google.protobuf.Message.Builder;
import com.google.protobuf.TextFormat;

public class ProtoUtil {

    public static void merge(IResource input, Builder builder) throws IOException, CompileExceptionError {
        try {
            TextFormat.merge(new String(input.getContent()), builder);
        } catch (TextFormat.ParseException e) {
            // 1:7: String missing ending quote.
            Pattern pattern = Pattern.compile("(\\d+):(\\d+): (.*)");
            Matcher m = pattern.matcher(e.getMessage());
            if (m.matches()) {
                throw new CompileExceptionError(input, Integer.parseInt(m.group(1)), m.group(3), e);
            } else {
                throw new CompileExceptionError(input, 0, e.getMessage(), e);
            }
        }
    }
}
