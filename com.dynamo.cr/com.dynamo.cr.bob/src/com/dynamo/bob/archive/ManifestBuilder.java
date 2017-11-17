package com.dynamo.bob.archive;

import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.security.InvalidKeyException;
import java.security.KeyFactory;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.spec.InvalidKeySpecException;
import java.security.spec.PKCS8EncodedKeySpec;
import java.security.spec.X509EncodedKeySpec;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Set;
import java.util.TreeSet;

import javax.crypto.BadPaddingException;
import javax.crypto.Cipher;
import javax.crypto.IllegalBlockSizeException;
import javax.crypto.NoSuchPaddingException;
import javax.security.auth.DestroyFailedException;

import com.dynamo.bob.pipeline.ResourceNode;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.liveupdate.proto.Manifest.HashAlgorithm;
import com.dynamo.liveupdate.proto.Manifest.HashDigest;
import com.dynamo.liveupdate.proto.Manifest.ManifestData;
import com.dynamo.liveupdate.proto.Manifest.ManifestFile;
import com.dynamo.liveupdate.proto.Manifest.ManifestHeader;
import com.dynamo.liveupdate.proto.Manifest.ResourceEntry;
import com.dynamo.liveupdate.proto.Manifest.SignAlgorithm;
import com.google.protobuf.ByteString;

public class ManifestBuilder {

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
            for ( int j = 0; j < bytes.length; j++ ) {
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

        public static byte[] encrypt(byte[] plaintext, SignAlgorithm algorithm, PrivateKey privateKey) throws NoSuchAlgorithmException, NoSuchPaddingException, InvalidKeyException, IllegalBlockSizeException, BadPaddingException {
            byte[] result = null;
            if (algorithm.equals(SignAlgorithm.SIGN_RSA)) {
                try {
                    final Cipher cipher = Cipher.getInstance("RSA");
                    cipher.init(Cipher.ENCRYPT_MODE, privateKey);
                    result = cipher.doFinal(plaintext);
                } finally {
                    if (privateKey != null) {
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
            } else {
                throw new NoSuchAlgorithmException("The algorithm specified is not supported!");
            }

            return result;
        }

        public static byte[] decrypt(byte[] ciphertext, SignAlgorithm algorithm, PublicKey publicKey) throws NoSuchAlgorithmException, NoSuchPaddingException, InvalidKeyException, IllegalBlockSizeException, BadPaddingException {
            byte[] result = null;
            if (algorithm.equals(SignAlgorithm.SIGN_RSA)) {
                final Cipher cipher = Cipher.getInstance("RSA");
                cipher.init(Cipher.DECRYPT_MODE, publicKey);
                result = cipher.doFinal(ciphertext);
            } else {
                throw new NoSuchAlgorithmException("The algorithm specified is not supported!");
            }

            return result;
        }

        public static byte[] sign(byte[] data, HashAlgorithm hashAlgorithm, SignAlgorithm signAlgorithm, PrivateKey privateKey) throws NoSuchAlgorithmException, InvalidKeyException, IllegalBlockSizeException, BadPaddingException, NoSuchPaddingException {
            byte[] hash = CryptographicOperations.hash(data, hashAlgorithm);
            byte[] ciphertext = CryptographicOperations.encrypt(hash, signAlgorithm, privateKey);
            return ciphertext;
        }

        public static HashDigest createHashDigest(byte[] data, HashAlgorithm algorithm) throws NoSuchAlgorithmException {
            byte[] hashDigest = CryptographicOperations.hash(data, algorithm);
            HashDigest.Builder builder = HashDigest.newBuilder();
            builder.setData(ByteString.copyFrom(hashDigest));
            return builder.build();
        }

        public static void generateKeyPair(SignAlgorithm algorithm, String privateKeyFilepath, String publicKeyFilepath) throws NoSuchAlgorithmException, IOException {
            byte[] privateKeyContent = null;
            byte[] publicKeyContent = null;
            if (algorithm.equals(SignAlgorithm.SIGN_RSA)) {
                KeyPairGenerator generator = KeyPairGenerator.getInstance("RSA");
                generator.initialize(1024);
                KeyPair keyPair = generator.generateKeyPair();

                // Private key
                PKCS8EncodedKeySpec privateSpecification = new PKCS8EncodedKeySpec(keyPair.getPrivate().getEncoded());
                privateKeyContent = privateSpecification.getEncoded();

                // Public key
                X509EncodedKeySpec publicSpecification = new X509EncodedKeySpec(keyPair.getPublic().getEncoded());
                publicKeyContent = publicSpecification.getEncoded();
            } else {
                throw new NoSuchAlgorithmException("The algorithm specified is not supported!");
            }

            // Private key
            FileOutputStream privateOutputStream = null;
            try {
                privateOutputStream = new FileOutputStream(privateKeyFilepath);
                privateOutputStream.write(privateKeyContent);
            } catch (IOException exception) {
                throw new IOException("Unable to create asymmetric keypair, cannot write to file: " + privateKeyFilepath);
            } finally {
                if (privateOutputStream != null) {
                    try {
                        privateOutputStream.close();
                    } catch (Exception exception) {
                        // Nothing to do at this point
                    }
                }
            }

            // Public key
            FileOutputStream publicOutputStream = null;
            try {
                publicOutputStream = new FileOutputStream(publicKeyFilepath);
                publicOutputStream.write(publicKeyContent);
            } catch (IOException exception) {
                throw new IOException("Unable to create asymmetric keypair, cannot write to file: " + publicKeyFilepath);
            } finally {
                if (publicOutputStream != null) {
                    try {
                        publicOutputStream.close();
                    } catch (Exception exception) {
                        // Nothing to do at this point
                    }
                }
            }
        }

        public static PrivateKey loadPrivateKey(String filepath, SignAlgorithm algorithm) throws NoSuchAlgorithmException, InvalidKeySpecException, IOException {
            PrivateKey result = null;
            byte[] data = null;
            try {
                Path pathHandle = Paths.get(filepath);
                data = Files.readAllBytes(pathHandle);
                PKCS8EncodedKeySpec specification = new PKCS8EncodedKeySpec(data);
                KeyFactory keyFactory = null;
                if (algorithm.equals(SignAlgorithm.SIGN_RSA)) {
                    keyFactory = KeyFactory.getInstance("RSA");
                } else {
                    throw new NoSuchAlgorithmException("The algorithm specified is not supported!");
                }

                result = keyFactory.generatePrivate(specification);
            } finally {
                if (data != null) {
                    // Always make sure that we wipe key content from memory!
                    // When using a byte array we don't have to rely on GC to zeroize memory.
                    Arrays.fill(data, (byte) 0x0);
                }
            }

            // The private key should be destroyed as soon as it has been used.
            return result;
        }

        public static PublicKey loadPublicKey(String filepath, SignAlgorithm algorithm) throws IOException, NoSuchAlgorithmException, InvalidKeySpecException {
            PublicKey result = null;
            byte[] data = null;
            try {
                Path pathHandle = Paths.get(filepath);
                data = Files.readAllBytes(pathHandle);
                X509EncodedKeySpec specification = new X509EncodedKeySpec(data);
                KeyFactory keyFactory = null;
                if (algorithm.equals(SignAlgorithm.SIGN_RSA)) {
                    keyFactory = KeyFactory.getInstance("RSA");
                } else {
                    throw new NoSuchAlgorithmException("The algorithm specified is not supported!");
                }

                result = keyFactory.generatePublic(specification);
            } finally {
                if (data != null) {
                    // Always make sure that we wipe key content from memory!
                    // When using a byte array we don't have to rely on GC to zeroize memory.
                    Arrays.fill(data, (byte) 0x0);
                }
            }

            // The public key should be destroyed shortly after it has been used.
            return result;
        }

    }

    public static final int CONST_MAGIC_NUMBER = 0x43cb6d06;
    public static final int CONST_VERSION = 0x01;

    private HashAlgorithm resourceHashAlgorithm = HashAlgorithm.HASH_UNKNOWN;
    private HashAlgorithm signatureHashAlgorithm = HashAlgorithm.HASH_UNKNOWN;
    private SignAlgorithm signatureSignAlgorithm = SignAlgorithm.SIGN_UNKNOWN;
    private String privateKeyFilepath = null;
    private String publicKeyFilepath = null;
    private String projectIdentifier = null;
    private ResourceNode dependencies = null;
    private byte[] archiveIdentifier = new byte[ArchiveBuilder.MD5_HASH_DIGEST_BYTE_LENGTH];
    private Set<HashDigest> supportedEngineVersions = new HashSet<HashDigest>();
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
            // byte compare, because java does not have unsigned types
            byte[] s1_bytes = ByteBuffer.allocate(Long.SIZE / Byte.SIZE).putLong(s1.getUrlHash()).array();
            byte[] s2_bytes = ByteBuffer.allocate(Long.SIZE / Byte.SIZE).putLong(s2.getUrlHash()).array();
            return compare(s1_bytes, s2_bytes);
        }
    });

    public ManifestBuilder() {

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

    public void setDependencies(ResourceNode dependencies) {
        this.dependencies = dependencies;

        // DEBUG STUFF
        List<ResourceNode> queue = new LinkedList<ResourceNode>();
        queue.add(this.dependencies);
        // Find occurences of resource in tree (may be referenced from several collections for example)
        while (!queue.isEmpty()) {
            ResourceNode current = queue.remove(0);
            if (current != null) {
                // ResourceNode parent = current.getParent();
                // System.out.println("setDependencies queue, current.relativeFilepath: " + current.relativeFilepath);// + ", parent.relativeFilepath: " + (parent != null ? parent.relativeFilepath : "") );
                // String apa = "";
                // while (parent != null) {
                //     apa += "--";
                //     System.out.println(apa + ">" + parent.relativeFilepath);
                //     parent = parent.getParent();
                // }
                for (ResourceNode child : current.getChildren()) {
                    queue.add(child);
                }
            }
        }
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

    public void addSupportedEngineVersion(String hash) {
        HashDigest.Builder builder = HashDigest.newBuilder();
        builder.setData(ByteString.copyFrom(hash.getBytes()));
        this.supportedEngineVersions.add(builder.build());
    }

    public void addResourceEntry(String url, byte[] data) throws IOException {
        try {
            ResourceEntry.Builder builder = ResourceEntry.newBuilder();
            builder.setUrl(url);
            builder.setUrlHash(MurmurHash.hash64(url)); // sort on this
            HashDigest hash = CryptographicOperations.createHashDigest(data, this.resourceHashAlgorithm);
            builder.setHash(hash);
            this.resourceEntries.add(builder.buildPartial());
        } catch (NoSuchAlgorithmException exception) {
            throw new IOException("Unable to create Manifest, hashing algorithm is not supported!");
        }
    }

    // Calculate all parent collectionproxies for a resource
    // Resource could occur multiple times in the tree (referenced from several collections) or several times within the same collection
    public List<ArrayList<String>> getParentFilepath(String filepath) {
        // System.out.println("#############################");
        List<ArrayList<String>> result = new ArrayList<ArrayList<String>>();
        List<ResourceNode> candidates = new LinkedList<ResourceNode>();
        List<ResourceNode> queue = new LinkedList<ResourceNode>();
        queue.add(this.dependencies);
        // Find occurences of resource in tree (may be referenced from several collections for example)
        while (!queue.isEmpty()) {
            ResourceNode current = queue.remove(0);
            if (current != null) {
                // if (filepath.contains("shared_go_generated_0")) {
                //     ResourceNode parent = current.getParent();
                //     System.out.println("queue, current.relativeFilepath: " + current.relativeFilepath);// + ", parent.relativeFilepath: " + (parent != null ? parent.relativeFilepath : "") );
                //     String apa = "";
                //     while (parent != null) {
                //         apa += "--";
                //         System.out.println(apa + ">" + parent.relativeFilepath);
                //         parent = parent.getParent();
                //     }
                // }
                if (current.relativeFilepath.equals(filepath)) {
                    if (!candidates.contains(current)) {
                        candidates.add(current);
                    }
                    else {
                        System.out.println("Got CANDIDATE duplicate! filepath: " + filepath + ", current.relativeFilepath: " + current.relativeFilepath);
                    }
                } else {
                    for (ResourceNode child : current.getChildren()) {
                        queue.add(child);
                    }
                }
            }
        }

        // if (filepath.contains("shared_go_generated_0")) {
        //     System.out.println("num parent candidates for shared_go_generated_0.labelc: " + candidates.size());

        //     for (ResourceNode cand : candidates) {
        //         System.out.println("----> " + cand.relativeFilepath);
        //     }
        // }

        int i = 0;
        while (!candidates.isEmpty()) {
            ResourceNode current = candidates.remove(0).getParent();
            result.add(new ArrayList<String>());
            while (current != null) {
                // if (filepath.contains("shared_go_generated_0")) {
                //     System.out.println("current.relativeFilepath: " + current.relativeFilepath);
                // }
                if (current.relativeFilepath.endsWith("collectionproxyc")) {
                    // check for duplicates
                    boolean duplicate = false;
                    for(ArrayList<String> res : result) {
                        if (res.contains(current.relativeFilepath)) {
                            //System.out.println("Got duplicate! filepath: " + filepath + ", current.relativeFilepath: " + current.relativeFilepath);
                            duplicate = true;
                        }
                    }
                        
                    if (!duplicate) {
                        result.get(i).add(current.relativeFilepath);
                    }
                }

                current = current.getParent();
            }
            ++i;
        }

        return result;
    }

    public List<String> getDependants(String filepath) throws IOException {
        /* This function first tries to find the correct resource in the
           dependency tree. Since there is no index we have to iterate over
           the structure and match the relative filepaths that identifies the
           resource.

           Once a candidate has been found the children, the children, and so
           on are added to the list of dependants. If a CollectionProxy is
           found that resource itself is added to the list of dependants, but
           it is seen as a leaf and the Collection that it points to is ignored.

           The reason children of a CollectionProxy is ignored is that they are
           not required to load the parent Collection. This allows us to
           exclude an entire Collection that is loaded through a CollectionProxy
           and thus create a partial archive that has to be updated (through
           LiveUpdate) before that CollectionProxy can be loaded.
        */
        ResourceNode candidate = null;
        List<ResourceNode> queue = new LinkedList<ResourceNode>();
        queue.add(this.dependencies);
        while (candidate == null && !queue.isEmpty()) {
            ResourceNode current = queue.remove(0);
            if (current.relativeFilepath.equals(filepath)) {
                candidate = current;
            } else {
                for (ResourceNode child : current.getChildren()) {
                    queue.add(child);
                }
            }
        }

        List<String> dependants = new ArrayList<String>();
        if (candidate != null) {
            queue.clear();
            queue.add(candidate);
            while (!queue.isEmpty()) {
                ResourceNode current = queue.remove(0);
                for (ResourceNode child : current.getChildren()) {
                    dependants.add(child.relativeFilepath);
                    if (!child.relativeFilepath.endsWith("collectionproxyc")) {
                        queue.add(child);
                    }
                }
            }
        }

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

    public ManifestData buildManifestData() throws IOException {
        ManifestData.Builder builder = ManifestData.newBuilder();

        ManifestHeader manifestHeader = this.buildManifestHeader();
        builder.setHeader(manifestHeader);

        builder.addAllEngineVersions(this.supportedEngineVersions);
        for (ResourceEntry entry : this.resourceEntries) {
            ResourceEntry.Builder resourceEntryBuilder = entry.toBuilder();

            List<String> dependants = this.getDependants(entry.getUrl());
            for (String dependant : dependants) {
                for (ResourceEntry dependantEntry : this.resourceEntries) {
                    if (dependantEntry.getUrl().equals(dependant)) {
                        if (dependantEntry.hasHash()) {
                            resourceEntryBuilder.addDependants(dependantEntry.getHash());
                        } else {
                            throw new IOException("Unable to create ManifestData, an incomplete resource was found!");
                        }
                    }
                }
            }

            builder.addResources(resourceEntryBuilder.build());
        }

        return builder.build();
    }

    public ManifestFile buildManifestFile() throws IOException {
        ManifestFile.Builder builder = ManifestFile.newBuilder();

        ManifestData manifestData = this.buildManifestData();
        builder.setData(manifestData);
        builder.setArchiveIdentifier(ByteString.copyFrom(this.archiveIdentifier));
        PrivateKey privateKey = null;
        try {
            privateKey = CryptographicOperations.loadPrivateKey(this.privateKeyFilepath, this.signatureSignAlgorithm);
            byte[] signature = CryptographicOperations.sign(manifestData.toByteArray(), this.signatureHashAlgorithm, this.signatureSignAlgorithm, privateKey);
            builder.setSignature(ByteString.copyFrom(signature));
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