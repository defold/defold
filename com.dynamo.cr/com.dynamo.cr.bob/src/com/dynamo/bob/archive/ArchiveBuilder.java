package com.dynamo.bob.archive;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.file.Files;
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
import com.dynamo.liveupdate.proto.Manifest.HashAlgorithm;

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

    public void write(RandomAccessFile archiveIndex, RandomAccessFile archiveData, Path resourcePackDirectory, List<String> excludedResources) throws IOException {
        // INDEX
        archiveIndex.writeInt(VERSION); // Version
        archiveIndex.writeInt(0); // Pad
        archiveIndex.writeLong(0); // UserData, used in runtime to distinguish between if the index and resources are memory mapped or loaded from disk
        archiveIndex.writeInt(0); // EntryCount
        archiveIndex.writeInt(0); // EntryOffset
        archiveIndex.writeInt(0); // HashOffset
        archiveIndex.writeInt(0); // HashLength

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

            // Add entry to manifest
            String normalisedPath = FilenameUtils.separatorsToUnix(entry.relName);
            manifestBuilder.addResourceEntry(normalisedPath, buffer);

            // Calculate hash digest values for resource
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
                alignBuffer(archiveData, 4);
                entry.resourceOffset = (int) archiveData.getFilePointer();
                archiveData.write(buffer, 0, buffer.length);
            }
        }

        // Write sorted hashes to index file
        Collections.sort(entries);
        int hashOffset = (int) archiveIndex.getFilePointer();
        for(ArchiveEntry entry : entries) {
            archiveIndex.write(entry.hash);
        }

        // Write sorted entries to index file
        int entryOffset = (int) archiveIndex.getFilePointer();
        alignBuffer(archiveIndex, 4);
        for (ArchiveEntry entry : entries) {
            archiveIndex.writeInt(entry.resourceOffset);
            archiveIndex.writeInt(entry.size);
            archiveIndex.writeInt(entry.compressedSize);
            archiveIndex.writeInt(entry.flags);
        }

        // Update index header with offsets.
        archiveIndex.seek(0);
        archiveIndex.writeInt(VERSION);
        archiveIndex.writeInt(0); // Pad
        archiveIndex.writeLong(0); // UserData
        archiveIndex.writeInt(entries.size());
        archiveIndex.writeInt(entryOffset);
        archiveIndex.writeInt(hashOffset);
        archiveIndex.writeInt(ManifestBuilder.CryptographicOperations.getHashSize(manifestBuilder.getResourceHashAlgorithm()));
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
            System.err.println("USAGE: ArchiveBuilder <ROOT> <(out) archive-index> <(out) archive-data> [-c] <FILE1> <FILE2>...");
        }

        int firstFileArg = 3;
        boolean doCompress = false;
        if(args.length > 3)
        {
            if("-c".equals(args[3])) {
                doCompress = true;
                firstFileArg = 4;
            } else {
                doCompress = false;
                firstFileArg = 3;
            }
        }

        ManifestBuilder manifestBuilder = new ManifestBuilder();
        manifestBuilder.setResourceHashAlgorithm(HashAlgorithm.HASH_MD5);
        ArchiveBuilder ab = new ArchiveBuilder(args[0], manifestBuilder);
        for (int i = firstFileArg; i < args.length; ++i) {
            ab.add(args[i], doCompress);
        }

        RandomAccessFile archiveIndex = new RandomAccessFile(args[1], "rw");
        archiveIndex.setLength(0);
        RandomAccessFile archiveData  = new RandomAccessFile(args[2], "rw");
        archiveData.setLength(0);

        Path resourcePackDirectory = Files.createTempDirectory("tmp.defold.resourcepack_");
        ab.write(archiveIndex, archiveData, resourcePackDirectory, new ArrayList<String>());

        archiveIndex.close();
        archiveData.close();

    }
}
