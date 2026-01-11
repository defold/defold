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

package com.dynamo.bob.archive;

import java.io.IOException;
import java.io.File;

import org.apache.commons.io.FilenameUtils;

public class ArchiveEntry implements Comparable<ArchiveEntry> {
    public static final int FLAG_ENCRYPTED = 1 << 0;
    public static final int FLAG_COMPRESSED = 1 << 1;
    public static final int FLAG_LIVEUPDATE = 1 << 2;
    public static final int FLAG_UNCOMPRESSED = 0xFFFFFFFF;

    private int size;
    private int compressedSize;
    private int resourceOffset;
    private int flags = 0;
    private String relName;
    private String fileName;
    private String hexDigest;
    private byte[] hash = null;
    private byte[] header = new byte[0];
    private boolean duplicatedDataBlob;

    public ArchiveEntry(String fileName) throws IOException {
        this.fileName = fileName;
        this.relName = fileName;
    }

    public ArchiveEntry(String root, String fileName, boolean compress, boolean encrypt, boolean isLiveUpdate) throws IOException {
        File file = new File(fileName);
        if (!file.exists()) {
            throw new IOException(String.format("File %s does not exists",
                    fileName));
        }

        fileName = file.getAbsolutePath();
        if (!fileName.startsWith(root)) {
            throw new IOException(String.format(
                    "File %s isn't relative to root directory %s",
                    fileName, root));
        }

        this.size = (int) file.length();
        if(compress) {
            // Will be set to real value after compression
            this.compressedSize = 0;
        } else {
            this.compressedSize = FLAG_UNCOMPRESSED;
        }

        if (encrypt) {
            this.flags = this.flags | FLAG_ENCRYPTED;
        }

        if (isLiveUpdate) {
            this.flags = this.flags | FLAG_LIVEUPDATE;
        }

        this.relName = FilenameUtils.separatorsToUnix(fileName.substring(root.length()));
        this.fileName = fileName;
    }

    public ArchiveEntry(String root, String fileName, boolean compress, boolean encrypt) throws IOException {
        this(root, fileName, compress, encrypt, false);
    }

    public ArchiveEntry(String root, String fileName) throws IOException {
        this(root, fileName, false, false, false);
    }

    public void setHeader(byte[] header) {
        this.header = header;
    }

    public byte[] getHeader() {
        return header;
    }

    public int getSize() {
        return size;
    }

    public void setSize(int size) {
        this.size = size;
    }

    public int getCompressedSize() {
        return compressedSize;
    }

    public void setCompressedSize(int compressedSize) {
        this.compressedSize = compressedSize;
    }

    public byte[] getHash() {
        return hash;
    }

    public void setHash(byte[] hash) {
        this.hash = hash;
    }

    public String getHexDigest() {
        return hexDigest;
    }

    public void setHexDigest(String hexDigest) {
        this.hexDigest = hexDigest;
    }

    public int getResourceOffset() {
        return resourceOffset;
    }

    public void setResourceOffset(int resourceOffset) {
        this.resourceOffset = resourceOffset;
    }

    public String getName() {
        return FilenameUtils.getName(fileName);
    }

    public String getFilename() {
        return fileName;
    }

    public void setFilename(String fileName) {
        this.fileName = fileName;
    }
    
    public String getRelativeFilename() {
        return relName;
    }

    public void setRelativeFilename(String relName) {
        this.relName = relName;
    }

    public boolean isCompressed() {
        return compressedSize != ArchiveEntry.FLAG_UNCOMPRESSED;
    }

    public boolean isEncrypted() {
        return (flags & FLAG_ENCRYPTED) != 0;
    }

    public boolean isExcluded() {
        return (flags & FLAG_LIVEUPDATE) != 0;
    }

    public int getFlags() {
        return flags;
    }

    public void setFlags(int flags) {
        this.flags = flags;
    }

    public void setFlag(int flag) {
        this.flags = this.flags | flag;
    }

    // For checking duplicate when constructing archive
    @Override
    public boolean equals(Object other){
        boolean result = this.getClass().equals(other.getClass());
        if (result) {
            ArchiveEntry entryOther = (ArchiveEntry)other;
            result = this.fileName.equals(entryOther.fileName)
                    && this.relName.equals(entryOther.relName)
                    && this.flags == entryOther.flags;
        }
        return result;
    }

    public int hashCode() {
        return 17 * this.fileName.hashCode() + 31 * this.relName.hashCode();
    }

    private int compare(byte[] left, byte[] right) {
        for (int i = 0, j = 0; i < left.length && j < right.length; i++, j++) {
            int a = (left[i] & 0xff);
            int b = (right[j] & 0xff);
            if (a != b) {
                return a - b;
            }
        }
        return left.length - right.length;
    }

    // For sorting according to hash when building archive
    @Override
    public int compareTo(ArchiveEntry other) {
        if (this.hash == null || other.hash == null) {
            return this.relName.compareTo(other.relName);
        }

        return this.compare(this.hash, other.hash);
    }

    @Override
    public String toString() {
        return getClass().getName() + " " + this.fileName + ":" + this.hexDigest;
    }

    public void setDuplicatedDataBlob(boolean b) {
        this.duplicatedDataBlob = b;
    }

    public boolean isDuplicatedDataBlob() {
        return duplicatedDataBlob;
    }
}
