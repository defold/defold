// Copyright 2020-2023 The Defold Foundation
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
import java.util.Set;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;
import org.apache.commons.io.IOUtils;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.pipeline.ResourceNode;
import com.dynamo.liveupdate.proto.Manifest.HashAlgorithm;
import com.dynamo.liveupdate.proto.Manifest.SignAlgorithm;
import com.dynamo.liveupdate.proto.Manifest.ResourceEntryFlag;

import com.dynamo.bob.archive.publisher.PublisherSettings;
import com.dynamo.bob.archive.publisher.Publisher;
import com.dynamo.bob.archive.publisher.ZipPublisher;

import net.jpountz.lz4.LZ4Compressor;
import net.jpountz.lz4.LZ4Factory;

public class ArchiveBuilder {

    public static final int VERSION = 5;
    public static final int HASH_MAX_LENGTH = 64; // 512 bits
    public static final int HASH_LENGTH = 20;
    public static final int MD5_HASH_DIGEST_BYTE_LENGTH = 16; // 128 bits

    private List<ArchiveEntry> entries = new ArrayList<ArchiveEntry>();
    private List<ArchiveEntry> excludedEntries = new ArrayList<ArchiveEntry>();
    private Set<String> lookup = new HashSet<String>(); // To see if a resource has already been added
    private String root;
    private ManifestBuilder manifestBuilder = null;
    private LZ4Compressor lz4Compressor;
    private byte[] archiveIndexMD5 = new byte[MD5_HASH_DIGEST_BYTE_LENGTH];
    private int resourcePadding = 4;
    private boolean forceCompression = false; // for building unit tests to create test content

    public ArchiveBuilder(String root, ManifestBuilder manifestBuilder, int resourcePadding) {
        this.root = new File(root).getAbsolutePath();
        this.manifestBuilder = manifestBuilder;
        this.lz4Compressor = LZ4Factory.fastestInstance().highCompressor();
        this.resourcePadding = resourcePadding;
    }

    private void add(String fileName, boolean compress, boolean encrypt, boolean isLiveUpdate) throws IOException {
        ArchiveEntry e = new ArchiveEntry(root, fileName, compress, encrypt, isLiveUpdate);
        if (!contains(e)) {
            lookup.add(e.getRelativeFilename());
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
        return lookup.contains(e.getRelativeFilename());
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

    public void setForceCompression(boolean forceCompression) {
        this.forceCompression = forceCompression;
    }

    public boolean getForceCompression() {
        return forceCompression;
    }

    public boolean shouldUseCompressedResourceData(byte[] original, byte[] compressed) {
        if (this.getForceCompression())
            return compressed.length < original.length;

        double ratio = (double) compressed.length / (double) original.length;
        return ratio <= 0.95;
    }

    public byte[] encryptResourceData(byte[] buffer) throws CompileExceptionError {
        return ResourceEncryption.encrypt(buffer);
    }

    public void writeResourcePack(ArchiveEntry entry, String directory, byte[] buffer) throws IOException {
        FileOutputStream outputStream = null;
        try {
            File fhandle = new File(directory, entry.getHexDigest());
            if (!fhandle.exists()) {
                byte[] padding = new byte[11];
                byte[] size_bytes = ByteBuffer.allocate(4).putInt(entry.getSize()).array();
                Arrays.fill(padding, (byte)0xED);
                outputStream = new FileOutputStream(fhandle);
                outputStream.write(size_bytes); // 4 bytes
                outputStream.write((byte)entry.getFlags()); // 1 byte
                outputStream.write(padding); // 11 bytes
                outputStream.write(buffer);

            }
        } finally {
            IOUtils.closeQuietly(outputStream);
        }
    }

    // Checks if any of the parents are excluded
    // Parents are sorted, deepest parent first, root parent last
    public boolean isTreeExcluded(List<String> parents, List<String> excludedResources) {
        for (String parent : parents) {
            if (excludedResources.contains(parent))
                return true;
        }
        return false;
    }

    public List<ArchiveEntry> getExcludedEntries() {
        return excludedEntries;
    }

    public boolean excludeResource(String filepath, List<String> excludedResources) {
        boolean result = false;
        if (this.manifestBuilder != null) {
            List<ArrayList<String>> parentChains = this.manifestBuilder.getParentCollections(filepath);
            for (List<String> parents : parentChains) {
                boolean excluded = isTreeExcluded(parents, excludedResources);
                if (!excluded) {
                    return false; // as long as one tree path requires this resource, we cannot exclude it
                }
                result = true;
            }

            if (excludedResources.contains(filepath))
            {
                return true;
            }
        }

        return result;
    }

    public void write(RandomAccessFile archiveIndex, RandomAccessFile archiveData, Path resourcePackDirectory, List<String> excludedResources) throws IOException, CompileExceptionError {
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

            byte[] buffer = this.loadResourceData(entry.getFilename());

            int resourceEntryFlags = 0;

            // Encrypt data first, so we can decrypt in place at runtime
            // this allows us to minimize allocations/copying
            if (entry.isEncrypted()) {
                buffer = this.encryptResourceData(buffer);
                resourceEntryFlags |= ResourceEntryFlag.ENCRYPTED.getNumber();
            }

            if (entry.isCompressed()) {
                // Compress data
                byte[] compressed = this.compressResourceData(buffer);
                if (this.shouldUseCompressedResourceData(buffer, compressed)) {
                    // Note, when forced, the compressed size may be larger than the original size (For unit tests)
                    buffer = compressed;
                    entry.setCompressedSize(compressed.length);
                    entry.setFlag(ArchiveEntry.FLAG_COMPRESSED);
                    resourceEntryFlags |= ResourceEntryFlag.COMPRESSED.getNumber();
                } else {
                    entry.setCompressedSize(ArchiveEntry.FLAG_UNCOMPRESSED);
                }
            }

            // Add entry to manifest
            String normalisedPath = FilenameUtils.separatorsToUnix(entry.getRelativeFilename());

            // Calculate hash digest values for resource
            String hexDigest = null;
            try {
                byte[] hashDigest = ManifestBuilder.CryptographicOperations.hash(buffer, manifestBuilder.getResourceHashAlgorithm());
                entry.setHash(new byte[HASH_MAX_LENGTH]);
                System.arraycopy(hashDigest, 0, entry.getHash(), 0, hashDigest.length);
                hexDigest = ManifestBuilder.CryptographicOperations.hexdigest(hashDigest);
            } catch (NoSuchAlgorithmException exception) {
                throw new IOException("Unable to create a Resource Pack, the hashing algorithm is not supported!");
            }

            // Store association between hexdigest and original filename in a lookup table
            entry.setHexDigest(hexDigest);

            // Write resource to resource pack or data archive
            if (this.excludeResource(normalisedPath, excludedResources)) {
                this.writeResourcePack(entry, resourcePackDirectory.toString(), buffer);
                entries.remove(i);
                excludedEntries.add(entry);
                resourceEntryFlags |= ResourceEntryFlag.EXCLUDED.getNumber();
            } else {
                alignBuffer(archiveData, this.resourcePadding);
                entry.setResourceOffset((int) archiveData.getFilePointer());
                archiveData.write(buffer, 0, buffer.length);
                resourceEntryFlags |= ResourceEntryFlag.BUNDLED.getNumber();
            }

            manifestBuilder.addResourceEntry(normalisedPath, buffer, entry.getSize(), entry.getCompressedSize(), resourceEntryFlags);
        }

        Collections.sort(entries); // Since it has a hash, it sorts on hash

        // Write sorted hashes to index file
        int hashOffset = (int) archiveIndex.getFilePointer();
        for(ArchiveEntry entry : entries) {
            archiveIndex.write(entry.getHash());
        }

        // Write sorted entries to index file
        int entryOffset = (int) archiveIndex.getFilePointer();
        alignBuffer(archiveIndex, 4);

        ByteBuffer indexBuffer = ByteBuffer.allocate(4 * 4 * entries.size());
        for (ArchiveEntry entry : entries) {
            indexBuffer.putInt(entry.getResourceOffset());
            indexBuffer.putInt(entry.getSize());
            indexBuffer.putInt(entry.getCompressedSize());
            indexBuffer.putInt(entry.getFlags());
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
        System.err.println("                      Three files (arci, arcd, dmanifest) will be generated.");
        System.err.println("  <file>            - filepath relative to <root> of file to build.");
        System.err.println("  -c                - Compress archive (default false).");
        if (message != null) {
            System.err.println("\nError: " + message);
        }

        System.exit(1);
    }

    // The main function is used to create builtins archive for the runtime connect app, and also test content
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
        File filepathZipArchive     = new File(args[1] + ".zip");

        if (!dirpathRoot.isDirectory()) {
            printUsageAndTerminate("root does not exist: " + dirpathRoot.getAbsolutePath());
        }

        boolean doCompress = false;
        boolean doOutputManifestHashFile = false;
        List<File> inputs = new ArrayList<File>();
        for (int i = 2; i < args.length; ++i) {
            if (args[i].equals("-c")) {
                doCompress = true;
            } else if (args[i].equals("-m")) {
                doOutputManifestHashFile = true;
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

        ManifestBuilder manifestBuilder = new ManifestBuilder(doOutputManifestHashFile);
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

        List<String> excludedResources = new ArrayList<String>();

        int archivedEntries = 0;
        String dirpathRootString = dirpathRoot.toString();
        ArchiveBuilder archiveBuilder = new ArchiveBuilder(dirpathRoot.toString(), manifestBuilder, 4);
        archiveBuilder.setForceCompression(doCompress);
        for (File currentInput : inputs) {
            String absolutePath = currentInput.getAbsolutePath();
            boolean encrypt = ( absolutePath.endsWith("luac") ||
                                absolutePath.endsWith("scriptc") ||
                                absolutePath.endsWith("gui_scriptc") ||
                                absolutePath.endsWith("render_scriptc"));
            if (currentInput.getName().startsWith("liveupdate.")){
                archiveBuilder.add(absolutePath, doCompress, encrypt, true);

                String relativePath = currentInput.getAbsolutePath().substring(dirpathRootString.length());
                relativePath = FilenameUtils.separatorsToUnix(relativePath);
                excludedResources.add(relativePath);
            } else {
                archivedEntries++;
                archiveBuilder.add(absolutePath, doCompress, encrypt, false);
            }
            ResourceNode currentNode = new ResourceNode(currentInput.getPath(), absolutePath);
            rootNode.addChild(currentNode);
        }
        System.out.println("Added " + Integer.toString(archivedEntries + excludedResources.size()) + " entries to archive (" + Integer.toString(excludedResources.size()) + " entries tagged as 'liveupdate' in archive).");

        manifestBuilder.setRoot(rootNode);

        RandomAccessFile archiveIndex = new RandomAccessFile(filepathArchiveIndex, "rw");
        RandomAccessFile archiveData  = new RandomAccessFile(filepathArchiveData, "rw");
        archiveIndex.setLength(0);
        archiveData.setLength(0);

        Path resourcePackDirectory = Files.createTempDirectory("tmp.defold.resourcepack_");
        FileOutputStream outputStreamManifest = new FileOutputStream(filepathManifest);
        try {
            System.out.println("Writing " + filepathArchiveIndex.getCanonicalPath());
            System.out.println("Writing " + filepathArchiveData.getCanonicalPath());

            archiveBuilder.write(archiveIndex, archiveData, resourcePackDirectory, excludedResources);
            manifestBuilder.setArchiveIdentifier(archiveBuilder.getArchiveIndexHash());

            System.out.println("Writing " + filepathManifest.getCanonicalPath());
            byte[] manifestFile = manifestBuilder.buildManifest();
            outputStreamManifest.write(manifestFile);

            if (doOutputManifestHashFile) {
                if (filepathManifestHash.exists()) {
                    filepathManifestHash.delete();
                    filepathManifestHash.createNewFile();
                }
                FileOutputStream manifestHashOutoutStream = new FileOutputStream(filepathManifestHash);
                manifestHashOutoutStream.write(manifestBuilder.getManifestDataHash());
                manifestHashOutoutStream.close();
            }

            PublisherSettings settings = new PublisherSettings();
            settings.setZipFilepath(dirpathRoot.getAbsolutePath());

            ZipPublisher publisher = new ZipPublisher(dirpathRoot.getAbsolutePath(), settings);
            String rootDir = resourcePackDirectory.toAbsolutePath().toString();
            publisher.setFilename(filepathZipArchive.getName());
            for (File fhandle : (new File(rootDir)).listFiles()) {
                if (fhandle.isFile()) {
                    publisher.AddEntry(fhandle, new ArchiveEntry(rootDir, fhandle.getAbsolutePath()));
                }
            }

            String liveupdateManifestFilename = "liveupdate.game.dmanifest";
            File luManifestFile = new File(dirpathRoot, liveupdateManifestFilename);
            FileUtils.copyFile(filepathManifest, luManifestFile);
            publisher.AddEntry(luManifestFile, new ArchiveEntry(dirpathRoot.getAbsolutePath(), luManifestFile.getAbsolutePath()));
            publisher.Publish();

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
