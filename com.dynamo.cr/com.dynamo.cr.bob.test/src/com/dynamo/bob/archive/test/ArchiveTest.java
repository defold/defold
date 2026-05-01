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

package com.dynamo.bob.archive.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.List;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.archive.ArchiveEntry;
import com.dynamo.bob.archive.ArchiveBuilder;
import com.dynamo.bob.archive.ArchiveReader;
import com.dynamo.bob.archive.ManifestBuilder;
import com.dynamo.bob.Project;
import com.dynamo.bob.util.FileUtil;
import com.dynamo.bob.fs.DefaultFileSystem;
import com.dynamo.bob.pipeline.graph.ResourceNode;
import com.dynamo.bob.pipeline.graph.ResourceGraph;

import com.dynamo.liveupdate.proto.Manifest.HashAlgorithm;

public class ArchiveTest {

    private String contentRoot;
    private File outputDarc;
    private File outputIndex;
    private File outputData;

    private ManifestBuilder manifestBuilder;

    private ResourceGraph resourceGraph;

    private Project project;

    private String createDummyFile(String dir, String filepath, byte[] data) throws IOException {
        File tmp = new File(Paths.get(FilenameUtils.concat(dir, filepath)).toString());
        tmp.getParentFile().mkdirs();
        FileUtil.deleteOnExit(tmp);

        FileOutputStream fos = new FileOutputStream(tmp);
        fos.write(data);
        fos.close();

        return tmp.getAbsolutePath();
    }

    private int bitwiseCompare(byte b1, byte b2)
    {
        // Need to compare bit by bit since the byte value is unsigned, but Java has
        // no unsigned types. We use masking to filter out sign extension when shifting
        // (https://en.wikipedia.org/wiki/Sign_extension).
        int ones = 0xFF;
        for(int i=7; i>=0; i--)
        {
            int mask = ones >> i;
            byte b1Shift = (byte) ((b1 >> i) & mask);
            byte b2Shift = (byte) ((b2 >> i) & mask);
            if(b1Shift > b2Shift)
                return 1;
            else if(b1Shift < b2Shift)
                return -1;
        }

        return 0;
    }

    private ResourceNode addEntry(String filename, String content, ArchiveBuilder archiveBuilder, ResourceNode parent) throws IOException {
        String filepath = createDummyFile(contentRoot, filename, filename.getBytes());
        archiveBuilder.add(filepath, true, false);
        return resourceGraph.add(filename, parent);
    }
    private ResourceNode addExcludedEntry(String filename, String content, ArchiveBuilder archiveBuilder, ResourceNode parent) throws IOException {
        ResourceNode current = addEntry(filename, content, archiveBuilder, parent);
        current.setType(ResourceNode.Type.ExcludedCollectionProxy);
        return current;
    }

    @Before
    public void setUp() throws Exception {
        project = new Project(new DefaultFileSystem());
        resourceGraph = new ResourceGraph(project);

        contentRoot = Files.createTempDirectory(null).toFile().getAbsolutePath();
        outputDarc = Files.createTempFile("tmp.darc", "").toFile();

        outputIndex = Files.createTempFile("tmp.defold", "arci").toFile();
        outputData = Files.createTempFile("tmp.defold", "arcd").toFile();

        manifestBuilder = new ManifestBuilder();
        manifestBuilder.setResourceHashAlgorithm(HashAlgorithm.HASH_SHA1);
    }

    @After
    public void tearDown() throws IOException {
        FileUtils.deleteDirectory(new File(contentRoot));
        FileUtils.deleteQuietly(outputDarc);

        FileUtils.deleteQuietly(outputIndex);
        FileUtils.deleteQuietly(outputData);
    }

    @Test
    public void testBuilderAndReader() throws IOException, CompileExceptionError
    {
        // Create
        ArchiveBuilder ab = new ArchiveBuilder(contentRoot, manifestBuilder, 4, project);
        ab.add(createDummyFile(contentRoot, "a.txt", "abc123".getBytes()));
        ab.add(createDummyFile(contentRoot, "b.txt", "apaBEPAc e p a".getBytes()));
        ab.add(createDummyFile(contentRoot, "c.txt", "åäöåäöasd".getBytes()));

        // Write
        RandomAccessFile outFileIndex = new RandomAccessFile(outputIndex, "rw");
        RandomAccessFile outFileData = new RandomAccessFile(outputData, "rw");
        outFileIndex.setLength(0);
        outFileData.setLength(0);
        ab.write(outFileIndex, outFileData, new ArrayList<String>());
        outFileIndex.close();
        outFileData.close();

        // Read
        ArchiveReader ar = new ArchiveReader(outputIndex.getAbsolutePath(), outputData.getAbsolutePath(), null);
        ar.read();
        ar.close();
    }

    @Test
    public void testEntriesOrder() throws IOException, CompileExceptionError {

        // Create
        ArchiveBuilder ab = new ArchiveBuilder(FilenameUtils.separatorsToSystem(contentRoot), manifestBuilder, 4, project);
        ab.add(FilenameUtils.separatorsToSystem(createDummyFile(contentRoot, "main/a.txt", "abc123".getBytes())), true, false);
        ab.add(FilenameUtils.separatorsToSystem(createDummyFile(contentRoot, "main2/a.txt", "apaBEPAc e p a".getBytes())), true, false);
        ab.add(FilenameUtils.separatorsToSystem(createDummyFile(contentRoot, "a.txt", "åäöåäöasd".getBytes())), true, false);

        // Write
        RandomAccessFile outFileIndex = new RandomAccessFile(outputIndex, "rw");
        RandomAccessFile outFileData = new RandomAccessFile(outputData, "rw");
        outFileIndex.setLength(0);
        outFileData.setLength(0);
        ab.write(outFileIndex, outFileData, new ArrayList<String>());
        outFileIndex.close();
        outFileData.close();

        // Read
        ArchiveReader ar = new ArchiveReader(outputIndex.getAbsolutePath(), outputData.getAbsolutePath(), null);
        ar.read();
        List<ArchiveEntry> entries = ar.getEntries();

        Boolean correctOrder = false;
        for(int i=1; i<entries.size(); i++) {
            ArchiveEntry ePrev = entries.get(i-1);
            ArchiveEntry eCurr = entries.get(i);
            correctOrder = false;
            for (int j = 0; j < eCurr.getHash().length; j++) {
                correctOrder = bitwiseCompare(eCurr.getHash()[j], ePrev.getHash()[j]) == 1;

                if(correctOrder)
                    break;
            }
            assertEquals(correctOrder, true);
        }

        ar.close();
    }

    @Test
    public void testArchiveIndexAlignment() throws IOException, CompileExceptionError {
        ArchiveBuilder instance = new ArchiveBuilder(FilenameUtils.separatorsToSystem(contentRoot), manifestBuilder, 4, project);
        for (int i = 1; i < 50; ++i) {
            String filename = "dummy" + Integer.toString(i);
            String content  = "dummy" + Integer.toString(i);
            byte[] archiveIndexMD5 = new byte[16];
            instance.add(FilenameUtils.separatorsToSystem(createDummyFile(contentRoot, filename, content.getBytes())));

            RandomAccessFile archiveIndex = new RandomAccessFile(outputIndex, "rw");
            RandomAccessFile archiveData = new RandomAccessFile(outputData, "rw");
            archiveIndex.setLength(0);
            archiveData.setLength(0);
            instance.write(archiveIndex, archiveData, new ArrayList<String>());
            archiveIndex.close();
            archiveData.close();

            archiveIndex = new RandomAccessFile(outputIndex, "r");

            archiveIndex.readInt();                     // Version
            archiveIndex.readInt();                     // Padding
            archiveIndex.readLong();                    // UserData
            int entrySize   = archiveIndex.readInt();   // EntrySize
            int entryOffset = archiveIndex.readInt();   // EntryOffset
            int hashOffset  = archiveIndex.readInt();   // HashOffset
            archiveIndex.readInt();                     // HashSize
            archiveIndex.read(archiveIndexMD5);         // Archive index MD5 identifier
            archiveIndex.close();

            assertEquals(48, hashOffset);
            assertEquals(48 + entrySize * ArchiveBuilder.HASH_MAX_LENGTH, entryOffset);
            assertTrue(entryOffset % 4 == 0);
            assertTrue(hashOffset % 4 == 0);
        }
    }

    @Test
    public void testLoadResourceData() throws Exception {
        byte[] content = "Hello, world".getBytes();
        String filepath = FilenameUtils.separatorsToSystem(createDummyFile(contentRoot, "resource.tmp", content));
        ArchiveBuilder instance = new ArchiveBuilder(FilenameUtils.separatorsToSystem(contentRoot), manifestBuilder, 4, project);

        byte[] actual = instance.loadResourceData(filepath);

        assertArrayEquals(content, actual);
    }

    @Test
    public void testCompressResourceData() throws Exception {
        byte[] content = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA".getBytes();
        ArchiveBuilder instance = new ArchiveBuilder(FilenameUtils.separatorsToSystem(contentRoot), manifestBuilder, 4, project);

        byte[] expected = { 31, 65, 1, 0, 75, 80, 65, 65, 65, 65, 65 };
        byte[] actual = instance.compressResourceData(content);

        assertArrayEquals(expected, actual);
    }

    @Test
    public void testShouldUseCompressedResourceData() throws Exception {
        ArchiveBuilder instance = new ArchiveBuilder(FilenameUtils.separatorsToSystem(contentRoot), manifestBuilder, 4, project);

        byte[] original = { 10, 10, 10, 10, 10 };
        byte[] compressed = { 10, 10, 10, 10 };
        assertTrue(instance.shouldUseCompressedResourceData(original, compressed));     // 0.80

        original = new byte[] { 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10 };
        compressed = new byte[] { 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10 };
        assertTrue(instance.shouldUseCompressedResourceData(original, compressed));     // 0.95

        original= new byte[] { 10, 10, 10, 10, 10, 10, 10, 10 };
        compressed = new byte[] { 10, 10, 10, 10, 10, 10, 10, 10, 10, 10 };
        assertFalse(instance.shouldUseCompressedResourceData(original, compressed));    // 1.25
    }

    @SuppressWarnings("unused")
    @Test
    public void testWriteArchive() throws Exception {
        ManifestBuilder manifestBuilder = new ManifestBuilder();
        manifestBuilder.setResourceHashAlgorithm(HashAlgorithm.HASH_MD5);
        manifestBuilder.setResourceGraph(resourceGraph);

        ArchiveBuilder instance = new ArchiveBuilder(FilenameUtils.separatorsToSystem(contentRoot), manifestBuilder, 4, project);

        ResourceNode root = resourceGraph.getRootNode();
        ResourceNode collection1 = addEntry("main.collectionc", "alpha", instance, root);
        ResourceNode collectionproxy1 = addExcludedEntry("main.collectionproxyc", "beta", instance, collection1);
        ResourceNode gameobject1 = addEntry("main.goc", "delta", instance, collectionproxy1);

        List<String> excludedResources = resourceGraph.createExcludedResourcesList();

        // Test
        RandomAccessFile outFileIndex = new RandomAccessFile(outputIndex, "rw");
        RandomAccessFile outFileData = new RandomAccessFile(outputData, "rw");
        instance.write(outFileIndex, outFileData, excludedResources);

        assertEquals(2, instance.getArchiveEntrySize());
        assertEquals("/main.collectionproxyc", instance.getArchiveEntry(0).getRelativeFilename());    // 987bcab01b929eb2c07877b224215c92
        assertEquals("/main.collectionc", instance.getArchiveEntry(1).getRelativeFilename());         // 2c1743a391305fbf367df8e4f069f9f9
    }

    @SuppressWarnings("unused")
    @Test
    public void testWriteArchive_SiblingProxies() throws Exception {
        ManifestBuilder manifestBuilder = new ManifestBuilder();
        manifestBuilder.setResourceHashAlgorithm(HashAlgorithm.HASH_MD5);
        manifestBuilder.setResourceGraph(resourceGraph);

        ArchiveBuilder instance = new ArchiveBuilder(FilenameUtils.separatorsToSystem(contentRoot), manifestBuilder, 4, project);

        ResourceNode root = resourceGraph.getRootNode();
        ResourceNode collection1 = addEntry("main.collectionc", "alpha", instance, root);
        ResourceNode collectionproxy1 = addEntry("level1.collectionproxyc", "beta", instance, collection1);
        ResourceNode collectionproxy2 = addExcludedEntry("level2.collectionproxyc", "delta", instance, collection1);
        ResourceNode gameobject1 = addEntry("level1.goc", "gamma", instance, collectionproxy1);
        ResourceNode gameobject2 = addEntry("level2.goc", "epsilon", instance, collectionproxy2);

        List<String> excludedResources = resourceGraph.createExcludedResourcesList();

        // Test
        RandomAccessFile outFileIndex = new RandomAccessFile(outputIndex, "rw");
        RandomAccessFile outFileData = new RandomAccessFile(outputData, "rw");
        instance.write(outFileIndex, outFileData, excludedResources);

        assertEquals(4, instance.getArchiveEntrySize());
        assertEquals("/level1.collectionproxyc", instance.getArchiveEntry(0).getRelativeFilename());  // 617905b1d0e858ca35230357710cf5f2
        assertEquals("/main.collectionc", instance.getArchiveEntry(1).getRelativeFilename());         // b32b3904944e63ed5a269caa47904645
        assertEquals("/level2.collectionproxyc", instance.getArchiveEntry(2).getRelativeFilename());  // bc05302047f95ca60709254556402710
        assertEquals("/level1.goc", instance.getArchiveEntry(3).getRelativeFilename());               // d25298c59a872b5bfd5473de7b36a4a4
    }

    @SuppressWarnings("unused")
    @Test
    public void testWriteArchive_SharedResourceExcludedProxy() throws Exception {
        ManifestBuilder manifestBuilder = new ManifestBuilder();
        manifestBuilder.setResourceHashAlgorithm(HashAlgorithm.HASH_MD5);
        manifestBuilder.setResourceGraph(resourceGraph);

        ArchiveBuilder instance = new ArchiveBuilder(FilenameUtils.separatorsToSystem(contentRoot), manifestBuilder, 4, project);

        ResourceNode root = resourceGraph.getRootNode();
        ResourceNode collection1 = addEntry("main.collectionc", "alpha", instance, root);
        ResourceNode collectionproxy1 = addExcludedEntry("level1.collectionproxyc", "beta", instance, collection1);
        ResourceNode collectionproxy2 = addEntry("level2.collectionproxyc", "delta", instance, collection1);
        ResourceNode gameobject1 = addEntry("shared.goc", "gamma", instance, collectionproxy1);
        ResourceNode gameobject2 = addEntry("shared.goc", "gamma", instance, collectionproxy2);

        List<String> excludedResources = resourceGraph.createExcludedResourcesList();

        // Test
        RandomAccessFile outFileIndex = new RandomAccessFile(outputIndex, "rw");
        RandomAccessFile outFileData = new RandomAccessFile(outputData, "rw");
        instance.write(outFileIndex, outFileData, excludedResources);

        assertEquals(4, instance.getArchiveEntrySize());
        assertEquals("/shared.goc", instance.getArchiveEntry(0).getRelativeFilename());
        assertEquals("/level1.collectionproxyc", instance.getArchiveEntry(1).getRelativeFilename());  // 617905b1d0e858ca35230357710cf5f2
        assertEquals("/main.collectionc", instance.getArchiveEntry(2).getRelativeFilename());         // b32b3904944e63ed5a269caa47904645
        assertEquals("/level2.collectionproxyc", instance.getArchiveEntry(3).getRelativeFilename());  // bc05302047f95ca60709254556402710
    }

    @SuppressWarnings("unused")
    @Test
    public void testWriteArchive_DeepProxies() throws Exception {
        ManifestBuilder manifestBuilder = new ManifestBuilder();
        manifestBuilder.setResourceHashAlgorithm(HashAlgorithm.HASH_MD5);
        manifestBuilder.setResourceGraph(resourceGraph);

        ArchiveBuilder instance = new ArchiveBuilder(FilenameUtils.separatorsToSystem(contentRoot), manifestBuilder, 4, project);

        ResourceNode root = resourceGraph.getRootNode();
        ResourceNode collection1 = addEntry("main.collectionc", "alpha", instance, root);
        ResourceNode collectionproxy1 = addEntry("level1.collectionproxyc", "beta", instance, collection1);
        ResourceNode collectionproxy2 = addExcludedEntry("level2.collectionproxyc", "delta", instance, collectionproxy1);
        ResourceNode gameobject1 = addEntry("level1.goc", "gamma", instance, collectionproxy1);
        ResourceNode gameobject2 = addEntry("level2.goc", "epsilon", instance, collectionproxy2);

        List<String> excludedResources = resourceGraph.createExcludedResourcesList();

        // Test
        RandomAccessFile outFileIndex = new RandomAccessFile(outputIndex, "rw");
        RandomAccessFile outFileData = new RandomAccessFile(outputData, "rw");
        instance.write(outFileIndex, outFileData, excludedResources);

        assertEquals(4, instance.getArchiveEntrySize());
        assertEquals("/level1.collectionproxyc", instance.getArchiveEntry(0).getRelativeFilename());  // 617905b1d0e858ca35230357710cf5f2
        assertEquals("/main.collectionc", instance.getArchiveEntry(1).getRelativeFilename());         // b32b3904944e63ed5a269caa47904645
        assertEquals("/level2.collectionproxyc", instance.getArchiveEntry(2).getRelativeFilename());  // bc05302047f95ca60709254556402710
        assertEquals("/level1.goc", instance.getArchiveEntry(3).getRelativeFilename());               // d25298c59a872b5bfd5473de7b36a4a4
    }

    @SuppressWarnings("unused")
    @Test
    public void testWriteArchive_ExcludedProxy() throws Exception {
        ManifestBuilder manifestBuilder = new ManifestBuilder();
        manifestBuilder.setResourceHashAlgorithm(HashAlgorithm.HASH_MD5);
        manifestBuilder.setResourceGraph(resourceGraph);

        ArchiveBuilder instance = new ArchiveBuilder(FilenameUtils.separatorsToSystem(contentRoot), manifestBuilder, 4, project);
        ResourceNode root = resourceGraph.getRootNode();
        ResourceNode collection1 = addEntry("main.collectionc", "alpha", instance, root);
        ResourceNode collectionproxy1 = addEntry("level1.collectionproxyc", "beta", instance, collection1);
        ResourceNode collectionproxy2 = addExcludedEntry("level2.collectionproxyc", "delta", instance, collection1);
        ResourceNode gameobject11 = addEntry("level1.goc", "gamma", instance, collectionproxy1); // should be bundled
        ResourceNode gameobject2 = addEntry("level2.goc", "epsilon", instance, collectionproxy2); // should be excluded

        List<String> excludedResources = resourceGraph.createExcludedResourcesList();

        // Test
        RandomAccessFile outFileIndex = new RandomAccessFile(outputIndex, "rw");
        RandomAccessFile outFileData = new RandomAccessFile(outputData, "rw");
        instance.write(outFileIndex, outFileData, excludedResources);

        assertEquals(4, instance.getArchiveEntrySize());
        assertEquals("/level1.collectionproxyc", instance.getArchiveEntry(0).getRelativeFilename());  // 617905b1d0e858ca35230357710cf5f2
        assertEquals("/main.collectionc", instance.getArchiveEntry(1).getRelativeFilename());         // b32b3904944e63ed5a269caa47904645
        assertEquals("/level2.collectionproxyc", instance.getArchiveEntry(2).getRelativeFilename());  // bc05302047f95ca60709254556402710
        assertEquals("/level1.goc", instance.getArchiveEntry(3).getRelativeFilename());               // d25298c59a872b5bfd5473de7b36a4a4

    }

    @SuppressWarnings("unused")
    @Test
    public void testWriteArchive_ResourceInBundledAndExcludedProxies() throws Exception {
        ManifestBuilder manifestBuilder = new ManifestBuilder();
        manifestBuilder.setResourceHashAlgorithm(HashAlgorithm.HASH_MD5);
        manifestBuilder.setResourceGraph(resourceGraph);

        ArchiveBuilder instance = new ArchiveBuilder(FilenameUtils.separatorsToSystem(contentRoot), manifestBuilder, 4, project);
        ResourceNode root = resourceGraph.getRootNode();
        ResourceNode collection1 = addEntry("main.collectionc", "alpha", instance, root);
        ResourceNode collectionproxy1 = addEntry("level1.collectionproxyc", "beta", instance, collection1);
        ResourceNode collectionproxy2 = addExcludedEntry("level2.collectionproxyc", "delta", instance, collection1);
        ResourceNode gameobject11 = addEntry("level1.goc", "gamma", instance, collectionproxy1); // should be bundled
        ResourceNode gameobject12 = addEntry("level1.goc", "gamma", instance, collectionproxy2);
        ResourceNode gameobject2 = addEntry("level2.goc", "epsilon", instance, collectionproxy2); // should be excluded
        ResourceNode gameobject3 = addEntry("level3.goc", "eta", instance, collectionproxy2); // should be excluded

        List<String> excludedResources = resourceGraph.createExcludedResourcesList();

        // Test
        RandomAccessFile outFileIndex = new RandomAccessFile(outputIndex, "rw");
        RandomAccessFile outFileData = new RandomAccessFile(outputData, "rw");
        instance.write(outFileIndex, outFileData, excludedResources);

        assertEquals(4, instance.getArchiveEntrySize());
        assertEquals("/level1.collectionproxyc", instance.getArchiveEntry(0).getRelativeFilename());  // 617905b1d0e858ca35230357710cf5f2
        assertEquals("/main.collectionc", instance.getArchiveEntry(1).getRelativeFilename());         // b32b3904944e63ed5a269caa47904645
        assertEquals("/level2.collectionproxyc", instance.getArchiveEntry(2).getRelativeFilename());  // bc05302047f95ca60709254556402710
        assertEquals("/level1.goc", instance.getArchiveEntry(3).getRelativeFilename());               // d25298c59a872b5bfd5473de7b36a4a4
    }

    @SuppressWarnings("unused")
    @Test
    public void testWriteArchive_DeepProxiesExcludeGrandparent() throws Exception {
        ManifestBuilder manifestBuilder = new ManifestBuilder();
        manifestBuilder.setResourceHashAlgorithm(HashAlgorithm.HASH_MD5);
        manifestBuilder.setResourceGraph(resourceGraph);

        ArchiveBuilder instance = new ArchiveBuilder(FilenameUtils.separatorsToSystem(contentRoot), manifestBuilder, 4, project);

        ResourceNode root = resourceGraph.getRootNode();
        ResourceNode collection1 = addEntry("main.collectionc", "alpha", instance, root);
        ResourceNode collectionproxy1 = addExcludedEntry("level1.collectionproxyc", "beta", instance, collection1);
        ResourceNode collectionproxy2 = addEntry("level2.collectionproxyc", "delta", instance, collectionproxy1);
        ResourceNode gameobject1 = addEntry("level1.goc", "gamma", instance, collectionproxy1);
        ResourceNode gameobject2 = addEntry("level2.goc", "epsilon", instance, collectionproxy2);

        List<String> excludedResources = resourceGraph.createExcludedResourcesList();

        // Test
        RandomAccessFile outFileIndex = new RandomAccessFile(outputIndex, "rw");
        RandomAccessFile outFileData = new RandomAccessFile(outputData, "rw");
        instance.write(outFileIndex, outFileData, excludedResources);

        assertEquals(2, instance.getArchiveEntrySize());
        assertEquals("/level1.collectionproxyc", instance.getArchiveEntry(0).getRelativeFilename());  // 617905b1d0e858ca35230357710cf5f2
        assertEquals("/main.collectionc", instance.getArchiveEntry(1).getRelativeFilename());         // b32b3904944e63ed5a269caa47904645
    }


}
