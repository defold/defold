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

package com.dynamo.bob.pipeline;

import java.io.IOException;

import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Task;
import com.dynamo.bob.font.Fontc;
import com.dynamo.bob.fs.IResource;

import com.dynamo.bob.pipeline.GlyphBankBuilder;

import com.dynamo.render.proto.Font.FontDesc;
import com.dynamo.render.proto.Font.FontMap;

@BuilderParams(name = "Font", inExts = ".font", outExt = ".fontc")
public class FontBuilder extends Builder<Void>  {

    @Override
    public Task<Void> create(IResource input) throws IOException, CompileExceptionError {
        FontDesc.Builder fontDescbuilder = FontDesc.newBuilder();
        ProtoUtil.merge(input, fontDescbuilder);
        FontDesc fontDesc = fontDescbuilder.build();

        Task<?> embedTask = this.project.createTask(input, GlyphBankBuilder.class);

        Task.TaskBuilder<Void> taskBuilder = Task.<Void>newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addInput(input.getResource(fontDesc.getFont()))
                .addOutput(input.changeExt(params.outExt()));

        taskBuilder.addInput(embedTask.getOutputs().get(0));

        Task<Void> task = taskBuilder.build();
        embedTask.setProductOf(task);
        return task;
    }

    @Override
    public void build(Task<Void> task) throws CompileExceptionError, IOException {
        FontDesc.Builder fontDescbuilder = FontDesc.newBuilder();
        ProtoUtil.merge(task.input(0), fontDescbuilder);
        FontDesc fontDesc = fontDescbuilder.build();

        BuilderUtil.checkResource(this.project, task.input(0), "material", fontDesc.getMaterial());

        int buildDirLen        = this.project.getBuildDirectory().length();
        String genResourcePath = task.input(2).getPath().substring(buildDirLen);

        FontMap.Builder fontMapBuilder = FontMap.newBuilder();
        fontMapBuilder.setMaterial(BuilderUtil.replaceExt(fontDesc.getMaterial(), ".material", ".materialc"));

        fontMapBuilder.setGlyphBank(genResourcePath);
        fontMapBuilder.setShadowX(fontDesc.getShadowX());
        fontMapBuilder.setShadowY(fontDesc.getShadowY());
        fontMapBuilder.setAlpha(fontDesc.getAlpha());
        fontMapBuilder.setOutlineAlpha(fontDesc.getOutlineAlpha());
        fontMapBuilder.setShadowAlpha(fontDesc.getShadowAlpha());
        fontMapBuilder.setLayerMask(Fontc.GetFontMapLayerMask(fontDesc));

        task.output(0).setContent(fontMapBuilder.build().toByteArray());
    }
}
