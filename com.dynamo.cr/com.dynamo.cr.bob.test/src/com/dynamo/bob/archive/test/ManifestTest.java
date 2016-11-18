package com.dynamo.bob.archive.test;

import static org.junit.Assert.*;

import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import java.security.InvalidKeyException;
import java.security.KeyFactory;
import java.security.NoSuchAlgorithmException;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.spec.InvalidKeySpecException;
import java.security.spec.X509EncodedKeySpec;
import java.util.Base64;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import com.dynamo.bob.archive.ManifestBuilder;
import com.dynamo.liveupdate.proto.Manifest.HashAlgorithm;

public class ManifestTest {

    private ManifestBuilder manifestBuilder = null;

    private byte[] intArrayToByteArray(int[] array) {
    	byte[] result = new byte[array.length];
    	for (int i = 0; i < array.length; ++i) {
    		result[i] = (byte) array[i];
    	}
    	
    	return result;
    }
    
    private PublicKey loadPublicKey(String filepath) {
    	byte[] filecontent;
		try {
			filecontent = Files.readAllBytes(new File(filepath).toPath());
	    	X509EncodedKeySpec specification = new X509EncodedKeySpec(filecontent);
	    	KeyFactory keyFactory = KeyFactory.getInstance("RSA");
	    	return keyFactory.generatePublic(specification);   
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		} catch (NoSuchAlgorithmException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		} catch (InvalidKeySpecException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		
		return null;
    }


    @Before
    public void setUp() throws Exception {
        this.manifestBuilder = new ManifestBuilder();
    }

    @After
    public void tearDown() throws IOException {
        this.manifestBuilder = null;
    }

    @Test
    public void testCreateHash_NullData() throws NoSuchAlgorithmException {
    	byte[] actual = manifestBuilder.createHash(null, HashAlgorithm.HASH_MD5);
    	assertNull(actual);
    }
    
    @Test
    public void testCreateHash_MD5() throws NoSuchAlgorithmException {
    	String data = "defold";
    	byte[] actual = manifestBuilder.createHash(data.getBytes(), HashAlgorithm.HASH_MD5);
    	byte[] expected = intArrayToByteArray(
    			new int[] { 0x01, 0x75, 0x7d, 0xd6, 0x17, 0x3a, 0x9e, 0x1b, 0x01, 0x71, 0x4b, 0xb5, 0x84, 0xef, 0x00, 0xe5 });
    	
    	assertArrayEquals(expected, actual);
    }
    
    @Test
    public void testCreateHash_SHA1() throws NoSuchAlgorithmException {
    	String data = "defold";
    	byte[] actual = manifestBuilder.createHash(data.getBytes(), HashAlgorithm.HASH_SHA1);
    	byte[] expected = intArrayToByteArray(
    			new int[] { 0x2c, 0x43, 0xff, 0xbe, 0x82, 0x00, 0x90, 0x8a, 0x0c, 0xbc, 0x9b, 0xde, 0xa0, 0x39, 0x36, 0x66, 0xe4, 0xfb, 0x56, 0xad });
    	
    	assertArrayEquals(expected, actual);
    }
    
    @Test
    public void testCreateHash_SHA256() throws NoSuchAlgorithmException {
    	String data = "defold";
    	byte[] actual = manifestBuilder.createHash(data.getBytes(), HashAlgorithm.HASH_SHA256);
    	byte[] expected = intArrayToByteArray(
    			new int[] { 0xc8, 0xfa, 0x85, 0xe3, 0x1c, 0xb2, 0x12, 0x11, 0x97, 0xbb, 0xa7, 0xb1, 0x11, 0xf4, 0x31, 0x0e, 0x86, 0x22, 0xcb, 0x9d, 0x63, 0x79, 0x78, 0xfc, 0x60, 0x9d, 0xdc, 0x04, 0x66, 0x51, 0x2d, 0x8d });
    	
    	assertArrayEquals(expected, actual);
    }
    
    @Test
    public void testCreateHash_SHA512() throws NoSuchAlgorithmException {
    	String data = "defold";
    	byte[] actual = manifestBuilder.createHash(data.getBytes(), HashAlgorithm.HASH_SHA512);
    	byte[] expected = intArrayToByteArray(
    			new int[] { 0xeb, 0x4f, 0xfa, 0xa4, 0xe0, 0x14, 0x2f, 0xa9, 0xd1, 0x61, 0xc7, 0xdb, 0x2a, 0x87, 0xd6, 0xd9, 0x67, 0x2b, 0x65, 0x2a, 0xa6, 0x02, 0xaa, 0xc3, 0x49, 0x04, 0x96, 0x12, 0x0f, 0xac, 0xdb, 0xf8, 0x66, 0xea, 0xaf, 0x47, 0x3a, 0x7a, 0x96, 0xd2, 0x6b, 0xb0, 0xd9, 0xb8, 0x60, 0x91, 0xb8, 0x55, 0xd9, 0x57, 0xc5, 0x64, 0xbc, 0x94, 0xfc, 0x61, 0x93, 0x70, 0xe1, 0x49, 0x98, 0x50, 0x51, 0x11 });
    	
    	assertArrayEquals(expected, actual);
    }
    
    @Test
    public void testGetPrivateKey() {
    	String filepath = "test/private_rsa_1024_1.der";
    	manifestBuilder.setPrivateKeyFilepath(filepath);
    	PrivateKey privateKey = manifestBuilder.getPrivateKey();
    	
    	assertNotNull(privateKey);
    }
    
    @Test
    public void testGetPrivateKey_InvalidFilepath() {
    	String filepath = "test/doesntexist.pkcs8";
    	manifestBuilder.setPrivateKeyFilepath(filepath);
    	PrivateKey privateKey = manifestBuilder.getPrivateKey();
    	
    	assertNull(privateKey);
    }
    
    @Test
    public void testGetPrivateKey_InvalidKeyFormat() {
    	String filepath = "test/private_rsa_1024_3.pem";
    	manifestBuilder.setPrivateKeyFilepath(filepath);
    	PrivateKey privateKey = manifestBuilder.getPrivateKey();
    	
    	assertNull(privateKey);
    }
    
    @Test
    public void testCreateSignature() throws InvalidKeyException, NoSuchAlgorithmException {
    	String filepath = "test/private_rsa_1024_1.der";
    	String data = "defold";
    	
    	manifestBuilder.setPrivateKeyFilepath(filepath);
    	byte[] actual = manifestBuilder.createSignature(data.getBytes());
    	
    	assertNotNull(actual);
    }
    
    @Test
    public void testCreateSignature_Verify() throws InvalidKeyException, NoSuchAlgorithmException {
    	String filepathPrivateKey = "test/private_rsa_1024_1.der";
    	String filepathPublicKey = "test/public_rsa_1024_1.der";
    	String data = "defold";
    	
    	manifestBuilder.setPrivateKeyFilepath(filepathPrivateKey);
    	byte[] signature = manifestBuilder.createSignature(data.getBytes());
    	
    	PublicKey publicKey = this.loadPublicKey(filepathPublicKey);
    	boolean actual = manifestBuilder.verifySignature(data.getBytes(), signature, publicKey);
    	
    	assertTrue(actual);
    }
    
    @Test
    public void testCreateSignature_Verify_HashMismatch() throws InvalidKeyException, NoSuchAlgorithmException {
    	String filepathPrivateKey = "test/private_rsa_1024_1.der";
    	String filepathPublicKey = "test/public_rsa_1024_1.der";
    	String data1 = "defold";
    	String data2 = "defolds";
    	
    	manifestBuilder.setPrivateKeyFilepath(filepathPrivateKey);
    	byte[] signature = manifestBuilder.createSignature(data1.getBytes());
    	
    	PublicKey publicKey = this.loadPublicKey(filepathPublicKey);
    	boolean actual = manifestBuilder.verifySignature(data2.getBytes(), signature, publicKey);
    	
    	assertFalse(actual);
    }
    
    @Test
    public void testCreateSignature_Verify_KeyMismatch() throws InvalidKeyException, NoSuchAlgorithmException {
    	String filepathPrivateKey = "test/private_rsa_1024_1.der";
    	String filepathPublicKey = "test/public_rsa_1024_2.der";
    	String data = "defold";
    	
    	manifestBuilder.setPrivateKeyFilepath(filepathPrivateKey);
    	byte[] signature = manifestBuilder.createSignature(data.getBytes());
    	
    	PublicKey publicKey = this.loadPublicKey(filepathPublicKey);
    	boolean actual = manifestBuilder.verifySignature(data.getBytes(), signature, publicKey);
    	
    	assertFalse(actual);
    }
    
}
