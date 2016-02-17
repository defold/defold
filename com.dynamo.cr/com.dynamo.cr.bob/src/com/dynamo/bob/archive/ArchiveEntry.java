package com.dynamo.bob.archive;

import java.io.IOException;
import java.io.File;

public class ArchiveEntry implements Comparable<ArchiveEntry> {
    public static final int FLAG_ENCRYPTED = 1 << 0;
    public static final int FLAG_UNCOMPRESSED = 0xFFFFFFFF;

    // Member vars, TODO make these private and add getters/setters
    public int size;
    public int compressedSize;
    public int resourceOffset;
    public int flags;
    public String relName;
    public String fileName;

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

        this.relName = fileName.substring(root.length());
        this.fileName = fileName;
    }

    @Override
    public int compareTo(ArchiveEntry other) {
        return relName.compareTo(other.relName);
    }

}
