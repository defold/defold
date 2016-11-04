package com.dynamo.cr.ddfeditor.scene;

import javax.media.opengl.GL2;
import javax.vecmath.Vector3d;

import org.eclipse.swt.graphics.RGB;

import com.dynamo.cr.go.core.ComponentTypeNode;

import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Resource;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.properties.Range;
import com.dynamo.cr.sceneed.core.FontRendererHandle;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.label.proto.Label.LabelDesc.BlendMode;
import com.dynamo.label.proto.Label.LabelDesc.Pivot;

@SuppressWarnings("serial")
public class LabelNode extends ComponentTypeNode {

    @Property
    private Vector3d size = new Vector3d(0.0, 0.0, 0.0);    // Size of the text box

    @Property
    private String text = "";

    @Property
    private RGB color = new RGB(255, 255, 255);

    @Property
    @Range(min = 0.0, max = 1.0)
    private double alpha = 1.0;

    @Property
    private BlendMode blendMode = BlendMode.BLEND_MODE_ALPHA;

    @Property
    private Pivot pivot = Pivot.PIVOT_CENTER;

    @Property
    private boolean lineBreak = false;

    @Property
    @Range(min = -10.0, max = 10.0)
    private double leading = 1.0;

    @Property
    @Range(min = -10.0, max = 10.0)
    private double tracking = 0;

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

    @Property(editorType=EditorType.RESOURCE, extensions={"font"})
    @Resource
    @NotEmpty
    private String font = "";

    @Property(editorType=EditorType.RESOURCE, extensions={"material"})
    @Resource
    @NotEmpty
    private String material = "";

    private boolean fontIsDirty = false;

    private transient FontRendererHandle fontRendererHandle = null;

    public LabelNode() {
        super();
        updateFont();
    }

    @Override
    public void dispose(GL2 gl) {
        super.dispose(gl);
        if (this.fontRendererHandle != null &&
            this.fontRendererHandle.getDeferredClear()) {
            this.fontRendererHandle.clear(gl);
            this.fontRendererHandle = null;
        }
    }

    @Override
    public void parentSet() {
        if (getParent() != null) {
            setFlags(Flags.TRANSFORMABLE);
            setFlags(Flags.SCALABLE);
        } else {
            clearFlags(Flags.TRANSFORMABLE);
            clearFlags(Flags.SCALABLE);
        }
    }

    public String getText() {
        return text;
    }

    public void setText(String text) {
        this.text = text;
    }

    public boolean getLineBreak() {
        return lineBreak;
    }

    public void setLineBreak(boolean lineBreak) {
        this.lineBreak = lineBreak;
    }

    public String getFont() {
        return font;
    }

    public void setFont(String font) {
        this.font = font;
        updateFont();
    }

    public void setMaterial(String material) {
        this.material = material;
    }

    public String getMaterial() {
        return material;
    }

    public BlendMode getBlendMode() {
        return this.blendMode;
    }

    public void setBlendMode(BlendMode blendMode) {
        this.blendMode = blendMode;
    }

    public Pivot getPivot() {
        return this.pivot;
    }

    public void setPivot(Pivot pivot) {
        this.pivot = pivot;
    }

    public Vector3d getSize() {
        return this.size;
    }

    public void setSize(Vector3d size) {
        this.size = size;
    }

    public RGB getColor() {
        return new RGB(color.red, color.green, color.blue);
    }

    public void setColor(RGB color) {
        this.color.red = color.red;
        this.color.green = color.green;
        this.color.blue = color.blue;
    }

    public double getAlpha() {
        return this.alpha;
    }

    public void setAlpha(double alpha) {
        this.alpha = alpha;
    }

    public RGB getOutline() {
        return new RGB(outline.red, outline.green, outline.blue);
    }

    public void setOutline(RGB outline) {
        this.outline.red = outline.red;
        this.outline.green = outline.green;
        this.outline.blue = outline.blue;
    }

    public double getOutlineAlpha() {
        return outlineAlpha;
    }

    public void setOutlineAlpha(double outlineAlpha) {
        this.outlineAlpha = outlineAlpha;
    }

    public RGB getShadow() {
        return new RGB(shadow.red, shadow.green, shadow.blue);
    }

    public void setShadow(RGB shadow) {
        this.shadow.red = shadow.red;
        this.shadow.green = shadow.green;
        this.shadow.blue = shadow.blue;
    }

    public double getShadowAlpha() {
        return shadowAlpha;
    }

    public void setShadowAlpha(double shadowAlpha) {
        this.shadowAlpha = shadowAlpha;
    }

    public double getLeading() {
        return leading;
    }

    public void setLeading(double leading) {
        this.leading = leading;
    }

    public double getTracking() {
        return tracking;
    }

    public void setTracking(double tracking) {
        this.tracking = tracking;
    }

    public float[] calcNormRGBA() {
        float[] rgba = new float[] {1.0f, 1.0f, 1.0f, 1.0f};
        float inv = 1.0f / 255.0f;
        RGB rgb = this.getColor();
        rgba[0] *= rgb.red * inv;
        rgba[1] *= rgb.green * inv;
        rgba[2] *= rgb.blue * inv;
        rgba[3] *= (float) this.getAlpha();
        return rgba;
    }

    public FontRendererHandle getFontRendererHandle(GL2 gl) {
        if (getModel() != null) {

            if (this.fontRendererHandle != null && this.fontIsDirty ) {
                if (this.fontRendererHandle.getDeferredClear()) {
                    this.fontRendererHandle.clear(gl);
                    this.fontRendererHandle = null;
                }
            }

            if (this.fontIsDirty) {
                this.fontRendererHandle = null;
            }

            if (this.fontRendererHandle == null && !this.font.isEmpty()) {
                this.fontRendererHandle = getModel().getFont(this.font);
            }
        }

        return this.fontRendererHandle;
    }


    public FontRendererHandle getDefaultFontRendererHandle() {
        if (getModel() != null) {
            return getModel().getDefaultFontRendererHandle();
        }
        return null;
    }

    private void updateFont() {
        this.fontIsDirty = true;
    }

    @Override
    public void setModel(ISceneModel model) {
        super.setModel(model);
        updateFont();
    }
}
