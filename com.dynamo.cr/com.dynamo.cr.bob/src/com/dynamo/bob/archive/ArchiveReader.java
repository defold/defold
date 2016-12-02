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
    public static final int HASH_MAX_LENGTH = 64; // 512 bits

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
    	if (this.darcPath == null) {
    		if (indexFilePath != null && dataFilePath != null) {
    			read2();
    			return;
    		} else {
    			throw new IOException("No DARC path set.");
    		}
        }

        if (inFile != null) {
            this.close();
        }
        
        inFile = new RandomAccessFile(this.darcPath, "r");

        // Version
        inFile.seek(0);
        int version = inFile.readInt();
        if(version == VERSION) {
        	readDarc();
        } else {
        	throw new IOException("Invalid DARC format: " + version);
        }
    }
    
    private void read2() throws IOException {
    	if (indexFilePath == null || dataFilePath == null) {
            throw new IOException("No index or data file path set.");
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
        if (indexVersion == VERSION) { // files written with hash digests are tagged with VERSION+1
        	readDarc2();
        } else {
        	throw new IOException("Invalid index or data format, version: " + indexVersion);
        }
    }
    
    private void readDarc() throws IOException {
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
    
    private void readDarc2() throws IOException {
    	// INDEX
        indexFile.readInt(); // Pad
        indexFile.readLong(); // UserData, should be 0
        entryCount = indexFile.readInt();
        entryOffset = indexFile.readInt();
        hashOffset = indexFile.readInt();
        hashLength = indexFile.readInt();
        
        entries = new ArrayList<ArchiveEntry>(entryCount);
        
        indexFile.seek(hashOffset);
        // Read hash into entries
        for (int i=0; i<entryCount; ++i) {
        	indexFile.seek(hashOffset + i * HASH_MAX_LENGTH);
        	ArchiveEntry e = new ArchiveEntry("");
        	e.hash = new byte[HASH_MAX_LENGTH];
        	indexFile.read(e.hash, 0, hashLength);
        	entries.add(e);
        }

        // seek to entries
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
