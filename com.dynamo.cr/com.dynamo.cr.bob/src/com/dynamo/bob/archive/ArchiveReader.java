package com.dynamo.bob.archive;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.RandomAccessFile;

import java.util.List;
import java.util.ArrayList;
import java.util.Arrays;

import java.io.File;
import java.io.FileNotFoundException;

public class ArchiveReader {

    public static final int VERSION = 4;

    private ArrayList<ArchiveEntry> entries = null;

    private int stringPoolOffset = 0;
    private int stringPoolSize = 0;
    private int entryCount = 0;
    private int entryOffset = 0;

    private String darcPath = null;
    private RandomAccessFile inFile = null;

    public ArchiveReader(String darcPath) {
        this.darcPath = darcPath;
    }

    public void read() throws IOException {
        if (darcPath == null) {
            throw new IOException("No DARC path set.");
        }

        if (inFile != null) {
            this.close();
        }
        inFile = new RandomAccessFile(this.darcPath, "r");

        // Version
        inFile.seek(0);
        int version = inFile.readInt();
        if (version != VERSION) {
            throw new IOException("Invalid DARC format: " + version);
        }

        // Pad
        inFile.readInt();

        // Userdata, should be 0
        inFile.readLong();

        stringPoolOffset = inFile.readInt();
        stringPoolSize = inFile.readInt();
        entryCount = inFile.readInt();
        entryOffset = inFile.readInt();

        // Jump to string pool
        inFile.seek(stringPoolOffset);

        // Gather entry strings
        entries = new ArrayList<ArchiveEntry>();
        for (int i = 0; i < entryCount; i++) {
            String entryName = "";
            while (true) {
                byte b = inFile.readByte();
                if (b == (byte)0) {
                    break;
                }
                entryName += (char)b;
            }
            entries.add(new ArchiveEntry(entryName));
        }

        inFile.seek(entryOffset);

        // Fill entry meta data
        for (int i = 0; i < entryCount; i++) {
            int stringOffset = inFile.readInt();

            ArchiveEntry entry = entries.get(i);
            entry.resourceOffset = inFile.readInt();
            entry.size = inFile.readInt();
            entry.compressedSize = inFile.readInt();
            if (entry.compressedSize == ArchiveEntry.FLAG_UNCOMPRESSED) {
                entry.compressedSize = entry.size;
            }
            entry.flags = inFile.readInt();
        }
    }

    public List<ArchiveEntry> getEntries() {
        return entries;
    }

    public byte[] getEntryContent(ArchiveEntry entry) throws IOException {
        byte[] buf = new byte[entry.size];
        inFile.seek(entry.resourceOffset);
        inFile.read(buf, 0, entry.size);

        return buf;
    }

    public void extractAll(String path) throws IOException {

        int entryCount = entries.size();

        System.out.println("Extracting entries to " + path + ": ");
        for (int i = 0; i < entryCount; i++) {
            ArchiveEntry entry = entries.get(i);
            String outdir = path + entry.fileName;
            System.out.println("> " + entry.fileName);
            int readSize = entry.compressedSize;

            // extract
            inFile.seek(entry.resourceOffset);
            byte[] buf = new byte[entry.size];
            inFile.read(buf, 0, readSize);
            File fo = new File(outdir);
            fo.getParentFile().mkdirs();
            FileOutputStream os = new FileOutputStream(fo);
            os.write(buf);
            os.close();
        }
    }

    public void close() throws IOException {
        inFile.close();
        inFile = null;
    }

    public static void main(String[] args) throws IOException {

        if (args.length < 1) {
            System.err.println("USAGE: java ArchiveReader <DARC-FILE> [EXTRACT-DIR]");
            System.exit(1);
        }

        String darcPath = args[0];
        String extractPath = null;
        if (args.length > 1) {
            extractPath = args[1];
        }

        ArchiveReader ar = new ArchiveReader(args[0]);

        try {
            ar.read();
        } catch (FileNotFoundException e) {
            System.err.println("File not found: " + darcPath);
            System.exit(1);
        }

        // Extract content of darc
        if (extractPath != null) {
            try {
                File file = new File(extractPath);
                file.mkdirs();
                ar.extractAll(extractPath);
            } catch (IOException e) {
                System.out.println("could not extract files");
                System.out.println(e);
                System.exit(1);
            } finally {
                ar.close();
            }

        // List content of darc
        } else {

            List<ArchiveEntry> entries = ar.getEntries();
            int entryCount = entries.size();

            for (int i = 0; i < entryCount; i++) {
                ArchiveEntry entry = entries.get(i);
                System.out.println("> " + entry.fileName);
                System.out.println("       size: " + entry.size);
                System.out.println(" compressed: " + entry.compressedSize);
                System.out.println("      ratio: " + ((float)entry.compressedSize / (float)entry.size));
                System.out.println("  encrypted: " + ((entry.flags & ArchiveEntry.FLAG_ENCRYPTED) == ArchiveEntry.FLAG_ENCRYPTED ? "yes" : "-"));
                System.out.println("");
            }

            ar.close();
        }

        System.exit(0);
    }
}
