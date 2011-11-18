package com.dynamo.cr.sceneed.ui;

import org.eclipse.ui.plugin.AbstractUIPlugin;
import org.osgi.framework.BundleContext;

/**
 * The activator class controls the plug-in life cycle
 */
public class Activator extends AbstractUIPlugin {

    // The plug-in ID
    public static final String PLUGIN_ID = "com.dynamo.cr.sceneed.ui"; //$NON-NLS-1$

    // Context IDs
    public static final String SCENEED_CONTEXT_ID = "com.dynamo.cr.sceneed.contexts.sceneEditor"; //$NON-NLS-1$

    // Command IDs
    public static final String ENTER_COMMAND_ID = "com.dynamo.cr.sceneed.commands.enter"; //$NON-NLS-1$

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

}
