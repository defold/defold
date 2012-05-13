package com.dynamo.cr.guieditor.scene;

import org.eclipse.core.resources.IResource;
import org.eclipse.core.runtime.IAdaptable;

import com.dynamo.cr.guieditor.render.GuiFontResource;
import com.dynamo.cr.properties.Entity;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.PropertyIntrospector;
import com.dynamo.cr.properties.PropertyIntrospectorModel;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.gui.proto.Gui.SceneDesc.FontDesc;

@Entity(commandFactory = UndoableCommandFactory.class)
public class EditorFontDesc implements IAdaptable {
    @Property
    private String name;

    @Property(editorType=EditorType.RESOURCE, extensions={"font"})
    private String font;

    private GuiScene scene;

    private GuiFontResource fontResource;

    private long lastModified = IResource.NULL_STAMP;

    private static PropertyIntrospector<EditorFontDesc, GuiScene> introspector = new PropertyIntrospector<EditorFontDesc, GuiScene>(EditorFontDesc.class);

    public EditorFontDesc(GuiScene scene, FontDesc fontDesc) {
        this.scene = scene;
        this.name = fontDesc.getName();
        this.font = fontDesc.getFont();
    }

    @Override
    public Object getAdapter(@SuppressWarnings("rawtypes") Class adapter) {
        if (adapter == IPropertyModel.class) {
            return new PropertyIntrospectorModel<EditorFontDesc, GuiScene>(this, scene, introspector);
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

    public long getLastModified() {
        return lastModified;
    }

    public void setLastModified(long lastModified) {
        this.lastModified = lastModified;
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
