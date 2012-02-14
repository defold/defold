package com.dynamo.bob.pipeline;

import java.awt.FontFormatException;
import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;

import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.IResource;
import com.dynamo.bob.Task;
import com.dynamo.render.Fontc;
import com.dynamo.render.proto.Font.FontDesc;
import com.google.protobuf.TextFormat;

@BuilderParams(name = "Font", inExts = ".font", outExt = ".fontc")
public class FontBuilder extends Builder<Void>  {

    @Override
    public Task<Void> create(IResource input) throws IOException {
        return defaultTask(input);
    }

    @Override
    public void build(Task<Void> task) throws CompileExceptionError,
            IOException {

        String basedir = ".";
        FontDesc.Builder fontDescbuilder = FontDesc.newBuilder();
        TextFormat.merge(new String(task.input(0).getContent()), fontDescbuilder);
        FontDesc fontDesc = fontDescbuilder.build();
        if (fontDesc.getFont().length() == 0) {
            throw new CompileExceptionError("No ttf font specified in " + task.input(0).getPath() + ".");
        }
        String ttfFileName = basedir + File.separator + fontDesc.getFont();
        File ttfFile = new File(ttfFileName);
        if (!ttfFile.exists()) {
            throw new CompileExceptionError(String.format("%s:0 error: is missing the dependent ttf-file '%s'", task.input(0).getPath(), fontDesc.getFont()));
        }

        String materialFileName = basedir + File.separator + fontDesc.getMaterial();
        File materialFile = new File(materialFileName);
        if (!materialFile.isFile()) {
            throw new CompileExceptionError(String.format("%s:0 error: is missing the dependent material-file '%s'", task.input(0).getPath(), fontDesc.getMaterial()));
        }

        Fontc fontc = new Fontc();
        String fontFile = basedir + File.separator + fontDesc.getFont();
        BufferedInputStream fontStream = new BufferedInputStream(new FileInputStream(fontFile));
        try {
            // TODO: The api for Fontc#run should perhaps be changed to accept an OutputStream instead
            // of a path to the file
            fontc.run(fontStream, fontDesc, task.output(0).getPath());
        } catch (FontFormatException e) {
            throw new CompileExceptionError(e.getMessage());
        } finally {
            fontStream.close();
        }
    }
}
