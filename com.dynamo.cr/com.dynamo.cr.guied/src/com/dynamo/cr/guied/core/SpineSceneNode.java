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
public class SpineSceneNode extends Node implements Identifiable {

    @Property
    @NotEmpty(severity = IStatus.ERROR)
    @Unique(scope = SpineSceneNode.class, base = SpineScenesNode.class)
    private String id;

    @Property(editorType = EditorType.RESOURCE, extensions = { "spinescene" })
    private String spineScene;

    public SpineSceneNode() {
        this.spineScene = "";
        this.id = "";
    }

    public SpineSceneNode(String spineScene) {
        this.spineScene = spineScene;
        this.id = new Path(spineScene).removeFileExtension().lastSegment();
    }

    @Override
    public String getId() {
        return this.id;
    }

    @Override
    public void setId(String id) {
        this.id = id;
    }

    public String getSpineScene() {
        return this.spineScene;
    }

    public void setSpineScene(String spineScene) {
        this.spineScene = spineScene;
    }

    @Override
    public String toString() {
        return this.id;
    }

    @Override
    public Image getIcon() {
        return Activator.getDefault().getImageRegistry().get(Activator.SPINESCENE_IMAGE_ID);
    }

}
