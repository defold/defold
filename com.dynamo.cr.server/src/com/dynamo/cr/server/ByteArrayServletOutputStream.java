/**
 * Copyright (C) 2009 Mo Chen <withinsea@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.dynamo.cr.server;

import java.io.IOException;

import javax.servlet.ServletOutputStream;

public class ByteArrayServletOutputStream extends ServletOutputStream {

    protected byte buf[];

    protected int count;

    public ByteArrayServletOutputStream() {
        this(32);
    }

    public ByteArrayServletOutputStream(int size) {
        if (size < 0) {
            throw new IllegalArgumentException("Negative initial size: " + size);
        }
        buf = new byte[size];
    }

    public synchronized byte toByteArray()[] {
        return copyOf(buf, count);
    }

    public synchronized void reset() {
        count = 0;
    }

    public synchronized int size() {
        return count;
    }

    public void enlarge(int size) {
        if (size > buf.length) {
            buf = copyOf(buf, Math.max(buf.length << 1, size));
        }
    }

    @Override
    public synchronized void write(int b) throws IOException {
        int newcount = count + 1;
        enlarge(newcount);
        buf[count] = (byte) b;
        count = newcount;
    }

    /**
     * copy from: jdk1.6, java.util.Arrays.copyOf(byte[] original, int
     * newLength)
     */
    private static byte[] copyOf(byte[] original, int newLength) {
        byte[] copy = new byte[newLength];
        System.arraycopy(original, 0, copy, 0, Math.min(original.length,
                newLength));
        return copy;
    }
}