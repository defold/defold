package com.dynamo.cr.goeditor;

import java.util.HashMap;
import java.util.Map;

import org.eclipse.jface.resource.ImageDescriptor;
import org.eclipse.swt.graphics.Image;
import org.eclipse.ui.IEditorRegistry;
import org.eclipse.ui.PlatformUI;

public class ImageFactory {
    private IEditorRegistry editorRegistry;
    private Map<ImageDescriptor, Image> images = new HashMap<ImageDescriptor, Image>();

    public ImageFactory() {
        editorRegistry = PlatformUI.getWorkbench().getEditorRegistry();
    }

    public void dispose() {
        for (Image image : images.values()) {
            image.dispose();
        }
        images.clear();
    }

    public Image getImageFromFilename(String filename) {
        ImageDescriptor imageDesc = editorRegistry.getImageDescriptor(filename);

        if (!images.containsKey(imageDesc)) {
            Image image = imageDesc.createImage();
            images.put(imageDesc, image);
        }

        return images.get(imageDesc);
    }

}
