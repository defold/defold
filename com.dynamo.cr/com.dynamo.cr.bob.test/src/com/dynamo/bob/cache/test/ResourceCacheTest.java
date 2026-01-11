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

package com.dynamo.bob.cache.test;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertTrue;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.cache.ResourceCache;


public class ResourceCacheTest {

	private Path cacheDir;

	private int resourceCount = 0;

	private ResourceCache resourceCache = null;

	@Before
	public void setUp() throws Exception {
		cacheDir = Files.createTempDirectory(null);
		resourceCache = new ResourceCache();
	}

	@After
	public void tearDown() throws IOException {
	}

	// nothing should happen if the resource cache is disabled
	@Test
	public void testGetAndPutWhenDisabled() throws CompileExceptionError, IOException {
		final String key = "somekey";

		assertTrue(resourceCache.get(key) == null);
		resourceCache.put(key, "somedata".getBytes());
		assertTrue(resourceCache.get(key) == null);
	}

	// it should be possible to get and put data in the cache when it is enabled
	@Test
	public void testGetAndPutWhenEnabled() throws CompileExceptionError, IOException {
		resourceCache.init(cacheDir.toString(), null);
		final String key = "somekey";
		final byte[] data = "somedata".getBytes();
		assertTrue(resourceCache.get(key) == null);
		resourceCache.put(key, data);
		assertTrue(resourceCache.contains(key));
		assertArrayEquals(data, resourceCache.get(key));
	}

}
