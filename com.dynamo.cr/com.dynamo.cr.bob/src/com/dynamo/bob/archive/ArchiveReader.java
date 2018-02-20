package com.dynamo.bob.archive;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.RandomAccessFile;
import java.util.ArrayList;
import java.util.List;

import com.dynamo.liveupdate.proto.Manifest.ManifestData;
import com.dynamo.liveupdate.proto.Manifest.ManifestFile;
import com.dynamo.liveupdate.proto.Manifest.ResourceEntry;

public class ArchiveReader {
    public static final int VERSION = 4;
    public static final int HASH_BUFFER_BYTESIZE = 64; // 512 bits

    private ArrayList<ArchiveEntry> entries = null;

    private int entryCount = 0;
    private int entryOffset = 0;
    private int hashOffset = 0;
    private int hashLength = 0;

    private final String archiveIndexFilepath;
    private final String archiveDataFilepath;
    private final String manifestFilepath;
    private RandomAccessFile archiveIndexFile = null;
    private RandomAccessFile archiveDataFile = null;
    private ManifestFile manifestFile = null;

    public ArchiveReader(String archiveIndexFilepath, String archiveDataFilepath, String manifestFilepath) {
        this.archiveIndexFilepath = archiveIndexFilepath;
        this.archiveDataFilepath = archiveDataFilepath;
        this.manifestFilepath = manifestFilepath;
    }

    public void read() throws IOException {
    	this.archiveIndexFile = new RandomAccessFile(this.archiveIndexFilepath, "r");
        this.archiveDataFile = new RandomAccessFile(this.archiveDataFilepath, "r");
        
        this.archiveIndexFile.seek(0);
        this.archiveDataFile.seek(0);
        
        if (this.manifestFilepath != null) {
	        InputStream manifestInputStream = new FileInputStream(this.manifestFilepath);
	        this.manifestFile = ManifestFile.parseFrom(manifestInputStream);
	        manifestInputStream.close();
        }

        // Version
        int indexVersion = this.archiveIndexFile.readInt();
        if (indexVersion == ArchiveReader.VERSION) {
            readArchiveData();
        } else {
            throw new IOException("Unsupported archive index version: " + indexVersion);
        }
    }
    
    private boolean matchHash(byte[] a, byte[] b, int hlen) {
    	if (a.length < hlen || b.length < hlen) {
    		return false;
    	}
    	
    	for (int i = 0; i < hlen; ++i) {
    		if (a[i] != b[i]) {
    			return false;
    		}
    	}
    	
    	return true;
    }

    private void readArchiveData() throws IOException {
        // INDEX
        archiveIndexFile.readInt(); // Pad
        archiveIndexFile.readLong(); // UserData, should be 0
        entryCount = archiveIndexFile.readInt();
        entryOffset = archiveIndexFile.readInt();
        hashOffset = archiveIndexFile.readInt();
        hashLength = archiveIndexFile.readInt();

        entries = new ArrayList<ArchiveEntry>(entryCount);

        // Hashes are stored linearly in memory instead of within each entry, so the hashes are read in a separate loop.
        // Once the hashes are read, the rest of the entries are read.

        archiveIndexFile.seek(hashOffset);
        // Read entry hashes
        for (int i = 0; i < entryCount; ++i) {
            archiveIndexFile.seek(hashOffset + i * HASH_BUFFER_BYTESIZE);
            ArchiveEntry e = new ArchiveEntry("");
            e.hash = new byte[HASH_BUFFER_BYTESIZE];
            archiveIndexFile.read(e.hash, 0, hashLength);

            if (this.manifestFile != null) {
                ManifestData manifestData = ManifestData.parseFrom(this.manifestFile.getData());
	            for (ResourceEntry resource : manifestData.getResourcesList()) {
	            	if (matchHash(e.hash, resource.getHash().getData().toByteArray(), this.hashLength)) {
	            		e.fileName = resource.getUrl();
	            		e.relName = resource.getUrl();
	            	}
	            }
            }
            
            entries.add(e);
        }

        // Read entries
        archiveIndexFile.seek(entryOffset);
        for (int i=0; i<entryCount; ++i) {
            ArchiveEntry e = entries.get(i);

            e.resourceOffset = archiveIndexFile.readInt();
            e.size = archiveIndexFile.readInt();
            e.compressedSize = archiveIndexFile.readInt();
            e.flags = archiveIndexFile.readInt();
        }
    }

    public List<ArchiveEntry> getEntries() {
        return entries;
    }

    public byte[] getEntryContent(ArchiveEntry entry) throws IOException {
        byte[] buf = new byte[entry.size];
        archiveDataFile.seek(entry.resourceOffset);
        archiveDataFile.read(buf, 0, entry.size);

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
            archiveDataFile.seek(entry.resourceOffset);
            archiveDataFile.read(buf, 0, readSize);

            File fo = new File(outdir);
            fo.getParentFile().mkdirs();
            FileOutputStream os = new FileOutputStream(fo);
            os.write(buf);
            os.close();
        }
    }

    public void close() throws IOException {
        if (archiveIndexFile != null) {
            archiveIndexFile.close();
            archiveIndexFile = null;
        }

        if (archiveDataFile != null) {
            archiveDataFile.close();
            archiveDataFile = null;
        }
    }
}
