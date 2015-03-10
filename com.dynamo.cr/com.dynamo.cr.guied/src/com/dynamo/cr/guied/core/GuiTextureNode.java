package com.dynamo.cr.guied.core;

import java.awt.image.BufferedImage;

import javax.media.opengl.GL2;
import javax.media.opengl.GLProfile;
import javax.vecmath.Point2d;
import javax.vecmath.Vector2d;

import com.dynamo.bob.textureset.TextureSetGenerator;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.tileeditor.scene.RuntimeTextureSet;
import com.dynamo.cr.tileeditor.scene.TextureSetNode;
import com.dynamo.textureset.proto.TextureSetProto.TextureSetAnimation;
import com.jogamp.opengl.util.texture.Texture;
import com.jogamp.opengl.util.texture.awt.AWTTextureIO;


public class GuiTextureNode {
    private BufferedImage image;
    private TextureSetNode textureSetNode;
    private String animation;

    private Texture texture;
    private boolean reloadTexture = false;

    String[] supportedFormats = {".atlas", ".tilesource"};
    private boolean IsValidResource(String resourceName) {
        for (String name : this.supportedFormats) {
            if(resourceName.endsWith(name))
                return true;
        }
        return false;
    }

    public void setTexture(GuiNode node, String texturePath, String textureAnimation) {
        this.image = null;
        this.textureSetNode = null;
        this.animation = null;

        if(this.IsValidResource(texturePath)) {
            try {
                Node n = node.getModel().loadNode(texturePath);
                if(n instanceof TextureSetNode) {
                    n.setModel(node.getModel());
                    this.textureSetNode = (TextureSetNode) n;
                } else {
                    System.err.println("ERROR: ImageNode failed loading '" + texturePath + "', not an atlas or tilesource resource.");
                    return;
                }
            } catch (Exception e) {
                System.err.println("ERROR: ImageNode failed loading '" + texturePath + "' (" + e.getMessage() + ").");
                return;
            }
            try {
                this.animation = textureAnimation.substring(textureAnimation.indexOf("/")+1);
            } catch (Exception e) {
                this.animation = null;
            }
        } else {
            this.image = node.getModel().getImage(texturePath);
            this.animation = null;
        }
        this.reloadTexture = true;
    }

    public Texture getTexture(GL2 gl) {
        if (this.reloadTexture) {
            this.reloadTexture = false;

            if (this.image != null) {
                this.textureSetNode = null;

                if (this.texture == null) {
                    this.texture = AWTTextureIO.newTexture(GLProfile.getGL2GL3(), this.image, true);
                } else {
                    this.texture.updateImage(gl, AWTTextureIO.newTextureData(GLProfile.getGL2GL3(), this.image, true));
                }
                this.image = null;

            } else {
                if (this.texture != null) {
                    this.texture.destroy(gl);
                    this.texture = null;
                }

                if(this.textureSetNode != null) {
                    this.textureSetNode.getRuntimeTextureSet();
                }
            }
        }
        return this.textureSetNode == null ? this.texture : this.textureSetNode.getTextureHandle().getTexture(gl);
    }

    public class UVTransform {
        public Point2d translation;
        public Vector2d scale;
        public boolean rotated;
        public boolean flipX;
        public boolean flipY;
    }

    public UVTransform getUVTransform() {
        UVTransform uvTransform = new UVTransform();

        RuntimeTextureSet rTS = null;
        TextureSetAnimation animation = null;
        if(this.textureSetNode != null && this.animation != null) {
            rTS = this.textureSetNode.getRuntimeTextureSet();
            animation = rTS.getAnimation(this.animation);
        }

        if(animation == null) {
            uvTransform.translation = new Point2d(0,0);
            uvTransform.scale = new Vector2d(1,1);
            uvTransform.rotated = false;
            uvTransform.flipX = false;
            uvTransform.flipY = false;
        } else {
            TextureSetGenerator.UVTransform uvt = rTS.getUvTransform(animation, 0);
            uvTransform.translation = uvt.translation;
            uvTransform.scale = uvt.scale;
            uvTransform.rotated = uvt.rotated;
            uvTransform.flipX = animation.getFlipHorizontal() != 0;
            uvTransform.flipY = animation.getFlipVertical() != 0;
        }

        return uvTransform;
    }

}
