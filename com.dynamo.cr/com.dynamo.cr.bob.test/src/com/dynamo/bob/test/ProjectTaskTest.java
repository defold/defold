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

import org.junit.Assert;
import org.junit.Test;

import com.dynamo.bob.Task;
import com.dynamo.bob.pipeline.AbstractProtoBuilderTest;

public class ProjectTaskTest extends AbstractProtoBuilderTest {
    // Indexes each producer once and drops completed tasks before a later build creates new instances.
    @Test
    public void testOutputIndexFollowsTaskLifecycle() throws Exception {
        String source = "material: \"\"";
        addFile("/test.gui", source);
        var input = getProject().getResource("/test.gui");
        Task first = getProject().createTask(input);
        Assert.assertSame(first, getProject().getTaskForOutput(first.output(0)));
        Assert.assertSame(first, getProject().createTask(input));
        Assert.assertNull(getProject().getTaskForOutput(input));

        build("/test.gui", source);
        Assert.assertNull(getProject().getTaskForOutput(first.output(0)));
        Task next = getProject().createTask(input);
        Assert.assertNotSame(first, next);
        Assert.assertSame(next, getProject().getTaskForOutput(next.output(0)));
    }

    // Includes combined gamepad tasks, which are registered outside the ordinary createTask path.
    @Test
    public void testCombinedGamepadTaskIsIndexed() throws Exception {
        addFile("/test.gamepads", "");
        addFile("/gamecontrollerdb.txt", "");
        var maps = getProject().getResource("/test.gamepads");
        var database = getProject().getResource("/gamecontrollerdb.txt");
        Task task = getProject().createGamepadTask(database, maps);
        Assert.assertSame(task, getProject().getTaskForOutput(task.output(0)));
        Assert.assertSame(task, getProject().createGamepadTask(database, maps));
    }
}
