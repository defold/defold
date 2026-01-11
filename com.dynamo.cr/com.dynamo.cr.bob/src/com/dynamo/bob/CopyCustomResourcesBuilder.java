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

package com.dynamo.bob;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import com.dynamo.bob.Task.TaskBuilder;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.util.BobProjectProperties;

public class CopyCustomResourcesBuilder extends Builder {
    @Override
    public Task create(IResource input) throws CompileExceptionError {
        BobProjectProperties properties = this.project.getProjectProperties();

        String[] resources = properties.getStringArrayValue("project", "custom_resources", new String[0]);

        TaskBuilder b = Task.newBuilder(this)
                .setName("Copy Custom Resources");

        for (String s : resources) {
            s = s.trim();
            if (s.length() > 0) {
                // Could be directory or file; use findResourcePaths to traverse & grab all.
                ArrayList<String> paths = new ArrayList<String>();
                this.project.findResourcePaths(s, paths);
                for (String path : paths) {
                    IResource r = this.project.getResource(path);
                    b.addInput(r);
                    b.addOutput(r.output());
                }
            }
        }
        return b.build();
    }

    @Override
    public void build(Task task) throws IOException {
        final List<IResource> outputs = task.getOutputs();
        final List<IResource> inputs = task.getInputs();
        final int n = inputs.size();
        for (int i = 0; i < n; i++) {
            outputs.get(i).setContent(inputs.get(i).getContent());
        }
    }
}
