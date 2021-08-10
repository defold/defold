package com.dynamo.bob.cache;

import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.List;
import java.util.Map;
import java.io.IOException;
import java.math.BigInteger;

import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.archive.EngineVersion;

public class ResourceCacheKey {

	public static String calculate(Task<?> task, Map<String, String> options, IResource outputResource) throws RuntimeException, IOException {
		MessageDigest digest = task.calculateSignatureDigest();

		digest.update(outputResource.getPath().getBytes());
		digest.update(EngineVersion.sha1.getBytes());
		digest.update(options.toString().getBytes());

		byte[] key = digest.digest();
		// byte digest to hex string
		return new BigInteger(1, key).toString(16);
	}
}
