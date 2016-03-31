package com.dynamo.cr.editor.handlers;
import java.util.Map;

/**
 * Bundle Linux handler
 * TODO: The BundleLinuxHandler is located in com.dynamo.cr.editor in order
 * to avoid cyclic dependencies between .editor and .target
 * @author chmu
 *
 */
public class BundleLinuxHandler extends BundleGenericHandler {

    @Override
    protected boolean openBundleDialog() {
        setTitle("Package Linux Application");
        return super.openBundleDialog();
    }

    @Override
    protected void setProjectOptions(Map<String, String> options) {
        if (System.getProperty("os.arch") == "x86") {
            options.put("platform", "x86-linux");
        }
        else
        {
            options.put("platform", "x86_64-linux");
        }
        super.setProjectOptions(options);
    }

}
