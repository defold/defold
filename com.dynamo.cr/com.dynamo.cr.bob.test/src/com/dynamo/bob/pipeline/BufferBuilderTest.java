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

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Arrays;
import java.util.Collections;

import org.junit.Test;

import com.dynamo.bob.Progress;
import com.dynamo.bob.TaskResult;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.gamesys.proto.BufferProto.BufferDesc;
import com.dynamo.gamesys.proto.BufferProto.StreamDesc;
import com.dynamo.gamesys.proto.BufferProto.ValueType;

import static org.junit.Assert.*;

public class BufferBuilderTest extends AbstractProtoBuilderTest {
    private StreamDesc.Builder stream(long hash, ValueType type, int count) {
        return StreamDesc.newBuilder().setName("stream" + hash).setNameHash(hash).setValueType(type).setValueCount(count);
    }

    private byte[] buildJson(String source) throws Exception {
        addFile("/test.buffer", source);
        getProject().setInputs(Collections.singletonList("/test.buffer"));
        byte[] packed = null;
        for (TaskResult result : getProject().build(Progress.discarding(), "build")) {
            assertTrue(result.getMessage(), result.isOk());
            for (IResource output : result.getTask().getOutputs()) {
                if (output.getPath().endsWith(".bufferc")) {
                    packed = output.getContent();
                }
            }
        }
        assertNotNull(packed);
        return packed;
    }

    // Verify exact interleaving, alignment and zero filling for the layout in #6272.
    @Test
    public void testMixedStreams() {
        BufferDesc desc = BufferDesc.newBuilder()
            .addStreams(stream(1, ValueType.VALUE_TYPE_FLOAT32, 3).addAllF(Arrays.asList(1f, 2f, 3f, 4f, 5f, 6f)))
            .addStreams(stream(2, ValueType.VALUE_TYPE_INT8, 3).addAllI(Arrays.asList(-1, 2, -3, 4)))
            .addStreams(stream(3, ValueType.VALUE_TYPE_UINT8, 3))
            .build();
        ByteBuffer expected = ByteBuffer.allocate(104).order(ByteOrder.LITTLE_ENDIAN);
        expected.put(new byte[] {'B', 'U', 'F', 'C', 1, 0, 3, 0, 2, 0, 0, 0, 20, 0, 0, 0});
        expected.putLong(1).putInt(0).put((byte) 8).put((byte) 3).putShort((short) 0);
        expected.putLong(2).putInt(12).put((byte) 4).put((byte) 3).putShort((short) 0);
        expected.putLong(3).putInt(15).put((byte) 0).put((byte) 3).putShort((short) 0);
        expected.putFloat(64, 1).putFloat(68, 2).putFloat(72, 3);
        expected.put(76, (byte) -1).put(77, (byte) 2).put(78, (byte) -3);
        expected.putFloat(84, 4).putFloat(88, 5).putFloat(92, 6).put(96, (byte) 4);
        assertArrayEquals(expected.array(), BufferBuilder.buildBuffer(desc));
    }

    // Every scalar width must preserve its bits, including 64-bit values and negative integers.
    @Test
    public void testAllTypes() {
        BufferDesc desc = BufferDesc.newBuilder()
            .addStreams(stream(1, ValueType.VALUE_TYPE_UINT8, 1).addUi(255))
            .addStreams(stream(2, ValueType.VALUE_TYPE_UINT16, 1).addUi(0x1234))
            .addStreams(stream(3, ValueType.VALUE_TYPE_UINT32, 1).addUi(0x89abcdef))
            .addStreams(stream(4, ValueType.VALUE_TYPE_UINT64, 1).addUi64(0x0123456789abcdefL))
            .addStreams(stream(5, ValueType.VALUE_TYPE_INT8, 1).addI(-2))
            .addStreams(stream(6, ValueType.VALUE_TYPE_INT16, 1).addI(-12345))
            .addStreams(stream(7, ValueType.VALUE_TYPE_INT32, 1).addI(-123456789))
            .addStreams(stream(8, ValueType.VALUE_TYPE_INT64, 1).addI64(-1234567890123456789L))
            .addStreams(stream(9, ValueType.VALUE_TYPE_FLOAT32, 1).addF(-1.5f))
            .build();
        byte[] packed = BufferBuilder.buildBuffer(desc);
        assertEquals(200, packed.length);
        ByteBuffer expected = ByteBuffer.allocate(40).order(ByteOrder.LITTLE_ENDIAN);
        expected.put(0, (byte) 255).putShort(2, (short) 0x1234).putInt(4, 0x89abcdef);
        expected.putLong(8, 0x0123456789abcdefL).put(16, (byte) -2).putShort(18, (short) -12345);
        expected.putInt(20, -123456789).putLong(24, -1234567890123456789L).putFloat(32, -1.5f);
        assertArrayEquals(expected.array(), Arrays.copyOfRange(packed, 160, 200));
        int[] offsets = {0, 2, 4, 8, 16, 18, 20, 24, 32};
        ByteBuffer bytes = ByteBuffer.wrap(packed).order(ByteOrder.LITTLE_ENDIAN);
        for (int i = 0; i < offsets.length; ++i) {
            assertEquals(offsets[i], bytes.getInt(24 + i * 16));
        }
    }

    // Missing data retains stream declarations and produces a zero-element runtime buffer.
    @Test
    public void testNoData() throws Exception {
        byte[] packed = buildJson("[{\"name\":\"position\",\"type\":\"float32\",\"count\":3}]");
        ByteBuffer bytes = ByteBuffer.wrap(packed).order(ByteOrder.LITTLE_ENDIAN);
        assertEquals(32, packed.length);
        assertEquals(0, bytes.getInt(8));
        assertEquals(12, bytes.getInt(12));
        assertEquals(MurmurHash.hash64("position"), bytes.getLong(16));
    }

    // Bob must produce the same binary bytes as the shared packer, preserving integer narrowing.
    @Test
    public void testJsonBuild() throws Exception {
        byte[] packed = buildJson("[{\"name\":\"normal\",\"type\":\"int8\",\"count\":1,\"data\":[-128,127,255]}]");
        BufferDesc desc = BufferDesc.newBuilder()
            .addStreams(StreamDesc.newBuilder().setName("normal").setNameHash(MurmurHash.hash64("normal"))
                .setValueType(ValueType.VALUE_TYPE_INT8).setValueCount(1).addI(-128).addI(127).addI(255))
            .build();
        assertArrayEquals(BufferBuilder.buildBuffer(desc), packed);
        assertArrayEquals(new byte[] {-128, 127, -1}, Arrays.copyOfRange(packed, 32, 35));
    }

    // Partial final elements are ignored when determining the element count, as before.
    @Test
    public void testIncompleteElement() {
        BufferDesc desc = BufferDesc.newBuilder()
            .addStreams(stream(1, ValueType.VALUE_TYPE_INT16, 2).addI(1).addI(2).addI(3)).build();
        byte[] packed = BufferBuilder.buildBuffer(desc);
        assertEquals(36, packed.length);
        assertArrayEquals(new byte[] {1, 0, 2, 0}, Arrays.copyOfRange(packed, 32, 36));
    }

    // Layouts outside the runtime's byte-sized stream/component limits must fail compilation.
    @Test
    public void testInvalidLayouts() {
        BufferDesc[] invalid = {
            BufferDesc.getDefaultInstance(),
            BufferDesc.newBuilder().addStreams(stream(1, ValueType.VALUE_TYPE_UINT8, 0)).build(),
            BufferDesc.newBuilder().addStreams(stream(1, ValueType.VALUE_TYPE_UINT8, 256)).build(),
            BufferDesc.newBuilder().addAllStreams(Collections.nCopies(256, stream(1, ValueType.VALUE_TYPE_UINT8, 1).build())).build()
        };
        for (BufferDesc desc : invalid) {
            try {
                BufferBuilder.buildBuffer(desc);
                fail("Expected invalid layout");
            } catch (IllegalArgumentException expected) {
                assertNotNull(expected.getMessage());
            }
        }
    }
}
