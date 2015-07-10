package com.dynamo.cr.guied.core;

import javax.media.opengl.GL2;

import org.eclipse.core.resources.IFile;
import org.eclipse.swt.graphics.Image;

import com.dynamo.cr.guied.Activator;
import com.dynamo.cr.guied.util.GuiNodeStateBuilder;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.properties.Range;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.gui.proto.Gui.NodeDesc;
import com.dynamo.gui.proto.Gui.NodeDesc.PieBounds;
import com.google.protobuf.Descriptors.EnumValueDescriptor;

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

    @Override
    public void dispose(GL2 gl) {
        super.dispose(gl);
        if (this.guiTextureNode != null) {
            this.guiTextureNode.dispose(gl);
        }
    }

    public float getInnerRadius() {
        return innerRadius;
    }

    public void setInnerRadius(float value) {
        innerRadius = value;
        GuiNodeStateBuilder.setField(this, "InnerRadius", value);
    }

    public void resetInnerRadius() {
        this.innerRadius = (Float)GuiNodeStateBuilder.resetField(this, "InnerRadius");
    }

    public boolean isInnerRadiusOverridden() {
        return GuiNodeStateBuilder.isFieldOverridden(this, "InnerRadius", this.innerRadius);
    }

    public NodeDesc.PieBounds getOuterBounds() {
        return outerBounds;
    }

    public void setOuterBounds(NodeDesc.PieBounds value) {
        outerBounds = value;
        GuiNodeStateBuilder.setField(this, "OuterBounds", value);
    }

    public void resetOuterBounds() {
        this.outerBounds = NodeDesc.PieBounds.valueOf((EnumValueDescriptor)GuiNodeStateBuilder.resetField(this, "OuterBounds"));
    }

    public boolean isOuterBoundsOverridden() {
        return GuiNodeStateBuilder.isFieldOverridden(this, "OuterBounds", this.outerBounds);
    }

    public int getPerimeterVertices() {
        return perimeterVertices;
    }

    public void setPerimeterVertices(int value) {
        perimeterVertices = value;
        GuiNodeStateBuilder.setField(this, "PerimeterVertices", value);
    }

    public void resetPerimeterVertices() {
        this.perimeterVertices = (Integer)GuiNodeStateBuilder.resetField(this, "PerimeterVertices");
    }

    public boolean isPerimeterVerticesOverridden() {
        return GuiNodeStateBuilder.isFieldOverridden(this, "PerimeterVertices", this.perimeterVertices);
    }

    public float getPieFillAngle() {
        return pieFillAngle;
    }

    public void setPieFillAngle(float value) {
        pieFillAngle = value;
        GuiNodeStateBuilder.setField(this, "PieFillAngle", value);
    }

    public void resetPieFillAngle() {
        this.pieFillAngle = (Float)GuiNodeStateBuilder.resetField(this, "PieFillAngle");
    }

    public boolean isPieFillAngleOverridden() {
        return GuiNodeStateBuilder.isFieldOverridden(this, "PieFillAngle", this.pieFillAngle);
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

    private TextureNode getTextureNode() {
        TextureNode textureNode = ((TexturesNode) getScene().getTexturesNode()).getTextureNode(this.texture);
        if(textureNode == null) {
            TemplateNode parentTemplate = this.getParentTemplateNode();
            if(parentTemplate != null && parentTemplate.getTemplateScene() != null) {
                textureNode = ((TexturesNode) parentTemplate.getTemplateScene().getTexturesNode()).getTextureNode(this.texture);
            }
        }
        return textureNode;
    }

    private void updateTexture() {
        if (!this.texture.isEmpty() && getModel() != null) {
            TextureNode textureNode = this.getTextureNode();
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
            TextureNode textureNode = this.getTextureNode();
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
            if(isTemplateNodeChild()) {
                return Activator.getDefault().getImageRegistry().get(Activator.BOX_NODE_OVERRIDDEN_TEMPLATE_IMAGE_ID);
            }
            return Activator.getDefault().getImageRegistry().get(Activator.BOX_NODE_OVERRIDDEN_IMAGE_ID);
        }
        return Activator.getDefault().getImageRegistry().get(Activator.BOX_NODE_IMAGE_ID);
    }
}
