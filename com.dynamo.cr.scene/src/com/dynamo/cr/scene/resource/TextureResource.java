package com.dynamo.cr.scene.resource;

import java.awt.image.BufferedImage;

import com.sun.opengl.util.texture.Texture;
import com.sun.opengl.util.texture.TextureIO;

public class TextureResource extends Resource {

    private BufferedImage image;
    private Texture texture;
    private int width;
    private int height;

    public TextureResource(String path, BufferedImage image) {
        super(path);
        setImage(image);
    }

    public Texture getTexture() {
        if (this.texture == null) {
            this.texture = TextureIO.newTexture(this.image, true);
            this.width = texture.getWidth();
            this.height = texture.getHeight();
            this.image = null;
        }
        return this.texture;
    }

    public int getWidth() {
        return this.width;
    }

    public int getHeight() {
        return this.height;
    }

    public BufferedImage getImage() {
        return this.image;
    }

    public void setImage(BufferedImage image) {
        this.image = image;
        this.width = image.getWidth();
        this.height = image.getHeight();
        this.texture = null;
    }
}
