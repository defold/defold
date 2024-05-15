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

package com.dynamo.bob;

import java.io.IOException;

import com.dynamo.bob.fs.IResource;

/**
 * Copy builder. This class is abstract. Inherit from this class
 * and add appropriate {@link BuilderParams}
 * @author Christian Murray
 *
 */
public abstract class CopyBuilder extends Builder<Void> {

    @Override
    public Task<Void> create(IResource input) throws IOException, CompileExceptionError {
        Task<Void> task = Task.<Void>newBuilder(this)
                .setName(params.name())
                .disableCache()
                .addInput(input)
                .addOutput(input.changeExt(params.outExt()))
                .build();
        return task;
    }

    @Override
    public void build(Task<Void> task) throws IOException {
        IResource in = task.input(0);
        IResource out = task.output(0);
        out.setContent(in.getContent());
    }
}
