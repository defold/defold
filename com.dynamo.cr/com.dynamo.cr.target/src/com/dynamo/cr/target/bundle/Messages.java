package com.dynamo.cr.target.bundle;

import org.eclipse.osgi.util.NLS;

public class Messages extends NLS {
    private static final String BUNDLE_NAME = "com.dynamo.cr.target.bundle.messages"; //$NON-NLS-1$

    static {
        // initialize resource bundle
        NLS.initializeMessages(BUNDLE_NAME, Messages.class);
    }

    private Messages() {
    }

    public static String BundleiOSPresenter_NO_ICONS_SPECIFIED;
    public static String BundleiOSPresenter_DIALOG_MESSAGE;
    public static String BundleiOSPresenter_PROFILE_NOT_FOUND;
    public static String BundleiOSPresenter_NO_IDENTITY_FOUND;

    public static String BundleTVOSPresenter_NO_ICONS_SPECIFIED;
    public static String BundleTVOSPresenter_DIALOG_MESSAGE;
    public static String BundleTVOSPresenter_PROFILE_NOT_FOUND;
    public static String BundleTVOSPresenter_NO_IDENTITY_FOUND;

    public static String BundleAndroidPresenter_DIALOG_MESSAGE;
}
