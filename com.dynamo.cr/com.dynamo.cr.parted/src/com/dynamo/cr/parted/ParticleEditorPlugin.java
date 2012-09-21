package com.dynamo.cr.parted;

import java.net.URL;

import org.eclipse.core.runtime.FileLocator;
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

		URL bundleUrl;
		if (System.getProperty("os.name").toLowerCase().indexOf("mac") != -1) {
		    // The editor is 64-bit only on Mac OS X and shared libraries are
		    // loaded from platform directory
	        bundleUrl = getBundle().getEntry("/DYNAMO_HOME/lib/x86_64-darwin");
		} else {
		    // On other platforms shared libraries are loaded from default location
		    // We should perhaps always use qualifed directories?
            bundleUrl = getBundle().getEntry("/DYNAMO_HOME/lib");
		}

        URL fileUrl = FileLocator.toFileURL(bundleUrl);
        System.setProperty("jna.library.path", fileUrl.getPath());

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
