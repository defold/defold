package com.dynamo.cr.guieditor.scene;

import org.eclipse.core.resources.IResource;
import org.eclipse.core.runtime.IAdaptable;

import com.dynamo.cr.guieditor.render.GuiTextureResource;
import com.dynamo.cr.properties.Entity;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.PropertyIntrospector;
import com.dynamo.cr.properties.PropertyIntrospectorModel;
import com.dynamo.gui.proto.Gui.SceneDesc.TextureDesc;

@Entity(commandFactory = UndoableCommandFactory.class)
public class EditorTextureDesc implements IAdaptable {
    @Property
    private String name;

    @Property(isResource = true)
    private String texture;

    private GuiScene scene;

    private GuiTextureResource textureResource;

    private long lastModified = IResource.NULL_STAMP;

    private static PropertyIntrospector<EditorTextureDesc, GuiScene> introspector = new PropertyIntrospector<EditorTextureDesc, GuiScene>(EditorTextureDesc.class);

    public EditorTextureDesc(GuiScene scene, TextureDesc textureDesc) {
        this.scene = scene;
        this.name = textureDesc.getName();
        this.texture = textureDesc.getTexture();
    }

    @Override
    public Object getAdapter(@SuppressWarnings("rawtypes") Class adapter) {
        if (adapter == IPropertyModel.class) {
            return new PropertyIntrospectorModel<EditorTextureDesc, GuiScene>(this, scene, introspector, scene.getContentRoot());
        }
        return null;
    }

    public String getName() {
        return name;
    }

    public void setName(String name) {
        this.name = name;
    }

    public String getTexture() {
        return texture;
    }

    public void setTexture(String texture) {
        this.texture = texture;
    }

    public void setTextureResource(GuiTextureResource textureResource) {
        this.textureResource = textureResource;
    }

    public GuiTextureResource getTextureResource() {
        return textureResource;
    }

    public long getLastModified() {
        return lastModified;
    }

    public void setLastModified(long lastModified) {
        this.lastModified = lastModified;
    }

    public TextureDesc buildDesc() {
        return TextureDesc.newBuilder().setName(name).setTexture(texture).build();
    }

    @Override
    public final int hashCode() {
        // NOTE: Do not override this method. We have data-structures that rely on referential equivalence, eg textureToIndex in GuiScene
        return super.hashCode();
    }

    @Override
    public final boolean equals(Object obj) {
        // NOTE: Do not override this method. We have data-structures that rely on referential equivalence, eg textureToIndex in GuiScene
        return super.equals(obj);
    }
}