package com.dynamo.cr.sceneed.ui;

import org.eclipse.osgi.util.NLS;

public class Messages extends NLS {
    private static final String BUNDLE_NAME = "com.dynamo.cr.sceneed.ui.messages"; //$NON-NLS-1$

    // Preferences

    public static String PreferencePage_TopBkgdColor;
    public static String PreferencePage_BottomBkgdColor;

    static {
        // initialize resource bundle
        NLS.initializeMessages(BUNDLE_NAME, Messages.class);
    }

    private Messages() {
    }
}
