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

import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Task;
import com.dynamo.bob.font.Fontc;
import com.dynamo.bob.fs.IResource;

import com.dynamo.render.proto.Font.FontDesc;
import com.dynamo.render.proto.Font.FontMap;
import com.dynamo.render.proto.Font.FontTextureFormat;

@BuilderParams(name = "Font", inExts = ".font", outExt = ".fontc")
public class FontBuilder extends Builder  {

    @Override
    public Task create(IResource input) throws IOException, CompileExceptionError {
        FontDesc.Builder fontDescbuilder = FontDesc.newBuilder();
        ProtoUtil.merge(input, fontDescbuilder);
        FontDesc fontDesc = fontDescbuilder.build();

        IResource ttfResource = input.getResource(fontDesc.getFont());
        Task.TaskBuilder taskBuilder = Task.newBuilder(this)
                .setName(params.name())
                .addOutput(input.changeExt(params.outExt()));

        // input(0)
        taskBuilder.addInput(input);

        // input(1)
        createSubTask(fontDesc.getMaterial(),"material", taskBuilder);

        Task subTask = null;
        if (fontDesc.getDynamic())
        {
            // input(2)
            subTask = createSubTask(ttfResource, CopyBuilders.TTFBuilder.class, taskBuilder);
        }
        else
        {
            // input(2)
            taskBuilder.addInput(ttfResource);
            // input(3)
            subTask = createSubTask(input, GlyphBankBuilder.class, taskBuilder);
        }

        Task task = taskBuilder.build();
        subTask.setProductOf(task);
        return task;
    }

    @Override
    public void build(Task task) throws CompileExceptionError, IOException {
        FontDesc.Builder fontDescbuilder = FontDesc.newBuilder();
        ProtoUtil.merge(task.firstInput(), fontDescbuilder);
        FontDesc fontDesc = fontDescbuilder.build();
        boolean dynamic = fontDesc.getDynamic();
        FontMap.Builder fontMapBuilder = FontMap.newBuilder();


        BuilderUtil.checkResource(this.project, task.firstInput(), "material", fontDesc.getMaterial());
        if (dynamic)
        {
            BuilderUtil.checkResource(this.project, task.firstInput(), "font", fontDesc.getFont());
            fontMapBuilder.setDynamic(dynamic);
            fontMapBuilder.setFont(fontDesc.getFont()); // Keep the suffix as-is (i.e. ".ttf")
        }
        else
        {
            int buildDirLen        = this.project.getBuildDirectory().length();
            String glyphBankPath   = task.input(3).getPath().substring(buildDirLen);
            fontMapBuilder.setGlyphBank(glyphBankPath);
        }

        fontMapBuilder.setMaterial(BuilderUtil.replaceExt(fontDesc.getMaterial(), ".material", ".materialc"));

        boolean all_chars = fontDesc.getAllChars();

        if (all_chars)
        {
            fontMapBuilder.setAllChars(all_chars); // 0x000000 - 0x10FFFF
        }
        else
        {
            fontMapBuilder.setCharacters(fontDesc.getCharacters());
        }

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
        fontMapBuilder.setCacheWidth(fontDesc.getCacheWidth());
        fontMapBuilder.setCacheHeight(fontDesc.getCacheHeight());

        // TODO: SDK vars
        if (fontDesc.getOutputFormat() == FontTextureFormat.TYPE_DISTANCE_FIELD)
        {
            fontMapBuilder.setSdfSpread(Fontc.GetFontMapSdfSpread(fontDesc));
            fontMapBuilder.setSdfOutline(Fontc.GetFontMapSdfOutline(fontDesc));
            fontMapBuilder.setSdfShadow(Fontc.GetFontMapSdfShadow(fontDesc));
        }

        fontMapBuilder.setPadding(Fontc.GetFontMapPadding(fontDesc));

        fontMapBuilder.setOutputFormat(fontDesc.getOutputFormat());
        fontMapBuilder.setRenderMode(fontDesc.getRenderMode());

        task.output(0).setContent(fontMapBuilder.build().toByteArray());
    }
}
