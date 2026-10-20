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

package com.dynamo.bob.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.fail;

import java.util.List;
import java.util.Set;

import org.junit.Before;
import org.junit.Test;

import com.dynamo.bob.Task;
import com.dynamo.bob.Task.OutputFlags;
import com.dynamo.bob.fs.DefaultFileSystem;
import com.dynamo.bob.fs.IResource;

public class TaskTest {
    private DefaultFileSystem fileSystem;

    @Before
    public void setUp() {
        fileSystem = new DefaultFileSystem();
        fileSystem.setBuildDirectory("build");
    }

    // Verifies that known unflagged outputs remain distinct from unknown resources and indices.
    @Test
    public void testUnflaggedAndUnknownOutputs() {
        Task task = Task.newBuilder(null)
                .addOutput(fileSystem.get("build/plain"))
                .build();

        assertEquals(Set.of(), task.getOutputFlags(0));
        assertEquals(Set.of(), task.getOutputFlags(fileSystem.get("build/plain")));
        assertNull(task.getOutputFlags(fileSystem.get("build/unknown")));
        assertNull(task.getOutputFlags(null));
        assertNull(task.getOutputFlags(-1));
        assertNull(task.getOutputFlags(1));
    }

    // Verifies all flag combinations, including explicit NONE, stay independent across bit-set word boundaries.
    @Test
    public void testOutputFlagsAcrossManyOutputs() {
        List<Set<OutputFlags>> combinations = List.of(
                Set.of(),
                Set.of(OutputFlags.NONE),
                Set.of(OutputFlags.UNCOMPRESSED),
                Set.of(OutputFlags.ENCRYPTED),
                Set.of(OutputFlags.NONE, OutputFlags.UNCOMPRESSED),
                Set.of(OutputFlags.NONE, OutputFlags.ENCRYPTED),
                Set.of(OutputFlags.UNCOMPRESSED, OutputFlags.ENCRYPTED),
                Set.of(OutputFlags.NONE, OutputFlags.UNCOMPRESSED, OutputFlags.ENCRYPTED));
        Task.TaskBuilder<Void> builder = new Task.TaskBuilder<>(null);
        for (int i = 0; i < 64; ++i) {
            Set<OutputFlags> flags = combinations.get(i % combinations.size());
            builder.addOutput(fileSystem.get("build/output_" + i), flags.toArray(new OutputFlags[0]));
        }
        Task task = builder.build();

        for (int i = 0; i < 64; ++i) {
            Set<OutputFlags> expected = combinations.get(i % combinations.size());
            assertEquals("Output " + i, expected, task.getOutputFlags(i));
            assertEquals("Resource " + i, expected, task.getOutputFlags(fileSystem.get("build/output_" + i)));
        }
    }

    // Verifies resource equality and the last declaration still determine flags for duplicate output paths.
    @Test
    public void testRepeatedOutputUsesLatestFlags() {
        IResource first = fileSystem.get("build/repeated");
        IResource second = fileSystem.get("build/repeated");
        assertNotSame(first, second);
        Task.TaskBuilder<Void> builder = new Task.TaskBuilder<>(null);
        builder.addOutput(first, OutputFlags.ENCRYPTED)
                .addOutput(fileSystem.get("build/other"), OutputFlags.ENCRYPTED)
                .addOutput(second, OutputFlags.UNCOMPRESSED);
        Task task = builder.build();

        assertEquals(Set.of(OutputFlags.UNCOMPRESSED), task.getOutputFlags(first));
        assertEquals(Set.of(OutputFlags.UNCOMPRESSED), task.getOutputFlags(second));

        builder.addOutput(fileSystem.get("build/repeated"));
        assertEquals(Set.of(), task.getOutputFlags(first));
        assertEquals(Set.of(OutputFlags.ENCRYPTED), task.getOutputFlags(fileSystem.get("build/other")));
    }

    // Verifies sharing a resource across tasks cannot leak one task's flags into another task.
    @Test
    public void testOutputFlagsBelongToEachTask() {
        IResource output = fileSystem.get("build/shared");
        Task encrypted = Task.newBuilder(null).addOutput(output, OutputFlags.ENCRYPTED).build();
        Task plain = Task.newBuilder(null).addOutput(output).build();
        Task uncompressed = Task.newBuilder(null).addOutput(output, OutputFlags.UNCOMPRESSED).build();

        assertEquals(Set.of(OutputFlags.ENCRYPTED), encrypted.getOutputFlags(output));
        assertEquals(Set.of(), plain.getOutputFlags(output));
        assertEquals(Set.of(OutputFlags.UNCOMPRESSED), uncompressed.getOutputFlags(output));
    }

    // Verifies callers cannot mutate the shared flag sets, including the empty set.
    @Test
    public void testOutputFlagsAreImmutable() {
        Task task = Task.newBuilder(null)
                .addOutput(fileSystem.get("build/plain"))
                .addOutput(fileSystem.get("build/encrypted"), OutputFlags.ENCRYPTED)
                .build();

        for (int i = 0; i < 2; ++i) {
            try {
                task.getOutputFlags(i).add(OutputFlags.NONE);
                fail("Output flags must be immutable");
            } catch (UnsupportedOperationException expected) {
            }
        }
        assertEquals(Set.of(), task.getOutputFlags(0));
        assertEquals(Set.of(OutputFlags.ENCRYPTED), task.getOutputFlags(1));
    }
}
