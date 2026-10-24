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

package com.dynamo.bob.archive.liveupdate;

import com.dynamo.bob.Builder;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;
import java.io.IOException;

/** Passthrough for manifest-selected custom resources without a resource compiler. */
public final class CopyPackageResource extends Builder {
    @Override
    public Task create(IResource input) {
        return Task.newBuilder(this).setName("Copy package resource").addInput(input).addOutput(input.output()).build();
    }

    @Override
    public void build(Task task) throws IOException {
        task.output(0).setContent(task.input(0).getContent());
    }
}
