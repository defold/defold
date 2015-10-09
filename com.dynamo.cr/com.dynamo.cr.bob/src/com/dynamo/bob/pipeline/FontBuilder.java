package com.dynamo.bob.pipeline;

import java.awt.FontFormatException;
import java.awt.image.BufferedImage;
import java.io.BufferedInputStream;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;

import org.apache.commons.io.FilenameUtils;

import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Task;
import com.dynamo.bob.font.Fontc;
import com.dynamo.bob.font.Fontc.FontResourceResolver;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.util.TextureUtil;
import com.dynamo.graphics.proto.Graphics.TextureImage;
import com.dynamo.graphics.proto.Graphics.TextureProfile;
import com.dynamo.render.proto.Font.FontDesc;
import com.dynamo.render.proto.Font.FontMap;

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
                .addOutput(input.changeExt(params.outExt()))
                .addOutput(input.changeExt("_tex0.texturec"));

        String textureProfilesPath = this.project.getProjectProperties().getStringValue("graphics", "texture_profiles");
        if (textureProfilesPath != null) {
            task.addInput(this.project.getResource(textureProfilesPath));
        }

        return task.build();
    }

    @Override
    public void build(Task<Void> task) throws CompileExceptionError,
            IOException {

        TextureProfile texProfile = TextureUtil.getTextureProfileByPath(this.project.getTextureProfiles(), task.input(0).getPath());

        FontDesc.Builder fontDescbuilder = FontDesc.newBuilder();
        ProtoUtil.merge(task.input(0), fontDescbuilder);
        FontDesc fontDesc = fontDescbuilder.build();
        final IResource curRes = task.input(0);

        IResource inputFontFile = BuilderUtil.checkResource(this.project, task.input(0), "font", fontDesc.getFont());
        BuilderUtil.checkResource(this.project, task.input(0), "material", fontDesc.getMaterial());

        Fontc fontc = new Fontc();
        BufferedInputStream fontStream = new BufferedInputStream(new ByteArrayInputStream(inputFontFile.getContent()));
        BufferedImage outputImage = null;
        try {

            // Run fontc, fills the fontmap builder and returns an image
            FontMap.Builder fontMapBuilder = FontMap.newBuilder();
            outputImage = fontc.compile(fontStream, fontDesc, fontMapBuilder, new FontResourceResolver() {
                @Override
                public InputStream getResource(String resourceName)
                        throws FileNotFoundException {
                    IResource res = curRes.getResource(resourceName);
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

            // Generate texture from font image
            TextureImage texture = TextureGenerator.generate(outputImage, texProfile);
            ByteArrayOutputStream textureOutputStream = new ByteArrayOutputStream(1024 * 1024);
            texture.writeTo(textureOutputStream);
            textureOutputStream.close();
            task.output(1).setContent(textureOutputStream.toByteArray());

            String texturePath = task.input(0).changeExt("_tex0.texturec").output().getPath();
            texturePath = texturePath.substring(project.getBuildDirectory().length());
            fontMapBuilder.addTextures(texturePath);

            // Save fontmap file
            task.output(0).setContent(fontMapBuilder.build().toByteArray());

            BuilderUtil.checkResource(project, task.output(0), "texture", task.output(1).getPath());

        } catch (FontFormatException e) {
            task.output(0).remove();
            task.output(1).remove();
            throw new CompileExceptionError(task.input(0), 0, e.getMessage());
        } catch (TextureGeneratorException e) {
            task.output(0).remove();
            task.output(1).remove();
            throw new CompileExceptionError(task.input(0), -1, e.getMessage(), e);
        } finally {
            fontStream.close();
        }
    }
}
