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

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.file.Files;
import java.security.NoSuchAlgorithmException;

import org.apache.commons.io.FileUtils;
import org.junit.Test;

import com.dynamo.bob.Project;
import com.dynamo.bob.archive.ManifestBuilder;
import com.dynamo.bob.fs.DefaultFileSystem;
import com.dynamo.bob.pipeline.graph.ResourceGraph;
import com.dynamo.bob.pipeline.graph.ResourceNode;
import com.dynamo.liveupdate.proto.Manifest.HashAlgorithm;
import com.dynamo.liveupdate.proto.Manifest.HashDigest;
import com.dynamo.liveupdate.proto.Manifest.ManifestData;
import com.dynamo.liveupdate.proto.Manifest.ManifestFile;
import com.dynamo.liveupdate.proto.Manifest.ManifestHeader;
import com.dynamo.liveupdate.proto.Manifest.ResourceEntry;
import com.dynamo.liveupdate.proto.Manifest.ResourceEntryFlag;
import com.dynamo.liveupdate.proto.Manifest.SignAlgorithm;
import com.google.protobuf.ByteString;

public class ManifestTest {

    private class ManifestInstance {

        public final String projectIdentifier = "Defold test";
        public final String[] supportedEngineVersions = { "alpha", "beta", "gamma", "delta" };
        public final String[][] resources;
        public final ResourceGraph resourceGraph;

        public ManifestBuilder manifestBuilder = new ManifestBuilder();

        public ManifestHeader manifestHeader = null;
        public ManifestData manifestData = null;
        public ManifestFile manifestFile = null;
        public byte[] manifest = null;

        public ManifestInstance() throws IOException {
            this.resources = this.createResources();
            this.resourceGraph = this.createResourceGraph();
            manifestBuilder.setResourceHashAlgorithm(HashAlgorithm.HASH_SHA1);
            manifestBuilder.setSignatureHashAlgorithm(HashAlgorithm.HASH_SHA1);
            manifestBuilder.setSignatureSignAlgorithm(SignAlgorithm.SIGN_RSA);
            manifestBuilder.setProjectIdentifier(projectIdentifier);
            manifestBuilder.setResourceGraph(this.resourceGraph);

            for (String supportedEngineVersion : this.supportedEngineVersions) {
                manifestBuilder.addSupportedEngineVersion(supportedEngineVersion);
            }

            for (String[] entry : this.resources) {
                byte[] data = entry[1].getBytes();
                manifestBuilder.addResourceEntry(entry[0], data, data.length, data.length, ResourceEntryFlag.BUNDLED.getNumber());
            }

            this.manifestHeader = manifestBuilder.buildManifestHeader();
            this.manifestData = manifestBuilder.buildManifestData();
            this.manifestFile = manifestBuilder.buildManifestFile();
            this.manifest = manifestBuilder.buildManifest();
        }

        private String[][] createResources() {
            String[][] resources = {
                    { "/main/main.collectionc", "1" },
                    { "/main/main.goc", "2" },
                    { "/main/main.scriptc", "3" },
                    { "/main/level1.collectionproxyc", "4" },
                    { "/main/level1.collectionc", "5" },
                    { "/main/level1.goc", "5" },
                    { "/main/level1.scriptc", "6" },
                    { "/main/level2.collectionproxyc", "7" },
                    { "/main/level2.collectionc", "8" },
                    { "/main/level2.goc", "9" },
                    { "/main/level2.soundc", "10" },
                    { "/main/shared_go.goc", "11" },
                    { "/main/shared_go.scriptc", "12" }
            };

            return resources;
        }

        private ResourceGraph createResourceGraph() {
            Project project = new Project(new DefaultFileSystem());
            ResourceGraph graph = new ResourceGraph(project);

            ResourceNode root = graph.getRootNode();
            ResourceNode mainCollection = graph.add("/main/main.collectionc", root);
            ResourceNode mainGo = graph.add("/main/main.goc", mainCollection);
            graph.add("/main/main.scriptc", mainGo);
            ResourceNode dynamicCollection = graph.add("/main/dynamic.collectionc", mainCollection);
            ResourceNode dynamicGo = graph.add("/main/dynamic.goc", dynamicCollection);
            ResourceNode sharedGo = graph.add("/main/shared_go.goc", mainCollection);

            ResourceNode level1Proxy = graph.add("/main/level1.collectionproxyc", mainGo);
            ResourceNode level1Collection = graph.add("/main/level1.collectionc", level1Proxy);
            level1Proxy.setType(ResourceNode.Type.ExcludedCollectionProxy);
            level1Collection.setType(ResourceNode.Type.ExcludedCollection);
            ResourceNode level1Go = graph.add("/main/level1.goc", level1Collection);
            graph.add("/main/level1.scriptc", level1Go);

            ResourceNode level2Proxy = graph.add("/main/level2.collectionproxyc", level1Go);
            ResourceNode level2Collection = graph.add("/main/level2.collectionc", level2Proxy);
            graph.add("/main/level2.goc", level2Collection);
            graph.add("/main/level2.soundc", level2Collection);

            graph.add(dynamicGo, level1Collection);
            graph.add(dynamicGo, level2Collection);
            graph.add(sharedGo, level1Go);

            graph.findAllResourcesReferencedFromMainCollection();
            return graph;
        }

        public HashDigest projectIdentifierHash() {
            try {
                return ManifestBuilder.CryptographicOperations.createHashDigest(projectIdentifier.getBytes(), HashAlgorithm.HASH_SHA1);
            } catch (Exception exception) {
                throw new RuntimeException("Unable to create hash for project identifier", exception);
            }
        }

        public HashDigest supportedEngineVersionHash(int index) {
            try {
                HashDigest.Builder builder = HashDigest.newBuilder();
                byte[] hash = ManifestBuilder.CryptographicOperations.hash(this.supportedEngineVersions[index].getBytes(), HashAlgorithm.HASH_SHA1);
                return builder.setData(ByteString.copyFrom(hash)).build();
            } catch (Exception exception) {
                throw new RuntimeException("Unable to create hash for engine version", exception);
            }
        }
    }

    private byte[] intArrayToByteArray(int[] array) {
        byte[] result = new byte[array.length];
        for (int i = 0; i < array.length; ++i) {
            result[i] = (byte) array[i];
        }

        return result;
    }

    @Test
    public void testCreateHash_MD5() throws NoSuchAlgorithmException {
        String data = "defold";
        byte[] actual = ManifestBuilder.CryptographicOperations.hash(data.getBytes(), HashAlgorithm.HASH_MD5);
        byte[] expected = intArrayToByteArray(
                new int[] { 0x01, 0x75, 0x7d, 0xd6, 0x17, 0x3a, 0x9e, 0x1b, 0x01, 0x71, 0x4b, 0xb5, 0x84, 0xef, 0x00, 0xe5 });

        assertArrayEquals(expected, actual);
    }

    @Test
    public void testCreateHash_SHA1() throws NoSuchAlgorithmException {
        String data = "defold";
        byte[] actual = ManifestBuilder.CryptographicOperations.hash(data.getBytes(), HashAlgorithm.HASH_SHA1);
        byte[] expected = intArrayToByteArray(
                new int[] { 0x2c, 0x43, 0xff, 0xbe, 0x82, 0x00, 0x90, 0x8a, 0x0c, 0xbc, 0x9b, 0xde, 0xa0, 0x39, 0x36, 0x66, 0xe4, 0xfb, 0x56, 0xad });

        assertArrayEquals(expected, actual);
    }

    @Test
    public void testCreateHash_SHA256() throws NoSuchAlgorithmException {
        String data = "defold";
        byte[] actual = ManifestBuilder.CryptographicOperations.hash(data.getBytes(), HashAlgorithm.HASH_SHA256);
        byte[] expected = intArrayToByteArray(
                new int[] { 0xc8, 0xfa, 0x85, 0xe3, 0x1c, 0xb2, 0x12, 0x11, 0x97, 0xbb, 0xa7, 0xb1, 0x11, 0xf4, 0x31, 0x0e, 0x86, 0x22, 0xcb, 0x9d, 0x63, 0x79, 0x78, 0xfc, 0x60, 0x9d, 0xdc, 0x04, 0x66, 0x51, 0x2d, 0x8d });

        assertArrayEquals(expected, actual);
    }

    @Test
    public void testCreateHash_SHA512() throws NoSuchAlgorithmException {
        String data = "defold";
        byte[] actual = ManifestBuilder.CryptographicOperations.hash(data.getBytes(), HashAlgorithm.HASH_SHA512);
        byte[] expected = intArrayToByteArray(
                new int[] { 0xeb, 0x4f, 0xfa, 0xa4, 0xe0, 0x14, 0x2f, 0xa9, 0xd1, 0x61, 0xc7, 0xdb, 0x2a, 0x87, 0xd6, 0xd9, 0x67, 0x2b, 0x65, 0x2a, 0xa6, 0x02, 0xaa, 0xc3, 0x49, 0x04, 0x96, 0x12, 0x0f, 0xac, 0xdb, 0xf8, 0x66, 0xea, 0xaf, 0x47, 0x3a, 0x7a, 0x96, 0xd2, 0x6b, 0xb0, 0xd9, 0xb8, 0x60, 0x91, 0xb8, 0x55, 0xd9, 0x57, 0xc5, 0x64, 0xbc, 0x94, 0xfc, 0x61, 0x93, 0x70, 0xe1, 0x49, 0x98, 0x50, 0x51, 0x11 });

        assertArrayEquals(expected, actual);
    }

    @Test
    public void testReadManifestFile() throws IOException {
        ManifestInstance instance = new ManifestInstance();
        ManifestFile manifestFile = instance.manifestBuilder.buildManifestFile();

        File tmpFile = Files.createTempFile("test_tmp", "dmanifest").toFile();
        FileOutputStream fouts = new FileOutputStream(tmpFile);
        manifestFile.writeTo(fouts);
        fouts.close();

        FileInputStream fins = new FileInputStream(tmpFile);
        ManifestFile manifestFile2 = ManifestFile.parseFrom(fins);
        fins.close();

        assertTrue(manifestFile2.hasData());
        ManifestData data = ManifestData.parseFrom(manifestFile2.getData());
        ManifestHeader header = data.getHeader();

        assertEquals(ManifestBuilder.CONST_VERSION, manifestFile2.getVersion());
        assertEquals(HashAlgorithm.HASH_SHA1, header.getResourceHashAlgorithm());
        assertEquals(HashAlgorithm.HASH_SHA1, header.getSignatureHashAlgorithm());
        assertEquals(SignAlgorithm.SIGN_RSA, header.getSignatureSignAlgorithm());
        assertEquals(instance.projectIdentifierHash(), header.getProjectIdentifier());
        assertTrue(manifestFile2.getSignature().isEmpty());

        FileUtils.deleteQuietly(tmpFile);
    }

    @Test
    public void testBuildManifest_UnsignedSignature() throws IOException {
        ManifestInstance instance = new ManifestInstance();

        ManifestFile manifestFile = ManifestFile.parseFrom(instance.manifest);
        assertTrue(manifestFile.getSignature().isEmpty());
    }

    @Test
    public void testBuildManifest_ManifestHashOutput() throws IOException, NoSuchAlgorithmException {
        ManifestInstance instance = new ManifestInstance();
        ManifestBuilder manifestBuilder = new ManifestBuilder(true);
        manifestBuilder.setProjectIdentifier(instance.projectIdentifier);
        manifestBuilder.setResourceHashAlgorithm(HashAlgorithm.HASH_SHA1);
        manifestBuilder.setSignatureHashAlgorithm(HashAlgorithm.HASH_SHA1);
        manifestBuilder.setSignatureSignAlgorithm(SignAlgorithm.SIGN_RSA);
        manifestBuilder.setResourceGraph(instance.resourceGraph);
        for (String version : instance.supportedEngineVersions) {
            manifestBuilder.addSupportedEngineVersion(version);
        }
        for (String[] entry : instance.resources) {
            byte[] data = entry[1].getBytes();
            manifestBuilder.addResourceEntry(entry[0], data, data.length, data.length, ResourceEntryFlag.BUNDLED.getNumber());
        }

        ManifestFile manifestFile = manifestBuilder.buildManifestFile();
        byte[] expectedHash = ManifestBuilder.CryptographicOperations.hash(manifestFile.getData().toByteArray(), HashAlgorithm.HASH_SHA1);

        assertArrayEquals(expectedHash, manifestBuilder.getManifestDataHash());
        assertTrue(manifestFile.getSignature().isEmpty());
    }

    @Test
    public void testBuildManifest_ManifestHeader() throws IOException {
        ManifestInstance instance = new ManifestInstance();
        ManifestHeader header = instance.manifestHeader;

        assertEquals(HashAlgorithm.HASH_SHA1, header.getResourceHashAlgorithm());
        assertEquals(HashAlgorithm.HASH_SHA1, header.getSignatureHashAlgorithm());
        assertEquals(SignAlgorithm.SIGN_RSA, header.getSignatureSignAlgorithm());
        assertEquals(instance.projectIdentifierHash(), header.getProjectIdentifier());
    }

    @Test
    public void testBuildManifest_SupportedEngineVersions() throws IOException {
        ManifestInstance instance = new ManifestInstance();
        ManifestData data = instance.manifestData;

        assertEquals(4, data.getEngineVersionsCount());

        for (int i = 0; i < instance.supportedEngineVersions.length; ++i) {
            boolean foundMatch = false;
            HashDigest expected = instance.supportedEngineVersionHash(i);
            for (HashDigest candidate : data.getEngineVersionsList()) {
                if (expected.equals(candidate)) {
                    foundMatch = true;
                    break;
                }
            }

            assertTrue(foundMatch);
        }
    }

    private void printDeps(ManifestData data, ResourceEntry searchFor) {
        for (int i = 0; i < data.getResourcesCount(); ++i) {
            ResourceEntry current = data.getResources(i);
            for (long hash : searchFor.getDependantsList()) {
                if (hash == current.getUrlHash()) {
                    System.out.println(current.getUrl());
                }
            }
        }
    }

    @Test
    public void testCreateManifest_Resources() throws IOException {
        ManifestInstance instance = new ManifestInstance();
        ManifestData data = instance.manifestData;

        assertEquals(instance.resources.length, data.getResourcesCount());

        for (int i = 0; i < data.getResourcesCount(); ++i) {
            ResourceEntry current = data.getResources(i);
            if (current.getUrl().equals("/main/main.scriptc")) {
                assertEquals(0, current.getDependantsCount());
            }

            if (current.getUrl().equals("/main/main.collectionc")) {
                assertEquals(0, current.getDependantsCount());
            }

            if (current.getUrl().equals("/main/level1.collectionc")) {
                printDeps(data, current);
                assertEquals(3, current.getDependantsCount());
            }
        }
    }
}
