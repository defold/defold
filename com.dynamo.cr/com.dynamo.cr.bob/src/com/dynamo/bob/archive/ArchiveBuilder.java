package com.dynamo.bob.archive;

import java.io.File;

import java.io.FileInputStream;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;
import org.apache.commons.io.IOUtils;

import com.dynamo.bob.pipeline.ResourceNode;
import com.dynamo.crypt.Crypt;

import net.jpountz.lz4.LZ4Compressor;
import net.jpountz.lz4.LZ4Factory;

public class ArchiveBuilder {

    public static final int VERSION = 4;
    public static final int HASH_MAX_LENGTH = 64;
    public static final int HASH_LENGTH = 18;

    private static final byte[] KEY = "aQj8CScgNP4VsfXK".getBytes();

    private static final List<String> ENCRYPTED_EXTS = Arrays.asList("luac", "scriptc", "gui_scriptc", "render_scriptc");

    private List<ArchiveEntry> entries = new ArrayList<ArchiveEntry>();
    private String root;
    private ManifestBuilder manifestBuilder = null;
    private LZ4Compressor lz4Compressor;

    public ArchiveBuilder(String root, ManifestBuilder manifestBuilder) {
        this.root = new File(root).getAbsolutePath();
        this.manifestBuilder = manifestBuilder;
        this.lz4Compressor = LZ4Factory.fastestInstance().highCompressor();
    }

    public void add(String fileName, boolean doCompress) throws IOException {
        ArchiveEntry e = new ArchiveEntry(root, fileName, doCompress);
        entries.add(e);
    }

    public void add(String fileName) throws IOException {
        ArchiveEntry e = new ArchiveEntry(root, fileName, false);
        entries.add(e);
    }

    private int compress(String fileName, int fileSize, RandomAccessFile outFile) throws IOException {
        // Read source data
        byte[] tmp_buf = FileUtils.readFileToByteArray(new File(fileName));

        int maxCompressedSize = lz4Compressor.maxCompressedLength(tmp_buf.length);

        // Compress data
        byte[] compress_buf = new byte[maxCompressedSize];
        int compressedSize = lz4Compressor.compress(tmp_buf, compress_buf);

        double compRatio = (double)compressedSize/(double)fileSize;
        if(compRatio > 0.95) {
            // Store uncompressed if we gain less than 5%
            outFile.write(tmp_buf, 0, fileSize);
            compressedSize = ArchiveEntry.FLAG_UNCOMPRESSED;
        } else {
            outFile.write(compress_buf, 0, compressedSize);
        }

        return compressedSize;
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
        for (ArchiveEntry e : entries) {
            // Store offset to string
            stringsOffset.add((int) (outFile.getFilePointer() - stringPoolOffset));
            String normalisedPath = FilenameUtils.separatorsToUnix(e.relName);
            outFile.write(normalisedPath.getBytes());
            outFile.writeByte((byte) 0);
        }

        int stringPoolSize = (int) (outFile.getFilePointer() - stringPoolOffset);

        List<Integer> resourcesOffset = new ArrayList<Integer>();
        for (ArchiveEntry e : entries) {
            alignBuffer(outFile, 4);
            resourcesOffset.add((int) outFile.getFilePointer());
            if(e.compressedSize != ArchiveEntry.FLAG_UNCOMPRESSED) {
                e.compressedSize = compress(e.fileName, e.size, outFile);
            } else {
                copy(e.fileName, outFile);
            }
        }

        alignBuffer(outFile, 4);
        int entryOffset = (int) outFile.getFilePointer();
        int i = 0;
        for (ArchiveEntry e : entries) {
            outFile.writeInt(stringsOffset.get(i));
            outFile.writeInt(resourcesOffset.get(i));
            outFile.writeInt(e.size);
            outFile.writeInt(e.compressedSize);
            String ext = FilenameUtils.getExtension(e.fileName);
            if (ENCRYPTED_EXTS.indexOf(ext) != -1) {
                e.flags = ArchiveEntry.FLAG_ENCRYPTED;
            }
            outFile.writeInt(e.flags);
            ++i;
        }

        i = 0;
        for (ArchiveEntry e : entries) {
            String ext = FilenameUtils.getExtension(e.fileName);

            int size = e.size;
            if (e.compressedSize != ArchiveEntry.FLAG_UNCOMPRESSED) {
                size = e.compressedSize;
            }
            byte[] buf = new byte[size];

            if ((e.flags & ArchiveEntry.FLAG_ENCRYPTED) == ArchiveEntry.FLAG_ENCRYPTED) {
                outFile.seek(resourcesOffset.get(i));
                outFile.read(buf);
                byte[] enc = Crypt.encryptCTR(buf, KEY);
                outFile.seek(resourcesOffset.get(i));
                outFile.write(enc);
            }

            String normalisedPath = FilenameUtils.separatorsToUnix(e.relName);
            outFile.seek(resourcesOffset.get(i));
            outFile.read(buf);
            if (manifestBuilder != null) {
                manifestBuilder.addResourceEntry(normalisedPath, buf);
            }
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
    
    public void write2(RandomAccessFile outFileIndex, RandomAccessFile outFileData) throws IOException {
    	// INDEX
        outFileIndex.writeInt(VERSION); // Version
        outFileIndex.writeInt(0); // Pad
        outFileIndex.writeLong(0); // UserData
        outFileIndex.writeInt(0); // EntryCount
        outFileIndex.writeInt(0); // EntryOffset
        outFileIndex.writeInt(0); // HashOffset
        outFileIndex.writeInt(HASH_LENGTH); // HashLength

        int entryCount = entries.size();
        
        // Write to data file
        for (ArchiveEntry e : entries) {
        	// copy resource data to file (compress if flag set)
        	alignBuffer(outFileData, 4);
            e.resourceOffset = (int) outFileData.getFilePointer();
            if(e.compressedSize != ArchiveEntry.FLAG_UNCOMPRESSED) {
                e.compressedSize = compress(e.fileName, e.size, outFileData);
            } else {
                copy(e.fileName, outFileData);
            }
        }
        
        // TODO after merge, we should use manifestBuilder to create proper hash.
        // Write hashes to index file
        int debug_count = entryCount;
        int hashOffset = (int) outFileIndex.getFilePointer();
        for (int i = 0; i < entryCount; i++) {
        	byte[] full_hash = new byte[HASH_MAX_LENGTH];
        	byte[] debug_hash = ("wicked hash " + debug_count).getBytes();
        	for (int j = 0; j < debug_hash.length; j++) {
				full_hash[j] = debug_hash[j];
			}
        	
        	ArchiveEntry e = entries.get(i);
        	e.hash = full_hash;
        	debug_count -= 1;
		}

        // sort on hash here
        Collections.sort(entries);

        // Write entries to index file
        int entryOffset = (int) outFileIndex.getFilePointer();
        debug_count = 0;
        for (ArchiveEntry e : entries) {
        	outFileIndex.writeInt(e.resourceOffset);
        	outFileIndex.writeInt(e.size);
        	outFileIndex.writeInt(e.compressedSize);
        	outFileIndex.writeInt(e.flags);
        }

        // INDEX
        outFileIndex.seek(0); // Reset file and write actual offsets
        outFileIndex.writeInt(VERSION);
        outFileIndex.writeInt(0); // Pad
        outFileIndex.writeLong(0); // UserData
        outFileIndex.writeInt(entryCount);
        outFileIndex.writeInt(entryOffset);
        outFileIndex.writeInt(hashOffset);
        outFileIndex.writeInt(HASH_LENGTH); 
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
            System.err.println("USAGE: ArchiveBuilder <ROOT> <OUT> [-c] <FILE1> <FILE2>...");
        }

        int firstFileArg = 2;
        boolean doCompress = false;
        if(args.length > 2)
        {
            if("-c".equals(args[2])) {
                doCompress = true;
                firstFileArg = 3;
            } else {
                doCompress = false;
                firstFileArg = 2;
            }
        }

        ArchiveBuilder ab = new ArchiveBuilder(args[0], null);
        for (int i = firstFileArg; i < args.length; ++i) {
            ab.add(args[i], doCompress);
        }

        RandomAccessFile outFile = new RandomAccessFile(args[1], "rw");
        outFile.setLength(0);
        ab.write(outFile);
        outFile.close();
    }
}
