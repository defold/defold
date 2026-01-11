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

package com.dynamo.cr.common.providers.test;

import static org.junit.Assert.assertEquals;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;

import javax.ws.rs.WebApplicationException;
import javax.ws.rs.ext.MessageBodyReader;

import org.codehaus.jackson.JsonNode;
import org.codehaus.jackson.map.ObjectMapper;
import org.junit.Test;

import com.dynamo.cr.common.providers.JsonProviders.ProtobufMessageBodyReader;
import com.dynamo.cr.common.providers.JsonProviders.ProtobufMessageBodyWriter;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus.State;
import com.dynamo.cr.protocol.proto.Protocol.UserInfo;
import com.google.protobuf.Message;

public class JsonProvidersTest {

    @SuppressWarnings({ "rawtypes", "unchecked" })
    @Test
    public void testWriteTo2() throws WebApplicationException, IOException {
        BranchStatus.Builder b = BranchStatus.newBuilder();
        b.setName("a name");
        b.setBranchState(State.DIRTY);
        b.addFileStatus(BranchStatus.Status.newBuilder().setName("foo.cpp").setOriginal("bar.cpp").setIndexStatus("M").setWorkingTreeStatus(" ").build());
        b.setCommitsAhead(4);
        b.setCommitsBehind(3);

        ProtobufMessageBodyWriter writer = new ProtobufMessageBodyWriter();
        BranchStatus message = b.build();
        long size = writer.getSize(message, null, null, null, null);
        ByteArrayOutputStream stream = new ByteArrayOutputStream();
        writer.writeTo(message, null, null, null, null, null, stream);
        assertEquals(size, stream.size());

        ObjectMapper m = new ObjectMapper();
        JsonNode node = m.readValue(stream.toString(), JsonNode.class);
        assertEquals(message.getName(), node.get("name").getValueAsText());
        assertEquals(message.getBranchState().getValueDescriptor().getName(), node.get("branch_state").getValueAsText());
        assertEquals(1, node.get("file_status").size());
        JsonNode fileStatus = node.get("file_status").get(0);
        assertEquals("foo.cpp", fileStatus.get("name").getValueAsText());
        assertEquals(message.getCommitsAhead(), node.get("commits_ahead").getValueAsInt());
        assertEquals(message.getCommitsBehind(), node.get("commits_behind").getValueAsInt());

        ByteArrayInputStream inStream = new ByteArrayInputStream(node.toString().getBytes());
        MessageBodyReader reader = new ProtobufMessageBodyReader();
        Message message2 = (Message) reader.readFrom(message.getClass(), null, null, null, null, inStream);
        assertEquals(message, message2);
    }

    @SuppressWarnings({ "rawtypes", "unchecked" })
    @Test
    public void testJsonUnicode() throws WebApplicationException, IOException {

        UserInfo message = UserInfo
                .newBuilder()
                .setFirstName("åäö")
                .setLastName("ÅÄÖ")
                .setId(123)
                .setEmail("foo@bar.com")
                .build();

        ProtobufMessageBodyWriter writer = new ProtobufMessageBodyWriter();
        long size = writer.getSize(message, null, null, null, null);
        ByteArrayOutputStream stream = new ByteArrayOutputStream();
        writer.writeTo(message, null, null, null, null, null, stream);
        assertEquals(size, stream.size());

        ObjectMapper m = new ObjectMapper();
        JsonNode node = m.readValue(stream.toString("UTF-8"), JsonNode.class);
        System.out.println(node);
        assertEquals(message.getFirstName(), node.get("first_name").asText());
        assertEquals(message.getLastName(), node.get("last_name").asText());

        ByteArrayInputStream inStream = new ByteArrayInputStream(node.toString().getBytes("UTF-8"));
        MessageBodyReader reader = new ProtobufMessageBodyReader();
        Message message2 = (Message) reader.readFrom(message.getClass(), null, null, null, null, inStream);
        assertEquals(message, message2);
    }

}
