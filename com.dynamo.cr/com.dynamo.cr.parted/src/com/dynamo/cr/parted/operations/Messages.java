package com.dynamo.cr.parted.operations;

import org.eclipse.osgi.util.NLS;

public class Messages extends NLS {
    private static final String BUNDLE_NAME = "com.dynamo.cr.parted.operations.messages"; //$NON-NLS-1$
    public static String InsertPointOperation_LABEL;
    public static String RemovePointsOperation_LABEL;
    public static String MovePointsOperation_LABEL;
    public static String SetTangentOperation_LABEL;
    static {
        // initialize resource bundle
        NLS.initializeMessages(BUNDLE_NAME, Messages.class);
    }

    private Messages() {
    }
}
