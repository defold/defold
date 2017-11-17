package com.dynamo.cr.editor.handlers;
import java.util.Map;

/**
 * Bundle Win32 handler
 * TODO: The BundleWin32Handler is located in com.dynamo.cr.editor in order
 * to avoid cyclic dependencies between .editor and .target
 * @author chmu
 *
 */
public class BundleWin32Handler extends BundleGenericHandler {

    private static String PLATFORM_STRING = "x86-win32";

    @Override
    protected boolean openBundleDialog() {
        setTitle("Package Windows Application");
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
