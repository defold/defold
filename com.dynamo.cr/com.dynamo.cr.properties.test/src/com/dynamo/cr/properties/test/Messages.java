package com.dynamo.cr.properties.test;

import org.eclipse.osgi.util.NLS;

public class Messages extends NLS {
    private static final String BUNDLE_NAME = "com.dynamo.cr.properties.test.messages"; //$NON-NLS-1$

    public static String DummyClass_integerValue_OUTSIDE_RANGE;

    static {
        // initialize resource bundle
        NLS.initializeMessages(BUNDLE_NAME, Messages.class);
    }

    private Messages() {
    }
}
