package com.dynamo.cr.guied.core;

import org.eclipse.swt.graphics.Image;

import com.dynamo.cr.guied.Activator;
import com.dynamo.cr.sceneed.core.Node;

@SuppressWarnings("serial")
public class LabelNode extends Node {

    private String label;

    public LabelNode(String label) {
        this.label = label;
    }

    @Override
    public String toString() {
        return this.label;
    }

    @Override
    public Image getIcon() {
        return Activator.getDefault().getImageRegistry().get(Activator.FOLDER_IMAGE_ID);
    }
}
