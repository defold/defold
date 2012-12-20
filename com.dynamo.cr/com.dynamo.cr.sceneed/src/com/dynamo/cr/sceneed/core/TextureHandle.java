package com.dynamo.cr.sceneed.core;

import java.awt.image.BufferedImage;

import com.sun.opengl.util.texture.Texture;
import com.sun.opengl.util.texture.TextureIO;

public class TextureHandle {
    private BufferedImage image;
    private Texture texture;
    private boolean reloadTexture = false;

    public void setImage(BufferedImage image) {
        this.image = image;
        this.reloadTexture = true;
    }

    public Texture getTexture() {
        if (this.reloadTexture) {
            if (this.image != null) {
                if (this.texture == null) {
                    this.texture = TextureIO.newTexture(this.image, true);
                } else {
                    this.texture.updateImage(TextureIO.newTextureData(this.image, true));
                }
                this.image = null;
            } else if (this.texture != null) {
                this.texture.dispose();
                this.texture = null;
            }
            this.reloadTexture = false;
        }
        return this.texture;
    }

    public void clear() {
        this.image = null;
        if (this.texture != null) {
            this.texture.dispose();
            this.texture = null;
        }
    }
}
