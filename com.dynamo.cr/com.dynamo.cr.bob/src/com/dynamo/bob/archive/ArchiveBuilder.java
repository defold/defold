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

    public ArchiveEntry getArchiveEntry(int index) {
        return this.entries.get(index);
    }

    public int getArchiveEntrySize() {
        return this.entries.size();
    }

    public byte[] loadResourceData(String filepath) throws IOException {
        File fhandle = new File(filepath);
        return FileUtils.readFileToByteArray(fhandle);
    }

    public byte[] compressResourceData(byte[] buffer) {
        int maximumCompressedSize = lz4Compressor.maxCompressedLength(buffer.length);
        byte[] compressedContent = new byte[maximumCompressedSize];
        int compressedSize = lz4Compressor.compress(buffer, compressedContent);
        return Arrays.copyOfRange(compressedContent, 0, compressedSize);
    }

    public boolean shouldUseCompressedResourceData(byte[] original, byte[] compressed) {
        double ratio = (double) compressed.length / (double) original.length;
        return ratio <= 0.95;
    }

    public byte[] encryptResourceData(byte[] buffer) {
        return Crypt.encryptCTR(buffer, KEY);
    }

    public void writeResourcePack(String filename, String directory, byte[] buffer) throws IOException {
        FileOutputStream outputStream = null;
        try {
            File fhandle = new File(directory, filename);
            if (!fhandle.exists()) {
                outputStream = new FileOutputStream(fhandle);
                outputStream.write(buffer);
            }
        } finally {
            IOUtils.closeQuietly(outputStream);
        }
    }

    public boolean excludeResource(String filepath, List<String> excludedResources) {
        if (this.manifestBuilder != null) {
            List<String> parentFilepaths = this.manifestBuilder.getParentFilepath(filepath);
            for (String parentFilepath : parentFilepaths) {
                for (String excludedResource : excludedResources) {
                    if (parentFilepath.equals(excludedResource)) {
                        return true;
                    }
                }
            }
        }

        return false;
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

    public void write2(RandomAccessFile outFileIndex, RandomAccessFile outFileData, Path resourcePackDirectory, List<String> excludedResources) throws IOException {
        // INDEX
        outFileIndex.writeInt(VERSION); // Version
        outFileIndex.writeInt(0); // Pad
        outFileIndex.writeLong(0); // UserData, used in runtime to distinguish between if the index and resources are memory mapped or loaded from disk
        outFileIndex.writeInt(0); // EntryCount
        outFileIndex.writeInt(0); // EntryOffset
        outFileIndex.writeInt(0); // HashOffset
        outFileIndex.writeInt(HASH_LENGTH); // HashLength

        for (int i = entries.size() - 1; i >= 0; --i) {
            ArchiveEntry entry = entries.get(i);
            byte[] buffer = this.loadResourceData(entry.fileName);

            // Compress data
            byte[] compressed = this.compressResourceData(buffer);
            if (this.shouldUseCompressedResourceData(buffer, compressed)) {
                buffer = compressed;
                entry.compressedSize = compressed.length;
            } else {
                entry.compressedSize = ArchiveEntry.FLAG_UNCOMPRESSED;
            }

            // Encrypt data
            String extension = FilenameUtils.getExtension(entry.fileName);
            if (ENCRYPTED_EXTS.indexOf(extension) != -1) {
                entry.flags = ArchiveEntry.FLAG_ENCRYPTED;
                buffer = this.encryptResourceData(buffer);
            }

            // Write entry to manifest
            String normalisedPath = FilenameUtils.separatorsToUnix(entry.relName);
            manifestBuilder.addResourceEntry(normalisedPath, buffer);

            // Calculate hash values for resource
            String hexDigest = null;
            try {
                byte[] hashDigest = ManifestBuilder.CryptographicOperations.hash(buffer, manifestBuilder.getResourceHashAlgorithm());
                entry.hash = new byte[HASH_MAX_LENGTH];
                System.arraycopy(hashDigest, 0, entry.hash, 0, hashDigest.length);
                hexDigest = ManifestBuilder.CryptographicOperations.hexdigest(hashDigest);
            } catch (NoSuchAlgorithmException exception) {
                throw new IOException("Unable to create a Resource Pack, the hashing algorithm is not supported!");
            }

            // Write resource to resourcepack
            this.writeResourcePack(hexDigest, resourcePackDirectory.toString(), buffer);

            // Write resource to data archive
            if (this.excludeResource(normalisedPath, excludedResources)) {
                entries.remove(i);
            } else {
                alignBuffer(outFileData, 4);
                entry.resourceOffset = (int) outFileData.getFilePointer();
                outFileData.write(buffer, 0, buffer.length);
            }
        }

        // Write sorted hashes to index file
        Collections.sort(entries);
        int hashOffset = (int) outFileIndex.getFilePointer();
        for(ArchiveEntry entry : entries) {
            outFileIndex.write(entry.hash);
        }

        // Write sorted entries to index file
        int entryOffset = (int) outFileIndex.getFilePointer();
        alignBuffer(outFileIndex, 4);
        for (ArchiveEntry entry : entries) {
            outFileIndex.writeInt(entry.resourceOffset);
            outFileIndex.writeInt(entry.size);
            outFileIndex.writeInt(entry.compressedSize);
            outFileIndex.writeInt(entry.flags);
        }

        // Update index header with offsets.
        outFileIndex.seek(0);
        outFileIndex.writeInt(VERSION);
        outFileIndex.writeInt(0); // Pad
        outFileIndex.writeLong(0); // UserData
        outFileIndex.writeInt(entries.size());
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
