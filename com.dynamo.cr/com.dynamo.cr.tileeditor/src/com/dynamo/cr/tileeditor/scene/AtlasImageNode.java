package com.dynamo.cr.tileeditor.scene;

import java.awt.image.BufferedImage;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Status;
import org.eclipse.swt.graphics.Image;

import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.tileeditor.Activator;

@SuppressWarnings("serial")
public class AtlasImageNode extends Node {

    private transient BufferedImage loadedImage;
    private String image;
    private String id;
    private int index = 0;

    public AtlasImageNode(String image) {
        this.image = image;
        this.id = new Path(image).removeFileExtension().lastSegment();
    }

    @Override
    public void setModel(ISceneModel model) {
        super.setModel(model);
        if (model != null) {
            this.loadedImage = getModel().getImage(image);
        }
    }

    @Override
    protected IStatus validateNode() {
        if (loadedImage != null) {
            return Status.OK_STATUS;
        } else {
            return new Status(IStatus.ERROR, Activator.PLUGIN_ID, Messages.Atlas_UNABLE_TO_LOAD_IMAGE);
        }
    }

    public String getImage() {
        return this.image;
    }

    public String getId() {
        return this.id;
    }

    public void setIndex(int index) {
        this.index = index;
    }

    public int getIndex() {
        return this.index;
    }

    @Override
    public String toString() {
        return new Path(image).lastSegment();
    }

    @Override
    public Image getIcon() {
        return Activator.getDefault().getImageRegistry().get(Activator.IMAGE_IMAGE_ID);
    }

}
