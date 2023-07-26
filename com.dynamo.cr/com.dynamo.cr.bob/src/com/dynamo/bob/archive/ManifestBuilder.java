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

package com.dynamo.bob.archive;

import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.security.MessageDigest;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.spec.InvalidKeySpecException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Set;
import java.util.TreeSet;

import javax.crypto.BadPaddingException;
import javax.crypto.IllegalBlockSizeException;
import javax.crypto.NoSuchPaddingException;
import javax.security.auth.DestroyFailedException;

import com.dynamo.bob.pipeline.ResourceNode;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.bob.util.CryptographicOperations;
import com.dynamo.bob.logging.Logger;
import com.dynamo.liveupdate.proto.Manifest.HashAlgorithm;
import com.dynamo.liveupdate.proto.Manifest.HashDigest;
import com.dynamo.liveupdate.proto.Manifest.ManifestData;
import com.dynamo.liveupdate.proto.Manifest.ManifestFile;
import com.dynamo.liveupdate.proto.Manifest.ManifestHeader;
import com.dynamo.liveupdate.proto.Manifest.ResourceEntry;
import com.dynamo.liveupdate.proto.Manifest.SignAlgorithm;
import com.google.protobuf.ByteString;

public class ManifestBuilder {

    private static Logger logger = Logger.getLogger(ManifestBuilder.class.getName());

    public static final int CONST_MAGIC_NUMBER = 0x43cb6d06;
    public static final int CONST_VERSION = 0x04;

    private HashAlgorithm resourceHashAlgorithm = HashAlgorithm.HASH_UNKNOWN;
    private HashAlgorithm signatureHashAlgorithm = HashAlgorithm.HASH_UNKNOWN;
    private SignAlgorithm signatureSignAlgorithm = SignAlgorithm.SIGN_UNKNOWN;
    private String privateKeyFilepath = null;
    private String publicKeyFilepath = null;
    private String projectIdentifier = null;
    private ResourceNode root = null;
    private boolean outputManifestHash = false;
    private byte[] manifestDataHash = null;
    private byte[] archiveIdentifier = new byte[ArchiveBuilder.MD5_HASH_DIGEST_BYTE_LENGTH];
    private Set<String> excludedResources = new HashSet<>();
    private HashMap<String, ResourceNode> pathToNode = new HashMap<>();
    private HashMap<String, List<String>> pathToDependants = new HashMap<>();
    private HashMap<String, ResourceEntry> urlToResource = new HashMap<>();
    private HashMap<String, List<ResourceNode>> pathToOccurrances = null; // We build it at first request
    private Set<HashDigest> supportedEngineVersions = new HashSet<HashDigest>();
    private Set<ResourceEntry> resourceEntries = new TreeSet<ResourceEntry>(new Comparator<ResourceEntry>() {
        // We need to make sure the entries are sorted properly in order to do the binary search
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
            // We want a set sorted on hashes, so we can do binary search at runtime

            // byte compare, because java does not have unsigned types
            byte[] s1_bytes = ByteBuffer.allocate(Long.SIZE / Byte.SIZE).putLong(s1.getUrlHash()).array();
            byte[] s2_bytes = ByteBuffer.allocate(Long.SIZE / Byte.SIZE).putLong(s2.getUrlHash()).array();
            return compare(s1_bytes, s2_bytes);
        }
    });

    public ManifestBuilder() {

    }

    public ManifestBuilder(boolean outputManifestHash) {
        this.outputManifestHash = outputManifestHash;
    }

    public static ManifestBuilder create() {
        return create(false);
    }

    public static ManifestBuilder create(boolean outputManifestHash) {
        ManifestBuilder manifestBuilder =  new ManifestBuilder(outputManifestHash);
        manifestBuilder.setResourceHashAlgorithm(HashAlgorithm.HASH_SHA1);
        manifestBuilder.setSignatureHashAlgorithm(HashAlgorithm.HASH_SHA256);
        manifestBuilder.setSignatureSignAlgorithm(SignAlgorithm.SIGN_RSA);
        return manifestBuilder;
    }

    public MessageDigest getResourceHashDigest() throws NoSuchAlgorithmException {
        return CryptographicOperations.getMessageDigest(resourceHashAlgorithm);
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

    public void setRoot(ResourceNode root) {
        this.root = root;
    }

    public ResourceNode getRoot() {
        return this.root;
    }

    public void setPrivateKeyFilepath(String filepath) {
        this.privateKeyFilepath = filepath;
    }

    public String getPrivateKeyFilepath() {
        return this.privateKeyFilepath;
    }

    public void setPublicKeyFilepath(String filepath) {
        this.publicKeyFilepath = filepath;
    }

    public String getPublicKeyFilepath() {
        return this.publicKeyFilepath;
    }

    public void setProjectIdentifier(String projectIdentifier) {
        this.projectIdentifier = projectIdentifier;
    }

    public void setArchiveIdentifier(byte[] archiveIdentifier) {
        if (archiveIdentifier.length == ArchiveBuilder.MD5_HASH_DIGEST_BYTE_LENGTH) {
            this.archiveIdentifier = archiveIdentifier;
        }
    }

    public void addSupportedEngineVersion(String version) {
        try {
            // strip any leading or trailing quotation marks
            version = version.replaceAll("^\"|\"$", "");
            byte[] hashBytes = CryptographicOperations.hash(version.getBytes(), HashAlgorithm.HASH_SHA1);
            HashDigest.Builder builder = HashDigest.newBuilder();
            builder.setData(ByteString.copyFrom(hashBytes));
            this.supportedEngineVersions.add(builder.build());
        } catch (NoSuchAlgorithmException e) {
            System.out.println("Algorithm not found when adding supported engine versions to manifest, msg: " + e.getMessage());
        } catch (Exception e) {
            System.out.println("Failed to add supported engine versions to manifest, msg: " + e.getMessage());
        }
    }

    public void addResourceEntry(String url, byte[] hashDigest, int flags) throws IOException {
        try {
            ResourceEntry.Builder builder = ResourceEntry.newBuilder();
            builder.setUrl(url);
            builder.setUrlHash(MurmurHash.hash64(url)); // sort on this
            HashDigest hash = CryptographicOperations.toHashDigest(hashDigest);
            builder.setHash(hash);
            builder.setFlags(flags);
            this.resourceEntries.add(builder.buildPartial());
        } catch (NoSuchAlgorithmException exception) {
            throw new IOException("Unable to create Manifest, hashing algorithm is not supported!");
        }
    }

    private void buildResourceOccurrancesMap(ResourceNode node) {
        // The resource may occur at many instances in the tree
        // This map contains the mapping url -> occurrances (i.e. nodes)

        String key = node.relativeFilepath;
        if (!pathToOccurrances.containsKey(key)) {
            pathToOccurrances.put(key, new ArrayList<ResourceNode>());
        }

        List<ResourceNode> list = pathToOccurrances.get(key);
        list.add(node);

        for (ResourceNode child : node.getChildren()) {
            buildResourceOccurrancesMap(child);
        }
    }

    // Calculate all parent collection paths (to the root) for a resource
    // Resource could occur multiple times in the tree (referenced from several collections) or several times within the same collection
    public List<ArrayList<String>> getParentCollections(String filepath) {
        if (pathToOccurrances == null) {
            pathToOccurrances = new HashMap<>();

            ResourceNode root = getRoot();
            if (root != null) // for tests really
                buildResourceOccurrancesMap(root);
        }

        List<ArrayList<String>> result = new ArrayList<ArrayList<String>>();

        List<ResourceNode> candidates = pathToOccurrances.get(filepath);
        if (candidates == null)
            return result;

        int i = 0;
        while (!candidates.isEmpty()) {
            ResourceNode current = candidates.remove(0).getParent();
            result.add(new ArrayList<String>());
            while (current != null) {
                if (current.relativeFilepath.endsWith("collectionproxyc") ||
                    current.relativeFilepath.endsWith("collectionc")) {
                    result.get(i).add(current.relativeFilepath);
                }

                current = current.getParent();
            }
            ++i;
        }
        return result;
    }

    public List<String> getDependants(String filepath) throws IOException {
        /* Once a candidate has been found the children, the children, and so
           on are added to the list of dependants. If a CollectionProxy is
           found that resource itself is added to the list of dependants, but
           it is seen as a leaf and the Collection that it points to is ignored.

           The reason children of a CollectionProxy is ignored is that they are
           not required to load the parent Collection. This allows us to
           exclude an entire Collection that is loaded through a CollectionProxy
           and thus create a partial archive that has to be updated (through
           LiveUpdate) before that CollectionProxy can be loaded.
        */

        ResourceNode candidate = pathToNode.get(filepath);

        if (candidate == null)
            return new ArrayList<String>();

        List<String> dependants = pathToDependants.get(filepath);
        if (dependants != null) {
            return dependants;
        }

        dependants = new ArrayList<String>();

        for (ResourceNode child : candidate.getChildren()) {
            dependants.add(child.relativeFilepath);

            if (!child.relativeFilepath.endsWith("collectionproxyc")) {
                dependants.addAll(getDependants(child.relativeFilepath));
            }
        }

        pathToDependants.put(filepath, dependants);

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
        builder.setMagicNumber(ManifestBuilder.CONST_MAGIC_NUMBER);
        builder.setVersion(ManifestBuilder.CONST_VERSION);
        builder.setProjectIdentifier(projectIdentifierHash);
        builder.setResourceHashAlgorithm(this.resourceHashAlgorithm);
        builder.setSignatureHashAlgorithm(this.signatureHashAlgorithm);
        builder.setSignatureSignAlgorithm(this.signatureSignAlgorithm);
        return builder.build();
    }

    private void buildPathToNodeMap(ResourceNode node) {
        pathToNode.put(node.relativeFilepath, node);
        for (ResourceNode child : node.getChildren()) {
            buildPathToNodeMap(child);
        }
    }

    private void buildUrlToResourceMap(Set<ResourceEntry> entries) throws IOException {
        for (ResourceEntry entry : entries) {
            if (entry.hasHash()) {
                urlToResource.put(entry.getUrl(), entry);
            }
            else {
                throw new IOException(String.format("Unable to create ManifestData, an incomplete resource was found (missing hash)! %s", entry.getUrl() ));
            }
        }
    }

    public void setExcludedResources(List<String> excludedResources) {
        this.excludedResources.addAll(excludedResources);
    }

    public ManifestData buildManifestData() throws IOException {
        logger.info("buildManifestData begin");
        long tstart = System.currentTimeMillis();

        ManifestData.Builder builder = ManifestData.newBuilder();

        ManifestHeader manifestHeader = this.buildManifestHeader();
        builder.setHeader(manifestHeader);

        buildPathToNodeMap(getRoot());
        buildUrlToResourceMap(this.resourceEntries);

        builder.addAllEngineVersions(this.supportedEngineVersions);

        int resourceIndex = 0;
        HashMap<String, Integer> resourceToIndex = new HashMap<>(); // at what index is the resource stored?
        for (ResourceEntry entry : this.resourceEntries) {
            resourceToIndex.put(entry.getUrl(), resourceIndex);
            resourceIndex++;
        }

        for (ResourceEntry entry : this.resourceEntries) {
            String url = entry.getUrl();
            ResourceEntry.Builder resourceEntryBuilder = entry.toBuilder();

            // Since we'll only ever ask collection proxies, we only store those lists
            if (url.endsWith("collectionproxyc"))
            {
                // We'll only store the dependencies for the excluded collection proxies
                if (excludedResources.contains(url)) {
                    List<String> dependants = this.getDependants(url);

                    for (String dependant : dependants) {
                        ResourceEntry resource = urlToResource.get(dependant);
                        if (resource == null) {
                            continue;
                        }


                        int index = resourceToIndex.get(dependant);
                        resourceEntryBuilder.addDependants(index);
                    }
                }
            }

            builder.addResources(resourceEntryBuilder.build());
        }

        long tend = System.currentTimeMillis();
        logger.info("ManifestBuilder.buildManifestData took %f", (tend-tstart)/1000.0);

        return builder.build();
    }

    public ManifestFile buildManifestFile() throws IOException {
        ManifestFile.Builder builder = ManifestFile.newBuilder();

        ManifestData manifestData = this.buildManifestData();
        builder.setData(ByteString.copyFrom(manifestData.toByteArray()));
        builder.setArchiveIdentifier(ByteString.copyFrom(this.archiveIdentifier));
        PrivateKey privateKey = null;
        try {
            privateKey = CryptographicOperations.loadPrivateKey(this.privateKeyFilepath, this.signatureSignAlgorithm);
            byte[] signature = CryptographicOperations.sign(manifestData.toByteArray(), this.signatureHashAlgorithm, this.signatureSignAlgorithm, privateKey);
            builder.setSignature(ByteString.copyFrom(signature));
            if (this.outputManifestHash) {
                this.manifestDataHash = CryptographicOperations.hash(manifestData.toByteArray(), this.signatureHashAlgorithm);
            }
        } catch (NoSuchAlgorithmException exception) {
            throw new IOException("Unable to create ManifestFile, hashing algorithm is not supported!");
        } catch (InvalidKeySpecException | InvalidKeyException exception) {
            throw new IOException("Unable to create ManifestFile, private key has invalid format!");
        } catch (IllegalBlockSizeException | BadPaddingException | NoSuchPaddingException exception) {
            throw new IOException("Unable to create ManifestFile, cryptographic error!");
        } finally {
            if (privateKey != null && !privateKey.isDestroyed()) {
                try {
                    privateKey.destroy();
                } catch (DestroyFailedException exception) {
                    // java.security.PrivateKey does not implement destroy() for RSA.
                    // It is implemented using BigInteger which is immutable.
                    // This is a security risk, although a very small one.
                    // System.err.println("Warning! Failed to destroy the private key after creating signature, key may remain in memory!");
                }
            }
        }

        return builder.build();
    }

    public byte[] buildManifest() throws IOException {
        return this.buildManifestFile().toByteArray();
    }

}