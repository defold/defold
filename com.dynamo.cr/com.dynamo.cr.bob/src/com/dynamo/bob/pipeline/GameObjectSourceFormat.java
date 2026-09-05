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

import java.util.List;
import java.util.Map;

import com.dynamo.gameobject.proto.GameObjectSource;
import com.dynamo.gamesys.proto.DataProto.Data;
import com.dynamo.proto.DdfStruct.Value;
import com.google.protobuf.DescriptorProtos.DescriptorProto;
import com.google.protobuf.DescriptorProtos.FieldDescriptorProto;
import com.google.protobuf.DescriptorProtos.FileDescriptorProto;
import com.google.protobuf.Descriptors;
import com.google.protobuf.Descriptors.FieldDescriptor;
import com.google.protobuf.DynamicMessage;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;

/** Loads string payloads from released Defold game object and collection files. */
final class GameObjectSourceFormat {
    private static final Descriptors.FileDescriptor READ_DESCRIPTOR = makeReadDescriptor();

    private GameObjectSourceFormat() {
    }

    private static Descriptors.FileDescriptor makeReadDescriptor() {
        Descriptors.FileDescriptor source = GameObjectSource.getDescriptor();
        FileDescriptorProto.Builder file = source.toProto().toBuilder();
        for (DescriptorProto.Builder message : file.getMessageTypeBuilderList()) {
            if (message.getName().equals("EmbeddedComponentDesc") || message.getName().equals("EmbeddedInstanceDesc")) {
                message.addField(FieldDescriptorProto.newBuilder()
                        .setName("data").setNumber(message.getFieldCount() + 1).setOneofIndex(0)
                        .setLabel(FieldDescriptorProto.Label.LABEL_OPTIONAL)
                        .setType(FieldDescriptorProto.Type.TYPE_STRING));
            }
        }
        try {
            return Descriptors.FileDescriptor.buildFrom(file.build(),
                    source.getDependencies().toArray(new Descriptors.FileDescriptor[0]));
        } catch (Descriptors.DescriptorValidationException e) {
            throw new ExceptionInInitializerError(e);
        }
    }

    static Message.Builder newReadBuilder(Message.Builder builder) {
        return DynamicMessage.newBuilder(READ_DESCRIPTOR.findMessageTypeByName(builder.getDescriptorForType().getName()));
    }

    static void copyToSource(Message parsed, Message.Builder target) throws TextFormat.ParseException {
        for (Map.Entry<FieldDescriptor, Object> entry : parsed.getAllFields().entrySet()) {
            FieldDescriptor field = entry.getKey();
            FieldDescriptor targetField = target.getDescriptorForType().findFieldByName(field.getName());
            if (targetField == null) {
                if (field.getName().equals("data")) {
                    String text = (String) entry.getValue();
                    if (target.getDescriptorForType().getName().equals("EmbeddedInstanceDesc")) {
                        GameObjectSource.PrototypeDesc.Builder prototype = GameObjectSource.PrototypeDesc.newBuilder();
                        ProtoUtil.mergeStrictText(text, prototype);
                        target.setField(target.getDescriptorForType().findFieldByName("prototype"), prototype.buildPartial());
                    } else {
                        // A transient text value keeps legacy component bytes intact
                        // until the component's registered reader handles them.
                        target.setField(target.getDescriptorForType().findFieldByName("component_data"),
                                Data.newBuilder().setData(Value.newBuilder().setString(text)).build());
                    }
                }
            } else if (field.isRepeated()) {
                for (Object value : (List<?>) entry.getValue()) {
                    target.addRepeatedField(targetField, copyValue(field, value, target, targetField));
                }
            } else {
                target.setField(targetField, copyValue(field, entry.getValue(), target, targetField));
            }
        }
    }

    private static Object copyValue(FieldDescriptor field, Object value, Message.Builder target, FieldDescriptor targetField)
            throws TextFormat.ParseException {
        if (field.getJavaType() != FieldDescriptor.JavaType.MESSAGE
                || !field.getMessageType().getFile().getPackage().equals("dmGameObjectSourceDDF")) {
            return value;
        }
        Message.Builder child = target.newBuilderForField(targetField);
        copyToSource((Message) value, child);
        return child.buildPartial();
    }
}
