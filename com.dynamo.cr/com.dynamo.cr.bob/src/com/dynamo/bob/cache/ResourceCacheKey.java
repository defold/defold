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

package com.dynamo.bob.cache;

import java.security.MessageDigest;
import java.util.List;
import java.util.ArrayList;
import java.util.Map;
import java.util.Set;
import java.util.HashSet;
import java.util.Collections;
import java.io.IOException;
import java.math.BigInteger;

import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.archive.EngineVersion;

public class ResourceCacheKey {

	/*
	 * A set of options which have an impact on the created resources
	 * and must be included when calculating the resource key
	 */
	private static Set<String> options = new HashSet<String>();

	/**
	 * Add an option that should be included in the resource key
	 * calculation
	 */
	public static void includeOption(String option) {
		options.add(option);
	}


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
			if (options.contains(key)) {
				digest.update(key.getBytes());
				String value = projectOptions.get(key);
				if (value == null) {
					value = "";
				}
				digest.update(value.getBytes());
			}
		}

		byte[] key = digest.digest();
		// byte digest to hex string
		return new BigInteger(1, key).toString(16);
	}
}
