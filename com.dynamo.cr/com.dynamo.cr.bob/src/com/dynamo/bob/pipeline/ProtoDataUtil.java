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

import java.util.Base64;
import java.util.List;
import java.util.Map;

import com.dynamo.gamesys.proto.DataProto.Data;
import com.dynamo.proto.DdfStruct.ListValue;
import com.dynamo.proto.DdfStruct.Struct;
import com.dynamo.proto.DdfStruct.Value;
import com.google.protobuf.ByteString;
import com.google.protobuf.Descriptors.EnumValueDescriptor;
import com.google.protobuf.Descriptors.FieldDescriptor;
import com.google.protobuf.Message;

/** Lossless, schema-independent storage for extension protobuf messages. */
public final class ProtoDataUtil {
    private ProtoDataUtil() {
    }

    public static Data toData(Message message) {
        return Data.newBuilder().setData(toValue(message)).build();
    }

    private static Value toValue(Message message) {
        Struct.Builder fields = Struct.newBuilder();
        for (Map.Entry<FieldDescriptor, Object> entry : message.getAllFields().entrySet()) {
            FieldDescriptor field = entry.getKey();
            Value value;
            if (field.isRepeated()) {
                ListValue.Builder values = ListValue.newBuilder();
                for (Object element : (List<?>) entry.getValue()) {
                    values.addValues(toValue(field, element));
                }
                value = Value.newBuilder().setList(values).build();
            } else {
                value = toValue(field, entry.getValue());
            }
            fields.putFields(field.getName(), value);
        }
        return Value.newBuilder().setStruct(fields).build();
    }

    private static Value toValue(FieldDescriptor field, Object value) {
        Value.Builder result = Value.newBuilder();
        switch (field.getJavaType()) {
            case MESSAGE: return toValue((Message) value);
            case BOOLEAN: return result.setBool((Boolean) value).build();
            case STRING: return result.setString((String) value).build();
            case ENUM: return result.setString(((EnumValueDescriptor) value).getName()).build();
            case BYTE_STRING:
                return result.setString(Base64.getEncoder().encodeToString(((ByteString) value).toByteArray())).build();
            // A double cannot represent all 64-bit integers. Keep their exact
            // decimal representation, including unsigned values.
            case LONG:
                return result.setString(field.getType() == FieldDescriptor.Type.UINT64
                        || field.getType() == FieldDescriptor.Type.FIXED64
                        ? Long.toUnsignedString((Long) value) : value.toString()).build();
            case INT:
                return result.setNumber(field.getType() == FieldDescriptor.Type.UINT32
                        || field.getType() == FieldDescriptor.Type.FIXED32
                        ? Integer.toUnsignedLong((Integer) value) : (Integer) value).build();
            default: return result.setNumber(((Number) value).doubleValue()).build();
        }
    }

    public static Message fromData(Data data, Message.Builder builder) {
        mergeStruct(data.getData(), builder);
        return builder.build();
    }

    private static void mergeStruct(Value value, Message.Builder builder) {
        requireKind(value, Value.KindCase.STRUCT);
        for (Map.Entry<String, Value> entry : value.getStruct().getFieldsMap().entrySet()) {
            FieldDescriptor field = builder.getDescriptorForType().findFieldByName(entry.getKey());
            if (field == null) {
                throw new IllegalArgumentException("Unknown field '" + entry.getKey() + "' in "
                        + builder.getDescriptorForType().getFullName());
            }
            if (field.getContainingOneof() != null && builder.hasOneof(field.getContainingOneof())) {
                throw new IllegalArgumentException("Multiple fields in oneof '" + field.getContainingOneof().getName() + "'");
            }
            if (field.isRepeated()) {
                requireKind(entry.getValue(), Value.KindCase.LIST);
                for (Value element : entry.getValue().getList().getValuesList()) {
                    builder.addRepeatedField(field, fromValue(field, element, builder));
                }
            } else {
                builder.setField(field, fromValue(field, entry.getValue(), builder));
            }
        }
    }

    private static void requireKind(Value value, Value.KindCase expected) {
        if (value.getKindCase() != expected) {
            throw new IllegalArgumentException("Expected " + expected + ", got " + value.getKindCase());
        }
    }

    private static Object fromValue(FieldDescriptor field, Value value, Message.Builder builder) {
        switch (field.getJavaType()) {
            case MESSAGE:
                Message.Builder child = builder.newBuilderForField(field);
                mergeStruct(value, child);
                return child.build();
            case BOOLEAN:
                requireKind(value, Value.KindCase.BOOL);
                return value.getBool();
            case STRING:
                requireKind(value, Value.KindCase.STRING);
                return value.getString();
            case ENUM:
                requireKind(value, Value.KindCase.STRING);
                EnumValueDescriptor enumValue = field.getEnumType().findValueByName(value.getString());
                if (enumValue == null) {
                    throw new IllegalArgumentException("Unknown enum value '" + value.getString() + "' for " + field.getFullName());
                }
                return enumValue;
            case BYTE_STRING:
                requireKind(value, Value.KindCase.STRING);
                return ByteString.copyFrom(Base64.getDecoder().decode(value.getString()));
            case LONG:
                requireKind(value, Value.KindCase.STRING);
                return field.getType() == FieldDescriptor.Type.UINT64 || field.getType() == FieldDescriptor.Type.FIXED64
                        ? Long.parseUnsignedLong(value.getString()) : Long.parseLong(value.getString());
            case INT:
                requireKind(value, Value.KindCase.NUMBER);
                double number = value.getNumber();
                boolean unsigned = field.getType() == FieldDescriptor.Type.UINT32 || field.getType() == FieldDescriptor.Type.FIXED32;
                if (!Double.isFinite(number) || number != Math.rint(number)
                        || number < (unsigned ? 0 : Integer.MIN_VALUE)
                        || number > (unsigned ? 0xffffffffL : Integer.MAX_VALUE)) {
                    throw new IllegalArgumentException("Invalid integer for " + field.getFullName() + ": " + number);
                }
                return (int) (long) number;
            case FLOAT:
                requireKind(value, Value.KindCase.NUMBER);
                return (float) value.getNumber();
            default:
                requireKind(value, Value.KindCase.NUMBER);
                return value.getNumber();
        }
    }
}
