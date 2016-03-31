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

    @Override
    protected boolean openBundleDialog() {
        setTitle("Package Linux Application");
        return super.openBundleDialog();
    }

    @Override
    protected void setProjectOptions(Map<String, String> options) {
        options.put("platform", "x86-win32");
        super.setProjectOptions(options);
    }

}
