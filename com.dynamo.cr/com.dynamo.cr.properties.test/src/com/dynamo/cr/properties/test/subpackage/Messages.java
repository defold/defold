package com.dynamo.cr.properties.test.subpackage;

import org.eclipse.osgi.util.NLS;

public class Messages extends NLS {
    private static final String BUNDLE_NAME = "com.dynamo.cr.properties.test.subpackage.messages"; //$NON-NLS-1$

    public static String SubDummyClass_subIntegerValue_OUTSIDE_RANGE;

    static {
        // initialize resource bundle
        NLS.initializeMessages(BUNDLE_NAME, Messages.class);
    }

    private Messages() {
    }
}
