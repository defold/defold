package com.dynamo.cr.parted;

import java.io.File;
import java.net.URL;

import org.eclipse.core.runtime.FileLocator;
import org.eclipse.jface.resource.ImageDescriptor;
import org.eclipse.jface.resource.ImageRegistry;
import org.osgi.framework.BundleContext;

import com.dynamo.cr.editor.core.EditorCorePlugin;
import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.editor.ui.AbstractDefoldPlugin;

/**
 * The activator class controls the plug-in life cycle
 */
public class ParticleEditorPlugin extends AbstractDefoldPlugin {

	// The plug-in ID
	public static final String PLUGIN_ID = "com.dynamo.cr.parted"; //$NON-NLS-1$

    public static final String EMITTER_IMAGE_ID = "EMITTER"; //$NON-NLS-1$
    public static final String MODIFIER_IMAGE_ID = "MODIFIER"; //$NON-NLS-1$

    public static final String PARTED_CONTEXT_ID = "com.dynamo.cr.parted.contexts.partEditor";

    public static final String CURVE_EDITOR_VIEW_ID = "com.dynamo.cr.parted.curveEditorView";

	// The shared instance
	private static ParticleEditorPlugin plugin;

	/**
	 * The constructor
	 */
	public ParticleEditorPlugin() {
	}

	public void start(BundleContext context) throws Exception {
		super.start(context);

		String jnaLibraryPath = null;

		// Check if jna.library.path is set externally.
		if (System.getProperty("jna.library.path") != null) {
            jnaLibraryPath = System.getProperty("jna.library.path");
        }

		URL bundleUrl;
		String platform = EditorCorePlugin.getPlatform();
		if (platform.equals("darwin")) {
            // The editor is 64-bit only on Mac OS X and shared libraries are
            // loaded from platform directory
		    platform = "x86_64-darwin";
		}
        bundleUrl = getBundle().getEntry("/lib/" + platform);

        URL fileUrl = FileLocator.toFileURL(bundleUrl);
        if (jnaLibraryPath == null) {
            // Set path where particle_shared library is found.
            jnaLibraryPath = fileUrl.getPath();
        } else {
            // Append path where particle_shared library is found.
            jnaLibraryPath += File.pathSeparator + fileUrl.getPath();
        }

        System.setProperty("jna.library.path", jnaLibraryPath);

		plugin = this;
	}

	public void stop(BundleContext context) throws Exception {
		plugin = null;
		super.stop(context);
	}

	@Override
	protected void initializeImageRegistry(ImageRegistry reg) {
	    super.initializeImageRegistry(reg);
        registerImage(reg, EMITTER_IMAGE_ID, "icons/drop.png");
        registerImage(reg, MODIFIER_IMAGE_ID, "icons/tornado.png");
	}

    private void registerImage(ImageRegistry registry, String key,
            String fileName) {

        ImageDescriptor id = imageDescriptorFromPlugin(PLUGIN_ID, fileName);
        registry.put(key, id);
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
