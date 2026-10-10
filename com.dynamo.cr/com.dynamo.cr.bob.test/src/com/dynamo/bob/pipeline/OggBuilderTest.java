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

import java.util.Set;

import org.junit.Before;
import org.junit.Test;

import com.dynamo.bob.Project;
import com.dynamo.bob.Task;

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
