// Copyright 2020-2024 The Defold Foundation
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
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.NoSuchFileException;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.spec.InvalidKeySpecException;

import javax.crypto.BadPaddingException;
import javax.crypto.IllegalBlockSizeException;
import javax.crypto.NoSuchPaddingException;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.bob.archive.ManifestBuilder;
import com.dynamo.bob.Project;
import com.dynamo.bob.fs.DefaultFileSystem;
import com.dynamo.bob.pipeline.graph.ResourceNode;
import com.dynamo.bob.pipeline.graph.ResourceGraph;
import com.dynamo.liveupdate.proto.Manifest.HashAlgorithm;
import com.dynamo.liveupdate.proto.Manifest.HashDigest;
import com.dynamo.liveupdate.proto.Manifest.ManifestData;
import com.dynamo.liveupdate.proto.Manifest.ManifestFile;
import com.dynamo.liveupdate.proto.Manifest.ManifestHeader;
import com.dynamo.liveupdate.proto.Manifest.ResourceEntry;
import com.dynamo.liveupdate.proto.Manifest.ResourceEntryFlag;
import com.dynamo.liveupdate.proto.Manifest.SignAlgorithm;
import com.google.protobuf.ByteString;
import org.apache.commons.io.FileUtils;

public class ManifestTest {

    private class ManifestInstance {

        public final String projectIdentifier = "Defold test";
        public final String[] supportedEngineVersions = { "alpha", "beta", "gamma", "delta" };
        public final String privateKeyFilepath = "test/private_rsa_1024_1.der";
        public final String publicKeyFilepath = "test/public_rsa_1024_1.der";
        public final String[][] resources;
        public final ResourceGraph resourceGraph;
        public final PublicKey publicKey;

        public ManifestBuilder manifestBuilder = new ManifestBuilder();

        public ManifestHeader manifestHeader = null;
        public ManifestData manifestData = null;
        @SuppressWarnings("unused")
		public ManifestFile manifestFile = null;
        public byte[] manifest = null;

        public ManifestInstance() throws IOException, NoSuchAlgorithmException, InvalidKeySpecException {
            this.resources = this.createResources();
            this.resourceGraph = this.createResourceGraph();
            this.publicKey = ManifestBuilder.CryptographicOperations.loadPublicKey(this.publicKeyFilepath, SignAlgorithm.SIGN_RSA);
            manifestBuilder.setResourceHashAlgorithm(HashAlgorithm.HASH_SHA1);
            manifestBuilder.setSignatureHashAlgorithm(HashAlgorithm.HASH_SHA1);
            manifestBuilder.setSignatureSignAlgorithm(SignAlgorithm.SIGN_RSA);
            manifestBuilder.setProjectIdentifier(projectIdentifier);
            manifestBuilder.setPrivateKeyFilepath(privateKeyFilepath);
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

            /*
            root
            |
            +--/main/main.collectionc
               +--/main/main.goc
               |  +--/main/main.scriptc
               |
               +--/main/shared_go.goc
               |
               +--/main/dynamic.collectionc
               |  +--/main/dynamic.goc
               |
               +--/main/level1.collectionproxyc
                  +--/main/level1.collectionc
                     +--/main/dynamic.goc
                     +--/main/level1.goc
                     |  +--/main/level1.scriptc
                     |  +--/main/shared_go.goc
                     |
                     +--/main/level2.collectionproxyc
                        +--/main/level2.collectionc
                            +--/main/dynamic.goc
                            +--/main/level2.goc
                            +--/main/level2.soundc
            */

            ResourceNode root = graph.getRootNode();
            ResourceNode main_collectionc = graph.add("/main/main.collectionc", root);
            ResourceNode main_goc = graph.add("/main/main.goc", main_collectionc);
            ResourceNode main_scriptc = graph.add("/main/main.scriptc", main_goc);
            ResourceNode dynamic_collectionc = graph.add("/main/dynamic.collectionc", main_collectionc);
            ResourceNode dynamic_goc = graph.add("/main/dynamic.goc", dynamic_collectionc);
            ResourceNode shared_goc = graph.add("/main/shared_go.goc", main_collectionc);

            ResourceNode level1_collectionproxyc = graph.add("/main/level1.collectionproxyc", main_goc);
            ResourceNode level1_collectionc = graph.add("/main/level1.collectionc", level1_collectionproxyc);
            level1_collectionproxyc.setType(ResourceNode.Type.ExcludedCollectionProxy);
            ResourceNode level1_goc = graph.add("/main/level1.goc", level1_collectionc);
            ResourceNode level1_scriptc = graph.add("/main/level1.scriptc", level1_goc);

            ResourceNode level2_collectionproxyc = graph.add("/main/level2.collectionproxyc", level1_goc);
            ResourceNode level2_collectionc = graph.add("/main/level2.collectionc", level2_collectionproxyc);
            ResourceNode level2_goc = graph.add("/main/level2.goc", level2_collectionc);
            ResourceNode level2_soundc = graph.add("/main/level2.soundc", level2_goc);

            graph.add(dynamic_goc, level1_collectionc);
            graph.add(dynamic_goc, level2_collectionc);
            graph.add(shared_goc,  level1_goc);

            graph.findAllResourcesReferencedFromMainCollection();
            return graph;
        }

        public HashDigest projectIdentifierHash() {
            try {
                return ManifestBuilder.CryptographicOperations.createHashDigest(projectIdentifier.getBytes(), HashAlgorithm.HASH_SHA1);
            } catch (Exception exception) {
                System.out.println("TEST ERROR: Unable to create hash for project identifier!");
            }

            return null;
        }

        public HashDigest supportedEngineVersionHash(int index) {
            HashDigest result = null;
            try {
                HashDigest.Builder builder = HashDigest.newBuilder();
                byte[] hash = ManifestBuilder.CryptographicOperations.hash(this.supportedEngineVersions[index].getBytes(), HashAlgorithm.HASH_SHA1);
                ByteString content = ByteString.copyFrom(hash);
                result = builder.setData(content).build();
            } catch (Exception exception) {
                System.out.println("TEST ERROR: Unable to create hash for engine version!");
            }
            return result;
        }

    }

    private byte[] intArrayToByteArray(int[] array) {
        byte[] result = new byte[array.length];
        for (int i = 0; i < array.length; ++i) {
            result[i] = (byte) array[i];
        }

        return result;
    }


    @Before
    public void setUp() throws Exception {

    }

    @After
    public void tearDown() throws IOException {

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
    public void testEncrypt_RSA() throws NoSuchAlgorithmException, InvalidKeySpecException, IOException, InvalidKeyException, NoSuchPaddingException, IllegalBlockSizeException, BadPaddingException {
        String filepath = "test/private_rsa_1024_1.der";
        PrivateKey privateKey = ManifestBuilder.CryptographicOperations.loadPrivateKey(filepath, SignAlgorithm.SIGN_RSA);
        String data = "defold";
        byte[] ciphertext = ManifestBuilder.CryptographicOperations.encrypt(data.getBytes(), SignAlgorithm.SIGN_RSA, privateKey);

        assertNotNull(ciphertext);
        assertEquals(128, ciphertext.length);
    }

    @Test
    public void testDecrypt_RSA() throws NoSuchAlgorithmException, InvalidKeySpecException, IOException, InvalidKeyException, NoSuchPaddingException, IllegalBlockSizeException, BadPaddingException {
        PrivateKey privateKey = ManifestBuilder.CryptographicOperations.loadPrivateKey("test/private_rsa_1024_1.der", SignAlgorithm.SIGN_RSA);
        PublicKey publicKey = ManifestBuilder.CryptographicOperations.loadPublicKey("test/public_rsa_1024_1.der", SignAlgorithm.SIGN_RSA);
        String data = "defold";
        byte[] ciphertext = ManifestBuilder.CryptographicOperations.encrypt(data.getBytes(), SignAlgorithm.SIGN_RSA, privateKey);
        byte[] plaintext = ManifestBuilder.CryptographicOperations.decrypt(ciphertext, SignAlgorithm.SIGN_RSA, publicKey);

        assertNotNull(ciphertext);
        assertEquals(128, ciphertext.length);

        assertNotNull(plaintext);
        assertEquals(6, plaintext.length);

        assertArrayEquals(data.getBytes(), plaintext);
    }

    @Test
    public void testloadPrivateKey() throws NoSuchAlgorithmException, InvalidKeySpecException, IOException {
        String filepath = "test/private_rsa_1024_1.der";
        PrivateKey privateKey = ManifestBuilder.CryptographicOperations.loadPrivateKey(filepath, SignAlgorithm.SIGN_RSA);
        assertNotNull(privateKey);
    }

    @Test(expected=NoSuchFileException.class)
    public void testloadPrivateKey_InvalidFilepath() throws NoSuchAlgorithmException, InvalidKeySpecException, IOException {
        String filepath = "test/doesntexist.pkcs8";
        ManifestBuilder.CryptographicOperations.loadPrivateKey(filepath, SignAlgorithm.SIGN_RSA);
    }

    @Test(expected=InvalidKeySpecException.class)
    public void testloadPrivateKey_InvalidKeyFormat() throws NoSuchAlgorithmException, InvalidKeySpecException, IOException {
        String filepath = "test/private_rsa_1024_3.pem";
        ManifestBuilder.CryptographicOperations.loadPrivateKey(filepath, SignAlgorithm.SIGN_RSA);
    }

    @Test
    public void testSign_MD5() throws InvalidKeyException, NoSuchAlgorithmException, IllegalBlockSizeException, BadPaddingException, NoSuchPaddingException, InvalidKeySpecException, IOException {
        HashAlgorithm hashAlgorithm = HashAlgorithm.HASH_MD5;
        SignAlgorithm signAlgorithm = SignAlgorithm.SIGN_RSA;
        String filepath = "test/private_rsa_1024_1.der";
        PrivateKey privateKey = ManifestBuilder.CryptographicOperations.loadPrivateKey(filepath, signAlgorithm);
        String data = "defold";

        byte[] actual = ManifestBuilder.CryptographicOperations.sign(data.getBytes(), hashAlgorithm, signAlgorithm, privateKey);
        assertNotNull(actual);
        assertTrue(actual.length > 0);
    }

    @Test
    public void testSign_SHA1() throws InvalidKeyException, NoSuchAlgorithmException, IllegalBlockSizeException, BadPaddingException, NoSuchPaddingException, InvalidKeySpecException, IOException {
        HashAlgorithm hashAlgorithm = HashAlgorithm.HASH_SHA1;
        SignAlgorithm signAlgorithm = SignAlgorithm.SIGN_RSA;
        String filepath = "test/private_rsa_1024_1.der";
        PrivateKey privateKey = ManifestBuilder.CryptographicOperations.loadPrivateKey(filepath, signAlgorithm);
        String data = "defold";

        byte[] actual = ManifestBuilder.CryptographicOperations.sign(data.getBytes(), hashAlgorithm, signAlgorithm, privateKey);
        assertNotNull(actual);
        assertTrue(actual.length > 0);
    }

    @Test
    public void testSign_SHA256() throws InvalidKeyException, NoSuchAlgorithmException, IllegalBlockSizeException, BadPaddingException, NoSuchPaddingException, InvalidKeySpecException, IOException {
        HashAlgorithm hashAlgorithm = HashAlgorithm.HASH_SHA256;
        SignAlgorithm signAlgorithm = SignAlgorithm.SIGN_RSA;
        String filepath = "test/private_rsa_1024_1.der";
        PrivateKey privateKey = ManifestBuilder.CryptographicOperations.loadPrivateKey(filepath, signAlgorithm);
        String data = "defold";

        byte[] actual = ManifestBuilder.CryptographicOperations.sign(data.getBytes(), hashAlgorithm, signAlgorithm, privateKey);
        assertNotNull(actual);
        assertTrue(actual.length > 0);
    }

    @Test
    public void testSign_SHA512() throws InvalidKeyException, NoSuchAlgorithmException, IllegalBlockSizeException, BadPaddingException, NoSuchPaddingException, InvalidKeySpecException, IOException {
        HashAlgorithm hashAlgorithm = HashAlgorithm.HASH_SHA512;
        SignAlgorithm signAlgorithm = SignAlgorithm.SIGN_RSA;
        String filepath = "test/private_rsa_1024_1.der";
        PrivateKey privateKey = ManifestBuilder.CryptographicOperations.loadPrivateKey(filepath, signAlgorithm);
        String data = "defold";

        byte[] actual = ManifestBuilder.CryptographicOperations.sign(data.getBytes(), hashAlgorithm, signAlgorithm, privateKey);
        assertNotNull(actual);
        assertTrue(actual.length > 0);
    }

    @Test
    public void testSign_DecryptPublic() throws InvalidKeyException, NoSuchAlgorithmException, IllegalBlockSizeException, BadPaddingException, NoSuchPaddingException, InvalidKeySpecException, IOException {
        HashAlgorithm hashAlgorithm = HashAlgorithm.HASH_SHA1;
        SignAlgorithm signAlgorithm = SignAlgorithm.SIGN_RSA;
        String privateKeyFilepath = "test/private_rsa_1024_1.der";
        String publicKeyFilepath = "test/public_rsa_1024_1.der";
        PrivateKey privateKey = ManifestBuilder.CryptographicOperations.loadPrivateKey(privateKeyFilepath, signAlgorithm);
        PublicKey publicKey = ManifestBuilder.CryptographicOperations.loadPublicKey(publicKeyFilepath, signAlgorithm);
        String data = "defold";

        byte[] expected = ManifestBuilder.CryptographicOperations.hash(data.getBytes(), hashAlgorithm);
        byte[] signature = ManifestBuilder.CryptographicOperations.sign(data.getBytes(), hashAlgorithm, signAlgorithm, privateKey);
        byte[] actual = ManifestBuilder.CryptographicOperations.decrypt(signature, signAlgorithm, publicKey);

        assertNotNull(expected);
        assertNotNull(signature);
        assertNotNull(actual);

        assertArrayEquals(expected, actual);
    }

    @Test
    public void testReadManifestFile() throws NoSuchAlgorithmException, InvalidKeySpecException, IOException, InvalidKeyException, NoSuchPaddingException, IllegalBlockSizeException, BadPaddingException {
        ManifestInstance instance = new ManifestInstance();
        ManifestFile manifestFile = instance.manifestBuilder.buildManifestFile();

        File tmp_file = Files.createTempFile("test_tmp", "dmanifest").toFile();
        FileOutputStream fouts = new FileOutputStream(tmp_file);
        manifestFile.writeTo(fouts);
        fouts.close();

        FileInputStream fins = new FileInputStream(tmp_file);
        ManifestFile manifestFile2 = ManifestFile.parseFrom(fins);
        fins.close();

        assertEquals(true, manifestFile2.hasData());
        ManifestData data = ManifestData.parseFrom(manifestFile2.getData());
        ManifestHeader header = data.getHeader();

        assertEquals(ManifestBuilder.CONST_VERSION, manifestFile2.getVersion());
        assertEquals(HashAlgorithm.HASH_SHA1, header.getResourceHashAlgorithm());
        assertEquals(HashAlgorithm.HASH_SHA1, header.getSignatureHashAlgorithm());
        assertEquals(SignAlgorithm.SIGN_RSA, header.getSignatureSignAlgorithm());
        assertEquals(instance.projectIdentifierHash(), header.getProjectIdentifier());

        FileUtils.deleteQuietly(tmp_file);
    }

    @Test
    public void testBuildManifest_VerifySignature() throws NoSuchAlgorithmException, InvalidKeySpecException, IOException, InvalidKeyException, NoSuchPaddingException, IllegalBlockSizeException, BadPaddingException {
        ManifestInstance instance = new ManifestInstance();

        ManifestFile manifestFile = ManifestFile.parseFrom(instance.manifest);
        byte[] expected = ManifestBuilder.CryptographicOperations.hash(manifestFile.getData().toByteArray(), HashAlgorithm.HASH_SHA1);
        byte[] actual = ManifestBuilder.CryptographicOperations.decrypt(manifestFile.getSignature().toByteArray(), SignAlgorithm.SIGN_RSA, instance.publicKey);

        assertNotNull(expected);
        assertNotNull(actual);

        assertArrayEquals(expected, actual);
    }

    @Test
    public void testBuildManifest_ManifestHeader() throws NoSuchAlgorithmException, InvalidKeySpecException, IOException {
        ManifestInstance instance = new ManifestInstance();
        ManifestHeader header = instance.manifestHeader;

        assertEquals(HashAlgorithm.HASH_SHA1, header.getResourceHashAlgorithm());
        assertEquals(HashAlgorithm.HASH_SHA1, header.getSignatureHashAlgorithm());
        assertEquals(SignAlgorithm.SIGN_RSA, header.getSignatureSignAlgorithm());
        assertEquals(instance.projectIdentifierHash(), header.getProjectIdentifier());
    }

    @Test
    public void testBuildManifest_SupportedEngineVersions() throws NoSuchAlgorithmException, InvalidKeySpecException, IOException {
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

    // Help function for debug that prints paths
    private void printDeps(ManifestData data, ResourceEntry searchFor) {
        for (int i = 0; i < data.getResourcesCount(); ++i) {
            ResourceEntry current = data.getResources(i);
            for (long hash:searchFor.getDependantsList()) {
                if (hash == current.getUrlHash()) {
                    System.out.println(current.getUrl());
                }
            }
        }
    }

    @Test
    public void testCreateManifest_Resources() throws NoSuchAlgorithmException, InvalidKeySpecException, IOException {
        ManifestInstance instance = new ManifestInstance();
        ManifestData data = instance.manifestData;
        System.out.println(instance.resourceGraph.toJSON());

        assertEquals(instance.resources.length, data.getResourcesCount());

        for (int i = 0; i < data.getResourcesCount(); ++i) {
            ResourceEntry current = data.getResources(i);
            if (current.getUrl().equals("/main/main.scriptc")) {
                assertEquals(0, current.getDependantsCount());
            }

            if (current.getUrl().equals("/main/main.collectionc")) {
                assertEquals(0, current.getDependantsCount());
            }

            if (current.getUrl().equals("/main/level1.collectionproxyc")) {
                printDeps(data, current);
                assertEquals(4, current.getDependantsCount());
            }
        }
    }
}
