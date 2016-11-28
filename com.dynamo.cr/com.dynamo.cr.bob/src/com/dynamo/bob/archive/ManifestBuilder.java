package com.dynamo.bob.archive;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.security.InvalidKeyException;
import java.security.KeyFactory;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.spec.InvalidKeySpecException;
import java.security.spec.PKCS8EncodedKeySpec;
import java.security.spec.X509EncodedKeySpec;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Set;

import javax.crypto.BadPaddingException;
import javax.crypto.Cipher;
import javax.crypto.IllegalBlockSizeException;
import javax.crypto.NoSuchPaddingException;
import javax.security.auth.DestroyFailedException;

import com.dynamo.bob.pipeline.ResourceNode;
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

        public static PrivateKey createPrivateKey(String filepath, SignAlgorithm algorithm) throws NoSuchAlgorithmException, InvalidKeySpecException, IOException {
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

        public static PublicKey createPublicKey(String filepath, SignAlgorithm algorithm) throws IOException, NoSuchAlgorithmException, InvalidKeySpecException {
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
    private String projectIdentifier = null;
    private ResourceNode dependencies = null;
    private Set<HashDigest> supportedEngineVersions = new HashSet<HashDigest>();
    private Set<ResourceEntry> resourceEntries = new HashSet<ResourceEntry>();

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
    }

    public void setPrivateKeyFilepath(String filepath) {
        this.privateKeyFilepath = filepath;
    }

    public void setProjectIdentifier(String projectIdentifier) {
        this.projectIdentifier = projectIdentifier;
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
            HashDigest hash = CryptographicOperations.createHashDigest(data, this.resourceHashAlgorithm);
            builder.setHash(hash);
            this.resourceEntries.add(builder.buildPartial());
        } catch (NoSuchAlgorithmException exception) {
            throw new IOException("Unable to create Manifest, hashing algorithm is not supported!");
        }
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
        } else {
            throw new IOException("Unable to create ManifestData, entry is not part of dependencies: " + filepath);
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

        if (this.supportedEngineVersions.isEmpty()) {
            throw new IOException("Unable to create ManifestData, there are no supported engine versions!");
        }
        builder.addAllEngineVersions(this.supportedEngineVersions);

        if (this.resourceEntries.isEmpty()) {
            throw new IOException("Unable to create ManifestData, there are no resources!");
        }
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

        PrivateKey privateKey = null;
        try {
            privateKey = CryptographicOperations.createPrivateKey(this.privateKeyFilepath, this.signatureSignAlgorithm);
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