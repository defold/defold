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

package com.dynamo.bob.pipeline;

import java.io.IOException;

import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.ProtoBuilder;
import com.dynamo.bob.ProtoParams;
import com.dynamo.bob.Task;
import com.dynamo.bob.font.Fontc;
import com.dynamo.bob.fs.IResource;

import com.dynamo.render.proto.Font.FontDesc;
import com.dynamo.render.proto.Font.FontMap;

@ProtoParams(srcClass = FontDesc.class, messageClass = FontDesc.class)
@BuilderParams(name = "Font", inExts = ".font", outExt = ".fontc")
public class FontBuilder extends ProtoBuilder<FontDesc.Builder> {

    @Override
    public Task create(IResource input) throws IOException, CompileExceptionError {
        FontDesc.Builder builder = getSrcBuilder(input);
        FontDesc fontDesc = builder.build();

        Task.TaskBuilder taskBuilder = Task.newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addInput(input.getResource(fontDesc.getFont()))
                .addOutput(input.changeExt(params.outExt()));

        Task glyphBankTask = createSubTask(input, GlyphBankBuilder.class, taskBuilder);
        createSubTask(fontDesc.getMaterial(),"material", taskBuilder);

        Task task = taskBuilder.build();
        glyphBankTask.setProductOf(task);
        return task;
    }

    @Override
    public void build(Task task) throws CompileExceptionError, IOException {
        FontDesc.Builder builder = getSrcBuilder(task.firstInput());
        FontDesc fontDesc = builder.build();

        BuilderUtil.checkResource(this.project, task.firstInput(), "material", fontDesc.getMaterial());

        int buildDirLen        = this.project.getBuildDirectory().length();
        String genResourcePath = task.input(2).getPath().substring(buildDirLen);

        FontMap.Builder fontMapBuilder = FontMap.newBuilder();
        fontMapBuilder.setMaterial(BuilderUtil.replaceExt(fontDesc.getMaterial(), ".material", ".materialc"));

        fontMapBuilder.setGlyphBank(genResourcePath);

        fontMapBuilder.setSize(fontDesc.getSize());
        fontMapBuilder.setAntialias(fontDesc.getAntialias());
        fontMapBuilder.setShadowX(fontDesc.getShadowX());
        fontMapBuilder.setShadowY(fontDesc.getShadowY());
        fontMapBuilder.setShadowBlur(fontDesc.getShadowBlur());
        fontMapBuilder.setShadowAlpha(fontDesc.getShadowAlpha());
        fontMapBuilder.setAlpha(fontDesc.getAlpha());
        fontMapBuilder.setOutlineAlpha(fontDesc.getOutlineAlpha());
        fontMapBuilder.setOutlineWidth(fontDesc.getOutlineWidth());
        fontMapBuilder.setLayerMask(Fontc.GetFontMapLayerMask(fontDesc));

        fontMapBuilder.setOutputFormat(fontDesc.getOutputFormat());
        fontMapBuilder.setRenderMode(fontDesc.getRenderMode());

        task.output(0).setContent(fontMapBuilder.build().toByteArray());
    }
}
