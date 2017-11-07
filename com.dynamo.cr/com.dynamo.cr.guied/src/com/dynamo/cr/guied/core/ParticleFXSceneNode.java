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
public class ParticleFXSceneNode extends Node implements Identifiable {

    @Property
    @NotEmpty(severity = IStatus.ERROR)
    @Unique(scope = ParticleFXSceneNode.class, base = ParticleFXScenesNode.class)
    private String id;

    @Property(editorType = EditorType.RESOURCE, extensions = { "particlefx" })
    private String particlefx;

    public ParticleFXSceneNode() {
        this.particlefx = "";
        this.id = "";
    }

    public ParticleFXSceneNode(String particlefx) {
        this.particlefx = particlefx;
        this.id = new Path(particlefx).removeFileExtension().lastSegment();
    }

    @Override
    public String getId() {
        return this.id;
    }

    @Override
    public void setId(String id) {
        this.id = id;
    }

    public String getParticlefx() {
        return this.particlefx;
    }

    public void setParticlefx(String particlefx) {
        this.particlefx = particlefx;
    }

    @Override
    public String toString() {
        return this.id;
    }

    @Override
    public Image getIcon() {
        return Activator.getDefault().getImageRegistry().get(Activator.PARTICLEFXSCENE_IMAGE_ID);
    }

}
