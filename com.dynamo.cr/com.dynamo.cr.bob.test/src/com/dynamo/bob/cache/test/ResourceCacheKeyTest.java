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

package com.dynamo.bob.cache.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import java.io.IOException;
import java.util.Map;
import java.util.HashMap;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.Builder;
import com.dynamo.bob.Task;
import com.dynamo.bob.Task.TaskBuilder;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.cache.ResourceCacheKey;
import com.dynamo.bob.test.util.MockFileSystem;
import com.dynamo.bob.test.util.MockResource;

public class ResourceCacheKeyTest {

	private class DummyBuilder extends Builder<Void> {
		private TaskBuilder<Void> builder;

		public DummyBuilder() {
			builder = Task.<Void> newBuilder(this);
		}
		public DummyBuilder addInput(IResource input) {
			builder.addInput(input);
			return this;
		}
		public DummyBuilder addOutput(IResource output) {
			builder.addOutput(output);
			return this;
		}
		@Override
		public Task<Void> create(IResource input) throws IOException, CompileExceptionError {
			if (input != null) {
				builder.addInput(input);
			}
			return builder.build();
		}

		@Override
		public void build(Task<Void> task) throws CompileExceptionError, IOException {

		}
	}

	private MockFileSystem fs;

	private int resourceCount = 0;

	private MockResource createResource(String content) throws IOException {
		resourceCount = resourceCount + 1;
		String name = "tmpresource" + resourceCount;
		return fs.addFile(name, content.getBytes());
	}

	private Map<String, String> createImportantOptions() {
		Map<String, String> options = new HashMap<String, String>();
		options.put("important_option", "yep");
		return options;
	}
	private Map<String, String> createEmptyOptions() {
		return new HashMap<String, String>();
	}

	@Before
	public void setUp() throws Exception {
		fs = new MockFileSystem();
		fs.setBuildDirectory("build");
		ResourceCacheKey.includeOption("important_option");
	}

	@After
	public void tearDown() throws IOException {
	}

	// can we create a cache key at all?
	@Test
	public void testBasicKeyCreation() throws CompileExceptionError, IOException {
		IResource input = createResource("someInput");
		IResource output = createResource("someOutput").output();

		DummyBuilder builder = new DummyBuilder();
		Task<?> task = builder.addInput(input).addOutput(output).create(null);
		String key = ResourceCacheKey.calculate(task, createEmptyOptions(), output);
		assertTrue(key != null);
	}

	// do we always get the same key with the same input?
	@Test
	public void testDeterministicKeyCreation() throws CompileExceptionError, IOException {
		IResource input = createResource("someInput");
		IResource output = createResource("someOutput").output();

		DummyBuilder builder1 = new DummyBuilder();
		Task<?> task1 = builder1.addInput(input).addOutput(output).create(null);
		String key1 = ResourceCacheKey.calculate(task1, createEmptyOptions(), output);

		DummyBuilder builder2 = new DummyBuilder();
		Task<?> task2 = builder2.addInput(input).addOutput(output).create(null);
		String key2 = ResourceCacheKey.calculate(task2, createEmptyOptions(), output);

		assertEquals(key1, key2);
	}

	// is project options taken into account and produce different keys?
	@Test
	public void testProjectOptions() throws CompileExceptionError, IOException {
		IResource input = createResource("someInput");
		IResource output = createResource("someOutput").output();

		DummyBuilder builder = new DummyBuilder();
		Task<?> task = builder.addInput(input).addOutput(output).create(null);
		String key1 = ResourceCacheKey.calculate(task, createImportantOptions(), output);
		String key2 = ResourceCacheKey.calculate(task, createEmptyOptions(), output);

		assertNotEquals(key1, key2);
	}

	// are input resources taken into account and produce different keys?
	@Test
	public void testInputs() throws CompileExceptionError, IOException {
		IResource input = createResource("someInput");
		IResource output = createResource("someOutput").output();

		DummyBuilder builder1 = new DummyBuilder();
		Task<?> task1 = builder1.addInput(input).addOutput(output).create(null);
		String key1 = ResourceCacheKey.calculate(task1, createEmptyOptions(), output);

		DummyBuilder builder2 = new DummyBuilder();
		input.setContent("someOtherInput".getBytes());
		Task<?> task2 = builder2.addInput(input).addOutput(output).create(null);
		String key2 = ResourceCacheKey.calculate(task2, createEmptyOptions(), output);

		assertNotEquals(key1, key2);
	}

	// are output resource names taken into account and produce different keys?
	@Test
	public void testOutputNames() throws CompileExceptionError, IOException {
		IResource input = createResource("someInput");

		DummyBuilder builder1 = new DummyBuilder();
		IResource output1 = createResource("someOutput").output();
		Task<?> task1 = builder1.addInput(input).addOutput(output1).create(null);
		String key1 = ResourceCacheKey.calculate(task1, createEmptyOptions(), output1);

		DummyBuilder builder2 = new DummyBuilder();
		IResource output2 = createResource("someOutput").output();
		Task<?> task2 = builder2.addInput(input).addOutput(output2).create(null);
		String key2 = ResourceCacheKey.calculate(task2, createEmptyOptions(), output2);

		assertNotEquals(key1, key2);
	}

	// some project options have no impact on key creation
	@Test
	public void testIgnoredOptions() throws CompileExceptionError, IOException {
		IResource input = createResource("someInput");
		IResource output = createResource("someOutput").output();

		Map<String, String> ignoredOptions1 = new HashMap<String, String>();
		ignoredOptions1.put("email", "foo@bar.com");
		ignoredOptions1.put("debug", "true");
		DummyBuilder builder1 = new DummyBuilder();
		Task<?> task1 = builder1.addInput(input).addOutput(output).create(null);
		String key1 = ResourceCacheKey.calculate(task1, ignoredOptions1, output);

		Map<String, String> ignoredOptions2 = new HashMap<String, String>();
		ignoredOptions2.put("email", "bob@acme.com");
		ignoredOptions2.put("debug", "false");
		DummyBuilder builder2 = new DummyBuilder();
		Task<?> task2 = builder2.addInput(input).addOutput(output).create(null);
		String key2 = ResourceCacheKey.calculate(task2, ignoredOptions2, output);

		assertEquals(key1, key2);
	}
}
