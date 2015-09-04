package com.dynamo.cr.guied.core;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.Reader;
import java.util.ArrayList;
import java.util.List;

import org.apache.commons.io.IOUtils;
import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.Path;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.graphics.RGB;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.guied.Activator;
import com.dynamo.cr.guied.util.GuiNodeStateBuilder;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.properties.Range;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.TextRendererHandle;
import com.dynamo.cr.sceneed.core.util.LoaderUtil;
import com.dynamo.proto.DdfMath.Vector4;
import com.dynamo.render.proto.Font;
import com.google.protobuf.TextFormat;

@SuppressWarnings("serial")
public class TextNode extends GuiNode {

    private static Logger logger = LoggerFactory.getLogger(TextNode.class);

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

    private transient TextRendererHandle textRendererHandle = new TextRendererHandle();

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
        this. outline = LoaderUtil.toRGB((Vector4)GuiNodeStateBuilder.resetField(this, "Outline"));
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

    public TextRendererHandle getTextRendererHandle() {
        return this.textRendererHandle;
    }

    private void loadFont(String fontPath) throws CoreException, IOException {
        IContainer contentRoot = getModel().getContentRoot();
        IFile fontFile = contentRoot.getFile(new Path(fontPath));
        InputStream is = fontFile.getContents();
        try {
            Reader reader = new InputStreamReader(is);
            Font.FontDesc.Builder fontDescBuilder = Font.FontDesc.newBuilder();
            TextFormat.merge(reader, fontDescBuilder);
            Font.FontDesc fontDesc = fontDescBuilder.build();

            String ttfFileName = fontDesc.getFont();
            IFile ttfFile = contentRoot.getFile(new Path(ttfFileName));

            ByteArrayOutputStream output = new ByteArrayOutputStream(1024 * 128);
            IOUtils.copy(ttfFile.getContents(), output);

            if (this.textRendererHandle == null) {
                this.textRendererHandle = new TextRendererHandle();
            }
            this.textRendererHandle.setFont(output.toByteArray(), fontDesc.getSize());
        } finally {
            is.close();
        }
    }

    private String findFontByName(List<Node> fontNodes) {
        for (Node n : fontNodes) {
            FontNode fontNode = (FontNode) n;
            if (fontNode.getId().equals(this.font)) {
                return fontNode.getFont();
            }
        }
        return null;
    }

    private void updateFont() {
        if (!this.font.isEmpty() && getModel() != null) {
            GuiSceneNode scene = getScene();
            String fontPath = this.findFontByName(scene.getFontsNode().getChildren());
            if(fontPath == null) {
                TemplateNode parentTemplate = this.getParentTemplateNode();
                if(parentTemplate != null && parentTemplate.getTemplateScene() != null) {
                    fontPath = this.findFontByName(parentTemplate.getTemplateScene().getFontsNode().getChildren());
                }
            }
            if (fontPath != null) {
                try {
                    loadFont(fontPath);
                } catch (CoreException e) {
                    logger.error("Could not load font " + fontPath, e);
                } catch (IOException e) {
                    logger.error("Could not load font " + fontPath, e);
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
