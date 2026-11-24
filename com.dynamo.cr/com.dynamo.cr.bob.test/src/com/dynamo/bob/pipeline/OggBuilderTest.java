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

package com.dynamo.bob.pipeline;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import java.util.Arrays;
import java.util.Set;

import org.junit.Before;
import org.junit.Test;

import com.dynamo.bob.Project;
import com.dynamo.bob.Task;
import com.dynamo.bob.util.OggUtil;

public class OggBuilderTest extends AbstractProtoBuilderTest {

    @Before
    public void setup() {
        addTestFiles();
    }

    @Test
    public void testOgg() throws Exception {
        String src = "components {" +
                "  id: \"sound\"\n" +
                "  component: \"/test.ogg\"\n" +
                "}\n";
        build("/test.go", src);
    }

    // Verifies metadata stripping in both streaming modes and invalidation of outputs from the old copy builder.
    @Test
    public void testOggMetadataStripped() throws Exception {
        byte[] source = getFile("/test.ogg");
        byte[] stripped = OggUtil.stripMetadata(source);
        assertTrue(stripped.length < source.length);
        for (boolean streaming : new boolean[]{false, true}) {
            getProject().setOption("sound-stream-enabled", Boolean.toString(streaming));
            String path = "/metadata_" + streaming + ".ogg";
            addFile(path, source);
            Task task = getProject().createTask(getProject().getResource(path));
            Task previousCopyTask = Task.newBuilder(task.getBuilder()).addInput(task.firstInput()).build();
            assertFalse(Arrays.equals(previousCopyTask.calculateSignature(), task.calculateSignature()));
            task.getBuilder().build(task);
            assertArrayEquals(stripped, task.output(0).getContent());
            assertArrayEquals(source, task.firstInput().getContent());
        }
    }

    // Preserves Opus content named .ogg in both streaming modes, including output flags and the source asset.
    @Test
    public void testOpusPreserved() throws Exception {
        byte[] source = getFile("/opus-audio.ogg");
        for (boolean streaming : new boolean[]{false, true}) {
            getProject().setOption("sound-stream-enabled", Boolean.toString(streaming));
            String path = "/opus_" + streaming + ".ogg";
            addFile(path, source);
            Task task = getProject().createTask(getProject().getResource(path));
            task.getBuilder().build(task);
            assertArrayEquals(source, task.output(0).getContent());
            assertArrayEquals(source, task.firstInput().getContent());
            Set<Task.OutputFlags> expected = streaming ? Set.of(Task.OutputFlags.UNCOMPRESSED) : Set.of();
            assertEquals(expected, task.getOutputFlags(task.output(0)));
        }
    }

    // Verifies that streaming flags are declared when creating sound tasks, before compilation can be skipped or cached.
    @Test
    public void testSoundOutputFlagsDeclaredDuringTaskCreation() throws Exception {
        Project project = getProject();
        for (boolean streaming : new boolean[]{false, true}) {
            project.setOption("sound-stream-enabled", Boolean.toString(streaming));
            for (String extension : new String[]{".ogg", ".wav"}) {
                String path = "/flags_" + streaming + extension;
                addFile(path, getFile("/test.ogg"));
                Task task = project.createTask(project.getResource(path));
                Set<Task.OutputFlags> expected = streaming ? Set.of(Task.OutputFlags.UNCOMPRESSED) : Set.of();
                assertEquals(expected, task.getOutputFlags(task.output(0)));
            }
        }
    }
}
