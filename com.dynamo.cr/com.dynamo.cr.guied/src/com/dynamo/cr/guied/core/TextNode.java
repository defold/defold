package com.dynamo.cr.guied.core;

import java.util.ArrayList;
import java.util.List;

import javax.media.opengl.GL2;

import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.graphics.RGB;

import com.dynamo.cr.guied.Activator;
import com.dynamo.cr.guied.util.GuiNodeStateBuilder;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.properties.Range;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.FontRendererHandle;
import com.dynamo.cr.sceneed.core.util.LoaderUtil;
import com.dynamo.proto.DdfMath.Vector4;

@SuppressWarnings("serial")
public class TextNode extends GuiNode {

    @Property
    private String text = "";

    @Property
    private boolean lineBreak = false;

    @Property(editorType = EditorType.DROP_DOWN)
    private String font = "";

    @Property()
    private RGB outline = new RGB(255, 255, 255);

    @Property()
    @Range(min = 0.0, max = 1.0)
    private double outlineAlpha = 1.0;

    @Property()
    private RGB shadow = new RGB(255, 255, 255);

    @Property
    @Range(min = 0.0, max = 1.0)
    private double shadowAlpha = 1.0;

    private transient FontNode fontNode = null;

    public TextNode() {
        super();
        updateFont();
    }

    public void dispose(GL2 gl) {
        if (this.fontNode != null) {
            this.fontNode.dispose(gl);
        }
    }

    public String getText() {
        return text;
    }

    public void setText(String text) {
        this.text = text;
        GuiNodeStateBuilder.setField(this, "Text", text);
    }

    public void resetText() {
        this.text = (String)GuiNodeStateBuilder.resetField(this, "Text");
    }

    public boolean isTextOverridden() {
        return GuiNodeStateBuilder.isFieldOverridden(this, "Text", this.text);
    }

    public boolean isLineBreak() {
        return lineBreak;
    }

    public void setLineBreak(boolean lineBreak) {
        this.lineBreak = lineBreak;
        GuiNodeStateBuilder.setField(this, "LineBreak", lineBreak);
    }

    public void resetLineBreak() {
        this.lineBreak = (Boolean)GuiNodeStateBuilder.resetField(this, "LineBreak");
    }

    public boolean isLineBreakOverridden() {
        return GuiNodeStateBuilder.isFieldOverridden(this, "LineBreak", this.lineBreak);
    }

    public String getFont() {
        return font;
    }

    public void setFont(String font) {
        this.font = font;
        updateFont();
        GuiNodeStateBuilder.setField(this, "Font", font);
    }

    public void resetFont() {
        this.font = (String)GuiNodeStateBuilder.resetField(this, "Font");
        updateFont();
    }

    public boolean isFontOverridden() {
        return GuiNodeStateBuilder.isFieldOverridden(this, "Font", this.font);
    }

    public Object[] getFontOptions() {
        List<Node> fonts = getScene().getFontsNode().getChildren();
        List<String> ids = new ArrayList<String>(fonts.size());
        for (Node n : fonts) {
            ids.add(((FontNode) n).getId());
        }
        return ids.toArray();
    }

    public RGB getOutline() {
        return new RGB(outline.red, outline.green, outline.blue);
    }

    public void setOutline(RGB outline) {
        this.outline.red = outline.red;
        this.outline.green = outline.green;
        this.outline.blue = outline.blue;
        GuiNodeStateBuilder.setField(this, "Outline", LoaderUtil.toVector4(this.outline, 1.0));
    }

    public void resetOutline() {
        this.outline = LoaderUtil.toRGB((Vector4)GuiNodeStateBuilder.resetField(this, "Outline"));
    }

    public boolean isOutlineOverridden() {
        return GuiNodeStateBuilder.isFieldOverridden(this, "Outline", LoaderUtil.toVector4(this.outline, 1.0));
    }

    public double getOutlineAlpha() {
        return outlineAlpha;
    }

    public void setOutlineAlpha(double outlineAlpha) {
        this.outlineAlpha = outlineAlpha;
        GuiNodeStateBuilder.setField(this, "OutlineAlpha", (float) outlineAlpha);
    }

    public void resetOutlineAlpha() {
        this.outlineAlpha = (Float)GuiNodeStateBuilder.resetField(this, "OutlineAlpha");
    }

    public boolean isOutlineAlphaOverridden() {
        return GuiNodeStateBuilder.isFieldOverridden(this, "OutlineAlpha", (float)this.outlineAlpha);
    }

    public RGB getShadow() {
        return new RGB(shadow.red, shadow.green, shadow.blue);
    }

    public void setShadow(RGB shadow) {
        this.shadow.red = shadow.red;
        this.shadow.green = shadow.green;
        this.shadow.blue = shadow.blue;
        GuiNodeStateBuilder.setField(this, "Shadow", LoaderUtil.toVector4(this.shadow, 1.0));
    }

    public void resetShadow() {
        this.shadow = LoaderUtil.toRGB((Vector4)GuiNodeStateBuilder.resetField(this, "Shadow"));
    }

    public boolean isShadowOverridden() {
        return GuiNodeStateBuilder.isFieldOverridden(this, "Shadow", LoaderUtil.toVector4(this.shadow, 1.0));
    }

    public double getShadowAlpha() {
        return shadowAlpha;
    }

    public void setShadowAlpha(double shadowAlpha) {
        this.shadowAlpha = shadowAlpha;
        GuiNodeStateBuilder.setField(this, "ShadowAlpha", (float) shadowAlpha);
    }

    public void resetShadowAlpha() {
        this.shadowAlpha = (Float)GuiNodeStateBuilder.resetField(this, "ShadowAlpha");
    }

    public boolean isShadowAlphaOverridden() {
        return GuiNodeStateBuilder.isFieldOverridden(this, "ShadowAlpha", (float)this.shadowAlpha);
    }

    private FontNode findFontNodeByName(List<Node> fontNodes) {
        for (Node n : fontNodes) {
            FontNode fontNode = (FontNode) n;
            if (fontNode.getId().equals(this.font)) {
                return fontNode;
            }
        }
        return null;
    }

    public FontRendererHandle getFontRendererHandle() {
        if (this.fontNode != null) {
            return this.fontNode.getFontRendererHandle();
        }
        return null;
    }

    public FontRendererHandle getDefaultFontRendererHandle() {
        if (this.fontNode != null) {
            return this.fontNode.getDefaultFontRendererHandle();
        }
        return null;
    }

    private void updateFont() {
        if (!this.font.isEmpty() && getModel() != null) {
            GuiSceneNode scene = getScene();
            this.fontNode = this.findFontNodeByName(scene.getFontsNode().getChildren());
            if(this.fontNode == null) {
                TemplateNode parentTemplate = this.getParentTemplateNode();
                if(parentTemplate != null && parentTemplate.getTemplateScene() != null) {
                    this.fontNode = this.findFontNodeByName(parentTemplate.getTemplateScene().getFontsNode().getChildren());
                }
            }
        }
    }

    @Override
    public void setModel(ISceneModel model) {
        super.setModel(model);
        updateFont();
    }

    @Override
    public Image getIcon() {
        if(GuiNodeStateBuilder.isStateSet(this)) {
            if(isTemplateNodeChild()) {
                return Activator.getDefault().getImageRegistry().get(Activator.TEXT_NODE_OVERRIDDEN_TEMPLATE_IMAGE_ID);
            }
            return Activator.getDefault().getImageRegistry().get(Activator.TEXT_NODE_OVERRIDDEN_IMAGE_ID);
        }
        return Activator.getDefault().getImageRegistry().get(Activator.TEXT_NODE_IMAGE_ID);
    }

}
