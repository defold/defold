// Copyright 2020-2026 The Defold Foundation
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
// You may obtain a copy of the License at https://www.defold.com/license

package com.dynamo.bob.util;

import java.io.IOException;
import java.util.Arrays;

import com.dynamo.bob.Bob;
import com.sun.jna.Native;

public final class LZ4 {
    static {
        try {
            Native.register(LZ4.class, Bob.getSharedLib("dlib_shared").getAbsolutePath());
        } catch (IOException e) {
            throw new ExceptionInInitializerError(e);
        }
    }

    private LZ4() {
    }

    private static native int LZ4MaxCompressedSize(int size, int[] maxCompressedSize);
    private static native int LZ4CompressBuffer(byte[] buffer, int size, byte[] compressed, int[] compressedSize);

    /** Compress a raw LZ4 block using the engine's high-compression settings. */
    public static byte[] compress(byte[] buffer) {
        int[] size = new int[1];
        int result = LZ4MaxCompressedSize(buffer.length, size);
        if (result != 0) {
            throw new IllegalArgumentException("LZ4 input is too large: " + buffer.length);
        }
        byte[] compressed = new byte[size[0]];
        result = LZ4CompressBuffer(buffer, buffer.length, compressed, size);
        if (result != 0) {
            throw new IllegalStateException("LZ4 compression failed: " + result);
        }
        return Arrays.copyOf(compressed, size[0]);
    }
}
