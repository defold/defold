package com.dynamo.cr.parted.views;

import org.eclipse.osgi.util.NLS;

public class Messages extends NLS {
    private static final String BUNDLE_NAME = "com.dynamo.cr.parted.views.messages";//$NON-NLS-1$

    public static String CurveEditor_noCurves;

    static {
        // load message values from bundle file
        NLS.initializeMessages(BUNDLE_NAME, Messages.class);
    }
}