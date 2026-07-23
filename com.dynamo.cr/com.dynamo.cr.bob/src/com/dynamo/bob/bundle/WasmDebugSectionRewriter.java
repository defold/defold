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

package com.dynamo.bob.bundle;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;

/**
 * Rewrites the debug info references embedded in a wasm binary.
 *
 * Emscripten embeds the names of the debug info sidecar files into the wasm
 * (external_debug_info for the separate DWARF file produced by -gseparate-dwarf,
 * sourceMappingURL for the source map produced by -gsource-map). Browser devtools
 * resolve these references relative to the URL the wasm module was fetched from.
 * Since bundling renames the engine binary (dmengine.wasm -> <ProjectTitle>.wasm),
 * the embedded references must be rewritten to match the sidecar files actually
 * served next to the renamed wasm.
 *
 * Binary layout references:
 * - module and section framing (custom section = id 0, name + payload):
 *   https://webassembly.github.io/spec/core/binary/modules.html#custom-section
 * - external_debug_info and sourceMappingURL section conventions
 *   (both contain a single LEB-length-prefixed URL string):
 *   https://github.com/WebAssembly/tool-conventions/blob/main/Debugging.md
 * - LEB128 integer encoding:
 *   https://webassembly.github.io/spec/core/binary/values.html#integers
 */
public class WasmDebugSectionRewriter {

    public static final String EXTERNAL_DEBUG_INFO = "external_debug_info";
    public static final String SOURCE_MAPPING_URL  = "sourceMappingURL";

    private static final byte[] WASM_MAGIC   = {0x00, 0x61, 0x73, 0x6d}; // "\0asm"
    private static final byte[] WASM_VERSION = {0x01, 0x00, 0x00, 0x00};
    private static final int SECTION_CUSTOM = 0;
    private static final int SECTION_CODE   = 10;

    /**
     * Copies the wasm binary from in to out, removing any existing
     * external_debug_info / sourceMappingURL custom sections and appending
     * replacements at the end of the module (the same placement emscripten uses).
     * Pass null for a url to only strip the corresponding section. The rewrite
     * only moves bytes located after the code section, so the byte offsets used
     * by DWARF info and wasm source maps remain valid.
     */
    public static void rewrite(File in, File out, String dwarfUrl, String sourceMapUrl) throws IOException {
        try (InputStream is = new BufferedInputStream(new FileInputStream(in));
             OutputStream os = new BufferedOutputStream(new FileOutputStream(out))) {

            byte[] magic = readBytes(is, 4, "wasm header");
            byte[] version = readBytes(is, 4, "wasm header");
            if (!Arrays.equals(magic, WASM_MAGIC) || !Arrays.equals(version, WASM_VERSION)) {
                throw new IOException(String.format("%s is not a wasm (version 1) binary", in));
            }
            os.write(magic);
            os.write(version);

            boolean codeSectionSeen = false;
            int id;
            while ((id = is.read()) != -1) {
                Leb size = readLeb(is);
                if (id == SECTION_CUSTOM) {
                    Leb nameLen = readLeb(is);
                    if (nameLen.size + nameLen.value > size.value) {
                        throw new IOException(String.format("Malformed custom section in %s: name is larger than the section", in));
                    }
                    byte[] name = readBytes(is, (int)nameLen.value, "custom section name");
                    long remaining = size.value - nameLen.size - name.length;
                    String nameStr = new String(name, StandardCharsets.UTF_8);
                    if (nameStr.equals(EXTERNAL_DEBUG_INFO) || nameStr.equals(SOURCE_MAPPING_URL)) {
                        // Debug reference sections always trail the code section in
                        // emscripten output. Bytes before the code section must not
                        // move: wasm source map addresses and DWARF references are
                        // offsets from the start of the file.
                        if (!codeSectionSeen) {
                            throw new IOException(String.format("Unexpected %s section before the code section in %s", nameStr, in));
                        }
                        skipBytes(is, remaining, nameStr);
                        continue; // drop the section
                    }
                    os.write(id);
                    os.write(size.raw, 0, size.size); // keep the original (possibly padded) size encoding
                    os.write(nameLen.raw, 0, nameLen.size);
                    os.write(name);
                    copyBytes(is, os, remaining, nameStr);
                } else {
                    if (id == SECTION_CODE) {
                        codeSectionSeen = true;
                    }
                    os.write(id);
                    os.write(size.raw, 0, size.size);
                    copyBytes(is, os, size.value, String.format("section id %d", id));
                }
            }

            // Same order as emscripten emits them: sourceMappingURL, then external_debug_info
            if (sourceMapUrl != null) {
                writeUrlSection(os, SOURCE_MAPPING_URL, sourceMapUrl);
            }
            if (dwarfUrl != null) {
                writeUrlSection(os, EXTERNAL_DEBUG_INFO, dwarfUrl);
            }
        }
    }

    private static void writeUrlSection(OutputStream os, String name, String url) throws IOException {
        byte[] nameBytes = name.getBytes(StandardCharsets.UTF_8);
        byte[] urlBytes = url.getBytes(StandardCharsets.UTF_8);
        byte[] nameLen = writeLeb(nameBytes.length);
        byte[] urlLen = writeLeb(urlBytes.length);
        os.write(SECTION_CUSTOM);
        os.write(writeLeb(nameLen.length + nameBytes.length + urlLen.length + urlBytes.length));
        os.write(nameLen);
        os.write(nameBytes);
        os.write(urlLen);
        os.write(urlBytes);
    }

    private static class Leb {
        long value;
        byte[] raw = new byte[5];
        int size;
    }

    // Unsigned LEB128, at most 5 bytes for u32. Non-minimal (padded) encodings are
    // valid and used by binaryen for section sizes, so the raw bytes are kept for
    // verbatim copies.
    private static Leb readLeb(InputStream is) throws IOException {
        Leb leb = new Leb();
        for (int shift = 0; ; shift += 7) {
            int b = is.read();
            if (b == -1) {
                throw new IOException("Truncated wasm binary: unexpected end of file in a LEB128 value");
            }
            if (leb.size == 5 || (leb.size == 4 && (b & 0x70) != 0)) {
                throw new IOException("Malformed wasm binary: LEB128 value does not fit in 32 bits");
            }
            leb.raw[leb.size++] = (byte)b;
            leb.value |= (long)(b & 0x7f) << shift;
            if ((b & 0x80) == 0) {
                return leb;
            }
        }
    }

    private static byte[] writeLeb(long value) {
        byte[] out = new byte[5];
        int size = 0;
        do {
            int b = (int)(value & 0x7f);
            value >>>= 7;
            out[size++] = (byte)(value != 0 ? (b | 0x80) : b);
        } while (value != 0);
        return Arrays.copyOf(out, size);
    }

    private static byte[] readBytes(InputStream is, int count, String what) throws IOException {
        byte[] bytes = is.readNBytes(count);
        if (bytes.length != count) {
            throw new IOException(String.format("Truncated wasm binary: unexpected end of file in %s", what));
        }
        return bytes;
    }

    // read-and-discard instead of InputStream.skip, which can silently seek past
    // the end of a file
    private static void skipBytes(InputStream is, long count, String what) throws IOException {
        byte[] buffer = new byte[8 * 1024];
        long skipped = 0;
        while (skipped < count) {
            int n = is.read(buffer, 0, (int)Math.min(buffer.length, count - skipped));
            if (n == -1) {
                throw new IOException(String.format("Truncated wasm binary: unexpected end of file in %s", what));
            }
            skipped += n;
        }
    }

    private static void copyBytes(InputStream is, OutputStream os, long count, String what) throws IOException {
        byte[] buffer = new byte[64 * 1024];
        long copied = 0;
        while (copied < count) {
            int n = is.read(buffer, 0, (int)Math.min(buffer.length, count - copied));
            if (n == -1) {
                throw new IOException(String.format("Truncated wasm binary: unexpected end of file in %s", what));
            }
            os.write(buffer, 0, n);
            copied += n;
        }
    }
}
