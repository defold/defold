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

import java.io.ByteArrayOutputStream;
import java.io.Closeable;
import java.io.File;
import java.io.IOException;
import java.lang.ref.Reference;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.file.Files;
import java.util.Arrays;
import java.util.List;

import com.dynamo.bob.Bob;
import com.dynamo.bob.Platform;
import com.sun.jna.Callback;
import com.sun.jna.Memory;
import com.sun.jna.Native;
import com.sun.jna.NativeLibrary;
import com.sun.jna.NativeLong;
import com.sun.jna.Pointer;
import com.sun.jna.Structure;

public final class OggUtil {
    private static final NativeLong ALL_STREAMS = new NativeLong(-1);
    private static final int OGGZ_READ = 0;
    private static final int OGGZ_WRITE = 1;
    private static final int OGGZ_PREFIX = 0x40;
    private static final int OGGZ_FLUSH_AFTER = 2;
    private static final int OGGZ_STOP_ERR = -1;
    private static final byte[] VORBIS_IDENTIFICATION = {1, 'v', 'o', 'r', 'b', 'i', 's'};
    private static final byte[] OPUS_IDENTIFICATION = {'O', 'p', 'u', 's', 'H', 'e', 'a', 'd'};
    // Vorbis comment packet: signature, empty vendor, zero comments, framing bit.
    private static final byte[] EMPTY_COMMENTS = {
        3, 'v', 'o', 'r', 'b', 'i', 's', 0, 0, 0, 0, 0, 0, 0, 0, 1
    };

    private enum Codec {
        VORBIS, OPUS
    }

    private OggUtil() {
    }

    private static NativeLibrary loadLibrary(String name) {
        try {
            Bob.init();
            Platform platform = Platform.getHostPlatform();
            // These libraries accompany oggz-validate in libexec, including the
            // "lib" prefix on Windows. Use Bob's atomic extraction/cache as well
            // when called directly from the Editor, before Bob has unpacked tools.
            Files.createDirectories(new File(Bob.getRootFolder(), platform.getPair()).toPath());
            String path = platform.getPair() + "/lib" + name + platform.getLibSuffix();
            return NativeLibrary.getInstance(Bob.getLibExecPath(path));
        } catch (IOException e) {
            throw new ExceptionInInitializerError(e);
        }
    }

    private static class Ogg {
        static {
            Native.register(Ogg.class, loadLibrary("ogg"));
        }

        static void initialize() {
        }

        static native void ogg_page_checksum_set(OggPage page);
    }

    private static class Oggz {
        static {
            // Preload the dependency by its absolute path, including on Windows.
            Ogg.initialize();
            Native.register(Oggz.class, loadLibrary("oggz"));
        }

        static native Pointer oggz_new(int flags);
        static native int oggz_close(Pointer oggz);
        static native int oggz_set_read_callback(Pointer oggz, NativeLong serial, ReadPacket callback, Pointer userData);
        static native int oggz_set_read_page(Pointer oggz, NativeLong serial, ReadPage callback, Pointer userData);
        static native NativeLong oggz_read_input(Pointer oggz, byte[] data, NativeLong size);
        static native int oggz_write_feed(Pointer oggz, Pointer packet, NativeLong serial, int flush, Pointer guard);
        static native NativeLong oggz_write_output(Pointer oggz, byte[] data, NativeLong size);
        static native NativeLong oggz_write_get_next_page_size(Pointer oggz);
    }

    private interface ReadPacket extends Callback {
        int invoke(Pointer oggz, Pointer packet, NativeLong serial, Pointer userData);
    }

    private interface ReadPage extends Callback {
        int invoke(Pointer oggz, Pointer page, NativeLong serial, Pointer userData);
    }

    // Native C long is 32 bits on Windows and 64 bits on macOS/Linux. The
    // granule position and packet number are explicitly 64 bits on every host.
    public static class OggPacket extends Structure {
        public Pointer packet;
        public NativeLong bytes;
        public NativeLong bos;
        public NativeLong eos;
        public long granule;
        public long number;

        public OggPacket() {
        }

        public OggPacket(Pointer pointer) {
            super(pointer);
            read();
        }

        @Override
        protected List<String> getFieldOrder() {
            return Arrays.asList("packet", "bytes", "bos", "eos", "granule", "number");
        }
    }

    public static class OggPage extends Structure {
        public Pointer header;
        public NativeLong headerLength;
        public Pointer body;
        public NativeLong bodyLength;

        public OggPage() {
        }

        public OggPage(Pointer pointer) {
            super(pointer);
            read();
        }

        @Override
        protected List<String> getFieldOrder() {
            return Arrays.asList("header", "headerLength", "body", "bodyLength");
        }
    }

    private static void checkResult(long result, String operation) throws IOException {
        if (result < 0) {
            throw new IOException("Ogg " + operation + " failed (liboggz error " + result + ").");
        }
    }

    private static boolean hasSignature(OggPacket packet, byte[] signature) {
        return packet.bytes.longValue() >= signature.length &&
                Arrays.equals(signature, packet.packet.getByteArray(0, signature.length));
    }

    /**
     * Remove the vendor string and user comments (including embedded artwork)
     * from a single-stream Ogg Vorbis file. The identification/setup packets,
     * audio packets, audio page boundaries and granule positions are preserved.
     * Ogg Opus files are validated and returned unchanged, including metadata.
     * The codec is detected from the first packet, independent of the extension.
     *
     * @param source the complete source .ogg file; it is not modified
     * @return the compiled .oggc bytes, or the original source for Opus, without re-encoding audio
     * @throws IOException if the Ogg framing or Vorbis headers are malformed,
     *         the input is not a single Vorbis or Opus stream, or the bundled
     *         native libraries cannot be loaded
     */
    public static byte[] stripMetadata(byte[] source) throws IOException {
        try (Rewriter rewriter = new Rewriter(source.length)) {
            return rewriter.rewrite(source);
        } catch (LinkageError e) {
            throw new IOException("Could not load the bundled Ogg libraries.", e);
        }
    }

    private static class Rewriter implements Closeable {
        private final int sourceSize;
        private final ReadPacket readPacket = this::readPacket;
        private final ReadPage readPage = this::readPage;
        private ByteArrayOutputStream output;
        private byte[] outputBuffer;
        private HeaderBuffer headerBuffer;
        private Pointer reader;
        private Pointer writer;
        private IOException failure;
        private Codec codec;
        private byte[] unidentifiedPageHeader;
        private long serial;
        private long inputBytes;
        private int inputPages;
        private int outputPages;
        private int headerPackets;
        private boolean eos;

        Rewriter(int size) throws IOException {
            sourceSize = size;
            reader = Oggz.oggz_new(OGGZ_READ);
            if (reader == null) {
                throw new IOException("Could not allocate the Ogg reader.");
            }
        }

        byte[] rewrite(byte[] source) throws IOException {
            checkResult(Oggz.oggz_set_read_callback(reader, ALL_STREAMS, readPacket, null), "packet callback");
            checkResult(Oggz.oggz_set_read_page(reader, ALL_STREAMS, readPage, null), "page callback");
            long consumed = Oggz.oggz_read_input(reader, source, new NativeLong(source.length)).longValue();
            if (failure != null) {
                throw failure;
            }
            checkResult(consumed, "read");
            // libogg skips damaged pages and trailing partial pages. Never turn
            // that recovery into a successful, silently truncated build output.
            if (consumed != source.length || inputBytes != source.length || codec == null || !eos ||
                    (codec == Codec.VORBIS && headerPackets != 3)) {
                throw new IOException("Invalid or truncated Ogg audio file.");
            }
            return codec == Codec.OPUS ? source : output.toByteArray();
        }

        private int readPage(Pointer oggz, Pointer pointer, NativeLong streamSerial, Pointer userData) {
            try {
                OggPage page = new OggPage(pointer);
                byte[] header = page.header.getByteArray(0, page.headerLength.intValue());
                ByteBuffer fields = ByteBuffer.wrap(header).order(ByteOrder.LITTLE_ENDIAN);
                if (inputPages == 0) {
                    serial = streamSerial.longValue();
                }
                if (eos || serial != streamSerial.longValue()) {
                    throw new IOException("Only a single Ogg Vorbis or Opus stream is supported.");
                }
                if (header[4] != 0 || fields.getInt(18) != inputPages) {
                    throw new IOException("Invalid Ogg page version or sequence number.");
                }
                inputPages++;
                inputBytes += page.headerLength.longValue() + page.bodyLength.longValue();
                eos = (header[5] & 4) != 0;

                if (codec == null) {
                    // Page callbacks precede packet callbacks. Defer Vorbis's
                    // header layout checks until the first packet identifies it.
                    unidentifiedPageHeader = header;
                } else if (codec == Codec.OPUS) {
                    // Keep reading every page to validate framing, checksums and
                    // EOF, but preserve the complete source without rewriting it.
                    return 0;
                } else if (headerPackets < 3) {
                    checkVorbisHeaderPage(header);
                } else {
                    fields.putInt(18, outputPages++);
                    // Copy the header before changing it: liboggz still needs
                    // the original sequence number when it consumes this page.
                    headerBuffer.write(0, header, 0, header.length);
                    OggPage copiedPage = new OggPage();
                    copiedPage.header = headerBuffer;
                    copiedPage.headerLength = page.headerLength;
                    copiedPage.body = page.body;
                    copiedPage.bodyLength = page.bodyLength;
                    Ogg.ogg_page_checksum_set(copiedPage);
                    output.write(headerBuffer.getByteArray(0, header.length));
                    output.write(page.body.getByteArray(0, page.bodyLength.intValue()));
                }
                return 0;
            } catch (IOException | RuntimeException e) {
                failure = new IOException("Could not rewrite Ogg pages: " + e.getMessage(), e);
                return OGGZ_STOP_ERR;
            }
        }

        private void checkVorbisHeaderPage(byte[] header) throws IOException {
            int completed = 0;
            for (int i = 27; i < header.length; i++) {
                if ((header[i] & 0xff) < 255) {
                    completed++;
                }
            }
            if (headerPackets + completed > 3 || eos ||
                    (headerPackets + completed == 3 && (header[header.length - 1] & 0xff) == 255)) {
                throw new IOException("Vorbis headers must end on a separate page before the audio.");
            }
        }

        private int readPacket(Pointer oggz, Pointer pointer, NativeLong streamSerial, Pointer userData) {
            try {
                OggPacket packet = new OggPacket(pointer);
                if (codec == null) {
                    if (packet.number != 0 || packet.bos.longValue() == 0) {
                        throw new IOException("Invalid Ogg identification packet.");
                    }
                    // liboggz 1.1.1 predates Opus. Inspect the packet signature
                    // directly instead of relying on its content-type detection.
                    if (hasSignature(packet, OPUS_IDENTIFICATION)) {
                        if (packet.bytes.longValue() < 19 || packet.eos.longValue() != 0) {
                            throw new IOException("Invalid Opus identification header.");
                        }
                        codec = Codec.OPUS;
                        unidentifiedPageHeader = null;
                        checkResult(Oggz.oggz_set_read_callback(reader, ALL_STREAMS, null, null), "remove packet callback");
                        return 0;
                    }
                    if (!hasSignature(packet, VORBIS_IDENTIFICATION)) {
                        throw new IOException("Expected an Ogg Vorbis or Opus identification header.");
                    }
                    codec = Codec.VORBIS;
                    checkVorbisHeaderPage(unidentifiedPageHeader);
                    unidentifiedPageHeader = null;
                    // Defer rewrite allocations so Opus validation does not duplicate the input.
                    writer = Oggz.oggz_new(OGGZ_WRITE | OGGZ_PREFIX);
                    if (writer == null) {
                        throw new IOException("Could not allocate the Ogg writer.");
                    }
                    output = new ByteArrayOutputStream(sourceSize);
                    outputBuffer = new byte[65307];
                    headerBuffer = new HeaderBuffer();
                }
                byte[] signature = { (byte)(1 + 2 * headerPackets), 'v', 'o', 'r', 'b', 'i', 's' };
                if (!hasSignature(packet, signature) || packet.number != headerPackets) {
                    throw new IOException("Expected the Vorbis identification, comment and setup headers.");
                }

                if (headerPackets == 1) {
                    // The bundled Windows liboggz exports the core read/write
                    // API, but not the comment-generation helpers. Feed a
                    // minimal comment packet; liboggz handles its Ogg framing.
                    headerBuffer.write(0, EMPTY_COMMENTS, 0, EMPTY_COMMENTS.length);
                    OggPacket comments = new OggPacket();
                    comments.packet = headerBuffer;
                    comments.bytes = new NativeLong(EMPTY_COMMENTS.length);
                    comments.bos = new NativeLong(0);
                    comments.eos = new NativeLong(0);
                    comments.number = 1;
                    comments.write();
                    // A null guard tells liboggz to copy the packet data.
                    checkResult(Oggz.oggz_write_feed(writer, comments.getPointer(), streamSerial, 0, null), "write comments");
                } else {
                    checkResult(Oggz.oggz_write_feed(writer, pointer, streamSerial, OGGZ_FLUSH_AFTER, null), "write header");
                }

                if (++headerPackets == 3) {
                    writeHeaders();
                    checkResult(Oggz.oggz_set_read_callback(reader, ALL_STREAMS, null, null), "remove packet callback");
                }
                return 0;
            } catch (IOException | RuntimeException e) {
                failure = new IOException("Could not process Ogg headers: " + e.getMessage(), e);
                return OGGZ_STOP_ERR;
            }
        }

        private void writeHeaders() throws IOException {
            // Read one byte to make the writer prepare a page, then drain that
            // page. Counting these pages lets us renumber the copied audio pages.
            long size;
            while ((size = Oggz.oggz_write_output(writer, outputBuffer, new NativeLong(1)).longValue()) > 0) {
                output.write(outputBuffer[0]);
                int remaining = Oggz.oggz_write_get_next_page_size(writer).intValue();
                checkResult(remaining, "header page size");
                long written = Oggz.oggz_write_output(writer, outputBuffer, new NativeLong(remaining)).longValue();
                if (written != remaining) {
                    throw new IOException("Could not write the complete Ogg header page.");
                }
                output.write(outputBuffer, 0, remaining);
                outputPages++;
            }
            checkResult(size, "write headers");
        }

        @Override
        public void close() {
            if (reader != null) {
                Oggz.oggz_close(reader);
                reader = null;
            }
            if (writer != null) {
                Oggz.oggz_close(writer);
                writer = null;
            }
            if (headerBuffer != null) {
                headerBuffer.release();
                headerBuffer = null;
            }
            // Keep callbacks strongly reachable until the native reader is closed.
            Reference.reachabilityFence(this);
        }
    }

    // The Editor also supports JNA 4, whose Memory does not implement Closeable.
    private static class HeaderBuffer extends Memory {
        HeaderBuffer() {
            super(282);
        }

        @SuppressWarnings("deprecation") // Also works with the Editor's JNA 4.
        void release() {
            super.dispose();
        }
    }
}
