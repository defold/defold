package com.dynamo.bob.cache;

import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.List;
import java.util.ArrayList;
import java.util.Map;
import java.util.Set;
import java.util.Collections;
import java.io.IOException;
import java.math.BigInteger;

import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.archive.EngineVersion;

public class ResourceCacheKey {

	/**
	 * Calculate the key to use when caching a resource.
	 * The key is created from the task used which created the resource, the
	 * bytes of the resource itself, as well as engine sha and project options.
	 * @param task The task which created the resource
	 * @param projectOptions The project options
	 * @param resource The resource to calculate cache key for
	 * @return The cache key as a hex string
	 */
	public static String calculate(Task<?> task, Map<String, String> projectOptions, IResource resource) throws RuntimeException, IOException {
		MessageDigest digest = task.calculateSignatureDigest();

		digest.update(resource.getPath().getBytes());
		digest.update(EngineVersion.sha1.getBytes());

		// sort and add project options
		List<String> keys = new ArrayList<String>(projectOptions.keySet());
		Collections.sort(keys);
		for (String key : keys) {
			digest.update(key.getBytes());
			String value = projectOptions.get(key);
			if (value == null) {
				value = "";
			}
			digest.update(value.getBytes());
		}

		byte[] key = digest.digest();
		// byte digest to hex string
		return new BigInteger(1, key).toString(16);
	}
}
