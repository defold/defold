package com.dynamo.bob.archive;

import java.io.File;
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
import java.util.Arrays;
import java.util.Set;
import java.util.HashSet;

import javax.crypto.BadPaddingException;
import javax.crypto.Cipher;
import javax.crypto.IllegalBlockSizeException;
import javax.crypto.NoSuchPaddingException;
import javax.security.auth.DestroyFailedException;

import com.dynamo.liveupdate.proto.Manifest;
import com.dynamo.liveupdate.proto.Manifest.HashAlgorithm;
import com.dynamo.liveupdate.proto.Manifest.HashDigest;
import com.dynamo.liveupdate.proto.Manifest.ManifestData;
import com.dynamo.liveupdate.proto.Manifest.ManifestFile;
import com.dynamo.liveupdate.proto.Manifest.ManifestHeader;
import com.dynamo.liveupdate.proto.Manifest.ResourceEntry;
import com.dynamo.liveupdate.proto.Manifest.SignAlgorithm;
import com.google.protobuf.ByteString;

public class ManifestBuilder {
	private static final int CONST_MAGIC_NUMBER = 0x01;
	private static final int CONST_VERSION = 0x01;

	private HashAlgorithm resourceHashAlgorithm = HashAlgorithm.HASH_UNKNOWN;
	private HashAlgorithm signatureHashAlgorithm = HashAlgorithm.HASH_UNKNOWN;
	private SignAlgorithm signatureSignAlgorithm = SignAlgorithm.SIGN_UNKNOWN;

	private String privateKeyFilepath = null;

	private ManifestHeader manifestHeader = null;
	private Set<HashDigest> supportedEngineVersions = new HashSet<HashDigest>();
	private Set<ResourceEntry> resourceEntries = new HashSet<ResourceEntry>();

	public ManifestBuilder() {
		this.setResourceHashAlgorithm(HashAlgorithm.HASH_SHA1);
		this.setSignatureHashAlgorithm(HashAlgorithm.HASH_SHA1);
		this.setSignatureSignAlgorithm(SignAlgorithm.SIGN_RSA);
	}

	public byte[] createHash(byte[] data, HashAlgorithm algorithm) throws NoSuchAlgorithmException {
		if (data == null) {
			return null;
		}

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

	public HashDigest createHashDigest(byte[] data, HashAlgorithm algorithm) throws NoSuchAlgorithmException {
		byte[] hash = this.createHash(data, algorithm);

		HashDigest.Builder builder = HashDigest.newBuilder();
		builder.addData(ByteString.copyFrom(hash));
		return builder.build();
	}

	public byte[] createSignature(byte[] data) throws InvalidKeyException, NoSuchAlgorithmException {
		byte[] result = null;
		if (this.signatureSignAlgorithm.equals(SignAlgorithm.SIGN_RSA)) {
			PrivateKey privateKey = null;
			try {
				byte[] hash = this.createHash(data, this.signatureHashAlgorithm);

				final Cipher cipher = Cipher.getInstance("RSA");
				privateKey = this.getPrivateKey();
				cipher.init(Cipher.ENCRYPT_MODE, privateKey);
				result = cipher.doFinal(hash);
			} catch (NoSuchPaddingException exception) {
				throw new NoSuchAlgorithmException(exception);
			} catch (BadPaddingException exception) {
				throw new NoSuchAlgorithmException(exception);
			} catch (IllegalBlockSizeException exception) {
				// TODO: Log
			} finally {
				if (privateKey != null) {
					try {
						privateKey.destroy();
					} catch (DestroyFailedException exception) {
						// TODO: Log "Warning! Failed to destroy the private key after creating signature, key may remain in memory!
					}
				}
			}
		} else {
			throw new NoSuchAlgorithmException("The algorithm specified is not supported!");
		}

		return result;
	}

	public boolean verifySignature(byte[] data, byte[] signature, PublicKey publicKey) throws NoSuchAlgorithmException, InvalidKeyException {
		if (this.signatureSignAlgorithm.equals(SignAlgorithm.SIGN_RSA)) {
			try {
				final Cipher cipher = Cipher.getInstance("RSA");
				cipher.init(Cipher.DECRYPT_MODE, publicKey);
				byte[] decryptedSignature = cipher.doFinal(signature);
				byte[] hash = this.createHash(data, this.signatureHashAlgorithm);

				boolean result = decryptedSignature.length == hash.length;
				for (int i = 0; i < Math.min(decryptedSignature.length, hash.length); ++i) {
					result = decryptedSignature[i] == hash[i] && result;
				}

				return result;
			} catch (NoSuchPaddingException exception) {
				// TODO: Log
			} catch (BadPaddingException exception) {
				// TODO: Log
			} catch (IllegalBlockSizeException exception) {
				// TODO: Log
			}
		}

		return false;
	}

	public HashAlgorithm setResourceHashAlgorithm(HashAlgorithm algorithm) {
		if (this.resourceHashAlgorithm.equals(HashAlgorithm.HASH_UNKNOWN)) {
			this.resourceHashAlgorithm = algorithm;
		}

		return this.resourceHashAlgorithm;
	}

	public HashAlgorithm setSignatureHashAlgorithm(HashAlgorithm algorithm) {
		if (this.signatureHashAlgorithm.equals(HashAlgorithm.HASH_UNKNOWN)) {
			this.signatureHashAlgorithm = algorithm;
		}

		return this.signatureHashAlgorithm;
	}

	public SignAlgorithm setSignatureSignAlgorithm(SignAlgorithm algorithm) {
		if (this.signatureSignAlgorithm.equals(SignAlgorithm.SIGN_UNKNOWN)) {
			this.signatureSignAlgorithm = algorithm;
		}

		return this.signatureSignAlgorithm;
	}

	public void setPrivateKeyFilepath(String filepath) {
		this.privateKeyFilepath = filepath;
	}

	public PrivateKey getPrivateKey() {
		PrivateKey privateKey = null;
		if (this.privateKeyFilepath != null) {
			Path fhandle = Paths.get(this.privateKeyFilepath);
			try {
				// TODO: We should verify file permissions to ensure limited access.
				byte[] filecontent = Files.readAllBytes(fhandle);
				PKCS8EncodedKeySpec specification = new PKCS8EncodedKeySpec(filecontent);
				KeyFactory keyFactory = null;
				try {
					if (this.signatureSignAlgorithm.equals(SignAlgorithm.SIGN_RSA)) {
						keyFactory = KeyFactory.getInstance("RSA");
					} else {
						// TODO: Log "The algorithm specified is not supported!"
						System.out.println("The algorithm specified is not supported: " + this.signatureSignAlgorithm.toString());
					}

					if (keyFactory != null) {
						privateKey = keyFactory.generatePrivate(specification);
						Arrays.fill(filecontent, (byte) 0x0); // Zero out memory for private key
						return privateKey;
					} else {
                        Arrays.fill(filecontent, (byte) 0x0); // Zero out memory for private key
                    }
				} catch (NoSuchAlgorithmException exception) {
					// TODO: Log "The algorithm specified is not supported!"
					System.out.println("The algorithm specified is not supported: " + this.signatureSignAlgorithm.toString());
				} catch (InvalidKeySpecException exception) {
					// TODO: Log "Unable to read private key from file '%s'"
					System.out.println(exception);
					System.out.println("Unable to read private key from file: " + this.privateKeyFilepath);
				}
			} catch (IOException exception) {
				// TODO: Log "Unable to access file '%s'"
				System.out.println("Unable to access file: " + this.privateKeyFilepath);
			}
		}

		return privateKey;
	}

	public byte[] generateManifestFile() {
		byte[] result = null;
		ManifestData.Builder manifestDataBuilder = ManifestData.newBuilder();
		manifestDataBuilder.setHeader(this.manifestHeader);
		manifestDataBuilder.addAllEngineVersions(this.supportedEngineVersions);
		for (ResourceEntry entry : this.resourceEntries) {
			ResourceEntry.Builder resourceEntryBuilder = entry.toBuilder();
			// resourceBuilder.addDependants(value);
			manifestDataBuilder.addResources(resourceEntryBuilder.build());
		}

		ManifestData manifestData = manifestDataBuilder.build();

		try {
			ManifestFile.Builder manifestFileBuilder = ManifestFile.newBuilder();
			manifestFileBuilder.setData(manifestData);
			byte[] signature = this.createSignature(manifestFileBuilder.getData().toByteArray());
			manifestFileBuilder.addSignature(ByteString.copyFrom(signature));
			ManifestFile manifestFile = manifestFileBuilder.build();
			result = manifestFile.toByteArray();
		} catch (NoSuchAlgorithmException exception) {
			// TODO: Log
		} catch (InvalidKeyException exception) {
			// TODO: Log
		}

		return result;
	}

	public void constructManifestHeader(String projectIdentifier) {
		try {
			ManifestHeader.Builder builder = ManifestHeader.newBuilder();
			builder.setMagicNumber(ManifestBuilder.CONST_MAGIC_NUMBER);
			builder.setVersion(ManifestBuilder.CONST_VERSION);
			builder.setProjectIdentifier(this.createHashDigest(projectIdentifier.getBytes(), HashAlgorithm.HASH_SHA1));
			builder.setResourceHashAlgorithm(this.resourceHashAlgorithm);
			builder.setSignatureHashAlgorithm(this.signatureHashAlgorithm);
			builder.setSignatureSignAlgorithm(this.signatureSignAlgorithm);
			this.manifestHeader = builder.build();
		} catch (NoSuchAlgorithmException exception) {
			this.manifestHeader = null;
		}
	}

	public void addSupportedEngineVersion(String hash) {
		HashDigest.Builder builder = HashDigest.newBuilder();
		builder.addData(ByteString.copyFrom(hash.getBytes()));
		this.supportedEngineVersions.add(builder.build());
	}

	public boolean addResourceEntry(String url, byte[] data) {
		try {
			ResourceEntry.Builder builder = ResourceEntry.newBuilder();
			builder.addUrl(url);
			builder.setHash(this.createHashDigest(data, this.resourceHashAlgorithm));
			this.resourceEntries.add(builder.buildPartial());
		} catch (NoSuchAlgorithmException exception) {
			return false;
		}

		return true;
	}

}