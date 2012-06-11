package com.dynamo.cr.target.sign;

import org.eclipse.osgi.util.NLS;

public class Messages extends NLS {
    private static final String BUNDLE_NAME = "com.dynamo.cr.target.sign.messages"; //$NON-NLS-1$

    static {
        // initialize resource bundle
        NLS.initializeMessages(BUNDLE_NAME, Messages.class);
    }

    private Messages() {
    }

    public static String SignDialog_DIALOG_MESSAGE;
    public static String SignPresenter_PROFILE_NOT_FOUND;
    public static String SignPresenter_NO_IDENTITY_FOUND;
}
