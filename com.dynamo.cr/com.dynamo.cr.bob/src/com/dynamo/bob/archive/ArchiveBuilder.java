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

import com.dynamo.crypt.Crypt;

import net.jpountz.lz4.LZ4Compressor;
import net.jpountz.lz4.LZ4Factory;

public class ArchiveBuilder {

    public static final int VERSION = 4;

    private static final byte[] KEY = "aQj8CScgNP4VsfXK".getBytes();

    private static final List<String> ENCRYPTED_EXTS = Arrays.asList("luac", "scriptc", "gui_scriptc", "render_scriptc");

    private List<ArchiveEntry> entries = new ArrayList<ArchiveEntry>();
    private String root;
    private LZ4Compressor lz4Compressor;

    public ArchiveBuilder(String root) {
        this.root = new File(root).getAbsolutePath();
        this.lz4Compressor = LZ4Factory.fastestInstance().highCompressor();
    }

    public void add(String fileName, boolean doCompress) throws IOException {
        ArchiveEntry e = new ArchiveEntry(root, fileName, doCompress);
        entries.add(e);
    }
    
    public void add(String fileName, byte[] hashData, boolean doCompress) throws IOException
    {
    	ArchiveEntry e = new ArchiveEntry(root, fileName, hashData, doCompress);
    	entries.add(e);
    }
    
    public void add(String fileName, byte[] hashData, int hashSize) throws IOException
    {
    	ArchiveEntry e = new ArchiveEntry(root, fileName, hashData, false);
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
            if ((e.flags & ArchiveEntry.FLAG_ENCRYPTED) == ArchiveEntry.FLAG_ENCRYPTED) {
                outFile.seek(resourcesOffset.get(i));
                int size = e.size;
                if (e.compressedSize != ArchiveEntry.FLAG_UNCOMPRESSED) {
                    size = e.compressedSize;
                }
                byte[] buf = new byte[size];
                outFile.read(buf);
                byte[] enc = Crypt.encryptCTR(buf, KEY);
                outFile.seek(resourcesOffset.get(i));
                outFile.write(enc);
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
        // Version
        outFileIndex.writeInt(VERSION+1);
        outFileData.writeInt(VERSION+1);
        // Pad
        outFileIndex.writeInt(0);
        outFileData.writeInt(0);
        // Userdata
        outFileIndex.writeLong(0);
        outFileData.writeLong(0);
        // StringPoolOffset
        outFileIndex.writeInt(0);
        // StringPoolSize
        outFileIndex.writeInt(0);
        // HashDigestOffset
        outFileIndex.writeInt(0);
        //HashDigestSize
        outFileIndex.writeInt(0);
        // EntryCount
        outFileData.writeInt(0);
        // EntryOffset
        outFileData.writeInt(0);
        
        /*
         * Iterate first over entries, and store then to outputFileData in sequence
         * Then iterate for index file, store stringpool, hashdigest also in sequence
         * */

        Collections.sort(entries);

        int stringPoolOffset = (int) outFileIndex.getFilePointer();
        List<Integer> stringsOffset = new ArrayList<Integer>();
        for (ArchiveEntry e : entries) {
            // Store offset to string
            stringsOffset.add((int) (outFileIndex.getFilePointer() - stringPoolOffset));
            String normalisedPath = FilenameUtils.separatorsToUnix(e.relName);
            outFileIndex.write(normalisedPath.getBytes());
            outFileIndex.writeByte((byte) 0);
        }
        int stringPoolSize = (int) (outFileIndex.getFilePointer() - stringPoolOffset);
        
        int hashDigestPoolOffset = (int) outFileIndex.getFilePointer();
        List<Integer> hashDigestOffsets = new ArrayList<Integer>();
        for(ArchiveEntry e : entries) {
        	// Store offset to hash digest
        	hashDigestOffsets.add((int) (outFileIndex.getFilePointer() - hashDigestPoolOffset));
        	//outFile.writeInt(e.hashDigestSize);
        	outFileIndex.writeInt(e.hashDigest.length);
        	outFileIndex.write(e.hashDigest, 0, e.hashDigest.length);
        }
        
        int hashDigestPoolSize = (int) (outFileIndex.getFilePointer() - hashDigestPoolOffset);

        // Copy resources to data file
        List<Integer> resourcesOffset = new ArrayList<Integer>();
        for (ArchiveEntry e : entries) {
            alignBuffer(outFileData, 4);
            resourcesOffset.add((int) outFileData.getFilePointer());
            if(e.compressedSize != ArchiveEntry.FLAG_UNCOMPRESSED) {
                e.compressedSize = compress(e.fileName, e.size, outFileData);
            } else {
                copy(e.fileName, outFileData);
            }
        }

        // Entry meta data
        alignBuffer(outFileData, 4);
        int entryOffset = (int) outFileData.getFilePointer();
        int i = 0;
        for (ArchiveEntry e : entries) {
        	/*outFileData.writeInt(stringsOffset.get(i));
        	outFileData.writeInt(hashDigestOffsets.get(i));*/
        	outFileData.writeInt(resourcesOffset.get(i));
        	outFileData.writeInt(e.size);
        	outFileData.writeInt(e.compressedSize);
            String ext = FilenameUtils.getExtension(e.fileName);
            if (ENCRYPTED_EXTS.indexOf(ext) != -1) {
                e.flags = ArchiveEntry.FLAG_ENCRYPTED;
            }
            outFileData.writeInt(e.flags);
            ++i;
        }

        i = 0;
        for (ArchiveEntry e : entries) {
            String ext = FilenameUtils.getExtension(e.fileName);
            if ((e.flags & ArchiveEntry.FLAG_ENCRYPTED) == ArchiveEntry.FLAG_ENCRYPTED) {
            	outFileData.seek(resourcesOffset.get(i));
                int size = e.size;
                if (e.compressedSize != ArchiveEntry.FLAG_UNCOMPRESSED) {
                    size = e.compressedSize;
                }
                byte[] buf = new byte[size];
                outFileData.read(buf);
                byte[] enc = Crypt.encryptCTR(buf, KEY);
                outFileData.seek(resourcesOffset.get(i));
                outFileData.write(enc);
            }
            ++i;
        }

        // Reset file and write actual offsets
        outFileIndex.seek(0);
        outFileData.seek(0);
        // Version
        outFileIndex.writeInt(VERSION+1);
        outFileData.writeInt(VERSION+1);
        // Pad
        outFileIndex.writeInt(0);
        outFileData.writeInt(0);
        // Userdata
        outFileIndex.writeLong(0);
        outFileData.writeLong(0);
        // StringPoolOffset
        outFileIndex.writeInt(stringPoolOffset);
        // StringPoolSize
        outFileIndex.writeInt(stringPoolSize);
        // HashDigestOffset
        outFileIndex.writeInt(hashDigestPoolOffset);
        //HashDigestSize
        outFileIndex.writeInt(hashDigestPoolSize);
        // EntryCount
        outFileData.writeInt(entries.size());
        // EntryOffset
        outFileData.writeInt(entryOffset);
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
        
        ArchiveBuilder ab = new ArchiveBuilder(args[0]);
        for (int i = firstFileArg; i < args.length; ++i) {
            ab.add(args[i], doCompress);
        }

        RandomAccessFile outFile = new RandomAccessFile(args[1], "rw");
        outFile.setLength(0);
        ab.write(outFile);
        outFile.close();
    }
}
