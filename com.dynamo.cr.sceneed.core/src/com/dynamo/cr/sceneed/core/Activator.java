package com.dynamo.cr.sceneed.core;

import org.eclipse.core.runtime.Plugin;
import org.osgi.framework.BundleContext;

import com.dynamo.cr.sceneed.core.internal.ManipulatorRegistry;
import com.dynamo.cr.sceneed.core.internal.NodeTypeRegistry;

public class Activator extends Plugin {

    // The plug-in ID
    public static final String PLUGIN_ID = "com.dynamo.cr.sceneed.core"; //$NON-NLS-1$

    private static BundleContext context;

    static BundleContext getContext() {
        return context;
    }

    // The shared instance
    private static Activator plugin;

    private NodeTypeRegistry nodeTypeRegistry = null;

    private ManipulatorRegistry manipulatorRegistry = null;

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

    /*
     * (non-Javadoc)
     * @see org.osgi.framework.BundleActivator#start(org.osgi.framework.BundleContext)
     */
    @Override
    public void start(BundleContext bundleContext) throws Exception {
        Activator.context = bundleContext;
        plugin = this;
    }

    /*
     * (non-Javadoc)
     * @see org.osgi.framework.BundleActivator#stop(org.osgi.framework.BundleContext)
     */
    @Override
    public void stop(BundleContext bundleContext) throws Exception {
        Activator.context = null;
        plugin = null;

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
