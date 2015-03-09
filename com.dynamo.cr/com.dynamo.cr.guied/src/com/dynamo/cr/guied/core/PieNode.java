package com.dynamo.cr.guied.core;

import org.eclipse.core.resources.IFile;
import org.eclipse.swt.graphics.Image;

import com.dynamo.cr.guied.Activator;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.properties.Range;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.gui.proto.Gui.NodeDesc;
import com.dynamo.gui.proto.Gui.NodeDesc.PieBounds;

@SuppressWarnings("serial")
public class PieNode extends ClippingNode {

    @Property(editorType = EditorType.DROP_DOWN)
    private String texture = "";

    private transient GuiTextureNode guiTextureNode = new GuiTextureNode();

    @Property
    private float innerRadius = 0.0f;

    @Property
    private NodeDesc.PieBounds outerBounds = PieBounds.PIEBOUNDS_ELLIPSE;

    @Property
    @Range(min = 4, max = 1000)
    private int perimeterVertices = 10;

    @Property
    @Range(min = -360, max = 360)
    private float pieFillAngle = 360;

    public float getInnerRadius() {
        return innerRadius;
    }

    public void setInnerRadius(float value) {
        innerRadius = value;
    }

    public NodeDesc.PieBounds getOuterBounds() {
        return outerBounds;
    }

    public void setOuterBounds(NodeDesc.PieBounds value) {
        outerBounds = value;
    }

    public int getPerimeterVertices() {
        return perimeterVertices;
    }

    public void setPerimeterVertices(int value) {
        perimeterVertices = value;
    }

    public float getPieFillAngle() {
        return pieFillAngle;
    }

    public void setPieFillAngle(float value) {
        pieFillAngle = value;
    }

    public String getTexture() {
        return this.texture;
    }

    public void setTexture(String texture) {
        this.texture = texture;
        updateTexture();
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
        return Activator.getDefault().getImageRegistry().get(Activator.BOX_NODE_IMAGE_ID);
    }
}
