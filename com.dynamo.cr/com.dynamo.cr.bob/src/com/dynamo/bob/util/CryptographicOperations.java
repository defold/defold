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

package com.dynamo.bob.util;

import java.io.FileOutputStream;
import java.io.IOException;

import java.util.Arrays;

import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;

import java.security.spec.InvalidKeySpecException;
import java.security.spec.PKCS8EncodedKeySpec;
import java.security.spec.X509EncodedKeySpec;
import javax.security.auth.DestroyFailedException;

import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;
import java.security.MessageDigest;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.KeyFactory;
import java.security.KeyPair;
import java.security.KeyPairGenerator;

import javax.crypto.Cipher;
import javax.crypto.BadPaddingException;
import javax.crypto.IllegalBlockSizeException;
import javax.crypto.NoSuchPaddingException;

import com.dynamo.liveupdate.proto.Manifest.HashDigest;
import com.dynamo.liveupdate.proto.Manifest.HashAlgorithm;
import com.dynamo.liveupdate.proto.Manifest.SignAlgorithm;

import com.google.protobuf.ByteString;


public class CryptographicOperations {

    private CryptographicOperations() {}

    public static MessageDigest getMessageDigest(HashAlgorithm algorithm) throws NoSuchAlgorithmException {
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
        return messageDigest;
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
        byte[] hash = hash(data, hashAlgorithm);
        byte[] ciphertext = encrypt(hash, signAlgorithm, privateKey);
        return ciphertext;
    }

    public static HashDigest createHashDigest(byte[] data, HashAlgorithm algorithm) throws NoSuchAlgorithmException {
        byte[] hashDigest = hash(data, algorithm);
        HashDigest.Builder builder = HashDigest.newBuilder();
        builder.setData(ByteString.copyFrom(hashDigest));
        return builder.build();
    }

    public static HashDigest toHashDigest(byte[] hashDigest) {
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
