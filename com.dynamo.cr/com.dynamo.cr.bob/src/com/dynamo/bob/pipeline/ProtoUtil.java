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
        byte[] inputContent = input.getContent();
        if (inputContent == null) {
            if (!input.exists()) {
                throw new CompileExceptionError(input, 0, "Resource does not exist");
            }
            else {
                throw new CompileExceptionError(input, 0, "Resource is empty");
            }
        }
        merge(input, inputContent, builder);
    }

    public static void merge(IResource input, byte[] inputContent, Builder builder) throws IOException, CompileExceptionError {
        try {
            TextFormat.merge(new String(inputContent), builder);
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
