package com.dynamo.cr.parted;

import org.eclipse.ui.plugin.AbstractUIPlugin;
import org.osgi.framework.BundleContext;

/**
 * The activator class controls the plug-in life cycle
 */
public class ParticleEditorPlugin extends AbstractUIPlugin {

	// The plug-in ID
	public static final String PLUGIN_ID = "com.dynamo.cr.parted"; //$NON-NLS-1$

	// The shared instance
	private static ParticleEditorPlugin plugin;

	/**
	 * The constructor
	 */
	public ParticleEditorPlugin() {
	}

	public void start(BundleContext context) throws Exception {
		super.start(context);
		plugin = this;
	}

	public void stop(BundleContext context) throws Exception {
		plugin = null;
		super.stop(context);
	}

	/**
	 * Returns the shared instance
	 *
	 * @return the shared instance
	 */
	public static ParticleEditorPlugin getDefault() {
		return plugin;
	}

}
