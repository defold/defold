package com.dynamo.cr.spine;

import org.eclipse.jface.resource.ImageRegistry;
import org.osgi.framework.BundleContext;

import com.dynamo.cr.editor.ui.AbstractDefoldPlugin;

public class Activator extends AbstractDefoldPlugin {
    // The plug-in ID
    public static final String PLUGIN_ID = "com.dynamo.cr.spine"; //$NON-NLS-1$

    // Image ids
    public static final String BONE_IMAGE_ID = "BONE"; //$NON-NLS-1$

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
    protected void initializeImageRegistry(ImageRegistry registry) {
        super.initializeImageRegistry(registry);

        registry.put(BONE_IMAGE_ID, imageDescriptorFromPlugin(PLUGIN_ID, "icons/bullet_black.png"));
    }
}
