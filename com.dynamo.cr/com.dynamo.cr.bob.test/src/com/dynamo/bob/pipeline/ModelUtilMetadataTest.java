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

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.security.DigestInputStream;
import java.security.MessageDigest;
import java.util.List;

import org.junit.Test;

public class ModelUtilMetadataTest {
    private static final int GLB_MAGIC = 0x46546c67;
    private static final int GLB_VERSION = 2;
    private static final int GLB_JSON_CHUNK_TYPE = 0x4e4f534a;
    private static final int GLB_BIN_CHUNK_TYPE = 0x004e4942;

    private static final class TrackingInputStream extends ByteArrayInputStream {
        private int largestReadRequest;

        TrackingInputStream(byte[] data) {
            super(data);
        }

        @Override
        public synchronized int read(byte[] buffer, int offset, int length) {
            largestReadRequest = Math.max(largestReadRequest, length);
            return super.read(buffer, offset, length);
        }

        @Override
        public synchronized byte[] readAllBytes() {
            throw new AssertionError("The complete model file must not be retained");
        }
    }

    private static byte[] makeGlb(String json, int binByteCount) {
        byte[] jsonBytes = json.getBytes(StandardCharsets.UTF_8);
        int paddedJsonByteCount = (jsonBytes.length + 3) & ~3;
        int totalByteCount = 12 + 8 + paddedJsonByteCount + (binByteCount == 0 ? 0 : 8 + binByteCount);
        ByteBuffer glb = ByteBuffer.allocate(totalByteCount).order(ByteOrder.LITTLE_ENDIAN);
        glb.putInt(GLB_MAGIC);
        glb.putInt(GLB_VERSION);
        glb.putInt(totalByteCount);
        glb.putInt(paddedJsonByteCount);
        glb.putInt(GLB_JSON_CHUNK_TYPE);
        glb.put(jsonBytes);
        while (glb.position() < 12 + 8 + paddedJsonByteCount) {
            glb.put((byte) ' ');
        }
        if (binByteCount > 0) {
            glb.putInt(binByteCount);
            glb.putInt(GLB_BIN_CHUNK_TYPE);
            glb.position(glb.position() + binByteCount);
        }
        return glb.array();
    }

    private static byte[] sha256(byte[] data) throws Exception {
        return MessageDigest.getInstance("SHA-256").digest(data);
    }

    private static TrackingInputStream assertMalformedInputIsDrained(byte[] data) throws Exception {
        TrackingInputStream stream = new TrackingInputStream(data);
        MessageDigest digest = MessageDigest.getInstance("SHA-256");

        try {
            ModelUtil.getModelMetadata(new DigestInputStream(stream, digest));
            fail("Expected malformed model metadata to fail inspection");
        } catch (IOException expected) {
            assertEquals(0, stream.available());
            assertArrayEquals(sha256(data), digest.digest());
        }
        return stream;
    }

    @Test
    public void testExternalBufferUrisFromGltf() throws Exception {
        String gltf = "{" +
                "\"buffers\":[" +
                "{\"uri\":\"mesh.bin\"}," +
                "{\"uri\":\"mesh.bin\"}," +
                "{\"uri\":\"DATA:application/octet-stream;base64,AA==\"}," +
                "{\"uri\":\"\"}," +
                "{}]}";
        byte[] data = gltf.getBytes(StandardCharsets.UTF_8);
        TrackingInputStream stream = new TrackingInputStream(data);
        MessageDigest digest = MessageDigest.getInstance("SHA-256");

        ModelUtil.ModelMetadata metadata = ModelUtil.getModelMetadata(new DigestInputStream(stream, digest));

        assertEquals(List.of("mesh.bin"), metadata.externalBufferUris());
        assertFalse(metadata.hasMorphTargets());
        assertEquals(0, stream.available());
        assertArrayEquals(sha256(data), digest.digest());
    }

    @Test
    public void testGlbBinChunkIsDrainedWithoutBeingRetained() throws Exception {
        int binByteCount = 1024 * 1024;
        byte[] glb = makeGlb("{" +
                "\"buffers\":[{\"uri\":\"mesh.bin\"}]," +
                "\"meshes\":[{\"primitives\":[{\"targets\":[{}]}]}]}", binByteCount);
        TrackingInputStream stream = new TrackingInputStream(glb);
        MessageDigest digest = MessageDigest.getInstance("SHA-256");

        ModelUtil.ModelMetadata metadata = ModelUtil.getModelMetadata(new DigestInputStream(stream, digest));

        assertEquals(List.of("mesh.bin"), metadata.externalBufferUris());
        assertTrue(metadata.hasMorphTargets());
        assertEquals(0, stream.available());
        assertTrue(stream.largestReadRequest < binByteCount);
        assertArrayEquals(sha256(glb), digest.digest());
    }

    @Test
    public void testMalformedGlbIsDrained() throws Exception {
        byte[] glb = makeGlb("{}", 1024 * 1024);
        ByteBuffer.wrap(glb).order(ByteOrder.LITTLE_ENDIAN).putInt(16, 0);
        assertMalformedInputIsDrained(glb);
    }

    @Test
    public void testMalformedGltfIsDrained() throws Exception {
        assertMalformedInputIsDrained("{\"buffers\":[".getBytes(StandardCharsets.UTF_8));
    }

    @Test
    public void testGlbTotalLengthMustMatchStreamLength() throws Exception {
        byte[] tooShort = makeGlb("{}", 1024);
        ByteBuffer.wrap(tooShort).order(ByteOrder.LITTLE_ENDIAN).putInt(8, tooShort.length - 1);
        assertMalformedInputIsDrained(tooShort);

        byte[] tooLong = makeGlb("{}", 1024);
        ByteBuffer.wrap(tooLong).order(ByteOrder.LITTLE_ENDIAN).putInt(8, tooLong.length + 1);
        assertMalformedInputIsDrained(tooLong);
    }

    @Test
    public void testHugeTruncatedJsonChunkIsNotRetained() throws Exception {
        int declaredJsonByteCount = 1024 * 1024;
        ByteBuffer glb = ByteBuffer.allocate(24).order(ByteOrder.LITTLE_ENDIAN);
        glb.putInt(GLB_MAGIC);
        glb.putInt(GLB_VERSION);
        glb.putInt(20 + declaredJsonByteCount);
        glb.putInt(declaredJsonByteCount);
        glb.putInt(GLB_JSON_CHUNK_TYPE);
        glb.put("{}  ".getBytes(StandardCharsets.UTF_8));

        TrackingInputStream stream = assertMalformedInputIsDrained(glb.array());
        assertTrue(stream.largestReadRequest <= 8192);
    }
}
