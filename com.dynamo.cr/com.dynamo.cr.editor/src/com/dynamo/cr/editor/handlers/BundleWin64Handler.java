package com.dynamo.cr.editor.handlers;
import java.util.Map;

/**
 * Bundle Win64 handler
 * TODO: The BundleWin64Handler is located in com.dynamo.cr.editor in order
 * to avoid cyclic dependencies between .editor and .target
 * @author chmu
 *
 */
public class BundleWin64Handler extends BundleGenericHandler {

    @Override
    protected boolean openBundleDialog() {
        setTitle("Package Windows Application");
        return super.openBundleDialog();
    }

    @Override
    protected void setProjectOptions(Map<String, String> options) {
        options.put("platform", "x86_64-win32");
        super.setProjectOptions(options);
    }

}
