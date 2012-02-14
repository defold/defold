package com.dynamo.bob.pipeline;

import java.awt.image.BufferedImage;
import java.awt.image.ColorModel;
import java.awt.image.Raster;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;

import javax.imageio.ImageIO;

import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.IResource;
import com.dynamo.bob.Task;
import com.dynamo.graphics.proto.Graphics.TextureImage;
import com.dynamo.graphics.proto.Graphics.TextureImage.TextureFormat;
import com.google.protobuf.ByteString;

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
        BufferedImage image = ImageIO.read(is);
        TextureImage.Builder textureBuilder = TextureImage.newBuilder();

        ColorModel colorModel = image.getColorModel();
        int numComponents = colorModel.getNumComponents();
        boolean hasAlpha = colorModel.hasAlpha();

        TextureFormat format = TextureFormat.TEXTURE_FORMAT_LUMINANCE;
        if (numComponents == 1 && hasAlpha == false) {
            format = TextureFormat.TEXTURE_FORMAT_LUMINANCE;
        } else if (numComponents == 3 && hasAlpha == false) {
            format = TextureFormat.TEXTURE_FORMAT_RGB;
        } else if (numComponents == 4 && hasAlpha == true) {
            format = TextureFormat.TEXTURE_FORMAT_RGBA;
        } else {
            throw new CompileExceptionError(String.format("Unsupported color model: '%s'", colorModel.toString()));
        }

        int width = image.getWidth();
        int height = image.getHeight();
        TextureImage.Image.Builder uncompressed = TextureImage.Image
                .newBuilder()
                .setWidth(width)
                .setHeight(height);

        Raster raster = image.getData();
        int[] rasterData = new int[width * height * numComponents];
        raster.getPixels(0, 0, width, height, rasterData);
        byte[] byteRasterData = new byte[width * height * numComponents];

        for (int i = 0; i < rasterData.length; ++i) {
            byteRasterData[i] = (byte) (rasterData[i] & 0xff);
        }

        uncompressed.setData(ByteString.copyFrom(byteRasterData));
        uncompressed.setFormat(format);
        uncompressed.addMipMapOffset(0);
        uncompressed.addMipMapSize(byteRasterData.length);

        textureBuilder.addAlternatives(uncompressed);

        ByteArrayOutputStream out = new ByteArrayOutputStream(1024 * 1024);
        TextureImage texture = textureBuilder.build();
        texture.writeTo(out);
        out.close();
        task.output(0).setContent(out.toByteArray());
    }

}
