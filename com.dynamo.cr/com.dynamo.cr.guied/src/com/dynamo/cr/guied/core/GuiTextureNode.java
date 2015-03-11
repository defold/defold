package com.dynamo.cr.guied.core;
import javax.media.opengl.GL2;
import javax.vecmath.Point2d;
import javax.vecmath.Vector2d;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import com.dynamo.bob.textureset.TextureSetGenerator;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.TextureHandle;
import com.dynamo.cr.tileeditor.scene.RuntimeTextureSet;
import com.dynamo.cr.tileeditor.scene.TextureSetNode;
import com.dynamo.textureset.proto.TextureSetProto.TextureSetAnimation;


public class GuiTextureNode {

    private static Logger logger = LoggerFactory.getLogger(GuiTextureNode.class);

    private TextureSetNode textureSetNode;
    private String animation;

    private TextureHandle textureHandle = new TextureHandle();

    public void dispose(GL2 gl) {
        textureHandle.clear(gl);
    }

    String[] supportedFormats = {".atlas", ".tilesource"};
    private boolean IsValidResource(String resourceName) {
        for (String name : this.supportedFormats) {
            if(resourceName.endsWith(name))
                return true;
        }
        return false;
    }

    public void setTexture(GuiNode node, String texturePath, String textureAnimation) {
        this.textureSetNode = null;
        this.animation = null;

        if(this.IsValidResource(texturePath)) {
            try {
                Node n = node.getModel().loadNode(texturePath);
                n.setModel(node.getModel());
                this.textureSetNode = (TextureSetNode) n;
            } catch (Exception e) {
                logger.error("Failed loading resource: " + texturePath, e);
                return;
            }
            try {
                this.animation = textureAnimation.substring(textureAnimation.indexOf("/")+1);
            } catch (Exception e) {
                this.animation = null;
            }
        } else {
            this.textureHandle.setImage(node.getModel().getImage(texturePath));
            this.animation = null;
            this.textureSetNode = null;
        }
    }

    public TextureHandle getTextureHandle() {
        if (this.textureSetNode != null) {
            this.textureSetNode.getRuntimeTextureSet();
            return this.textureSetNode.getTextureHandle();
        } else {
            return this.textureHandle;
        }
    }

    public static class UVTransform {
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
