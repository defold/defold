package com.dynamo.cr.sceneed;

import org.eclipse.jface.resource.ImageDescriptor;
import org.eclipse.jface.resource.ImageRegistry;
import org.eclipse.swt.graphics.Image;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.plugin.AbstractUIPlugin;
import org.osgi.framework.BundleContext;

import com.dynamo.cr.sceneed.core.IImageProvider;
import com.dynamo.cr.sceneed.core.IManipulatorRegistry;
import com.dynamo.cr.sceneed.core.INodeTypeRegistry;
import com.dynamo.cr.sceneed.core.internal.ManipulatorRegistry;
import com.dynamo.cr.sceneed.core.internal.NodeTypeRegistry;

/**
 * The activator class controls the plug-in life cycle
 */
public class Activator extends AbstractUIPlugin implements IImageProvider {

    // The plug-in ID
    public static final String PLUGIN_ID = "com.dynamo.cr.sceneed"; //$NON-NLS-1$

    // Context IDs
    public static final String SCENEED_CONTEXT_ID = "com.dynamo.cr.sceneed.ui.contexts.sceneEditor"; //$NON-NLS-1$

    // Command IDs
    public static final String ENTER_COMMAND_ID = "com.dynamo.cr.sceneed.ui.commands.enter"; //$NON-NLS-1$

    public static final String MOVE_MODE_ID = "com.dynamo.cr.sceneed.core.manipulators.move-mode"; //$NON-NLS-1$

    public static final String ROTATE_MODE_ID = "com.dynamo.cr.sceneed.core.manipulators.rotate-mode"; //$NON-NLS-1$

    public static final String SCALE_MODE_ID = "com.dynamo.cr.sceneed.core.manipulators.scale-mode"; //$NON-NLS-1$

    // Image IDs
    public static final String IMG_OVERLAY_INFO = "IMG_OVERLAY_INFO";
    public static final String IMG_OVERLAY_WARNING = "IMG_OVERLAY_WARNING";
    public static final String IMG_OVERLAY_ERROR = "IMG_OVERLAY_ERROR";
    public static final String IMG_UNKNOWN = "UNKNOWN";
    public static final String IMG_FOLDER = "FOLDER";


    // The shared instance
    private static Activator plugin;

    private NodeTypeRegistry nodeTypeRegistry = null;
    private ManipulatorRegistry manipulatorRegistry = null;

    private static BundleContext context;

    static BundleContext getContext() {
        return context;
    }

    /*
     * (non-Javadoc)
     * @see org.eclipse.ui.plugin.AbstractUIPlugin#start(org.osgi.framework.BundleContext)
     */
    @Override
    public void start(BundleContext context) throws Exception {
        super.start(context);
        Activator.context = context;
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

    public INodeTypeRegistry getNodeTypeRegistry() {
        if (this.nodeTypeRegistry == null) {
            this.nodeTypeRegistry = new NodeTypeRegistry();
            this.nodeTypeRegistry.init(this);
        }
        return this.nodeTypeRegistry;
    }

    public IManipulatorRegistry getManipulatorRegistry() {
        if (this.manipulatorRegistry == null) {
            this.manipulatorRegistry = new ManipulatorRegistry();
            this.manipulatorRegistry.init(this);
        }
        return this.manipulatorRegistry;
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

        reg.put(IMG_OVERLAY_INFO, getImageDescriptor("icons/overlay_info.png"));
        reg.put(IMG_OVERLAY_WARNING, getImageDescriptor("icons/overlay_warning.png"));
        reg.put(IMG_OVERLAY_ERROR, getImageDescriptor("icons/overlay_error.png"));
        reg.put(IMG_UNKNOWN, getImageDescriptor("icons/unknown.png"));
        reg.put(IMG_FOLDER, getImageDescriptor("icons/folder.png"));
    }

    private static ImageDescriptor getImageDescriptor(String path) {
        return imageDescriptorFromPlugin(PLUGIN_ID, path);
    }
}
