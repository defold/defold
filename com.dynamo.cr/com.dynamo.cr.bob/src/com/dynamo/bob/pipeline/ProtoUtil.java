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
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.fs.IResource;
import com.google.protobuf.Descriptors.Descriptor;
import com.google.protobuf.Descriptors.FieldDescriptor;
import com.google.protobuf.Descriptors.OneofDescriptor;
import com.google.protobuf.Message.Builder;
import com.google.protobuf.TextFormat;
import com.google.protobuf.TextFormatParseInfoTree;
import com.google.protobuf.TextFormatParseLocation;

public class ProtoUtil {

    private static final String GAME_OBJECT_SOURCE_PACKAGE = "dmGameObjectSourceDDF";

    private static final class FieldOccurrence {
        final FieldDescriptor field;
        final TextFormatParseLocation location;

        FieldOccurrence(FieldDescriptor field, TextFormatParseLocation location) {
            this.field = field;
            this.location = location;
        }
    }

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
        merge(input, inputContent, builder, false);
    }

    public static void mergeStrict(IResource input, Builder builder) throws IOException, CompileExceptionError {
        byte[] inputContent = input.getContent();
        if (inputContent == null) {
            if (!input.exists()) {
                throw new CompileExceptionError(input, 0, "Resource does not exist");
            } else {
                throw new CompileExceptionError(input, 0, "Resource is empty");
            }
        }
        merge(input, inputContent, builder, true);
    }

    public static void mergeStrict(IResource input, byte[] inputContent, Builder builder) throws IOException, CompileExceptionError {
        merge(input, inputContent, builder, true);
    }

    private static void merge(IResource input, byte[] inputContent, Builder builder, boolean strict)
            throws CompileExceptionError {
        try {
            String text = new String(inputContent, StandardCharsets.UTF_8);
            if (strict) {
                TextFormatParseInfoTree.Builder parseInfoBuilder = TextFormatParseInfoTree.builder();
                TextFormat.Parser parser = TextFormat.Parser.newBuilder()
                        .setParseInfoTreeBuilder(parseInfoBuilder)
                        .build();
                parser.merge(text, builder);
                rejectConflictingOneofFields(builder.getDescriptorForType(),
                        Collections.singletonList(parseInfoBuilder.build()));
            } else {
                TextFormat.merge(text, builder);
            }
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

    private static void rejectConflictingOneofFields(Descriptor descriptor,
                                                      List<TextFormatParseInfoTree> fragments)
            throws TextFormat.ParseException {
        if (!descriptor.getFile().getPackage().equals(GAME_OBJECT_SOURCE_PACKAGE)) {
            return;
        }

        for (OneofDescriptor oneof : descriptor.getOneofs()) {
            if (!oneof.getName().equals("payload")) {
                continue;
            }
            List<FieldOccurrence> occurrences = new ArrayList<>();
            for (FieldDescriptor field : oneof.getFields()) {
                for (TextFormatParseInfoTree fragment : fragments) {
                    for (TextFormatParseLocation location : fragment.getLocations(field)) {
                        occurrences.add(new FieldOccurrence(field, location));
                    }
                }
            }

            occurrences.sort(Comparator
                    .comparingInt((FieldOccurrence occurrence) -> occurrence.location.getLine())
                    .thenComparingInt(occurrence -> occurrence.location.getColumn()));

            if (!occurrences.isEmpty()) {
                FieldDescriptor firstField = occurrences.get(0).field;
                for (FieldOccurrence occurrence : occurrences) {
                    if (occurrence.field != firstField) {
                        throw new TextFormat.ParseException(
                                occurrence.location.getLine() + 1,
                                occurrence.location.getColumn() + 1,
                                "field '" + occurrence.field.getName() + "' is specified along with field '"
                                        + firstField.getName() + "', another member of oneof '"
                                        + oneof.getName() + "'");
                    }
                }
            }
        }

        for (FieldDescriptor field : descriptor.getFields()) {
            if (field.getJavaType() != FieldDescriptor.JavaType.MESSAGE
                    || !field.getMessageType().getFile().getPackage().equals(GAME_OBJECT_SOURCE_PACKAGE)) {
                continue;
            }

            List<TextFormatParseInfoTree> nestedFragments = new ArrayList<>();
            for (TextFormatParseInfoTree fragment : fragments) {
                nestedFragments.addAll(fragment.getNestedTrees(field));
            }

            if (field.isRepeated()) {
                for (TextFormatParseInfoTree nestedFragment : nestedFragments) {
                    rejectConflictingOneofFields(field.getMessageType(),
                            Collections.singletonList(nestedFragment));
                }
            } else if (!nestedFragments.isEmpty()) {
                // TextFormat permits a singular message field to be split across
                // multiple fragments and merges them into one logical message.
                rejectConflictingOneofFields(field.getMessageType(), nestedFragments);
            }
        }
    }
}
