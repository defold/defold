package com.dynamo.cr.ddfeditor.scene;

import org.eclipse.swt.graphics.Image;

import com.dynamo.cr.sceneed.core.Identifiable;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.ddfeditor.Activator;

@SuppressWarnings("serial")
public class ModelBoneNode extends Node implements Identifiable {

    public ModelBoneNode(String id) {
        this.id = id;
    }

    private String id = "";

    @Override
    public String getId() {
        return id;
    }

    @Override
    public void setId(String id) {
        this.id = id;
    }

    @Override
    public String toString() {
        return this.id;
    }

    @Override
    public Image getIcon() {
        return Activator.getDefault().getImageRegistry().get(Activator.MODEL_BONE_IMAGE_ID);
    }
}
