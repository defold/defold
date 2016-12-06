package com.dynamo.bob.archive;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.file.Path;
import java.security.NoSuchAlgorithmException;
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
    public static final int HASH_MAX_LENGTH = 64; // 512 bits
    public static final int HASH_LENGTH = 20;

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

    public void write(RandomAccessFile outFile, Path resourcePackDirectory) throws IOException {
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
        int duplicate_size = 0;
        for (ArchiveEntry e : entries) {
            int size = (e.compressedSize == ArchiveEntry.FLAG_UNCOMPRESSED) ? e.size : e.compressedSize;
            byte[] buffer = new byte[size];

            outFile.seek(resourcesOffset.get(i));
            outFile.read(buffer);

            // Encrypt content
            if ((e.flags & ArchiveEntry.FLAG_ENCRYPTED) == ArchiveEntry.FLAG_ENCRYPTED) {
                byte[] ciphertext = Crypt.encryptCTR(buffer, KEY);
                outFile.seek(resourcesOffset.get(i));
                outFile.write(ciphertext);
                buffer = Arrays.copyOf(ciphertext, ciphertext.length);
            }

            // Write an entry to the manifest
            if (manifestBuilder != null) {
                String normalisedPath = FilenameUtils.separatorsToUnix(e.relName);
                manifestBuilder.addResourceEntry(normalisedPath, buffer);

                // Write the entry to the Resource pack directory
                byte[] hashDigest;
                try {
                    hashDigest = ManifestBuilder.CryptographicOperations.hash(buffer, manifestBuilder.getResourceHashAlgorithm());
                } catch (NoSuchAlgorithmException exception) {
                    throw new IOException("Unable to create a Resource Pack, the hashing algorithm is not supported!");
                }
                String filename = ManifestBuilder.CryptographicOperations.hexdigest(hashDigest);
                FileOutputStream outputStream = null;
                try {
                    File fhandle = new File(resourcePackDirectory.toString(), filename);
                    if (!fhandle.exists()) {
                        outputStream = new FileOutputStream(fhandle);
                        outputStream.write(buffer);
                    } else {
                        duplicate_size += buffer.length;
                        System.out.println("The resource has already been created: " + filename);
                    }
                } finally {
                    if (outputStream != null) {
                        try {
                            outputStream.close();
                        } catch (IOException exception) {
                            // There's nothing we can do at this point
                        }
                    }
                }
            }

            ++i;
        }

        System.out.println("Storage size saved from removing duplicates: " + Integer.toString(duplicate_size) + " byte(s).");

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
    
    private void WriteToManifest(ManifestBuilder manifestBuilder, ArchiveEntry e, Path resourcePackDirectory) throws IOException {
        int size = (e.compressedSize == ArchiveEntry.FLAG_UNCOMPRESSED) ? e.size : e.compressedSize;
        byte[] buffer = new byte[size];
        e.hash = new byte[HASH_MAX_LENGTH];
        String normalisedPath = FilenameUtils.separatorsToUnix(e.relName);
        manifestBuilder.addResourceEntry(normalisedPath, buffer);

        // Write the entry to the Resource pack directory
        byte[] hashDigest;
        try {
            hashDigest = ManifestBuilder.CryptographicOperations.hash(buffer, manifestBuilder.getResourceHashAlgorithm());
            for (int j = 0; j < hashDigest.length; j++) {
                e.hash[j] = hashDigest[j];
            }
        } catch (NoSuchAlgorithmException exception) {
            throw new IOException("Unable to create a Resource Pack, the hashing algorithm is not supported!");
        }
        String filename = ManifestBuilder.CryptographicOperations.hexdigest(hashDigest);
        FileOutputStream outputStream = null;
        try {
            File fhandle = new File(resourcePackDirectory.toString(), filename);
            if (!fhandle.exists()) {
                outputStream = new FileOutputStream(fhandle);
                outputStream.write(buffer);
            }
        } finally {
            if (outputStream != null) {
                try {
                    outputStream.close();
                } catch (IOException exception) {
                    // There's nothing we can do at this point
                }
            }
        }
    }
    
    public void write2(RandomAccessFile outFileIndex, RandomAccessFile outFileData, Path resourcePackDirectory) throws IOException {
        // INDEX
        outFileIndex.writeInt(VERSION); // Version
        outFileIndex.writeInt(0); // Pad
        outFileIndex.writeLong(0); // UserData, used in runtime to distinguish between if the index and resources are memory mapped or loaded from disk
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
            
            if(manifestBuilder != null) {
            	WriteToManifest(manifestBuilder, e, resourcePackDirectory);
            }
        }
        
        // Write hashes to index file
        int hashOffset = (int) outFileIndex.getFilePointer();
        
        // sort on hash
        Collections.sort(entries);

        // Write sorted hashes to index file
        for(ArchiveEntry e : entries) {
        	outFileIndex.write(e.hash);
        }

        // Write entries (now sorted on hash) to index file
        int entryOffset = (int) outFileIndex.getFilePointer();
        alignBuffer(outFileIndex, 4);
        for (ArchiveEntry e : entries) {
            outFileIndex.writeInt(e.resourceOffset);
            outFileIndex.writeInt(e.size);
            outFileIndex.writeInt(e.compressedSize);
            String ext = FilenameUtils.getExtension(e.fileName);
            if (ENCRYPTED_EXTS.indexOf(ext) != -1) {
                e.flags = ArchiveEntry.FLAG_ENCRYPTED;
            }
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
        ab.write(outFile, null);
        outFile.close();
    }
}
