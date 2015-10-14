package com.dynamo.cr.guied.core;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Path;
import org.eclipse.swt.graphics.Image;

import com.dynamo.cr.guied.Activator;
import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Identifiable;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.validators.Unique;

@SuppressWarnings("serial")
public class FontNode extends Node implements Identifiable {

    @Property
    @NotEmpty(severity = IStatus.ERROR)
    @Unique(scope = FontNode.class, base = FontsNode.class)
    private String id;

    @Property(editorType = EditorType.RESOURCE, extensions = { "font" })
    private String font;

    public FontNode() {
        this.font = "";
        this.id = "";
    }

    public FontNode(String font) {
        this.font = font;
        this.id = new Path(font).removeFileExtension().lastSegment();
    }

    @Override
    public void setModel(ISceneModel model) {
        super.setModel(model);
    }

    @Override
    public String getId() {
        return this.id;
    }

    @Override
    public void setId(String id) {
        this.id = id;
    }

    public String getFont() {
        return this.font;
    }

    public void setFont(String font) {
        this.font = font;
    }

    @Override
    public String toString() {
        return this.id;
    }

    @Override
    public Image getIcon() {
        return Activator.getDefault().getImageRegistry().get(Activator.FONT_IMAGE_ID);
    }
}
