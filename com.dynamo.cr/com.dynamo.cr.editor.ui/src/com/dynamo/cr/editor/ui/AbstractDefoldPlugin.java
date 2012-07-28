package com.dynamo.cr.editor.ui;

import org.apache.commons.io.FilenameUtils;
import org.eclipse.jface.resource.ImageDescriptor;
import org.eclipse.jface.resource.ImageRegistry;
import org.eclipse.swt.graphics.Image;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.plugin.AbstractUIPlugin;

public abstract class AbstractDefoldPlugin extends AbstractUIPlugin implements IImageProvider {

    public static final String IMG_UNKNOWN = "UNKNOWN";

    @Override
    public Image getIconByExtension(String extension) {
        // Extract extension
        if (extension.indexOf('.') != -1) {
            extension = FilenameUtils.getExtension(extension);
        }
        ImageRegistry imageRegistry = getImageRegistry();
        if (extension != null) {
            ImageDescriptor descriptor = imageRegistry.getDescriptor(extension);
            if (descriptor == null) {
                descriptor = PlatformUI.getWorkbench().getEditorRegistry().getImageDescriptor("." + extension);
                if (descriptor != null) {
                    imageRegistry.put(extension, descriptor);
                }
            }
            if (descriptor != null) {
                return imageRegistry.get(extension);
            }
        }
        return imageRegistry.get(IMG_UNKNOWN);
    }

    @Override
    protected void initializeImageRegistry(ImageRegistry reg) {
        super.initializeImageRegistry(reg);

        reg.put(IMG_UNKNOWN, imageDescriptorFromPlugin(EditorUIPlugin.PLUGIN_ID, "icons/unknown.png"));
    }
}
