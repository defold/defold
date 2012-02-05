package com.dynamo.cr.sceneed.ui.preferences;

import org.eclipse.core.runtime.preferences.AbstractPreferenceInitializer;
import org.eclipse.jface.preference.IPreferenceStore;

import com.dynamo.cr.sceneed.Activator;

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
        store.setDefault(PreferenceConstants.P_TOP_BKGD_COLOR, "123,143,167");
        store.setDefault(PreferenceConstants.P_BOTTOM_BKGD_COLOR, "28,29,31");
        store.setDefault(PreferenceConstants.P_GRID, PreferenceConstants.P_GRID_AUTO_VALUE);
        store.setDefault(PreferenceConstants.P_GRID_SIZE, 100);
        store.setDefault(PreferenceConstants.P_GRID_COLOR, "114,123,130");
        store.setDefault(PreferenceConstants.P_SELECTION_COLOR, "131,188,212");
    }
}
