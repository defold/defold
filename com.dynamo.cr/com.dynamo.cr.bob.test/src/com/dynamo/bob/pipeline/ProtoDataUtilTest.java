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

import org.junit.Assert;
import org.junit.Test;

import com.dynamo.gameobject.proto.GameObject;
import com.dynamo.gamesys.proto.DataProto.Data;
import com.dynamo.proto.DdfStruct.Struct;
import com.dynamo.proto.DdfStruct.Value;
import com.google.protobuf.ByteString;
import com.google.protobuf.DescriptorProtos.DescriptorProto;
import com.google.protobuf.DescriptorProtos.FieldDescriptorProto;
import com.google.protobuf.DescriptorProtos.FieldDescriptorProto.Type;
import com.google.protobuf.DescriptorProtos.FileDescriptorProto;
import com.google.protobuf.Descriptors;
import com.google.protobuf.DynamicMessage;
import com.google.protobuf.Message;

public class ProtoDataUtilTest {
    private static FieldDescriptorProto field(String name, int number, Type type) {
        return FieldDescriptorProto.newBuilder().setName(name).setNumber(number).setType(type)
                .setLabel(FieldDescriptorProto.Label.LABEL_OPTIONAL).build();
    }

    private static Descriptors.Descriptor descriptor() throws Exception {
        DescriptorProto message = DescriptorProto.newBuilder().setName("Extension")
                .addField(field("signed", 1, Type.TYPE_INT64))
                .addField(field("unsigned", 2, Type.TYPE_UINT64))
                .addField(field("bytes", 3, Type.TYPE_BYTES))
                .addField(field("text", 4, Type.TYPE_STRING))
                .addField(field("enabled", 5, Type.TYPE_BOOL))
                .addField(field("weight", 6, Type.TYPE_FLOAT))
                .addField(field("indices", 7, Type.TYPE_UINT32).toBuilder()
                        .setLabel(FieldDescriptorProto.Label.LABEL_REPEATED))
                .addField(field("property", 8, Type.TYPE_MESSAGE).toBuilder()
                        .setTypeName(".dmGameObjectDDF.PropertyDesc"))
                .build();
        FileDescriptorProto file = FileDescriptorProto.newBuilder().setName("extension_test.proto")
                .setSyntax("proto2").addDependency(GameObject.getDescriptor().getName())
                .addMessageType(message).build();
        return Descriptors.FileDescriptor.buildFrom(file, new Descriptors.FileDescriptor[] {GameObject.getDescriptor()})
                .findMessageTypeByName("Extension");
    }

    @Test
    public void preservesExtensionFieldTypesAndPresence() throws Exception {
        Descriptors.Descriptor descriptor = descriptor();
        Message original = DynamicMessage.newBuilder(descriptor)
                .setField(descriptor.findFieldByName("signed"), Long.MIN_VALUE)
                .setField(descriptor.findFieldByName("unsigned"), -1L)
                .setField(descriptor.findFieldByName("bytes"), ByteString.copyFrom(new byte[] {0, -1, 42}))
                .setField(descriptor.findFieldByName("text"), "Spelare åäö")
                .setField(descriptor.findFieldByName("enabled"), false)
                .setField(descriptor.findFieldByName("weight"), -0.0f)
                .addRepeatedField(descriptor.findFieldByName("indices"), -1)
                .addRepeatedField(descriptor.findFieldByName("indices"), 0)
                .setField(descriptor.findFieldByName("property"), GameObject.PropertyDesc.newBuilder()
                        .setId("speed").setValue("12.5").setType(GameObject.PropertyType.PROPERTY_TYPE_NUMBER).build())
                .build();
        Data data = ProtoDataUtil.toData(original);
        Struct fields = data.getData().getStruct();
        Assert.assertEquals("-9223372036854775808", fields.getFieldsOrThrow("signed").getString());
        Assert.assertEquals("18446744073709551615", fields.getFieldsOrThrow("unsigned").getString());
        Assert.assertEquals(4294967295.0, fields.getFieldsOrThrow("indices").getList().getValues(0).getNumber(), 0);
        Assert.assertEquals("PROPERTY_TYPE_NUMBER", fields.getFieldsOrThrow("property").getStruct()
                .getFieldsOrThrow("type").getString());
        Assert.assertEquals(original, ProtoDataUtil.fromData(data, DynamicMessage.newBuilder(descriptor)));
        Assert.assertArrayEquals(original.toByteArray(),
                ProtoDataUtil.fromData(data, DynamicMessage.newBuilder(descriptor)).toByteArray());
    }

    @Test
    public void preservesMapsAndEmptyMessages() {
        Data original = Data.newBuilder().setData(Value.newBuilder().setStruct(Struct.newBuilder()
                .putFields("empty", Value.newBuilder().setStruct(Struct.getDefaultInstance()).build())
                .putFields("disabled", Value.newBuilder().setBool(false).build())))
                .build();
        Assert.assertEquals(original, ProtoDataUtil.fromData(ProtoDataUtil.toData(original), Data.newBuilder()));
    }

    @Test(expected = IllegalArgumentException.class)
    public void rejectsUnknownFields() {
        Data data = Data.newBuilder().setData(Value.newBuilder().setStruct(Struct.newBuilder()
                .putFields("typo", Value.newBuilder().setString("value").build()))).build();
        ProtoDataUtil.fromData(data, GameObject.PropertyDesc.newBuilder());
    }

    @Test(expected = IllegalArgumentException.class)
    public void rejectsFractionalIntegers() throws Exception {
        Data data = Data.newBuilder().setData(Value.newBuilder().setStruct(Struct.newBuilder()
                .putFields("indices", Value.newBuilder().setList(com.dynamo.proto.DdfStruct.ListValue.newBuilder()
                        .addValues(Value.newBuilder().setNumber(1.5))).build()))).build();
        ProtoDataUtil.fromData(data, DynamicMessage.newBuilder(descriptor()));
    }

    @Test(expected = IllegalArgumentException.class)
    public void rejectsConflictingExtensionOneofFields() {
        Data data = Data.newBuilder().setData(Value.newBuilder().setStruct(Struct.newBuilder()
                .putFields("string", Value.newBuilder().setString("value").build())
                .putFields("bool", Value.newBuilder().setBool(false).build()))).build();
        ProtoDataUtil.fromData(data, Value.newBuilder());
    }
}
