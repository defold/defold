package com.dynamo.cr.guieditor.scene;

import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.ui.views.properties.IPropertySource;

import com.dynamo.cr.guieditor.property.Property;
import com.dynamo.cr.guieditor.property.PropertyIntrospectorSource;
import com.dynamo.gui.proto.Gui.SceneDesc.FontDesc;

public class EditorFontDesc implements IAdaptable {
    @Property(commandFactory = UndoableCommandFactory.class)
    private String name;

    @Property(commandFactory = UndoableCommandFactory.class)
    private String font;

    private GuiScene scene;

    private PropertyIntrospectorSource<EditorFontDesc, GuiScene> propertySource;

    public EditorFontDesc(GuiScene scene, FontDesc fontDesc) {
        this.scene = scene;
        this.name = fontDesc.getName();
        this.font = fontDesc.getFont();
    }

    @Override
    public Object getAdapter(@SuppressWarnings("rawtypes") Class adapter) {
        if (adapter == IPropertySource.class) {
            if (this.propertySource == null) {
                this.propertySource = new PropertyIntrospectorSource<EditorFontDesc, GuiScene>(this, scene);
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

    public FontDesc buildDesc() {
        return FontDesc.newBuilder().setName(name).setFont(font).build();
    }
}
