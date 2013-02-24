package com.dynamo.cr.sceneed.core;

import java.awt.image.BufferedImage;

import javax.media.opengl.GL2;
import javax.media.opengl.GLProfile;

import com.jogamp.opengl.util.texture.Texture;
import com.jogamp.opengl.util.texture.awt.AWTTextureIO;

public class TextureHandle {
    private BufferedImage image;
    private Texture texture;
    private boolean reloadTexture = false;

    public void setImage(BufferedImage image) {
        this.image = image;
        this.reloadTexture = true;
    }

    public Texture getTexture(GL2 gl) {
        if (this.reloadTexture) {
            if (this.image != null) {
                if (this.texture == null) {
                    this.texture = AWTTextureIO.newTexture(GLProfile.getGL2GL3(), this.image, true);
                } else {

                    this.texture.updateImage(gl, AWTTextureIO.newTextureData(GLProfile.getGL2GL3(), this.image, true));
                }
                this.image = null;
            } else if (this.texture != null) {
                this.texture.destroy(gl);
                this.texture = null;
            }
            this.reloadTexture = false;
        }
        return this.texture;
    }

    public void clear(GL2 gl) {
        this.image = null;
        if (this.texture != null) {
            this.texture.destroy(gl);
            this.texture = null;
        }
    }
}
