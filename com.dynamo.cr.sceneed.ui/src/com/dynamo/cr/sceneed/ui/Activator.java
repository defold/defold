package com.dynamo.cr.sceneed.ui;

import org.eclipse.jface.resource.ImageDescriptor;
import org.eclipse.jface.resource.ImageRegistry;
import org.eclipse.swt.graphics.Image;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.plugin.AbstractUIPlugin;
import org.osgi.framework.BundleContext;

import com.dynamo.cr.sceneed.core.IImageProvider;

/**
 * The activator class controls the plug-in life cycle
 */
public class Activator extends AbstractUIPlugin implements IImageProvider {

    // The plug-in ID
    public static final String PLUGIN_ID = "com.dynamo.cr.sceneed.ui"; //$NON-NLS-1$

    // Context IDs
    public static final String SCENEED_CONTEXT_ID = "com.dynamo.cr.sceneed.ui.contexts.sceneEditor"; //$NON-NLS-1$

    // Command IDs
    public static final String ENTER_COMMAND_ID = "com.dynamo.cr.sceneed.ui.commands.enter"; //$NON-NLS-1$

    // Image IDs
    public static final String IMG_OVERLAY_INFO = "IMG_OVERLAY_INFO";
    public static final String IMG_OVERLAY_WARNING = "IMG_OVERLAY_WARNING";
    public static final String IMG_OVERLAY_ERROR = "IMG_OVERLAY_ERROR";

    // The shared instance
    private static Activator plugin;

    /*
     * (non-Javadoc)
     * @see org.eclipse.ui.plugin.AbstractUIPlugin#start(org.osgi.framework.BundleContext)
     */
    @Override
    public void start(BundleContext context) throws Exception {
        super.start(context);
        plugin = this;
    }

    /*
     * (non-Javadoc)
     * @see org.eclipse.ui.plugin.AbstractUIPlugin#stop(org.osgi.framework.BundleContext)
     */
    @Override
    public void stop(BundleContext context) throws Exception {
        plugin = null;
        super.stop(context);
    }

    /**
     * Returns the shared instance
     *
     * @return the shared instance
     */
    public static Activator getDefault() {
        return plugin;
    }

    @Override
    public Image getImage(String extension) {
        ImageRegistry imageRegistry = getImageRegistry();
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
        return null;
    }

    @Override
    protected void initializeImageRegistry(ImageRegistry reg) {
        super.initializeImageRegistry(reg);

        reg.put(IMG_OVERLAY_INFO, getImageDescriptor("icons/overlay_info.png"));
        reg.put(IMG_OVERLAY_WARNING, getImageDescriptor("icons/overlay_warning.png"));
        reg.put(IMG_OVERLAY_ERROR, getImageDescriptor("icons/overlay_error.png"));
    }

    private static ImageDescriptor getImageDescriptor(String path) {
        return imageDescriptorFromPlugin(PLUGIN_ID, path);
    }
}
