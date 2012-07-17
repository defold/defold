package com.dynamo.cr.ddfeditor;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.jface.resource.ImageDescriptor;
import org.eclipse.jface.resource.ImageRegistry;
import org.eclipse.ui.statushandlers.StatusManager;
import org.osgi.framework.BundleContext;

import com.dynamo.cr.editor.ui.AbstractDefoldPlugin;

public class Activator extends AbstractDefoldPlugin {

    public static final String PLUGIN_ID = "com.dynamo.cr.ddfeditor";
    private static Activator plugin;

    // Image ids
    public static final String COLLISION_SHAPE_IMAGE_ID = "COLLISION_SHAPE"; //$NON-NLS-1$

    public Activator() {
    }

    public static Activator getDefault() {
        return plugin;
    }

    @Override
    public void start(BundleContext context) throws Exception {
        super.start(context);
        plugin = this;
    }

    @Override
    public void stop(BundleContext context) throws Exception {
        super.stop(context);
        plugin = null;
    }

    public static void logException(Throwable e) {
        Status status = new Status(IStatus.ERROR, Activator.PLUGIN_ID, e.getMessage(), e);
        StatusManager.getManager().handle(status, StatusManager.LOG);
    }

    @Override
    protected void initializeImageRegistry(ImageRegistry reg) {
        super.initializeImageRegistry(reg);
        registerImage(reg, "link_add", "icons/link_add.png");
        registerImage(reg, "page_add", "icons/page_add.png");
        registerImage(reg, "delete", "icons/delete.png");
        registerImage(reg, "exclamation", "icons/exclamation.png");
        registerImage(reg, COLLISION_SHAPE_IMAGE_ID, "icons/draw_ellipse.png");
    }

    private void registerImage(ImageRegistry registry, String key,
            String fileName) {

        ImageDescriptor id = imageDescriptorFromPlugin(PLUGIN_ID, fileName);
        registry.put(key, id);
    }

}
