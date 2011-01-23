package com.dynamo.cr.contenteditor.editors;

import java.awt.image.BufferedImage;

import com.sun.opengl.util.texture.Texture;
import com.sun.opengl.util.texture.TextureIO;

public class TextureResource {

    private BufferedImage image;
    private Texture texture;

    public TextureResource(BufferedImage image) {
        this.image = image;
    }

    public Texture getTexture() {
        if (texture == null) {
            texture = TextureIO.newTexture(this.image, true);
            image = null;
        }
        return texture;
    }

}
