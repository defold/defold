package com.dynamo.cr.sceneed.core.validators;

import org.eclipse.osgi.util.NLS;

public class Messages extends NLS {
    private static final String BUNDLE_NAME = "com.dynamo.cr.sceneed.core.validators.messages"; //$NON-NLS-1$
    public static String UniqueValidator_DUPLICATE;
    static {
        // initialize resource bundle
        NLS.initializeMessages(BUNDLE_NAME, Messages.class);
    }

    private Messages() {
    }
}
