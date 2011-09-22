
package com.dynamo.cr.tileeditor;

import org.eclipse.jface.resource.ImageRegistry;
import org.eclipse.ui.plugin.AbstractUIPlugin;
import org.osgi.framework.BundleContext;

/**
 * The activator class controls the plug-in life cycle
 */
public class Activator extends AbstractUIPlugin {

    // The plug-in ID
    public static final String PLUGIN_ID = "com.dynamo.cr.tileeditor"; //$NON-NLS-1$

    // Context ID
    public static final String CONTEXT_ID = "com.dynamo.cr.tileeditor.contexts.TileSetEditor"; //$NON-NLS-1$

    // Image ids
    public static final String COLLISION_GROUP_IMAGE_ID = "COLLISION_GROUP"; //$NON-NLS-1$
    public static final String GRID_IMAGE_ID = "GRID"; //$NON-NLS-1$
    public static final String LAYER_IMAGE_ID = "LAYER"; //$NON-NLS-1$

    // The shared instance
    private static Activator plugin;

    /**
     * The constructor
     */
    public Activator() {
    }

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
    protected void initializeImageRegistry(ImageRegistry registry) {
        super.initializeImageRegistry(registry);

        registry.put(COLLISION_GROUP_IMAGE_ID, imageDescriptorFromPlugin(PLUGIN_ID, "icons/tile_grid.png"));
        registry.put(GRID_IMAGE_ID, imageDescriptorFromPlugin(PLUGIN_ID, "icons/tile_grid.png"));
        registry.put(LAYER_IMAGE_ID, imageDescriptorFromPlugin(PLUGIN_ID, "icons/layer.png"));
    }

}
