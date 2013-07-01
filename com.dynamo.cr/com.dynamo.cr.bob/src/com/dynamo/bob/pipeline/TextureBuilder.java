package com.dynamo.bob.pipeline;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;

import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.IResource;
import com.dynamo.bob.Task;
import com.dynamo.graphics.proto.Graphics.TextureImage;

@BuilderParams(name = "Texture", inExts = {".png", ".jpg"}, outExt = ".texturec")
public class TextureBuilder extends Builder<Void> {

    @Override
    public Task<Void> create(IResource input) throws IOException {
        return defaultTask(input);
    }

    @Override
    public void build(Task<Void> task) throws CompileExceptionError,
            IOException {

        ByteArrayInputStream is = new ByteArrayInputStream(task.input(0).getContent());
        TextureImage texture;
        try {
            texture = TextureGenerator.generate(is);
        } catch (TextureGeneratorException e) {
            throw new CompileExceptionError(task.input(0), -1, e.getMessage(), e);
        }

        ByteArrayOutputStream out = new ByteArrayOutputStream(1024 * 1024);
        texture.writeTo(out);
        out.close();
        task.output(0).setContent(out.toByteArray());
    }

}
