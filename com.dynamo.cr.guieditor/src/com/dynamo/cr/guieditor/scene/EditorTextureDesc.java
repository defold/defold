package com.dynamo.cr.guieditor.scene;

import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.ui.views.properties.IPropertySource;

import com.dynamo.cr.guieditor.property.Property;
import com.dynamo.cr.guieditor.property.PropertyIntrospectorSource;
import com.dynamo.cr.guieditor.render.GuiTextureResource;
import com.dynamo.gui.proto.Gui.SceneDesc.TextureDesc;

public class EditorTextureDesc implements IAdaptable {
    @Property(commandFactory = UndoableCommandFactory.class)
    private String name;

    @Property(commandFactory = UndoableCommandFactory.class, isResource = true)
    private String texture;

    private PropertyIntrospectorSource<EditorTextureDesc, GuiScene> propertySource;

    private GuiScene scene;

    private GuiTextureResource textureResource;

    public EditorTextureDesc(GuiScene scene, TextureDesc textureDesc) {
        this.scene = scene;
        this.name = textureDesc.getName();
        this.texture = textureDesc.getTexture();
    }

    @Override
    public Object getAdapter(@SuppressWarnings("rawtypes") Class adapter) {
        if (adapter == IPropertySource.class) {
            if (this.propertySource == null) {
                this.propertySource = new PropertyIntrospectorSource<EditorTextureDesc, GuiScene>(this, scene, scene.getContentRoot());
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