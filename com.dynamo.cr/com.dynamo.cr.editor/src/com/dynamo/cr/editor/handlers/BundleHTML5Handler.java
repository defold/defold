package com.dynamo.cr.editor.handlers;
import java.util.Map;

/**
 * Bundle HTML5 handler
 * TODO: The BundleHTML5Handler is located in com.dynamo.cr.editor in order
 * to avoid cyclic dependencies between .editor and .target
 * @author drlou
 *
 */
public class BundleHTML5Handler extends BundleGenericHandler {

    private static String PLATFORM_STRING = "js-web";

    @Override
    protected boolean openBundleDialog() {
        setTitle("Package HTML5 Application");
        return super.openBundleDialog();
    }

    @Override
    protected void setProjectOptions(Map<String, String> options) {
        options.put("platform", PLATFORM_STRING);
        super.setProjectOptions(options);
    }

    @Override
    protected String getOutputPlatformDir() {
        return PLATFORM_STRING;
    }

}
