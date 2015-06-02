package com.dynamo.cr.guied.core;

import javax.media.opengl.GL2;
import javax.vecmath.Vector4d;

import org.eclipse.core.resources.IFile;
import org.eclipse.swt.graphics.Image;

import com.dynamo.cr.guied.Activator;
import com.dynamo.cr.guied.util.GuiNodeStateBuilder;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.util.LoaderUtil;
import com.dynamo.proto.DdfMath.Vector4;

@SuppressWarnings("serial")
public class BoxNode extends ClippingNode {

    @Property(editorType = EditorType.DROP_DOWN, category = "")
    private String texture = "";

    @Property
    private Vector4d slice9 = new Vector4d(0,0,0,0);

    private transient GuiTextureNode guiTextureNode = new GuiTextureNode();

    @Override
    public void dispose(GL2 gl) {
        super.dispose(gl);
        if (this.guiTextureNode != null) {
            this.guiTextureNode.dispose(gl);
        }
    }

    public String getTexture() {
        return this.texture;
    }

    public void setTexture(String texture) {
        this.texture = texture;
        updateTexture();
        GuiNodeStateBuilder.setField(this, "Texture", texture);
    }

    public void resetTexture() {
        this.texture = (String)GuiNodeStateBuilder.resetField(this, "Texture");
        updateTexture();
    }

    public boolean isTextureOverridden() {
        return GuiNodeStateBuilder.isFieldOverridden(this, "Texture", this.texture);
    }

    private void updateTexture() {
        if (!this.texture.isEmpty() && getModel() != null) {
            TextureNode textureNode = ((TexturesNode) getScene().getTexturesNode()).getTextureNode(this.texture);
            if(textureNode != null)
            {
                if (this.guiTextureNode == null) {
                    this.guiTextureNode = new GuiTextureNode();
                }
                this.guiTextureNode.setTexture(this, textureNode.getTexture(), this.texture);
                return;
            }
        }
        this.guiTextureNode = null;
    }

    public Object[] getTextureOptions() {
        TexturesNode node = (TexturesNode) getScene().getTexturesNode();
        return node.getTextures(getModel()).toArray();
    }

    public GuiTextureNode getGuiTextureNode() {
        return this.guiTextureNode;
    }

    @Override
    public void setModel(ISceneModel model) {
        super.setModel(model);
        updateTexture();
    }

    @Override
    public boolean handleReload(IFile file, boolean childWasReloaded) {
        boolean reloaded = false;
        if (this.texture != null && !this.texture.isEmpty()) {
            TextureNode textureNode = ((TexturesNode) getScene().getTexturesNode()).getTextureNode(this.texture);
            if(textureNode != null) {
                IFile imgFile = getModel().getFile(textureNode.getTexture());
                if (file.equals(imgFile)) {
                    updateTexture();
                    reloaded = true;
                }
            }
        }
        return reloaded;
    }

    @Override
    public Image getIcon() {
        if(GuiNodeStateBuilder.isStateSet(this)) {
            return Activator.getDefault().getImageRegistry().get(Activator.BOX_NODE_OVERRIDDEN_IMAGE_ID);
        }
        return Activator.getDefault().getImageRegistry().get(Activator.BOX_NODE_IMAGE_ID);
    }

    public Vector4d getSlice9()
    {
        return new Vector4d(slice9);
    }

    public void setSlice9(Vector4d slice9)
    {
        this.slice9.set(slice9);
        GuiNodeStateBuilder.setField(this, "Slice9", LoaderUtil.toVector4(slice9));
    }

    public void resetSlice9() {
        this.slice9.set(LoaderUtil.toVector4((Vector4)GuiNodeStateBuilder.resetField(this, "Slice9")));
    }


    public boolean isSlice9Overridden() {
        return GuiNodeStateBuilder.isFieldOverridden(this, "Slice9", LoaderUtil.toVector4(this.slice9));
    }
}
