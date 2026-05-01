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

import java.awt.FontFormatException;
import java.io.BufferedInputStream;
import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.InputStream;
import java.io.IOException;

import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.ProtoBuilder;
import com.dynamo.bob.ProtoParams;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;

import com.dynamo.bob.font.Fontc;
import com.dynamo.bob.font.Fontc.FontResourceResolver;
import com.dynamo.render.proto.Font.GlyphBank;
import com.dynamo.render.proto.Font.FontDesc;

@ProtoParams(srcClass = FontDesc.class, messageClass = GlyphBank.class)
@BuilderParams(name = "Glyph Bank", inExts = ".glyph_bank", outExt = ".glyph_bankc", isCacheble = true)
public class GlyphBankBuilder extends ProtoBuilder<FontDesc.Builder> {

    @Override
    public Task create(IResource input) throws IOException, CompileExceptionError {

    	FontDesc.Builder builder = getSrcBuilder(input);
        FontDesc fontDesc = builder.build();

        File file = new File(fontDesc.getFont());
        String fileNameWithExtension = file.getName();
        int dotIndex = fileNameWithExtension.lastIndexOf(".");
        String fileNameWithoutExtension = (dotIndex == -1) ? fileNameWithExtension : fileNameWithExtension.substring(0, dotIndex);

        long fontDescHash = Fontc.FontDescToHash(fontDesc);
        IResource glyphBank = project.createGeneratedResource(fileNameWithoutExtension, fontDescHash, "glyph_bank");

        Task.TaskBuilder task = Task. newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addOutput(glyphBank.changeExt(params.outExt()));
		return task.build();
    }

    @Override
    public void build(Task task) throws CompileExceptionError, IOException {
    	FontDesc.Builder builder = getSrcBuilder(task.firstInput());
        FontDesc fontDesc = builder.build();

        final IResource inputFontFile = BuilderUtil.checkResource(this.project, task.firstInput(), "font", fontDesc.getFont());
        BufferedInputStream fontStream = new BufferedInputStream(new ByteArrayInputStream(inputFontFile.getContent()));
        Fontc fontc = new Fontc();

        try {
            fontc.compile(fontStream, fontDesc, false, new FontResourceResolver() {
                @Override
                public InputStream getResource(String resourceName)
                        throws FileNotFoundException {
                    IResource res = inputFontFile.getResource(resourceName);
                    if (!res.exists()) {
                        throw new FileNotFoundException("Could not find resource: " + res.getPath());
                    }

                    try {
                        return new BufferedInputStream(new ByteArrayInputStream(res.getContent()));
                    } catch (IOException e) {
                        throw new FileNotFoundException("Could not find resource: " + res.getPath());
                    }
                }
            });

            task.output(0).setContent(fontc.getGlyphBank().toByteArray());

        } catch (FontFormatException e) {
            task.output(0).remove();
            throw new CompileExceptionError(task.input(0), 0, e.getMessage());
        } catch (TextureGeneratorException e) {
            task.output(0).remove();
            throw new CompileExceptionError(task.input(0), 0, e.getMessage());
        } finally {
            fontStream.close();
        }
    }
}
