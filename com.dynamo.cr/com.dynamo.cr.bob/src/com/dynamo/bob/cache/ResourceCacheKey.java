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

package com.dynamo.bob.cache;

import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.io.IOException;
import java.math.BigInteger;

import com.dynamo.bob.archive.EngineVersion;
import com.dynamo.bob.fs.IResource;

public class ResourceCacheKey {

	/**
	 * Calculate the key to use when caching a resource.
	 * The key is created from the task used which created the resource and the
	 * bytes of the resource path.
	 * @param taskSignature The task which created the resource
	 * @param resource The resource to calculate cache key for
	 * @return The cache key as a hex string
	 */
	public static String calculate(byte[] taskSignature, IResource resource) throws RuntimeException, IOException, NoSuchAlgorithmException {
		MessageDigest digest = MessageDigest.getInstance("SHA1");
        digest.update(taskSignature);
		digest.update(EngineVersion.sha1.getBytes());
		digest.update(resource.getPath().getBytes());
		byte[] key = digest.digest();
		// byte digest to hex string
		return new BigInteger(1, key).toString(16);
	}
}
