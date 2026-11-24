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

package com.dynamo.bob.util;

import static org.junit.Assert.*;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;

import org.junit.Test;

public class OggUtilTest {
    private static final byte[] EMPTY_COMMENTS = {
        3, 'v', 'o', 'r', 'b', 'i', 's', 0, 0, 0, 0, 0, 0, 0, 0, 1
    };

    private static byte[] fixture(String name) throws IOException {
        try (InputStream input = OggUtilTest.class.getResourceAsStream("/test/" + name)) {
            return input.readAllBytes();
        }
    }

    private static ByteBuffer fields(byte[] bytes) {
        return ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN);
    }

    // Independent Ogg page/packet inspection also verifies output checksums and
    // sequence numbers, instead of relying on the library used by the utility.
    private static class OggFile {
        final List<byte[]> packets = new ArrayList<>();
        final List<byte[]> audioPages = new ArrayList<>();
        final int serial;

        OggFile(byte[] data) {
            serial = fields(data).getInt(14);
            ByteArrayOutputStream packet = new ByteArrayOutputStream();
            int offset = 0;
            int sequence = 0;
            while (offset < data.length) {
                assertEquals("OggS", new String(data, offset, 4, StandardCharsets.US_ASCII));
                int segments = data[offset + 26] & 0xff;
                int headerSize = 27 + segments;
                int size = headerSize;
                for (int i = 0; i < segments; i++) {
                    size += data[offset + 27 + i] & 0xff;
                }
                byte[] page = Arrays.copyOfRange(data, offset, offset + size);
                assertEquals(sequence++, fields(page).getInt(18));
                assertEquals(serial, fields(page).getInt(14));
                assertEquals(fields(page).getInt(22), checksum(page));
                if (packets.size() >= 3) {
                    audioPages.add(page);
                }
                int position = offset + headerSize;
                for (int i = 0; i < segments; i++) {
                    int length = data[offset + 27 + i] & 0xff;
                    packet.write(data, position, length);
                    position += length;
                    if (length < 255) {
                        packets.add(packet.toByteArray());
                        packet.reset();
                    }
                }
                offset += size;
            }
            assertEquals(data.length, offset);
            assertEquals(0, packet.size());
        }
    }

    private static int checksum(byte[] page) {
        int crc = 0;
        for (int i = 0; i < page.length; i++) {
            crc ^= (i >= 22 && i < 26 ? 0 : page[i] & 0xff) << 24;
            for (int bit = 0; bit < 8; bit++) {
                crc = (crc << 1) ^ (crc < 0 ? 0x04c11db7 : 0);
            }
        }
        return crc;
    }

    private static byte[] renumber(byte[] original, int sequence) {
        byte[] page = original.clone();
        fields(page).putInt(18, sequence).putInt(22, checksum(page));
        return page;
    }

    private static int writeHeader(ByteArrayOutputStream output, byte[] packet, int serial, int sequence) {
        int segmentCount = packet.length / 255 + 1;
        for (int startSegment = 0; startSegment < segmentCount; startSegment += 255) {
            int segments = Math.min(255, segmentCount - startSegment);
            int offset = startSegment * 255;
            int length = Math.min(segments * 255, packet.length - offset);
            boolean lastPage = startSegment + segments == segmentCount;
            byte[] page = new byte[27 + segments + length];
            ByteBuffer fields = fields(page);
            fields.put(new byte[]{'O', 'g', 'g', 'S', 0});
            fields.put((byte)(sequence == 0 ? 2 : startSegment > 0 ? 1 : 0));
            fields.putLong(lastPage ? 0 : -1).putInt(serial).putInt(sequence++).putInt(0).put((byte)segments);
            for (int i = 0; i < segments; i++) {
                fields.put((byte)Math.min(255, packet.length - offset - 255 * i));
            }
            fields.put(packet, offset, length);
            fields.putInt(22, checksum(page));
            output.write(page, 0, page.length);
        }
        return sequence;
    }

    private static byte[] withComments(byte[] source, byte[] vendor, byte[] comment) {
        OggFile original = new OggFile(source);
        byte[] comments = new byte[20 + vendor.length + comment.length];
        ByteBuffer fields = fields(comments);
        fields.put(EMPTY_COMMENTS, 0, 7).putInt(vendor.length).put(vendor);
        fields.putInt(1).putInt(comment.length).put(comment).put((byte)1);
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        int sequence = writeHeader(output, original.packets.get(0), original.serial, 0);
        sequence = writeHeader(output, comments, original.serial, sequence);
        sequence = writeHeader(output, original.packets.get(2), original.serial, sequence);
        for (byte[] page : original.audioPages) {
            byte[] renumbered = renumber(page, sequence++);
            output.write(renumbered, 0, renumbered.length);
        }
        return output.toByteArray();
    }

    private static void assertAudioUnchanged(byte[] originalBytes, byte[] strippedBytes) {
        OggFile original = new OggFile(originalBytes);
        OggFile stripped = new OggFile(strippedBytes);
        assertEquals(original.packets.size(), stripped.packets.size());
        for (int i = 0; i < original.packets.size(); i++) {
            assertArrayEquals("packet " + i, i == 1 ? EMPTY_COMMENTS : original.packets.get(i), stripped.packets.get(i));
        }
        assertEquals(original.audioPages.size(), stripped.audioPages.size());
        for (int i = 0; i < original.audioPages.size(); i++) {
            // Only sequence number and the consequent CRC may change.
            byte[] page = original.audioPages.get(i);
            byte[] strippedPage = stripped.audioPages.get(i);
            assertArrayEquals(renumber(page, fields(strippedPage).getInt(18)), strippedPage);
        }
    }

    // Guards against losing or re-encoding audio when removing multi-page artwork and vendor strings (#11729).
    @Test
    public void testLargeMetadata() throws Exception {
        byte[] vendor = "encoder".repeat(20000).getBytes(StandardCharsets.UTF_8);
        byte[] comment = ("METADATA_BLOCK_PICTURE=" + "A".repeat(315000)).getBytes(StandardCharsets.UTF_8);
        byte[] source = withComments(fixture("test.ogg"), vendor, comment);
        byte[] original = source.clone();
        byte[] stripped = OggUtil.stripMetadata(source);
        assertTrue(source.length - stripped.length > 450000);
        assertArrayEquals(original, source);
        assertAudioUnchanged(source, stripped);
        assertArrayEquals(stripped, OggUtil.stripMetadata(stripped));
    }

    // Verifies shared header pages and exact lacing/page boundaries, including zero-length terminating segments.
    @Test
    public void testHeaderBoundaries() throws Exception {
        byte[] source = fixture("test.ogg");
        assertAudioUnchanged(source, OggUtil.stripMetadata(source));
        for (int commentPacketSize : new int[]{20, 254, 255, 256, 65024, 65025, 65026}) {
            byte[] comment = new byte[commentPacketSize - 20];
            Arrays.fill(comment, (byte)'A');
            source = withComments(fixture("test.ogg"), new byte[0], comment);
            assertAudioUnchanged(source, OggUtil.stripMetadata(source));
        }
    }

    // Preserves the engine's glockenspiel.opus fixture renamed to .ogg, including its metadata and page framing.
    @Test
    public void testOpusPreserved() throws Exception {
        byte[] source = fixture("opus-audio.ogg");
        byte[] original = source.clone();
        assertArrayEquals(original, OggUtil.stripMetadata(source));
        assertArrayEquals(original, source);
    }

    private static void assertInvalid(byte[] source) throws Exception {
        try {
            OggUtil.stripMetadata(source);
            fail("Expected an invalid Ogg file to fail");
        } catch (IOException expected) {
            assertFalse(expected.getMessage().isEmpty());
        }
    }

    // Rejects corrupt/truncated Ogg framing in both codecs, including the Opus path that preserves the input bytes.
    @Test
    public void testInvalidInput() throws Exception {
        assertInvalid(new byte[0]);
        assertInvalid("not an ogg".getBytes(StandardCharsets.UTF_8));
        for (String name : new String[]{"test.ogg", "opus-audio.ogg"}) {
            byte[] source = fixture(name);
            assertInvalid(Arrays.copyOf(source, source.length - 1));
            assertInvalid(Arrays.copyOf(source, 58));
            byte[] corrupted = source.clone();
            corrupted[corrupted.length - 1] ^= 1;
            assertInvalid(corrupted);
            byte[] trailing = Arrays.copyOf(source, source.length + 1);
            assertInvalid(trailing);
            byte[] chained = Arrays.copyOf(source, 2 * source.length);
            System.arraycopy(source, 0, chained, source.length, source.length);
            assertInvalid(chained);
        }
    }

    // Guards against shared codec and native reader/writer state when compiling Vorbis and Opus concurrently.
    @Test
    public void testConcurrentCalls() throws Exception {
        byte[][] sources = {fixture("test.ogg"), fixture("opus-audio.ogg")};
        byte[][] expected = {OggUtil.stripMetadata(sources[0]), sources[1]};
        var executor = Executors.newFixedThreadPool(4);
        try {
            List<Callable<byte[]>> tasks = new ArrayList<>();
            for (int i = 0; i < 16; i++) {
                byte[] source = sources[i % sources.length];
                tasks.add(() -> OggUtil.stripMetadata(source));
            }
            List<Future<byte[]>> results = executor.invokeAll(tasks);
            for (int i = 0; i < results.size(); i++) {
                assertArrayEquals(expected[i % expected.length], results.get(i).get());
            }
        } finally {
            executor.shutdownNow();
        }
    }
}
