package com.dynamo.cr.guieditor.scene;

import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.ui.views.properties.IPropertySource;

import com.dynamo.cr.guieditor.render.GuiFontResource;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.PropertyIntrospectorSource;
import com.dynamo.gui.proto.Gui.SceneDesc.FontDesc;

public class EditorFontDesc implements IAdaptable {
    @Property(commandFactory = UndoableCommandFactory.class)
    private String name;

    @Property(commandFactory = UndoableCommandFactory.class, isResource = true)
    private String font;

    private GuiScene scene;

    private PropertyIntrospectorSource<EditorFontDesc, GuiScene> propertySource;

    private GuiFontResource fontResource;

    public EditorFontDesc(GuiScene scene, FontDesc fontDesc) {
        this.scene = scene;
        this.name = fontDesc.getName();
        this.font = fontDesc.getFont();
    }

    @Override
    public Object getAdapter(@SuppressWarnings("rawtypes") Class adapter) {
        if (adapter == IPropertySource.class) {
            if (this.propertySource == null) {
                this.propertySource = new PropertyIntrospectorSource<EditorFontDesc, GuiScene>(this, scene, scene.getContentRoot());
            }
            return this.propertySource;
        }
        return null;
    }

    public String getName() {
        return name;
    }

    public void setName(String name) {
        this.name = name;
    }

    public String getFont() {
        return font;
    }

    public void setFont(String font) {
        this.font = font;
    }

    public void setFontResource(GuiFontResource fontResource) {
        this.fontResource = fontResource;
    }

    public GuiFontResource getFontResource() {
        return fontResource;
    }

    public FontDesc buildDesc() {
        return FontDesc.newBuilder().setName(name).setFont(font).build();
    }

    @Override
    public final int hashCode() {
        // NOTE: Do not override this method. We have data-structures that rely on referential equivalence, eg fontToIndex in GuiScene
        return super.hashCode();
    }

    @Override
    public final boolean equals(Object obj) {
        // NOTE: Do not override this method. We have data-structures that rely on referential equivalence, eg fontToIndex in GuiScene
        return super.equals(obj);
    }

}
