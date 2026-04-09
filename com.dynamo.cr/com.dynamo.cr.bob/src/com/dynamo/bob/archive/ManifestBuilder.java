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

package com.dynamo.bob.archive;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.TreeSet;

import com.dynamo.bob.Bob;
import com.dynamo.bob.logging.Logger;
import com.dynamo.bob.pipeline.graph.ResourceGraph;
import com.dynamo.bob.pipeline.graph.ResourceNode;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.bob.util.TimeProfiler;
import com.dynamo.liveupdate.proto.Manifest.HashAlgorithm;
import com.dynamo.liveupdate.proto.Manifest.HashDigest;
import com.dynamo.liveupdate.proto.Manifest.ManifestData;
import com.dynamo.liveupdate.proto.Manifest.ManifestFile;
import com.dynamo.liveupdate.proto.Manifest.ManifestHeader;
import com.dynamo.liveupdate.proto.Manifest.ResourceEntry;
import com.dynamo.liveupdate.proto.Manifest.ResourceEntryFlag;
import com.dynamo.liveupdate.proto.Manifest.SignAlgorithm;
import com.google.protobuf.ByteString;

public class ManifestBuilder {

    private static Logger logger = Logger.getLogger(ManifestBuilder.class.getName());

    public static class CryptographicOperations {

        private CryptographicOperations() {
        }

        public static byte[] hash(byte[] data, HashAlgorithm algorithm) throws NoSuchAlgorithmException {
            MessageDigest messageDigest = null;
            if (algorithm.equals(HashAlgorithm.HASH_MD5)) {
                messageDigest = MessageDigest.getInstance("MD5");
            } else if (algorithm.equals(HashAlgorithm.HASH_SHA1)) {
                messageDigest = MessageDigest.getInstance("SHA-1");
            } else if (algorithm.equals(HashAlgorithm.HASH_SHA256)) {
                messageDigest = MessageDigest.getInstance("SHA-256");
            } else if (algorithm.equals(HashAlgorithm.HASH_SHA512)) {
                messageDigest = MessageDigest.getInstance("SHA-512");
            } else {
                throw new NoSuchAlgorithmException("The algorithm specified is not supported!");
            }

            messageDigest.update(data);
            return messageDigest.digest();
        }

        public static String hexdigest(byte[] bytes) {
            char[] hexArray = "0123456789abcdef".toCharArray();
            char[] hexChars = new char[bytes.length * 2];
            for (int j = 0; j < bytes.length; j++) {
                int v = bytes[j] & 0xFF;
                hexChars[j * 2] = hexArray[v >>> 4];
                hexChars[j * 2 + 1] = hexArray[v & 0x0F];
            }

            return new String(hexChars);
        }

        public static int getHashSize(HashAlgorithm algorithm) {
            if (algorithm.equals(HashAlgorithm.HASH_MD5)) {
                return 128 / 8;
            } else if (algorithm.equals(HashAlgorithm.HASH_SHA1)) {
                return 160 / 8;
            } else if (algorithm.equals(HashAlgorithm.HASH_SHA256)) {
                return 256 / 8;
            } else if (algorithm.equals(HashAlgorithm.HASH_SHA512)) {
                return 512 / 8;
            }

            return 0;
        }

        public static HashDigest createHashDigest(byte[] data, HashAlgorithm algorithm) throws NoSuchAlgorithmException {
            byte[] hashDigest = CryptographicOperations.hash(data, algorithm);
            HashDigest.Builder builder = HashDigest.newBuilder();
            builder.setData(ByteString.copyFrom(hashDigest));
            return builder.build();
        }
    }

    public static final int CONST_MAGIC_NUMBER = 0x43cb6d06;
    public static final int CONST_VERSION = 0x05;

    private HashAlgorithm resourceHashAlgorithm = HashAlgorithm.HASH_UNKNOWN;
    private HashAlgorithm signatureHashAlgorithm = HashAlgorithm.HASH_UNKNOWN;
    private SignAlgorithm signatureSignAlgorithm = SignAlgorithm.SIGN_UNKNOWN;
    private String projectIdentifier = null;
    private String buildVariant = null;
    private ResourceGraph resourceGraph = null;
    private boolean outputManifestHash = false;
    private byte[] manifestDataHash = null;
    private byte[] archiveIdentifier = new byte[ArchiveBuilder.MD5_HASH_DIGEST_BYTE_LENGTH];
    private HashMap<ResourceNode, HashSet<ResourceNode>> pathToDependants = new HashMap<>();
    private HashMap<String, ResourceEntry> urlToResource = new HashMap<>();
    private Set<String> supportedEngineVersions = new HashSet<String>();
    private Set<ResourceEntry> resourceEntries = new TreeSet<ResourceEntry>(new Comparator<ResourceEntry>() {
        private int compare(byte[] left, byte[] right) {
            for (int i = 0, j = 0; i < left.length && j < right.length; i++, j++) {
                int a = (left[i] & 0xff);
                int b = (right[j] & 0xff);
                if (a != b) {
                    return a - b;
                }
            }
            return left.length - right.length;
        }

        public int compare(ResourceEntry s1, ResourceEntry s2) {
            byte[] s1Bytes = ByteBuffer.allocate(Long.SIZE / Byte.SIZE).putLong(s1.getUrlHash()).array();
            byte[] s2Bytes = ByteBuffer.allocate(Long.SIZE / Byte.SIZE).putLong(s2.getUrlHash()).array();
            return compare(s1Bytes, s2Bytes);
        }
    });

    public ManifestBuilder() {
    }

    public ManifestBuilder(boolean outputManifestHash) {
        this.outputManifestHash = outputManifestHash;
    }

    public byte[] getManifestDataHash() {
        return this.manifestDataHash;
    }

    public void setResourceHashAlgorithm(HashAlgorithm algorithm) {
        this.resourceHashAlgorithm = algorithm;
    }

    public HashAlgorithm getResourceHashAlgorithm() {
        return this.resourceHashAlgorithm;
    }

    public void setSignatureHashAlgorithm(HashAlgorithm algorithm) {
        this.signatureHashAlgorithm = algorithm;
    }

    public HashAlgorithm getSignatureHashAlgorithm() {
        return this.signatureHashAlgorithm;
    }

    public void setSignatureSignAlgorithm(SignAlgorithm algorithm) {
        this.signatureSignAlgorithm = algorithm;
    }

    public SignAlgorithm getSignAlgorithm() {
        return this.signatureSignAlgorithm;
    }

    public void setResourceGraph(ResourceGraph resourceGraph) {
        this.resourceGraph = resourceGraph;
    }

    public ResourceGraph getResourceGraph() {
        return this.resourceGraph;
    }

    public void setProjectIdentifier(String projectIdentifier) {
        this.projectIdentifier = projectIdentifier;
    }

    public void setBuildVariant(String variant) {
        buildVariant = variant;
    }

    public void setArchiveIdentifier(byte[] archiveIdentifier) {
        if (archiveIdentifier.length == ArchiveBuilder.MD5_HASH_DIGEST_BYTE_LENGTH) {
            this.archiveIdentifier = archiveIdentifier;
        }
    }

    public void addSupportedEngineVersion(String version) {
        version = version.replaceAll("^\"|\"$", "");
        this.supportedEngineVersions.add(version);
    }

    public void addResourceEntry(String url, byte[] data, int size, int compressedSize, int flags) throws IOException {
        try {
            ResourceEntry.Builder builder = ResourceEntry.newBuilder();
            builder.setUrl(url);
            builder.setUrlHash(MurmurHash.hash64(url));
            HashDigest hash = CryptographicOperations.createHashDigest(data, this.resourceHashAlgorithm);
            builder.setHash(hash);
            builder.setFlags(flags);
            builder.setSize(size);
            builder.setCompressedSize(compressedSize);
            this.resourceEntries.add(builder.buildPartial());
        } catch (NoSuchAlgorithmException exception) {
            throw new IOException("Unable to create Manifest, hashing algorithm is not supported!");
        }
    }

    public HashSet<ResourceNode> getAllDependants(ResourceNode node) throws IOException {
        if (node == null) {
            return new HashSet<ResourceNode>();
        }

        HashSet<ResourceNode> dependants = pathToDependants.get(node);
        if (dependants != null) {
            return dependants;
        }

        dependants = new HashSet<ResourceNode>();

        for (ResourceNode child : node.getChildren()) {
            dependants.add(child);

            if (!child.checkType(ResourceNode.Type.CollectionProxy)) {
                dependants.addAll(getAllDependants(child));
            }
        }

        pathToDependants.put(node, dependants);
        return dependants;
    }

    public ManifestHeader buildManifestHeader() throws IOException {
        HashDigest projectIdentifierHash = null;
        try {
            projectIdentifierHash = CryptographicOperations.createHashDigest(this.projectIdentifier.getBytes(), HashAlgorithm.HASH_SHA1);
        } catch (NoSuchAlgorithmException exception) {
            throw new IOException("Unable to create ManifestHeader, hashing algorithm is not supported!");
        }

        ManifestHeader.Builder builder = ManifestHeader.newBuilder();
        builder.setProjectIdentifier(projectIdentifierHash);
        builder.setResourceHashAlgorithm(this.resourceHashAlgorithm);
        builder.setSignatureHashAlgorithm(this.signatureHashAlgorithm);
        builder.setSignatureSignAlgorithm(this.signatureSignAlgorithm);
        return builder.build();
    }

    private List<ResourceEntry> getManifestEntries(boolean stripExcludedEntries) {
        List<ResourceEntry> manifestEntries = new ArrayList<>();
        for (ResourceEntry entry : this.resourceEntries) {
            if (stripExcludedEntries && (entry.getFlags() & ResourceEntryFlag.EXCLUDED.getNumber()) != 0) {
                continue;
            }
            manifestEntries.add(entry);
        }
        return manifestEntries;
    }

    private void buildUrlToResourceMap(List<ResourceEntry> entries) throws IOException {
        urlToResource.clear();
        for (ResourceEntry entry : entries) {
            if (entry.hasHash()) {
                urlToResource.put(entry.getUrl(), entry);
            } else {
                throw new IOException(String.format("Unable to create ManifestData, an incomplete resource was found (missing hash)! %s", entry.getUrl()));
            }
        }
    }

    public ManifestData buildManifestData() throws IOException {
        return buildManifestData(false);
    }

    public ManifestData buildManifestData(boolean stripExcludedEntries) throws IOException {
        TimeProfiler.start("buildManifestData");
        logger.info("buildManifestData begin");
        long tstart = System.currentTimeMillis();

        ManifestData.Builder builder = ManifestData.newBuilder();

        ManifestHeader manifestHeader = this.buildManifestHeader();
        builder.setHeader(manifestHeader);

        List<ResourceEntry> manifestEntries = getManifestEntries(stripExcludedEntries);
        buildUrlToResourceMap(manifestEntries);

        List<String> sortedEngineVersions = new ArrayList<>(this.supportedEngineVersions);
        Collections.sort(sortedEngineVersions);
        for (String version : sortedEngineVersions) {
            try {
                byte[] hashBytes = CryptographicOperations.hash(version.getBytes(), HashAlgorithm.HASH_SHA1);
                HashDigest.Builder digestBuilder = HashDigest.newBuilder();
                digestBuilder.setData(ByteString.copyFrom(hashBytes));
                builder.addEngineVersions(digestBuilder.build());
            } catch (NoSuchAlgorithmException e) {
                System.out.println("Algorithm not found when adding supported engine versions to manifest, msg: " + e.getMessage());
            } catch (Exception e) {
                System.out.println("Failed to add supported engine versions to manifest, msg: " + e.getMessage());
            }
        }

        for (ResourceEntry entry : manifestEntries) {
            String url = entry.getUrl();
            ResourceEntry.Builder resourceEntryBuilder = entry.toBuilder();

            ResourceNode node = resourceGraph.getResourceNodeFromPath(url);
            if (node != null && node.checkType(ResourceNode.Type.ExcludedCollection)) {
                HashSet<ResourceNode> allCollectionDependants = this.getAllDependants(node);
                for (ResourceNode dependant : allCollectionDependants) {
                    if (dependant.isInMainBundle()) {
                        continue;
                    }
                    ResourceEntry resource = urlToResource.get(dependant.getPath());
                    if (resource == null) {
                        continue;
                    }
                    resourceEntryBuilder.addDependants(resource.getUrlHash());
                }
            }
            if (buildVariant != null && buildVariant.equals(Bob.VARIANT_RELEASE)) {
                resourceEntryBuilder.setUrl("");
            }
            builder.addResources(resourceEntryBuilder.build());
        }

        long tend = System.currentTimeMillis();
        logger.info("ManifestBuilder.buildManifestData took %f", (tend - tstart) / 1000.0);
        TimeProfiler.stop();
        return builder.build();
    }

    public ManifestFile buildManifestFile() throws IOException {
        return buildManifestFile(false);
    }

    public ManifestFile buildManifestFile(boolean stripExcludedEntries) throws IOException {
        TimeProfiler.start("buildManifestFile");
        ManifestFile.Builder builder = ManifestFile.newBuilder();

        ManifestData manifestData = this.buildManifestData(stripExcludedEntries);
        byte[] manifestBytes = manifestData.toByteArray();
        builder.setData(ByteString.copyFrom(manifestBytes));
        builder.setArchiveIdentifier(ByteString.copyFrom(this.archiveIdentifier));
        builder.setVersion(ManifestBuilder.CONST_VERSION);
        builder.setSignature(ByteString.EMPTY);

        try {
            if (this.outputManifestHash) {
                this.manifestDataHash = CryptographicOperations.hash(manifestBytes, this.signatureHashAlgorithm);
            }
        } catch (NoSuchAlgorithmException exception) {
            throw new IOException("Unable to create ManifestFile, hashing algorithm is not supported!");
        } finally {
            TimeProfiler.stop();
        }
        return builder.build();
    }

    public byte[] buildManifest() throws IOException {
        return this.buildManifest(false);
    }

    public byte[] buildManifest(boolean stripExcludedEntries) throws IOException {
        return this.buildManifestFile(stripExcludedEntries).toByteArray();
    }
}
