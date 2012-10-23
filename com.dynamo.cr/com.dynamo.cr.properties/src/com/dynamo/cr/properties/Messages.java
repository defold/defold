package com.dynamo.cr.properties;

import org.eclipse.osgi.util.NLS;

public class Messages extends NLS {
    private static final String BUNDLE_NAME = "com.dynamo.cr.properties.messages"; //$NON-NLS-1$

    public static String FormPropertySheetViewer_RESET_VALUE;
    public static String FormPropertySheetViewer_NO_PROPERTIES;

    public static String NotEmptyValidator_EMPTY;
    public static String RangeValidator_OUTSIDE_RANGE;
    public static String ResourceValidator_NOT_FOUND;
    public static String GreaterThanZero_OUTSIDE_RANGE;

    static {
        // initialize resource bundle
        NLS.initializeMessages(BUNDLE_NAME, Messages.class);
    }

    private Messages() {
    }
}
