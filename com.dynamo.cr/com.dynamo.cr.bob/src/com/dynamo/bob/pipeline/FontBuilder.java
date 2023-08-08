// Copyright 2020-2023 The Defold Foundation
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

import java.awt.FontFormatException;
import java.io.BufferedInputStream;
import java.io.ByteArrayInputStream;
import java.io.InputStream;
import java.io.IOException;

import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Task;
import com.dynamo.bob.font.Fontc;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.util.MurmurHash;

import com.dynamo.bob.pipeline.GlyphBankBuilder;

import com.dynamo.render.proto.Font.FontDesc;
import com.dynamo.render.proto.Font.FontMap;
import com.dynamo.render.proto.Font.FontRenderMode;

@BuilderParams(name = "Font", inExts = ".font", outExt = ".fontc")
public class FontBuilder extends Builder<Void>  {

    // TODO: merge with GlyphBankBuilder.java
    private long fontDescToHash(FontDesc fontDesc) {
        FontDesc.Builder fontDescbuilder = FontDesc.newBuilder();

        fontDescbuilder.mergeFrom(fontDesc)
                       .setMaterial("")
                       .clearOutlineAlpha()
                       .clearShadowAlpha()
                       .clearShadowX()
                       .clearShadowY()
                       .clearAlpha();

        byte[] fontDescBytes = fontDescbuilder.build().toByteArray();
        return MurmurHash.hash64(fontDescBytes, fontDescBytes.length);
    }

    @Override
    public Task<Void> create(IResource input) throws IOException, CompileExceptionError {
        FontDesc.Builder fontDescbuilder = FontDesc.newBuilder();
        ProtoUtil.merge(input, fontDescbuilder);
        FontDesc fontDesc = fontDescbuilder.build();

        this.project.createTask(input, GlyphBankBuilder.class);

        Task.TaskBuilder<Void> task = Task.<Void>newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addInput(input.getResource(fontDesc.getFont()))
                .addOutput(input.changeExt(params.outExt()));

        return task.build();
    }

    // These values are the same as font_renderer.cpp
    static final int LAYER_FACE    = 0x1;
    static final int LAYER_OUTLINE = 0x2;
    static final int LAYER_SHADOW  = 0x4;

    // The fontMapLayerMask contains a bitmask of the layers that should be rendered
    // by the font renderer. Note that this functionality requires that the render mode
    // property of the font resource must be set to MULTI_LAYER. Default behaviour
    // is to render the font as single layer (for compatability reasons).
    private int getFontMapLayerMask(FontDesc fontDesc)
    {
        int fontMapLayerMask = LAYER_FACE;

        if (fontDesc.getRenderMode() == FontRenderMode.MODE_MULTI_LAYER)
        {
            if (fontDesc.getOutlineAlpha() > 0 &&
                fontDesc.getOutlineWidth() > 0)
            {
                fontMapLayerMask |= LAYER_OUTLINE;
            }

            if (fontDesc.getShadowAlpha() > 0 &&
                fontDesc.getAlpha() > 0)
            {
                fontMapLayerMask |= LAYER_SHADOW;
            }
        }

        return fontMapLayerMask;
    }

    @Override
    public void build(Task<Void> task) throws CompileExceptionError, IOException {
        FontDesc.Builder fontDescbuilder = FontDesc.newBuilder();
        ProtoUtil.merge(task.input(0), fontDescbuilder);
        FontDesc fontDesc = fontDescbuilder.build();

        BuilderUtil.checkResource(this.project, task.input(0), "material", fontDesc.getMaterial());

        long fontDescHash      = fontDescToHash(fontDesc);
        IResource genResource  = this.project.getGeneratedResource(fontDescHash, "glyph_bank");
        int buildDirLen        = this.project.getBuildDirectory().length();
        String genResourcePath = genResource.getPath().substring(buildDirLen);

        FontMap.Builder fontMapBuilder = FontMap.newBuilder();
        fontMapBuilder.setMaterial(BuilderUtil.replaceExt(fontDesc.getMaterial(), ".material", ".materialc"));
        fontMapBuilder.setGlyphBank(BuilderUtil.replaceExt(genResourcePath, ".glyph_bank", ".glyph_bankc"));
        fontMapBuilder.setShadowX(fontDesc.getShadowX());
        fontMapBuilder.setShadowY(fontDesc.getShadowY());
        fontMapBuilder.setAlpha(fontDesc.getAlpha());
        fontMapBuilder.setOutlineAlpha(fontDesc.getOutlineAlpha());
        fontMapBuilder.setShadowAlpha(fontDesc.getShadowAlpha());
        fontMapBuilder.setLayerMask(getFontMapLayerMask(fontDesc));

        task.output(0).setContent(fontMapBuilder.build().toByteArray());
    }
}
