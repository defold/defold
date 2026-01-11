// Copyright 2020-2026 The Defold Foundation
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
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.HashSet;
import java.util.Set;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Future;
import java.util.concurrent.Executors;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;

import com.dynamo.bob.Project;
import com.dynamo.bob.fs.DefaultFileSystem;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.logging.Logger;
import com.dynamo.bob.pipeline.graph.ResourceNode;
import com.dynamo.bob.pipeline.graph.ResourceGraph;
import com.dynamo.liveupdate.proto.Manifest.HashAlgorithm;
import com.dynamo.liveupdate.proto.Manifest.SignAlgorithm;
import com.dynamo.liveupdate.proto.Manifest.ResourceEntryFlag;

import com.dynamo.bob.archive.publisher.PublisherSettings;
import com.dynamo.bob.archive.publisher.ZipPublisher;
import com.dynamo.bob.archive.publisher.Publisher;
import com.dynamo.bob.util.TimeProfiler;

import net.jpountz.lz4.LZ4Compressor;
import net.jpountz.lz4.LZ4Factory;

public class ArchiveBuilder {

    private static Logger logger = Logger.getLogger(ArchiveBuilder.class.getName());

    public static final int VERSION = 5;
    public static final int HASH_MAX_LENGTH = 64; // 512 bits
    public static final int MD5_HASH_DIGEST_BYTE_LENGTH = 16; // 128 bits

    private List<ArchiveEntry> entries = new ArrayList<ArchiveEntry>();
    private List<ArchiveEntry> excludedEntries;
    private List<ArchiveEntry> includedEntries;
    private Set<String> lookup = new HashSet<String>(); // To see if a resource has already been added
    private Map<String, String> hexDigestCache = new ConcurrentHashMap<>();
    private String root;
    private ManifestBuilder manifestBuilder = null;
    private LZ4Compressor lz4Compressor;
    private byte[] archiveIndexMD5 = new byte[MD5_HASH_DIGEST_BYTE_LENGTH];
    private int resourcePadding = 4;
    private boolean forceCompression = false; // for building unit tests to create test content

    private Project project;
    private Publisher publisher;
    private int nThreads;

    private byte[] archiveEntryPadding = new byte[11];

    public ArchiveBuilder(String root, ManifestBuilder manifestBuilder, int resourcePadding, Project project) {
        this.root = new File(root).getAbsolutePath();
        this.manifestBuilder = manifestBuilder;
        this.lz4Compressor = LZ4Factory.fastestInstance().highCompressor();
        this.resourcePadding = resourcePadding;
        this.project = project;
        this.publisher = project.getPublisher();
        Arrays.fill(archiveEntryPadding, (byte)0xED);
    }

    public int getResourcePadding() {
        return resourcePadding;
    }

    private void add(String fileName, boolean compress, boolean encrypt, boolean isLiveUpdate) throws IOException {
        ArchiveEntry e = new ArchiveEntry(root, fileName, compress, encrypt, isLiveUpdate);
        if (lookup.add(e.getRelativeFilename())) {
            entries.add(e);
        }
    }

    public void add(String fileName, boolean compress, boolean encrypt) throws IOException {
        add(fileName, compress, encrypt, false);
    }

    public void add(String fileName) throws IOException {
        add(fileName, false, false, false);
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

    public void setForceCompression(boolean forceCompression) {
        this.forceCompression = forceCompression;
    }

    public boolean getForceCompression() {
        return forceCompression;
    }

    public boolean shouldUseCompressedResourceData(byte[] original, byte[] compressed) {
        if (this.getForceCompression())
            return true;

        double ratio = (double) compressed.length / (double) original.length;
        return ratio <= 0.95;
    }

    public byte[] encryptResourceData(byte[] buffer) throws CompileExceptionError {
        return ResourceEncryption.encrypt(buffer);
    }

    public List<ArchiveEntry> getExcludedEntries() {
        return excludedEntries;
    }
    public List<ArchiveEntry> getIncludedEntries() {
        return includedEntries;
    }

    private void writeArchiveEntry(RandomAccessFile archiveData, ArchiveEntry entry, List<String> excludedResources, ConcurrentHashMap<String, ArchiveEntry> writtenIntoArcd) throws IOException, CompileExceptionError {
        byte[] buffer = this.loadResourceData(entry.getFilename());

        int resourceEntryFlags = 0;

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

        // we need to do this last or the compression won't work as well
        if (entry.isEncrypted()) {
            buffer = this.encryptResourceData(buffer);
            resourceEntryFlags |= ResourceEntryFlag.ENCRYPTED.getNumber();
        }

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

        entry.setHexDigest(hexDigest);
        hexDigestCache.put(entry.getRelativeFilename(), hexDigest);

        // Write resource to resource pack or data archive
        if (excludedResources.contains(normalisedPath)) {
            if (this.publisher != null) {
                byte[] header = ByteBuffer.allocate(16)
                    .putInt(entry.getSize()) // 4 bytes
                    .put((byte)entry.getFlags()) // 1 byte
                    .put(archiveEntryPadding) // 11 bytes
                    .array();
                entry.setHeader(header);
                this.publisher.publish(entry, buffer);
            }
            resourceEntryFlags |= ResourceEntryFlag.EXCLUDED.getNumber();
        } else {
            ArchiveEntry previousValue = writtenIntoArcd.putIfAbsent(hexDigest, entry);
            if (previousValue == null) {
                // synchronize on the archive data file so that multiple threads
                // do not write to it at the same time
                synchronized (archiveData) {
                    alignBuffer(archiveData, this.resourcePadding);
                    entry.setResourceOffset((int) archiveData.getFilePointer());
                    archiveData.write(buffer, 0, buffer.length);
                }
            }
            else {
                entry.setDuplicatedDataBlob(true);
            }
            resourceEntryFlags |= ResourceEntryFlag.BUNDLED.getNumber();
        }

        synchronized (manifestBuilder) {
            manifestBuilder.addResourceEntry(normalisedPath, buffer, entry.getSize(), entry.getCompressedSize(), resourceEntryFlags);
        }
    }

    private void writeArchiveIndex(RandomAccessFile archiveIndex, List<ArchiveEntry> archiveEntries) throws IOException {
        TimeProfiler.start("writeArchiveIndex");

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

        Collections.sort(archiveEntries); // Since it has a hash, it sorts on hash

        // Write sorted hashes to index file
        int hashOffset = (int) archiveIndex.getFilePointer();
        for(ArchiveEntry entry : archiveEntries) {
            archiveIndex.write(entry.getHash());
        }

        // Write sorted entries to index file
        int entryOffset = (int) archiveIndex.getFilePointer();
        alignBuffer(archiveIndex, 4);

        ByteBuffer indexBuffer = ByteBuffer.allocate(4 * 4 * archiveEntries.size());
        for (ArchiveEntry entry : archiveEntries) {
            indexBuffer.putInt(entry.getResourceOffset());
            indexBuffer.putInt(entry.getSize());
            indexBuffer.putInt(entry.getCompressedSize());
            indexBuffer.putInt(entry.getFlags());
        }
        archiveIndex.write(indexBuffer.array());

        byte[] archiveIndexMD5 = null;
        try {
            // Calc index file MD5 hash
            archiveIndex.seek(archiveIndexHeaderOffset);
            int num_bytes = (int) archiveIndex.length() - archiveIndexHeaderOffset;
            byte[] archiveIndexBytes = new byte[num_bytes];
            archiveIndex.readFully(archiveIndexBytes);
            archiveIndexMD5 = ManifestBuilder.CryptographicOperations.hash(archiveIndexBytes, HashAlgorithm.HASH_MD5);
            manifestBuilder.setArchiveIdentifier(archiveIndexMD5);
        } catch (NoSuchAlgorithmException e) {
            throw new IOException("Unable to create a Resource Pack, the hashing algorithm is not supported!");
        }

        // Update index header with offsets
        archiveIndex.seek(0);
        archiveIndex.writeInt(VERSION);
        archiveIndex.writeInt(0); // Pad
        archiveIndex.writeLong(0); // UserData
        archiveIndex.writeInt(archiveEntries.size());
        archiveIndex.writeInt(entryOffset);
        archiveIndex.writeInt(hashOffset);
        archiveIndex.writeInt(ManifestBuilder.CryptographicOperations.getHashSize(manifestBuilder.getResourceHashAlgorithm()));
        archiveIndex.write(archiveIndexMD5);

        TimeProfiler.stop();
    }

    // The flow of how a resource is found in the archive:
    // URL → url_hash ───> Manifest: url_hash → data_hash
    //                                            ↓
    //                    Archive Index: binary search over sorted array of data_hashes
    //                                            ↓
    //                    Archive Index: if found, use index to get Entry from parallel EntryData array
    //                                            ↓
    //                    EntryData = { offset, size, compressed_size, flags }
    //                                            ↓
    //                    Read bytes from .arcd (using offset via fseek or mmap)
    //                                            ↓
    //                    Optionally decompress and/or decrypt
    //                                            ↓
    //                    → Final in-memory resource ready for use
    //
    public void write(RandomAccessFile archiveIndex, RandomAccessFile archiveData, List<String> excludedResources) throws IOException, CompileExceptionError {
        // create the executor service to write entries in parallel
        int nThreads = project.getMaxCpuThreads();
        logger.info("Creating archive entries with a fixed thread pool executor using %d threads", nThreads);
        ExecutorService executorService = Executors.newFixedThreadPool(nThreads);

        Collections.sort(entries); // Since it has no hash, it sorts on path

        excludedEntries = new ArrayList<>(entries.size());
        includedEntries = new ArrayList<>(entries.size());
        ConcurrentHashMap<String, ArchiveEntry> writtenIntoArcd = new ConcurrentHashMap<>();

        // create archive entry write tasks
        List<Future<ArchiveEntry>> futures = new ArrayList<>(entries.size());
        for (int i = entries.size() - 1; i >= 0; --i) {
            ArchiveEntry entry = entries.get(i);
            String normalisedPath = FilenameUtils.separatorsToUnix(entry.getRelativeFilename());
            boolean excluded = excludedResources.contains(normalisedPath);
            if (excluded) {
                entry.setFlag(ArchiveEntry.FLAG_LIVEUPDATE);
                entries.remove(i);
                excludedEntries.add(entry);
            }
            else {
                includedEntries.add(entry);
            }
            Future<ArchiveEntry> future = executorService.submit(() -> {
                writeArchiveEntry(archiveData, entry, excludedResources, writtenIntoArcd);
                return entry;
            });
            futures.add(future);
        }

        // wait for all tasks to finish
        try {
            for (Future<ArchiveEntry> future : futures) {
                ArchiveEntry entry = future.get();
            }
        }
        catch (Exception e) {
            throw new CompileExceptionError("Error while writing archive", e);
        }
        finally {
            executorService.shutdownNow();
            archiveData.close();
        }

        entries = new ArrayList<ArchiveEntry>(writtenIntoArcd.values());
        writeArchiveIndex(archiveIndex, entries);
    }

    private void alignBuffer(RandomAccessFile outFile, int align) throws IOException {
        int pos = (int) outFile.getFilePointer();
        int newPos = (int) (outFile.getFilePointer() + (align - 1));
        newPos &= ~(align - 1);

        for (int i = 0; i < (newPos - pos); ++i) {
            outFile.writeByte((byte) 0);
        }
    }

    /**
     * Get all cached hex digests as a lookup between absolute filepath and
     * digest.
     * @return Map with hex digests, keyed on absolute filepath
     */
    public Map<String, String> getCachedHexDigests() {
        return hexDigestCache;
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

        Project project = new Project(new DefaultFileSystem());
        ResourceGraph resourceGraph = new ResourceGraph(project);
        manifestBuilder.setResourceGraph(resourceGraph);

        ResourceNode rootNode = resourceGraph.getRootNode();

        List<String> excludedResources = new ArrayList<String>();

        // set up publisher - has to be done before creating the ArchiveBuilder
        PublisherSettings settings = new PublisherSettings();
        settings.setZipFilepath(dirpathRoot.getAbsolutePath());
        ZipPublisher publisher = new ZipPublisher(dirpathRoot.getAbsolutePath(), settings);
        project.setPublisher(publisher);
        publisher.setFilename(filepathZipArchive.getName());

        int archivedEntries = 0;
        String dirpathRootString = dirpathRoot.toString();
        ArchiveBuilder archiveBuilder = new ArchiveBuilder(dirpathRoot.toString(), manifestBuilder, 4, project);
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
            ResourceNode currentNode = new ResourceNode(currentInput.getPath());
            rootNode.addChild(currentNode);
        }
        System.out.println("Added " + Integer.toString(archivedEntries + excludedResources.size()) + " entries to archive (" + Integer.toString(excludedResources.size()) + " entries tagged as 'liveupdate' in archive).");

        RandomAccessFile archiveIndex = new RandomAccessFile(filepathArchiveIndex, "rw");
        RandomAccessFile archiveData  = new RandomAccessFile(filepathArchiveData, "rw");
        archiveIndex.setLength(0);
        archiveData.setLength(0);

        publisher.start();
        FileOutputStream outputStreamManifest = new FileOutputStream(filepathManifest);
        try {
            System.out.println("Writing " + filepathArchiveIndex.getCanonicalPath());
            System.out.println("Writing " + filepathArchiveData.getCanonicalPath());

            archiveBuilder.write(archiveIndex, archiveData, excludedResources);

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

            String liveupdateManifestFilename = "liveupdate.game.dmanifest";
            File luManifestFile = new File(dirpathRoot, liveupdateManifestFilename);
            FileUtils.copyFile(filepathManifest, luManifestFile);
            publisher.publish(new ArchiveEntry(dirpathRoot.getAbsolutePath(), luManifestFile.getAbsolutePath()), luManifestFile);

        } finally {
            try {
                publisher.stop();
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
