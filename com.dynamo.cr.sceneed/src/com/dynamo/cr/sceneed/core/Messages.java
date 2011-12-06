package com.dynamo.cr.sceneed.core;

import org.eclipse.osgi.util.NLS;

/**
 * This class will be moved once the errors are moved to other packages.
 * @author rasv
 *
 */
public class Messages extends NLS {
    private static final String BUNDLE_NAME = "com.dynamo.cr.sceneed.core.messages"; //$NON-NLS-1$

    static {
        // initialize resource bundle
        NLS.initializeMessages(BUNDLE_NAME, Messages.class);
    }

    private Messages() {
    }
}
