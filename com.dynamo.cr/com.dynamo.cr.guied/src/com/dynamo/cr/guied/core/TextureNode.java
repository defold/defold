package com.dynamo.cr.guied.core;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Path;
import org.eclipse.swt.graphics.Image;

import com.dynamo.cr.guied.Activator;
import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.sceneed.core.Identifiable;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.validators.Unique;

@SuppressWarnings("serial")
public class TextureNode extends Node implements Identifiable {

    @Property
    @NotEmpty(severity = IStatus.ERROR)
    @Unique(scope = TextureNode.class, base = TexturesNode.class)
    private String id;

    String[] textureSetFormats = {".atlas", ".tilesource"};
    private boolean isTextureSet(String resourceName) {
        for (String name : this.textureSetFormats) {
            if(resourceName.endsWith(name))
                return true;
        }
        return false;
    }

    @Property(editorType = EditorType.RESOURCE, extensions = { "jpg", "png", "atlas", "tilesource" })
    private String texture;

    private boolean isTextureSet = false;

    public TextureNode() {
        this.texture = "";
        this.id = "";
    }

    public TextureNode(String texture) {
        this.isTextureSet = isTextureSet(texture);
        this.texture = texture;
        this.id = new Path(texture).removeFileExtension().lastSegment();
    }

    @Override
    public String getId() {
        return this.id;
    }

    @Override
    public void setId(String id) {
        this.id = id;
    }

    public String getTexture() {
        return this.texture;
    }

    public void setTexture(String texture) {
        this.isTextureSet = isTextureSet(texture);
        this.texture = texture;
    }

    @Override
    public String toString() {
        return this.id;
    }

    @Override
    public Image getIcon() {
        return Activator.getDefault().getImageRegistry().get(this.isTextureSet ? Activator.TEXTURE_ATLAS_IMAGE_ID : Activator.TEXTURE_IMAGE_ID);
    }
}
