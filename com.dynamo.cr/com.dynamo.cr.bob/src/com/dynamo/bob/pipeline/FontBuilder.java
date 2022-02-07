// Copyright 2020 The Defold Foundation
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
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;

import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Task;
import com.dynamo.bob.font.Fontc;
import com.dynamo.bob.font.Fontc.FontResourceResolver;
import com.dynamo.bob.fs.IResource;
import com.dynamo.render.proto.Font.FontDesc;

@BuilderParams(name = "Font", inExts = ".font", outExt = ".fontc")
public class FontBuilder extends Builder<Void>  {

    @Override
    public Task<Void> create(IResource input) throws IOException, CompileExceptionError {
        FontDesc.Builder fontDescbuilder = FontDesc.newBuilder();
        ProtoUtil.merge(input, fontDescbuilder);
        FontDesc fontDesc = fontDescbuilder.build();

        Task.TaskBuilder<Void> task = Task.<Void>newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addInput(input.getResource(fontDesc.getFont()))
                .addOutput(input.changeExt(params.outExt()));

        return task.build();
    }

    @Override
    public void build(Task<Void> task) throws CompileExceptionError,
            IOException {

        FontDesc.Builder fontDescbuilder = FontDesc.newBuilder();
        ProtoUtil.merge(task.input(0), fontDescbuilder);
        FontDesc fontDesc = fontDescbuilder.build();

        final IResource inputFontFile = BuilderUtil.checkResource(this.project, task.input(0), "font", fontDesc.getFont());
        BuilderUtil.checkResource(this.project, task.input(0), "material", fontDesc.getMaterial());

        Fontc fontc = new Fontc();
        BufferedInputStream fontStream = new BufferedInputStream(new ByteArrayInputStream(inputFontFile.getContent()));
        try {

            // Run fontc, fills the fontmap builder and returns an image
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

            // Save fontmap file
            task.output(0).setContent(fontc.getFontMap().toByteArray());

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
