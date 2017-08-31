package com.dynamo.cr.editor.handlers;
import java.util.Map;

import org.eclipse.jface.preference.IPreferenceStore;

import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.editor.core.EditorCorePlugin;
import com.dynamo.cr.editor.preferences.PreferenceConstants;

/**
 * Bundle HTML5 handler
 * TODO: The BundleHTML5Handler is located in com.dynamo.cr.editor in order
 * to avoid cyclic dependencies between .editor and .target
 * @author drlou
 *
 */
public class BundleHTML5Handler extends BundleGenericHandler {

    private static String PLATFORM_STRING = "js-web";

    @Override
    protected boolean openBundleDialog() {
        setTitle("Package HTML5 Application");
        return super.openBundleDialog();
    }

    @Override
    protected void setProjectOptions(Map<String, String> options) {
        final IPreferenceStore store = Activator.getDefault().getPreferenceStore();
        options.put("platform", PLATFORM_STRING);
        super.setProjectOptions(options);

        options.put("build-server", store.getString(PreferenceConstants.P_NATIVE_EXT_SERVER_URI));

        EditorCorePlugin corePlugin = EditorCorePlugin.getDefault();
        String sdkVersion = corePlugin.getSha1();
        if (sdkVersion == "NO SHA1") {
            sdkVersion = "";
        }
        options.put("defoldsdk", sdkVersion);
    }

    @Override
    protected String getOutputPlatformDir() {
        return PLATFORM_STRING;
    }

}
