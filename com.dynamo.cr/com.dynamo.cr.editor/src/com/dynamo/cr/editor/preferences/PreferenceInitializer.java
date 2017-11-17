package com.dynamo.cr.editor.preferences;

import org.eclipse.core.runtime.preferences.AbstractPreferenceInitializer;
import org.eclipse.jface.preference.IPreferenceStore;

import com.dynamo.cr.editor.Activator;


/**
 * Class used to initialize default preference values.
 */
public class PreferenceInitializer extends AbstractPreferenceInitializer {

	/*
	 * (non-Javadoc)
	 *
	 * @see org.eclipse.core.runtime.preferences.AbstractPreferenceInitializer#initializeDefaultPreferences()
	 */
	@Override
    public void initializeDefaultPreferences() {
		IPreferenceStore store = Activator.getDefault().getPreferenceStore();
	    store.setDefault(PreferenceConstants.P_SERVER_URI, "http://cr.defold.se:9998");
	    store.setDefault(PreferenceConstants.P_SOCKS_PROXY_PORT, 1080);
	    store.setDefault(PreferenceConstants.P_CUSTOM_APPLICATION, false);
        store.setDefault(PreferenceConstants.P_USE_LOCAL_BRANCHES, true);
        store.setDefault(PreferenceConstants.P_SHOW_WELCOME_PAGE, true);
        store.setDefault(PreferenceConstants.P_ANONYMOUS_LOGGING, true);
        store.setDefault(PreferenceConstants.P_TEXTURE_COMPRESSION, true);
        store.setDefault(PreferenceConstants.P_CHECK_BUNDLUNG_OVERWRITE, true);
        store.setDefault(PreferenceConstants.P_QUIT_ON_ESC, false);
	    store.setDefault(PreferenceConstants.P_NATIVE_EXT_SERVER_URI, "https://build.defold.com");
	}
}
