package com.dynamo.bob.archive;

import java.io.IOException;
import java.io.File;

import org.apache.commons.io.FilenameUtils;

public class ArchiveEntry implements Comparable<ArchiveEntry> {
    public static final int FLAG_ENCRYPTED = 1 << 0;
    public static final int FLAG_COMPRESSED = 1 << 1;
    public static final int FLAG_UNCOMPRESSED = 0xFFFFFFFF;

    // Member vars, TODO make these private and add getters/setters
    public int size;
    public int compressedSize;
    public int resourceOffset;
    public int flags;
    public String relName;
    public String fileName;
    public byte[] hash = null;

    public ArchiveEntry(String fileName) throws IOException {
        this.fileName = fileName;
    }

    public ArchiveEntry(String root, String fileName, boolean compress) throws IOException {
        File file = new File(fileName);
        if (!file.exists()) {
            throw new IOException(String.format("File %s doens't exists",
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

        this.relName = FilenameUtils.separatorsToUnix(fileName.substring(root.length()));
        this.fileName = fileName;
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


    @Override
    public int compareTo(ArchiveEntry other) {
        if (this.hash == null) {
            return this.relName.compareTo(other.relName);
        }

        return this.compare(this.hash, other.hash);
    }
}
