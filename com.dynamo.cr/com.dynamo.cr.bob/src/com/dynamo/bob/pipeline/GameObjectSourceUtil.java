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
// under the Defold License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

package com.dynamo.bob.pipeline;

import java.nio.charset.StandardCharsets;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.fs.IResource;
import com.dynamo.gameobject.proto.GameObjectSource;
import com.google.protobuf.Descriptors.FieldDescriptor;
import com.google.protobuf.Descriptors.OneofDescriptor;
import com.google.protobuf.MessageOrBuilder;
import com.google.protobuf.TextFormat;

/**
 * Converts the authoring-only game object messages into the standalone source
 * resources consumed by the existing component and game object builders.
 */
public final class GameObjectSourceUtil {
    private static final TextFormat.Printer TEXT_PRINTER = TextFormat.printer();

    private GameObjectSourceUtil() {
    }

    private static void requireNonEmpty(IResource input, String value, String fieldName) throws CompileExceptionError {
        if (value.isEmpty()) {
            throw new CompileExceptionError(input, 0, "missing required field '" + fieldName + "'");
        }
    }

    private static FieldDescriptor selectedPayload(IResource input, MessageOrBuilder message, String description)
            throws CompileExceptionError {
        OneofDescriptor payload = null;
        for (OneofDescriptor oneof : message.getDescriptorForType().getOneofs()) {
            if (oneof.getName().equals("payload")) {
                payload = oneof;
                break;
            }
        }
        if (payload == null) {
            throw new IllegalArgumentException(message.getDescriptorForType().getFullName() + " has no payload oneof");
        }
        FieldDescriptor selected = message.getOneofFieldDescriptor(payload);
        if (selected == null) {
            throw new CompileExceptionError(input, 0, description + " is missing a payload");
        }
        return selected;
    }

    public static byte[] getEmbeddedComponentData(IResource input, GameObjectSource.EmbeddedComponentDesc desc)
            throws CompileExceptionError {
        requireNonEmpty(input, desc.getId(), "id");
        requireNonEmpty(input, desc.getType(), "type");

        FieldDescriptor selected = selectedPayload(input, desc, "Embedded component '" + desc.getId() + "'");
        if (selected.getName().equals("data")) {
            return ((String) desc.getField(selected)).getBytes(StandardCharsets.UTF_8);
        }

        if (!selected.getName().equals(desc.getType())) {
            throw new CompileExceptionError(input, 0,
                    "Embedded component '" + desc.getId() + "' has type '" + desc.getType()
                            + "' but uses payload '" + selected.getName() + "'");
        }

        MessageOrBuilder payload = (MessageOrBuilder) desc.getField(selected);
        return TEXT_PRINTER.printToString(payload).getBytes(StandardCharsets.UTF_8);
    }

    public static byte[] getEmbeddedInstanceData(IResource input, GameObjectSource.EmbeddedInstanceDesc desc)
            throws CompileExceptionError {
        requireNonEmpty(input, desc.getId(), "id");

        FieldDescriptor selected = selectedPayload(input, desc, "Embedded instance '" + desc.getId() + "'");
        if (selected.getName().equals("data")) {
            return ((String) desc.getField(selected)).getBytes(StandardCharsets.UTF_8);
        }
        if (!selected.getName().equals("prototype")) {
            throw new CompileExceptionError(input, 0,
                    "Embedded instance '" + desc.getId() + "' uses unsupported payload '" + selected.getName() + "'");
        }

        MessageOrBuilder payload = (MessageOrBuilder) desc.getField(selected);
        return TEXT_PRINTER.printToString(payload).getBytes(StandardCharsets.UTF_8);
    }
}
