package com.dynamo.bob.archive;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import org.apache.commons.io.IOUtils;

public class ArchiveBuilder {

    public static final int VERSION = 2;

    class Entry implements Comparable<Entry> {

        int size;
        String relName;
        private String fileName;

        public Entry(String fileName) throws IOException {
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
            this.relName = fileName.substring(root.length());
            this.fileName = fileName;
        }

        @Override
        public int compareTo(Entry other) {
            return relName.compareTo(other.relName);
        }

    }

    private List<Entry> entries = new ArrayList<ArchiveBuilder.Entry>();
    private String root;

    public ArchiveBuilder(String root) {
        this.root = new File(root).getAbsolutePath();
    }

    public void add(String fileName) throws IOException {
        Entry e = new Entry(fileName);
        entries.add(e);
    }

    private void copy(String fileName, RandomAccessFile outFile) throws IOException {
        byte[] buf = new byte[64 * 1024];
        FileInputStream is = new FileInputStream(fileName);
        int n = is.read(buf);
        while (n > 0) {
            outFile.write(buf, 0, n);
            n = is.read(buf);
        }
        IOUtils.closeQuietly(is);
    }

    public void write(RandomAccessFile outFile) throws IOException {
        // Version
        outFile.writeInt(VERSION);
        // Pad
        outFile.writeInt(0);
        // Userdata
        outFile.writeLong(0);
        // StringPoolOffset
        outFile.writeInt(0);
        // StringPoolSize
        outFile.writeInt(0);
        // EntryCount
        outFile.writeInt(0);
        // EntryOffset
        outFile.writeInt(0);

        Collections.sort(entries);

        int stringPoolOffset = (int) outFile.getFilePointer();
        List<Integer> stringsOffset = new ArrayList<Integer>();
        for (Entry e : entries) {
            // Store offset to string
            stringsOffset.add((int) (outFile.getFilePointer() - stringPoolOffset));
            // Write filename string
            outFile.write(e.relName.getBytes());
            outFile.writeByte((byte) 0);
        }

        int stringPoolSize = (int) (outFile.getFilePointer() - stringPoolOffset);

        List<Integer> resourcesOffset = new ArrayList<Integer>();
        for (Entry e : entries) {
            alignBuffer(outFile, 4);
            resourcesOffset.add((int) outFile.getFilePointer());
            copy(e.fileName, outFile);
        }

        alignBuffer(outFile, 4);
        int entryOffset = (int) outFile.getFilePointer();
        int i = 0;
        for (Entry e : entries) {
            outFile.writeInt(stringsOffset.get(i));
            outFile.writeInt(resourcesOffset.get(i));
            outFile.writeInt((int) e.size);
            ++i;
        }

        // Reset file and write actual offsets
        outFile.seek(0);
        // Version
        outFile.writeInt(VERSION);
        // Pad
        outFile.writeInt(0);
        // Userdata
        outFile.writeLong(0);
        // StringPoolOffset
        outFile.writeInt(stringPoolOffset);
        // StringPoolSize
        outFile.writeInt(stringPoolSize);
        // EntryCount
        outFile.writeInt(entries.size());
        // EntryOffset
        outFile.writeInt(entryOffset);

    }

    private void alignBuffer(RandomAccessFile outFile, int align) throws IOException {
        int pos = (int) outFile.getFilePointer();
        int newPos = (int) (outFile.getFilePointer() + (align - 1));
        newPos &= ~(align - 1);

        for (int i = 0; i < (newPos - pos); ++i) {
            outFile.writeByte((byte) 0);
        }
    }

    public static void main(String[] args) throws IOException {
        if (args.length < 2) {
            System.err.println("USAGE: ArchiveBuilder <ROOT> <OUT> <FILE1> <FILE2>...");
        }

        ArchiveBuilder ab = new ArchiveBuilder(args[0]);
        for (int i = 2; i < args.length; ++i) {
            ab.add(args[i]);
        }

        RandomAccessFile outFile = new RandomAccessFile(args[1], "rw");
        outFile.setLength(0);
        ab.write(outFile);
        outFile.close();
    }
}
