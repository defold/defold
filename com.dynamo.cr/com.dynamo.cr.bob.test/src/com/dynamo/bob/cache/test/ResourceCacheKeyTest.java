// Copyright 2020-2025 The Defold Foundation
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

import static java.util.Map.entry;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import java.io.IOException;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

import com.dynamo.bob.Bob;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.Project;
import com.dynamo.bob.fs.DefaultFileSystem;
import org.junit.After;
import org.junit.Assert;
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

	@BuilderParams(name = "DummyBuilder", outExt = "", inExts = {}, paramsForSignature = {"important_option"})
	private class DummyBuilder extends Builder {
		private TaskBuilder builder;

		public DummyBuilder() {
			builder = Task. newBuilder(this);
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
		public Task create(IResource input) throws IOException, CompileExceptionError {
			if (input != null) {
				builder.addInput(input);
			}
			return builder.build();
		}

		@Override
		public void build(Task task) throws CompileExceptionError, IOException {

		}
	}

	private MockFileSystem fs;

	private int resourceCount = 0;

	private MockResource createResource(String content) throws IOException {
		resourceCount = resourceCount + 1;
		String name = "tmpresource" + resourceCount;
		return fs.addFile(name, content.getBytes());
	}

	@Before
	public void setUp() throws Exception {
		fs = new MockFileSystem();
		fs.setBuildDirectory("build");
	}

	@After
	public void tearDown() throws IOException {
	}

	// can we create a cache key at all?
	@Test
	public void testBasicKeyCreation() throws CompileExceptionError, IOException, NoSuchAlgorithmException {
		IResource input = createResource("someInput");
		IResource output = createResource("someOutput").output();

		DummyBuilder builder = new DummyBuilder();
		Task task = builder.addInput(input).addOutput(output).create(null);
		String key = ResourceCacheKey.calculate(task.calculateSignature(), output);
		assertTrue(key != null);
	}

	// do we always get the same key with the same input?
	@Test
	public void testDeterministicKeyCreation() throws CompileExceptionError, IOException, NoSuchAlgorithmException {
		IResource input = createResource("someInput");
		IResource output = createResource("someOutput").output();

		DummyBuilder builder1 = new DummyBuilder();
		Task task1 = builder1.addInput(input).addOutput(output).create(null);
		String key1 = ResourceCacheKey.calculate(task1.calculateSignature(), output);

		DummyBuilder builder2 = new DummyBuilder();
		Task task2 = builder2.addInput(input).addOutput(output).create(null);
		String key2 = ResourceCacheKey.calculate(task2.calculateSignature(), output);

		assertEquals(key1, key2);
	}

	// is project options taken into account and produce different keys?
	@Test
	public void testProjectOptions() throws CompileExceptionError, IOException, NoSuchAlgorithmException {
		IResource input = createResource("someInput");
		IResource output = createResource("someOutput").output();

		DummyBuilder builder = new DummyBuilder();
		Task task = builder.addInput(input).addOutput(output).create(null);
		String key1 = ResourceCacheKey.calculate(task.calculateSignature(), output);
		DummyBuilder.addParamsDigest(DummyBuilder.class, Map.ofEntries(
				entry("important_option", "yep")
			), DummyBuilder.class.getAnnotation(BuilderParams.class));
		String key2 = ResourceCacheKey.calculate(task.calculateSignature(), output);

		assertNotEquals(key1, key2);
	}

	// are input resources taken into account and produce different keys?
	@Test
	public void testInputs() throws CompileExceptionError, IOException, NoSuchAlgorithmException {
		IResource input = createResource("someInput");
		IResource output = createResource("someOutput").output();

		DummyBuilder builder1 = new DummyBuilder();
		Task task1 = builder1.addInput(input).addOutput(output).create(null);
		String key1 = ResourceCacheKey.calculate(task1.calculateSignature(), output);

		DummyBuilder builder2 = new DummyBuilder();
		input.setContent("someOtherInput".getBytes());
		Task task2 = builder2.addInput(input).addOutput(output).create(null);
		String key2 = ResourceCacheKey.calculate(task2.calculateSignature(), output);

		assertNotEquals(key1, key2);
	}

	// are output resource names taken into account and produce different keys?
	@Test
	public void testOutputNames() throws CompileExceptionError, IOException, NoSuchAlgorithmException {
		IResource input = createResource("someInput");

		DummyBuilder builder1 = new DummyBuilder();
		IResource output1 = createResource("someOutput").output();
		Task task1 = builder1.addInput(input).addOutput(output1).create(null);
		String key1 = ResourceCacheKey.calculate(task1.calculateSignature(), output1);

		DummyBuilder builder2 = new DummyBuilder();
		IResource output2 = createResource("someOutput").output();
		Task task2 = builder2.addInput(input).addOutput(output2).create(null);
		String key2 = ResourceCacheKey.calculate(task2.calculateSignature(), output2);

		assertNotEquals(key1, key2);
	}

	// some project options have no impact on key creation
	@Test
	public void testIgnoredOptions() throws CompileExceptionError, IOException, NoSuchAlgorithmException {
		IResource input = createResource("someInput");
		IResource output = createResource("someOutput").output();
		DummyBuilder.addParamsDigest(DummyBuilder.class, Map.ofEntries(
				entry("not_important_option", "yep")
		), DummyBuilder.class.getAnnotation(BuilderParams.class));
		DummyBuilder builder1 = new DummyBuilder();
		Task task1 = builder1.addInput(input).addOutput(output).create(null);
		String key1 = ResourceCacheKey.calculate(task1.calculateSignature(), output);

		DummyBuilder builder2 = new DummyBuilder();
		Task task2 = builder2.addInput(input).addOutput(output).create(null);
		String key2 = ResourceCacheKey.calculate(task2.calculateSignature(), output);

		assertEquals(key1, key2);
	}

	// Ensure that all parameters specified in all builders exist in Bob
	@Test
	public void testAllParametersExist() throws IOException, CompileExceptionError, NoSuchFieldException, IllegalAccessException, NoSuchMethodException, InvocationTargetException {
		// Initialize project
		Project project = new Project(new DefaultFileSystem());
		project.scanJavaClasses();
		project.configurePreBuildProjectOptions();

		// Access private static field classToParamsDigest
		Class<?> builderClass = Builder.class;
		Field field = builderClass.getDeclaredField("classToParamsDigest");
		field.setAccessible(true);
		Map<Class<?>, byte[]> map = (Map<Class<?>, byte[]>) field.get(null);

		// Get all available command line options
		List<Bob.CommandLineOption> commandLineOptions = Bob.getCommandLineOptions();
		Set<String> allOptions = new HashSet<>();

		// Collect all long options
		for (Bob.CommandLineOption option : commandLineOptions) {
			allOptions.add(option.longOpt);
		}

		for (String key :project.getOptions().keySet()) {
			allOptions.add(key);
		}

		// Validate each parameter in classToParamsDigest
		for (Class<?> klass : map.keySet()) {
			String[] params = klass.getAnnotation(BuilderParams.class).paramsForSignature();

			for (String param : params) {
				if (!allOptions.contains(param)) {
					if (param.equals("important_option"))
					{
						assertEquals(BuilderParams.class.toString(), "interface com.dynamo.bob.BuilderParams");
					}
					else {
						Assert.fail("Class " + klass.getName() + " uses parameter '" + param + "' in classToParamsDigest, but it does not exist in the command line options.");
					}
				}
			}
		}
	}

	// Fails if a new Bob parameter is added.
	// Ensure that new Bob parameters do not affect build signatures.
	// If a parameter does affect builders, add it to @BuilderParams -> paramsForSignature,
	// then update the list in this test.
	@Test
	public void testNoNewParametersAdded() throws IOException, CompileExceptionError, NoSuchFieldException, IllegalAccessException {
		// Initialize project
		Project project = new Project(new DefaultFileSystem());
		project.scanJavaClasses();

		// Access private static field classToParamsDigest
		Class<?> builderClass = Builder.class;
		Field field = builderClass.getDeclaredField("classToParamsDigest");
		field.setAccessible(true);
		Map<Class<?>, byte[]> map = (Map<Class<?>, byte[]>) field.get(null);

		// Get all available command line options
		List<Bob.CommandLineOption> commandLineOptions = Bob.getCommandLineOptions();
		List<String> allOptions = new ArrayList<>();

		// Collect all long options
		for (Bob.CommandLineOption option : commandLineOptions) {
			allOptions.add(option.longOpt);
		}
		Collections.sort(allOptions);

		List<String> existentParameters = new ArrayList<>(List.of(new String[]{
                "architectures",
                "archive",
                "archive-resource-padding",
                "auth",
                "binary-output",
                "build-artifacts",
                "build-report",
                "build-report-html",
                "build-report-json",
                "build-server",
                "build-server-header",
                "bundle-format",
                "bundle-output",
                "certificate",
                "debug",
                "debug-ne-upload",
                "debug-output-glsl",
                "debug-output-hlsl",
                "debug-output-spirv",
                "debug-output-wgsl",
                "debug-output-msl",
                "defoldsdk",
                "email",
                "exclude-archive",
                "exclude-build-folder",
                "help",
                "identity",
                "input",
                "key-pass",
                "keystore",
                "keystore-alias",
                "keystore-pass",
                "liveupdate",
                "manifest-private-key",
                "manifest-public-key",
                "max-cpu-threads",
                "mobileprovisioning",
                "ne-build-dir",
                "ne-output-name",
                "output",
                "platform",
                "private-key",
                "resource-cache-local",
                "resource-cache-remote",
                "resource-cache-remote-pass",
                "resource-cache-remote-user",
                "root",
                "settings",
                "strip-executable",
                "texture-compression",
                "texture-profiles",
                "use-async-build-server",
                "use-lua-bytecode-delta",
                "use-uncompressed-lua-source",
                "use-vanilla-lua",
                "variant",
                "verbose",
                "version",
                "with-sha1",
                "with-symbols"}));

		Collections.sort(existentParameters);

		assertEquals("Lists are not equal!", existentParameters, allOptions);
	}
}
