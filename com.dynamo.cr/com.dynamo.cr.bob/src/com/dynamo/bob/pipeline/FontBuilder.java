package com.dynamo.bob.pipeline;

import java.awt.FontFormatException;
import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;

import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Task;
import com.dynamo.bob.font.Fontc;
import com.dynamo.bob.fs.IResource;
import com.dynamo.render.proto.Font.FontDesc;

@BuilderParams(name = "Font", inExts = ".font", outExt = ".fontc")
public class FontBuilder extends Builder<Void>  {

    @Override
    public Task<Void> create(IResource input) throws IOException, CompileExceptionError {
        FontDesc.Builder fontDescbuilder = FontDesc.newBuilder();
        ProtoUtil.merge(input, fontDescbuilder);
        FontDesc fontDesc = fontDescbuilder.build();

        Task<Void> task = Task.<Void>newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addInput(input.getResource(fontDesc.getFont()))
                .addOutput(input.changeExt(params.outExt()))
                .build();
        return task;
    }

    @Override
    public void build(Task<Void> task) throws CompileExceptionError,
            IOException {

        FontDesc.Builder fontDescbuilder = FontDesc.newBuilder();
        ProtoUtil.merge(task.input(0), fontDescbuilder);
        FontDesc fontDesc = fontDescbuilder.build();

        File ttfFile = BuilderUtil.checkFile(this.project, task.input(0), "font", fontDesc.getFont());
        BuilderUtil.checkFile(this.project, task.input(0), "material", fontDesc.getMaterial());

        Fontc fontc = new Fontc();
        BufferedInputStream fontStream = new BufferedInputStream(new FileInputStream(ttfFile));
        try {
            // TODO: The api for Fontc#run should perhaps be changed to accept an OutputStream instead
            // of a path to the file

            // TODO: Workaround to create the path to the file
            task.output(0).setContent(new byte[0]);
            String fontMapFile = task.output(0).getAbsPath();
            fontc.run(fontStream, fontDesc, fontMapFile);
        } catch (FontFormatException e) {
            task.output(0).remove();
            throw new CompileExceptionError(task.input(0), 0, e.getMessage());
        } finally {
            fontStream.close();
        }
    }
}
