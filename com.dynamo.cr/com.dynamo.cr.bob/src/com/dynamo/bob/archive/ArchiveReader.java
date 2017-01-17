package com.dynamo.bob.archive;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.RandomAccessFile;

import java.util.List;
import java.util.ArrayList;

import java.io.FileNotFoundException;

public class ArchiveReader {
    public static final int VERSION = 4;
    public static final int HASH_BUFFER_BYTESIZE = 64; // 512 bits

    private ArrayList<ArchiveEntry> entries = null;

    private int stringPoolOffset = 0;
    private int stringPoolSize = 0;
    private int entryCount = 0;
    private int entryOffset = 0;
    private int hashOffset = 0;
    private int hashLength = 0;

    private String darcPath = null;
    private RandomAccessFile inFile = null;

    private String indexFilePath = null;
    private String dataFilePath = null;
    private RandomAccessFile indexFile = null;
    private RandomAccessFile dataFile = null;

    public ArchiveReader(String darcPath) {
        this.darcPath = darcPath;
    }

    public ArchiveReader(String indexPath, String dataPath) {
        this.indexFilePath = indexPath;
        this.dataFilePath = dataPath;
    }

    public void read() throws IOException {
        if (indexFilePath == null) {
            throw new IOException("No index file path set.");
        }

        if (dataFilePath == null) {
            throw new IOException("No data file path set.");
        }

        if (indexFile != null) {
            indexFile.close();
            indexFile = null;
        }

        if (dataFile != null) {
            dataFile.close();
            dataFile = null;
        }

        indexFile = new RandomAccessFile(this.indexFilePath, "r");
        dataFile = new RandomAccessFile(this.dataFilePath, "r");

        // Version
        indexFile.seek(0);
        dataFile.seek(0);
        int indexVersion = indexFile.readInt();
        if (indexVersion == VERSION) {
            readArchiveData();
        } else {
            throw new IOException("Invalid index or data format, version: " + indexVersion);
        }
    }

    private void readArchiveData() throws IOException {
        // INDEX
        indexFile.readInt(); // Pad
        indexFile.readLong(); // UserData, should be 0
        entryCount = indexFile.readInt();
        entryOffset = indexFile.readInt();
        hashOffset = indexFile.readInt();
        hashLength = indexFile.readInt();

        entries = new ArrayList<ArchiveEntry>(entryCount);

        // Hashes are stored linearly in memory instead of within each entry, so the hashes are read in a separate loop.
        // Once the hashes are read, the rest of the entries are read.

        indexFile.seek(hashOffset);
        // Read entry hashes
        for (int i=0; i<entryCount; ++i) {
            indexFile.seek(hashOffset + i * HASH_BUFFER_BYTESIZE);
            ArchiveEntry e = new ArchiveEntry("");
            e.hash = new byte[HASH_BUFFER_BYTESIZE];
            indexFile.read(e.hash, 0, hashLength);
            entries.add(e);
        }

        // Read entries
        indexFile.seek(entryOffset);
        for (int i=0; i<entryCount; ++i) {
            ArchiveEntry e = entries.get(i);

            e.resourceOffset = indexFile.readInt();
            e.size = indexFile.readInt();
            e.compressedSize = indexFile.readInt();
            e.flags = indexFile.readInt();
        }
    }

    public List<ArchiveEntry> getEntries() {
        return entries;
    }

    public byte[] getEntryContent(ArchiveEntry entry) throws IOException {
        byte[] buf = new byte[entry.size];

        if (this.darcPath != null) {
            inFile.seek(entry.resourceOffset);
            inFile.read(buf, 0, entry.size);
        } else {
            dataFile.seek(entry.resourceOffset);
            dataFile.read(buf, 0, entry.size);
        }

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
            byte[] buf = new byte[entry.size];
            if (this.darcPath != null) {
                inFile.seek(entry.resourceOffset);
                inFile.read(buf, 0, readSize);
            } else {
                dataFile.seek(entry.resourceOffset);
                dataFile.read(buf, 0, readSize);
            }

            File fo = new File(outdir);
            fo.getParentFile().mkdirs();
            FileOutputStream os = new FileOutputStream(fo);
            os.write(buf);
            os.close();
        }
    }

    public void close() throws IOException {
        if (inFile != null) {
            inFile.close();
            inFile = null;
        }

        if (indexFile != null) {
            indexFile.close();
            indexFile = null;
        }

        if (dataFile != null) {
            dataFile.close();
            dataFile = null;
        }
    }
}
