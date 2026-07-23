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

package com.dynamo.bob.bundle.test;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.fail;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

import com.dynamo.bob.bundle.WasmDebugSectionRewriter;

public class WasmDebugSectionRewriterTest {

    @Rule
    public TemporaryFolder tmp = new TemporaryFolder();

    private static final byte[] HEADER = {0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00};

    private static byte[] leb(long value) {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        do {
            int b = (int)(value & 0x7f);
            value >>>= 7;
            out.write(value != 0 ? (b | 0x80) : b);
        } while (value != 0);
        return out.toByteArray();
    }

    // non-minimal encoding padded with continuation bytes, as emitted by binaryen
    private static byte[] lebPadded(long value, int size) {
        byte[] out = new byte[size];
        for (int i = 0; i < size; ++i) {
            out[i] = (byte)((value & 0x7f) | (i < size - 1 ? 0x80 : 0));
            value >>>= 7;
        }
        return out;
    }

    private static byte[] concat(byte[]... arrays) throws IOException {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        for (byte[] array : arrays) {
            out.write(array);
        }
        return out.toByteArray();
    }

    private static byte[] section(int id, byte[] sizeEncoding, byte[] payload) throws IOException {
        return concat(new byte[] {(byte)id}, sizeEncoding, payload);
    }

    private static byte[] section(int id, byte[] payload) throws IOException {
        return section(id, leb(payload.length), payload);
    }

    private static byte[] customSection(String name, byte[] content) throws IOException {
        byte[] nameBytes = name.getBytes(StandardCharsets.UTF_8);
        return section(0, concat(leb(nameBytes.length), nameBytes, content));
    }

    private static byte[] urlSection(String name, String url) throws IOException {
        byte[] urlBytes = url.getBytes(StandardCharsets.UTF_8);
        return customSection(name, concat(leb(urlBytes.length), urlBytes));
    }

    // minimal valid-enough module prefix: type section (id 1) + code section (id 10)
    private static byte[] modulePrefix() throws IOException {
        return concat(HEADER,
                      section(1, new byte[] {0x00}),
                      section(10, new byte[] {0x00}));
    }

    private byte[] rewrite(byte[] input, String dwarfUrl, String sourceMapUrl) throws IOException {
        File in = tmp.newFile();
        File out = tmp.newFile();
        Files.write(in.toPath(), input);
        WasmDebugSectionRewriter.rewrite(in, out, dwarfUrl, sourceMapUrl);
        return Files.readAllBytes(out.toPath());
    }

    private void assertRewriteThrows(byte[] input, String dwarfUrl, String sourceMapUrl) throws IOException {
        try {
            rewrite(input, dwarfUrl, sourceMapUrl);
            fail("Expected an IOException");
        } catch (IOException e) {
            // expected
        }
    }

    @Test
    public void testRewrite() throws IOException {
        byte[] input = concat(modulePrefix(),
                              urlSection("sourceMappingURL", "dmengine.wasm.map"),
                              urlSection("external_debug_info", "dmengine.wasm.debug.wasm"));
        byte[] expected = concat(modulePrefix(),
                                 urlSection("sourceMappingURL", "Game.wasm.map"),
                                 urlSection("external_debug_info", "Game.wasm.debug.wasm"));
        assertArrayEquals(expected, rewrite(input, "Game.wasm.debug.wasm", "Game.wasm.map"));
    }

    @Test
    public void testRewriteIsIdempotent() throws IOException {
        byte[] input = concat(modulePrefix(),
                              urlSection("external_debug_info", "dmengine.wasm.debug.wasm"));
        byte[] once = rewrite(input, "Game.wasm.debug.wasm", "Game.wasm.map");
        assertArrayEquals(once, rewrite(once, "Game.wasm.debug.wasm", "Game.wasm.map"));
    }

    @Test
    public void testStripOnly() throws IOException {
        byte[] input = concat(modulePrefix(),
                              urlSection("sourceMappingURL", "dmengine.wasm.map"),
                              customSection("name", new byte[] {0x01, 0x02, 0x03}),
                              urlSection("external_debug_info", "dmengine.wasm.debug.wasm"));
        // unrelated custom sections (e.g. the name section) survive in place
        byte[] expected = concat(modulePrefix(),
                                 customSection("name", new byte[] {0x01, 0x02, 0x03}));
        assertArrayEquals(expected, rewrite(input, null, null));
    }

    @Test
    public void testAppendWhenAbsent() throws IOException {
        byte[] input = modulePrefix();
        byte[] expected = concat(modulePrefix(),
                                 urlSection("sourceMappingURL", "Game.wasm.map"),
                                 urlSection("external_debug_info", "Game.wasm.debug.wasm"));
        assertArrayEquals(expected, rewrite(input, "Game.wasm.debug.wasm", "Game.wasm.map"));
    }

    @Test
    public void testDuplicateSectionsAreAllDropped() throws IOException {
        byte[] input = concat(modulePrefix(),
                              urlSection("external_debug_info", "a.wasm.debug.wasm"),
                              urlSection("external_debug_info", "b.wasm.debug.wasm"));
        byte[] expected = concat(modulePrefix(),
                                 urlSection("external_debug_info", "Game.wasm.debug.wasm"));
        assertArrayEquals(expected, rewrite(input, "Game.wasm.debug.wasm", null));
    }

    @Test
    public void testPaddedSectionSizesAreCopiedVerbatim() throws IOException {
        byte[] codePayload = new byte[] {0x00};
        byte[] namePayload = concat(leb(4), "name".getBytes(StandardCharsets.UTF_8), new byte[] {0x7f});
        byte[] input = concat(HEADER,
                              section(1, new byte[] {0x00}),
                              section(10, lebPadded(codePayload.length, 5), codePayload),
                              section(0, lebPadded(namePayload.length, 5), namePayload),
                              urlSection("external_debug_info", "dmengine.wasm.debug.wasm"));
        byte[] expected = concat(HEADER,
                                 section(1, new byte[] {0x00}),
                                 section(10, lebPadded(codePayload.length, 5), codePayload),
                                 section(0, lebPadded(namePayload.length, 5), namePayload),
                                 urlSection("external_debug_info", "Game.wasm.debug.wasm"));
        assertArrayEquals(expected, rewrite(input, "Game.wasm.debug.wasm", null));
    }

    @Test
    public void testTruncatedInput() throws IOException {
        byte[] full = concat(modulePrefix(),
                             urlSection("external_debug_info", "dmengine.wasm.debug.wasm"));
        // cut into the middle of the last section's payload
        byte[] truncated = new byte[full.length - 5];
        System.arraycopy(full, 0, truncated, 0, truncated.length);
        assertRewriteThrows(truncated, null, null);
    }

    @Test
    public void testDebugSectionBeforeCodeSection() throws IOException {
        byte[] input = concat(HEADER,
                              urlSection("external_debug_info", "dmengine.wasm.debug.wasm"),
                              section(10, new byte[] {0x00}));
        assertRewriteThrows(input, "Game.wasm.debug.wasm", null);
    }

    @Test
    public void testNotAWasmBinary() throws IOException {
        assertRewriteThrows("not a wasm binary".getBytes(StandardCharsets.UTF_8), null, null);
    }
}
