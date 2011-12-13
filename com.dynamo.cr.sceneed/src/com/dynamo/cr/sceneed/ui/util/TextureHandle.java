package com.dynamo.cr.sceneed.ui.util;

import java.awt.image.BufferedImage;

import com.sun.opengl.util.texture.Texture;
import com.sun.opengl.util.texture.TextureIO;

public class TextureHandle {
    private BufferedImage image;
    private Texture texture;

    public void setImage(BufferedImage image) {
        this.image = image;
        this.texture = null;
    }

    public Texture getTexture() {
        if (this.texture == null && this.image != null) {
            this.texture = TextureIO.newTexture(this.image, false);
            this.image = null;
        }
        return this.texture;
    }

    public void clear() {
        if (this.texture != null) {
            this.texture.dispose();
            this.texture = null;
        }
    }
}
