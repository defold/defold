package com.dynamo.cr.editor.handlers;
import java.util.Map;

/**
 * Bundle OSX handler
 * TODO: The BundleOSXHandler is located in com.dynamo.cr.editor in order
 * to avoid cyclic dependencies between .editor and .target
 * @author chmu
 *
 */
public class BundleOSXHandler extends BundleGenericHandler {

    @Override
    protected boolean openBundleDialog() {
        setTitle("Package OSX Application");
        return super.openBundleDialog();
    }

    @Override
    protected void setProjectOptions(Map<String, String> options) {
        options.put("platform", "x86-darwin");
        super.setProjectOptions(options);
    }

}
