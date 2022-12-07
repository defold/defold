// Copyright 2020-2022 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
// 
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
// 
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

// Please keep the overview documentation up-to-date:
// https://github.com/defold/defold/blob/dev/engine/docs/ARCHIVE_FORMAT.md

package com.dynamo.bob.archive;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.ByteBuffer;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.HashSet;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;
import org.apache.commons.io.IOUtils;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.pipeline.ResourceNode;
import com.dynamo.liveupdate.proto.Manifest.HashAlgorithm;
import com.dynamo.liveupdate.proto.Manifest.SignAlgorithm;
import com.dynamo.liveupdate.proto.Manifest.ResourceEntryFlag;
import com.dynamo.liveupdate.proto.Manifest.ManifestData;

import net.jpountz.lz4.LZ4Compressor;
import net.jpountz.lz4.LZ4Factory;

public class ArchiveBuilder {

    public static final int VERSION = 4;
    public static final int HASH_MAX_LENGTH = 64; // 512 bits
    public static final int HASH_LENGTH = 20;
    public static final int MD5_HASH_DIGEST_BYTE_LENGTH = 16; // 128 bits

    private List<ArchiveEntry> entries = new ArrayList<ArchiveEntry>();
    private Set<String> lookup = new HashSet<String>(); // To see if a resource has already been added
    private String root;
    private ManifestBuilder manifestBuilder = null;
    private LZ4Compressor lz4Compressor;
    private byte[] archiveIndexMD5 = new byte[MD5_HASH_DIGEST_BYTE_LENGTH];
    private int resourcePadding = 4;

    public ArchiveBuilder(String root, ManifestBuilder manifestBuilder, int resourcePadding) {
        this.root = new File(root).getAbsolutePath();
        this.manifestBuilder = manifestBuilder;
        this.lz4Compressor = LZ4Factory.fastestInstance().highCompressor();
        this.resourcePadding = resourcePadding;
    }

    private void add(String fileName, boolean compress, boolean encrypt, boolean isLiveUpdate) throws IOException {
        ArchiveEntry e = new ArchiveEntry(root, fileName, compress, encrypt, isLiveUpdate);
        if (!contains(e)) {
            lookup.add(e.relName);
            entries.add(e);
        }
    }

    public void add(String fileName, boolean compress, boolean encrypt) throws IOException {
        add(fileName, compress, encrypt, false);
    }

    public void add(String fileName) throws IOException {
        add(fileName, false, false, false);
    }

    private boolean contains(ArchiveEntry e) {
        return lookup.contains(e.relName);
    }

    public ArchiveEntry getArchiveEntry(int index) {
        return this.entries.get(index);
    }

    public int getArchiveEntrySize() {
        return this.entries.size();
    }

    public byte[] getArchiveIndexHash() {
        return this.archiveIndexMD5;
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

    public byte[] encryptResourceData(byte[] buffer) throws CompileExceptionError {
        return ResourceEncryption.encrypt(buffer);
    }

    public void writeResourcePack(String filename, String directory, byte[] buffer, byte flags, int size) throws IOException {
        FileOutputStream outputStream = null;
        try {
            File fhandle = new File(directory, filename);
            if (!fhandle.exists()) {
                byte[] padding = new byte[11];
                byte[] size_bytes = ByteBuffer.allocate(4).putInt(size).array();
                Arrays.fill(padding, (byte)0xED);
                outputStream = new FileOutputStream(fhandle);
                outputStream.write(size_bytes); // 4 bytes
                outputStream.write(flags); // 1 byte
                outputStream.write(padding); // 11 bytes
                outputStream.write(buffer);

            }
        } finally {
            IOUtils.closeQuietly(outputStream);
        }
    }

    public void write(RandomAccessFile archiveIndex, RandomAccessFile archiveData, Path resourcePackDirectory,
                        Set<String> excludedResources, List<ArchiveResources> archives) throws IOException, CompileExceptionError {

System.out.printf("\n");
System.out.printf("ArchiveBuilder.write\n");

System.out.printf("  excludedResources:\n");
for (String url : excludedResources) {
    System.out.printf("    %s\n", url);
}
System.out.printf("\n");

        // INDEX
        archiveIndex.writeInt(VERSION); // Version
        archiveIndex.writeInt(0); // Pad
        archiveIndex.writeLong(0); // UserData, used in runtime to distinguish between if the index and resources are memory mapped or loaded from disk
        archiveIndex.writeInt(0); // EntryCount
        archiveIndex.writeInt(0); // EntryOffset
        archiveIndex.writeInt(0); // HashOffset
        archiveIndex.writeInt(0); // HashLength
        archiveIndex.write(new byte[MD5_HASH_DIGEST_BYTE_LENGTH]);

        int archiveIndexHeaderOffset = (int) archiveIndex.getFilePointer();

        Collections.sort(entries); // Since it has no hash, it sorts on path

        for (int i = entries.size() - 1; i >= 0; --i) {
            ArchiveEntry entry = entries.get(i);
            byte[] buffer = this.loadResourceData(entry.fileName);
            byte archiveEntryFlags = (byte) entry.flags;
            int resourceEntryFlags = ResourceEntryFlag.BUNDLED.getNumber();
            if (entry.compressedSize != ArchiveEntry.FLAG_UNCOMPRESSED) {
                // Compress data
                byte[] compressed = this.compressResourceData(buffer);
                if (this.shouldUseCompressedResourceData(buffer, compressed)) {
                    archiveEntryFlags = (byte)(archiveEntryFlags | ArchiveEntry.FLAG_COMPRESSED);
                    buffer = compressed;
                    entry.compressedSize = compressed.length;
                } else {
                    entry.compressedSize = ArchiveEntry.FLAG_UNCOMPRESSED;
                }
            }

            // Encrypt data
            if ((archiveEntryFlags & ArchiveEntry.FLAG_ENCRYPTED) != 0) {
                buffer = this.encryptResourceData(buffer);
            }

            // Add entry to manifest
            String normalisedPath = FilenameUtils.separatorsToUnix(entry.relName);

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

            // Write resource to data archive(s)

            boolean isExcluded = excludedResources.contains(normalisedPath);

System.out.printf("%s - %s\n", normalisedPath, isExcluded?"excluded":"");

            if (isExcluded) { // then at least one archive references it
                for (ArchiveResources archiveResources : archives) {
                    String archiveName = archiveResources.archiveName;

                    if (archiveResources.contains(normalisedPath)) {
                        File archiveDir = new File(resourcePackDirectory.toFile(), archiveName);
System.out.printf("  Writing to archive '%s': %s\n", archiveName, normalisedPath);
                        this.writeResourcePack(hexDigest, archiveDir.getAbsolutePath(), buffer, archiveEntryFlags, entry.size);
                    }
                }
            }

            System.out.printf("%s - %s\n", normalisedPath, isExcluded?"excluded":"");

            if (isExcluded) {
                entries.remove(i);
                resourceEntryFlags = ResourceEntryFlag.EXCLUDED.getNumber();
            } else {
                alignBuffer(archiveData, this.resourcePadding);
                entry.resourceOffset = (int) archiveData.getFilePointer();
                archiveData.write(buffer, 0, buffer.length);
            }

            manifestBuilder.addResourceEntry(normalisedPath, buffer, resourceEntryFlags);
        }

        Collections.sort(entries); // Since it has a hash, it sorts on hash

        // Write sorted hashes to index file
        int hashOffset = (int) archiveIndex.getFilePointer();
        for(ArchiveEntry entry : entries) {
            archiveIndex.write(entry.hash);
        }

        // Write sorted entries to index file
        int entryOffset = (int) archiveIndex.getFilePointer();
        alignBuffer(archiveIndex, 4);

        ByteBuffer indexBuffer = ByteBuffer.allocate(4 * 4 * entries.size());
        for (ArchiveEntry entry : entries) {
            indexBuffer.putInt(entry.resourceOffset);
            indexBuffer.putInt(entry.size);
            indexBuffer.putInt(entry.compressedSize);
            indexBuffer.putInt(entry.flags);
        }
        archiveIndex.write(indexBuffer.array());

        try {
            // Calc index file MD5 hash
            archiveIndex.seek(archiveIndexHeaderOffset);
            int num_bytes = (int) archiveIndex.length() - archiveIndexHeaderOffset;
            byte[] archiveIndexBytes = new byte[num_bytes];
            archiveIndex.readFully(archiveIndexBytes);
            this.archiveIndexMD5 = ManifestBuilder.CryptographicOperations.hash(archiveIndexBytes, HashAlgorithm.HASH_MD5);
        } catch (NoSuchAlgorithmException e) {
            System.err.println("The algorithm specified is not supported!");
            e.printStackTrace();
        }

        // Update index header with offsets
        archiveIndex.seek(0);
        archiveIndex.writeInt(VERSION);
        archiveIndex.writeInt(0); // Pad
        archiveIndex.writeLong(0); // UserData
        archiveIndex.writeInt(entries.size());
        archiveIndex.writeInt(entryOffset);
        archiveIndex.writeInt(hashOffset);
        archiveIndex.writeInt(ManifestBuilder.CryptographicOperations.getHashSize(manifestBuilder.getResourceHashAlgorithm()));
        archiveIndex.write(this.archiveIndexMD5);
    }

    private void alignBuffer(RandomAccessFile outFile, int align) throws IOException {
        int pos = (int) outFile.getFilePointer();
        int newPos = (int) (outFile.getFilePointer() + (align - 1));
        newPos &= ~(align - 1);

        for (int i = 0; i < (newPos - pos); ++i) {
            outFile.writeByte((byte) 0);
        }
    }

    private static void printUsageAndTerminate(String message) {
        System.err.println("Usage: ArchiveBuilder <root> <output> [-c] <file> [<file> ...]\n");
        System.err.println("  <root>            - directorypath to root of input files (<file>)");
        System.err.println("  <output>          - filepath for output content.");
        System.err.println("                      These files (.arci, .arcd, .dmanifest .public, .private, .manifest_hash) will be generated.");
        System.err.println("  <file>            - filepath relative to <root> of file to build.");
        System.err.println("  -c                - Compress archive (default false).");
        System.err.println("  -m                - Output manifest hash file (default false).");
        if (message != null) {
            System.err.println("\nError: " + message);
        }

        System.exit(1);
    }

    public static void main(String[] args) throws IOException, NoSuchAlgorithmException, CompileExceptionError {
        // Validate input
        if (args.length < 3) {
            printUsageAndTerminate("Too few arguments");
        }

        File dirpathRoot            = new File(args[0]);
        File filepathArchiveIndex   = new File(args[1] + ".arci");
        File filepathArchiveData    = new File(args[1] + ".arcd");
        File filepathManifest       = new File(args[1] + ".dmanifest");
        File filepathPublicKey      = new File(args[1] + ".public");
        File filepathPrivateKey     = new File(args[1] + ".private");
        File filepathManifestHash   = new File(args[1] + ".manifest_hash");

        if (!dirpathRoot.isDirectory()) {
            printUsageAndTerminate("root does not exist: " + dirpathRoot.getAbsolutePath());
        }

        boolean doCompress = false;
        List<File> inputs = new ArrayList<File>();
        for (int i = 2; i < args.length; ++i) {
            if (args[i].equals("-c")) {
                doCompress = true;
            } else {
                File currentInput = new File(args[i]);
                if (!currentInput.isFile()) {
                    printUsageAndTerminate("file does not exist: " + currentInput.getAbsolutePath());
                }

                inputs.add(currentInput);
            }
        }

        if (inputs.isEmpty()) {
            printUsageAndTerminate("There must be at least one file");
        }

        // Create manifest and archive

        ManifestBuilder manifestBuilder = new ManifestBuilder();
        manifestBuilder.setProjectIdentifier("<anonymous project>");
        manifestBuilder.addSupportedEngineVersion(EngineVersion.sha1);
        manifestBuilder.setResourceHashAlgorithm(HashAlgorithm.HASH_SHA1);
        manifestBuilder.setSignatureHashAlgorithm(HashAlgorithm.HASH_SHA256);
        manifestBuilder.setSignatureSignAlgorithm(SignAlgorithm.SIGN_RSA);

        System.out.println("Generating private key: " + filepathPrivateKey.getCanonicalPath());
        System.out.println("Generating public key: " + filepathPublicKey.getCanonicalPath());
        ManifestBuilder.CryptographicOperations.generateKeyPair(SignAlgorithm.SIGN_RSA, filepathPrivateKey.getAbsolutePath(), filepathPublicKey.getAbsolutePath());
        manifestBuilder.setPrivateKeyFilepath(filepathPrivateKey.getAbsolutePath());
        manifestBuilder.setPublicKeyFilepath(filepathPublicKey.getAbsolutePath());

        ResourceNode rootNode = new ResourceNode("<AnonymousRoot>", "<AnonymousRoot>");

        int archivedEntries = 0;
        int excludedEntries = 0;
        ArchiveBuilder archiveBuilder = new ArchiveBuilder(dirpathRoot.toString(), manifestBuilder, 4);
        for (File currentInput : inputs) {
            String absolutePath = currentInput.getAbsolutePath();
            boolean encrypt = (absolutePath.endsWith("luac") || absolutePath.endsWith("scriptc") || absolutePath.endsWith("gui_scriptc") || absolutePath.endsWith("render_scriptc"));
            if (currentInput.getName().startsWith("liveupdate.")){
                excludedEntries++;
                archiveBuilder.add(absolutePath, doCompress, encrypt, true);
            } else {
                archivedEntries++;
                archiveBuilder.add(absolutePath, doCompress, encrypt, false);
            }
            ResourceNode currentNode = new ResourceNode(currentInput.getPath(), absolutePath);
            rootNode.addChild(currentNode);
        }
        System.out.println("Added " + Integer.toString(archivedEntries + excludedEntries) + " entries to archive (" + Integer.toString(excludedEntries) + " entries tagged as 'liveupdate' in archive).");

        //manifestBuilder.setRoot(rootNode);

        RandomAccessFile archiveIndex = new RandomAccessFile(filepathArchiveIndex, "rw");
        RandomAccessFile archiveData  = new RandomAccessFile(filepathArchiveData, "rw");
        archiveIndex.setLength(0);
        archiveData.setLength(0);

        Path resourcePackDirectory = Files.createTempDirectory("tmp.defold.resourcepack_");
        FileOutputStream outputStreamManifest = new FileOutputStream(filepathManifest);
        try {
            System.out.println("Writing " + filepathArchiveIndex.getCanonicalPath());
            System.out.println("Writing " + filepathArchiveData.getCanonicalPath());

            archiveBuilder.write(archiveIndex, archiveData, resourcePackDirectory, new HashSet<String>(), new ArrayList<ArchiveResources>());
            manifestBuilder.setArchiveIdentifier(archiveBuilder.getArchiveIndexHash());

            System.out.println("Writing " + filepathManifest.getCanonicalPath());

            ManifestData mainManifestData = manifestBuilder.buildManifestData();
            byte[] mainManifestFileData = manifestBuilder.buildManifestFileByteArray(mainManifestData);
            outputStreamManifest.write(mainManifestFileData);

        } finally {
            FileUtils.deleteDirectory(resourcePackDirectory.toFile());
            try {
                archiveIndex.close();
                archiveData.close();
                outputStreamManifest.close();
            } catch (IOException exception) {
                // Nothing to do, moving on ...
            }
        }

        System.out.println("Done.");
    }
}
